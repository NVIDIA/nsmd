// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#ifndef LIBNSM_FPGA_RUN_H
#define LIBNSM_FPGA_RUN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fpga-controller.h"
#include "fpga-tables.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The row runner. nsmd asks for the next MMIO frame, sends it, hands the
   reply back and repeats; every row, poll, capture and check is decided here.
 */

#define NSM_FPGA_ITEM_BUFFER_MAX 128
#define NSM_FPGA_GROUP_BUFFER_MAX 64
#define NSM_FPGA_RUN_ERROR_MAX 96
/* Every op is one SET then one GET */
#define NSM_FPGA_PLAN_MAX_STEPS 2

/* What nsmd does next */
enum nsm_fpga_run_action {
	NSM_FPGA_RUN_SEND = 0, /* send the step, then call nsm_fpga_run_reply */
	NSM_FPGA_RUN_WAIT = 1, /* sleep wait_us, then call next again */
	NSM_FPGA_RUN_DONE =
	    2, /* the job finished; the buffers hold its result */
	NSM_FPGA_RUN_FAILED = 3, /* the job failed: see error and fence */
};

/* The grant the frames carry and the sequences the runner advances */
struct nsm_fpga_session {
	struct nsm_fpga_grant grant;
	uint32_t control_sequence; /* the last control sent */
	uint32_t data_sequence;	   /* the last verified data op */
};

/* Poll defaults for rows that name none, and the pending repeat bound */
struct nsm_fpga_run_limits {
	uint32_t poll_period_us;
	uint32_t poll_limit;
	uint32_t pending_repeats;
};

enum nsm_fpga_run_job {
	NSM_FPGA_JOB_NONE = 0,
	NSM_FPGA_JOB_ROWS = 1,
	NSM_FPGA_JOB_CONTROL = 2,
	NSM_FPGA_JOB_STATUS = 3,
	NSM_FPGA_JOB_RECOVER = 4,
};

enum nsm_fpga_run_state {
	NSM_FPGA_RUN_IDLE = 0,
	NSM_FPGA_RUN_RUNNING = 1,
	NSM_FPGA_RUN_FINISHED = 2,
	NSM_FPGA_RUN_STOPPED = 3,
};

/* The op whose frames are in flight */
enum nsm_fpga_run_op {
	NSM_FPGA_RUN_OP_NONE = 0,
	NSM_FPGA_RUN_OP_WORD = 1,
	NSM_FPGA_RUN_OP_BYTE = 2,
	NSM_FPGA_RUN_OP_CONTROL = 3,
	NSM_FPGA_RUN_OP_STATUS = 4,
	NSM_FPGA_RUN_OP_XFER = 5,
	NSM_FPGA_RUN_OP_RECOVER = 6,
};

struct nsm_fpga_run {
	const struct nsm_fpga_map *map;
	struct nsm_fpga_session *session;
	struct nsm_fpga_run_limits limits;
	uint8_t job;
	uint8_t state;
	/* ROWS job */
	const struct nsm_fpga_command *rows;
	size_t row_count;
	size_t row_index;
	/* CONTROL job */
	uint8_t control_opcode;
	uint32_t control_argument;
	/* the fresh epoch an authority ACTIVATE or RECOVER carries */
	uint64_t recover_epoch;
	/* frames of the op in flight */
	struct nsm_fpga_step steps[NSM_FPGA_PLAN_MAX_STEPS];
	size_t step_count;
	size_t step_index;
	struct nsm_fpga_identity identity;
	uint32_t sequence;
	uint8_t op;
	uint8_t word_opcode;
	uint8_t xfer_opcode;
	uint32_t argument;
	uint32_t pending;
	bool foreign_seen; /* the byte engine's stale reply was taken */
	uint32_t polls;
	uint32_t pops;
	bool pop_data;
	bool wait_pending;
	/* results of the last op */
	uint32_t value;
	bool nack;
	struct nsm_fpga_authority_status authority;
	/* buffers and flags of the item and the round */
	uint8_t item[NSM_FPGA_ITEM_BUFFER_MAX];
	size_t item_len;
	uint8_t group[NSM_FPGA_GROUP_BUFFER_MAX];
	size_t group_len;
	uint32_t last_word;
	uint32_t last_polled; /* what ACK_CLEAR writes back */
	bool any_read;
	bool wrote_any;
	uint64_t clean_stations; /* stations a leaves_clean row left clean */
	bool clean_blocked; /* the item failed: no row leaves a station clean */
	bool end_group;
	uint8_t group_state;
	uint8_t group_health;
	/* outcome */
	char error[NSM_FPGA_RUN_ERROR_MAX];
	bool fence;
	uint32_t wait_us;
};

/** @brief Bind the runner to a map, the session it stamps frames with and its
 *  limits; NULL limits take the defaults (5 ms, 200 polls, 16 repeats). */
void nsm_fpga_run_init(struct nsm_fpga_run *run, const struct nsm_fpga_map *map,
		       struct nsm_fpga_session *session,
		       const struct nsm_fpga_run_limits *limits);

/** @brief Start a group round: clears the group buffer and the item state. */
void nsm_fpga_run_begin_round(struct nsm_fpga_run *run);

/** @brief Start an item: clears the item buffer, the last words and the
 *  flags; the group buffer stays. */
void nsm_fpga_run_begin_item(struct nsm_fpga_run *run);

/** @brief Start walking a row list over the current buffers. */
void nsm_fpga_run_rows(struct nsm_fpga_run *run,
		       const struct nsm_fpga_command *rows, size_t count);

/** @brief Start one session control: ACQUIRE, RENEW or RELEASE. */
void nsm_fpga_run_control(struct nsm_fpga_run *run, uint8_t opcode,
			  uint32_t argument);

/** @brief Start a read of the authority status record into run->authority. */
void nsm_fpga_run_status(struct nsm_fpga_run *run);
/** @brief Start a RECOVER of the bank with a fresh epoch: nsmd's own lapsed
 *  lease only, decided by the caller from nsm_fpga_authority_recoverable. */
void nsm_fpga_run_recover(struct nsm_fpga_run *run, uint64_t epoch);
/** @brief Start an ACTIVATE of a bank nobody has activated yet, decided by
 *  the caller from nsm_fpga_authority_activatable. */
void nsm_fpga_run_activate(struct nsm_fpga_run *run, uint64_t epoch);

/** @brief The next action; on SEND, step points at the frame to send. */
enum nsm_fpga_run_action nsm_fpga_run_next(struct nsm_fpga_run *run,
					   const struct nsm_fpga_step **step);

/** @brief Hand back the answer of the step just sent: the 32-byte record of a
 *  GET (NULL after a SET), lost when no decodable answer came, no_session
 *  when the FPGA answered that it holds no session for the frame. */
void nsm_fpga_run_reply(struct nsm_fpga_run *run, const uint8_t *record,
			bool lost, bool no_session);

/** @brief Stop the job without fencing: the transport failed in a way
 *  nsmd describes itself. */
void nsm_fpga_run_abort(struct nsm_fpga_run *run, const char *message);

/** @brief Append the last word read to an empty item buffer, so a rule that
 *  captures nothing still publishes the word it read last. */
void nsm_fpga_run_default_capture(struct nsm_fpga_run *run);

#ifdef __cplusplus
}
#endif
#endif
