// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "base.h"
#include "fpga/fpga-controller.h"
#include "fpga/fpga-run.h"
#include "fpga/fpga-tables.h"

#include <array>
#include <cmath>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

// One negative test per public libnsm FPGA function: the inputs each one
// must refuse, and that a refusal leaves its outputs untouched.

namespace
{
using Record = std::array<uint8_t, NSM_FPGA_RECORD_SIZE>;

const nsm_fpga_identity validIdentity{0x1122334455667788ULL, 0x0a0b0c0d, 2, 1};
const nsm_fpga_grant validGrant{0x1122334455667788ULL, 0x0a0b0c0d, 2};
const nsm_fpga_window windowP0{2,      0x1000, 0x1020, 0x1040,
			       0x1060, 0x10a0, 0x10e0};

void putLe(Record &record, size_t offset, uint64_t value, size_t bytes)
{
	for (size_t i = 0; i < bytes; ++i) {
		record[offset + i] = static_cast<uint8_t>(value >> (8 * i));
	}
}

void putIdentity(Record &r, size_t offset, const nsm_fpga_identity &id)
{
	putLe(r, offset, id.epoch, 8);
	putLe(r, offset + 8, id.session, 4);
	putLe(r, offset + 12, id.generation, 4);
	putLe(r, offset + 16, id.sequence, 4);
}

Record controlReply(const nsm_fpga_identity &id, uint8_t opcode,
		    uint32_t argument, uint8_t result, bool pending,
		    uint8_t owner = NSM_FPGA_OWNER_NSM)
{
	Record r{};
	putLe(r, 0, NSM_FPGA_PROFILE_IBC2, 4);
	r[4] = opcode;
	r[5] = result;
	r[6] = owner;
	r[7] = static_cast<uint8_t>(1 | (pending ? 2 : 0));
	putIdentity(r, 8, id);
	putLe(r, 28, argument, 4);
	return r;
}

Record wordReply(const nsm_fpga_identity &id, uint8_t result, uint8_t flags,
		 uint32_t data, uint8_t opcode = NSM_FPGA_OP_READ32,
		 uint8_t owner = NSM_FPGA_OWNER_NSM)
{
	Record r{};
	putLe(r, 0, NSM_FPGA_PROFILE_IBC2, 4);
	r[4] = result;
	r[5] = flags;
	r[6] = owner;
	r[7] = opcode;
	putIdentity(r, 8, id);
	putLe(r, 28, data, 4);
	return r;
}

Record statusRecord(uint8_t flags, uint8_t owner, uint64_t epoch)
{
	Record r{};
	putLe(r, 0, NSM_FPGA_PROFILE_IBC2, 4);
	r[4] = 1;
	r[5] = flags;
	r[6] = owner;
	putLe(r, 8, epoch, 8);
	putLe(r, 16, 7, 4);
	putLe(r, 20, 3, 4);
	return r;
}

Record filled(uint8_t byte)
{
	Record r{};
	r.fill(byte);
	return r;
}

nsm_fpga_control_completion controlCompletion(uint8_t owner, uint8_t opcode,
					      uint32_t argument, uint8_t result,
					      bool pending)
{
	nsm_fpga_control_completion c{};
	c.identity = validIdentity;
	c.opcode = opcode;
	c.result = result;
	c.owner = owner;
	c.occupied = true;
	c.pending = pending;
	c.argument = argument;
	return c;
}

nsm_fpga_word_completion wordCompletion(uint8_t result, uint8_t flags,
					uint32_t data,
					uint8_t opcode = NSM_FPGA_OP_READ32,
					uint8_t owner = NSM_FPGA_OWNER_NSM)
{
	nsm_fpga_word_completion c{};
	c.identity = validIdentity;
	c.result = result;
	c.flags = flags;
	c.owner = owner;
	c.opcode = opcode;
	c.data = data;
	return c;
}

nsm_fpga_authority_status status(uint8_t flags, uint8_t owner, uint64_t epoch,
				 uint32_t generation = 3)
{
	nsm_fpga_authority_status s{};
	s.epoch = epoch;
	s.session = 7;
	s.generation = generation;
	s.owner = owner;
	s.flags = flags;
	return s;
}

constexpr uint8_t ready = NSM_FPGA_AUTH_INITIALIZED | NSM_FPGA_AUTH_AVAILABLE;
constexpr uint8_t lapsed = NSM_FPGA_AUTH_QUARANTINED | NSM_FPGA_AUTH_QUIESCENT |
			   NSM_FPGA_AUTH_RELEASED;

// A runner over the P0 map that hands back one scripted record per GET
struct Driver {
	nsm_fpga_session session{};
	nsm_fpga_run run{};

	Driver()
	{
		session.grant = validGrant;
		nsm_fpga_run_init(&run, nsm_fpga_map_p0(), &session, nullptr);
		nsm_fpga_run_begin_round(&run);
	}

	// Send steps until the first GET, then answer it with record
	nsm_fpga_run_action answer(const uint8_t *record)
	{
		for (int guard = 0; guard < 8; ++guard) {
			const nsm_fpga_step *step = nullptr;
			const auto action = nsm_fpga_run_next(&run, &step);
			if (action != NSM_FPGA_RUN_SEND) {
				return action;
			}
			if (step->kind == NSM_FPGA_STEP_WRITE) {
				nsm_fpga_run_reply(&run, nullptr, false, false);
				continue;
			}
			nsm_fpga_run_reply(&run, record, false, false);
			const nsm_fpga_step *next = nullptr;
			return nsm_fpga_run_next(&run, &next);
		}
		return NSM_FPGA_RUN_FAILED;
	}
};

const nsm_fpga_command readDdr5[] = {{.op = NSM_FPGA_OP_READ32,
				      .station = 29,
				      .capture = NSM_FPGA_BUF_ITEM,
				      .u = {.csr = {0x05000400ULL, 0}}}};
} // namespace

TEST(fpgaNegative, nsm_fpga_identity_valid)
{
	EXPECT_FALSE(nsm_fpga_identity_valid(nullptr));
	nsm_fpga_identity id = validIdentity;
	id.epoch = 0;
	EXPECT_FALSE(nsm_fpga_identity_valid(&id));
	id = validIdentity;
	id.session = 0;
	EXPECT_FALSE(nsm_fpga_identity_valid(&id));
	id = validIdentity;
	id.generation = 0;
	EXPECT_FALSE(nsm_fpga_identity_valid(&id));
	id = validIdentity;
	id.sequence = 0;
	EXPECT_FALSE(nsm_fpga_identity_valid(&id));
	id.sequence = NSM_FPGA_MAX_SEQUENCE + 1;
	EXPECT_FALSE(nsm_fpga_identity_valid(&id));
}

