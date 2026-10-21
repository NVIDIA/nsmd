// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#include "fpga-run.h"

#include "base.h"

#include <stdio.h>
#include <string.h>

/* The row runner: plans one op at a time from the current row, classifies
 * its completion and applies the row's poll, expect and capture. */

static const struct nsm_fpga_run_limits default_limits = {5000, 200, 16};

static void stop(struct nsm_fpga_run *run, bool fence, const char *message)
{
	snprintf(run->error, sizeof(run->error), "%s", message);
	run->fence = fence;
	run->state = NSM_FPGA_RUN_STOPPED;
	run->step_count = 0;
	/* a failed item may leave a station mid-transfer: trust none */
	run->clean_stations = 0;
	run->clean_blocked = true;
}

static void finish(struct nsm_fpga_run *run)
{
	run->state = NSM_FPGA_RUN_FINISHED;
	run->step_count = 0;
}

static void start(struct nsm_fpga_run *run, uint8_t job)
{
	run->job = job;
	run->state = NSM_FPGA_RUN_RUNNING;
	run->step_count = 0;
	run->step_index = 0;
	run->op = NSM_FPGA_RUN_OP_NONE;
	run->pending = 0;
	run->foreign_seen = false;
	run->polls = 0;
	run->pops = 0;
	run->pop_data = false;
	run->wait_pending = false;
	run->error[0] = '\0';
	run->fence = false;
}

void nsm_fpga_run_init(struct nsm_fpga_run *run, const struct nsm_fpga_map *map,
		       struct nsm_fpga_session *session,
		       const struct nsm_fpga_run_limits *limits)
{
	memset(run, 0, sizeof(*run));
	run->map = map;
	run->session = session;
	run->limits = limits != NULL ? *limits : default_limits;
}

void nsm_fpga_run_begin_item(struct nsm_fpga_run *run)
{
	run->item_len = 0;
	run->last_word = 0;
	run->last_polled = 0;
	run->any_read = false;
	run->wrote_any = false;
	run->clean_blocked = false;
	run->end_group = false;
	run->group_state = 0;
	run->group_health = 0;
}

void nsm_fpga_run_begin_round(struct nsm_fpga_run *run)
{
	nsm_fpga_run_begin_item(run);
	run->group_len = 0;
}

void nsm_fpga_run_rows(struct nsm_fpga_run *run,
		       const struct nsm_fpga_command *rows, size_t count)
{
	start(run, NSM_FPGA_JOB_ROWS);
	run->rows = rows;
	run->row_count = rows != NULL ? count : 0;
	run->row_index = 0;
}

void nsm_fpga_run_control(struct nsm_fpga_run *run, uint8_t opcode,
			  uint32_t argument)
{
	start(run, NSM_FPGA_JOB_CONTROL);
	run->control_opcode = opcode;
	run->control_argument = argument;
	if (opcode != NSM_FPGA_CTRL_RENEW) {
		/* another owner may touch the stations between sessions */
		run->clean_stations = 0;
	}
}

void nsm_fpga_run_status(struct nsm_fpga_run *run)
{
	start(run, NSM_FPGA_JOB_STATUS);
}

static void start_authority(struct nsm_fpga_run *run, uint8_t opcode,
			    uint64_t epoch)
{
	start(run, NSM_FPGA_JOB_RECOVER);
	run->control_opcode = opcode;
	run->recover_epoch = epoch;
	/* the previous owner may have left stations mid-transfer: trust none */
	run->clean_stations = 0;
}

void nsm_fpga_run_recover(struct nsm_fpga_run *run, uint64_t epoch)
{
	start_authority(run, NSM_FPGA_CTRL_RECOVER, epoch);
}

void nsm_fpga_run_activate(struct nsm_fpga_run *run, uint64_t epoch)
{
	start_authority(run, NSM_FPGA_CTRL_ACTIVATE, epoch);
}

void nsm_fpga_run_abort(struct nsm_fpga_run *run, const char *message)
{
	if (run->state == NSM_FPGA_RUN_RUNNING) {
		stop(run, false, message);
	}
}

void nsm_fpga_run_default_capture(struct nsm_fpga_run *run)
{
	if (run->item_len != 0) {
		return;
	}
	for (size_t i = 0; i < 4; ++i) {
		run->item[run->item_len++] =
		    (uint8_t)(run->last_word >> (8 * i));
	}
}

