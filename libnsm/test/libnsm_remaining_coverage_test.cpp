/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "base.h"
#include "device-configuration.h"
#include "firmware-utils.h"
#include "platform-environmental.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

// ============================================================================
// PART 1: platform-environmental.c - Aggregate data helpers
// ============================================================================

TEST(AggregateTimestampData, EncodeDecodeGood)
{
	// Arrange
	uint64_t timestamp_in = 0x123456789ABCDEF0ULL;
	uint8_t data[sizeof(uint64_t)] = {};
	size_t data_len = 0;

	// Act - Encode
	auto rc =
	    encode_aggregate_timestamp_data(timestamp_in, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint64_t));

	// Act - Decode
	uint64_t timestamp_out = 0;
	rc = decode_aggregate_timestamp_data(data, data_len, &timestamp_out);

	// Assert
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(timestamp_in, timestamp_out);
}

TEST(AggregateTimestampData, EncodeNullParams)
{
	uint64_t timestamp = 100;
	uint8_t data[8] = {};
	size_t data_len = 0;

	EXPECT_EQ(
	    encode_aggregate_timestamp_data(timestamp, nullptr, &data_len),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_aggregate_timestamp_data(timestamp, data, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateTimestampData, DecodeNullParams)
{
	uint8_t data[8] = {};
	uint64_t timestamp = 0;

	EXPECT_EQ(decode_aggregate_timestamp_data(nullptr, 8, &timestamp),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_aggregate_timestamp_data(data, 8, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateTimestampData, DecodeBadLength)
{
	uint8_t data[4] = {};
	uint64_t timestamp = 0;

	EXPECT_EQ(decode_aggregate_timestamp_data(data, 4, &timestamp),
		  NSM_SW_ERROR_LENGTH);
}

TEST(AggregateThermalParameterData, EncodeDecodeGood)
{
	int32_t threshold_in = -12345;
	uint8_t data[sizeof(int32_t)] = {};
	size_t data_len = 0;

	auto rc = encode_aggregate_thermal_parameter_data(threshold_in, data,
							  &data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(int32_t));

	int32_t threshold_out = 0;
	rc = decode_aggregate_thermal_parameter_data(data, data_len,
						     &threshold_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(threshold_in, threshold_out);
}

TEST(AggregateThermalParameterData, EncodeNullParams)
{
	int32_t threshold = 100;
	uint8_t data[4] = {};
	size_t data_len = 0;

	EXPECT_EQ(encode_aggregate_thermal_parameter_data(threshold, nullptr,
							  &data_len),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    encode_aggregate_thermal_parameter_data(threshold, data, nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(AggregateThermalParameterData, DecodeNullParams)
{
	uint8_t data[4] = {};
	int32_t threshold = 0;

	EXPECT_EQ(
	    decode_aggregate_thermal_parameter_data(nullptr, 4, &threshold),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_aggregate_thermal_parameter_data(data, 4, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateThermalParameterData, DecodeBadLength)
{
	uint8_t data[2] = {};
	int32_t threshold = 0;

	EXPECT_EQ(decode_aggregate_thermal_parameter_data(data, 2, &threshold),
		  NSM_SW_ERROR_LENGTH);
}

TEST(AggregateCurrentPowerDrawReading, EncodeDecodeGood)
{
	uint32_t reading_in = 0xDEADBEEF;
	uint8_t data[sizeof(uint32_t)] = {};
	size_t data_len = 0;

	auto rc = encode_aggregate_get_current_power_draw_reading(
	    reading_in, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint32_t));

	uint32_t reading_out = 0;
	rc = decode_aggregate_get_current_power_draw_reading(data, data_len,
							     &reading_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(reading_in, reading_out);
}

TEST(AggregateCurrentPowerDrawReading, EncodeNullParams)
{
	uint32_t reading = 100;
	uint8_t data[4] = {};
	size_t data_len = 0;

	EXPECT_EQ(encode_aggregate_get_current_power_draw_reading(
		      reading, nullptr, &data_len),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_aggregate_get_current_power_draw_reading(reading, data,
								  nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateCurrentPowerDrawReading, DecodeNullParams)
{
	uint8_t data[4] = {};
	uint32_t reading = 0;

	EXPECT_EQ(decode_aggregate_get_current_power_draw_reading(nullptr, 4,
								  &reading),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_aggregate_get_current_power_draw_reading(data, 4, nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(AggregateCurrentPowerDrawReading, DecodeBadLength)
{
	uint8_t data[2] = {};
	uint32_t reading = 0;

	EXPECT_EQ(
	    decode_aggregate_get_current_power_draw_reading(data, 2, &reading),
	    NSM_SW_ERROR_LENGTH);
}

TEST(AggregateTemperatureReadingData, EncodeDecodeGood)
{
	double temp_in = 72.5;
	uint8_t data[sizeof(int32_t)] = {};
	size_t data_len = 0;

	auto rc =
	    encode_aggregate_temperature_reading_data(temp_in, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(int32_t));

	double temp_out = 0.0;
	rc = decode_aggregate_temperature_reading_data(data, data_len,
						       &temp_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NEAR(temp_in, temp_out, 0.01);
}

TEST(AggregateTemperatureReadingData, EncodeNullParams)
{
	double temp = 72.5;
	uint8_t data[4] = {};
	size_t data_len = 0;

	EXPECT_EQ(
	    encode_aggregate_temperature_reading_data(temp, nullptr, &data_len),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    encode_aggregate_temperature_reading_data(temp, data, nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(AggregateTemperatureReadingData, DecodeNullParams)
{
	uint8_t data[4] = {};
	double temp = 0.0;

	EXPECT_EQ(decode_aggregate_temperature_reading_data(nullptr, 4, &temp),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_aggregate_temperature_reading_data(data, 4, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateTemperatureReadingData, DecodeBadLength)
{
	uint8_t data[2] = {};
	double temp = 0.0;

	EXPECT_EQ(decode_aggregate_temperature_reading_data(data, 2, &temp),
		  NSM_SW_ERROR_LENGTH);
}

TEST(AggregateEnergyCountData, EncodeDecodeGood)
{
	uint64_t energy_in = 0xFEDCBA9876543210ULL;
	uint8_t data[sizeof(uint64_t)] = {};
	size_t data_len = 0;

	auto rc =
	    encode_aggregate_energy_count_data(energy_in, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint64_t));

	uint64_t energy_out = 0;
	rc = decode_aggregate_energy_count_data(data, data_len, &energy_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(energy_in, energy_out);
}

TEST(AggregateEnergyCountData, EncodeNullParams)
{
	uint64_t energy = 100;
	uint8_t data[8] = {};
	size_t data_len = 0;

	EXPECT_EQ(
	    encode_aggregate_energy_count_data(energy, nullptr, &data_len),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_aggregate_energy_count_data(energy, data, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateEnergyCountData, DecodeNullParams)
{
	uint8_t data[8] = {};
	uint64_t energy = 0;

	EXPECT_EQ(decode_aggregate_energy_count_data(nullptr, 8, &energy),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_aggregate_energy_count_data(data, 8, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateEnergyCountData, DecodeBadLength)
{
	uint8_t data[4] = {};
	uint64_t energy = 0;

	EXPECT_EQ(decode_aggregate_energy_count_data(data, 4, &energy),
		  NSM_SW_ERROR_LENGTH);
}

TEST(AggregateVoltageData, EncodeDecodeGood)
{
	uint32_t voltage_in = 12000;
	uint8_t data[sizeof(uint32_t)] = {};
	size_t data_len = 0;

	auto rc = encode_aggregate_voltage_data(voltage_in, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint32_t));

	uint32_t voltage_out = 0;
	rc = decode_aggregate_voltage_data(data, data_len, &voltage_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(voltage_in, voltage_out);
}

TEST(AggregateVoltageData, EncodeNullParams)
{
	uint32_t voltage = 100;
	uint8_t data[4] = {};
	size_t data_len = 0;

	EXPECT_EQ(encode_aggregate_voltage_data(voltage, nullptr, &data_len),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_aggregate_voltage_data(voltage, data, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateVoltageData, DecodeNullParams)
{
	uint8_t data[4] = {};
	uint32_t voltage = 0;

	EXPECT_EQ(decode_aggregate_voltage_data(nullptr, 4, &voltage),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_aggregate_voltage_data(data, 4, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateVoltageData, DecodeBadLength)
{
	uint8_t data[2] = {};
	uint32_t voltage = 0;

	EXPECT_EQ(decode_aggregate_voltage_data(data, 2, &voltage),
		  NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// PART 2: platform-environmental.c - Aggregate resp/sample encode/decode
// ============================================================================

TEST(AggregateResp, EncodeDecodeGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_aggregate_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_aggregate_resp(0, 0x50, NSM_SUCCESS, 5, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	size_t consumed_len = 0;
	uint8_t cc = 0;
	uint16_t telemetry_count = 0;
	rc = decode_aggregate_resp(msg, msgBuf.size(), &consumed_len, &cc,
				   &telemetry_count);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(telemetry_count, 5);
	EXPECT_EQ(consumed_len,
		  sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST(AggregateResp, EncodeNullMsg)
{
	EXPECT_EQ(encode_aggregate_resp(0, 0x50, NSM_SUCCESS, 5, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateResp, DecodeNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_aggregate_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	size_t consumed = 0;
	uint8_t cc = 0;
	uint16_t count = 0;

	EXPECT_EQ(decode_aggregate_resp(nullptr, msgBuf.size(), &consumed, &cc,
					&count),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_aggregate_resp(msg, msgBuf.size(), &consumed, nullptr,
					&count),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_aggregate_resp(msg, msgBuf.size(), &consumed, &cc, nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(AggregateResp, DecodeBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	size_t consumed = 0;
	uint8_t cc = 0;
	uint16_t count = 0;

	EXPECT_EQ(
	    decode_aggregate_resp(msg, msgBuf.size(), &consumed, &cc, &count),
	    NSM_SW_ERROR_LENGTH);
}

TEST(AggregateRespSample, EncodeDecodeGood)
{
	uint8_t input_data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
	std::vector<uint8_t> sampleBuf(sizeof(nsm_aggregate_resp_sample) + 16);
	auto sample =
	    reinterpret_cast<nsm_aggregate_resp_sample *>(sampleBuf.data());
	size_t sample_len = 0;

	auto rc = encode_aggregate_resp_sample(7, true, input_data, 4, sample,
					       &sample_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t tag = 0;
	bool valid = false;
	const uint8_t *out_data = nullptr;
	size_t out_data_len = 0;
	size_t consumed_len = 0;

	rc = decode_aggregate_resp_sample(sample, sample_len, &consumed_len,
					  &tag, &valid, &out_data,
					  &out_data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(tag, 7);
	EXPECT_TRUE(valid);
	EXPECT_EQ(out_data_len, 4u);
	EXPECT_EQ(memcmp(out_data, input_data, 4), 0);
}

TEST(AggregateRespSample, EncodeNullParams)
{
	uint8_t data[4] = {1, 2, 3, 4};
	nsm_aggregate_resp_sample sample = {};
	size_t sample_len = 0;

	EXPECT_EQ(encode_aggregate_resp_sample(0, true, nullptr, 4, &sample,
					       &sample_len),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_aggregate_resp_sample(0, true, data, 4, nullptr,
					       &sample_len),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    encode_aggregate_resp_sample(0, true, data, 4, &sample, nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(AggregateRespSample, EncodeBadDataLen)
{
	uint8_t data[5] = {1, 2, 3, 4, 5};
	nsm_aggregate_resp_sample sample = {};
	size_t sample_len = 0;

	// data_len=5 is not a power of 2, should fail
	EXPECT_EQ(encode_aggregate_resp_sample(0, true, data, 5, &sample,
					       &sample_len),
		  NSM_SW_ERROR_DATA);
}

TEST(AggregateRespSample, DecodeNullParams)
{
	nsm_aggregate_resp_sample sample = {};
	size_t consumed = 0;
	uint8_t tag = 0;
	bool valid = false;
	const uint8_t *data = nullptr;
	size_t data_len = 0;

	EXPECT_EQ(decode_aggregate_resp_sample(nullptr, 10, &consumed, &tag,
					       &valid, &data, &data_len),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_aggregate_resp_sample(&sample, 10, nullptr, &tag,
					       &valid, &data, &data_len),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_aggregate_resp_sample(&sample, 10, &consumed, nullptr,
					       &valid, &data, &data_len),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_aggregate_resp_sample(&sample, 10, &consumed, &tag,
					       &valid, nullptr, &data_len),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_aggregate_resp_sample(&sample, 10, &consumed, &tag,
					       &valid, &data, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(AggregateRespSample, DecodeBadLength)
{
	nsm_aggregate_resp_sample sample = {};
	size_t consumed = 0;
	uint8_t tag = 0;
	bool valid = false;
	const uint8_t *data = nullptr;
	size_t data_len = 0;

	EXPECT_EQ(decode_aggregate_resp_sample(&sample, 0, &consumed, &tag,
					       &valid, &data, &data_len),
		  NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// PART 3: platform-environmental.c - Power Smoothing functions
// ============================================================================

TEST(PowerSmoothingFeatInfo, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_powersmoothing_featinfo_req(0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_get_powersmoothing_featinfo_req(msg, msgBuf.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(PowerSmoothingFeatInfo, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_smoothing_feat_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	nsm_pwr_smoothing_featureinfo_data data_in = {};
	data_in.feature_flag = 0x07;
	data_in.currentTmpSetting = 1000;
	data_in.currentTmpFloorSetting = 500;
	data_in.maxTmpFloorSettingInPercent = 90;
	data_in.minTmpFloorSettingInPercent = 10;

	auto rc = encode_get_powersmoothing_featinfo_resp(
	    0, NSM_SUCCESS, ERR_NULL, &data_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t data_size = 0;
	nsm_pwr_smoothing_featureinfo_data data_out = {};

	rc = decode_get_powersmoothing_featinfo_resp(
	    msg, msgBuf.size(), &cc, &reason_code, &data_size, &data_out);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_out.feature_flag, 0x07u);
	EXPECT_EQ(data_out.currentTmpSetting, 1000u);
	EXPECT_EQ(data_out.currentTmpFloorSetting, 500u);
	EXPECT_EQ(data_out.maxTmpFloorSettingInPercent, 90);
	EXPECT_EQ(data_out.minTmpFloorSettingInPercent, 10);
}

TEST(PowerSmoothingFeatInfo, EncodeRespNullParams)
{
	nsm_pwr_smoothing_featureinfo_data data = {};
	EXPECT_EQ(encode_get_powersmoothing_featinfo_resp(
		      0, NSM_SUCCESS, ERR_NULL, &data, nullptr),
		  NSM_SW_ERROR_NULL);
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	EXPECT_EQ(encode_get_powersmoothing_featinfo_resp(
		      0, NSM_SUCCESS, ERR_NULL, nullptr, msg),
		  NSM_SW_ERROR_NULL);
}

TEST(PowerSmoothingFeatInfo, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t ds = 0;
	nsm_pwr_smoothing_featureinfo_data data = {};

	EXPECT_EQ(decode_get_powersmoothing_featinfo_resp(nullptr, 256, &cc,
							  &reason, &ds, &data),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_powersmoothing_featinfo_resp(msg, 256, nullptr,
							  &reason, &ds, &data),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_powersmoothing_featinfo_resp(
		      msg, 256, &cc, &reason, nullptr, &data),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_powersmoothing_featinfo_resp(
		      msg, 256, &cc, &reason, &ds, nullptr),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 4: Hardware Lifetime Circuitry
// ============================================================================

TEST(HardwareLifetimeCircuitry, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_hardware_lifetime_cricuitry_req(0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_get_hardware_lifetime_cricuitry_req(msg, msgBuf.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(HardwareLifetimeCircuitry, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_hardwareciruitry_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	nsm_hardwarecircuitry_data data_in = {};
	data_in.reading = 42;

	auto rc = encode_get_hardware_lifetime_cricuitry_resp(
	    0, NSM_SUCCESS, ERR_NULL, &data_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t data_size = 0;
	nsm_hardwarecircuitry_data data_out = {};

	rc = decode_get_hardware_lifetime_cricuitry_resp(
	    msg, msgBuf.size(), &cc, &reason_code, &data_size, &data_out);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_out.reading, 42u);
}

TEST(HardwareLifetimeCircuitry, EncodeRespNullParams)
{
	nsm_hardwarecircuitry_data data = {};
	EXPECT_EQ(encode_get_hardware_lifetime_cricuitry_resp(
		      0, NSM_SUCCESS, ERR_NULL, &data, nullptr),
		  NSM_SW_ERROR_NULL);
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	EXPECT_EQ(encode_get_hardware_lifetime_cricuitry_resp(
		      0, NSM_SUCCESS, ERR_NULL, nullptr, msg),
		  NSM_SW_ERROR_NULL);
}

TEST(HardwareLifetimeCircuitry, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t ds = 0;
	nsm_hardwarecircuitry_data data = {};

	EXPECT_EQ(decode_get_hardware_lifetime_cricuitry_resp(
		      nullptr, 256, &cc, &reason, &ds, &data),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_hardware_lifetime_cricuitry_resp(
		      msg, 256, nullptr, &reason, &ds, &data),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_hardware_lifetime_cricuitry_resp(
		      msg, 256, &cc, &reason, nullptr, &data),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_hardware_lifetime_cricuitry_resp(
		      msg, 256, &cc, &reason, &ds, nullptr),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 5: Current Profile Info
// ============================================================================

TEST(CurrentProfileInfo, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_current_profile_info_req(0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_get_current_profile_info_req(msg, msgBuf.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(CurrentProfileInfo, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_profile_info_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	nsm_get_current_profile_data data_in = {};
	data_in.current_percent_tmp_floor = 50;
	data_in.current_rampup_rate_in_miliwatts_per_second = 1000;
	data_in.current_rampdown_rate_in_miliwatts_per_second = 2000;
	data_in.current_rampdown_hysteresis_value_in_milisec = 500;

	auto rc = encode_get_current_profile_info_resp(0, NSM_SUCCESS, ERR_NULL,
						       &data_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t data_size = 0;
	nsm_get_current_profile_data data_out = {};

	rc = decode_get_current_profile_info_resp(
	    msg, msgBuf.size(), &cc, &reason_code, &data_size, &data_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_out.current_percent_tmp_floor, 50);
	EXPECT_EQ(data_out.current_rampup_rate_in_miliwatts_per_second, 1000u);
	EXPECT_EQ(data_out.current_rampdown_rate_in_miliwatts_per_second,
		  2000u);
	EXPECT_EQ(data_out.current_rampdown_hysteresis_value_in_milisec, 500u);
}

TEST(CurrentProfileInfo, EncodeRespNullParams)
{
	nsm_get_current_profile_data data = {};
	EXPECT_EQ(encode_get_current_profile_info_resp(0, NSM_SUCCESS, ERR_NULL,
						       &data, nullptr),
		  NSM_SW_ERROR_NULL);
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	EXPECT_EQ(encode_get_current_profile_info_resp(0, NSM_SUCCESS, ERR_NULL,
						       nullptr, msg),
		  NSM_SW_ERROR_NULL);
}

TEST(CurrentProfileInfo, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t ds = 0;
	nsm_get_current_profile_data data = {};

	EXPECT_EQ(decode_get_current_profile_info_resp(msg, 256, &cc, &reason,
						       &ds, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_current_profile_info_resp(msg, 256, &cc, &reason,
						       nullptr, &data),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 6: Query Admin Override
// ============================================================================

TEST(QueryAdminOverride, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_query_admin_override_req(0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_query_admin_override_req(msg, msgBuf.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(QueryAdminOverride, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_admin_override_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	nsm_admin_override_data data_in = {};
	data_in.admin_override_percent_tmp_floor = 75;
	data_in.admin_override_ramup_rate_in_miliwatts_per_second = 3000;
	data_in.admin_override_rampdown_rate_in_miliwatts_per_second = 4000;
	data_in.admin_override_rampdown_hysteresis_value_in_milisec = 600;

	auto rc = encode_query_admin_override_resp(0, NSM_SUCCESS, ERR_NULL,
						   &data_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t data_size = 0;
	nsm_admin_override_data data_out = {};

	rc = decode_query_admin_override_resp(
	    msg, msgBuf.size(), &cc, &reason_code, &data_size, &data_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_out.admin_override_percent_tmp_floor, 75);
	EXPECT_EQ(data_out.admin_override_ramup_rate_in_miliwatts_per_second,
		  3000u);
	EXPECT_EQ(data_out.admin_override_rampdown_rate_in_miliwatts_per_second,
		  4000u);
	EXPECT_EQ(data_out.admin_override_rampdown_hysteresis_value_in_milisec,
		  600u);
}

TEST(QueryAdminOverride, EncodeRespNullParams)
{
	nsm_admin_override_data data = {};
	EXPECT_EQ(encode_query_admin_override_resp(0, NSM_SUCCESS, ERR_NULL,
						   &data, nullptr),
		  NSM_SW_ERROR_NULL);
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	EXPECT_EQ(encode_query_admin_override_resp(0, NSM_SUCCESS, ERR_NULL,
						   nullptr, msg),
		  NSM_SW_ERROR_NULL);
}

TEST(QueryAdminOverride, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t ds = 0;
	nsm_admin_override_data data = {};

	EXPECT_EQ(decode_query_admin_override_resp(msg, 256, &cc, &reason, &ds,
						   nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_query_admin_override_resp(msg, 256, &cc, &reason,
						   nullptr, &data),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 7: Set Active Preset Profile
// ============================================================================

TEST(SetActivePresetProfile, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_active_preset_profile_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_set_active_preset_profile_req(0, 3, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t profile_id = 0;
	rc = decode_set_active_preset_profile_req(msg, msgBuf.size(),
						  &profile_id);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(profile_id, 3);
}

TEST(SetActivePresetProfile, EncodeReqNullMsg)
{
	EXPECT_EQ(encode_set_active_preset_profile_req(0, 3, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(SetActivePresetProfile, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_active_preset_profile_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	EXPECT_EQ(decode_set_active_preset_profile_req(nullptr, msgBuf.size(),
						       nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_set_active_preset_profile_req(msg, msgBuf.size(), nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(SetActivePresetProfile, DecodeReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t profile_id = 0;

	EXPECT_EQ(decode_set_active_preset_profile_req(msg, msgBuf.size(),
						       &profile_id),
		  NSM_SW_ERROR_LENGTH);
}

TEST(SetActivePresetProfile, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_set_active_preset_profile_resp(0, NSM_SUCCESS,
							ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_set_active_preset_profile_resp(msg, msgBuf.size(), &cc,
						   &reason_code);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(SetActivePresetProfile, EncodeRespNullMsg)
{
	EXPECT_EQ(encode_set_active_preset_profile_resp(0, NSM_SUCCESS,
							ERR_NULL, nullptr),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 8: Setup Admin Override
// ============================================================================

TEST(SetupAdminOverride, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_setup_admin_override_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_setup_admin_override_req(0, 2, 5000, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t parameter_id = 0;
	uint32_t param_value = 0;
	rc = decode_setup_admin_override_req(msg, msgBuf.size(), &parameter_id,
					     &param_value);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(parameter_id, 2);
	EXPECT_EQ(param_value, 5000u);
}

TEST(SetupAdminOverride, EncodeReqNullMsg)
{
	EXPECT_EQ(encode_setup_admin_override_req(0, 2, 5000, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(SetupAdminOverride, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_setup_admin_override_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t pid = 0;
	uint32_t pval = 0;

	EXPECT_EQ(decode_setup_admin_override_req(nullptr, msgBuf.size(), &pid,
						  &pval),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_setup_admin_override_req(msg, msgBuf.size(), nullptr, &pval),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_setup_admin_override_req(msg, msgBuf.size(), &pid, nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(SetupAdminOverride, DecodeReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t pid = 0;
	uint32_t pval = 0;

	EXPECT_EQ(
	    decode_setup_admin_override_req(msg, msgBuf.size(), &pid, &pval),
	    NSM_SW_ERROR_LENGTH);
}

TEST(SetupAdminOverride, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc =
	    encode_setup_admin_override_resp(0, NSM_SUCCESS, ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_setup_admin_override_resp(msg, msgBuf.size(), &cc,
					      &reason_code);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(SetupAdminOverride, EncodeRespNullMsg)
{
	EXPECT_EQ(
	    encode_setup_admin_override_resp(0, NSM_SUCCESS, ERR_NULL, nullptr),
	    NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 9: Apply Admin Override
// ============================================================================

TEST(ApplyAdminOverride, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_apply_admin_override_req(0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_apply_admin_override_req(msg, msgBuf.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(ApplyAdminOverride, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc =
	    encode_apply_admin_override_resp(0, NSM_SUCCESS, ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_apply_admin_override_resp(msg, msgBuf.size(), &cc,
					      &reason_code);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(ApplyAdminOverride, EncodeRespNullMsg)
{
	EXPECT_EQ(
	    encode_apply_admin_override_resp(0, NSM_SUCCESS, ERR_NULL, nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(ApplyAdminOverride, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;

	EXPECT_EQ(decode_apply_admin_override_resp(nullptr, msgBuf.size(), &cc,
						   &reason),
		  NSM_ERR_INVALID_DATA);
	EXPECT_EQ(decode_apply_admin_override_resp(msg, msgBuf.size(), nullptr,
						   &reason),
		  NSM_ERR_INVALID_DATA);
}

// ============================================================================
// PART 10: Toggle Immediate Rampdown
// ============================================================================

TEST(ToggleImmediateRampdown, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_toggle_immediate_rampdown_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_toggle_immediate_rampdown_req(0, 1, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t ramp_down_toggle = 0;
	rc = decode_toggle_immediate_rampdown_req(msg, msgBuf.size(),
						  &ramp_down_toggle);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(ramp_down_toggle, 1);
}

TEST(ToggleImmediateRampdown, EncodeReqNullMsg)
{
	EXPECT_EQ(encode_toggle_immediate_rampdown_req(0, 1, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(ToggleImmediateRampdown, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_toggle_immediate_rampdown_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t toggle = 0;

	EXPECT_EQ(decode_toggle_immediate_rampdown_req(nullptr, msgBuf.size(),
						       &toggle),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_toggle_immediate_rampdown_req(msg, msgBuf.size(), nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(ToggleImmediateRampdown, DecodeReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t toggle = 0;

	EXPECT_EQ(
	    decode_toggle_immediate_rampdown_req(msg, msgBuf.size(), &toggle),
	    NSM_SW_ERROR_LENGTH);
}

TEST(ToggleImmediateRampdown, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_toggle_immediate_rampdown_resp(0, NSM_SUCCESS,
							ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_toggle_immediate_rampdown_resp(msg, msgBuf.size(), &cc,
						   &reason_code);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(ToggleImmediateRampdown, EncodeRespNullMsg)
{
	EXPECT_EQ(encode_toggle_immediate_rampdown_resp(0, NSM_SUCCESS,
							ERR_NULL, nullptr),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 11: Toggle Feature State
// ============================================================================

TEST(ToggleFeatureState, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_toggle_feature_state_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_toggle_feature_state_req(0, 1, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t feature_state = 0;
	rc =
	    decode_toggle_feature_state_req(msg, msgBuf.size(), &feature_state);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(feature_state, 1);
}

TEST(ToggleFeatureState, EncodeReqNullMsg)
{
	EXPECT_EQ(encode_toggle_feature_state_req(0, 1, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(ToggleFeatureState, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_toggle_feature_state_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t state = 0;

	EXPECT_EQ(
	    decode_toggle_feature_state_req(nullptr, msgBuf.size(), &state),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_toggle_feature_state_req(msg, msgBuf.size(), nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(ToggleFeatureState, DecodeReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t state = 0;

	EXPECT_EQ(decode_toggle_feature_state_req(msg, msgBuf.size(), &state),
		  NSM_SW_ERROR_LENGTH);
}

TEST(ToggleFeatureState, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc =
	    encode_toggle_feature_state_resp(0, NSM_SUCCESS, ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_toggle_feature_state_resp(msg, msgBuf.size(), &cc,
					      &reason_code);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(ToggleFeatureState, EncodeRespNullMsg)
{
	EXPECT_EQ(
	    encode_toggle_feature_state_resp(0, NSM_SUCCESS, ERR_NULL, nullptr),
	    NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 12: Update Preset Profile Param
// ============================================================================

TEST(UpdatePresetProfileParam, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_update_preset_profile_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_update_preset_profile_param_req(0, 1, 2, 7777, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t profile_id = 0;
	uint8_t parameter_id = 0;
	uint32_t param_value = 0;
	rc = decode_update_preset_profile_param_req(
	    msg, msgBuf.size(), &profile_id, &parameter_id, &param_value);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(profile_id, 1);
	EXPECT_EQ(parameter_id, 2);
	EXPECT_EQ(param_value, 7777u);
}

TEST(UpdatePresetProfileParam, EncodeReqNullMsg)
{
	EXPECT_EQ(
	    encode_update_preset_profile_param_req(0, 1, 2, 7777, nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(UpdatePresetProfileParam, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_update_preset_profile_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t pid = 0, paramid = 0;
	uint32_t pval = 0;

	EXPECT_EQ(decode_update_preset_profile_param_req(nullptr, msgBuf.size(),
							 &pid, &paramid, &pval),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_update_preset_profile_param_req(msg, msgBuf.size(),
							 &pid, nullptr, &pval),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_update_preset_profile_param_req(
		      msg, msgBuf.size(), &pid, &paramid, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(UpdatePresetProfileParam, DecodeReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t pid = 0, paramid = 0;
	uint32_t pval = 0;

	EXPECT_EQ(decode_update_preset_profile_param_req(msg, msgBuf.size(),
							 &pid, &paramid, &pval),
		  NSM_SW_ERROR_LENGTH);
}

TEST(UpdatePresetProfileParam, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_update_preset_profile_param_resp(0, NSM_SUCCESS,
							  ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_update_preset_profile_param_resp(msg, msgBuf.size(), &cc,
						     &reason_code);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(UpdatePresetProfileParam, EncodeRespNullMsg)
{
	EXPECT_EQ(encode_update_preset_profile_param_resp(0, NSM_SUCCESS,
							  ERR_NULL, nullptr),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 13: Enable/Disable Workload Power Profile
// ============================================================================

TEST(EnableWorkloadPowerProfile, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_enable_workload_power_profile_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	bitfield256_t mask_in = {};
	mask_in.fields[0].byte = 0x01;
	mask_in.fields[1].byte = 0x02;

	auto rc = encode_enable_workload_power_profile_req(0, &mask_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	bitfield256_t mask_out = {};
	rc = decode_enable_workload_power_profile_req(msg, msgBuf.size(),
						      &mask_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(EnableWorkloadPowerProfile, EncodeReqNullMsg)
{
	bitfield256_t mask = {};
	EXPECT_EQ(encode_enable_workload_power_profile_req(0, &mask, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(EnableWorkloadPowerProfile, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_enable_workload_power_profile_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	bitfield256_t mask = {};

	EXPECT_EQ(decode_enable_workload_power_profile_req(
		      nullptr, msgBuf.size(), &mask),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_enable_workload_power_profile_req(msg, msgBuf.size(),
							   nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(EnableWorkloadPowerProfile, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_enable_workload_power_profile_resp(0, NSM_SUCCESS,
							    ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_enable_workload_power_profile_resp(msg, msgBuf.size(), &cc,
						       &reason_code);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(EnableWorkloadPowerProfile, EncodeRespNullMsg)
{
	EXPECT_EQ(encode_enable_workload_power_profile_resp(0, NSM_SUCCESS,
							    ERR_NULL, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(EnableWorkloadPowerProfile, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;

	EXPECT_EQ(decode_enable_workload_power_profile_resp(
		      nullptr, msgBuf.size(), &cc, &reason),
		  NSM_ERR_INVALID_DATA);
	EXPECT_EQ(decode_enable_workload_power_profile_resp(msg, msgBuf.size(),
							    nullptr, &reason),
		  NSM_ERR_INVALID_DATA);
}

TEST(DisableWorkloadPowerProfile, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_disable_workload_power_profile_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	bitfield256_t mask_in = {};
	mask_in.fields[0].byte = 0x03;

	auto rc = encode_disable_workload_power_profile_req(0, &mask_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	bitfield256_t mask_out = {};
	rc = decode_disable_workload_power_profile_req(msg, msgBuf.size(),
						       &mask_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DisableWorkloadPowerProfile, EncodeReqNullMsg)
{
	bitfield256_t mask = {};
	EXPECT_EQ(encode_disable_workload_power_profile_req(0, &mask, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(DisableWorkloadPowerProfile, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_disable_workload_power_profile_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	bitfield256_t mask = {};

	EXPECT_EQ(decode_disable_workload_power_profile_req(
		      nullptr, msgBuf.size(), &mask),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_disable_workload_power_profile_req(msg, msgBuf.size(),
							    nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(DisableWorkloadPowerProfile, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_disable_workload_power_profile_resp(0, NSM_SUCCESS,
							     ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_disable_workload_power_profile_resp(msg, msgBuf.size(), &cc,
							&reason_code);
	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(DisableWorkloadPowerProfile, EncodeRespNullMsg)
{
	EXPECT_EQ(encode_disable_workload_power_profile_resp(0, NSM_SUCCESS,
							     ERR_NULL, nullptr),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 14: Get Workload Power Profile Status
// ============================================================================

TEST(WorkloadPowerProfileStatus, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_workload_power_profile_status_req(0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_get_workload_power_profile_status_req(msg, msgBuf.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(WorkloadPowerProfileStatus, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_get_workload_power_profile_status_info_resp),
	    0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	workload_power_profile_status data_in = {};
	data_in.supported_profile_mask.fields[0].byte = 0xFF;

	auto rc = encode_get_workload_power_profile_status_resp(
	    0, NSM_SUCCESS, ERR_NULL, &data_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t data_size = 0;
	workload_power_profile_status data_out = {};

	rc = decode_get_workload_power_profile_status_resp(
	    msg, msgBuf.size(), &cc, &reason_code, &data_size, &data_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(WorkloadPowerProfileStatus, EncodeRespNullParams)
{
	workload_power_profile_status data = {};
	EXPECT_EQ(encode_get_workload_power_profile_status_resp(
		      0, NSM_SUCCESS, ERR_NULL, &data, nullptr),
		  NSM_SW_ERROR_NULL);
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	EXPECT_EQ(encode_get_workload_power_profile_status_resp(
		      0, NSM_SUCCESS, ERR_NULL, nullptr, msg),
		  NSM_SW_ERROR_NULL);
}

TEST(WorkloadPowerProfileStatus, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t ds = 0;
	workload_power_profile_status data = {};

	EXPECT_EQ(decode_get_workload_power_profile_status_resp(
		      msg, 256, &cc, &reason, &ds, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_workload_power_profile_status_resp(
		      msg, 256, &cc, &reason, nullptr, &data),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 15: Get Workload Power Profile Info
// ============================================================================

TEST(WorkloadPowerProfileInfo, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_workload_power_profile_info_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_workload_power_profile_info_req(0, 42, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint16_t identifier = 0;
	rc = decode_get_workload_power_profile_info_req(msg, msgBuf.size(),
							&identifier);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(identifier, 42);
}

TEST(WorkloadPowerProfileInfo, EncodeReqNullMsg)
{
	EXPECT_EQ(encode_get_workload_power_profile_info_req(0, 42, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(WorkloadPowerProfileInfo, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_workload_power_profile_info_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint16_t id = 0;

	EXPECT_EQ(decode_get_workload_power_profile_info_req(
		      nullptr, msgBuf.size(), &id),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_workload_power_profile_info_req(msg, msgBuf.size(),
							     nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(WorkloadPowerProfileInfo, DecodeReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint16_t id = 0;

	EXPECT_EQ(
	    decode_get_workload_power_profile_info_req(msg, msgBuf.size(), &id),
	    NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// PART 16: Get Preset Profile (req encode/decode)
// ============================================================================

TEST(GetPresetProfile, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_preset_profile_req(0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_get_preset_profile_req(msg, msgBuf.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// PART 17: Violation Duration Event Resp
// ============================================================================

TEST(ViolationDurationEvent, EncodeDecodeRespGood)
{
	// This needs enough space for the long running response format
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_resp) +
					sizeof(nsm_violation_duration) + 64,
				    0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	nsm_violation_duration data_in = {};
	data_in.hw_violation_duration = 100;
	data_in.global_sw_violation_duration = 200;
	data_in.power_violation_duration = 300;
	data_in.thermal_violation_duration = 400;

	auto rc = encode_get_violation_duration_event_resp(
	    0, NSM_SUCCESS, ERR_NULL, &data_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// PART 18: device-configuration.c - Protection Options
// ============================================================================

TEST(ProtectionOptions, EncodeDecodeSetReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_protection_options_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_set_protection_options_req(0, 0x01, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t protection_mode = 0;
	rc = decode_set_protection_options_req(msg, msgBuf.size(),
					       &protection_mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(protection_mode, 0x01);
}

TEST(ProtectionOptions, EncodeDecodeSetRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc =
	    encode_set_protection_options_resp(0, NSM_SUCCESS, ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_set_protection_options_resp(msg, msgBuf.size(), &cc,
						&reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(ProtectionOptions, EncodeDecodeGetReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_protection_options_req(0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_get_protection_options_req(msg, msgBuf.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(ProtectionOptions, EncodeDecodeGetRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_protection_options_resp(0, NSM_SUCCESS, ERR_NULL,
						     0x02, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t protection_mode = 0;
	rc = decode_get_protection_options_resp(msg, msgBuf.size(), &cc,
						&reason_code, &protection_mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(protection_mode, 0x02);
}

TEST(ProtectionOptions, DecodeGetRespNullProtectionMode)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_protection_options_resp(0, NSM_SUCCESS, ERR_NULL,
						     0x02, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_get_protection_options_resp(msg, msgBuf.size(), &cc,
						&reason_code, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 19: device-configuration.c - EGM Mode
// ============================================================================

TEST(EGMMode, EncodeDecodeGetReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_EGM_mode_req(0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_get_EGM_mode_req(msg, msgBuf.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(EGMMode, EncodeDecodeSetReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_EGM_mode_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_set_EGM_mode_req(0, 1, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t requested_mode = 0;
	rc = decode_set_EGM_mode_req(msg, msgBuf.size(), &requested_mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(requested_mode, 1);
}

TEST(EGMMode, DecodeSetReqNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_EGM_mode_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t mode = 0;

	EXPECT_EQ(decode_set_EGM_mode_req(nullptr, msgBuf.size(), &mode),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_set_EGM_mode_req(msg, msgBuf.size(), nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(EGMMode, DecodeSetReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t mode = 0;

	EXPECT_EQ(decode_set_EGM_mode_req(msg, msgBuf.size(), &mode),
		  NSM_SW_ERROR_LENGTH);
}

TEST(EGMMode, EncodeDecodeSetRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_set_EGM_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t data_size = 0;
	uint16_t reason_code = 0;
	rc = decode_set_EGM_mode_resp(msg, msgBuf.size(), &cc, &data_size,
				      &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(EGMMode, EncodeDecodeGetRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_EGM_mode_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	bitfield8_t flags_in = {};
	flags_in.byte = 0x03;

	auto rc =
	    encode_get_EGM_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t data_size = 0;
	uint16_t reason_code = 0;
	bitfield8_t flags_out = {};

	rc = decode_get_EGM_mode_resp(msg, msgBuf.size(), &cc, &data_size,
				      &reason_code, &flags_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(flags_out.byte, 0x03);
}

TEST(EGMMode, EncodeGetRespNullParams)
{
	bitfield8_t flags = {};
	EXPECT_EQ(
	    encode_get_EGM_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, nullptr),
	    NSM_SW_ERROR_NULL);
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	EXPECT_EQ(
	    encode_get_EGM_mode_resp(0, NSM_SUCCESS, ERR_NULL, nullptr, msg),
	    NSM_SW_ERROR_NULL);
}

TEST(EGMMode, DecodeGetRespNullParams)
{
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t ds = 0;
	uint16_t reason = 0;
	bitfield8_t flags = {};

	EXPECT_EQ(
	    decode_get_EGM_mode_resp(nullptr, 256, &cc, &ds, &reason, &flags),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_get_EGM_mode_resp(msg, 256, nullptr, &ds, &reason, &flags),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_get_EGM_mode_resp(msg, 256, &cc, nullptr, &reason, &flags),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_get_EGM_mode_resp(msg, 256, &cc, &ds, &reason, nullptr),
	    NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 20: firmware-utils.c - Irreversible Config
// ============================================================================

TEST(FirmwareIrreversibleConfig, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_firmware_irreversible_config_req_command));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	nsm_firmware_irreversible_config_req fw_req_in = {};
	fw_req_in.request_type = 0;

	auto rc =
	    encode_nsm_firmware_irreversible_config_req(0, &fw_req_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_firmware_irreversible_config_req fw_req_out = {};
	rc = decode_nsm_firmware_irreversible_config_req(msg, msgBuf.size(),
							 &fw_req_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(fw_req_out.request_type, 0);
}

TEST(FirmwareIrreversibleConfig, EncodeReqNullMsg)
{
	nsm_firmware_irreversible_config_req fw_req = {};
	EXPECT_EQ(
	    encode_nsm_firmware_irreversible_config_req(0, &fw_req, nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(FirmwareIrreversibleConfig, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(256);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	nsm_firmware_irreversible_config_req fw_req = {};

	EXPECT_EQ(decode_nsm_firmware_irreversible_config_req(
		      nullptr, msgBuf.size(), &fw_req),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_nsm_firmware_irreversible_config_req(
		      msg, msgBuf.size(), nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(FirmwareIrreversibleConfig, DecodeReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	nsm_firmware_irreversible_config_req fw_req = {};

	EXPECT_EQ(decode_nsm_firmware_irreversible_config_req(
		      msg, msgBuf.size(), &fw_req),
		  NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// PART 21: firmware-utils.c - Update Security Version
// ============================================================================

TEST(FirmwareUpdateSecVer, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_firmware_update_min_sec_ver_req_command));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	nsm_firmware_update_min_sec_ver_req fw_req_in = {};
	fw_req_in.component_classification = 0x0A;
	fw_req_in.component_identifier = 0x0B;

	auto rc = encode_nsm_firmware_update_sec_ver_req(0, &fw_req_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_firmware_update_min_sec_ver_req fw_req_out = {};
	rc = decode_nsm_firmware_update_sec_ver_req(msg, msgBuf.size(),
						    &fw_req_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(FirmwareUpdateSecVer, EncodeReqNullMsg)
{
	nsm_firmware_update_min_sec_ver_req fw_req = {};
	EXPECT_EQ(encode_nsm_firmware_update_sec_ver_req(0, &fw_req, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(FirmwareUpdateSecVer, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(256);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	nsm_firmware_update_min_sec_ver_req fw_req = {};

	EXPECT_EQ(decode_nsm_firmware_update_sec_ver_req(nullptr, msgBuf.size(),
							 &fw_req),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_nsm_firmware_update_sec_ver_req(msg, msgBuf.size(), nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(FirmwareUpdateSecVer, DecodeReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	nsm_firmware_update_min_sec_ver_req fw_req = {};

	EXPECT_EQ(
	    decode_nsm_firmware_update_sec_ver_req(msg, msgBuf.size(), &fw_req),
	    NSM_SW_ERROR_LENGTH);
}

TEST(FirmwareUpdateSecVer, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_firmware_update_min_sec_ver_resp_command),
	    0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	nsm_firmware_update_min_sec_ver_resp sec_resp_in = {};

	auto rc = encode_nsm_firmware_update_sec_ver_resp(
	    0, NSM_SUCCESS, ERR_NULL, &sec_resp_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	nsm_firmware_update_min_sec_ver_resp sec_resp_out = {};

	rc = decode_nsm_firmware_update_sec_ver_resp(
	    msg, msgBuf.size(), &cc, &reason_code, &sec_resp_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(FirmwareUpdateSecVer, EncodeRespNullMsg)
{
	nsm_firmware_update_min_sec_ver_resp sec_resp = {};
	EXPECT_EQ(encode_nsm_firmware_update_sec_ver_resp(
		      0, NSM_SUCCESS, ERR_NULL, &sec_resp, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(FirmwareUpdateSecVer, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_firmware_update_min_sec_ver_resp sec_resp = {};

	EXPECT_EQ(decode_nsm_firmware_update_sec_ver_resp(nullptr, 256, &cc,
							  &reason, &sec_resp),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_nsm_firmware_update_sec_ver_resp(msg, 256, &cc,
							  &reason, nullptr),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 22: firmware-utils.c - Set ROT Property
// ============================================================================

TEST(FirmwareSetRotProperty, DecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_firmware_set_rot_property_resp_command),
	    0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
							    ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_nsm_firmware_set_rot_property_resp(msg, msgBuf.size(), &cc,
						       &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(FirmwareSetRotProperty, EncodeRespNullMsg)
{
	EXPECT_EQ(encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
							    ERR_NULL, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(FirmwareSetRotProperty, DecodeRespNullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;

	EXPECT_EQ(decode_nsm_firmware_set_rot_property_resp(nullptr, 256, &cc,
							    &reason),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 23: firmware-utils.c - DOT Disable
// ============================================================================

TEST(DotDisable, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_dot_disable_req_command));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	nsm_dot_disable_req req_in = {};
	req_in.unlock_method = 1;
	memset(req_in.lak_pub, 0xAA, sizeof(req_in.lak_pub));

	auto rc = encode_nsm_dot_disable_req(0, &req_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_disable_req req_out = {};
	rc = decode_nsm_dot_disable_req(msg, msgBuf.size(), &req_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(req_out.unlock_method, 1u);
}

TEST(DotDisable, EncodeReqNullParams)
{
	nsm_dot_disable_req req = {};
	EXPECT_EQ(encode_nsm_dot_disable_req(0, &req, nullptr),
		  NSM_SW_ERROR_NULL);
	std::vector<uint8_t> msgBuf(4096);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	EXPECT_EQ(encode_nsm_dot_disable_req(0, nullptr, msg),
		  NSM_SW_ERROR_NULL);
}

TEST(DotDisable, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(4096);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	nsm_dot_disable_req req = {};

	EXPECT_EQ(decode_nsm_dot_disable_req(nullptr, msgBuf.size(), &req),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_nsm_dot_disable_req(msg, msgBuf.size(), nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(DotDisable, DecodeReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	nsm_dot_disable_req req = {};

	EXPECT_EQ(decode_nsm_dot_disable_req(msg, msgBuf.size(), &req),
		  NSM_SW_ERROR_LENGTH);
}

TEST(DotDisable, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t blob_in[DOT_BLOB_SIZE];
	memset(blob_in, 0xBB, DOT_BLOB_SIZE);

	auto rc =
	    encode_nsm_dot_disable_resp(0, NSM_SUCCESS, ERR_NULL, blob_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t blob_out[DOT_BLOB_SIZE] = {};

	rc = decode_nsm_dot_disable_resp(msg, msgBuf.size(), &cc, &reason_code,
					 blob_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(memcmp(blob_in, blob_out, DOT_BLOB_SIZE), 0);
}

TEST(DotDisable, EncodeRespNullMsg)
{
	uint8_t blob[DOT_BLOB_SIZE] = {};
	EXPECT_EQ(encode_nsm_dot_disable_resp(0, NSM_SUCCESS, ERR_NULL, blob,
					      nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(DotDisable, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_dot_disable_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t blob[DOT_BLOB_SIZE] = {};

	EXPECT_EQ(decode_nsm_dot_disable_resp(nullptr, msgBuf.size(), &cc,
					      &reason, blob),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_nsm_dot_disable_resp(msg, msgBuf.size(), nullptr,
					      &reason, blob),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_nsm_dot_disable_resp(msg, msgBuf.size(), &cc, nullptr, blob),
	    NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 24: firmware-utils.c - DOT Recovery
// ============================================================================

TEST(DotRecovery, EncodeDecodeReqGood)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_dot_recovery_req_command));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	nsm_dot_recovery_req req_in = {};
	memset(req_in.dot_blob, 0xCC, DOT_BLOB_SIZE);

	auto rc = encode_nsm_dot_recovery_req(0, &req_in, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_recovery_req req_out = {};
	rc = decode_nsm_dot_recovery_req(msg, msgBuf.size(), &req_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(memcmp(req_in.dot_blob, req_out.dot_blob, DOT_BLOB_SIZE), 0);
}

TEST(DotRecovery, EncodeReqNullParams)
{
	nsm_dot_recovery_req req = {};
	EXPECT_EQ(encode_nsm_dot_recovery_req(0, &req, nullptr),
		  NSM_SW_ERROR_NULL);
	std::vector<uint8_t> msgBuf(4096);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	EXPECT_EQ(encode_nsm_dot_recovery_req(0, nullptr, msg),
		  NSM_SW_ERROR_NULL);
}

TEST(DotRecovery, DecodeReqNullParams)
{
	std::vector<uint8_t> msgBuf(4096);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	nsm_dot_recovery_req req = {};

	EXPECT_EQ(decode_nsm_dot_recovery_req(nullptr, msgBuf.size(), &req),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_nsm_dot_recovery_req(msg, msgBuf.size(), nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(DotRecovery, DecodeReqBadLength)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	nsm_dot_recovery_req req = {};

	EXPECT_EQ(decode_nsm_dot_recovery_req(msg, msgBuf.size(), &req),
		  NSM_SW_ERROR_LENGTH);
}

TEST(DotRecovery, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_nsm_dot_recovery_resp(0, NSM_SUCCESS, ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc =
	    decode_nsm_dot_recovery_resp(msg, msgBuf.size(), &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(DotRecovery, EncodeRespNullMsg)
{
	EXPECT_EQ(
	    encode_nsm_dot_recovery_resp(0, NSM_SUCCESS, ERR_NULL, nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(DotRecovery, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;

	EXPECT_EQ(
	    decode_nsm_dot_recovery_resp(nullptr, msgBuf.size(), &cc, &reason),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_nsm_dot_recovery_resp(msg, msgBuf.size(), nullptr, &reason),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_nsm_dot_recovery_resp(msg, msgBuf.size(), &cc, nullptr),
	    NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 25: firmware-utils.c - Code Auth Key Perm Query req decode
// ============================================================================

TEST(LeakDetectionInfo, EncodeDecodeRespGood)
{
	// 2 sensors, 2 threshold levels
	uint8_t num_sensors = 2;
	uint8_t num_thresholds = 2;

	// Each sensor: sensor_id(1) + leak_state(1) + thresholds(2*2) + adc(2)
	// = 8
	size_t sensor_info_size = sizeof(uint8_t) + sizeof(uint8_t) +
				  (num_thresholds * sizeof(uint16_t)) +
				  sizeof(uint16_t);
	size_t sensors_data_len = num_sensors * sensor_info_size;

	std::vector<uint8_t> sensors_in(sensors_data_len, 0);
	auto *s0 = reinterpret_cast<nsm_leak_detection_sensors_data *>(
	    sensors_in.data());
	s0->sensor_id = 1;
	s0->leak_state = 0;
	s0->thresholds[0] = 100;
	s0->thresholds[1] = 200;
	s0->adc_reading = 50;

	auto *s1 = reinterpret_cast<nsm_leak_detection_sensors_data *>(
	    sensors_in.data() + sensor_info_size);
	s1->sensor_id = 2;
	s1->leak_state = 1;
	s1->thresholds[0] = 300;
	s1->thresholds[1] = 400;
	s1->adc_reading = 350;

	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_leak_detection_info_resp) +
		sensors_data_len + 64,
	    0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_get_leak_detection_info_resp(
	    0, NSM_SUCCESS, ERR_NULL, num_sensors, num_thresholds,
	    sensors_in.data(), sensors_data_len, msg);
	EXPECT_EQ(rc, NSM_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t num_sensors_out = 0;
	uint8_t num_thresholds_out = 0;
	std::vector<uint8_t> sensors_out(sensors_data_len, 0);
	size_t sensors_data_len_out = 0;

	rc = decode_get_leak_detection_info_resp(
	    msg, msgBuf.size(), &cc, &reason_code, &num_sensors_out,
	    &num_thresholds_out, sensors_out.data(), &sensors_data_len_out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(num_sensors_out, num_sensors);
	EXPECT_EQ(num_thresholds_out, num_thresholds);
}

TEST(LeakDetectionInfo, EncodeRespNullParams)
{
	uint8_t sensors_data[8] = {};
	EXPECT_EQ(encode_get_leak_detection_info_resp(
		      0, NSM_SUCCESS, ERR_NULL, 1, 1, sensors_data, 8, nullptr),
		  NSM_SW_ERROR_NULL);
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	EXPECT_EQ(encode_get_leak_detection_info_resp(0, NSM_SUCCESS, ERR_NULL,
						      1, 1, nullptr, 8, msg),
		  NSM_SW_ERROR_NULL);
}

TEST(LeakDetectionInfo, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t ns = 0, nt = 0;
	uint8_t sd[64] = {};
	size_t sdl = 0;

	EXPECT_EQ(decode_get_leak_detection_info_resp(msg, 256, &cc, &reason,
						      nullptr, &nt, sd, &sdl),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_leak_detection_info_resp(msg, 256, &cc, &reason,
						      &ns, nullptr, sd, &sdl),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_leak_detection_info_resp(msg, 256, &cc, &reason,
						      &ns, &nt, nullptr, &sdl),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_leak_detection_info_resp(msg, 256, &cc, &reason,
						      &ns, &nt, sd, nullptr),
		  NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 28: Set Leak Detection Thresholds
// ============================================================================

TEST(SetLeakDetectionThresholds, EncodeDecodeRespGood)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_set_leak_detection_thresholds_resp),
	    0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_set_leak_detection_thresholds_resp(0, NSM_SUCCESS,
							    ERR_NULL, msg);
	EXPECT_EQ(rc, NSM_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_set_leak_detection_thresholds_resp(msg, msgBuf.size(), &cc,
						       &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(SetLeakDetectionThresholds, EncodeRespNullMsg)
{
	EXPECT_EQ(encode_set_leak_detection_thresholds_resp(0, NSM_SUCCESS,
							    ERR_NULL, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(SetLeakDetectionThresholds, DecodeRespNullParams)
{
	std::vector<uint8_t> msgBuf(256, 0);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;

	EXPECT_EQ(decode_set_leak_detection_thresholds_resp(nullptr, 256, &cc,
							    &reason),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_set_leak_detection_thresholds_resp(msg, 256, nullptr,
							    &reason),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_set_leak_detection_thresholds_resp(msg, 256, &cc, nullptr),
	    NSM_SW_ERROR_NULL);
}

// ============================================================================
// PART 29: Set Programmable EDPp Scaling Factor
// ============================================================================
