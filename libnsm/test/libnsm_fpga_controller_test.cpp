// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "base.h"
#include "fpga/fpga-controller.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{
using Record = std::array<uint8_t, NSM_FPGA_RECORD_SIZE>;

const nsm_fpga_identity grantIdentity{0x1122334455667788ULL, 0x0a0b0c0d,
				      0x00000002, 1};

void putLe(Record &record, size_t offset, uint64_t value, size_t bytes)
{
	for (size_t i = 0; i < bytes; ++i) {
		record[offset + i] = static_cast<uint8_t>(value >> (8 * i));
	}
}

// Control completion as the FPGA writes it: echo of the request plus status
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
	putLe(r, 8, id.epoch, 8);
	putLe(r, 16, id.session, 4);
	putLe(r, 20, id.generation, 4);
	putLe(r, 24, id.sequence, 4);
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
	putLe(r, 8, id.epoch, 8);
	putLe(r, 16, id.session, 4);
	putLe(r, 20, id.generation, 4);
	putLe(r, 24, id.sequence, 4);
	putLe(r, 28, data, 4);
	return r;
}

Record emptyReply()
{
	Record r{};
	putLe(r, 0, NSM_FPGA_PROFILE_IBC2, 4);
	return r;
}
} // namespace

TEST(fpgaController, controlRecordRules)
{
	Record record{};
	EXPECT_EQ(nsm_fpga_encode_control(&grantIdentity, NSM_FPGA_CTRL_ACQUIRE,
					  0, record.data()),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(record[4], NSM_FPGA_CTRL_ACQUIRE);
	EXPECT_EQ(record[5], 0);
	EXPECT_EQ(record[24], 1);
	// ACQUIRE is always sequence 1 with a zero argument
	nsm_fpga_identity later = grantIdentity;
	later.sequence = 2;
	EXPECT_EQ(nsm_fpga_encode_control(&later, NSM_FPGA_CTRL_ACQUIRE, 0,
					  record.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_control(&grantIdentity, NSM_FPGA_CTRL_RELEASE,
					  1, record.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_control(&later, NSM_FPGA_CTRL_RELEASE, 1,
					  record.data()),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(record[28], 1);
	EXPECT_EQ(nsm_fpga_encode_control(&later, NSM_FPGA_CTRL_RENEW, 0,
					  record.data()),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_encode_control(&later, 0x10, 0, record.data()),
		  NSM_SW_ERROR_DATA);
}

TEST(fpgaController, authorityRequestsCarryTheEpochOnly)
{
	Record record{};
	const nsm_fpga_identity epochOnly{0x1122334455667788ULL, 0, 0, 0};
	EXPECT_EQ(nsm_fpga_encode_control(&epochOnly, NSM_FPGA_CTRL_RECOVER, 0,
					  record.data()),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(record[4], NSM_FPGA_CTRL_RECOVER);
	EXPECT_EQ(record[8], 0x88);
	EXPECT_EQ(record[15], 0x11);
	for (size_t i = 16; i < 32; ++i) {
		EXPECT_EQ(record[i], 0) << i;
	}
	EXPECT_EQ(nsm_fpga_encode_control(&epochOnly, NSM_FPGA_CTRL_ACTIVATE, 0,
					  record.data()),
		  NSM_SW_SUCCESS);
	// The bank answers an authority request with no owner; the receipt
	// decodes and classifies as done
	Record receipt{};
	std::memcpy(receipt.data(), record.data(), 4);
	receipt[4] = NSM_FPGA_CTRL_RECOVER;
	receipt[5] = NSM_FPGA_CTRL_OK;
	receipt[6] = NSM_FPGA_OWNER_NONE;
	receipt[7] = 1;
	std::memcpy(receipt.data() + 8, record.data() + 8, 24);
	nsm_fpga_control_completion completion{};
	EXPECT_EQ(
	    nsm_fpga_decode_control_completion(receipt.data(), &completion),
	    NSM_SW_SUCCESS);
	uint8_t result = 0xff;
	EXPECT_EQ(nsm_fpga_classify_authority(receipt.data(),
					      0x1122334455667788ULL,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_DONE);
	// An ACQUIRE receipt still needs an owner
	receipt[4] = NSM_FPGA_CTRL_ACQUIRE;
	EXPECT_EQ(
	    nsm_fpga_decode_control_completion(receipt.data(), &completion),
	    NSM_SW_ERROR_DATA);
	// A session identity or an argument is not an authority request
	EXPECT_EQ(nsm_fpga_encode_control(&grantIdentity, NSM_FPGA_CTRL_RECOVER,
					  0, record.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_control(&epochOnly, NSM_FPGA_CTRL_RECOVER, 1,
					  record.data()),
		  NSM_SW_ERROR_DATA);
	nsm_fpga_identity noEpoch = epochOnly;
	noEpoch.epoch = 0;
	EXPECT_EQ(nsm_fpga_encode_control(&noEpoch, NSM_FPGA_CTRL_RECOVER, 0,
					  record.data()),
		  NSM_SW_ERROR_DATA);

	// The receipt names the opcode and the epoch; the bank has no owner
	result = 0;
	auto reply = controlReply(epochOnly, NSM_FPGA_CTRL_RECOVER, 0,
				  NSM_FPGA_CTRL_OK, false, NSM_FPGA_OWNER_NONE);
	EXPECT_EQ(nsm_fpga_classify_authority(reply.data(), epochOnly.epoch,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_DONE);
	reply = controlReply(epochOnly, NSM_FPGA_CTRL_RECOVER, 0,
			     NSM_FPGA_CTRL_OK, true, NSM_FPGA_OWNER_NONE);
	EXPECT_EQ(nsm_fpga_classify_authority(reply.data(), epochOnly.epoch,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_PENDING);
	reply = controlReply(epochOnly, NSM_FPGA_CTRL_RECOVER, 0,
			     NSM_FPGA_CTRL_DENIED, false, NSM_FPGA_OWNER_NONE);
	EXPECT_EQ(nsm_fpga_classify_authority(reply.data(), epochOnly.epoch,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_REJECTED);
	EXPECT_EQ(result, NSM_FPGA_CTRL_DENIED);
	EXPECT_EQ(nsm_fpga_classify_authority(reply.data(), epochOnly.epoch + 1,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_FOREIGN);
	reply = emptyReply();
	EXPECT_EQ(nsm_fpga_classify_authority(reply.data(), epochOnly.epoch,
					      NSM_FPGA_CTRL_RECOVER, &result),
		  NSM_FPGA_REPLY_PENDING);

	// Only nsmd's own lapsed lease is recoverable
	nsm_fpga_authority_status status{};
	status.epoch = 5;
	status.flags = NSM_FPGA_AUTH_INITIALIZED | NSM_FPGA_AUTH_QUARANTINED |
		       NSM_FPGA_AUTH_RING_ARMED | NSM_FPGA_AUTH_QUIESCENT |
		       NSM_FPGA_AUTH_RELEASED;
	status.owner = NSM_FPGA_OWNER_NSM;
	EXPECT_TRUE(nsm_fpga_authority_recoverable(&status));
	status.owner = NSM_FPGA_OWNER_NATIVE;
	EXPECT_FALSE(nsm_fpga_authority_recoverable(&status));
	status.owner = NSM_FPGA_OWNER_NSM;
	status.flags &= static_cast<uint8_t>(~NSM_FPGA_AUTH_RELEASED);
	EXPECT_FALSE(nsm_fpga_authority_recoverable(&status));
	status.flags = NSM_FPGA_AUTH_INITIALIZED | NSM_FPGA_AUTH_AVAILABLE;
	EXPECT_FALSE(nsm_fpga_authority_recoverable(&status));
	EXPECT_FALSE(nsm_fpga_authority_recoverable(nullptr));

	// Only a bank nobody activated since its controller came up
	nsm_fpga_authority_status fresh{};
	fresh.flags = NSM_FPGA_AUTH_RING_ARMED | NSM_FPGA_AUTH_QUIESCENT |
		      NSM_FPGA_AUTH_RELEASED;
	EXPECT_TRUE(nsm_fpga_authority_activatable(&fresh));
	fresh.epoch = 1;
	EXPECT_FALSE(nsm_fpga_authority_activatable(&fresh));
	fresh.epoch = 0;
	fresh.owner = NSM_FPGA_OWNER_NATIVE;
	EXPECT_FALSE(nsm_fpga_authority_activatable(&fresh));
	fresh.owner = NSM_FPGA_OWNER_NONE;
	fresh.flags |= NSM_FPGA_AUTH_INITIALIZED;
	EXPECT_FALSE(nsm_fpga_authority_activatable(&fresh));
	fresh.flags = NSM_FPGA_AUTH_QUARANTINED;
	EXPECT_FALSE(nsm_fpga_authority_activatable(&fresh));
	fresh.flags = NSM_FPGA_AUTH_GRANT_VALID;
	EXPECT_FALSE(nsm_fpga_authority_activatable(&fresh));
	EXPECT_FALSE(nsm_fpga_authority_activatable(nullptr));
}

TEST(fpgaController, controlCompletionMatching)
{
	nsm_fpga_control_completion completion{};
	auto reply = controlReply(grantIdentity, NSM_FPGA_CTRL_ACQUIRE, 0,
				  NSM_FPGA_CTRL_OK, false);
	ASSERT_EQ(nsm_fpga_decode_control_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_control(&completion, &grantIdentity,
					 NSM_FPGA_CTRL_ACQUIRE, 0),
		  NSM_FPGA_MATCH_SUCCESS);

	reply = controlReply(grantIdentity, NSM_FPGA_CTRL_ACQUIRE, 0,
			     NSM_FPGA_CTRL_OK, true);
	ASSERT_EQ(nsm_fpga_decode_control_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_control(&completion, &grantIdentity,
					 NSM_FPGA_CTRL_ACQUIRE, 0),
		  NSM_FPGA_MATCH_PENDING);

	reply = controlReply(grantIdentity, NSM_FPGA_CTRL_ACQUIRE, 0,
			     NSM_FPGA_CTRL_BUSY, false);
	ASSERT_EQ(nsm_fpga_decode_control_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_control(&completion, &grantIdentity,
					 NSM_FPGA_CTRL_ACQUIRE, 0),
		  NSM_FPGA_MATCH_REJECTED);

	reply = controlReply(grantIdentity, NSM_FPGA_CTRL_ACQUIRE, 0,
			     NSM_FPGA_CTRL_OK, false, NSM_FPGA_OWNER_NATIVE);
	ASSERT_EQ(nsm_fpga_decode_control_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_control(&completion, &grantIdentity,
					 NSM_FPGA_CTRL_ACQUIRE, 0),
		  NSM_FPGA_MATCH_FOREIGN);

	reply = emptyReply();
	ASSERT_EQ(nsm_fpga_decode_control_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_control(&completion, &grantIdentity,
					 NSM_FPGA_CTRL_ACQUIRE, 0),
		  NSM_FPGA_MATCH_EMPTY);

	// A pending receipt with a failure result is not a legal record
	reply = controlReply(grantIdentity, NSM_FPGA_CTRL_ACQUIRE, 0,
			     NSM_FPGA_CTRL_BUSY, true);
	EXPECT_EQ(nsm_fpga_decode_control_completion(reply.data(), &completion),
		  NSM_SW_ERROR_DATA);
	reply[0] = 0;
	EXPECT_EQ(nsm_fpga_decode_control_completion(reply.data(), &completion),
		  NSM_SW_ERROR_DATA);
}

TEST(fpgaController, wordCompletionMatching)
{
	nsm_fpga_word_completion completion{};
	auto reply =
	    wordReply(grantIdentity, NSM_FPGA_RESULT_SUCCESS,
		      NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
			  NSM_FPGA_FLAG_READ_VALID,
		      0x00000001);
	ASSERT_EQ(nsm_fpga_decode_word_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_word(&completion, &grantIdentity,
				      NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_SUCCESS);
	EXPECT_EQ(completion.data, 1u);

	// Success without READ_VALID cannot answer a read
	reply = wordReply(grantIdentity, NSM_FPGA_RESULT_SUCCESS,
			  NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT, 0);
	ASSERT_EQ(nsm_fpga_decode_word_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_word(&completion, &grantIdentity,
				      NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_REJECTED);

	reply = wordReply(grantIdentity, NSM_FPGA_RESULT_PENDING,
			  NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_PENDING, 0);
	ASSERT_EQ(nsm_fpga_decode_word_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_word(&completion, &grantIdentity,
				      NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_PENDING);

	// Fault: response byte 0x02, reason 3
	reply =
	    wordReply(grantIdentity, NSM_FPGA_RESULT_FAULT,
		      NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_FAULTED, 0x0302);
	ASSERT_EQ(nsm_fpga_decode_word_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_word(&completion, &grantIdentity,
				      NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_FAULT);
	EXPECT_EQ(completion.fault_reason, 3);

	nsm_fpga_identity other = grantIdentity;
	other.sequence = 7;
	reply = wordReply(other, NSM_FPGA_RESULT_SUCCESS,
			  NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
			      NSM_FPGA_FLAG_READ_VALID,
			  5);
	ASSERT_EQ(nsm_fpga_decode_word_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_word(&completion, &grantIdentity,
				      NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_FOREIGN);

	reply = emptyReply();
	ASSERT_EQ(nsm_fpga_decode_word_completion(reply.data(), &completion),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_match_word(&completion, &grantIdentity,
				      NSM_FPGA_OP_READ32),
		  NSM_FPGA_MATCH_EMPTY);
}

TEST(fpgaController, authorityStatusDrivesTheGrant)
{
	// Available bank, epoch 0x55, last grant session 7 generation 3
	Record r{};
	putLe(r, 0, NSM_FPGA_PROFILE_IBC2, 4);
	r[4] = 1;
	r[5] = NSM_FPGA_AUTH_INITIALIZED | NSM_FPGA_AUTH_AVAILABLE |
	       NSM_FPGA_AUTH_RING_ARMED | NSM_FPGA_AUTH_QUIESCENT |
	       NSM_FPGA_AUTH_RELEASED;
	r[6] = NSM_FPGA_OWNER_NONE;
	putLe(r, 8, 0x55, 8);
	putLe(r, 16, 7, 4);
	putLe(r, 20, 3, 4);
	nsm_fpga_authority_status st{};
	ASSERT_EQ(nsm_fpga_decode_authority_status(r.data(), &st),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(st.epoch, 0x55u);
	EXPECT_EQ(st.generation, 3u);
	EXPECT_TRUE(nsm_fpga_authority_acquirable(&st));
	uint64_t epoch = 0;
	uint32_t generation = 0;
	EXPECT_EQ(nsm_fpga_grant_from_authority(&st, 9, &epoch, &generation),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(epoch, 0x55u);
	EXPECT_EQ(generation, 4u);
	EXPECT_EQ(nsm_fpga_grant_from_authority(&st, 0, &epoch, &generation),
		  NSM_SW_ERROR_DATA);

	// Quarantined or not activated banks are never acquired by nsmd
	r[5] |= NSM_FPGA_AUTH_QUARANTINED;
	ASSERT_EQ(nsm_fpga_decode_authority_status(r.data(), &st),
		  NSM_SW_SUCCESS);
	EXPECT_FALSE(nsm_fpga_authority_acquirable(&st));
	Record idle{};
	putLe(idle, 0, NSM_FPGA_PROFILE_IBC2, 4);
	idle[4] = 1;
	ASSERT_EQ(nsm_fpga_decode_authority_status(idle.data(), &st),
		  NSM_SW_SUCCESS);
	EXPECT_FALSE(nsm_fpga_authority_acquirable(&st));
	// Initialized flag without an epoch is inconsistent
	idle[5] = NSM_FPGA_AUTH_INITIALIZED;
	EXPECT_EQ(nsm_fpga_decode_authority_status(idle.data(), &st),
		  NSM_SW_ERROR_DATA);
	r[4] = 2;
	EXPECT_EQ(nsm_fpga_decode_authority_status(r.data(), &st),
		  NSM_SW_ERROR_DATA);
	EXPECT_STREQ(nsm_fpga_control_result_name(NSM_FPGA_CTRL_STALE),
		     "STALE");
	EXPECT_STREQ(nsm_fpga_control_result_name(NSM_FPGA_CTRL_FENCED),
		     "FENCED");
}

const nsm_fpga_window windowP0{2,      0x1000, 0x1020, 0x1040,
			       0x1060, 0x10a0, 0x10e0};
const nsm_fpga_grant grantP0{0x1122334455667788ULL, 0x0a0b0c0d, 2};

TEST(fpgaController, sessionStateFollowsTheAuthorityWord)
{
	nsm_fpga_authority_status status = {.epoch = 7,
					    .session = 0,
					    .generation = 1,
					    .owner = NSM_FPGA_OWNER_NONE,
					    .flags = NSM_FPGA_AUTH_INITIALIZED |
						     NSM_FPGA_AUTH_AVAILABLE};
	EXPECT_EQ(nsm_fpga_session_classify(nullptr),
		  NSM_FPGA_SESSION_UNREACHABLE);
	EXPECT_EQ(nsm_fpga_session_classify(&status), NSM_FPGA_SESSION_READY);
	status.owner = NSM_FPGA_OWNER_NATIVE;
	EXPECT_EQ(nsm_fpga_session_classify(&status),
		  NSM_FPGA_SESSION_OWNED_BY_HOST);
	status.owner = NSM_FPGA_OWNER_NSM;
	EXPECT_EQ(nsm_fpga_session_classify(&status),
		  NSM_FPGA_SESSION_OWNED_BY_BMC);
	status.owner = NSM_FPGA_OWNER_NONE;
	status.flags |= NSM_FPGA_AUTH_GRANT_VALID;
	EXPECT_EQ(nsm_fpga_session_classify(&status),
		  NSM_FPGA_SESSION_OWNED_BY_BMC);
	status.flags |= NSM_FPGA_AUTH_QUARANTINED;
	EXPECT_EQ(nsm_fpga_session_classify(&status),
		  NSM_FPGA_SESSION_QUARANTINED);
	status.flags = NSM_FPGA_AUTH_AVAILABLE;
	EXPECT_EQ(nsm_fpga_session_classify(&status),
		  NSM_FPGA_SESSION_UNINITIALIZED);
	status.flags = NSM_FPGA_AUTH_INITIALIZED | NSM_FPGA_AUTH_AVAILABLE;
	status.epoch = 0;
	EXPECT_EQ(nsm_fpga_session_classify(&status),
		  NSM_FPGA_SESSION_UNINITIALIZED);
}

TEST(fpgaPlan, controlFramesCarryTheGrant)
{
	nsm_fpga_step steps[NSM_FPGA_PLAN_CONTROL_STEPS];
	ASSERT_EQ(nsm_fpga_plan_control(&windowP0, &grantP0,
					NSM_FPGA_CTRL_ACQUIRE, 1, 0, steps),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(steps[0].kind, NSM_FPGA_STEP_WRITE);
	EXPECT_EQ(steps[0].address, 0x1000u);
	EXPECT_EQ(steps[0].record[4], NSM_FPGA_CTRL_ACQUIRE);
	EXPECT_EQ(steps[0].record[24], 1);
	EXPECT_EQ(steps[0].record[16], 0x0d);
	EXPECT_EQ(steps[0].record[20], 2);
	EXPECT_EQ(steps[1].kind, NSM_FPGA_STEP_READ);
	EXPECT_EQ(steps[1].address, 0x1020u);
	// RELEASE carries the last verified data sequence as its argument
	ASSERT_EQ(nsm_fpga_plan_control(&windowP0, &grantP0,
					NSM_FPGA_CTRL_RELEASE, 2, 5, steps),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(steps[0].record[4], NSM_FPGA_CTRL_RELEASE);
	EXPECT_EQ(steps[0].record[24], 2);
	EXPECT_EQ(steps[0].record[28], 5);
	// A zero grant field is refused by the record encoder
	const nsm_fpga_grant bad{0, 1, 1};
	EXPECT_NE(nsm_fpga_plan_control(&windowP0, &bad, NSM_FPGA_CTRL_ACQUIRE,
					1, 0, steps),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(nsm_fpga_plan_control(nullptr, &grantP0,
					NSM_FPGA_CTRL_ACQUIRE, 1, 0, steps),
		  NSM_SW_ERROR_NULL);
}

TEST(fpgaPlan, statusReadAndAuthorityPlans)
{
	nsm_fpga_step status;
	ASSERT_EQ(nsm_fpga_plan_status_read(&windowP0, &status),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(status.kind, NSM_FPGA_STEP_READ);
	EXPECT_EQ(status.address, 0x10a0u);
	const auto id = nsm_fpga_plan_identity(&grantP0, 7);
	EXPECT_EQ(id.epoch, grantP0.epoch);
	EXPECT_EQ(id.sequence, 7u);
	// The activate plan is the recover plan with its own opcode
	nsm_fpga_step authority[NSM_FPGA_PLAN_CONTROL_STEPS];
	ASSERT_EQ(nsm_fpga_plan_authority(&windowP0, NSM_FPGA_CTRL_ACTIVATE,
					  0x0102030405060708ULL, authority),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(authority[0].address, 0x1000u);
	EXPECT_EQ(authority[0].record[4], NSM_FPGA_CTRL_ACTIVATE);
	EXPECT_EQ(authority[0].record[8], 0x08);
	EXPECT_EQ(authority[0].record[16], 0);
	EXPECT_EQ(authority[1].address, 0x1020u);
	EXPECT_EQ(nsm_fpga_plan_authority(&windowP0, NSM_FPGA_CTRL_ACQUIRE, 1,
					  authority),
		  NSM_SW_ERROR_DATA);
}

TEST(fpgaPlan, wordIsOneRecordThenResult)
{
	const nsm_fpga_window &window = windowP0;
	nsm_fpga_step steps[NSM_FPGA_PLAN_COMPACT_STEPS];
	ASSERT_EQ(nsm_fpga_plan_word_compact(&window, &grantP0, 3, 28,
					     NSM_FPGA_OP_READ32, 0x05000400ULL,
					     0, steps),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(steps[0].kind, NSM_FPGA_STEP_WRITE);
	EXPECT_EQ(steps[0].address, 0x10e0u);
	// identity first: epoch, session, generation, sequence
	EXPECT_EQ(steps[0].record[0], 0x88);
	EXPECT_EQ(steps[0].record[7], 0x11);
	EXPECT_EQ(steps[0].record[8], 0x0d);
	EXPECT_EQ(steps[0].record[12], 2);
	EXPECT_EQ(steps[0].record[16], 3);
	EXPECT_EQ(steps[0].record[20], 28);
	EXPECT_EQ(steps[0].record[21], NSM_FPGA_OP_READ32);
	EXPECT_EQ(steps[0].record[22], 0x0f);
	EXPECT_EQ(steps[0].record[23], NSM_FPGA_COMPACT_RECORD_VERSION);
	EXPECT_EQ(steps[0].record[24], 0x00);
	EXPECT_EQ(steps[0].record[25], 0x04);
	EXPECT_EQ(steps[0].record[27], 0x05);
	EXPECT_EQ(steps[0].record[28], 0);
	EXPECT_EQ(steps[1].kind, NSM_FPGA_STEP_READ);
	EXPECT_EQ(steps[1].address, 0x1060u);
	ASSERT_EQ(nsm_fpga_plan_word_compact(&window, &grantP0, 4, 28,
					     NSM_FPGA_OP_WRITE32, 0x0500045cULL,
					     0xa5b6c7d8, steps),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(steps[0].record[21], NSM_FPGA_OP_WRITE32);
	EXPECT_EQ(steps[0].record[28], 0xd8);
	EXPECT_EQ(steps[0].record[31], 0xa5);
	// a map without the word window cannot plan a word
	nsm_fpga_window noWords = windowP0;
	noWords.compact = 0;
	EXPECT_EQ(nsm_fpga_plan_word_compact(&noWords, &grantP0, 1, 28,
					     NSM_FPGA_OP_READ32, 0x05000400ULL,
					     0, steps),
		  NSM_SW_ERROR_DATA);
	uint8_t record[NSM_FPGA_RECORD_SIZE];
	// unaligned, beyond 32 bits, read with data, empty write mask
	EXPECT_EQ(nsm_fpga_encode_word_compact(&grantIdentity, 28,
					       NSM_FPGA_OP_READ32, 0x0f, 0x401,
					       0, record),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&grantIdentity, 28,
					       NSM_FPGA_OP_READ32, 0x0f,
					       0x100000000ULL, 0, record),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&grantIdentity, 28,
					       NSM_FPGA_OP_READ32, 0x0f, 0x400,
					       1, record),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&grantIdentity, 28,
					       NSM_FPGA_OP_WRITE32, 0, 0x400, 1,
					       record),
		  NSM_SW_ERROR_DATA);
	// a disabled byte must be zero, an enabled one may carry data
	EXPECT_EQ(nsm_fpga_encode_word_compact(&grantIdentity, 28,
					       NSM_FPGA_OP_WRITE32, 0x01, 0x400,
					       0x100, record),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&grantIdentity, 28,
					       NSM_FPGA_OP_WRITE32, 0x01, 0x400,
					       0xff, record),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(record[22], 0x01);
	EXPECT_EQ(nsm_fpga_encode_word_compact(&grantIdentity, 28,
					       NSM_FPGA_OP_READ32, 0x0f, 0x400,
					       0, nullptr),
		  NSM_SW_ERROR_NULL);
}

// Records the FPGA could return corrupted: every decoder and classifier
// must answer MALFORMED, FOREIGN or REJECTED and touch nothing outside the
// record, so a bad register value can never crash the daemon
TEST(fpgaClassify, randomRecordsAreRejectedWithoutHarm)
{
	uint32_t seed = 0x5eed1234u;
	auto next = [&seed]() {
		seed = seed * 1664525u + 1013904223u;
		return seed;
	};
	size_t accepted = 0;
	for (int trial = 0; trial < 20000; ++trial) {
		Record record{};
		for (auto &byte : record) {
			byte = static_cast<uint8_t>(next() >> 24);
		}
		// half the trials start with the profile so decoders go deeper
		if ((trial & 1) != 0) {
			putLe(record, 0, NSM_FPGA_PROFILE_IBC2, 4);
		}
		nsm_fpga_control_completion control{};
		nsm_fpga_decode_control_completion(record.data(), &control);
		nsm_fpga_word_completion word{};
		nsm_fpga_decode_word_completion(record.data(), &word);
		nsm_fpga_authority_status status{};
		nsm_fpga_decode_authority_status(record.data(), &status);
		uint8_t result = 0;
		uint32_t data = 0;
		uint8_t reason = 0;
		uint8_t byte = 0;
		bool nack = false;
		const auto c = nsm_fpga_classify_control(
		    record.data(), &grantIdentity, NSM_FPGA_CTRL_ACQUIRE, 0,
		    &result);
		const auto a = nsm_fpga_classify_authority(
		    record.data(), grantIdentity.epoch, NSM_FPGA_CTRL_RECOVER,
		    &result);
		const auto w =
		    nsm_fpga_classify_word(record.data(), &grantIdentity,
					   NSM_FPGA_OP_READ32, &data, &reason);
		const auto b = nsm_fpga_classify_i2c_byte(
		    record.data(), &grantIdentity, 0x00010050u, &byte, &nack);
		const auto x = nsm_fpga_classify_i2c_xfer(
		    record.data(), &grantIdentity, 0x49, &data, &nack, &reason);
		for (auto reply : {c, a, w, b, x}) {
			// only a record carrying this very grant can be DONE
			if (reply == NSM_FPGA_REPLY_DONE) {
				accepted++;
			}
		}
	}
	EXPECT_EQ(accepted, 0u);
}

TEST(fpgaClassify, controlReceipts)
{
	uint8_t result = 0xff;
	auto ok = controlReply(grantIdentity, NSM_FPGA_CTRL_ACQUIRE, 0,
			       NSM_FPGA_CTRL_OK, false);
	EXPECT_EQ(nsm_fpga_classify_control(ok.data(), &grantIdentity,
					    NSM_FPGA_CTRL_ACQUIRE, 0, &result),
		  NSM_FPGA_REPLY_DONE);
	/* result is written for a rejected receipt only */
	EXPECT_EQ(result, 0xff);
	auto pending = controlReply(grantIdentity, NSM_FPGA_CTRL_ACQUIRE, 0,
				    NSM_FPGA_CTRL_OK, true);
	EXPECT_EQ(nsm_fpga_classify_control(pending.data(), &grantIdentity,
					    NSM_FPGA_CTRL_ACQUIRE, 0, &result),
		  NSM_FPGA_REPLY_PENDING);
	auto empty = emptyReply();
	EXPECT_EQ(nsm_fpga_classify_control(empty.data(), &grantIdentity,
					    NSM_FPGA_CTRL_ACQUIRE, 0, &result),
		  NSM_FPGA_REPLY_PENDING);
	auto stale = controlReply(grantIdentity, NSM_FPGA_CTRL_ACQUIRE, 0,
				  NSM_FPGA_CTRL_STALE, false);
	EXPECT_EQ(nsm_fpga_classify_control(stale.data(), &grantIdentity,
					    NSM_FPGA_CTRL_ACQUIRE, 0, &result),
		  NSM_FPGA_REPLY_REJECTED);
	EXPECT_EQ(result, NSM_FPGA_CTRL_STALE);
	nsm_fpga_identity other = grantIdentity;
	other.session++;
	auto foreign = controlReply(other, NSM_FPGA_CTRL_ACQUIRE, 0,
				    NSM_FPGA_CTRL_OK, false);
	EXPECT_EQ(nsm_fpga_classify_control(foreign.data(), &grantIdentity,
					    NSM_FPGA_CTRL_ACQUIRE, 0, &result),
		  NSM_FPGA_REPLY_FOREIGN);
	Record junk{};
	EXPECT_EQ(nsm_fpga_classify_control(junk.data(), &grantIdentity,
					    NSM_FPGA_CTRL_ACQUIRE, 0, &result),
		  NSM_FPGA_REPLY_MALFORMED);
	EXPECT_STREQ(nsm_fpga_reply_name(NSM_FPGA_REPLY_REJECTED), "rejected");
}

TEST(fpgaClassify, transactionCompletions)
{
	const nsm_fpga_identity id = {0x68e7a1c0, 1, 1, 7};
	Record header{};
	EXPECT_TRUE(nsm_fpga_xfer_opcode_valid(0x49));
	EXPECT_TRUE(nsm_fpga_xfer_opcode_valid(0x58));
	EXPECT_FALSE(nsm_fpga_xfer_opcode_valid(0x40));
	EXPECT_FALSE(nsm_fpga_xfer_opcode_valid(0x45));
	EXPECT_FALSE(nsm_fpga_xfer_opcode_valid(0x20));
	// pointer byte 0x0e then two bytes read
	ASSERT_EQ(
	    nsm_fpga_encode_xfer_header(&id, 1, 0x4a, 0x0e50, header.data()),
	    NSM_SW_SUCCESS);
	EXPECT_EQ(header[4], 0x4a);
	EXPECT_EQ(header[5], 1);
	EXPECT_EQ(header[28], 0x50);
	EXPECT_EQ(header[29], 0x0e);
	// a write byte beyond the announced count, a reserved target
	EXPECT_EQ(
	    nsm_fpga_encode_xfer_header(&id, 1, 0x4a, 0x010e50, header.data()),
	    NSM_SW_ERROR_DATA);
	EXPECT_EQ(
	    nsm_fpga_encode_xfer_header(&id, 1, 0x4a, 0x0e07, header.data()),
	    NSM_SW_ERROR_DATA);
	Record reply{};
	putLe(reply, 0, NSM_FPGA_PROFILE_IBC2, 4);
	reply[6] = NSM_FPGA_OWNER_NSM;
	reply[7] = 0x4a;
	putLe(reply, 8, id.epoch, 8);
	putLe(reply, 16, id.session, 4);
	putLe(reply, 20, id.generation, 4);
	putLe(reply, 24, id.sequence, 4);
	uint32_t data = 0;
	bool nack = true;
	uint8_t reason = 0;
	// success carries the read bytes and a released bus
	reply[4] = NSM_FPGA_RESULT_SUCCESS;
	reply[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
		   NSM_FPGA_FLAG_BUS_RELEASED | NSM_FPGA_FLAG_READ_VALID;
	putLe(reply, 28, 0x3412, 4);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(reply.data(), &id, 0x4a, &data,
					     &nack, &reason),
		  NSM_FPGA_REPLY_DONE);
	EXPECT_EQ(data, 0x3412u);
	EXPECT_FALSE(nack);
	// a byte beyond the read count is rejected
	putLe(reply, 28, 0x563412, 4);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(reply.data(), &id, 0x4a, &data,
					     &nack, &reason),
		  NSM_FPGA_REPLY_REJECTED);
	// a bus still held after success is rejected
	putLe(reply, 28, 0x3412, 4);
	reply[5] |= NSM_FPGA_FLAG_BUS_HELD;
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(reply.data(), &id, 0x4a, &data,
					     &nack, &reason),
		  NSM_FPGA_REPLY_REJECTED);
	// the target NACK ends the transaction with the bus released
	reply[4] = NSM_FPGA_RESULT_TARGET_NACK;
	reply[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
		   NSM_FPGA_FLAG_BUS_RELEASED;
	putLe(reply, 28, 0x0101, 4);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(reply.data(), &id, 0x4a, &data,
					     &nack, &reason),
		  NSM_FPGA_REPLY_DONE);
	EXPECT_TRUE(nack);
	// pending, fault, another opcode
	reply[4] = NSM_FPGA_RESULT_PENDING;
	reply[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_PENDING;
	putLe(reply, 28, 0, 4);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(reply.data(), &id, 0x4a, &data,
					     &nack, &reason),
		  NSM_FPGA_REPLY_PENDING);
	reply[4] = NSM_FPGA_RESULT_FAULT;
	reply[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
		   NSM_FPGA_FLAG_FAULTED;
	putLe(reply, 28, 0x0002, 4);
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(reply.data(), &id, 0x4a, &data,
					     &nack, &reason),
		  NSM_FPGA_REPLY_FAULT);
	EXPECT_EQ(reason, 2);
	reply[7] = 0x49;
	EXPECT_EQ(nsm_fpga_classify_i2c_xfer(reply.data(), &id, 0x4a, &data,
					     &nack, &reason),
		  NSM_FPGA_REPLY_FOREIGN);
}

TEST(fpgaClassify, wordCompletions)
{
	uint32_t data = 0;
	uint8_t reason = 0;
	auto done = wordReply(grantIdentity, NSM_FPGA_RESULT_SUCCESS,
			      NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
				  NSM_FPGA_FLAG_READ_VALID,
			      0x00000002);
	EXPECT_EQ(nsm_fpga_classify_word(done.data(), &grantIdentity,
					 NSM_FPGA_OP_READ32, &data, &reason),
		  NSM_FPGA_REPLY_DONE);
	EXPECT_EQ(data, 2u);
	auto pending =
	    wordReply(grantIdentity, NSM_FPGA_RESULT_PENDING,
		      NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_PENDING, 0);
	EXPECT_EQ(nsm_fpga_classify_word(pending.data(), &grantIdentity,
					 NSM_FPGA_OP_READ32, &data, &reason),
		  NSM_FPGA_REPLY_PENDING);
	auto empty = emptyReply();
	EXPECT_EQ(nsm_fpga_classify_word(empty.data(), &grantIdentity,
					 NSM_FPGA_OP_READ32, &data, &reason),
		  NSM_FPGA_REPLY_PENDING);
	auto fault = wordReply(grantIdentity, NSM_FPGA_RESULT_FAULT,
			       NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_FAULTED,
			       0x00000203);
	EXPECT_EQ(nsm_fpga_classify_word(fault.data(), &grantIdentity,
					 NSM_FPGA_OP_READ32, &data, &reason),
		  NSM_FPGA_REPLY_FAULT);
	nsm_fpga_identity other = grantIdentity;
	other.sequence++;
	auto foreign =
	    wordReply(other, NSM_FPGA_RESULT_SUCCESS,
		      NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_READ_VALID, 1);
	EXPECT_EQ(nsm_fpga_classify_word(foreign.data(), &grantIdentity,
					 NSM_FPGA_OP_READ32, &data, &reason),
		  NSM_FPGA_REPLY_FOREIGN);
}