TEST(fpgaNegative, nsm_fpga_encode_control)
{
	Record untouched = filled(0xaa);
	Record r = untouched;
	EXPECT_EQ(nsm_fpga_encode_control(&validIdentity, NSM_FPGA_CTRL_ACQUIRE,
					  0, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_encode_control(nullptr, NSM_FPGA_CTRL_ACQUIRE, 0,
					  r.data()),
		  NSM_SW_ERROR_DATA);
	nsm_fpga_identity id = validIdentity;
	id.sequence = 2;
	EXPECT_EQ(
	    nsm_fpga_encode_control(&id, NSM_FPGA_CTRL_ACQUIRE, 0, r.data()),
	    NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_control(&validIdentity, NSM_FPGA_CTRL_RENEW,
					  0, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(
	    nsm_fpga_encode_control(&id, NSM_FPGA_CTRL_RENEW, 1, r.data()),
	    NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_control(&id, NSM_FPGA_CTRL_RELEASE,
					  NSM_FPGA_MAX_SEQUENCE + 1, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_control(&id, 0x99, 0, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_control(&validIdentity,
					  NSM_FPGA_CTRL_ACTIVATE, 0, r.data()),
		  NSM_SW_ERROR_DATA);
	const nsm_fpga_identity noEpoch{0, 0, 0, 0};
	EXPECT_EQ(nsm_fpga_encode_control(&noEpoch, NSM_FPGA_CTRL_RECOVER, 0,
					  r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(r, untouched);
}

TEST(fpgaNegative, nsm_fpga_decode_control_completion)
{
	nsm_fpga_control_completion c{};
	Record r =
	    controlReply(validIdentity, NSM_FPGA_CTRL_ACQUIRE, 0, 0, false);
	EXPECT_EQ(nsm_fpga_decode_control_completion(nullptr, &c),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_decode_control_completion(r.data(), nullptr),
		  NSM_SW_ERROR_NULL);
	Record bad = r;
	putLe(bad, 0, 0x12345678, 4);
	EXPECT_EQ(nsm_fpga_decode_control_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	bad = r;
	bad[5] = NSM_FPGA_CTRL_EXHAUSTED + 1;
	EXPECT_EQ(nsm_fpga_decode_control_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	bad = r;
	bad[7] = 0x04;
	EXPECT_EQ(nsm_fpga_decode_control_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	Record empty{};
	putLe(empty, 0, NSM_FPGA_PROFILE_IBC2, 4);
	empty[31] = 1;
	EXPECT_EQ(nsm_fpga_decode_control_completion(empty.data(), &c),
		  NSM_SW_ERROR_DATA);
	bad = controlReply(validIdentity, NSM_FPGA_CTRL_ACQUIRE, 0, 0, false,
			   NSM_FPGA_OWNER_NONE);
	EXPECT_EQ(nsm_fpga_decode_control_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	bad = controlReply(validIdentity, 0x77, 0, 0, false);
	EXPECT_EQ(nsm_fpga_decode_control_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	bad = controlReply(validIdentity, NSM_FPGA_CTRL_RENEW, 0,
			   NSM_FPGA_CTRL_BUSY, true);
	EXPECT_EQ(nsm_fpga_decode_control_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
}

TEST(fpgaNegative, nsm_fpga_match_control)
{
	const auto ok = controlCompletion(NSM_FPGA_OWNER_NSM,
					  NSM_FPGA_CTRL_ACQUIRE, 0, 0, false);
	EXPECT_EQ(nsm_fpga_match_control(nullptr, &validIdentity,
					 NSM_FPGA_CTRL_ACQUIRE, 0),
		  NSM_FPGA_MATCH_REJECTED);
	EXPECT_EQ(
	    nsm_fpga_match_control(&ok, nullptr, NSM_FPGA_CTRL_ACQUIRE, 0),
	    NSM_FPGA_MATCH_REJECTED);
	auto c = controlCompletion(NSM_FPGA_OWNER_NATIVE, NSM_FPGA_CTRL_ACQUIRE,
				   0, 0, false);
	EXPECT_EQ(nsm_fpga_match_control(&c, &validIdentity,
					 NSM_FPGA_CTRL_ACQUIRE, 0),
		  NSM_FPGA_MATCH_FOREIGN);
	EXPECT_EQ(
	    nsm_fpga_match_control(&ok, &validIdentity, NSM_FPGA_CTRL_RENEW, 0),
	    NSM_FPGA_MATCH_FOREIGN);
	EXPECT_EQ(nsm_fpga_match_control(&ok, &validIdentity,
					 NSM_FPGA_CTRL_ACQUIRE, 5),
		  NSM_FPGA_MATCH_FOREIGN);
	nsm_fpga_identity other = validIdentity;
	other.sequence = 9;
	EXPECT_EQ(nsm_fpga_match_control(&ok, &other, NSM_FPGA_CTRL_ACQUIRE, 0),
		  NSM_FPGA_MATCH_FOREIGN);
	c = controlCompletion(NSM_FPGA_OWNER_NSM, NSM_FPGA_CTRL_ACQUIRE, 0,
			      NSM_FPGA_CTRL_BUSY, false);
	EXPECT_EQ(nsm_fpga_match_control(&c, &validIdentity,
					 NSM_FPGA_CTRL_ACQUIRE, 0),
		  NSM_FPGA_MATCH_REJECTED);
}

TEST(fpgaNegative, nsm_fpga_decode_word_completion)
{
	nsm_fpga_word_completion c{};
	const uint8_t good = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
			     NSM_FPGA_FLAG_READ_VALID;
	Record r = wordReply(validIdentity, NSM_FPGA_RESULT_SUCCESS, good, 1);
	EXPECT_EQ(nsm_fpga_decode_word_completion(nullptr, &c),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_decode_word_completion(r.data(), nullptr),
		  NSM_SW_ERROR_NULL);
	Record bad = r;
	bad[0] ^= 0xff;
	EXPECT_EQ(nsm_fpga_decode_word_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	bad = r;
	bad[4] = NSM_FPGA_RESULT_FAULT + 1;
	EXPECT_EQ(nsm_fpga_decode_word_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	bad = r;
	bad[5] |= 0x80;
	EXPECT_EQ(nsm_fpga_decode_word_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	Record empty{};
	putLe(empty, 0, NSM_FPGA_PROFILE_IBC2, 4);
	empty[28] = 1;
	EXPECT_EQ(nsm_fpga_decode_word_completion(empty.data(), &c),
		  NSM_SW_ERROR_DATA);
	nsm_fpga_identity id = validIdentity;
	id.epoch = 0;
	bad = wordReply(id, NSM_FPGA_RESULT_SUCCESS, good, 1);
	EXPECT_EQ(nsm_fpga_decode_word_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	bad = wordReply(validIdentity, NSM_FPGA_RESULT_SUCCESS, good, 1,
			NSM_FPGA_OP_READ32, NSM_FPGA_OWNER_NONE);
	EXPECT_EQ(nsm_fpga_decode_word_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	bad = wordReply(validIdentity, NSM_FPGA_RESULT_SUCCESS, good, 1, 0x05);
	EXPECT_EQ(nsm_fpga_decode_word_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
	bad = wordReply(validIdentity, NSM_FPGA_RESULT_SUCCESS,
			NSM_FPGA_FLAG_QUIESCENT | NSM_FPGA_FLAG_READ_VALID, 1);
	EXPECT_EQ(nsm_fpga_decode_word_completion(bad.data(), &c),
		  NSM_SW_ERROR_DATA);
}

TEST(fpgaNegative, nsm_fpga_match_word)
{
	const uint8_t good = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
			     NSM_FPGA_FLAG_READ_VALID;
	const auto ok = wordCompletion(NSM_FPGA_RESULT_SUCCESS, good, 1);
	EXPECT_EQ(
	    nsm_fpga_match_word(nullptr, &validIdentity, NSM_FPGA_OP_READ32),
	    NSM_FPGA_MATCH_REJECTED);
	EXPECT_EQ(nsm_fpga_match_word(&ok, nullptr, NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_REJECTED);
	auto c = wordCompletion(NSM_FPGA_RESULT_SUCCESS, good, 1,
				NSM_FPGA_OP_READ32, NSM_FPGA_OWNER_NATIVE);
	EXPECT_EQ(nsm_fpga_match_word(&c, &validIdentity, NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_FOREIGN);
	EXPECT_EQ(nsm_fpga_match_word(&ok, &validIdentity, NSM_FPGA_OP_WRITE32),
		  NSM_FPGA_MATCH_FOREIGN);
	c = wordCompletion(
	    NSM_FPGA_RESULT_SUCCESS,
	    good | NSM_FPGA_FLAG_BUS_HELD | NSM_FPGA_FLAG_BUS_RELEASED, 1);
	EXPECT_EQ(nsm_fpga_match_word(&c, &validIdentity, NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_REJECTED);
	c = wordCompletion(NSM_FPGA_RESULT_PENDING,
			   NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_PENDING |
			       NSM_FPGA_FLAG_QUIESCENT,
			   0);
	EXPECT_EQ(nsm_fpga_match_word(&c, &validIdentity, NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_REJECTED);
	c = wordCompletion(NSM_FPGA_RESULT_PENDING,
			   NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_PENDING, 5);
	EXPECT_EQ(nsm_fpga_match_word(&c, &validIdentity, NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_REJECTED);
	c = wordCompletion(NSM_FPGA_RESULT_SUCCESS,
			   NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT, 1);
	EXPECT_EQ(nsm_fpga_match_word(&c, &validIdentity, NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_REJECTED);
	c = wordCompletion(NSM_FPGA_RESULT_SUCCESS,
			   NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT, 1,
			   NSM_FPGA_OP_WRITE32);
	EXPECT_EQ(nsm_fpga_match_word(&c, &validIdentity, NSM_FPGA_OP_WRITE32),
		  NSM_FPGA_MATCH_REJECTED);
	c = wordCompletion(NSM_FPGA_RESULT_FAULT,
			   NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_FAULTED, 0);
	c.fault_reason = 0;
	EXPECT_EQ(nsm_fpga_match_word(&c, &validIdentity, NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_REJECTED);
	c = wordCompletion(NSM_FPGA_RESULT_TARGET_NACK, good, 1);
	EXPECT_EQ(nsm_fpga_match_word(&c, &validIdentity, NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_REJECTED);
}

TEST(fpgaNegative, nsm_fpga_session_classify)
{
	EXPECT_EQ(nsm_fpga_session_classify(nullptr),
		  NSM_FPGA_SESSION_UNREACHABLE);
	const auto noFlags = status(0, NSM_FPGA_OWNER_NONE, 0x10);
	EXPECT_EQ(nsm_fpga_session_classify(&noFlags),
		  NSM_FPGA_SESSION_UNINITIALIZED);
	const auto notAvailable =
	    status(NSM_FPGA_AUTH_INITIALIZED, NSM_FPGA_OWNER_NONE, 0x10);
	EXPECT_EQ(nsm_fpga_session_classify(&notAvailable),
		  NSM_FPGA_SESSION_UNINITIALIZED);
}

TEST(fpgaNegative, nsm_fpga_decode_authority_status)
{
	nsm_fpga_authority_status s{};
	const Record r = statusRecord(ready, NSM_FPGA_OWNER_NONE, 0x10);
	EXPECT_EQ(nsm_fpga_decode_authority_status(nullptr, &s),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_decode_authority_status(r.data(), nullptr),
		  NSM_SW_ERROR_NULL);
	Record bad = r;
	bad[1] ^= 0xff;
	EXPECT_EQ(nsm_fpga_decode_authority_status(bad.data(), &s),
		  NSM_SW_ERROR_DATA);
	bad = r;
	bad[4] = 2;
	EXPECT_EQ(nsm_fpga_decode_authority_status(bad.data(), &s),
		  NSM_SW_ERROR_DATA);
	bad = r;
	bad[5] |= 0x80;
	EXPECT_EQ(nsm_fpga_decode_authority_status(bad.data(), &s),
		  NSM_SW_ERROR_DATA);
	bad = r;
	bad[6] = NSM_FPGA_OWNER_NSM + 1;
	EXPECT_EQ(nsm_fpga_decode_authority_status(bad.data(), &s),
		  NSM_SW_ERROR_DATA);
	bad = r;
	bad[7] = 1;
	EXPECT_EQ(nsm_fpga_decode_authority_status(bad.data(), &s),
		  NSM_SW_ERROR_DATA);
	bad = r;
	bad[31] = 1;
	EXPECT_EQ(nsm_fpga_decode_authority_status(bad.data(), &s),
		  NSM_SW_ERROR_DATA);
	bad = statusRecord(ready, NSM_FPGA_OWNER_NONE, 0);
	EXPECT_EQ(nsm_fpga_decode_authority_status(bad.data(), &s),
		  NSM_SW_ERROR_DATA);
	bad = statusRecord(0, NSM_FPGA_OWNER_NONE, 0x10);
	EXPECT_EQ(nsm_fpga_decode_authority_status(bad.data(), &s),
		  NSM_SW_ERROR_DATA);
}

TEST(fpgaNegative, nsm_fpga_control_result_name)
{
	EXPECT_STREQ(nsm_fpga_control_result_name(NSM_FPGA_CTRL_EXHAUSTED + 1),
		     "UNKNOWN");
	EXPECT_STREQ(nsm_fpga_control_result_name(0xff), "UNKNOWN");
}

TEST(fpgaNegative, nsm_fpga_authority_acquirable)
{
	EXPECT_FALSE(nsm_fpga_authority_acquirable(nullptr));
	auto s = status(ready | NSM_FPGA_AUTH_QUARANTINED, NSM_FPGA_OWNER_NONE,
			0x10);
	EXPECT_FALSE(nsm_fpga_authority_acquirable(&s));
	s = status(NSM_FPGA_AUTH_INITIALIZED, NSM_FPGA_OWNER_NONE, 0x10);
	EXPECT_FALSE(nsm_fpga_authority_acquirable(&s));
	s = status(NSM_FPGA_AUTH_AVAILABLE, NSM_FPGA_OWNER_NONE, 0x10);
	EXPECT_FALSE(nsm_fpga_authority_acquirable(&s));
	s = status(ready, NSM_FPGA_OWNER_NONE, 0);
	EXPECT_FALSE(nsm_fpga_authority_acquirable(&s));
}

TEST(fpgaNegative, nsm_fpga_authority_recoverable)
{
	EXPECT_FALSE(nsm_fpga_authority_recoverable(nullptr));
	auto s = status(lapsed, NSM_FPGA_OWNER_NATIVE, 0x10);
	EXPECT_FALSE(nsm_fpga_authority_recoverable(&s));
	s = status(lapsed, NSM_FPGA_OWNER_NONE, 0x10);
	EXPECT_FALSE(nsm_fpga_authority_recoverable(&s));
	s = status(NSM_FPGA_AUTH_QUARANTINED | NSM_FPGA_AUTH_RELEASED,
		   NSM_FPGA_OWNER_NSM, 0x10);
	EXPECT_FALSE(nsm_fpga_authority_recoverable(&s));
	s = status(NSM_FPGA_AUTH_QUARANTINED | NSM_FPGA_AUTH_QUIESCENT,
		   NSM_FPGA_OWNER_NSM, 0x10);
	EXPECT_FALSE(nsm_fpga_authority_recoverable(&s));
	s = status(NSM_FPGA_AUTH_QUIESCENT | NSM_FPGA_AUTH_RELEASED,
		   NSM_FPGA_OWNER_NSM, 0x10);
	EXPECT_FALSE(nsm_fpga_authority_recoverable(&s));
}

TEST(fpgaNegative, nsm_fpga_authority_activatable)
{
	EXPECT_FALSE(nsm_fpga_authority_activatable(nullptr));
	auto s = status(NSM_FPGA_AUTH_INITIALIZED, NSM_FPGA_OWNER_NONE, 0);
	EXPECT_FALSE(nsm_fpga_authority_activatable(&s));
	s = status(NSM_FPGA_AUTH_QUARANTINED, NSM_FPGA_OWNER_NONE, 0);
	EXPECT_FALSE(nsm_fpga_authority_activatable(&s));
	s = status(NSM_FPGA_AUTH_GRANT_VALID, NSM_FPGA_OWNER_NONE, 0);
	EXPECT_FALSE(nsm_fpga_authority_activatable(&s));
	s = status(0, NSM_FPGA_OWNER_NSM, 0);
	EXPECT_FALSE(nsm_fpga_authority_activatable(&s));
	s = status(0, NSM_FPGA_OWNER_NONE, 0x10);
	EXPECT_FALSE(nsm_fpga_authority_activatable(&s));
}

TEST(fpgaNegative, nsm_fpga_grant_from_authority)
{
	uint64_t epoch = 0xdead;
	uint32_t generation = 0xbeef;
	const auto ok = status(ready, NSM_FPGA_OWNER_NONE, 0x10);
	EXPECT_EQ(
	    nsm_fpga_grant_from_authority(nullptr, 7, &epoch, &generation),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_grant_from_authority(&ok, 7, nullptr, &generation),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_grant_from_authority(&ok, 7, &epoch, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_grant_from_authority(&ok, 0, &epoch, &generation),
		  NSM_SW_ERROR_DATA);
	const auto quarantined = status(ready | NSM_FPGA_AUTH_QUARANTINED,
					NSM_FPGA_OWNER_NONE, 0x10);
	EXPECT_EQ(
	    nsm_fpga_grant_from_authority(&quarantined, 7, &epoch, &generation),
	    NSM_SW_ERROR_DATA);
	const auto exhausted =
	    status(ready, NSM_FPGA_OWNER_NONE, 0x10, UINT32_MAX);
	EXPECT_EQ(
	    nsm_fpga_grant_from_authority(&exhausted, 7, &epoch, &generation),
	    NSM_SW_ERROR_DATA);
	EXPECT_EQ(epoch, 0xdeadu);
	EXPECT_EQ(generation, 0xbeefu);
}

TEST(fpgaNegative, nsm_fpga_plan_identity)
{
	const auto id = nsm_fpga_plan_identity(nullptr, 5);
	EXPECT_EQ(id.epoch, 0u);
	EXPECT_EQ(id.session, 0u);
	EXPECT_EQ(id.generation, 0u);
	EXPECT_EQ(id.sequence, 0u);
	EXPECT_FALSE(nsm_fpga_identity_valid(&id));
	const auto zero = nsm_fpga_plan_identity(&validGrant, 0);
	EXPECT_FALSE(nsm_fpga_identity_valid(&zero));
}

TEST(fpgaNegative, nsm_fpga_plan_control)
{
	nsm_fpga_step steps[NSM_FPGA_PLAN_CONTROL_STEPS]{};
	EXPECT_EQ(nsm_fpga_plan_control(nullptr, &validGrant,
					NSM_FPGA_CTRL_ACQUIRE, 1, 0, steps),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_control(&windowP0, nullptr,
					NSM_FPGA_CTRL_ACQUIRE, 1, 0, steps),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_control(&windowP0, &validGrant,
					NSM_FPGA_CTRL_ACQUIRE, 1, 0, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_control(&windowP0, &validGrant,
					NSM_FPGA_CTRL_ACQUIRE, 2, 0, steps),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_plan_control(&windowP0, &validGrant,
					NSM_FPGA_CTRL_RENEW, 1, 0, steps),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_plan_control(&windowP0, &validGrant,
					NSM_FPGA_CTRL_ACTIVATE, 1, 0, steps),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(
	    nsm_fpga_plan_control(&windowP0, &validGrant, 0x99, 2, 0, steps),
	    NSM_SW_ERROR_DATA);
}

TEST(fpgaNegative, nsm_fpga_encode_word_compact)
{
	Record untouched = filled(0x55);
	Record r = untouched;
	EXPECT_EQ(nsm_fpga_encode_word_compact(&validIdentity, 3,
					       NSM_FPGA_OP_READ32, 0x0f, 0x100,
					       0, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_encode_word_compact(nullptr, 3, NSM_FPGA_OP_READ32,
					       0x0f, 0x100, 0, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&validIdentity, 3,
					       NSM_FPGA_OP_READ32, 0x0f, 0x102,
					       0, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&validIdentity, 3,
					       NSM_FPGA_OP_READ32, 0x0f,
					       0x100000000ULL, 0, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&validIdentity, 3,
					       NSM_FPGA_OP_READ32, 0x03, 0x100,
					       0, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&validIdentity, 3,
					       NSM_FPGA_OP_READ32, 0x0f, 0x100,
					       1, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&validIdentity, 3,
					       NSM_FPGA_OP_WRITE32, 0x00, 0x100,
					       0, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&validIdentity, 3,
					       NSM_FPGA_OP_WRITE32, 0x10, 0x100,
					       0, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&validIdentity, 3,
					       NSM_FPGA_OP_WRITE32, 0x01, 0x100,
					       0x0100, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&validIdentity, 3,
					       NSM_FPGA_OP_BYTE, 0x0f, 0x100, 0,
					       r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(r, untouched);
}

TEST(fpgaNegative, nsm_fpga_plan_word_compact)
{
	nsm_fpga_step steps[NSM_FPGA_PLAN_COMPACT_STEPS]{};
	EXPECT_EQ(nsm_fpga_plan_word_compact(nullptr, &validGrant, 1, 3,
					     NSM_FPGA_OP_READ32, 0x100, 0,
					     steps),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_word_compact(&windowP0, nullptr, 1, 3,
					     NSM_FPGA_OP_READ32, 0x100, 0,
					     steps),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_word_compact(&windowP0, &validGrant, 1, 3,
					     NSM_FPGA_OP_READ32, 0x100, 0,
					     nullptr),
		  NSM_SW_ERROR_NULL);
	nsm_fpga_window noCompact = windowP0;
	noCompact.compact = 0;
	EXPECT_EQ(nsm_fpga_plan_word_compact(&noCompact, &validGrant, 1, 3,
					     NSM_FPGA_OP_READ32, 0x100, 0,
					     steps),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_plan_word_compact(&windowP0, &validGrant, 1, 3,
					     NSM_FPGA_OP_READ32, 0x101, 0,
					     steps),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_plan_word_compact(&windowP0, &validGrant, 0, 3,
					     NSM_FPGA_OP_READ32, 0x100, 0,
					     steps),
		  NSM_SW_ERROR_DATA);
}

TEST(fpgaNegative, nsm_fpga_i2c_argument_valid)
{
	EXPECT_FALSE(nsm_fpga_i2c_argument_valid(0x50 | 0x80));
	EXPECT_FALSE(nsm_fpga_i2c_argument_valid(0x50 | (1u << 20)));
	EXPECT_FALSE(nsm_fpga_i2c_argument_valid(0x07));
	EXPECT_FALSE(nsm_fpga_i2c_argument_valid(0x78));
	EXPECT_FALSE(nsm_fpga_i2c_argument_valid(0x50 | (0x11u << 8) |
						 (NSM_FPGA_I2C_READ << 16)));
	EXPECT_FALSE(nsm_fpga_i2c_argument_valid(
	    0x50 | ((NSM_FPGA_I2C_READ | NSM_FPGA_I2C_STOP) << 16)));
	EXPECT_FALSE(
	    nsm_fpga_i2c_argument_valid(0x50 | (NSM_FPGA_I2C_READ_NACK << 16)));
}

TEST(fpgaNegative, nsm_fpga_encode_byte_header)
{
	Record untouched = filled(0x33);
	Record r = untouched;
	EXPECT_EQ(nsm_fpga_encode_byte_header(&validIdentity, 0, 0x50, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_encode_byte_header(nullptr, 0, 0x50, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(
	    nsm_fpga_encode_byte_header(&validIdentity, 0, 0x07, r.data()),
	    NSM_SW_ERROR_DATA);
	EXPECT_EQ(r, untouched);
}

TEST(fpgaNegative, nsm_fpga_xfer_opcode_valid)
{
	EXPECT_FALSE(nsm_fpga_xfer_opcode_valid(0x40));
	EXPECT_FALSE(nsm_fpga_xfer_opcode_valid(0x45));
	EXPECT_FALSE(nsm_fpga_xfer_opcode_valid(0x47));
	EXPECT_FALSE(nsm_fpga_xfer_opcode_valid(0x20));
	EXPECT_FALSE(nsm_fpga_xfer_opcode_valid(0x60));
	EXPECT_FALSE(nsm_fpga_xfer_opcode_valid(NSM_FPGA_OP_BYTE));
	EXPECT_FALSE(nsm_fpga_xfer_opcode_valid(NSM_FPGA_OP_READ32));
}

TEST(fpgaNegative, nsm_fpga_encode_xfer_header)
{
	Record untouched = filled(0x44);
	Record r = untouched;
	const uint8_t oneWriteOneRead = 0x49;
	EXPECT_EQ(nsm_fpga_encode_xfer_header(&validIdentity, 1,
					      oneWriteOneRead, 0x50, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_encode_xfer_header(nullptr, 1, oneWriteOneRead, 0x50,
					      r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_xfer_header(&validIdentity, 1, 0x40, 0x50,
					      r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_xfer_header(&validIdentity, 1,
					      oneWriteOneRead, 0x50 | 0x80,
					      r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_xfer_header(&validIdentity, 1,
					      oneWriteOneRead, 0x7f, r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_xfer_header(&validIdentity, 1,
					      oneWriteOneRead,
					      0x50 | (0x22u << 16), r.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(r, untouched);
}

TEST(fpgaNegative, nsm_fpga_plan_i2c_xfer)
{
	nsm_fpga_step steps[NSM_FPGA_PLAN_BYTE_STEPS]{};
	EXPECT_EQ(nsm_fpga_plan_i2c_xfer(nullptr, &validGrant, 1, 1, 0x49, 0x50,
					 steps),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    nsm_fpga_plan_i2c_xfer(&windowP0, nullptr, 1, 1, 0x49, 0x50, steps),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_i2c_xfer(&windowP0, &validGrant, 1, 1, 0x49,
					 0x50, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_i2c_xfer(&windowP0, &validGrant, 1, 1, 0x40,
					 0x50, steps),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_plan_i2c_xfer(&windowP0, &validGrant, 0, 1, 0x49,
					 0x50, steps),
		  NSM_SW_ERROR_DATA);
}

TEST(fpgaNegative, nsm_fpga_plan_i2c_byte)
{
	nsm_fpga_step steps[NSM_FPGA_PLAN_BYTE_STEPS]{};
	EXPECT_EQ(
	    nsm_fpga_plan_i2c_byte(nullptr, &validGrant, 1, 0, 0x50, steps),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_i2c_byte(&windowP0, nullptr, 1, 0, 0x50, steps),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    nsm_fpga_plan_i2c_byte(&windowP0, &validGrant, 1, 0, 0x50, nullptr),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    nsm_fpga_plan_i2c_byte(&windowP0, &validGrant, 1, 0, 0x78, steps),
	    NSM_SW_ERROR_DATA);
	EXPECT_EQ(
	    nsm_fpga_plan_i2c_byte(&windowP0, &validGrant, 0, 0, 0x50, steps),
	    NSM_SW_ERROR_DATA);
}

TEST(fpgaNegative, nsm_fpga_plan_authority)
{
	nsm_fpga_step steps[NSM_FPGA_PLAN_CONTROL_STEPS]{};
	EXPECT_EQ(nsm_fpga_plan_authority(nullptr, NSM_FPGA_CTRL_RECOVER, 0x10,
					  steps),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_authority(&windowP0, NSM_FPGA_CTRL_RECOVER,
					  0x10, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_authority(&windowP0, NSM_FPGA_CTRL_ACQUIRE,
					  0x10, steps),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_plan_authority(&windowP0, NSM_FPGA_CTRL_ACTIVATE, 0,
					  steps),
		  NSM_SW_ERROR_DATA);
}

TEST(fpgaNegative, nsm_fpga_plan_recover)
{
	nsm_fpga_step steps[NSM_FPGA_PLAN_CONTROL_STEPS]{};
	EXPECT_EQ(nsm_fpga_plan_recover(nullptr, 0x10, steps),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_recover(&windowP0, 0x10, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_recover(&windowP0, 0, steps),
		  NSM_SW_ERROR_DATA);
}

TEST(fpgaNegative, nsm_fpga_plan_status_read)
{
	nsm_fpga_step step{};
	EXPECT_EQ(nsm_fpga_plan_status_read(nullptr, &step), NSM_SW_ERROR_NULL);
	EXPECT_EQ(nsm_fpga_plan_status_read(&windowP0, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(fpgaNegative, nsm_fpga_classify_control)
{
	uint8_t result = 0xee;
	const Record ok =
	    controlReply(validIdentity, NSM_FPGA_CTRL_ACQUIRE, 0, 0, false);
	EXPECT_EQ(nsm_fpga_classify_control(nullptr, &validIdentity,
					    NSM_FPGA_CTRL_ACQUIRE, 0, &result),
		  NSM_FPGA_REPLY_MALFORMED);
	EXPECT_EQ(nsm_fpga_classify_control(ok.data(), nullptr,
					    NSM_FPGA_CTRL_ACQUIRE, 0, &result),
		  NSM_FPGA_REPLY_MALFORMED);
	const Record junk = filled(0xff);
	EXPECT_EQ(nsm_fpga_classify_control(junk.data(), &validIdentity,
					    NSM_FPGA_CTRL_ACQUIRE, 0, &result),
		  NSM_FPGA_REPLY_MALFORMED);
	EXPECT_EQ(nsm_fpga_classify_control(ok.data(), &validIdentity,
					    NSM_FPGA_CTRL_RENEW, 0, &result),
		  NSM_FPGA_REPLY_FOREIGN);
	EXPECT_EQ(result, 0xee);
	const Record busy = controlReply(validIdentity, NSM_FPGA_CTRL_ACQUIRE,
					 0, NSM_FPGA_CTRL_BUSY, false);
	EXPECT_EQ(nsm_fpga_classify_control(busy.data(), &validIdentity,
					    NSM_FPGA_CTRL_ACQUIRE, 0, &result),
		  NSM_FPGA_REPLY_REJECTED);
	EXPECT_EQ(result, NSM_FPGA_CTRL_BUSY);
}

TEST(fpgaNegative, nsm_fpga_classify_authority)
{
	uint8_t result = 0xee;
	const nsm_fpga_identity epochOnly{0x10, 0, 0, 0};
	EXPECT_EQ(nsm_fpga_classify_authority(nullptr, 0x10,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_MALFORMED);
	const Record junk = filled(0x01);
	EXPECT_EQ(nsm_fpga_classify_authority(junk.data(), 0x10,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_MALFORMED);
	const Record ok = controlReply(epochOnly, NSM_FPGA_CTRL_RECOVER, 0, 0,
				       false, NSM_FPGA_OWNER_NONE);
	EXPECT_EQ(nsm_fpga_classify_authority(ok.data(), 0x11,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_FOREIGN);
	EXPECT_EQ(nsm_fpga_classify_authority(ok.data(), 0x10,
					      NSM_FPGA_CTRL_ACTIVATE, &result),
		  NSM_FPGA_REPLY_FOREIGN);
	const Record withArgument = controlReply(
	    epochOnly, NSM_FPGA_CTRL_RECOVER, 5, 0, false, NSM_FPGA_OWNER_NONE);
	EXPECT_EQ(nsm_fpga_classify_authority(withArgument.data(), 0x10,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_FOREIGN);
	EXPECT_EQ(result, 0xee);
	const Record denied =
	    controlReply(epochOnly, NSM_FPGA_CTRL_RECOVER, 0,
			 NSM_FPGA_CTRL_DENIED, false, NSM_FPGA_OWNER_NONE);
	EXPECT_EQ(nsm_fpga_classify_authority(denied.data(), 0x10,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_REJECTED);
	EXPECT_EQ(result, NSM_FPGA_CTRL_DENIED);
}

TEST(fpgaNegative, nsm_fpga_classify_word)
{
	uint32_t data = 0xabcd;
	uint8_t reason = 0xee;
	const uint8_t good = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
			     NSM_FPGA_FLAG_READ_VALID;
	const Record ok =
	    wordReply(validIdentity, NSM_FPGA_RESULT_SUCCESS, good, 1);
	EXPECT_EQ(nsm_fpga_classify_word(nullptr, &validIdentity,
					 NSM_FPGA_OP_READ32, &data, &reason),
		  NSM_FPGA_REPLY_MALFORMED);
	EXPECT_EQ(nsm_fpga_classify_word(ok.data(), nullptr, NSM_FPGA_OP_READ32,
					 &data, &reason),
		  NSM_FPGA_REPLY_MALFORMED);
	const Record junk = filled(0x80);
	EXPECT_EQ(nsm_fpga_classify_word(junk.data(), &validIdentity,
					 NSM_FPGA_OP_READ32, &data, &reason),
		  NSM_FPGA_REPLY_MALFORMED);
	EXPECT_EQ(nsm_fpga_classify_word(ok.data(), &validIdentity,
					 NSM_FPGA_OP_WRITE32, &data, &reason),
		  NSM_FPGA_REPLY_FOREIGN);
	const Record noRead =
	    wordReply(validIdentity, NSM_FPGA_RESULT_SUCCESS,
		      NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT, 1);
	EXPECT_EQ(nsm_fpga_classify_word(noRead.data(), &validIdentity,
					 NSM_FPGA_OP_READ32, &data, &reason),
		  NSM_FPGA_REPLY_REJECTED);
	EXPECT_EQ(data, 0xabcdu);
	EXPECT_EQ(reason, 0xee);
}

TEST(fpgaNegative, nsm_fpga_classify_i2c_byte)
{
	uint8_t data = 0xee;
	bool nack = true;
	const uint32_t readArgument = 0x50 | (NSM_FPGA_I2C_READ << 16);
	const uint8_t quiet = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT;
	const Record ok =
	    wordReply(validIdentity, NSM_FPGA_RESULT_SUCCESS,
		      quiet | NSM_FPGA_FLAG_READ_VALID | NSM_FPGA_FLAG_BUS_HELD,
		      0x42, NSM_FPGA_OP_BYTE);
	EXPECT_EQ(nsm_fpga_classify_i2c_byte(ok.data(), &validIdentity,
					     readArgument, &data, nullptr),
		  NSM_FPGA_REPLY_MALFORMED);
	EXPECT_EQ(nsm_fpga_classify_i2c_byte(nullptr, &validIdentity,
					     readArgument, &data, &nack),
		  NSM_FPGA_REPLY_MALFORMED);
	EXPECT_EQ(nsm_fpga_classify_i2c_byte(ok.data(), nullptr, readArgument,
					     &data, &nack),
		  NSM_FPGA_REPLY_MALFORMED);
	const Record word = wordReply(validIdentity, NSM_FPGA_RESULT_SUCCESS,
				      quiet | NSM_FPGA_FLAG_READ_VALID, 0x42);
	EXPECT_EQ(nsm_fpga_classify_i2c_byte(word.data(), &validIdentity,
					     readArgument, &data, &nack),
		  NSM_FPGA_REPLY_FOREIGN);
	const Record noRead =
	    wordReply(validIdentity, NSM_FPGA_RESULT_SUCCESS,
		      quiet | NSM_FPGA_FLAG_BUS_HELD, 0, NSM_FPGA_OP_BYTE);
	EXPECT_EQ(nsm_fpga_classify_i2c_byte(noRead.data(), &validIdentity,
					     readArgument, &data, &nack),
		  NSM_FPGA_REPLY_REJECTED);
	const Record bothBus = wordReply(
	    validIdentity, NSM_FPGA_RESULT_TARGET_NACK,
	    quiet | NSM_FPGA_FLAG_BUS_HELD | NSM_FPGA_FLAG_BUS_RELEASED, 0,
	    NSM_FPGA_OP_BYTE);
	EXPECT_EQ(nsm_fpga_classify_i2c_byte(bothBus.data(), &validIdentity,
					     readArgument, &data, &nack),
		  NSM_FPGA_REPLY_REJECTED);
	const Record notQuiet =
	    wordReply(validIdentity, NSM_FPGA_RESULT_TARGET_NACK,
		      NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_BUS_RELEASED, 0,
		      NSM_FPGA_OP_BYTE);
	EXPECT_EQ(nsm_fpga_classify_i2c_byte(notQuiet.data(), &validIdentity,
					     readArgument, &data, &nack),
		  NSM_FPGA_REPLY_REJECTED);
	EXPECT_EQ(data, 0xee);
	EXPECT_FALSE(nack);
}

TEST(fpgaNegative, nsm_fpga_classify_i2c_xfer)
{
	uint32_t data = 0xabcd;
	bool nack = true;
	uint8_t reason = 0xee;
	const uint8_t oneWriteOneRead = 0x49;
	const uint8_t released = NSM_FPGA_FLAG_OCCUPIED |
				 NSM_FPGA_FLAG_QUIESCENT |
				 NSM_FPGA_FLAG_BUS_RELEASED;
	const Record ok = wordReply(validIdentity, NSM_FPGA_RESULT_SUCCESS,
				    released | NSM_FPGA_FLAG_READ_VALID, 0x42,
				    oneWriteOneRead);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(ok.data(), &validIdentity,
					     oneWriteOneRead, &data, nullptr,
					     &reason),
		  NSM_FPGA_REPLY_MALFORMED);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(nullptr, &validIdentity,
					     oneWriteOneRead, &data, &nack,
					     &reason),
		  NSM_FPGA_REPLY_MALFORMED);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(ok.data(), &validIdentity, 0x4a,
					     &data, &nack, &reason),
		  NSM_FPGA_REPLY_FOREIGN);
	const Record tooManyBytes = wordReply(
	    validIdentity, NSM_FPGA_RESULT_SUCCESS,
	    released | NSM_FPGA_FLAG_READ_VALID, 0x4242, oneWriteOneRead);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(tooManyBytes.data(),
					     &validIdentity, oneWriteOneRead,
					     &data, &nack, &reason),
		  NSM_FPGA_REPLY_REJECTED);
	const Record nackHeld =
	    wordReply(validIdentity, NSM_FPGA_RESULT_TARGET_NACK,
		      NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
			  NSM_FPGA_FLAG_BUS_HELD,
		      1, oneWriteOneRead);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(nackHeld.data(), &validIdentity,
					     oneWriteOneRead, &data, &nack,
					     &reason),
		  NSM_FPGA_REPLY_REJECTED);
	const Record faultUnknown = wordReply(
	    validIdentity, NSM_FPGA_RESULT_FAULT,
	    NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_FAULTED, 5, oneWriteOneRead);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(faultUnknown.data(),
					     &validIdentity, oneWriteOneRead,
					     &data, &nack, &reason),
		  NSM_FPGA_REPLY_REJECTED);
	EXPECT_EQ(data, 0xabcdu);
	EXPECT_EQ(reason, 0xee);
	EXPECT_FALSE(nack);
}

TEST(fpgaNegative, nsm_fpga_reply_name)
{
	EXPECT_STREQ(nsm_fpga_reply_name(static_cast<nsm_fpga_reply>(99)),
		     "unknown");
}

TEST(fpgaNegative, nsm_fpga_map_p0)
{
	const nsm_fpga_map *map = nsm_fpga_map_p0();
	ASSERT_NE(map, nullptr);
	EXPECT_EQ(nsm_fpga_station_by_selector(map, 0xff), nullptr);
	EXPECT_EQ(nsm_fpga_find_rule(map, "no_such_item"), nullptr);
	EXPECT_EQ(nsm_fpga_find_group(map, "no_such_group"), nullptr);
	EXPECT_NE(map->window.compact, 0u);
}

TEST(fpgaNegative, nsm_fpga_station_by_selector)
{
	EXPECT_EQ(nsm_fpga_station_by_selector(nullptr, 29), nullptr);
	EXPECT_EQ(nsm_fpga_station_by_selector(nsm_fpga_map_p0(), 38), nullptr);
}

TEST(fpgaNegative, nsm_fpga_find_rule)
{
	const nsm_fpga_map *map = nsm_fpga_map_p0();
	EXPECT_EQ(nsm_fpga_find_rule(nullptr, "Temp_0"), nullptr);
	EXPECT_EQ(nsm_fpga_find_rule(map, nullptr), nullptr);
	EXPECT_EQ(nsm_fpga_find_rule(map, ""), nullptr);
	EXPECT_EQ(nsm_fpga_find_rule(map, "_"), nullptr);
	EXPECT_EQ(nsm_fpga_find_rule(map, "no_such_item"), nullptr);
	// a rule name is matched only after a '_', never inside a word
	for (size_t p = 0; p < map->part_count; ++p) {
		const nsm_fpga_part *part = map->parts[p];
		for (size_t i = 0; i < part->rule_count; ++i) {
			const std::string glued =
			    std::string("x") + part->rules[i].name;
			const nsm_fpga_rule *found =
			    nsm_fpga_find_rule(map, glued.c_str());
			EXPECT_NE(found, &part->rules[i]);
		}
	}
}

TEST(fpgaNegative, nsm_fpga_find_group)
{
	const nsm_fpga_map *map = nsm_fpga_map_p0();
	EXPECT_EQ(nsm_fpga_find_group(nullptr, "DIMM"), nullptr);
	EXPECT_EQ(nsm_fpga_find_group(map, nullptr), nullptr);
	EXPECT_EQ(nsm_fpga_find_group(map, ""), nullptr);
	EXPECT_EQ(nsm_fpga_find_group(map, "dimm"), nullptr);
}

TEST(fpgaNegative, nsm_fpga_i2c_argument)
{
	EXPECT_EQ(nsm_fpga_i2c_argument(nullptr), 0u);
	const nsm_fpga_i2c_op highBits{0xd0, 0x11, 0xff};
	EXPECT_EQ(nsm_fpga_i2c_argument(&highBits),
		  0x50u | (0x11u << 8) | (0xfu << 16));
}

TEST(fpgaNegative, nsm_fpga_xfer_opcode)
{
	EXPECT_EQ(nsm_fpga_xfer_opcode(nullptr), 0u);
	const nsm_fpga_xfer_op overCounts{0x50, {0, 0, 0}, 7, 9, 0, 0};
	EXPECT_EQ(nsm_fpga_xfer_opcode(&overCounts), 0x40u | (3u << 3) | 1u);
}

TEST(fpgaNegative, nsm_fpga_xfer_argument)
{
	const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44};
	EXPECT_EQ(nsm_fpga_xfer_argument(0xd0, bytes, 4),
		  0x50u | (0x11u << 8) | (0x22u << 16) | (0x33u << 24));
	EXPECT_EQ(nsm_fpga_xfer_argument(0x50, nullptr, 0), 0x50u);
}

TEST(fpgaNegative, nsm_fpga_commands_valid)
{
	const nsm_fpga_map *map = nsm_fpga_map_p0();
	EXPECT_FALSE(nsm_fpga_commands_valid(nullptr, readDdr5, 1));
	EXPECT_FALSE(nsm_fpga_commands_valid(map, nullptr, 1));
	nsm_fpga_command bad = readDdr5[0];
	bad.station = 38;
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &bad, 1));
	bad = readDdr5[0];
	bad.poll = NSM_FPGA_POLL_ANY + 1;
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &bad, 1));
	bad = readDdr5[0];
	bad.capture = NSM_FPGA_BUF_WORD;
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &bad, 1));
	bad = readDdr5[0];
	bad.on_expect_fail = NSM_FPGA_END_GROUP + 1;
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &bad, 1));
	bad = readDdr5[0];
	bad.run_if = {NSM_FPGA_BUF_ITEM, NSM_FPGA_CMP_EQ, 5, 0, 0xff, 1};
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &bad, 1));
	bad = readDdr5[0];
	bad.op = NSM_FPGA_OP_CHECK;
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &bad, 1));
	bad = readDdr5[0];
	bad.station = 0;
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &bad, 1));
	bad = readDdr5[0];
	bad.u.csr.address = 0x05000810ULL;
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &bad, 1));
	bad = readDdr5[0];
	bad.u.csr.address = 0x05000402ULL;
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &bad, 1));
	nsm_fpga_command byte{};
	byte.op = NSM_FPGA_OP_BYTE;
	byte.station = 29;
	byte.u.i2c = {0x50, 0, NSM_FPGA_I2C_START};
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &byte, 1));
	byte.station = 0;
	byte.u.i2c = {0x07, 0, NSM_FPGA_I2C_START};
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &byte, 1));
	nsm_fpga_command pop{};
	pop.op = NSM_FPGA_OP_POP;
	pop.station = 29;
	pop.u.pop = {0x05000400ULL, 0x05000404ULL};
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &pop, 1));
	nsm_fpga_command xfer{};
	xfer.op = NSM_FPGA_OP_I2C_XFER;
	xfer.station = 1;
	xfer.u.xfer = {0x07, {0, 0, 0}, 1, 1, 0, 0};
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &xfer, 1));
	xfer.u.xfer = {0x50, {0, 0, 0}, 0, 0, 0, 0};
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &xfer, 1));
	xfer.u.xfer = {0x50, {0, 0, 0}, 1, 1, 0x2, 0};
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &xfer, 1));
	xfer.station = 29;
	xfer.u.xfer = {0x50, {0, 0, 0}, 1, 1, 0, 0};
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &xfer, 1));
	bad = readDdr5[0];
	bad.op = 0x7f;
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &bad, 1));
	nsm_fpga_command sdm{};
	sdm.op = NSM_FPGA_OP_WRITE32;
	sdm.station = 37;
	sdm.u.csr = {0, 0x00001017};
	EXPECT_FALSE(nsm_fpga_commands_valid(map, &sdm, 1));
}

TEST(fpgaNegative, nsm_fpga_rule_valid)
{
	const nsm_fpga_map *map = nsm_fpga_map_p0();
	nsm_fpga_rule rule{};
	rule.name = "Item";
	rule.commands = readDdr5;
	rule.command_count = 1;
	rule.kind = NSM_FPGA_RULE_NUMERIC;
	rule.numeric.buffer = NSM_FPGA_BUF_ITEM;
	rule.numeric.format = NSM_FPGA_FORMAT_FIXED;
	rule.numeric.bit_width = 16;
	rule.numeric.scale = 1.0;
	rule.numeric.min_value = NAN;
	rule.numeric.max_value = NAN;
	ASSERT_TRUE(nsm_fpga_rule_valid(map, &rule));
	EXPECT_FALSE(nsm_fpga_rule_valid(nullptr, &rule));
	EXPECT_FALSE(nsm_fpga_rule_valid(map, nullptr));
	nsm_fpga_rule bad = rule;
	bad.identity_count = 1;
	EXPECT_FALSE(nsm_fpga_rule_valid(map, &bad));
	bad = rule;
	bad.numeric.vector = NSM_FPGA_VECTOR_TX_POWER;
	EXPECT_FALSE(nsm_fpga_rule_valid(map, &bad));
	bad = rule;
	bad.kind = NSM_FPGA_RULE_STATE;
	EXPECT_FALSE(nsm_fpga_rule_valid(map, &bad));
	bad = rule;
	bad.kind = NSM_FPGA_RULE_ARRAY;
	EXPECT_FALSE(nsm_fpga_rule_valid(map, &bad));
	bad = rule;
	bad.run_if = {NSM_FPGA_BUF_ITEM, NSM_FPGA_CMP_EQ, 1, 0, 0xff, 1};
	EXPECT_FALSE(nsm_fpga_rule_valid(map, &bad));
	bad = rule;
	bad.valid_if = {NSM_FPGA_BUF_ITEM, NSM_FPGA_CMP_EQ, 5, 0, 0xff, 1};
	EXPECT_FALSE(nsm_fpga_rule_valid(map, &bad));
	bad = rule;
	bad.commands = nullptr;
	EXPECT_FALSE(nsm_fpga_rule_valid(map, &bad));
	bad = rule;
	bad.command_count = 0;
	EXPECT_FALSE(nsm_fpga_rule_valid(map, &bad));
	bad = rule;
	bad.cleanup = readDdr5;
	bad.cleanup_count = 1;
	nsm_fpga_command badCleanup = readDdr5[0];
	badCleanup.station = 38;
	bad.cleanup = &badCleanup;
	EXPECT_FALSE(nsm_fpga_rule_valid(map, &bad));
}

TEST(fpgaNegative, nsm_fpga_rule_word)
{
	uint32_t word = 0xabcd;
	const uint8_t bytes[] = {1, 2, 3, 4};
	nsm_fpga_rule rule{};
	rule.commands = readDdr5;
	rule.command_count = 1;
	EXPECT_FALSE(nsm_fpga_rule_word(nullptr, bytes, 4, nullptr, 0, &word));
	EXPECT_FALSE(nsm_fpga_rule_word(&rule, bytes, 4, nullptr, 0, nullptr));
	EXPECT_FALSE(nsm_fpga_rule_word(&rule, nullptr, 4, bytes, 4, &word));
	EXPECT_FALSE(nsm_fpga_rule_word(&rule, bytes, 0, bytes, 4, &word));
	nsm_fpga_rule rowless{};
	rowless.numeric.offset = 4;
	EXPECT_FALSE(nsm_fpga_rule_word(&rowless, bytes, 4, bytes, 4, &word));
	EXPECT_FALSE(nsm_fpga_rule_word(&rowless, bytes, 4, nullptr, 0, &word));
	EXPECT_EQ(word, 0xabcdu);
}

TEST(fpgaNegative, nsm_fpga_compare_holds)
{
	const uint8_t bytes[] = {1, 2, 3};
	EXPECT_FALSE(nsm_fpga_compare_holds(nullptr, bytes, 3, nullptr, 0, 0));
	nsm_fpga_compare compare{NSM_FPGA_BUF_ITEM, NSM_FPGA_CMP_EQ, 4, 0,
				 0xffffffff,	    0x00030201};
	EXPECT_FALSE(nsm_fpga_compare_holds(&compare, bytes, 3, nullptr, 0, 0));
	compare = {NSM_FPGA_BUF_GROUP, NSM_FPGA_CMP_EQ, 1, 0, 0xff, 1};
	EXPECT_FALSE(nsm_fpga_compare_holds(&compare, bytes, 3, nullptr, 0, 0));
	compare = {7, NSM_FPGA_CMP_EQ, 1, 0, 0xff, 1};
	EXPECT_FALSE(nsm_fpga_compare_holds(&compare, bytes, 3, bytes, 3, 1));
	compare = {NSM_FPGA_BUF_NONE, NSM_FPGA_CMP_LE + 1, 1, 0, 0xff, 1};
	EXPECT_FALSE(nsm_fpga_compare_holds(&compare, bytes, 3, bytes, 3, 1));
	compare = {NSM_FPGA_BUF_ITEM, NSM_FPGA_CMP_EQ, 0, 0, 0xff, 1};
	EXPECT_FALSE(nsm_fpga_compare_holds(&compare, bytes, 3, bytes, 3, 1));
	compare = {NSM_FPGA_BUF_ITEM, NSM_FPGA_CMP_EQ, 1, 3, 0xff, 0};
	EXPECT_FALSE(nsm_fpga_compare_holds(&compare, bytes, 3, bytes, 3, 0));
}

TEST(fpgaNegative, nsm_fpga_poll_satisfied)
{
	EXPECT_FALSE(nsm_fpga_poll_satisfied(NSM_FPGA_POLL_ANY + 1, 0, 0));
	EXPECT_FALSE(nsm_fpga_poll_satisfied(NSM_FPGA_POLL_SET, 0x3, 0x1));
	EXPECT_FALSE(nsm_fpga_poll_satisfied(NSM_FPGA_POLL_ANY, 0x3, 0x4));
}

TEST(fpgaNegative, nsm_fpga_decode_numeric)
{
	double value = 42.0;
	const uint8_t bytes[] = {0x10, 0x20, 0x30, 0x40};
	nsm_fpga_numeric_spec spec{};
	spec.format = NSM_FPGA_FORMAT_FIXED;
	spec.buffer = NSM_FPGA_BUF_ITEM;
	spec.bit_width = 16;
	spec.scale = 1.0;
	spec.min_value = NAN;
	spec.max_value = NAN;
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(nullptr, bytes, 4, nullptr, 0, &value));
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&spec, bytes, 4, nullptr, 0, nullptr));
	nsm_fpga_numeric_spec bad = spec;
	bad.bit_width = 0;
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&bad, bytes, 4, nullptr, 0, &value));
	bad.bit_width = 33;
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&bad, bytes, 4, nullptr, 0, &value));
	bad = spec;
	bad.bit_shift = 20;
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&bad, bytes, 4, nullptr, 0, &value));
	bad = spec;
	bad.format = 7;
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&bad, bytes, 4, nullptr, 0, &value));
	bad = spec;
	bad.buffer = NSM_FPGA_BUF_NONE;
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&bad, bytes, 4, nullptr, 0, &value));
	bad = spec;
	bad.offset = 3;
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&bad, bytes, 4, nullptr, 0, &value));
	bad = spec;
	bad.max_value = 1.0;
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&bad, bytes, 4, nullptr, 0, &value));
	bad = spec;
	bad.min_value = 1e9;
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&bad, bytes, 4, nullptr, 0, &value));
	bad = spec;
	bad.scale = INFINITY;
	bad.bias = -INFINITY;
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&bad, bytes, 4, nullptr, 0, &value));
	EXPECT_FALSE(
	    nsm_fpga_decode_numeric(&spec, nullptr, 0, nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, 42.0);
}

