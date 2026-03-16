/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "base.h"
#include "diagnostics.h"

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

// Tests for encode_reset_count_256data
TEST(DiagnosticsHelpersTest, EncodeResetCount256dataValid)
{
	uint64_t counter[4] = {0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL,
			       0x1111111111111111ULL, 0x2222222222222222ULL};
	std::vector<uint8_t> data(32);
	size_t data_len = 0;

	int rc = encode_reset_count_256data(counter, data.data(), &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, 32u);
}

TEST(DiagnosticsHelpersTest, EncodeResetCount256dataNullData)
{
	uint64_t counter[4] = {1, 2, 3, 4};
	size_t data_len = 0;

	int rc = encode_reset_count_256data(counter, nullptr, &data_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeResetCount256dataNullDataLen)
{
	uint64_t counter[4] = {1, 2, 3, 4};
	std::vector<uint8_t> data(32);

	int rc = encode_reset_count_256data(counter, data.data(), nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeDecodeResetCount256dataRoundTrip)
{
	uint64_t counter[4] = {0xAAAAAAAAAAAAAAAAULL, 0xBBBBBBBBBBBBBBBBULL,
			       0xCCCCCCCCCCCCCCCCULL, 0xDDDDDDDDDDDDDDDDULL};
	std::vector<uint8_t> data(32);
	size_t data_len = 0;
	uint64_t decoded_counter[4] = {0};

	// Encode
	int rc = encode_reset_count_256data(counter, data.data(), &data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode
	rc = decode_reset_count_256data(data.data(), data_len, decoded_counter,
					4);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify
	for (int i = 0; i < 4; i++) {
		EXPECT_EQ(counter[i], decoded_counter[i]);
	}
}

// Tests for decode_reset_count_256data
TEST(DiagnosticsHelpersTest, DecodeResetCount256dataValid)
{
	std::vector<uint8_t> data(32, 0);
	uint64_t counter[4] = {0};

	int rc = decode_reset_count_256data(data.data(), 32, counter, 4);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsHelpersTest, DecodeResetCount256dataNullData)
{
	uint64_t counter[4] = {0};

	int rc = decode_reset_count_256data(nullptr, 32, counter, 4);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeResetCount256dataNullCounter)
{
	std::vector<uint8_t> data(32, 0);

	int rc = decode_reset_count_256data(data.data(), 32, nullptr, 4);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeResetCount256dataInvalidCounterLen)
{
	std::vector<uint8_t> data(32, 0);
	uint64_t counter[4] = {0};

	int rc = decode_reset_count_256data(data.data(), 32, counter, 3);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeResetCount256dataInvalidDataLen)
{
	std::vector<uint8_t> data(16, 0);
	uint64_t counter[4] = {0};

	int rc = decode_reset_count_256data(data.data(), 16, counter, 4);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_reset_enum_data
TEST(DiagnosticsHelpersTest, EncodeResetEnumDataValid)
{
	uint8_t resetType = 0x05;
	uint8_t data = 0;
	size_t data_len = 0;

	int rc = encode_reset_enum_data(resetType, &data, &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data, 0x05);
	EXPECT_EQ(data_len, sizeof(uint8_t));
}

TEST(DiagnosticsHelpersTest, EncodeResetEnumDataNullData)
{
	uint8_t resetType = 0x05;
	size_t data_len = 0;

	int rc = encode_reset_enum_data(resetType, nullptr, &data_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeResetEnumDataNullDataLen)
{
	uint8_t resetType = 0x05;
	uint8_t data = 0;

	int rc = encode_reset_enum_data(resetType, &data, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeDecodeResetEnumDataRoundTrip)
{
	uint8_t resetType = 0xAB;
	uint8_t data = 0;
	size_t data_len = 0;

	// Encode
	int rc = encode_reset_enum_data(resetType, &data, &data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode
	uint8_t decoded_resetType = 0;
	rc = decode_reset_enum_data(&data, data_len, &decoded_resetType);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_resetType, resetType);
}

// Tests for encode_reset_count_data
TEST(DiagnosticsHelpersTest, EncodeResetCountDataValid)
{
	uint16_t count = 0x1234;
	uint8_t data[2] = {0};
	size_t data_len = 0;

	int rc = encode_reset_count_data(count, data, &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint16_t));
	// Verify little-endian encoding
	EXPECT_EQ(data[0], 0x34);
	EXPECT_EQ(data[1], 0x12);
}

TEST(DiagnosticsHelpersTest, EncodeResetCountDataNullData)
{
	uint16_t count = 0x1234;
	size_t data_len = 0;

	int rc = encode_reset_count_data(count, nullptr, &data_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeResetCountDataNullDataLen)
{
	uint16_t count = 0x1234;
	uint8_t data[2] = {0};

	int rc = encode_reset_count_data(count, data, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeDecodeResetCountDataRoundTrip)
{
	uint16_t count = 0xABCD;
	uint8_t data[2] = {0};
	size_t data_len = 0;

	// Encode
	int rc = encode_reset_count_data(count, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode
	uint16_t decoded_count = 0;
	rc = decode_reset_count_data(data, data_len, &decoded_count);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_count, count);
}

// Tests for decode_reset_enum_data
TEST(DiagnosticsHelpersTest, DecodeResetEnumDataValid)
{
	uint8_t data = 0x05;
	uint8_t resetType = 0;

	int rc = decode_reset_enum_data(&data, sizeof(uint8_t), &resetType);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(resetType, 0x05);
}

TEST(DiagnosticsHelpersTest, DecodeResetEnumDataNullData)
{
	uint8_t resetType = 0;

	int rc = decode_reset_enum_data(nullptr, sizeof(uint8_t), &resetType);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeResetEnumDataNullResetType)
{
	uint8_t data = 0x05;

	int rc = decode_reset_enum_data(&data, sizeof(uint8_t), nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeResetEnumDataInvalidLength)
{
	uint8_t data = 0x05;
	uint8_t resetType = 0;

	int rc = decode_reset_enum_data(&data, 2, &resetType);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for decode_reset_count_data
TEST(DiagnosticsHelpersTest, DecodeResetCountDataValid)
{
	uint8_t data[2] = {0x34, 0x12}; // Little-endian 0x1234
	uint16_t count = 0;

	int rc = decode_reset_count_data(data, sizeof(uint16_t), &count);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(count, 0x1234);
}

TEST(DiagnosticsHelpersTest, DecodeResetCountDataNullData)
{
	uint16_t count = 0;

	int rc = decode_reset_count_data(nullptr, sizeof(uint16_t), &count);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeResetCountDataNullCount)
{
	uint8_t data[2] = {0x34, 0x12};

	int rc = decode_reset_count_data(data, sizeof(uint16_t), nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeResetCountDataInvalidLength)
{
	uint8_t data[2] = {0x34, 0x12};
	uint16_t count = 0;

	int rc = decode_reset_count_data(data, 1, &count);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_get_device_debug_parameters_req
TEST(DiagnosticsHelpersTest, EncodeGetDeviceDebugParametersReqValid)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 5;
	uint8_t debug_config_type = 0x01;
	struct nsm_debug_parameter_id param_id = {};
	param_id.port_number = 0x1234;
	param_id.index = 0x56;
	nsm_debug_parameter_sub_id_bitfield sub_id = {};
	sub_id.value = 0xABCD;

	int rc = encode_get_device_debug_parameters_req(
	    instance_id, debug_config_type, param_id, sub_id, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *req =
	    reinterpret_cast<struct nsm_get_device_debug_parameters_req *>(
		msg->payload);
	EXPECT_EQ(req->debug_configuration_type, debug_config_type);
}

TEST(DiagnosticsHelpersTest, EncodeGetDeviceDebugParametersReqNullMsg)
{
	uint8_t instance_id = 5;
	uint8_t debug_config_type = 0x01;
	struct nsm_debug_parameter_id param_id = {};
	nsm_debug_parameter_sub_id_bitfield sub_id = {};

	int rc = encode_get_device_debug_parameters_req(
	    instance_id, debug_config_type, param_id, sub_id, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeDecodeGetDeviceDebugParametersReqRoundTrip)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 7;
	uint8_t debug_config_type = 0x02;
	struct nsm_debug_parameter_id param_id = {};
	param_id.port_number = 0x9876;
	param_id.index = 0xAB;
	nsm_debug_parameter_sub_id_bitfield sub_id = {};
	sub_id.value = 0x12345678;

	// Encode
	int rc = encode_get_device_debug_parameters_req(
	    instance_id, debug_config_type, param_id, sub_id, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode
	uint8_t decoded_config_type = 0;
	struct nsm_debug_parameter_id decoded_param_id = {};
	nsm_debug_parameter_sub_id_bitfield decoded_sub_id = {};
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_req);

	rc = decode_get_device_debug_parameters_req(
	    msg, msg_len, &decoded_config_type, &decoded_param_id,
	    &decoded_sub_id);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_config_type, debug_config_type);
	EXPECT_EQ(decoded_param_id.port_number, param_id.port_number);
	EXPECT_EQ(decoded_param_id.index, param_id.index);
	EXPECT_EQ(decoded_sub_id.value, sub_id.value);
}

// Tests for decode_get_device_debug_parameters_req
TEST(DiagnosticsHelpersTest, DecodeGetDeviceDebugParametersReqNullMsg)
{
	uint8_t debug_config_type = 0;
	struct nsm_debug_parameter_id param_id = {};
	nsm_debug_parameter_sub_id_bitfield sub_id = {};
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_req);

	int rc = decode_get_device_debug_parameters_req(
	    nullptr, msg_len, &debug_config_type, &param_id, &sub_id);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeGetDeviceDebugParametersReqNullOutputs)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_req);

	int rc = decode_get_device_debug_parameters_req(msg, msg_len, nullptr,
							nullptr, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for encode_get_device_debug_parameters_resp
TEST(DiagnosticsHelpersTest, EncodeGetDeviceDebugParametersRespValidSuccess)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_resp) +
	    16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 3;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x0000;
	uint16_t data_size = 16;
	std::vector<uint8_t> data(16, 0xAA);

	int rc = encode_get_device_debug_parameters_resp(
	    instance_id, cc, reason_code, &data_size, data.data(), msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp =
	    reinterpret_cast<struct nsm_get_device_debug_parameters_resp *>(
		msg->payload);
	EXPECT_EQ(resp->hdr.command, NSM_GET_DEVICE_DEBUG_PARAMETERS);
	EXPECT_EQ(resp->hdr.completion_code, cc);
}

TEST(DiagnosticsHelpersTest, EncodeGetDeviceDebugParametersRespNullParams)
{
	uint8_t instance_id = 3;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x0000;

	int rc = encode_get_device_debug_parameters_resp(
	    instance_id, cc, reason_code, nullptr, nullptr, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeGetDeviceDebugParametersRespErrorCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 3;
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0x1234;
	uint16_t data_size = 16;
	std::vector<uint8_t> data(16, 0xAA);

	// When cc != NSM_SUCCESS, function encodes reason_code only
	int rc = encode_get_device_debug_parameters_resp(
	    instance_id, cc, reason_code, &data_size, data.data(), msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp = reinterpret_cast<struct nsm_common_resp *>(msg->payload);
	EXPECT_EQ(resp->completion_code, cc);
}

// Tests for decode_get_device_debug_parameters_resp
TEST(DiagnosticsHelpersTest, DecodeGetDeviceDebugParametersRespValid)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_resp) +
	    16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 3;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x0000;
	uint16_t data_size = 16;
	std::vector<uint8_t> data(16, 0xBB);

	encode_get_device_debug_parameters_resp(instance_id, cc, reason_code,
						&data_size, data.data(), msg);

	uint8_t decoded_cc = 0;
	uint16_t decoded_data_size = 0;
	std::vector<uint8_t> decoded_data(16);
	size_t msg_len = sizeof(nsm_msg_hdr) +
			 sizeof(nsm_get_device_debug_parameters_resp) + 16;

	int rc = decode_get_device_debug_parameters_resp(
	    msg, msg_len, &decoded_cc, &decoded_data_size, decoded_data.data());

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_cc, cc);
	EXPECT_EQ(decoded_data_size, data_size);
}

TEST(DiagnosticsHelpersTest, DecodeGetDeviceDebugParametersRespNullMsg)
{
	uint8_t cc = 0;
	uint16_t data_size = 0;
	std::vector<uint8_t> data(16);
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_resp);

	int rc = decode_get_device_debug_parameters_resp(
	    nullptr, msg_len, &cc, &data_size, data.data());

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeGetDeviceDebugParametersRespNullOutputs)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_resp) +
	    16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_resp);

	int rc = decode_get_device_debug_parameters_resp(msg, msg_len, nullptr,
							 nullptr, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeGetDeviceDebugParametersRespInvalidLength)
{
	std::vector<uint8_t> responseMsg(10);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t data_size = 0;
	std::vector<uint8_t> data(16);

	int rc = decode_get_device_debug_parameters_resp(
	    msg, 10, &cc, &data_size, data.data());

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_set_device_debug_parameters_req
TEST(DiagnosticsHelpersTest, EncodeSetDeviceDebugParametersReqValid)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_debug_parameters_req) +
	    16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 9;
	uint8_t debug_config_type = 0x03;
	struct nsm_debug_parameter_id param_id = {};
	param_id.port_number = 0x5678;
	param_id.index = 0xCD;
	nsm_debug_parameter_sub_id_bitfield sub_id = {};
	sub_id.value = 0x87654321;
	uint8_t data_size = 16;
	std::vector<uint8_t> data(16, 0xCC);

	int rc = encode_set_device_debug_parameters_req(
	    instance_id, debug_config_type, param_id, sub_id, data_size,
	    data.data(), msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *req =
	    reinterpret_cast<struct nsm_set_device_debug_parameters_req *>(
		msg->payload);
	EXPECT_EQ(req->debug_configuration_type, debug_config_type);
	EXPECT_EQ(req->data_size, data_size);
}

TEST(DiagnosticsHelpersTest, EncodeSetDeviceDebugParametersReqNullMsg)
{
	uint8_t instance_id = 9;
	uint8_t debug_config_type = 0x03;
	struct nsm_debug_parameter_id param_id = {};
	nsm_debug_parameter_sub_id_bitfield sub_id = {};
	uint8_t data_size = 16;
	std::vector<uint8_t> data(16, 0xCC);

	int rc = encode_set_device_debug_parameters_req(
	    instance_id, debug_config_type, param_id, sub_id, data_size,
	    data.data(), nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeSetDeviceDebugParametersReqNullData)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_debug_parameters_req) +
	    16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 9;
	uint8_t debug_config_type = 0x03;
	struct nsm_debug_parameter_id param_id = {};
	nsm_debug_parameter_sub_id_bitfield sub_id = {};
	uint8_t data_size = 16;

	int rc = encode_set_device_debug_parameters_req(
	    instance_id, debug_config_type, param_id, sub_id, data_size,
	    nullptr, msg);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for decode_set_device_debug_parameters_req
TEST(DiagnosticsHelpersTest, DISABLED_DecodeSetDeviceDebugParametersReqValid)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_debug_parameters_req) +
	    16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 11;
	uint8_t debug_config_type = 0x04;
	struct nsm_debug_parameter_id param_id = {};
	param_id.port_number = 0xABCD;
	param_id.index = 0xEF;
	nsm_debug_parameter_sub_id_bitfield sub_id = {};
	sub_id.value = 0xDEADBEEF;
	uint8_t data_size = 16;
	std::vector<uint8_t> data(16, 0xDD);

	encode_set_device_debug_parameters_req(instance_id, debug_config_type,
					       param_id, sub_id, data_size,
					       data.data(), msg);

	uint8_t decoded_config_type = 0;
	struct nsm_debug_parameter_id decoded_param_id = {};
	nsm_debug_parameter_sub_id_bitfield decoded_sub_id = {};
	uint8_t decoded_data_size = 0;
	std::vector<uint8_t> decoded_data(16);
	uint8_t *decoded_data_ptr = decoded_data.data();
	size_t msg_len = sizeof(nsm_msg_hdr) +
			 sizeof(nsm_set_device_debug_parameters_req) + 16;

	int rc = decode_set_device_debug_parameters_req(
	    msg, msg_len, &decoded_config_type, &decoded_param_id,
	    &decoded_sub_id, &decoded_data_size, &decoded_data_ptr);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_config_type, debug_config_type);
	EXPECT_EQ(decoded_param_id.port_number, param_id.port_number);
	EXPECT_EQ(decoded_param_id.index, param_id.index);
	EXPECT_EQ(decoded_sub_id.value, sub_id.value);
	EXPECT_EQ(decoded_data_size, data_size);
}

TEST(DiagnosticsHelpersTest, DecodeSetDeviceDebugParametersReqNullMsg)
{
	uint8_t debug_config_type = 0;
	struct nsm_debug_parameter_id param_id = {};
	nsm_debug_parameter_sub_id_bitfield sub_id = {};
	uint8_t data_size = 0;
	std::vector<uint8_t> data(16);
	uint8_t *data_ptr = data.data();
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_debug_parameters_req);

	int rc = decode_set_device_debug_parameters_req(
	    nullptr, msg_len, &debug_config_type, &param_id, &sub_id,
	    &data_size, &data_ptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeSetDeviceDebugParametersReqNullOutputs)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_debug_parameters_req) +
	    16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_debug_parameters_req);

	int rc = decode_set_device_debug_parameters_req(
	    msg, msg_len, nullptr, nullptr, nullptr, nullptr, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for decode_set_device_debug_parameters_resp
TEST(DiagnosticsHelpersTest, DecodeSetDeviceDebugParametersRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	auto *resp = reinterpret_cast<struct nsm_common_resp *>(msg->payload);
	resp->command = NSM_SET_DEVICE_DEBUG_PARAMETERS;
	resp->completion_code = NSM_SUCCESS;

	uint8_t cc = 0;
	size_t msg_len = sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp);

	int rc = decode_set_device_debug_parameters_resp(msg, msg_len, &cc);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(DiagnosticsHelpersTest, DecodeSetDeviceDebugParametersRespNullMsg)
{
	uint8_t cc = 0;
	size_t msg_len = sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp);

	int rc = decode_set_device_debug_parameters_resp(nullptr, msg_len, &cc);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeSetDeviceDebugParametersRespNullCc)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	size_t msg_len = sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp);

	int rc = decode_set_device_debug_parameters_resp(msg, msg_len, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeSetDeviceDebugParametersRespInvalidLength)
{
	std::vector<uint8_t> responseMsg(10);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;

	int rc = decode_set_device_debug_parameters_resp(msg, 5, &cc);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_get_network_device_debug_info_resp and
// decode_get_network_device_debug_info_resp
TEST(DiagnosticsHelpersTest, EncodeGetNetworkDeviceDebugInfoRespValid)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_resp) + 16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x15;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	std::vector<uint8_t> seg_data = {0x01, 0x02, 0x03, 0x04, 0x05};
	uint16_t seg_data_size = seg_data.size();
	uint32_t next_handle = 0x12345678;

	int rc = encode_get_network_device_debug_info_resp(
	    instance_id, cc, reason_code, seg_data.data(), seg_data_size,
	    next_handle, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsHelpersTest, EncodeGetNetworkDeviceDebugInfoRespNullMsg)
{
	uint8_t instance_id = 0x15;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	std::vector<uint8_t> seg_data = {0x01, 0x02, 0x03};
	uint16_t seg_data_size = seg_data.size();
	uint32_t next_handle = 0x12345678;

	int rc = encode_get_network_device_debug_info_resp(
	    instance_id, cc, reason_code, seg_data.data(), seg_data_size,
	    next_handle, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeGetNetworkDeviceDebugInfoRespNullSegData)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_resp) + 16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x15;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint16_t seg_data_size = 5;
	uint32_t next_handle = 0x12345678;

	int rc = encode_get_network_device_debug_info_resp(
	    instance_id, cc, reason_code, nullptr, seg_data_size, next_handle,
	    msg);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeGetNetworkDeviceDebugInfoRespErrorCc)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_resp) + 16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x15;
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0x1234;
	std::vector<uint8_t> seg_data = {0x01, 0x02, 0x03};
	uint16_t seg_data_size = seg_data.size();
	uint32_t next_handle = 0x12345678;

	int rc = encode_get_network_device_debug_info_resp(
	    instance_id, cc, reason_code, seg_data.data(), seg_data_size,
	    next_handle, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsHelpersTest, DecodeGetNetworkDeviceDebugInfoRespValid)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_resp) + 16);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x15;
	uint8_t cc_in = NSM_SUCCESS;
	uint16_t reason_code_in = 0;
	std::vector<uint8_t> seg_data_in = {0xAA, 0xBB, 0xCC, 0xDD};
	uint16_t seg_data_size_in = seg_data_in.size();
	uint32_t next_handle_in = 0x87654321;

	int rc = encode_get_network_device_debug_info_resp(
	    instance_id, cc_in, reason_code_in, seg_data_in.data(),
	    seg_data_size_in, next_handle_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc_out = 0;
	uint16_t reason_code_out = 0;
	uint16_t seg_data_size_out = 0;
	std::vector<uint8_t> seg_data_out(16);
	uint32_t next_handle_out = 0;

	size_t msg_len = sizeof(nsm_msg_hdr) +
			 sizeof(nsm_get_network_device_debug_info_resp) +
			 seg_data_size_in - 1;

	rc = decode_get_network_device_debug_info_resp(
	    msg, msg_len, &cc_out, &reason_code_out, &seg_data_size_out,
	    seg_data_out.data(), &next_handle_out);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc_out, cc_in);
	EXPECT_EQ(seg_data_size_out, seg_data_size_in);
	EXPECT_EQ(next_handle_out, next_handle_in);
	for (int i = 0; i < seg_data_size_in; i++) {
		EXPECT_EQ(seg_data_out[i], seg_data_in[i]);
	}
}