static bool holds(const struct nsm_fpga_run *run,
		  const struct nsm_fpga_compare *compare, uint32_t value)
{
	return nsm_fpga_compare_holds(compare, run->item, run->item_len,
				      run->group, run->group_len, value);
}

/* Frames of one word op on a station, refused before the wire when illegal */
static bool plan_word(struct nsm_fpga_run *run, uint8_t station, uint8_t opcode,
		      uint64_t address, uint32_t value)
{
	if (!nsm_fpga_word_legal(run->map, station, opcode, address, value,
				 0xf)) {
		char text[NSM_FPGA_RUN_ERROR_MAX];
		snprintf(text, sizeof(text),
			 "illegal word %s blocked before the wire (rule bug): "
			 "addr=0x%llx",
			 opcode == NSM_FPGA_OP_READ32 ? "read" : "write",
			 (unsigned long long)address);
		stop(run, false, text);
		return false;
	}
	run->sequence = run->session->data_sequence + 1;
	run->identity =
	    nsm_fpga_plan_identity(&run->session->grant, run->sequence);
	const int rc = nsm_fpga_plan_word_compact(
	    &run->map->window, &run->session->grant, run->sequence, station,
	    opcode, address, value, run->steps);
	if (rc != NSM_SW_SUCCESS) {
		stop(run, false, "word plan rejected");
		return false;
	}
	run->op = NSM_FPGA_RUN_OP_WORD;
	run->word_opcode = opcode;
	run->step_count = NSM_FPGA_PLAN_COMPACT_STEPS;
	run->step_index = 0;
	run->pending = 0;
	run->foreign_seen = false;
	return true;
}

static bool plan_xfer(struct nsm_fpga_run *run,
		      const struct nsm_fpga_command *row)
{
	const struct nsm_fpga_xfer_op *x = &row->u.xfer;
	uint8_t write[3] = {x->write[0], x->write[1], x->write[2]};
	size_t taken = 0;
	for (size_t i = 0; i < 3; ++i) {
		if ((x->captured & (1u << i)) == 0) {
			continue;
		}
		if (x->captured_offset + taken >= run->group_len) {
			stop(run, false, "transaction: group buffer too short");
			return false;
		}
		write[i] = run->group[x->captured_offset + taken++];
	}
	const uint8_t opcode = nsm_fpga_xfer_opcode(x);
	const uint32_t argument =
	    nsm_fpga_xfer_argument(x->dev_addr, write, x->write_count);
	run->sequence = run->session->data_sequence + 1;
	run->identity =
	    nsm_fpga_plan_identity(&run->session->grant, run->sequence);
	if (nsm_fpga_plan_i2c_xfer(&run->map->window, &run->session->grant,
				   run->sequence, row->station, opcode,
				   argument, run->steps) != NSM_SW_SUCCESS) {
		stop(run, false, "i2c transaction plan rejected");
		return false;
	}
	run->op = NSM_FPGA_RUN_OP_XFER;
	run->xfer_opcode = opcode;
	run->argument = argument;
	run->step_count = NSM_FPGA_PLAN_BYTE_STEPS;
	run->step_index = 0;
	run->pending = 0;
	run->foreign_seen = false;
	return true;
}

static bool plan_byte(struct nsm_fpga_run *run, uint8_t station,
		      uint32_t argument)
{
	run->sequence = run->session->data_sequence + 1;
	run->identity =
	    nsm_fpga_plan_identity(&run->session->grant, run->sequence);
	if (nsm_fpga_plan_i2c_byte(&run->map->window, &run->session->grant,
				   run->sequence, station, argument,
				   run->steps) != NSM_SW_SUCCESS) {
		stop(run, false, "i2c byte plan rejected");
		return false;
	}
	run->op = NSM_FPGA_RUN_OP_BYTE;
	run->argument = argument;
	run->step_count = NSM_FPGA_PLAN_BYTE_STEPS;
	run->step_index = 0;
	run->pending = 0;
	run->foreign_seen = false;
	return true;
}

static void plan_control(struct nsm_fpga_run *run)
{
	run->sequence = run->session->control_sequence + 1;
	run->identity =
	    nsm_fpga_plan_identity(&run->session->grant, run->sequence);
	if (nsm_fpga_plan_control(&run->map->window, &run->session->grant,
				  run->control_opcode, run->sequence,
				  run->control_argument,
				  run->steps) != NSM_SW_SUCCESS) {
		stop(run, false, "control plan rejected");
		return;
	}
	run->op = NSM_FPGA_RUN_OP_CONTROL;
	run->step_count = NSM_FPGA_PLAN_CONTROL_STEPS;
	run->step_index = 0;
}

