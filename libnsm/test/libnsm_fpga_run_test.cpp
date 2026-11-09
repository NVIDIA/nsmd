// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "base.h"
#include "fpga/fpga-run.h"
#include "fpga/fpga-tables.h"

#include <array>
#include <cstring>
#include <functional>
#include <map>
#include <string>

#include <gtest/gtest.h>

namespace
{
using Record = std::array<uint8_t, NSM_FPGA_RECORD_SIZE>;

uint64_t getLe(const uint8_t *p, size_t bytes)
{
	uint64_t value = 0;
	for (size_t i = 0; i < bytes; ++i) {
		value |= uint64_t{p[i]} << (8 * i);
	}
	return value;
}

void putLe(Record &record, size_t offset, uint64_t value, size_t bytes)
{
	for (size_t i = 0; i < bytes; ++i) {
		record[offset + i] = static_cast<uint8_t>(value >> (8 * i));
	}
}

// A scripted bank behind the P0 window, answering one MMIO step at a time
struct FakeBank {
	const nsm_fpga_map *map = nsm_fpga_map_p0();
	std::map<uint64_t, uint32_t> words{{0x05000400ULL, 1}};
	int pendingCompletions = 0;
	int faultCompletions = 0;
	int foreignByteReplies = 0;
	bool nackBytes = false;
	uint8_t byteData = 0;
	uint8_t acquireResult = NSM_FPGA_CTRL_OK;
	std::function<void(uint64_t)> onRead;
	std::function<void(uint32_t, uint8_t &, bool &)> onByte;
	size_t exchanges = 0;
	size_t waits = 0;
	size_t controls = 0;
	size_t words_sent = 0;
	Record control{};
	Record header{};
	// the station address and data of the last word record, as the three
	// frame header and extension once carried them
	Record extension{};

	Record authority() const
	{
		Record r{};
		putLe(r, 0, NSM_FPGA_PROFILE_IBC2, 4);
		r[4] = 1;
		r[5] = NSM_FPGA_AUTH_INITIALIZED | NSM_FPGA_AUTH_AVAILABLE |
		       NSM_FPGA_AUTH_RELEASED;
		r[6] = NSM_FPGA_OWNER_NONE;
		putLe(r, 8, 0x68e7a1c0, 8);
		putLe(r, 20, 3, 4);
		return r;
	}

	Record controlReply() const
	{
		Record r{};
		std::memcpy(r.data(), control.data(), 4);
		r[4] = control[4];
		r[5] = (control[4] == NSM_FPGA_CTRL_ACQUIRE ||
			control[4] == NSM_FPGA_CTRL_RECOVER ||
			control[4] == NSM_FPGA_CTRL_ACTIVATE)
			   ? acquireResult
			   : static_cast<uint8_t>(NSM_FPGA_CTRL_OK);
		r[6] = NSM_FPGA_OWNER_NSM;
		r[7] = 1;
		std::memcpy(r.data() + 8, control.data() + 8, 24);
		return r;
	}

	bool busByte(uint32_t argument, uint8_t &data)
	{
		data = byteData;
		bool nack = nackBytes;
		if (onByte) {
			onByte(argument, data, nack);
		}
		return !nack;
	}

	// A bounded transaction through the same scripted bus, byte by byte
	Record transaction(Record r, uint8_t opcode, uint32_t argument)
	{
		const uint8_t writes = (opcode >> 3) & 0x3;
		const uint8_t reads = opcode & 0x7;
		const uint32_t device = argument & 0x7f;
		uint32_t payload = 0;
		for (uint8_t i = 0; i < writes; ++i) {
			const uint32_t byte =
			    (argument >> (8 * (i + 1))) & 0xff;
			const uint32_t flags =
			    (i == 0 ? NSM_FPGA_I2C_START : 0) |
			    (reads == 0 && i + 1 == writes ? NSM_FPGA_I2C_STOP
							   : 0);
			uint8_t data = 0;
			if (!busByte(device | (byte << 8) | (flags << 16),
				     data)) {
				r[4] = NSM_FPGA_RESULT_TARGET_NACK;
				r[5] = NSM_FPGA_FLAG_OCCUPIED |
				       NSM_FPGA_FLAG_QUIESCENT |
				       NSM_FPGA_FLAG_BUS_RELEASED;
				r[28] = 1;
				r[29] = i;
				return r;
			}
		}
		for (uint8_t i = 0; i < reads; ++i) {
			const bool last = i + 1 == reads;
			const uint32_t flags =
			    NSM_FPGA_I2C_READ |
			    (i == 0 ? NSM_FPGA_I2C_START : 0) |
			    (last ? NSM_FPGA_I2C_STOP | NSM_FPGA_I2C_READ_NACK
				  : 0);
			uint8_t data = 0;
			if (!busByte(device | (flags << 16), data)) {
				r[4] = NSM_FPGA_RESULT_TARGET_NACK;
				r[5] = NSM_FPGA_FLAG_OCCUPIED |
				       NSM_FPGA_FLAG_QUIESCENT |
				       NSM_FPGA_FLAG_BUS_RELEASED;
				r[28] = 1;
				r[29] = static_cast<uint8_t>(writes | (i << 4));
				return r;
			}
			payload |= static_cast<uint32_t>(data) << (8 * i);
		}
		r[4] = NSM_FPGA_RESULT_SUCCESS;
		r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
		       NSM_FPGA_FLAG_BUS_RELEASED |
		       (reads ? NSM_FPGA_FLAG_READ_VALID : 0);
		putLe(r, 28, payload, 4);
		return r;
	}