TEST(fpgaNegative, nsm_fpga_rule_identity)
{
	size_t count = 7;
	nsm_fpga_rule rule{};
	EXPECT_EQ(nsm_fpga_rule_identity(nullptr, &count), nullptr);
	EXPECT_EQ(nsm_fpga_rule_identity(&rule, nullptr), nullptr);
	EXPECT_EQ(count, 7u);
}

TEST(fpgaNegative, nsm_fpga_decode_identity)
{
	char out[NSM_FPGA_IDENTITY_TEXT_MAX];
	std::memset(out, 'x', sizeof(out));
	const uint8_t bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	nsm_fpga_identity_field field{NSM_FPGA_ASSET_SERIAL,
				      NSM_FPGA_IDENTITY_HEX_LE,
				      NSM_FPGA_BUF_ITEM, 0, 8};
	EXPECT_FALSE(nsm_fpga_decode_identity(nullptr, bytes, 8, nullptr, 0,
					      out, sizeof(out)));
	EXPECT_FALSE(nsm_fpga_decode_identity(&field, bytes, 8, nullptr, 0,
					      nullptr, sizeof(out)));
	EXPECT_FALSE(
	    nsm_fpga_decode_identity(&field, bytes, 8, nullptr, 0, out, 0));
	nsm_fpga_identity_field bad = field;
	bad.length = 0;
	EXPECT_FALSE(nsm_fpga_decode_identity(&bad, bytes, 8, nullptr, 0, out,
					      sizeof(out)));
	bad = field;
	bad.length = 9;
	EXPECT_FALSE(nsm_fpga_decode_identity(&bad, bytes, 8, nullptr, 0, out,
					      sizeof(out)));
	bad = field;
	bad.asset = NSM_FPGA_ASSET_REVISION + 1;
	EXPECT_FALSE(nsm_fpga_decode_identity(&bad, bytes, 8, nullptr, 0, out,
					      sizeof(out)));
	bad = field;
	bad.buffer = NSM_FPGA_BUF_NONE;
	EXPECT_FALSE(nsm_fpga_decode_identity(&bad, bytes, 8, nullptr, 0, out,
					      sizeof(out)));
	EXPECT_FALSE(nsm_fpga_decode_identity(&field, bytes, 7, nullptr, 0, out,
					      sizeof(out)));
	EXPECT_FALSE(nsm_fpga_decode_identity(&field, nullptr, 0, bytes, 8, out,
					      sizeof(out)));
	EXPECT_FALSE(
	    nsm_fpga_decode_identity(&field, bytes, 8, nullptr, 0, out, 18));
	bad = field;
	bad.format = NSM_FPGA_IDENTITY_ASCII;
	EXPECT_FALSE(
	    nsm_fpga_decode_identity(&bad, bytes, 8, nullptr, 0, out, 8));
	EXPECT_EQ(out[0], 'x');
}