static void plan_recover(struct nsm_fpga_run *run)
{
	run->identity = (struct nsm_fpga_identity){run->recover_epoch, 0, 0, 0};
	if (nsm_fpga_plan_authority(&run->map->window, run->control_opcode,
				    run->recover_epoch,
				    run->steps) != NSM_SW_SUCCESS) {
		stop(run, false, "authority plan rejected");
		return;
	}
	run->op = NSM_FPGA_RUN_OP_RECOVER;
	run->step_count = NSM_FPGA_PLAN_CONTROL_STEPS;
	run->step_index = 0;
}

static void plan_status(struct nsm_fpga_run *run)
{
	nsm_fpga_plan_status_read(&run->map->window, run->steps);
	run->op = NSM_FPGA_RUN_OP_STATUS;
	run->step_count = 1;
	run->step_index = 0;
}

/* A failed expect fails the item or ends the group, as the row says */
static void expect_failed(struct nsm_fpga_run *run,
			  const struct nsm_fpga_command *row,
			  const char *message)
{
	if (row->on_expect_fail == NSM_FPGA_END_GROUP) {
		run->end_group = true;
		run->group_state = row->group_state;
		run->group_health = row->group_health;
		stop(run, false, "group ended by its setup check");
		return;
	}
	stop(run, false, message);
}

static bool station_clean(const struct nsm_fpga_run *run, uint8_t station)
{
	return station < 64 && (run->clean_stations & (1ULL << station)) != 0;
}

/* Plan the frames of the row at row_index; false when nothing goes out */
static bool plan_row(struct nsm_fpga_run *run,
		     const struct nsm_fpga_command *row)
{
	if (row->station < 64) {
		/* any op on a station ends what a leaves_clean row vouched for
		 */
		run->clean_stations &= ~(1ULL << row->station);
	}
	switch (row->op) {
	case NSM_FPGA_OP_READ32:
		return plan_word(run, row->station, NSM_FPGA_OP_READ32,
				 row->u.csr.address, 0);
	case NSM_FPGA_OP_WRITE32:
		return plan_word(run, row->station, NSM_FPGA_OP_WRITE32,
				 row->u.csr.address, row->u.csr.value);
	case NSM_FPGA_OP_ACK_CLEAR:
		return plan_word(run, row->station, NSM_FPGA_OP_WRITE32,
				 row->u.csr.address,
				 run->last_polled & ~row->poll_mask);
	case NSM_FPGA_OP_POP:
		return plan_word(run, row->station, NSM_FPGA_OP_READ32,
				 run->pop_data ? row->u.pop.data_address
					       : row->u.pop.status_address,
				 0);
	case NSM_FPGA_OP_BYTE:
		return plan_byte(run, row->station,
				 nsm_fpga_i2c_argument(&row->u.i2c));
	case NSM_FPGA_OP_I2C_XFER:
		return plan_xfer(run, row);
	case NSM_FPGA_OP_WRITE_CAPTURED: {
		const struct nsm_fpga_captured_op *cap = &row->u.captured;
		const struct nsm_fpga_station *station =
		    nsm_fpga_station_by_selector(run->map, row->station);
		if (station == NULL) {
			stop(run, false, "write captured: unknown station");
			return false;
		}
		if (station->kind == NSM_FPGA_STATION_I2C_BYTE) {
			if (cap->offset >= run->group_len) {
				stop(run, false,
				     "write captured: group buffer too short");
				return false;
			}
			const struct nsm_fpga_i2c_op op = {
			    cap->dev_addr, run->group[cap->offset],
			    (uint8_t)(cap->flags & ~NSM_FPGA_I2C_READ)};
			return plan_byte(run, row->station,
					 nsm_fpga_i2c_argument(&op));
		}
		if (cap->offset + 4 > run->group_len) {
			stop(run, false,
			     "write captured: group buffer too short");
			return false;
		}
		uint32_t word = 0;
		for (size_t i = 0; i < 4; ++i) {
			word |= (uint32_t)run->group[cap->offset + i]
				<< (8 * i);
		}
		return plan_word(run, row->station, NSM_FPGA_OP_WRITE32,
				 cap->address, word);
	}
	default:
		stop(run, false, "unsupported command op");
		return false;
	}
}