	Record completion()
	{
		Record r{};
		std::memcpy(r.data(), header.data(), 4);
		r[6] = NSM_FPGA_OWNER_NSM;
		r[7] = header[4];
		std::memcpy(r.data() + 8, header.data() + 8, 20);
		if (pendingCompletions > 0) {
			pendingCompletions--;
			r[4] = NSM_FPGA_RESULT_PENDING;
			r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_PENDING;
			return r;
		}
		if (faultCompletions > 0) {
			faultCompletions--;
			r[4] = NSM_FPGA_RESULT_FAULT;
			r[5] = NSM_FPGA_FLAG_OCCUPIED |
			       NSM_FPGA_FLAG_QUIESCENT | NSM_FPGA_FLAG_FAULTED;
			r[29] = 0x2;
			return r;
		}
		if (header[4] == NSM_FPGA_OP_WRITE32) {
			r[4] = NSM_FPGA_RESULT_SUCCESS;
			r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT;
			return r;
		}
		if (nsm_fpga_xfer_opcode_valid(header[4])) {
			return transaction(r, header[4],
					   static_cast<uint32_t>(
					       getLe(header.data() + 28, 4)));
		}
		if (header[4] == NSM_FPGA_OP_BYTE) {
			if (foreignByteReplies > 0) {
				// the engine still answers for the previous op
				foreignByteReplies--;
				putLe(r, 24, getLe(header.data() + 24, 4) + 1,
				      4);
			}
			const auto argument =
			    static_cast<uint32_t>(getLe(header.data() + 28, 4));
			const bool read = ((argument >> 16) & 1) != 0;
			const bool stop = ((argument >> 18) & 1) != 0;
			uint8_t data = byteData;
			bool nack = nackBytes;
			if (onByte) {
				onByte(argument, data, nack);
			}
			if (nack) {
				r[4] = NSM_FPGA_RESULT_TARGET_NACK;
				r[5] = NSM_FPGA_FLAG_OCCUPIED |
				       NSM_FPGA_FLAG_QUIESCENT |
				       NSM_FPGA_FLAG_BUS_RELEASED;
				return r;
			}
			r[4] = NSM_FPGA_RESULT_SUCCESS;
			r[5] = NSM_FPGA_FLAG_OCCUPIED |
			       NSM_FPGA_FLAG_QUIESCENT |
			       (read ? NSM_FPGA_FLAG_READ_VALID : 0) |
			       (stop ? NSM_FPGA_FLAG_BUS_RELEASED
				     : NSM_FPGA_FLAG_BUS_HELD);
			r[28] = read ? data : 0;
			return r;
		}
		r[4] = NSM_FPGA_RESULT_SUCCESS;
		r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
		       NSM_FPGA_FLAG_READ_VALID;
		const uint64_t address = getLe(extension.data() + 20, 8);
		const auto it = words.find(address);
		putLe(r, 28, it == words.end() ? 0 : it->second, 4);
		if (onRead && header[4] == NSM_FPGA_OP_READ32) {
			onRead(address);
		}
		return r;
	}

	// Answer one step; false when the address is not part of the window
	bool answer(const nsm_fpga_step &step, Record &reply)
	{
		exchanges++;
		const nsm_fpga_window &w = map->window;
		if (step.kind == NSM_FPGA_STEP_WRITE) {
			Record record{};
			std::memcpy(record.data(), step.record, record.size());
			if (step.address == w.control_write) {
				control = record;
				controls++;
			} else if (step.address == w.compact) {
				// unfold the word record into a header view
				// and an address and data view
				words_sent++;
				header = Record{};
				extension = Record{};
				putLe(header, 0, NSM_FPGA_PROFILE_IBC2, 4);
				header[4] = record[21];
				header[5] = record[20];
				header[6] = record[22];
				std::memcpy(header.data() + 8, record.data(),
					    20);
				std::memcpy(extension.data(), record.data(),
					    20);
				putLe(extension, 20,
				      getLe(record.data() + 24, 4), 8);
				putLe(extension, 28,
				      getLe(record.data() + 28, 4), 4);
				if (record[21] == NSM_FPGA_OP_WRITE32) {
					words[getLe(extension.data() + 20, 8)] =
					    static_cast<uint32_t>(getLe(
						extension.data() + 28, 4));
				}
			} else if (step.address == w.data_write) {
				header = record;
			} else {
				return false;
			}
			return true;
		}
		if (step.address == w.status) {
			reply = authority();
		} else if (step.address == w.control_read) {
			reply = controlReply();
		} else if (step.address == w.completion_read) {
			reply = completion();
		} else {
			return false;
		}
		return true;
	}

	// Run the job to its end; lostAt (1-based exchange) loses that answer
	nsm_fpga_run_action drive(nsm_fpga_run &run, size_t lostAt = 0,
				  size_t noSessionAt = 0)
	{
		for (int guard = 0; guard < 4000; ++guard) {
			const nsm_fpga_step *step = nullptr;
			const auto action = nsm_fpga_run_next(&run, &step);
			if (action == NSM_FPGA_RUN_WAIT) {
				waits++;
				continue;
			}
			if (action != NSM_FPGA_RUN_SEND) {
				return action;
			}
			Record reply{};
			if (!answer(*step, reply)) {
				ADD_FAILURE() << "step outside the window";
				return NSM_FPGA_RUN_FAILED;
			}
			const bool lost = exchanges == lostAt;
			const bool noSession = exchanges == noSessionAt;
			nsm_fpga_run_reply(&run,
					   step->kind == NSM_FPGA_STEP_READ
					       ? reply.data()
					       : nullptr,
					   lost, noSession);
		}
		ADD_FAILURE() << "job did not end";
		return NSM_FPGA_RUN_FAILED;
	}
};

struct Runner {
	FakeBank bank;
	nsm_fpga_session session{};
	nsm_fpga_run run{};