TEST(fpgaNegative, nsm_fpga_decode_element)
{
	double value = 42.0;
	const uint8_t bytes[] = {0x10, 0x20, 0x30, 0x40};
	nsm_fpga_numeric_spec spec{};
	spec.format = NSM_FPGA_FORMAT_U16BE;
	spec.buffer = NSM_FPGA_BUF_ITEM;
	spec.scale = 1.0;
	spec.min_value = NAN;
	spec.max_value = NAN;
	spec.element_count = 2;
	spec.stride_bits = 16;
	EXPECT_FALSE(
	    nsm_fpga_decode_element(nullptr, 0, bytes, 4, nullptr, 0, &value));
	EXPECT_FALSE(
	    nsm_fpga_decode_element(&spec, 0, bytes, 4, nullptr, 0, nullptr));
	EXPECT_FALSE(
	    nsm_fpga_decode_element(&spec, 2, bytes, 4, nullptr, 0, &value));
	nsm_fpga_numeric_spec bad = spec;
	bad.stride_bits = 3;
	EXPECT_FALSE(
	    nsm_fpga_decode_element(&bad, 0, bytes, 4, nullptr, 0, &value));
	bad = spec;
	bad.element_count = SIZE_MAX;
	bad.stride_bits = 8;
	EXPECT_FALSE(nsm_fpga_decode_element(&bad, SIZE_MAX - 1, bytes, 4,
					     nullptr, 0, &value));
	bad = spec;
	bad.offset = SIZE_MAX;
	EXPECT_FALSE(
	    nsm_fpga_decode_element(&bad, 1, bytes, 4, nullptr, 0, &value));
	EXPECT_FALSE(
	    nsm_fpga_decode_element(&spec, 1, bytes, 3, nullptr, 0, &value));
	EXPECT_DOUBLE_EQ(value, 42.0);
}