enum nsm_fpga_run_action nsm_fpga_run_next(struct nsm_fpga_run *run,
					   const struct nsm_fpga_step **step)
{
	if (run->state == NSM_FPGA_RUN_FINISHED ||
	    run->state == NSM_FPGA_RUN_IDLE) {
		return NSM_FPGA_RUN_DONE;
	}
	if (run->state == NSM_FPGA_RUN_STOPPED) {
		return NSM_FPGA_RUN_FAILED;
	}
	if (run->wait_pending) {
		run->wait_pending = false;
		return NSM_FPGA_RUN_WAIT;
	}
	for (;;) {
		if (run->step_index < run->step_count) {
			*step = &run->steps[run->step_index];
			return NSM_FPGA_RUN_SEND;
		}
		if (run->job == NSM_FPGA_JOB_CONTROL) {
			plan_control(run);
		} else if (run->job == NSM_FPGA_JOB_STATUS) {
			plan_status(run);
		} else if (run->job == NSM_FPGA_JOB_RECOVER) {
			plan_recover(run);
		} else {
			if (run->row_index >= run->row_count) {
				finish(run);
				return NSM_FPGA_RUN_DONE;
			}
			const struct nsm_fpga_command *row =
			    &run->rows[run->row_index];
			if (row->run_if.buffer != NSM_FPGA_BUF_NONE &&
			    !holds(run, &row->run_if, run->last_word)) {
				run->row_index++;
				continue;
			}
			if (row->clean == NSM_FPGA_UNLESS_CLEAN &&
			    station_clean(run, row->station)) {
				run->row_index++;
				continue;
			}
			if (row->op == NSM_FPGA_OP_CHECK) {
				/* nothing goes to the wire: the buffers decide
				 */
				if (!holds(run, &row->expect, 0)) {
					expect_failed(run, row,
						      "buffer value not in the "
						      "expected state");
					return NSM_FPGA_RUN_FAILED;
				}
				run->row_index++;
				continue;
			}
			plan_row(run, row);
		}
		if (run->state == NSM_FPGA_RUN_STOPPED) {
			return NSM_FPGA_RUN_FAILED;
		}
	}
}

static void capture(struct nsm_fpga_run *run,
		    const struct nsm_fpga_command *row, uint32_t value,
		    size_t width)
{
	uint8_t *buffer = NULL;
	size_t *length = NULL;
	size_t capacity = 0;
	if (row->capture == NSM_FPGA_BUF_ITEM) {
		buffer = run->item;
		length = &run->item_len;
		capacity = sizeof(run->item);
	} else if (row->capture == NSM_FPGA_BUF_GROUP) {
		buffer = run->group;
		length = &run->group_len;
		capacity = sizeof(run->group);
	} else {
		return;
	}
	if (*length + width > capacity) {
		stop(run, false, "capture buffer full");
		return;
	}
	for (size_t i = 0; i < width; ++i) {
		buffer[(*length)++] = (uint8_t)(value >> (8 * i));
	}
}

/* The row is done: a leaves_clean row of a good run marks its station */
static void row_done(struct nsm_fpga_run *run,
		     const struct nsm_fpga_command *row)
{
	if (row->clean == NSM_FPGA_LEAVES_CLEAN && !run->clean_blocked &&
	    row->station < 64) {
		run->clean_stations |= 1ULL << row->station;
	}
	run->row_index++;
}

