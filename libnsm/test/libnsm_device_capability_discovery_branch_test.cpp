/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Branch coverage for device-capability-discovery.c partial branches.
 *
 * Targets uncovered 1/2 branches not exercised by existing tests:
 * L153, L245, L293, L301, L373, L382, L495, L530, L564, L578, L608, L819, L851
 */

#include "base.h"
#include "device-capability-discovery.h"
#include <gtest/gtest.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: minimal non-null buffer (all zeros, no valid NSM header needed for
// functions that don't call unpack_nsm_header)
// ---------------------------------------------------------------------------
static std::vector<uint8_t> minBuf(size_t extra = 0)
{
	return std::vector<uint8_t>(sizeof(nsm_msg_hdr) + extra, 0);
}

// Buffer with error completion_code at payload[1] (nsm_common_resp layout).
// decode_reason_code_and_cc: cc = payload[1]; if size != nsm_hdr+4 →
// ERROR_LENGTH
static std::vector<uint8_t> errCcBuf(size_t sz = sizeof(nsm_msg_hdr) + 100)
{
	std::vector<uint8_t> buf(sz, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // completion_code = non-success
	return buf;
}

// ===========================================================================
// L153: decode_nsm_get_event_source_resp
// if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) TRUE branch
// ===========================================================================
TEST(DevCapDiscBranch, GetEventSourceResp_RcFail)
{
	auto buf = errCcBuf(); // payload[1]=0xFF, size=nsm_hdr+100 != nsm_hdr+4
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	bitfield8_t sources[EVENT_SOURCES_LENGTH] = {};
	auto rc = decode_nsm_get_event_source_resp(msg, buf.size(), &cc,
						   &reason, sources);
	// decode_reason_code_and_cc returns NSM_SW_ERROR_LENGTH → rc != SUCCESS
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L245: decode_nsm_get_event_subscription_resp
// if (response->hdr.data_size != 1) TRUE branch (data_size = 0)
// Requires valid header (function calls unpack_nsm_header)
// ===========================================================================
TEST(DevCapDiscBranch, GetEventSubscriptionResp_DataSizeNotOne)
{
	// Build valid buffer using encode, then corrupt data_size
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_subscription_resp), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// encode produces valid header
	encode_nsm_get_event_subscription_resp(0, NSM_SUCCESS, 0, 1, msg);
	// Corrupt data_size to 0 (≠ 1) — at payload[4..5] LE16
	auto *resp =
	    reinterpret_cast<nsm_get_event_subscription_resp *>(msg->payload);
	resp->hdr.data_size = 0; // triggers L245 TRUE
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t receiver_eid = 0;
	auto rc = decode_nsm_get_event_subscription_resp(
	    msg, buf.size(), &cc, &reason, &receiver_eid);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L293: decode_nsm_set_event_subscription_req
// if (msg_len < sizeof(hdr) + sizeof(req)) TRUE — buffer too small
// ===========================================================================
TEST(DevCapDiscBranch, SetEventSubscriptionReq_MsgLenTooSmall)
{
	auto buf = minBuf(); // only nsm_msg_hdr bytes, no payload
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t global_setting = 0, receiver_eid = 0;
	auto rc = decode_nsm_set_event_subscription_req(
	    msg, buf.size(), &global_setting, &receiver_eid);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA_LENGTH);
}

// ===========================================================================
// L301: decode_nsm_set_event_subscription_req
// if (request->hdr.data_size < NSM_SET_EVENT_SUBSCRIPTION_REQ_DATA_SIZE) TRUE
// data_size at payload[1] = 0 < 2
// ===========================================================================
TEST(DevCapDiscBranch, SetEventSubscriptionReq_DataSizeTooSmall)
{
	// msg_len large enough (>= nsm_hdr + sizeof(req) = 5+4=9)
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_event_subscription_req), 0);
	// data_size = buf[sizeof(nsm_msg_hdr)+1] = 0 < 2 → L301 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t global_setting = 0, receiver_eid = 0;
	auto rc = decode_nsm_set_event_subscription_req(
	    msg, buf.size(), &global_setting, &receiver_eid);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA_LENGTH);
}