TEST(fpgaNegative, nsm_fpga_sdm_word_legal)
{
	EXPECT_FALSE(nsm_fpga_sdm_word_legal(NSM_FPGA_OP_BYTE, 8, 0, 0xf));
	EXPECT_FALSE(nsm_fpga_sdm_word_legal(NSM_FPGA_OP_READ32, 4, 0, 0xf));
	EXPECT_FALSE(nsm_fpga_sdm_word_legal(NSM_FPGA_OP_READ32, 12, 0, 0xf));
	EXPECT_FALSE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 0, 0x00001019, 0xe));
	EXPECT_FALSE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 0, 0x00001017, 0xf));
	EXPECT_FALSE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 0, 0x00002019, 0xf));
	EXPECT_FALSE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 0, 0x00001819, 0xf));
	EXPECT_FALSE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 0, 0x10001019, 0xf));
	EXPECT_FALSE(nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 4, 2, 0xf));
	EXPECT_FALSE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 4, 0x00001010, 0xf));
	EXPECT_FALSE(
	    nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 4, 0x00000011, 0xf));
	EXPECT_FALSE(nsm_fpga_sdm_word_legal(NSM_FPGA_OP_WRITE32, 8, 1, 0xf));
}

TEST(fpgaNegative, nsm_fpga_word_legal)
{
	const nsm_fpga_map *map = nsm_fpga_map_p0();
	EXPECT_FALSE(nsm_fpga_word_legal(nullptr, 29, NSM_FPGA_OP_READ32,
					 0x05000400ULL, 0, 0xf));
	EXPECT_FALSE(nsm_fpga_word_legal(map, 38, NSM_FPGA_OP_READ32,
					 0x05000400ULL, 0, 0xf));
	EXPECT_FALSE(nsm_fpga_word_legal(map, 29, NSM_FPGA_OP_BYTE,
					 0x05000400ULL, 0, 0xf));
	EXPECT_FALSE(
	    nsm_fpga_word_legal(map, 0, NSM_FPGA_OP_READ32, 0, 0, 0xf));
	EXPECT_FALSE(nsm_fpga_word_legal(map, 29, NSM_FPGA_OP_READ32,
					 0x05000400ULL, 0, 0x7));
	EXPECT_FALSE(nsm_fpga_word_legal(map, 29, NSM_FPGA_OP_READ32,
					 0x05000810ULL, 0, 0xf));
	EXPECT_FALSE(nsm_fpga_word_legal(map, 29, NSM_FPGA_OP_READ32,
					 0x0500080dULL, 0, 0xf));
	EXPECT_FALSE(nsm_fpga_word_legal(map, 29, NSM_FPGA_OP_READ32,
					 0x05000402ULL, 0, 0xf));
	EXPECT_FALSE(
	    nsm_fpga_word_legal(map, 37, NSM_FPGA_OP_READ32, 12, 0, 0xf));
	EXPECT_FALSE(
	    nsm_fpga_word_legal(map, 37, NSM_FPGA_OP_ACK_CLEAR, 8, 0, 0xf));
}