/* The op of the current row completed: apply the row and move on */
static void row_completed(struct nsm_fpga_run *run)
{
	const struct nsm_fpga_command *row = &run->rows[run->row_index];
	const bool byte = run->op == NSM_FPGA_RUN_OP_BYTE;
	const bool xfer = run->op == NSM_FPGA_RUN_OP_XFER;
	const uint8_t reads = xfer ? (uint8_t)(run->xfer_opcode & 0x7) : 1;
	const bool byte_read =
	    (byte && (run->argument & (1u << 16)) != 0) || (xfer && reads > 0);
	const bool wrote = (byte && !byte_read) ||
			   (xfer && ((run->xfer_opcode >> 3) & 0x3) != 0);
	run->op = NSM_FPGA_RUN_OP_NONE;
	if (byte || xfer) {
		run->wrote_any = run->wrote_any || wrote;
		if (run->nack) {
			/* a target NACK fails the item and never fences */
			stop(run, false,
			     row->op == NSM_FPGA_OP_WRITE_CAPTURED
				 ? "i2c target nack on captured write"
				 : "i2c target nack");
			return;
		}
		if (!byte_read) {
			row_done(run, row);
			return;
		}
	} else if (run->word_opcode == NSM_FPGA_OP_WRITE32) {
		run->wrote_any = true;
		row_done(run, row);
		return;
	}
	if (row->op == NSM_FPGA_OP_POP) {
		/* drain a FIFO: while the status word shows credit, pop a word
		 */
		const uint32_t limit =
		    row->limit ? row->limit : run->limits.poll_limit;
		if (!run->pop_data) {
			if ((run->value & row->poll_mask) == 0) {
				run->pops = 0;
				row_done(run, row);
				return;
			}
			run->pop_data = true;
			return;
		}
		run->pop_data = false;
		if (++run->pops >= limit) {
			stop(run, false, "fifo not drained within the bound");
		}
		return;
	}
	if (!nsm_fpga_poll_satisfied(row->poll, row->poll_mask, run->value)) {
		const uint32_t limit =
		    row->limit ? row->limit : run->limits.poll_limit;
		if (run->polls + 1 >= limit) {
			stop(run, false, "poll condition never satisfied");
			return;
		}
		run->polls++;
		run->wait_us = row->period_us ? row->period_us
					      : run->limits.poll_period_us;
		run->wait_pending = true;
		return;
	}
	run->polls = 0;
	run->last_word = run->value;
	run->any_read = true;
	if (row->poll != NSM_FPGA_POLL_NONE || row->remember) {
		run->last_polled = run->value;
	}
	if (row->expect.mask != 0 && !holds(run, &row->expect, run->value)) {
		expect_failed(run, row, "read value not in the expected state");
		return;
	}
	capture(run, row, run->value, (byte || xfer) ? reads : 4);
	if (run->state == NSM_FPGA_RUN_RUNNING) {
		row_done(run, row);
	}
}

/* The reply stays pending: read the completion again, within the bound */
static void repeat_read(struct nsm_fpga_run *run)
{
	if (++run->pending > run->limits.pending_repeats) {
		stop(run, false, "reply stayed pending");
	}
}

static void stop_reply(struct nsm_fpga_run *run, const char *what,
		       enum nsm_fpga_reply reply)
{
	char text[NSM_FPGA_RUN_ERROR_MAX];
	snprintf(text, sizeof(text), "%s %s", what, nsm_fpga_reply_name(reply));
	/* foreign and rejected records mean the grant is gone: fence */
	stop(run,
	     reply == NSM_FPGA_REPLY_FOREIGN ||
		 reply == NSM_FPGA_REPLY_REJECTED,
	     text);
}

static void completed(struct nsm_fpga_run *run)
{
	run->session->data_sequence = run->sequence;
	run->step_count = 0;
	if (run->job == NSM_FPGA_JOB_ROWS) {
		row_completed(run);
	}
}