// ===========================================================================
// L373: decode_nsm_configure_event_acknowledgement_req
// if (msg_len < sizeof(hdr) + sizeof(req)) TRUE — buffer too small
// ===========================================================================
TEST(DevCapDiscBranch, ConfigureEventAckReq_MsgLenTooSmall)
{
	auto buf = minBuf(); // no payload
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t nvidia_msg_type = 0;
	bitfield8_t *mask = nullptr;
	auto rc = decode_nsm_configure_event_acknowledgement_req(
	    msg, buf.size(), &nvidia_msg_type, &mask);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA_LENGTH);
}

// ===========================================================================
// L382: decode_nsm_configure_event_acknowledgement_req
// if (request->hdr.data_size <
// NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT_REQ_DATA_SIZE) TRUE data_size = 0 < 9
// ===========================================================================
TEST(DevCapDiscBranch, ConfigureEventAckReq_DataSizeTooSmall)
{
	// sizeof(nsm_configure_event_acknowledgement_req) = 2+1+8=11
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_configure_event_acknowledgement_req),
	    0);
	// data_size at payload[1] = 0 < 9 → L382 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t nvidia_msg_type = 0;
	bitfield8_t *mask = nullptr;
	auto rc = decode_nsm_configure_event_acknowledgement_req(
	    msg, buf.size(), &nvidia_msg_type, &mask);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA_LENGTH);
}

// ===========================================================================
// L495: decode_nsm_set_current_event_sources_req
// if (request->hdr.data_size != NSM_SET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE)
// TRUE data_size = 0 != 9
// ===========================================================================
TEST(DevCapDiscBranch, SetCurrentEventSourcesReq_DataSizeMismatch)
{
	// sizeof(nsm_set_current_event_source_req) = 2+1+8=11
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_current_event_source_req), 0);
	// data_size at payload[1] = 0 ≠ 9 → L495 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t nvidia_msg_type = 0;
	bitfield8_t sources[EVENT_SOURCES_LENGTH] = {};
	auto rc = decode_nsm_set_current_event_sources_req(
	    msg, buf.size(), &nvidia_msg_type, sources);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L530: decode_nsm_set_current_event_sources_resp
// if (msg_len < sizeof(hdr) + sizeof(resp)) TRUE — buffer too small
// ===========================================================================
TEST(DevCapDiscBranch, SetCurrentEventSourcesResp_MsgLenTooSmall)
{
	auto buf = minBuf(); // no payload
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc =
	    decode_nsm_set_current_event_sources_resp(msg, buf.size(), &cc);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA_LENGTH);
}

// ===========================================================================
// L564: decode_nsm_get_event_log_record_resp
// if (msg_len < sizeof(hdr) + sizeof(resp)-1) TRUE — buffer too small
// ===========================================================================
TEST(DevCapDiscBranch, GetEventLogRecordResp_MsgLenTooSmall)
{
	auto buf = minBuf(); // no payload
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0, nvidia_msg_type = 0, event_id = 0;
	uint32_t event_handle = 0;
	uint64_t timestamp = 0;
	uint8_t *payload = nullptr;
	uint16_t payload_len = 0;
	auto rc = decode_nsm_get_event_log_record_resp(
	    msg, buf.size(), &cc, &nvidia_msg_type, &event_id, &event_handle,
	    &timestamp, &payload, &payload_len);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA_LENGTH);
}