TEST(fpgaNegative, nsm_fpga_decode_state)
{
	uint8_t state = 0xee;
	uint8_t health = 0xee;
	const nsm_fpga_state_entry entries[] = {
	    {0xff, 0x01, NSM_FPGA_STATE_ENABLED, NSM_FPGA_HEALTH_OK}};
	nsm_fpga_rule rule{};
	rule.kind = NSM_FPGA_RULE_STATE;
	rule.states = entries;
	rule.state_count = 1;
	EXPECT_FALSE(nsm_fpga_decode_state(nullptr, 1, &state, &health));
	EXPECT_FALSE(nsm_fpga_decode_state(&rule, 1, nullptr, &health));
	EXPECT_FALSE(nsm_fpga_decode_state(&rule, 1, &state, nullptr));
	EXPECT_FALSE(nsm_fpga_decode_state(&rule, 2, &state, &health));
	nsm_fpga_rule empty{};
	EXPECT_FALSE(nsm_fpga_decode_state(&empty, 1, &state, &health));
	EXPECT_EQ(state, 0xee);
	EXPECT_EQ(health, 0xee);
}

TEST(fpgaNegative, nsm_fpga_run_init)
{
	Driver d;
	EXPECT_EQ(d.run.limits.poll_period_us, 5000u);
	EXPECT_EQ(d.run.limits.poll_limit, 200u);
	EXPECT_EQ(d.run.limits.pending_repeats, 16u);
	const nsm_fpga_step *step = nullptr;
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(step, nullptr);
	EXPECT_EQ(d.run.state, NSM_FPGA_RUN_IDLE);
}