	Runner()
	{
		session.grant = {0x68e7a1c0, 7, 4};
		nsm_fpga_run_init(&run, bank.map, &session, nullptr);
		nsm_fpga_run_begin_round(&run);
	}

	nsm_fpga_run_action rows(const nsm_fpga_command *list, size_t count)
	{
		nsm_fpga_run_rows(&run, list, count);
		return bank.drive(run);
	}
};

const nsm_fpga_command readCalibration[] = {{.op = NSM_FPGA_OP_READ32,
					     .station = 29,
					     .capture = NSM_FPGA_BUF_ITEM,
					     .u = {.csr = {0x05000400ULL, 0}}}};
} // namespace

TEST(fpgaRun, wordReadIsTwoFramesAndCapturesTheWord)
{
	Runner r;
	r.bank.words[0x05000400ULL] = 0x1234;
	EXPECT_EQ(r.rows(readCalibration, 1), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.exchanges, 2u);
	EXPECT_EQ(r.run.last_word, 0x1234u);
	ASSERT_EQ(r.run.item_len, 4u);
	EXPECT_EQ(r.run.item[0], 0x34);
	EXPECT_EQ(r.run.item[1], 0x12);
	EXPECT_TRUE(r.run.any_read);
	EXPECT_FALSE(r.run.wrote_any);
	EXPECT_EQ(r.session.data_sequence, 1u);
	// the header carried the sequence and the station
	EXPECT_EQ(r.bank.header[24], 1);
	EXPECT_EQ(r.bank.header[5], 29);
}

TEST(fpgaRun, pollWaitsBetweenReadsUntilTheBitIsSet)
{
	Runner r;
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .poll = NSM_FPGA_POLL_SET,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}}}};
	r.bank.words[0x0500045cULL] = 0;
	// the bit comes up after the second read, so the third read sees it
	int reads = 0;
	r.bank.onRead = [&](uint64_t) {
		if (++reads == 2) {
			r.bank.words[0x0500045cULL] = 0x1;
		}
	};
	EXPECT_EQ(r.rows(rows, 1), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.waits, 2u);
	EXPECT_EQ(r.bank.exchanges, 6u);
	EXPECT_EQ(r.run.last_polled, 0x1u);
	EXPECT_EQ(r.run.wait_us, 5000u);
	EXPECT_EQ(r.session.data_sequence, 3u);
}