TEST(DiagnosticsHelpersTest, DecodeGetNetworkDeviceDebugInfoRespNullSegData)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t seg_data_size = 0;
	uint32_t next_handle = 0;
	size_t msg_len = responseMsg.size();

	int rc = decode_get_network_device_debug_info_resp(
	    msg, msg_len, &cc, &reason_code, &seg_data_size, nullptr,
	    &next_handle);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeGetNetworkDeviceDebugInfoRespNullSegDataSize)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	std::vector<uint8_t> seg_data(16);
	uint32_t next_handle = 0;
	size_t msg_len = responseMsg.size();

	int rc = decode_get_network_device_debug_info_resp(
	    msg, msg_len, &cc, &reason_code, nullptr, seg_data.data(),
	    &next_handle);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeGetNetworkDeviceDebugInfoRespNullNextHandle)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t seg_data_size = 0;
	std::vector<uint8_t> seg_data(16);
	size_t msg_len = responseMsg.size();

	int rc = decode_get_network_device_debug_info_resp(
	    msg, msg_len, &cc, &reason_code, &seg_data_size, seg_data.data(),
	    nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeGetNetworkDeviceDebugInfoRespInvalidLength)
{
	std::vector<uint8_t> responseMsg(10);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t seg_data_size = 0;
	std::vector<uint8_t> seg_data(16);
	uint32_t next_handle = 0;

	int rc = decode_get_network_device_debug_info_resp(
	    msg, 5, &cc, &reason_code, &seg_data_size, seg_data.data(),
	    &next_handle);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_erase_trace_req and decode_erase_trace_req
TEST(DiagnosticsHelpersTest, EncodeEraseTraceReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_trace_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 0x1A;

	int rc = encode_erase_trace_req(instance_id, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsHelpersTest, EncodeEraseTraceReqNullMsg)
{
	uint8_t instance_id = 0x1A;

	int rc = encode_erase_trace_req(instance_id, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeEraseTraceReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_trace_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 0x1A;

	int rc = encode_erase_trace_req(instance_id, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_erase_trace_req(msg, requestMsg.size());

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsHelpersTest, DecodeEraseTraceReqNullMsg)
{
	int rc = decode_erase_trace_req(nullptr, 100);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeEraseTraceReqInvalidLength)
{
	std::vector<uint8_t> requestMsg(10);
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());

	int rc = decode_erase_trace_req(msg, 5);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DiagnosticsHelpersTest, DecodeEraseTraceReqInvalidDataSize)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_trace_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	auto *request =
	    reinterpret_cast<struct nsm_erase_trace_req *>(msg->payload);

	// Set invalid data_size (should be 0)
	request->hdr.data_size = 0x10;

	int rc = decode_erase_trace_req(msg, requestMsg.size());

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// Tests for encode_erase_trace_resp and decode_erase_trace_resp
TEST(DiagnosticsHelpersTest, EncodeEraseTraceRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_trace_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x1A;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint8_t result_status = 0x01;

	int rc = encode_erase_trace_resp(instance_id, cc, reason_code,
					 result_status, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsHelpersTest, EncodeEraseTraceRespNullMsg)
{
	uint8_t instance_id = 0x1A;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint8_t result_status = 0x01;

	int rc = encode_erase_trace_resp(instance_id, cc, reason_code,
					 result_status, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeEraseTraceRespErrorCc)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_trace_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x1A;
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0x5678;
	uint8_t result_status = 0x01;

	int rc = encode_erase_trace_resp(instance_id, cc, reason_code,
					 result_status, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsHelpersTest, DecodeEraseTraceRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_trace_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x1A;
	uint8_t cc_in = NSM_SUCCESS;
	uint16_t reason_code_in = 0;
	uint8_t result_status_in = 0xAB;

	int rc = encode_erase_trace_resp(instance_id, cc_in, reason_code_in,
					 result_status_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc_out = 0;
	uint16_t reason_code_out = 0;
	uint8_t result_status_out = 0;

	rc = decode_erase_trace_resp(msg, responseMsg.size(), &cc_out,
				     &reason_code_out, &result_status_out);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc_out, cc_in);
	EXPECT_EQ(result_status_out, result_status_in);
}

TEST(DiagnosticsHelpersTest, DecodeEraseTraceRespNullResultStatus)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_trace_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;

	int rc = decode_erase_trace_resp(msg, responseMsg.size(), &cc,
					 &reason_code, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeEraseTraceRespInvalidLength)
{
	std::vector<uint8_t> responseMsg(10);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t result_status = 0;

	int rc =
	    decode_erase_trace_resp(msg, 5, &cc, &reason_code, &result_status);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_erase_debug_info_req and decode_erase_debug_info_req
TEST(DiagnosticsHelpersTest, EncodeEraseDebugInfoReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_debug_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 0x1D;
	uint8_t info_type = 0x03;

	int rc = encode_erase_debug_info_req(instance_id, info_type, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsHelpersTest, EncodeEraseDebugInfoReqNullMsg)
{
	uint8_t instance_id = 0x1D;
	uint8_t info_type = 0x03;

	int rc = encode_erase_debug_info_req(instance_id, info_type, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeEraseDebugInfoReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_debug_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 0x1D;
	uint8_t info_type_in = 0x05;

	int rc = encode_erase_debug_info_req(instance_id, info_type_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t info_type_out = 0;
	rc =
	    decode_erase_debug_info_req(msg, requestMsg.size(), &info_type_out);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(info_type_out, info_type_in);
}

TEST(DiagnosticsHelpersTest, DecodeEraseDebugInfoReqNullMsg)
{
	uint8_t info_type = 0;

	int rc = decode_erase_debug_info_req(nullptr, 100, &info_type);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeEraseDebugInfoReqNullInfoType)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_debug_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());

	int rc = decode_erase_debug_info_req(msg, requestMsg.size(), nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeEraseDebugInfoReqInvalidLength)
{
	std::vector<uint8_t> requestMsg(10);
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t info_type = 0;

	int rc = decode_erase_debug_info_req(msg, 5, &info_type);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_erase_debug_info_resp and decode_erase_debug_info_resp
TEST(DiagnosticsHelpersTest, EncodeEraseDebugInfoRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_debug_info_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x1E;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint8_t result_status = 0x02;

	int rc = encode_erase_debug_info_resp(instance_id, cc, reason_code,
					      result_status, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsHelpersTest, EncodeEraseDebugInfoRespNullMsg)
{
	uint8_t instance_id = 0x1E;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint8_t result_status = 0x02;

	int rc = encode_erase_debug_info_resp(instance_id, cc, reason_code,
					      result_status, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, EncodeEraseDebugInfoRespErrorCc)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_debug_info_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x1E;
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0xABCD;
	uint8_t result_status = 0x02;

	int rc = encode_erase_debug_info_resp(instance_id, cc, reason_code,
					      result_status, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsHelpersTest, DecodeEraseDebugInfoRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_debug_info_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x1E;
	uint8_t cc_in = NSM_SUCCESS;
	uint16_t reason_code_in = 0;
	uint8_t result_status_in = 0xCD;

	int rc = encode_erase_debug_info_resp(
	    instance_id, cc_in, reason_code_in, result_status_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc_out = 0;
	uint16_t reason_code_out = 0;
	uint8_t result_status_out = 0;

	rc = decode_erase_debug_info_resp(msg, responseMsg.size(), &cc_out,
					  &reason_code_out, &result_status_out);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc_out, cc_in);
	EXPECT_EQ(result_status_out, result_status_in);
}

TEST(DiagnosticsHelpersTest, DecodeEraseDebugInfoRespNullResultStatus)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_debug_info_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;

	int rc = decode_erase_debug_info_resp(msg, responseMsg.size(), &cc,
					      &reason_code, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsHelpersTest, DecodeEraseDebugInfoRespInvalidLength)
{
	std::vector<uint8_t> responseMsg(10);
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t result_status = 0;

	int rc = decode_erase_debug_info_resp(msg, 5, &cc, &reason_code,
					      &result_status);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