TEST(fpgaNegative, nsm_fpga_run_begin_round)
{
	Driver d;
	d.run.group[0] = 0x11;
	d.run.group_len = 1;
	d.run.item_len = 2;
	d.run.end_group = true;
	nsm_fpga_run_begin_round(&d.run);
	EXPECT_EQ(d.run.group_len, 0u);
	EXPECT_EQ(d.run.item_len, 0u);
	EXPECT_FALSE(d.run.end_group);
}

TEST(fpgaNegative, nsm_fpga_run_begin_item)
{
	Driver d;
	d.run.group[0] = 0x11;
	d.run.group_len = 1;
	d.run.item_len = 2;
	d.run.last_word = 5;
	d.run.clean_blocked = true;
	nsm_fpga_run_begin_item(&d.run);
	EXPECT_EQ(d.run.group_len, 1u);
	EXPECT_EQ(d.run.item_len, 0u);
	EXPECT_EQ(d.run.last_word, 0u);
	EXPECT_FALSE(d.run.clean_blocked);
}

TEST(fpgaNegative, nsm_fpga_run_rows)
{
	Driver d;
	nsm_fpga_run_rows(&d.run, nullptr, 5);
	EXPECT_EQ(d.run.row_count, 0u);
	const nsm_fpga_step *step = nullptr;
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(d.run.state, NSM_FPGA_RUN_FINISHED);
	EXPECT_EQ(d.session.data_sequence, 0u);
	const nsm_fpga_command badOp[] = {{.op = 0x7f, .station = 29}};
	nsm_fpga_run_rows(&d.run, badOp, 1);
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(d.run.fence);
	EXPECT_STREQ(d.run.error, "unsupported command op");
}