// ===========================================================================
// L578: decode_nsm_get_event_log_record_resp
// if (data_size > NSM_GET_EVENT_LOG_RECORD_RESP_MIN_DATA_SIZE) TRUE
// data_size = 15 > 14 → *payload and *payload_len populated
// ===========================================================================
TEST(DevCapDiscBranch, GetEventLogRecordResp_DataSizeHasPayload)
{
	// sizeof(nsm_get_event_log_record_resp)-1 = 20 bytes minimum payload
	// Add extra for data beyond min_data_size
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(nsm_get_event_log_record_resp) + 10,
				 0);
	// data_size at payload[4..5] (nsm_common_resp layout, LE16)
	// Set to 15 > NSM_GET_EVENT_LOG_RECORD_RESP_MIN_DATA_SIZE=14
	buf[sizeof(nsm_msg_hdr) + 4] = 15;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0, nvidia_msg_type = 0, event_id = 0;
	uint32_t event_handle = 0;
	uint64_t timestamp = 0;
	uint8_t *payload = nullptr;
	uint16_t payload_len = 0;
	auto rc = decode_nsm_get_event_log_record_resp(
	    msg, buf.size(), &cc, &nvidia_msg_type, &event_id, &event_handle,
	    &timestamp, &payload, &payload_len);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(payload_len, 15 - 14); // 1 byte extra
	EXPECT_NE(payload, nullptr);
}

// ===========================================================================
// L608: decode_nsm_rediscovery_event
// if (event->data_size > 0) TRUE — non-zero data_size → error
// event->data_size is at payload[5] (nsm_event layout)
// ===========================================================================
TEST(DevCapDiscBranch, RediscoveryEvent_DataSizeNonZero)
{
	// msg_len must be >= sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN = 5+6=11
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + 1,
				 0);
	buf[sizeof(nsm_msg_hdr) + 5] =
	    1; // event->data_size = 1 > 0 → L608 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t event_class = 0;
	uint16_t event_state = 0;
	auto rc = decode_nsm_rediscovery_event(msg, buf.size(), &event_class,
					       &event_state);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA_LENGTH);
}

// ===========================================================================
// L819: decode_nsm_gpio_state_change_event
// if (...|| event_state == NULL || payload == NULL) secondary null conditions
// ===========================================================================
TEST(DevCapDiscBranch, GpioStateChangeEvent_NullEventState)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t event_class = 0;
	struct nsm_gpio_state_change_event_payload *payload = nullptr;
	auto rc = decode_nsm_gpio_state_change_event(
	    msg, buf.size(), &event_class, nullptr, &payload);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevCapDiscBranch, GpioStateChangeEvent_NullPayloadPtr)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t event_class = 0;
	uint16_t event_state = 0;
	auto rc = decode_nsm_gpio_state_change_event(
	    msg, buf.size(), &event_class, &event_state, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L851: decode_nsm_gpio_state_change_event
// if (event->data_size != expected_size) TRUE
// data_size=11 but num_gpio_events=0 → expected=10 → 11 ≠ 10
// ===========================================================================
TEST(DevCapDiscBranch, GpioStateChangeEvent_DataSizeMismatch)
{
	// Layout: 5 (hdr) + 6 (nsm_event fixed) + 11 (data) = 22 bytes
	// event->data_size (payload[5]) = 11
	// event->data starts at payload[6]:
	//   timestamp_low [0..3], timestamp_high [4..7],
	//   num_gpio_events [8..9] = 0 → expected_size = 10 ≠ 11
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + 11,
				 0);
	buf[sizeof(nsm_msg_hdr) + 5] = 11; // data_size = 11
	// num_gpio_events at event->data[8..9] = 0 (zero-fill)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t event_class = 0;
	uint16_t event_state = 0;
	struct nsm_gpio_state_change_event_payload *payload = nullptr;
	auto rc = decode_nsm_gpio_state_change_event(
	    msg, buf.size(), &event_class, &event_state, &payload);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_nsm_get_event_log_record_req — L530 TRUE (msg==NULL)
// ===========================================================================
TEST(DevCapDiscBranch, EncodeGetEventLogRecordReq_NullMsg)
{
	auto rc = encode_nsm_get_event_log_record_req(0, 0, 0,
						      nullptr); // msg==NULL
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// ===========================================================================
// encode_nsm_get_event_log_record_req — L530 FALSE (msg != NULL)
// Covers lines L534-552: success path body (FALSE branch of msg==NULL check)
// ===========================================================================
TEST(DevCapDiscBranch, EncodeGetEventLogRecordReq_Success)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_nsm_get_event_log_record_req(0, 0, 0, msg);
	EXPECT_EQ(rc, NSM_SUCCESS);
}