TEST(fpgaRun, pollBoundFailsTheItemWithoutFencing)
{
	Runner r;
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .poll = NSM_FPGA_POLL_SET,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}},
	     .period_us = 100,
	     .limit = 3}};
	r.bank.words[0x0500045cULL] = 0;
	EXPECT_EQ(r.rows(rows, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_EQ(r.bank.exchanges, 6u);
	EXPECT_EQ(r.bank.waits, 2u);
	EXPECT_EQ(r.run.wait_us, 100u);
	EXPECT_FALSE(r.run.fence);
	EXPECT_NE(std::strstr(r.run.error, "poll condition"), nullptr);
}

TEST(fpgaRun, pendingCompletionIsReadAgainWithinTheBound)
{
	Runner r;
	r.bank.pendingCompletions = 2;
	EXPECT_EQ(r.rows(readCalibration, 1), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.exchanges, 4u);
	Runner stuck;
	stuck.bank.pendingCompletions = 100;
	EXPECT_EQ(stuck.rows(readCalibration, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_EQ(stuck.bank.exchanges, 2u + 16u);
	EXPECT_FALSE(stuck.run.fence);
	EXPECT_NE(std::strstr(stuck.run.error, "pending"), nullptr);
}

TEST(fpgaRun, expectFailureFailsTheItemOrEndsTheGroup)
{
	Runner r;
	static const nsm_fpga_command item[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .u = {.csr = {0x05000400ULL, 0}},
	     .expect = {.mask = 0x7, .value = 0}}};
	EXPECT_EQ(r.rows(item, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(r.run.end_group);
	EXPECT_FALSE(r.run.fence);
	EXPECT_NE(std::strstr(r.run.error, "expected state"), nullptr);
	static const nsm_fpga_command group[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .u = {.csr = {0x05000400ULL, 0}},
	     .expect = {.mask = 0x7, .value = 0},
	     .on_expect_fail = NSM_FPGA_END_GROUP,
	     .group_state = NSM_FPGA_STATE_ABSENT,
	     .group_health = NSM_FPGA_HEALTH_OK}};
	Runner g;
	EXPECT_EQ(g.rows(group, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(g.run.end_group);
	EXPECT_EQ(g.run.group_state, NSM_FPGA_STATE_ABSENT);
	EXPECT_EQ(g.run.group_health, NSM_FPGA_HEALTH_OK);
	EXPECT_FALSE(g.run.fence);
}

TEST(fpgaRun, checkRowsReadTheGroupBufferWithoutFrames)
{
	Runner r;
	r.run.group[0] = 0x12;
	r.run.group[1] = 0x02;
	r.run.group_len = 2;
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_CHECK,
	     .station = 1,
	     .expect = {.buffer = NSM_FPGA_BUF_GROUP,
			.width = 1,
			.offset = 1,
			.mask = 0x10,
			.value = 0},
	     .on_expect_fail = NSM_FPGA_END_GROUP,
	     .group_state = NSM_FPGA_STATE_ABSENT},
	    {.op = NSM_FPGA_OP_CHECK,
	     .station = 1,
	     .expect = {.buffer = NSM_FPGA_BUF_GROUP,
			.width = 1,
			.offset = 1,
			.mask = 0x02,
			.value = 0x02},
	     .on_expect_fail = NSM_FPGA_END_GROUP,
	     .group_state = NSM_FPGA_STATE_UNAVAILABLE_OFFLINE,
	     .group_health = NSM_FPGA_HEALTH_WARNING}};
	EXPECT_EQ(r.rows(rows, 2), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.exchanges, 0u);
	r.run.group[1] = 0x10;
	EXPECT_EQ(r.rows(rows, 2), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(r.run.end_group);
	EXPECT_EQ(r.run.group_state, NSM_FPGA_STATE_ABSENT);
	EXPECT_EQ(r.bank.exchanges, 0u);
}

TEST(fpgaRun, runIfSkipsRowsWhoseConditionFails)
{
	Runner r;
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .capture = NSM_FPGA_BUF_ITEM,
	     .u = {.csr = {0x05000400ULL, 0}}},
	    {.op = NSM_FPGA_OP_WRITE32,
	     .station = 29,
	     .u = {.csr = {0x05000438ULL, 0x55}},
	     .run_if = {.buffer = NSM_FPGA_BUF_ITEM,
			.width = 1,
			.mask = 0xff,
			.value = 0}}};
	r.bank.words[0x05000400ULL] = 1;
	EXPECT_EQ(r.rows(rows, 2), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.words.count(0x05000438ULL), 0u);
	EXPECT_FALSE(r.run.wrote_any);
	Runner w;
	w.bank.words[0x05000400ULL] = 0;
	EXPECT_EQ(w.rows(rows, 2), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(w.bank.words[0x05000438ULL], 0x55u);
	EXPECT_TRUE(w.run.wrote_any);
}

TEST(fpgaRun, wordsAreOneRecordThenTheResult)
{
	Runner r;
	r.bank.words[0x05000400ULL] = 0x1234;
	EXPECT_EQ(r.rows(readCalibration, 1), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.exchanges, 2u);
	EXPECT_EQ(r.bank.words_sent, 1u);
	EXPECT_EQ(r.run.last_word, 0x1234u);
	ASSERT_EQ(r.run.item_len, 4u);
	EXPECT_EQ(r.run.item[0], 0x34);
	EXPECT_EQ(r.session.data_sequence, 1u);
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .poll = NSM_FPGA_POLL_SET,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}}},
	    {.op = NSM_FPGA_OP_ACK_CLEAR,
	     .station = 29,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}}}};
	Runner w;
	w.bank.words[0x0500045cULL] = 0x75;
	EXPECT_EQ(w.rows(rows, 2), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(w.bank.words[0x0500045cULL], 0x74u);
	EXPECT_EQ(w.bank.exchanges, 4u);
	EXPECT_EQ(w.bank.words_sent, 2u);
	// a pending completion is read again, a lost answer still fences
	Runner p;
	p.bank.pendingCompletions = 2;
	EXPECT_EQ(p.rows(readCalibration, 1), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(p.bank.exchanges, 4u);
	Runner l;
	nsm_fpga_run_rows(&l.run, readCalibration, 1);
	EXPECT_EQ(l.bank.drive(l.run, 2), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(l.run.fence);
}

TEST(fpgaRun, ackClearWritesThePolledWordWithTheBitClear)
{
	Runner r;
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .poll = NSM_FPGA_POLL_SET,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}}},
	    {.op = NSM_FPGA_OP_ACK_CLEAR,
	     .station = 29,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}}}};
	r.bank.words[0x0500045cULL] = 0x75;
	EXPECT_EQ(r.rows(rows, 2), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.words[0x0500045cULL], 0x74u);
	EXPECT_TRUE(r.run.wrote_any);
	EXPECT_EQ(r.bank.exchanges, 4u);
}