TEST(fpgaNegative, nsm_fpga_run_control)
{
	Driver d;
	nsm_fpga_run_control(&d.run, NSM_FPGA_CTRL_ACTIVATE, 0);
	const nsm_fpga_step *step = nullptr;
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(d.run.fence);
	EXPECT_STREQ(d.run.error, "control plan rejected");
	EXPECT_EQ(d.session.control_sequence, 0u);
	Driver busy;
	nsm_fpga_run_control(&busy.run, NSM_FPGA_CTRL_ACQUIRE, 0);
	const nsm_fpga_identity sent{validGrant.epoch, validGrant.session,
				     validGrant.generation, 1};
	const Record refused = controlReply(sent, NSM_FPGA_CTRL_ACQUIRE, 0,
					    NSM_FPGA_CTRL_BUSY, false);
	EXPECT_EQ(busy.answer(refused.data()), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(busy.run.fence);
	EXPECT_STREQ(busy.run.error, "session control rejected BUSY");
	EXPECT_EQ(busy.session.control_sequence, 0u);
}

TEST(fpgaNegative, nsm_fpga_run_status)
{
	Driver d;
	nsm_fpga_run_status(&d.run);
	const Record junk = filled(0xff);
	EXPECT_EQ(d.answer(junk.data()), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(d.run.fence);
	EXPECT_STREQ(d.run.error, "authority status record malformed");
	EXPECT_EQ(d.run.authority.epoch, 0u);
}

TEST(fpgaNegative, nsm_fpga_run_recover)
{
	Driver d;
	nsm_fpga_run_recover(&d.run, 0);
	const nsm_fpga_step *step = nullptr;
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(d.run.fence);
	EXPECT_STREQ(d.run.error, "authority plan rejected");
	Driver denied;
	nsm_fpga_run_recover(&denied.run, 0x20);
	const nsm_fpga_identity epochOnly{0x20, 0, 0, 0};
	const Record refused =
	    controlReply(epochOnly, NSM_FPGA_CTRL_RECOVER, 0,
			 NSM_FPGA_CTRL_DENIED, false, NSM_FPGA_OWNER_NONE);
	EXPECT_EQ(denied.answer(refused.data()), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(denied.run.fence);
	EXPECT_STREQ(denied.run.error, "recover refused DENIED");
}

TEST(fpgaNegative, nsm_fpga_run_activate)
{
	Driver d;
	nsm_fpga_run_activate(&d.run, 0);
	const nsm_fpga_step *step = nullptr;
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(d.run.fence);
	EXPECT_STREQ(d.run.error, "authority plan rejected");
	Driver stale;
	nsm_fpga_run_activate(&stale.run, 0x20);
	const nsm_fpga_identity otherEpoch{0x21, 0, 0, 0};
	const Record foreign = controlReply(otherEpoch, NSM_FPGA_CTRL_ACTIVATE,
					    0, 0, false, NSM_FPGA_OWNER_NONE);
	EXPECT_EQ(stale.answer(foreign.data()), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(stale.run.fence);
	EXPECT_STREQ(stale.run.error, "activate receipt foreign");
}

TEST(fpgaNegative, nsm_fpga_run_next)
{
	Driver d;
	const nsm_fpga_step *step = nullptr;
	nsm_fpga_run_rows(&d.run, readDdr5, 1);
	ASSERT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_SEND);
	nsm_fpga_run_abort(&d.run, "transport closed");
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_FAILED);
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_FAILED);
	nsm_fpga_run_rows(&d.run, readDdr5, 0);
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_DONE);
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_DONE);
}

TEST(fpgaNegative, nsm_fpga_run_reply)
{
	Driver idle;
	const Record junk = filled(0xff);
	nsm_fpga_run_reply(&idle.run, junk.data(), false, false);
	EXPECT_EQ(idle.run.state, NSM_FPGA_RUN_IDLE);
	EXPECT_EQ(idle.run.error[0], '\0');
	Driver missing;
	nsm_fpga_run_rows(&missing.run, readDdr5, 1);
	EXPECT_EQ(missing.answer(nullptr), NSM_FPGA_RUN_FAILED);
	EXPECT_FALSE(missing.run.fence);
	EXPECT_STREQ(missing.run.error, "completion record missing");
	EXPECT_EQ(missing.run.item_len, 0u);
	EXPECT_EQ(missing.session.data_sequence, 0u);
	Driver foreign;
	nsm_fpga_run_rows(&foreign.run, readDdr5, 1);
	const nsm_fpga_identity sent{validGrant.epoch, validGrant.session,
				     validGrant.generation, 1};
	const Record native =
	    wordReply(sent, NSM_FPGA_RESULT_SUCCESS,
		      NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
			  NSM_FPGA_FLAG_READ_VALID,
		      7, NSM_FPGA_OP_READ32, NSM_FPGA_OWNER_NATIVE);
	EXPECT_EQ(foreign.answer(native.data()), NSM_FPGA_RUN_FAILED);
	EXPECT_TRUE(foreign.run.fence);
	EXPECT_STREQ(foreign.run.error, "word completion foreign");
	EXPECT_EQ(foreign.run.item_len, 0u);
}

TEST(fpgaNegative, nsm_fpga_run_abort)
{
	Driver d;
	nsm_fpga_run_abort(&d.run, "nothing running");
	EXPECT_EQ(d.run.state, NSM_FPGA_RUN_IDLE);
	EXPECT_EQ(d.run.error[0], '\0');
	nsm_fpga_run_rows(&d.run, readDdr5, 0);
	const nsm_fpga_step *step = nullptr;
	EXPECT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_DONE);
	nsm_fpga_run_abort(&d.run, "too late");
	EXPECT_EQ(d.run.state, NSM_FPGA_RUN_FINISHED);
	EXPECT_EQ(d.run.error[0], '\0');
	nsm_fpga_run_rows(&d.run, readDdr5, 1);
	ASSERT_EQ(nsm_fpga_run_next(&d.run, &step), NSM_FPGA_RUN_SEND);
	nsm_fpga_run_abort(&d.run, "transport closed");
	EXPECT_EQ(d.run.state, NSM_FPGA_RUN_STOPPED);
	EXPECT_FALSE(d.run.fence);
	EXPECT_STREQ(d.run.error, "transport closed");
}

TEST(fpgaNegative, nsm_fpga_run_default_capture)
{
	Driver d;
	d.run.item[0] = 0x99;
	d.run.item_len = 1;
	d.run.last_word = 0x04030201;
	nsm_fpga_run_default_capture(&d.run);
	EXPECT_EQ(d.run.item_len, 1u);
	EXPECT_EQ(d.run.item[0], 0x99);
	EXPECT_EQ(d.run.item[1], 0);
}