void nsm_fpga_run_reply(struct nsm_fpga_run *run, const uint8_t *record,
			bool lost, bool no_session)
{
	if (run->state != NSM_FPGA_RUN_RUNNING ||
	    run->step_index >= run->step_count) {
		return;
	}
	const struct nsm_fpga_step *step = &run->steps[run->step_index];
	if (no_session) {
		stop(run, true, "FPGA reports no session");
		return;
	}
	if (lost) {
		/* once a record went out the operation's effect is unknown */
		const bool published =
		    step->kind == NSM_FPGA_STEP_WRITE || run->step_index > 0;
		stop(run, published,
		     published ? "answer to a published record lost"
			       : "transport failed");
		return;
	}
	if (step->kind == NSM_FPGA_STEP_WRITE) {
		run->step_index++;
		return;
	}
	if (record == NULL) {
		stop(run, false, "completion record missing");
		return;
	}
	switch (run->op) {
	case NSM_FPGA_RUN_OP_STATUS:
		if (nsm_fpga_decode_authority_status(record, &run->authority) !=
		    NSM_SW_SUCCESS) {
			stop(run, false, "authority status record malformed");
			return;
		}
		finish(run);
		return;
	case NSM_FPGA_RUN_OP_CONTROL: {
		uint8_t result = 0;
		const enum nsm_fpga_reply reply = nsm_fpga_classify_control(
		    record, &run->identity, run->control_opcode,
		    run->control_argument, &result);
		if (reply == NSM_FPGA_REPLY_DONE) {
			run->session->control_sequence = run->sequence;
			finish(run);
		} else if (reply == NSM_FPGA_REPLY_PENDING) {
			repeat_read(run);
		} else if (reply == NSM_FPGA_REPLY_REJECTED) {
			char text[NSM_FPGA_RUN_ERROR_MAX];
			snprintf(text, sizeof(text),
				 "session control rejected %s",
				 nsm_fpga_control_result_name(result));
			stop(run, true, text);
		} else {
			stop_reply(run, "control receipt", reply);
		}
		return;
	}
	case NSM_FPGA_RUN_OP_RECOVER: {
		uint8_t result = 0;
		const enum nsm_fpga_reply reply = nsm_fpga_classify_authority(
		    record, run->recover_epoch, run->control_opcode, &result);
		const char *what = run->control_opcode == NSM_FPGA_CTRL_ACTIVATE
				       ? "activate"
				       : "recover";
		if (reply == NSM_FPGA_REPLY_DONE) {
			finish(run);
		} else if (reply == NSM_FPGA_REPLY_PENDING) {
			repeat_read(run);
		} else if (reply == NSM_FPGA_REPLY_REJECTED) {
			char text[NSM_FPGA_RUN_ERROR_MAX];
			snprintf(text, sizeof(text), "%s refused %s", what,
				 nsm_fpga_control_result_name(result));
			/* no session exists to fence; the caller holds off */
			stop(run, false, text);
		} else {
			char text[NSM_FPGA_RUN_ERROR_MAX];
			snprintf(text, sizeof(text), "%s receipt", what);
			stop_reply(run, text, reply);
		}
		return;
	}
	case NSM_FPGA_RUN_OP_WORD: {
		uint32_t data = 0;
		uint8_t fault_reason = 0;
		const enum nsm_fpga_reply reply = nsm_fpga_classify_word(
		    record, &run->identity, run->word_opcode, &data,
		    &fault_reason);
		if (reply == NSM_FPGA_REPLY_DONE) {
			run->value = data;
			completed(run);
		} else if (reply == NSM_FPGA_REPLY_PENDING) {
			repeat_read(run);
		} else if (reply == NSM_FPGA_REPLY_FAULT) {
			char text[NSM_FPGA_RUN_ERROR_MAX];
			snprintf(text, sizeof(text),
				 "operation faulted reason=%u, bank fenced - "
				 "recover required",
				 fault_reason);
			stop(run, true, text);
		} else {
			stop_reply(run, "word completion", reply);
		}
		return;
	}
	case NSM_FPGA_RUN_OP_BYTE: {
		uint8_t data = 0;
		bool nack = false;
		const enum nsm_fpga_reply reply = nsm_fpga_classify_i2c_byte(
		    record, &run->identity, run->argument, &data, &nack);
		if (reply == NSM_FPGA_REPLY_DONE) {
			run->value = data;
			run->nack = nack;
			completed(run);
		} else if (reply == NSM_FPGA_REPLY_PENDING) {
			repeat_read(run);
		} else if (reply == NSM_FPGA_REPLY_FOREIGN &&
			   !run->foreign_seen) {
			/* the byte engine answers for the previous op first,
			 * once */
			run->foreign_seen = true;
			repeat_read(run);
		} else if (reply == NSM_FPGA_REPLY_FAULT) {
			stop(
			    run, true,
			    "i2c byte faulted, bank fenced - recover required");
		} else {
			stop_reply(run, "i2c byte", reply);
		}
		return;
	}
	case NSM_FPGA_RUN_OP_XFER: {
		uint32_t data = 0;
		bool nack = false;
		uint8_t fault_reason = 0;
		const enum nsm_fpga_reply reply = nsm_fpga_classify_i2c_xfer(
		    record, &run->identity, run->xfer_opcode, &data, &nack,
		    &fault_reason);
		if (reply == NSM_FPGA_REPLY_DONE) {
			run->value = data;
			run->nack = nack;
			completed(run);
		} else if (reply == NSM_FPGA_REPLY_PENDING) {
			repeat_read(run);
		} else if (reply == NSM_FPGA_REPLY_FOREIGN &&
			   !run->foreign_seen) {
			run->foreign_seen = true;
			repeat_read(run);
		} else if (reply == NSM_FPGA_REPLY_FAULT) {
			char text[NSM_FPGA_RUN_ERROR_MAX];
			snprintf(text, sizeof(text),
				 "i2c transaction faulted reason=%u, bank "
				 "fenced - recover required",
				 fault_reason);
			stop(run, true, text);
		} else {
			stop_reply(run, "i2c transaction", reply);
		}
		return;
	}
	default:
		stop(run, false, "reply without an operation");
		return;
	}
}