TEST(fpgaRun, rememberedReadFeedsAckClearGatedByTheWord)
{
	// A stale response is acknowledged, an idle status is left alone
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .remember = 1,
	     .u = {.csr = {0x0500045cULL, 0}}},
	    {.op = NSM_FPGA_OP_ACK_CLEAR,
	     .station = 29,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}},
	     .run_if = {
		 .buffer = NSM_FPGA_BUF_WORD, .mask = 0x1, .value = 0x1}}};
	Runner stale;
	stale.bank.words[0x0500045cULL] = 0x75;
	EXPECT_EQ(stale.rows(rows, 2), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(stale.bank.words[0x0500045cULL], 0x74u);
	EXPECT_TRUE(stale.run.wrote_any);
	EXPECT_EQ(stale.bank.exchanges, 4u);
	Runner idle;
	idle.bank.words[0x0500045cULL] = 0x74;
	EXPECT_EQ(idle.rows(rows, 2), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(idle.bank.words[0x0500045cULL], 0x74u);
	EXPECT_FALSE(idle.run.wrote_any);
	EXPECT_EQ(idle.bank.exchanges, 2u);
}

TEST(fpgaRun, cleanStationSkipsItsGuardRowsUntilSomethingTouchesIt)
{
	// Guard rows clear a stale response; the closing ack leaves the station
	// clean, so the next item on it skips the guard until a failure, an
	// ACQUIRE or a RELEASE makes the station's state unknown again
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .remember = 1,
	     .clean = NSM_FPGA_UNLESS_CLEAN,
	     .u = {.csr = {0x0500045cULL, 0}}},
	    {.op = NSM_FPGA_OP_ACK_CLEAR,
	     .station = 29,
	     .clean = NSM_FPGA_UNLESS_CLEAN,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}},
	     .run_if = {.buffer = NSM_FPGA_BUF_WORD,
			.mask = 0x1,
			.value = 0x1}},
	    {.op = NSM_FPGA_OP_WRITE32,
	     .station = 29,
	     .u = {.csr = {0x0500045cULL, 0x1}}},
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .remember = 1,
	     .u = {.csr = {0x0500045cULL, 0}}},
	    {.op = NSM_FPGA_OP_ACK_CLEAR,
	     .station = 29,
	     .clean = NSM_FPGA_LEAVES_CLEAN,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}}}};
	static const nsm_fpga_command failing[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .poll = NSM_FPGA_POLL_SET,
	     .poll_mask = 0x2,
	     .u = {.csr = {0x0500045cULL, 0}},
	     .limit = 2}};
	Runner r;
	// The round runs inside a held session
	nsm_fpga_run_control(&r.run, NSM_FPGA_CTRL_ACQUIRE, 0);
	EXPECT_EQ(r.bank.drive(r.run), NSM_FPGA_RUN_DONE);
	r.bank.words[0x0500045cULL] = 0x1;
	auto item = [&](const nsm_fpga_command *list, size_t count) {
		const size_t before = r.bank.exchanges;
		nsm_fpga_run_begin_item(&r.run);
		const auto action = r.rows(list, count);
		return std::make_pair(action, r.bank.exchanges - before);
	};
	// A stale response: both guard rows run, five word ops
	EXPECT_EQ(item(rows, 5), std::make_pair(NSM_FPGA_RUN_DONE, size_t{10}));
	EXPECT_EQ(r.bank.words[0x0500045cULL], 0u);
	// Left clean: the guard rows are skipped
	EXPECT_EQ(item(rows, 5), std::make_pair(NSM_FPGA_RUN_DONE, size_t{6}));
	// A RENEW keeps the session, so the station stays clean
	nsm_fpga_run_control(&r.run, NSM_FPGA_CTRL_RENEW, 0);
	EXPECT_EQ(r.bank.drive(r.run), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(item(rows, 5), std::make_pair(NSM_FPGA_RUN_DONE, size_t{6}));
	// A failed item on the station makes its state unknown: the guard
	// reads again and finds nothing stale to acknowledge
	EXPECT_EQ(item(failing, 1).first, NSM_FPGA_RUN_FAILED);
	EXPECT_EQ(item(rows, 5), std::make_pair(NSM_FPGA_RUN_DONE, size_t{8}));
	// So does a new session, whose control sequence starts afresh
	r.session.control_sequence = 0;
	nsm_fpga_run_control(&r.run, NSM_FPGA_CTRL_ACQUIRE, 0);
	EXPECT_EQ(r.bank.drive(r.run), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(item(rows, 5), std::make_pair(NSM_FPGA_RUN_DONE, size_t{8}));
}

TEST(fpgaRun, popDrainsUntilTheStatusShowsNoCredit)
{
	Runner r;
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_POP,
	     .station = 37,
	     .poll_mask = 0xfffffffc,
	     .u = {.pop = {0x18ULL, 0x14ULL}},
	     .limit = 8}};
	r.bank.words[0x18ULL] = 3 << 2;
	size_t dataReads = 0;
	r.bank.onRead = [&](uint64_t address) {
		if (address == 0x14ULL) {
			dataReads++;
			r.bank.words[0x18ULL] -= 1 << 2;
		}
	};
	EXPECT_EQ(r.rows(rows, 1), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(dataReads, 3u);
	// four status reads and three data reads, two frames each
	EXPECT_EQ(r.bank.exchanges, 14u);
	Runner stuck;
	stuck.bank.words[0x18ULL] = 1 << 2;
	EXPECT_EQ(stuck.rows(rows, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_NE(std::strstr(stuck.run.error, "not drained"), nullptr);
}

TEST(fpgaRun, writeCapturedTakesTheGroupBytes)
{
	Runner r;
	r.run.group[0] = 0x08;
	r.run.group[1] = 0x44;
	r.run.group[2] = 0x33;
	r.run.group[3] = 0x22;
	r.run.group[4] = 0x11;
	r.run.group_len = 5;
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_WRITE_CAPTURED,
	     .station = 1,
	     .u = {.captured = {.dev_addr = 0x71,
				.flags = NSM_FPGA_I2C_START | NSM_FPGA_I2C_STOP,
				.offset = 0}}},
	    {.op = NSM_FPGA_OP_WRITE_CAPTURED,
	     .station = 29,
	     .u = {.captured = {.address = 0x05000438ULL, .offset = 1}}}};
	uint32_t argument = 0;
	r.bank.onByte = [&](uint32_t arg, uint8_t &, bool &) {
		argument = arg;
	};
	EXPECT_EQ(r.rows(rows, 2), NSM_FPGA_RUN_DONE);
	// the byte op carried the mux address, the captured mask and the flags
	EXPECT_EQ(argument, 0x71u | (0x08u << 8) | (0x6u << 16));
	EXPECT_EQ(r.bank.words[0x05000438ULL], 0x11223344u);
	EXPECT_TRUE(r.run.wrote_any);
	// a buffer that does not reach the offset fails before the wire
	Runner s;
	EXPECT_EQ(s.rows(rows, 2), NSM_FPGA_RUN_FAILED);
	EXPECT_EQ(s.bank.exchanges, 0u);
}

