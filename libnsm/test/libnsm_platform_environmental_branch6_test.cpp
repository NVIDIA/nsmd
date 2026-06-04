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
 * Branch coverage batch 6 for platform-environmental.c
 *
 * Targets: NULL pointer and length validation branches across
 * encode/decode functions not yet exercised by branch tests 1-5.
 */

#include "base.h"
#include "platform-environmental.h"
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t kBadIid = 32; // > NSM_INSTANCE_MAX(31)

// ===========================================================================
// encode_get_inventory_information_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetInventoryInfoReq_NullMsg)
{
	auto rc = encode_get_inventory_information_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_inventory_information_req: pack fail (iid=32)
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetInventoryInfoReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_inventory_information_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_inventory_information_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetInventoryInfoReq_NullMsg)
{
	uint8_t prop = 0;
	auto rc = decode_get_inventory_information_req(nullptr, 100, &prop);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_inventory_information_req: NULL property_identifier
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetInventoryInfoReq_NullProp)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_get_inventory_information_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_inventory_information_req: msg too short
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetInventoryInfoReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t prop = 0;
	auto rc = decode_get_inventory_information_req(msg, buf.size(), &prop);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_get_inventory_information_req: data_size too small
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetInventoryInfoReq_DataSizeTooSmall)
{
	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(struct nsm_get_inventory_information_req);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t prop = 0;
	auto rc = decode_get_inventory_information_req(msg, msgLen, &prop);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_inventory_information_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetInventoryInfoResp_NullMsg)
{
	uint8_t data[4] = {0};
	auto rc = encode_get_inventory_information_resp(0, NSM_SUCCESS, 0, 4,
							data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_inventory_information_resp: NULL inventory_information
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetInventoryInfoResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_inventory_information_resp(0, NSM_SUCCESS, 0, 4,
							nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_inventory_information_resp: NULL data_size
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetInventoryInfoResp_NullDataSize)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t inv[64] = {};
	auto rc = decode_get_inventory_information_resp(
	    msg, buf.size(), nullptr, nullptr, nullptr, inv);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_inventory_information_resp: NULL inventory_information
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetInventoryInfoResp_NullInvInfo)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t ds = 0;
	auto rc = decode_get_inventory_information_resp(
	    msg, buf.size(), nullptr, nullptr, &ds, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_platform_env_command_no_payload_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeNoPaylReq_NullMsg)
{
	auto rc = encode_get_platform_env_command_no_payload_req(
	    0, nullptr, NSM_GET_MIG_MODE);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_platform_env_command_no_payload_req: pack fail
// ===========================================================================
TEST(PlatEnvBranch6, EncodeNoPaylReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_platform_env_command_no_payload_req(
	    kBadIid, msg, NSM_GET_MIG_MODE);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_platform_env_command_no_payload_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, DecodeNoPaylReq_NullMsg)
{
	auto rc = decode_get_platform_env_command_no_payload_req(
	    nullptr, 100, NSM_GET_MIG_MODE);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_platform_env_command_no_payload_req: msg too short
// ===========================================================================
TEST(PlatEnvBranch6, DecodeNoPaylReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_platform_env_command_no_payload_req(
	    msg, buf.size(), NSM_GET_MIG_MODE);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_temperature_reading_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetTempReadingReq_NullMsg)
{
	auto rc = encode_get_temperature_reading_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_temperature_reading_req: pack fail
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetTempReadingReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_temperature_reading_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_get_temperature_reading_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetTempReadingResp_NullMsg)
{
	auto rc = encode_get_temperature_reading_resp(0, NSM_SUCCESS, 0, 0.0,
						      nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_temperature_reading_resp: NULL temperature_reading
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetTempReadingResp_NullTemp)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_temperature_reading_resp(msg, buf.size(), &cc,
						      &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_read_thermal_parameter_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeReadThermalParamReq_NullMsg)
{
	auto rc = encode_read_thermal_parameter_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_read_thermal_parameter_req: pack fail
// ===========================================================================
TEST(PlatEnvBranch6, EncodeReadThermalParamReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_read_thermal_parameter_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_read_thermal_parameter_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeReadThermalParamResp_NullMsg)
{
	auto rc =
	    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_read_thermal_parameter_resp: NULL threshold
// ===========================================================================
TEST(PlatEnvBranch6, DecodeReadThermalParamResp_NullThreshold)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_read_thermal_parameter_resp(msg, buf.size(), &cc,
						     &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_aggregate_thermal_parameter_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateThermalParam_NullData)
{
	size_t len = 0;
	auto rc = encode_aggregate_thermal_parameter_data(0, nullptr, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_aggregate_thermal_parameter_data: NULL data_len
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateThermalParam_NullLen)
{
	uint8_t data[8] = {};
	auto rc = encode_aggregate_thermal_parameter_data(0, data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_thermal_parameter_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateThermalParam_NullData)
{
	int32_t threshold = 0;
	auto rc =
	    decode_aggregate_thermal_parameter_data(nullptr, 4, &threshold);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_thermal_parameter_data: NULL output
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateThermalParam_NullOutput)
{
	uint8_t data[4] = {};
	auto rc = decode_aggregate_thermal_parameter_data(data, 4, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_thermal_parameter_data: wrong data_len
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateThermalParam_WrongLen)
{
	uint8_t data[8] = {};
	int32_t threshold = 0;
	auto rc = decode_aggregate_thermal_parameter_data(data, 3, &threshold);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_current_power_draw_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetCurrPowerDrawReq_NullMsg)
{
	auto rc = encode_get_current_power_draw_req(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_current_power_draw_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetCurrPowerDrawResp_NullMsg)
{
	auto rc =
	    encode_get_current_power_draw_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_current_power_draw_resp: NULL reading
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetCurrPowerDrawResp_NullReading)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_current_power_draw_resp(msg, buf.size(), &cc,
						     &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_aggregate_get_current_power_draw_reading: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregatePowerReading_NullData)
{
	size_t len = 0;
	auto rc =
	    encode_aggregate_get_current_power_draw_reading(0, nullptr, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_get_current_power_draw_reading: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregatePowerReading_NullData)
{
	uint32_t reading = 0;
	auto rc = decode_aggregate_get_current_power_draw_reading(nullptr, 4,
								  &reading);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_get_current_power_draw_reading: wrong data_len
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregatePowerReading_WrongLen)
{
	uint8_t data[8] = {};
	uint32_t reading = 0;
	auto rc =
	    decode_aggregate_get_current_power_draw_reading(data, 3, &reading);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_max_observed_power_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetMaxObsPowerReq_NullMsg)
{
	auto rc = encode_get_max_observed_power_req(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_max_observed_power_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetMaxObsPowerResp_NullMsg)
{
	auto rc =
	    encode_get_max_observed_power_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_max_observed_power_resp: NULL reading
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetMaxObsPowerResp_NullReading)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_max_observed_power_resp(msg, buf.size(), &cc,
						     &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_current_energy_count_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetCurrEnergyCountReq_NullMsg)
{
	auto rc = encode_get_current_energy_count_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_current_energy_count_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetCurrEnergyCountResp_NullMsg)
{
	auto rc =
	    encode_get_current_energy_count_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_current_energy_count_resp: NULL energy_reading
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetCurrEnergyCountResp_NullReading)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_current_energy_count_resp(msg, buf.size(), &cc,
						       &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_voltage_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetVoltageReq_NullMsg)
{
	auto rc = encode_get_voltage_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_voltage_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetVoltageResp_NullMsg)
{
	auto rc = encode_get_voltage_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_voltage_resp: NULL voltage
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetVoltageResp_NullVoltage)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_get_voltage_resp(msg, buf.size(), &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_altitude_pressure_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetAltPressureReq_NullMsg)
{
	auto rc = encode_get_altitude_pressure_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_altitude_pressure_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetAltPressureResp_NullMsg)
{
	auto rc =
	    encode_get_altitude_pressure_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_altitude_pressure_resp: NULL reading
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetAltPressureResp_NullReading)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_altitude_pressure_resp(msg, buf.size(), &cc,
						    &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_aggregate_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateResp_NullMsg)
{
	auto rc = encode_aggregate_resp(0, 0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateResp_NullMsg)
{
	size_t consumed = 0;
	uint8_t cc = 0;
	uint16_t tc = 0;
	auto rc = decode_aggregate_resp(nullptr, 100, &consumed, &cc, &tc);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_resp: NULL cc
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	size_t consumed = 0;
	uint16_t tc = 0;
	auto rc =
	    decode_aggregate_resp(msg, buf.size(), &consumed, nullptr, &tc);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_resp: NULL telemetry_count
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateResp_NullCount)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	size_t consumed = 0;
	uint8_t cc = 0;
	auto rc =
	    decode_aggregate_resp(msg, buf.size(), &consumed, &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_resp: msg too short
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateResp_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	size_t consumed = 0;
	uint8_t cc = 0;
	uint16_t tc = 0;
	auto rc = decode_aggregate_resp(msg, buf.size(), &consumed, &cc, &tc);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_aggregate_resp_sample: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateRespSample_NullData)
{
	struct nsm_aggregate_resp_sample sample = {};
	size_t sample_len = 0;
	auto rc = encode_aggregate_resp_sample(0, true, nullptr, 4, &sample,
					       &sample_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_aggregate_resp_sample: NULL sample
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateRespSample_NullSample)
{
	uint8_t data[4] = {};
	size_t sample_len = 0;
	auto rc = encode_aggregate_resp_sample(0, true, data, 4, nullptr,
					       &sample_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_aggregate_resp_sample: NULL sample_len
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateRespSample_NullSampleLen)
{
	uint8_t data[4] = {};
	struct nsm_aggregate_resp_sample sample = {};
	auto rc =
	    encode_aggregate_resp_sample(0, true, data, 4, &sample, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_aggregate_resp_sample: len_encoding=1 stores data_len directly (byte
// length encoding)
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateRespSample_ByteLengthEncoding)
{
	uint8_t data[5] = {1, 2, 3, 4, 5};
	uint8_t sampleBuf[sizeof(struct nsm_aggregate_resp_sample) + 5] = {};
	auto sample =
	    reinterpret_cast<struct nsm_aggregate_resp_sample *>(sampleBuf);
	size_t sample_len = 0;
	auto rc = encode_aggregate_resp_sample(0, true, data, 5, sample,
					       &sample_len, 1);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(sample->len_encoding, 1);
	EXPECT_EQ(sample->length, 5);
	EXPECT_EQ(sample_len,
		  5u + sizeof(struct nsm_aggregate_resp_sample) - 1);
}

// encode_aggregate_resp_sample: data_len that does not fit the 3-bit length
// field of byte length encoding is invalid
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateRespSample_BadDataLen)
{
	uint8_t data[9] = {};
	struct nsm_aggregate_resp_sample sample = {};
	size_t sample_len = 0;
	auto rc = encode_aggregate_resp_sample(0, true, data, 9, &sample,
					       &sample_len, 1);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_aggregate_resp_sample: NULL sample
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateRespSample_NullSample)
{
	size_t consumed = 0;
	uint8_t tag = 0;
	bool valid = false;
	const uint8_t *data = nullptr;
	size_t data_len = 0;
	auto rc = decode_aggregate_resp_sample(nullptr, 100, &consumed, &tag,
					       &valid, &data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_resp_sample: NULL tag
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateRespSample_NullTag)
{
	struct nsm_aggregate_resp_sample sample = {};
	size_t consumed = 0;
	bool valid = false;
	const uint8_t *data = nullptr;
	size_t data_len = 0;
	auto rc = decode_aggregate_resp_sample(&sample, 100, &consumed, nullptr,
					       &valid, &data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_resp_sample: msg too short
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateRespSample_MsgTooShort)
{
	struct nsm_aggregate_resp_sample sample = {};
	size_t consumed = 0;
	uint8_t tag = 0;
	bool valid = false;
	const uint8_t *data = nullptr;
	size_t data_len = 0;
	auto rc = decode_aggregate_resp_sample(&sample, 0, &consumed, &tag,
					       &valid, &data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_aggregate_temperature_reading_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateTempReading_NullData)
{
	size_t len = 0;
	auto rc = encode_aggregate_temperature_reading_data(0.0, nullptr, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_temperature_reading_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateTempReading_NullData)
{
	double temp = 0.0;
	auto rc = decode_aggregate_temperature_reading_data(nullptr, 4, &temp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_temperature_reading_data: NULL output
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateTempReading_NullOutput)
{
	uint8_t data[4] = {};
	auto rc = decode_aggregate_temperature_reading_data(data, 4, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_temperature_reading_data: wrong data_len
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateTempReading_WrongLen)
{
	uint8_t data[8] = {};
	double temp = 0.0;
	auto rc = decode_aggregate_temperature_reading_data(data, 3, &temp);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_aggregate_energy_count_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateEnergyCount_NullData)
{
	size_t len = 0;
	auto rc = encode_aggregate_energy_count_data(0, nullptr, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_energy_count_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateEnergyCount_NullData)
{
	uint64_t ec = 0;
	auto rc = decode_aggregate_energy_count_data(nullptr, 8, &ec);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_energy_count_data: NULL output
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateEnergyCount_NullOutput)
{
	uint8_t data[8] = {};
	auto rc = decode_aggregate_energy_count_data(data, 8, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_energy_count_data: wrong data_len
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateEnergyCount_WrongLen)
{
	uint8_t data[8] = {};
	uint64_t ec = 0;
	auto rc = decode_aggregate_energy_count_data(data, 4, &ec);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_aggregate_voltage_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggregateVoltage_NullData)
{
	size_t len = 0;
	auto rc = encode_aggregate_voltage_data(0, nullptr, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_voltage_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateVoltage_NullData)
{
	uint32_t voltage = 0;
	auto rc = decode_aggregate_voltage_data(nullptr, 4, &voltage);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_voltage_data: wrong data_len
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggregateVoltage_WrongLen)
{
	uint8_t data[8] = {};
	uint32_t voltage = 0;
	auto rc = decode_aggregate_voltage_data(data, 3, &voltage);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_MIG_mode_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetMigModeReq_NullMsg)
{
	auto rc = encode_get_MIG_mode_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_MIG_mode_req: pack fail
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetMigModeReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_MIG_mode_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_get_MIG_mode_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetMigModeResp_NullMsg)
{
	bitfield8_t flags = {0};
	auto rc = encode_get_MIG_mode_resp(0, NSM_SUCCESS, 0, &flags, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_MIG_mode_resp: NULL flags
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetMigModeResp_NullFlags)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_MIG_mode_resp(0, NSM_SUCCESS, 0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_MIG_mode_resp: NULL cc
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetMigModeResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t ds = 0, reason = 0;
	bitfield8_t flags = {0};
	auto rc = decode_get_MIG_mode_resp(msg, buf.size(), nullptr, &ds,
					   &reason, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_MIG_mode_resp: msg_len mismatch (cc=0=success)
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetMigModeResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	bitfield8_t flags = {0};
	auto rc = decode_get_MIG_mode_resp(msg, buf.size(), &cc, &ds, &reason,
					   &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_violation_duration_req: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetViolationDurationReq_NullMsg)
{
	auto rc = encode_get_violation_duration_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_violation_duration_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetViolationDurationResp_NullMsg)
{
	struct nsm_violation_duration data = {};
	auto rc = encode_get_violation_duration_resp(0, NSM_SUCCESS, 0, &data,
						     nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_violation_duration_resp: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetViolationDurationResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc =
	    encode_get_violation_duration_resp(0, NSM_SUCCESS, 0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_violation_duration_resp: NULL data_size
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetViolationDurationResp_NullDataSize)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_violation_duration data = {};
	auto rc = decode_get_violation_duration_resp(msg, buf.size(), &cc,
						     nullptr, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_violation_duration_resp: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetViolationDurationResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	auto rc = decode_get_violation_duration_resp(msg, buf.size(), &cc, &ds,
						     &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_violation_duration_resp: msg_len mismatch (cc=0=success)
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetViolationDurationResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	struct nsm_violation_duration data = {};
	auto rc = decode_get_violation_duration_resp(msg, buf.size(), &cc, &ds,
						     &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_powersmoothing_featinfo_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetPwrSmoothFeatInfoResp_NullMsg)
{
	struct nsm_pwr_smoothing_featureinfo_data data = {};
	auto rc = encode_get_powersmoothing_featinfo_resp(0, NSM_SUCCESS, 0,
							  &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_powersmoothing_featinfo_resp: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetPwrSmoothFeatInfoResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_powersmoothing_featinfo_resp(0, NSM_SUCCESS, 0,
							  nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_powersmoothing_featinfo_resp: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetPwrSmoothFeatInfoResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	auto rc = decode_get_powersmoothing_featinfo_resp(
	    msg, buf.size(), &cc, &reason, &ds, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_hardware_lifetime_cricuitry_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetHwLifetimeResp_NullMsg)
{
	struct nsm_hardwarecircuitry_data data = {};
	auto rc = encode_get_hardware_lifetime_cricuitry_resp(0, NSM_SUCCESS, 0,
							      &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_hardware_lifetime_cricuitry_resp: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetHwLifetimeResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_hardware_lifetime_cricuitry_resp(0, NSM_SUCCESS, 0,
							      nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_hardware_lifetime_cricuitry_resp: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetHwLifetimeResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	auto rc = decode_get_hardware_lifetime_cricuitry_resp(
	    msg, buf.size(), &cc, &reason, &ds, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_current_profile_info_resp: NULL msg
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetCurrProfileInfoResp_NullMsg)
{
	struct nsm_get_current_profile_data data = {};
	auto rc = encode_get_current_profile_info_resp(0, NSM_SUCCESS, 0, &data,
						       nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_current_profile_info_resp: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetCurrProfileInfoResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_current_profile_info_resp(0, NSM_SUCCESS, 0,
						       nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_current_profile_info_resp: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetCurrProfileInfoResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t ds = 0;
	auto rc = decode_get_current_profile_info_resp(msg, buf.size(), nullptr,
						       nullptr, &ds, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_gpm_metric_percentage_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggGpmPercent_NullData)
{
	double pct = 0.0;
	auto rc = decode_aggregate_gpm_metric_percentage_data(nullptr, 4, &pct);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_gpm_metric_percentage_data: NULL output
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggGpmPercent_NullOutput)
{
	uint8_t data[4] = {};
	auto rc = decode_aggregate_gpm_metric_percentage_data(data, 4, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_gpm_metric_percentage_data: wrong data_len
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggGpmPercent_WrongLen)
{
	uint8_t data[8] = {};
	double pct = 0.0;
	auto rc = decode_aggregate_gpm_metric_percentage_data(data, 3, &pct);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_aggregate_gpm_metric_bandwidth_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggGpmBandwidth_NullData)
{
	size_t len = 0;
	auto rc = encode_aggregate_gpm_metric_bandwidth_data(0, nullptr, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_gpm_metric_bandwidth_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggGpmBandwidth_NullData)
{
	uint64_t bw = 0;
	auto rc = decode_aggregate_gpm_metric_bandwidth_data(nullptr, 8, &bw);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_gpm_metric_bandwidth_data: NULL output
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggGpmBandwidth_NullOutput)
{
	uint8_t data[8] = {};
	auto rc = decode_aggregate_gpm_metric_bandwidth_data(data, 8, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_aggregate_gpm_metric_bandwidth_data: wrong data_len
// ===========================================================================
TEST(PlatEnvBranch6, DecodeAggGpmBandwidth_WrongLen)
{
	uint8_t data[8] = {};
	uint64_t bw = 0;
	auto rc = decode_aggregate_gpm_metric_bandwidth_data(data, 4, &bw);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_aggregate_gpm_metric_percentage_data: NULL data
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggGpmPercent_NullData)
{
	size_t len = 0;
	auto rc =
	    encode_aggregate_gpm_metric_percentage_data(1.0, nullptr, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_aggregate_gpm_metric_percentage_data: NULL data_len
// ===========================================================================
TEST(PlatEnvBranch6, EncodeAggGpmPercent_NullLen)
{
	uint8_t data[8] = {};
	auto rc =
	    encode_aggregate_gpm_metric_percentage_data(1.0, data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_ECC_error_counts_req: NULL msg (via encode_common_req)
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetECCErrorCountsReq_NullMsg)
{
	auto rc = encode_get_ECC_error_counts_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_current_energy_count_req: pack fail
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetCurrEnergyCountReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_current_energy_count_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_get_voltage_req: pack fail
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetVoltageReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_voltage_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_get_current_power_draw_req: pack fail
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetCurrPowerDrawReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_current_power_draw_req(kBadIid, 0, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_get_max_observed_power_req: pack fail
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetMaxObsPowerReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_max_observed_power_req(kBadIid, 0, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_get_altitude_pressure_req: pack fail
// ===========================================================================
TEST(PlatEnvBranch6, EncodeGetAltPressureReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_altitude_pressure_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_current_energy_count_req: NULL sensor_id
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetCurrEnergyCountReq_NullSensor)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_current_energy_count_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_current_energy_count_req: msg too short
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetCurrEnergyCountReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t sensor = 0;
	auto rc = decode_get_current_energy_count_req(msg, buf.size(), &sensor);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_get_voltage_req: NULL sensor_id
// ===========================================================================
TEST(PlatEnvBranch6, DecodeGetVoltageReq_NullSensor)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_voltage_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}
