/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "base.h"
#include "diagnostics.h"

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

/*
 * Tests for Get Diag System Config Event (Event ID 0x00)
 */

TEST(DiagPreBootTest, EncodeGetSystemConfigEventValid)
{
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_get_system_config_event_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_get_system_config_event(
	    5, true, NSM_DIAG_CONFIG_TYPE_MB1, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagPreBootTest, EncodeGetSystemConfigEventNull)
{
	int rc =
	    encode_nsm_diag_get_system_config_event(5, true, 0x00, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagPreBootTest, GetSystemConfigEventRoundTrip)
{
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_get_system_config_event_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_get_system_config_event(
	    5, true, NSM_DIAG_CONFIG_TYPE_TEST, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t event_class = 0xFF;
	uint16_t event_state = 0xFFFF;
	uint8_t config_type = 0xFF;

	rc = decode_nsm_diag_get_system_config_event(
	    msg, eventMsg.size(), &event_class, &event_state, &config_type);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(event_class, NSM_GENERAL_EVENT_CLASS);
	EXPECT_EQ(event_state, 0);
	EXPECT_EQ(config_type, NSM_DIAG_CONFIG_TYPE_TEST);
}

TEST(DiagPreBootTest, DecodeGetSystemConfigEventNull)
{
	uint8_t event_class;
	uint16_t event_state;
	uint8_t config_type;

	EXPECT_EQ(decode_nsm_diag_get_system_config_event(
		      nullptr, 0, &event_class, &event_state, &config_type),
		  NSM_SW_ERROR_NULL);

	std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
				      1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	EXPECT_EQ(decode_nsm_diag_get_system_config_event(msg, eventMsg.size(),
							  nullptr, &event_state,
							  &config_type),
		  NSM_SW_ERROR_NULL);
}

TEST(DiagPreBootTest, DecodeGetSystemConfigEventShortLength)
{
	std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());
	uint8_t event_class;
	uint16_t event_state;
	uint8_t config_type;

	EXPECT_EQ(
	    decode_nsm_diag_get_system_config_event(
		msg, eventMsg.size(), &event_class, &event_state, &config_type),
	    NSM_SW_ERROR_LENGTH);
}

/*
 * Tests for Get Diag TID Config Event (Event ID 0x01)
 */

TEST(DiagPreBootTest, EncodeGetTidConfigEventValid)
{
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_get_tid_config_event_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_get_tid_config_event(5, true, 0x01, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagPreBootTest, EncodeGetTidConfigEventNull)
{
	int rc = encode_nsm_diag_get_tid_config_event(5, true, 0x01, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagPreBootTest, GetTidConfigEventRoundTrip)
{
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_get_tid_config_event_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_get_tid_config_event(5, true, 0x09, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t event_class = 0xFF;
	uint16_t event_state = 0xFFFF;
	uint8_t tid = 0xFF;

	rc = decode_nsm_diag_get_tid_config_event(
	    msg, eventMsg.size(), &event_class, &event_state, &tid);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(event_class, NSM_GENERAL_EVENT_CLASS);
	EXPECT_EQ(event_state, 0);
	EXPECT_EQ(tid, 0x09);
}

TEST(DiagPreBootTest, DecodeGetTidConfigEventNull)
{
	uint8_t event_class;
	uint16_t event_state;
	uint8_t tid;

	EXPECT_EQ(decode_nsm_diag_get_tid_config_event(nullptr, 0, &event_class,
						       &event_state, &tid),
		  NSM_SW_ERROR_NULL);
}

/*
 * Tests for Set Diag Test Result Event (Event ID 0x02)
 */

TEST(DiagPreBootTest, EncodeSetTestResultEventValid)
{
	uint8_t dynamic_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_set_test_result_event_data) - 1 +
	    sizeof(dynamic_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_set_test_result_event(
	    5, true, 0x01, NSM_DIAG_TEST_PASS, sizeof(dynamic_data),
	    dynamic_data, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagPreBootTest, EncodeSetTestResultEventNull)
{
	int rc = encode_nsm_diag_set_test_result_event(5, true, 0x01, 0, 0,
						       nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagPreBootTest, EncodeSetTestResultEventNullDynamicData)
{
	std::vector<uint8_t> eventMsg(256);
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_set_test_result_event(5, true, 0x01, 0, 4,
						       nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagPreBootTest, SetTestResultEventRoundTrip)
{
	uint8_t dynamic_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_set_test_result_event_data) - 1 +
	    sizeof(dynamic_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_set_test_result_event(
	    5, true, 0x01, 0x0006, sizeof(dynamic_data), dynamic_data, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t event_class = 0xFF;
	uint16_t event_state = 0xFFFF;
	uint8_t tid = 0xFF;
	uint16_t test_error_code = 0xFFFF;
	uint8_t dyn_data_size = 0xFF;
	uint8_t dyn_data[4] = {};

	rc = decode_nsm_diag_set_test_result_event(
	    msg, eventMsg.size(), &event_class, &event_state, &tid,
	    &test_error_code, &dyn_data_size, dyn_data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(event_class, NSM_GENERAL_EVENT_CLASS);
	EXPECT_EQ(event_state, 0);
	EXPECT_EQ(tid, 0x01);
	EXPECT_EQ(test_error_code, 0x0006);
	EXPECT_EQ(dyn_data_size, sizeof(dynamic_data));
	EXPECT_EQ(memcmp(dyn_data, dynamic_data, sizeof(dynamic_data)), 0);
}

TEST(DiagPreBootTest, SetTestResultEventRoundTripNoDynamicData)
{
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_set_test_result_event_data) - 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_set_test_result_event(
	    5, true, 0x02, NSM_DIAG_TEST_FAIL, 0, nullptr, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t event_class, tid, dyn_data_size;
	uint16_t event_state, test_error_code;

	rc = decode_nsm_diag_set_test_result_event(
	    msg, eventMsg.size(), &event_class, &event_state, &tid,
	    &test_error_code, &dyn_data_size, nullptr);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(tid, 0x02);
	EXPECT_EQ(test_error_code, NSM_DIAG_TEST_FAIL);
	EXPECT_EQ(dyn_data_size, 0);
}

TEST(DiagPreBootTest, SetTestResultEventEndianness)
{
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_set_test_result_event_data) - 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_set_test_result_event(5, true, 0x01, 0x1234, 0,
						       nullptr, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t event_class, tid, dyn_data_size;
	uint16_t event_state, test_error_code;

	rc = decode_nsm_diag_set_test_result_event(
	    msg, eventMsg.size(), &event_class, &event_state, &tid,
	    &test_error_code, &dyn_data_size, nullptr);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(test_error_code, 0x1234);
}

TEST(DiagPreBootTest, DecodeSetTestResultEventNull)
{
	uint8_t event_class, tid, dyn_data_size;
	uint16_t event_state, test_error_code;

	EXPECT_EQ(decode_nsm_diag_set_test_result_event(
		      nullptr, 0, &event_class, &event_state, &tid,
		      &test_error_code, &dyn_data_size, nullptr),
		  NSM_SW_ERROR_NULL);
}

/*
 * Tests for Set Diag Flow Control Event (Event ID 0x03)
 */

TEST(DiagPreBootTest, EncodeSetFlowControlEventValid)
{
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_set_flow_control_event_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_set_flow_control_event(
	    5, false, NSM_DIAG_FLOW_CTRL_IN_PROGRESS, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagPreBootTest, EncodeSetFlowControlEventNull)
{
	int rc =
	    encode_nsm_diag_set_flow_control_event(5, false, 0x01, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagPreBootTest, SetFlowControlEventRoundTrip)
{
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_set_flow_control_event_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_set_flow_control_event(
	    5, false, NSM_DIAG_FLOW_CTRL_EXECUTION_FINISHED, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t event_class = 0xFF;
	uint16_t event_state = 0xFFFF;
	uint8_t flow_ctrl_status = 0xFF;

	rc = decode_nsm_diag_set_flow_control_event(msg, eventMsg.size(),
						    &event_class, &event_state,
						    &flow_ctrl_status);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(event_class, NSM_GENERAL_EVENT_CLASS);
	EXPECT_EQ(event_state, 0);
	EXPECT_EQ(flow_ctrl_status, NSM_DIAG_FLOW_CTRL_EXECUTION_FINISHED);
}

TEST(DiagPreBootTest, DecodeSetFlowControlEventNull)
{
	uint8_t event_class;
	uint16_t event_state;
	uint8_t flow_ctrl_status;

	EXPECT_EQ(
	    decode_nsm_diag_set_flow_control_event(
		nullptr, 0, &event_class, &event_state, &flow_ctrl_status),
	    NSM_SW_ERROR_NULL);
}

/*
 * Tests for Set Diag System Config Command (Command 0x80)
 */

TEST(DiagPreBootTest, EncodeSetSystemConfigReqValid)
{
	uint8_t dynamic_data[] = {0x11, 0x22, 0x33};
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_system_config_req) - 1 +
				    sizeof(dynamic_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_system_config_req(
	    5, NSM_DIAG_CONFIG_TYPE_MB1, 0x02, dynamic_data,
	    sizeof(dynamic_data), msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *request =
	    reinterpret_cast<struct nsm_diag_set_system_config_req *>(
		msg->payload);
	EXPECT_EQ(request->hdr.command, NSM_DIAG_SET_SYSTEM_CONFIG);
	EXPECT_EQ(request->config_type, NSM_DIAG_CONFIG_TYPE_MB1);
	EXPECT_EQ(request->system_test_duration, 0x02);
}

TEST(DiagPreBootTest, EncodeSetSystemConfigReqNull)
{
	int rc =
	    encode_nsm_diag_set_system_config_req(5, 0, 0, nullptr, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagPreBootTest, SetSystemConfigReqRoundTrip)
{
	uint8_t dynamic_data[] = {0x11, 0x22, 0x33};
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_system_config_req) - 1 +
				    sizeof(dynamic_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_system_config_req(
	    5, NSM_DIAG_CONFIG_TYPE_TEST, 0x03, dynamic_data,
	    sizeof(dynamic_data), msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t config_type = 0xFF;
	uint8_t system_test_duration = 0xFF;
	uint8_t dyn_data_size = 0xFF;
	uint8_t dyn_data[3] = {};

	rc = decode_nsm_diag_set_system_config_req(
	    msg, reqMsg.size(), &config_type, &system_test_duration,
	    &dyn_data_size, dyn_data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(config_type, NSM_DIAG_CONFIG_TYPE_TEST);
	EXPECT_EQ(system_test_duration, 0x03);
	EXPECT_EQ(dyn_data_size, sizeof(dynamic_data));
	EXPECT_EQ(memcmp(dyn_data, dynamic_data, sizeof(dynamic_data)), 0);
}

TEST(DiagPreBootTest, DecodeSetSystemConfigReqNull)
{
	uint8_t config_type, duration, dyn_size;

	EXPECT_EQ(decode_nsm_diag_set_system_config_req(
		      nullptr, 0, &config_type, &duration, &dyn_size, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(DiagPreBootTest, DecodeSetSystemConfigReqShortLength)
{
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) + 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());
	uint8_t config_type, duration, dyn_size;

	EXPECT_EQ(decode_nsm_diag_set_system_config_req(msg, reqMsg.size(),
							&config_type, &duration,
							&dyn_size, nullptr),
		  NSM_SW_ERROR_LENGTH);
}

/*
 * Tests for Set Diag TID Config Command (Command 0x81)
 */

TEST(DiagPreBootTest, EncodeSetTidConfigReqValid)
{
	uint8_t dynamic_data[] = {0xAA, 0xBB};
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_tid_config_req) - 1 +
				    sizeof(dynamic_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_tid_config_req(
	    5, 0x01, 0x02, 100, NSM_DIAG_LOG_ERROR, sizeof(dynamic_data),
	    dynamic_data, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *request = reinterpret_cast<struct nsm_diag_set_tid_config_req *>(
	    msg->payload);
	EXPECT_EQ(request->hdr.command, NSM_DIAG_SET_TID_CONFIG);
	EXPECT_EQ(request->tid, 0x01);
	EXPECT_EQ(request->tid_test_duration, 0x02);
}

TEST(DiagPreBootTest, EncodeSetTidConfigReqNull)
{
	int rc = encode_nsm_diag_set_tid_config_req(5, 0, 0, 0, 0, 0, nullptr,
						    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagPreBootTest, SetTidConfigReqRoundTrip)
{
	uint8_t dynamic_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_tid_config_req) - 1 +
				    sizeof(dynamic_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_tid_config_req(
	    5, 0x09, 0x03, 500, NSM_DIAG_LOG_DEBUG, sizeof(dynamic_data),
	    dynamic_data, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t tid = 0xFF;
	uint8_t tid_test_duration = 0xFF;
	uint16_t loops = 0xFFFF;
	uint8_t console_log_level = 0xFF;
	uint8_t dyn_data_size = 0xFF;
	uint8_t dyn_data[4] = {};

	rc = decode_nsm_diag_set_tid_config_req(
	    msg, reqMsg.size(), &tid, &tid_test_duration, &loops,
	    &console_log_level, &dyn_data_size, dyn_data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(tid, 0x09);
	EXPECT_EQ(tid_test_duration, 0x03);
	EXPECT_EQ(loops, 500);
	EXPECT_EQ(console_log_level, NSM_DIAG_LOG_DEBUG);
	EXPECT_EQ(dyn_data_size, sizeof(dynamic_data));
	EXPECT_EQ(memcmp(dyn_data, dynamic_data, sizeof(dynamic_data)), 0);
}

TEST(DiagPreBootTest, SetTidConfigReqEndianness)
{
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_tid_config_req) - 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_tid_config_req(5, 0x01, 0x01, 0x1234, 0x00,
						    0, nullptr, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t tid, duration, log_level, dyn_size;
	uint16_t loops;

	rc = decode_nsm_diag_set_tid_config_req(msg, reqMsg.size(), &tid,
						&duration, &loops, &log_level,
						&dyn_size, nullptr);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(loops, 0x1234);
}

TEST(DiagPreBootTest, DecodeSetTidConfigReqNull)
{
	uint8_t tid, duration, log_level, dyn_size;
	uint16_t loops;

	EXPECT_EQ(decode_nsm_diag_set_tid_config_req(
		      nullptr, 0, &tid, &duration, &loops, &log_level,
		      &dyn_size, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(DiagPreBootTest, DecodeSetTidConfigReqShortLength)
{
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) + 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());
	uint8_t tid, duration, log_level, dyn_size;
	uint16_t loops;

	EXPECT_EQ(decode_nsm_diag_set_tid_config_req(
		      msg, reqMsg.size(), &tid, &duration, &loops, &log_level,
		      &dyn_size, nullptr),
		  NSM_SW_ERROR_LENGTH);
}

/*
 * Missing coverage tests
 */

TEST(DiagPreBootTest, EncodeSetTestResultEventOversizeDynamicData)
{
	uint8_t bigData[NSM_DIAG_MAX_DYNAMIC_DATA_SIZE + 1] = {};
	std::vector<uint8_t> eventMsg(512);
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_set_test_result_event(
	    5, true, 0x01, 0, sizeof(bigData), bigData, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DiagPreBootTest, EncodeSetSystemConfigReqOversizeDynamicData)
{
	uint8_t bigData[NSM_DIAG_MAX_DYNAMIC_DATA_SIZE + 1] = {};
	std::vector<uint8_t> reqMsg(512);
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_system_config_req(5, 0, 0, bigData,
						       sizeof(bigData), msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DiagPreBootTest, EncodeSetTidConfigReqOversizeDynamicData)
{
	/* TID config has a tighter cap than the event-side 251 because
	 * the TID command also carries 6 fixed bytes in the 256-byte budget. */
	uint8_t bigData[NSM_DIAG_MAX_TID_DYNAMIC_DATA_SIZE + 1] = {};
	std::vector<uint8_t> reqMsg(512);
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_tid_config_req(
	    5, 0x01, 0, 1, 0, sizeof(bigData), bigData, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DiagPreBootTest, EncodeSetTidConfigReqAtTidCapBoundary)
{
	/* Right at the cap (244) must succeed. */
	uint8_t data[NSM_DIAG_MAX_TID_DYNAMIC_DATA_SIZE] = {};
	std::vector<uint8_t> reqMsg(512);
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_tid_config_req(5, 0x01, 0, 1, 0,
						    sizeof(data), data, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagPreBootTest, DecodeGetTidConfigEventShortLength)
{
	std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());
	uint8_t event_class;
	uint16_t event_state;
	uint8_t tid;

	EXPECT_EQ(decode_nsm_diag_get_tid_config_event(
		      msg, eventMsg.size(), &event_class, &event_state, &tid),
		  NSM_SW_ERROR_LENGTH);
}

TEST(DiagPreBootTest, DecodeSetTestResultEventShortLength)
{
	std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());
	uint8_t event_class, tid, dyn_data_size;
	uint16_t event_state, test_error_code;

	EXPECT_EQ(decode_nsm_diag_set_test_result_event(
		      msg, eventMsg.size(), &event_class, &event_state, &tid,
		      &test_error_code, &dyn_data_size, nullptr),
		  NSM_SW_ERROR_LENGTH);
}

TEST(DiagPreBootTest, DecodeSetFlowControlEventShortLength)
{
	std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());
	uint8_t event_class;
	uint16_t event_state;
	uint8_t flow_ctrl_status;

	EXPECT_EQ(decode_nsm_diag_set_flow_control_event(
		      msg, eventMsg.size(), &event_class, &event_state,
		      &flow_ctrl_status),
		  NSM_SW_ERROR_LENGTH);
}

TEST(DiagPreBootTest, DecodeSetSystemConfigReqTruncatedDataSize)
{
	// Build a valid-looking message but with hdr.data_size < fixed fields
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_system_config_req) - 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	// Encode a valid request first to set up headers
	int rc = encode_nsm_diag_set_system_config_req(5, 0x00, 0x01, nullptr,
						       0, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	// Corrupt data_size to be less than the two fixed fields
	auto *request =
	    reinterpret_cast<struct nsm_diag_set_system_config_req *>(
		msg->payload);
	request->hdr.data_size =
	    0; // less than sizeof(config_type) + sizeof(duration)

	uint8_t config_type, duration, dyn_size;
	rc = decode_nsm_diag_set_system_config_req(
	    msg, reqMsg.size(), &config_type, &duration, &dyn_size, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(DiagPreBootTest, DecodeSetSystemConfigReqOOBDynamicData)
{
	// Encode a valid request with no dynamic data
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_system_config_req) - 1);
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_system_config_req(5, 0x00, 0x01, nullptr,
						       0, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	// Corrupt data_size to claim large dynamic data beyond buffer
	auto *request =
	    reinterpret_cast<struct nsm_diag_set_system_config_req *>(
		msg->payload);
	request->hdr.data_size = 200; // claims 198 bytes of dynamic data

	uint8_t config_type, duration, dyn_size;
	uint8_t dyn_data[256] = {};
	rc = decode_nsm_diag_set_system_config_req(
	    msg, reqMsg.size(), &config_type, &duration, &dyn_size, dyn_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DiagPreBootTest, DecodeSetTestResultEventNullDynamicDataWithNonzeroSize)
{
	// Encode a valid event with dynamic data
	uint8_t dynamic_data[] = {0xAA, 0xBB};
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_set_test_result_event_data) - 1 +
	    sizeof(dynamic_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	int rc = encode_nsm_diag_set_test_result_event(
	    5, true, 0x01, 0x0000, sizeof(dynamic_data), dynamic_data, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	// Decode with NULL dynamic_data buffer — should return error
	uint8_t event_class, tid, dyn_data_size;
	uint16_t event_state, test_error_code;

	rc = decode_nsm_diag_set_test_result_event(
	    msg, eventMsg.size(), &event_class, &event_state, &tid,
	    &test_error_code, &dyn_data_size, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

/* C1 attack vector: a crafted Set Diag Test Result event with
 * data_size at the wire-level upper bound (255) and dynamic_data_size
 * greater than the spec cap (251) must be rejected. Without the cap
 * check the caller's stack buffer (sized to NSM_DIAG_MAX_DYNAMIC_DATA_SIZE)
 * would overflow during memcpy. */
TEST(DiagPreBootTest, DecodeSetTestResultEventOverProtocolCap)
{
	/* Build a buffer large enough to hold the crafted dynamic_data_size,
	 * so the inner data_size check passes but our new cap check fires. */
	std::vector<uint8_t> eventMsg(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
	    sizeof(nsm_diag_set_test_result_event_data) - 1 + 255);
	auto *msg = reinterpret_cast<struct nsm_msg *>(eventMsg.data());

	/* Encode with valid (small) data first so the headers are well-formed.
	 */
	uint8_t small[1] = {0};
	int rc = encode_nsm_diag_set_test_result_event(5, true, 0x01, 0x0000, 1,
						       small, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	/* Now corrupt the wire to claim 252 bytes of dynamic data. The wire
	 * data_size is uint8_t (max 255), so we set it to its upper bound;
	 * the cap check on dynamic_data_size fires before the inner
	 * consistency check, so this is enough to exercise the guard. */
	auto *event = reinterpret_cast<struct nsm_event *>(msg->payload);
	event->data_size = 255;
	auto *data =
	    reinterpret_cast<struct nsm_diag_set_test_result_event_data *>(
		event->data);
	data->dynamic_data_size = 252; /* > NSM_DIAG_MAX_DYNAMIC_DATA_SIZE */

	uint8_t out_buf[NSM_DIAG_MAX_DYNAMIC_DATA_SIZE] = {};
	uint8_t event_class, tid, dyn_data_size;
	uint16_t event_state, test_error_code;
	rc = decode_nsm_diag_set_test_result_event(
	    msg, eventMsg.size(), &event_class, &event_state, &tid,
	    &test_error_code, &dyn_data_size, out_buf);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

/* C4: decode_nsm_diag_set_system_config_req must error when dynamic_data is
 * NULL but the message claims a non-zero dynamic payload. */
TEST(DiagPreBootTest, DecodeSetSystemConfigReqNullBufferWithSize)
{
	uint8_t dynamic_data[] = {0x11, 0x22, 0x33};
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_system_config_req) - 1 +
				    sizeof(dynamic_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_system_config_req(
	    5, 0, 0x01, dynamic_data, sizeof(dynamic_data), msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t config_type, duration, dyn_size;
	rc = decode_nsm_diag_set_system_config_req(
	    msg, reqMsg.size(), &config_type, &duration, &dyn_size, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

/* C4: decode_nsm_diag_set_tid_config_req must error when dynamic_data is
 * NULL but the message claims a non-zero dynamic payload. */
TEST(DiagPreBootTest, DecodeSetTidConfigReqNullBufferWithSize)
{
	uint8_t dynamic_data[] = {0xAA, 0xBB};
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_tid_config_req) - 1 +
				    sizeof(dynamic_data));
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_tid_config_req(
	    5, 0x01, 0x01, 1, 0, sizeof(dynamic_data), dynamic_data, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t tid, duration, log_level, dyn_size;
	uint16_t loops;
	rc = decode_nsm_diag_set_tid_config_req(msg, reqMsg.size(), &tid,
						&duration, &loops, &log_level,
						&dyn_size, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

/* decode_nsm_diag_set_system_config_req derives the dynamic size from
 * hdr.data_size, which can claim up to 253 dynamic bytes — more than the
 * NSM_DIAG_MAX_DYNAMIC_DATA_SIZE (251) buffer callers provide. A crafted
 * message must be rejected before any copy. */
TEST(DiagPreBootTest, DecodeSetSystemConfigReqOversizeDynamicData)
{
	/* Message long enough that the wire-level "available" check alone
	 * would pass; only the protocol cap catches the oversize claim. */
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_system_config_req) - 1 +
				    UINT8_MAX);
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc =
	    encode_nsm_diag_set_system_config_req(5, 0, 0x01, nullptr, 0, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	auto *request =
	    reinterpret_cast<struct nsm_diag_set_system_config_req *>(
		msg->payload);
	request->hdr.data_size = UINT8_MAX; /* => 253 dynamic bytes */

	uint8_t config_type, duration, dyn_size;
	uint8_t dyn_data[NSM_DIAG_MAX_DYNAMIC_DATA_SIZE] = {};
	rc = decode_nsm_diag_set_system_config_req(
	    msg, reqMsg.size(), &config_type, &duration, &dyn_size, dyn_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

/* Same guard for the TID config decoder: the explicit wire byte can claim
 * up to 255 dynamic bytes, above the NSM_DIAG_MAX_TID_DYNAMIC_DATA_SIZE
 * (244) cap. */
TEST(DiagPreBootTest, DecodeSetTidConfigReqOversizeDynamicData)
{
	std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_diag_set_tid_config_req) - 1 +
				    UINT8_MAX);
	auto *msg = reinterpret_cast<struct nsm_msg *>(reqMsg.data());

	int rc = encode_nsm_diag_set_tid_config_req(5, 0x01, 0x01, 1, 0, 0,
						    nullptr, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	auto *request = reinterpret_cast<struct nsm_diag_set_tid_config_req *>(
	    msg->payload);
	request->dynamic_data_size = UINT8_MAX;

	uint8_t tid, duration, log_level, dyn_size;
	uint16_t loops;
	uint8_t dyn_data[NSM_DIAG_MAX_TID_DYNAMIC_DATA_SIZE] = {};
	rc = decode_nsm_diag_set_tid_config_req(msg, reqMsg.size(), &tid,
						&duration, &loops, &log_level,
						&dyn_size, dyn_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