TEST(fpgaRun, transactionWritesReadsCapturesAndNackFailsWithoutFence)
{
	// one row: pointer byte 0x0e then two bytes read, captured
	Runner r;
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_I2C_XFER,
	     .station = 1,
	     .capture = NSM_FPGA_BUF_ITEM,
	     .u = {.xfer = {0x50, {0x0e, 0, 0}, 1, 2, 0, 0}}}};
	std::vector<uint32_t> seen;
	r.bank.onByte = [&](uint32_t argument, uint8_t &data, bool &) {
		seen.push_back(argument);
		data = static_cast<uint8_t>(0x10 + seen.size());
	};
	EXPECT_EQ(r.rows(rows, 1), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.exchanges, 2u);
	ASSERT_EQ(r.run.item_len, 2u);
	EXPECT_EQ(r.run.item[0], 0x12);
	EXPECT_EQ(r.run.item[1], 0x13);
	EXPECT_TRUE(r.run.wrote_any);
	// the bus saw start+write, repeated start read, read with nack+stop
	ASSERT_EQ(seen.size(), 3u);
	EXPECT_EQ(seen[0], 0x50u | (0x0eu << 8) | (NSM_FPGA_I2C_START << 16));
	EXPECT_EQ(seen[1],
		  0x50u | ((NSM_FPGA_I2C_READ | NSM_FPGA_I2C_START) << 16));
	EXPECT_EQ(seen[2], 0x50u | ((NSM_FPGA_I2C_READ | NSM_FPGA_I2C_STOP |
				     NSM_FPGA_I2C_READ_NACK)
				    << 16));
	// a NACK ends the item without a fence; the controller released the bus
	Runner n;
	n.bank.nackBytes = true;
	EXPECT_EQ(n.rows(rows, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(n.run.fence);
	EXPECT_NE(std::strstr(n.run.error, "nack"), nullptr);
	// a captured write byte comes from the group buffer
	Runner c;
	static const nsm_fpga_command restore[] = {
	    {.op = NSM_FPGA_OP_I2C_XFER,
	     .station = 1,
	     .u = {.xfer = {0x50, {0x7e, 0, 0}, 3, 0, 0x6, 2}}}};
	c.run.group[2] = 0x01;
	c.run.group[3] = 0x11;
	c.run.group_len = 4;
	std::vector<uint32_t> wrote;
	c.bank.onByte = [&](uint32_t argument, uint8_t &, bool &) {
		wrote.push_back(argument);
	};
	EXPECT_EQ(c.rows(restore, 1), NSM_FPGA_RUN_DONE);
	ASSERT_EQ(wrote.size(), 3u);
	EXPECT_EQ((wrote[1] >> 8) & 0xff, 0x01u);
	EXPECT_EQ((wrote[2] >> 8) & 0xff, 0x11u);
	EXPECT_NE(wrote[2] & (NSM_FPGA_I2C_STOP << 16), 0u);
	Runner s;
	s.run.group_len = 1;
	EXPECT_EQ(s.rows(restore, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(s.run.fence);
}

TEST(fpgaRun, byteReadCapturesAndNackFailsWithoutFence)
{
	Runner r;
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_BYTE,
	     .station = 1,
	     .u = {.i2c = {0x50, 0x0e, NSM_FPGA_I2C_START}}},
	    {.op = NSM_FPGA_OP_BYTE,
	     .station = 1,
	     .capture = NSM_FPGA_BUF_ITEM,
	     .u = {.i2c = {0x50, 0x00,
			   NSM_FPGA_I2C_READ | NSM_FPGA_I2C_STOP |
			       NSM_FPGA_I2C_READ_NACK}}}};
	r.bank.byteData = 0x5a;
	EXPECT_EQ(r.rows(rows, 2), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.exchanges, 4u);
	ASSERT_EQ(r.run.item_len, 1u);
	EXPECT_EQ(r.run.item[0], 0x5a);
	EXPECT_TRUE(r.run.wrote_any);
	EXPECT_EQ(r.session.data_sequence, 2u);
	Runner n;
	n.bank.nackBytes = true;
	EXPECT_EQ(n.rows(rows, 2), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(n.run.fence);
	EXPECT_NE(std::strstr(n.run.error, "nack"), nullptr);
	EXPECT_EQ(n.bank.exchanges, 2u);
	// a stale answer from the previous byte op is read past
	Runner f;
	f.bank.foreignByteReplies = 1;
	f.bank.byteData = 0x33;
	EXPECT_EQ(f.rows(rows, 2), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(f.bank.exchanges, 5u);
	EXPECT_EQ(f.run.item[0], 0x33);
}

TEST(fpgaRun, faultForeignNoSessionAndLostSetFence)
{
	Runner fault;
	fault.bank.faultCompletions = 1;
	EXPECT_EQ(fault.rows(readCalibration, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(fault.run.fence);
	EXPECT_NE(std::strstr(fault.run.error, "faulted"), nullptr);
	Runner foreign;
	foreign.session.grant.session = 9;
	nsm_fpga_run_rows(&foreign.run, readCalibration, 1);
	// the bank answers with another session's identity
	const nsm_fpga_step *step = nullptr;
	EXPECT_EQ(nsm_fpga_run_next(&foreign.run, &step), NSM_FPGA_RUN_SEND);
	Record sent{};
	foreign.bank.answer(*step, sent);
	nsm_fpga_run_reply(&foreign.run, nullptr, false, false);
	EXPECT_EQ(nsm_fpga_run_next(&foreign.run, &step), NSM_FPGA_RUN_SEND);
	Record reply{};
	foreign.bank.answer(*step, reply);
	putLe(reply, 16, 1234, 4);
	nsm_fpga_run_reply(&foreign.run, reply.data(), false, false);
	EXPECT_EQ(nsm_fpga_run_next(&foreign.run, &step), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(foreign.run.fence);
	EXPECT_NE(std::strstr(foreign.run.error, "foreign"), nullptr);
	Runner noSession;
	nsm_fpga_run_rows(&noSession.run, readCalibration, 1);
	EXPECT_EQ(noSession.bank.drive(noSession.run, 0, 2),
		  NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(noSession.run.fence);
	EXPECT_NE(std::strstr(noSession.run.error, "no session"), nullptr);
	Runner lostSet;
	nsm_fpga_run_rows(&lostSet.run, readCalibration, 1);
	EXPECT_EQ(lostSet.bank.drive(lostSet.run, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(lostSet.run.fence);
	/* the request went out, so a lost completion read fences too */
	Runner lostGet;
	nsm_fpga_run_rows(&lostGet.run, readCalibration, 1);
	EXPECT_EQ(lostGet.bank.drive(lostGet.run, 2), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(lostGet.run.fence);
	EXPECT_EQ(lostGet.session.data_sequence, 0u);
	/* a lost status read published nothing and does not fence */
	Runner lostStatus;
	nsm_fpga_run_status(&lostStatus.run);
	EXPECT_EQ(lostStatus.bank.drive(lostStatus.run, 1),
		  NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(lostStatus.run.fence);
}

// A bank answering garbage: every job must end within the poll and repeat
// bounds, never read or write past the item and group buffers, and never
// count a garbage reply as a verified data operation
TEST(fpgaRun, randomRepliesEndEveryJobInsideItsBounds)
{
	uint32_t seed = 0xc0ffee11u;
	auto next = [&seed]() {
		seed = seed * 1664525u + 1013904223u;
		return seed;
	};
	static const nsm_fpga_command rows[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .capture = NSM_FPGA_BUF_ITEM,
	     .u = {.csr = {0x05000400ULL, 0}}},
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .poll = NSM_FPGA_POLL_SET,
	     .capture = NSM_FPGA_BUF_GROUP,
	     .poll_mask = 0x1,
	     .u = {.csr = {0x0500045cULL, 0}}},
	    {.op = NSM_FPGA_OP_I2C_XFER,
	     .station = 1,
	     .capture = NSM_FPGA_BUF_ITEM,
	     .u = {.xfer = {0x50, {0x00, 0, 0}, 1, 2, 0, 0}}}};
	for (int trial = 0; trial < 300; ++trial) {
		Runner r;
		nsm_fpga_run_rows(&r.run, rows, 3);
		nsm_fpga_run_action action = NSM_FPGA_RUN_SEND;
		int guard = 0;
		for (; guard < 5000; ++guard) {
			const nsm_fpga_step *step = nullptr;
			action = nsm_fpga_run_next(&r.run, &step);
			if (action == NSM_FPGA_RUN_WAIT) {
				continue;
			}
			if (action != NSM_FPGA_RUN_SEND) {
				break;
			}
			Record reply{};
			for (auto &byte : reply) {
				byte = static_cast<uint8_t>(next() >> 24);
			}
			if ((next() & 3) == 0) {
				putLe(reply, 0, NSM_FPGA_PROFILE_IBC2, 4);
			}
			nsm_fpga_run_reply(&r.run,
					   step->kind == NSM_FPGA_STEP_READ
					       ? reply.data()
					       : nullptr,
					   false, false);
		}
		EXPECT_TRUE(guard < 5000);
		EXPECT_EQ(action, NSM_FPGA_RUN_FAILED);
		EXPECT_TRUE(r.run.item_len <= sizeof(r.run.item));
		EXPECT_TRUE(r.run.group_len <= sizeof(r.run.group));
		EXPECT_EQ(r.session.data_sequence, 0u);
	}
}

TEST(fpgaRun, illegalWordIsRefusedBeforeTheWire)
{
	Runner r;
	static const nsm_fpga_command misaligned[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .u = {.csr = {0x05000401ULL, 0}}}};
	EXPECT_EQ(r.rows(misaligned, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_EQ(r.bank.exchanges, 0u);
	EXPECT_FALSE(r.run.fence);
	EXPECT_NE(std::strstr(r.run.error, "blocked"), nullptr);
	static const nsm_fpga_command quarantine[] = {
	    {.op = NSM_FPGA_OP_WRITE32,
	     .station = 37,
	     .u = {.csr = {0x04ULL, 0x07000019u}}}};
	Runner q;
	EXPECT_EQ(q.rows(quarantine, 1), NSM_FPGA_RUN_FAILED);
	EXPECT_EQ(q.bank.exchanges, 0u);
}

TEST(fpgaRun, controlAndStatusJobs)
{
	Runner r;
	nsm_fpga_run_status(&r.run);
	EXPECT_EQ(r.bank.drive(r.run), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.exchanges, 1u);
	EXPECT_EQ(r.run.authority.epoch, 0x68e7a1c0u);
	EXPECT_EQ(r.run.authority.generation, 3u);
	nsm_fpga_run_control(&r.run, NSM_FPGA_CTRL_ACQUIRE, 0);
	EXPECT_EQ(r.bank.drive(r.run), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.exchanges, 3u);
	EXPECT_EQ(r.bank.controls, 1u);
	EXPECT_EQ(r.session.control_sequence, 1u);
	EXPECT_EQ(r.bank.control[4], NSM_FPGA_CTRL_ACQUIRE);
	nsm_fpga_run_control(&r.run, NSM_FPGA_CTRL_RELEASE, 5);
	EXPECT_EQ(r.bank.drive(r.run), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.session.control_sequence, 2u);
	EXPECT_EQ(r.bank.control[28], 5);
	Runner rejected;
	rejected.bank.acquireResult = NSM_FPGA_CTRL_BUSY;
	nsm_fpga_run_control(&rejected.run, NSM_FPGA_CTRL_ACQUIRE, 0);
	EXPECT_EQ(rejected.bank.drive(rejected.run), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(rejected.run.fence);
	EXPECT_NE(std::strstr(rejected.run.error, "rejected"), nullptr);
	EXPECT_NE(std::strstr(rejected.run.error, "BUSY"), nullptr);
}

TEST(fpgaRun, recoverCarriesTheEpochAndRefusalDoesNotFence)
{
	Runner r;
	nsm_fpga_run_recover(&r.run, 0x0102030405060708ULL);
	EXPECT_EQ(r.bank.drive(r.run), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.exchanges, 2u);
	EXPECT_EQ(r.bank.controls, 1u);
	EXPECT_EQ(r.bank.control[4], NSM_FPGA_CTRL_RECOVER);
	EXPECT_EQ(r.bank.control[8], 0x08);
	EXPECT_EQ(r.bank.control[15], 0x01);
	EXPECT_EQ(r.bank.control[16], 0);
	EXPECT_EQ(r.bank.control[24], 0);
	// No session was used, so its sequence is untouched
	EXPECT_EQ(r.session.control_sequence, 0u);
	Runner refused;
	refused.bank.acquireResult = NSM_FPGA_CTRL_DENIED;
	nsm_fpga_run_recover(&refused.run, 0x0102030405060708ULL);
	EXPECT_EQ(refused.bank.drive(refused.run), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(refused.run.fence);
	EXPECT_NE(std::strstr(refused.run.error, "recover refused"), nullptr);
	EXPECT_NE(std::strstr(refused.run.error, "DENIED"), nullptr);
}

TEST(fpgaRun, activateCarriesTheEpochLikeRecover)
{
	Runner r;
	nsm_fpga_run_activate(&r.run, 0x0102030405060708ULL);
	EXPECT_EQ(r.bank.drive(r.run), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.bank.exchanges, 2u);
	EXPECT_EQ(r.bank.control[4], NSM_FPGA_CTRL_ACTIVATE);
	EXPECT_EQ(r.bank.control[8], 0x08);
	EXPECT_EQ(r.bank.control[16], 0);
	EXPECT_EQ(r.session.control_sequence, 0u);
	Runner refused;
	refused.bank.acquireResult = NSM_FPGA_CTRL_DENIED;
	nsm_fpga_run_activate(&refused.run, 0x0102030405060708ULL);
	EXPECT_EQ(refused.bank.drive(refused.run), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(refused.run.fence);
	EXPECT_NE(std::strstr(refused.run.error, "activate refused"), nullptr);
}

TEST(fpgaRun, beginItemKeepsTheGroupBufferAndDefaultCapture)
{
	Runner r;
	static const nsm_fpga_command toGroup[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .capture = NSM_FPGA_BUF_GROUP,
	     .u = {.csr = {0x05000400ULL, 0}}}};
	r.bank.words[0x05000400ULL] = 0xabcd;
	EXPECT_EQ(r.rows(toGroup, 1), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.run.group_len, 4u);
	nsm_fpga_run_begin_item(&r.run);
	EXPECT_EQ(r.run.group_len, 4u);
	EXPECT_EQ(r.run.item_len, 0u);
	EXPECT_FALSE(r.run.any_read);
	static const nsm_fpga_command plain[] = {
	    {.op = NSM_FPGA_OP_READ32,
	     .station = 29,
	     .u = {.csr = {0x05000400ULL, 0}}}};
	EXPECT_EQ(r.rows(plain, 1), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(r.run.item_len, 0u);
	nsm_fpga_run_default_capture(&r.run);
	ASSERT_EQ(r.run.item_len, 4u);
	EXPECT_EQ(r.run.item[0], 0xcd);
	EXPECT_EQ(r.run.item[1], 0xab);
	nsm_fpga_run_begin_round(&r.run);
	EXPECT_EQ(r.run.group_len, 0u);
}

TEST(fpgaRun, theMapRowsRunOnTheBank)
{
	// Every rule of the map runs to its end against the scripted bank
	const nsm_fpga_map *p0 = nsm_fpga_map_p0();
	for (size_t p = 0; p < p0->part_count; ++p) {
		const nsm_fpga_part *part = p0->parts[p];
		for (size_t i = 0; i < part->rule_count; ++i) {
			const nsm_fpga_rule *rule = &part->rules[i];
			if (rule->command_count == 0) {
				continue;
			}
			Runner r;
			r.bank.words[0x0500045cULL] = 0x1;
			r.bank.words[0x08ULL] = 1;
			r.bank.words[0x18ULL] = 0;
			const auto action =
			    r.rows(rule->commands, rule->command_count);
			// a precheck that the scripted bank does not satisfy
			// fails cleanly, everything else runs through
			EXPECT_TRUE(
			    action == NSM_FPGA_RUN_DONE ||
			    (action == NSM_FPGA_RUN_FAILED && !r.run.fence))
			    << rule->name << ": " << r.run.error;
		}
	}
}
