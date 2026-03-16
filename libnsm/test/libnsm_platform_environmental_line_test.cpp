/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

// Line Coverage Tests for libnsm/platform-environmental.c
// Focus: Untested aggregate and environmental sensor encode/decode functions

#include "base.h"
#include "platform-environmental.h"

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

// ========== Aggregate Temperature Reading Data Tests ==========

TEST(PlatformEnvLine, EncodeAggregateTemperatureReadingDataValid)
{
	double temperature = 25.5;
	uint8_t data[4] = {0};
	size_t data_len = 0;

	int rc = encode_aggregate_temperature_reading_data(temperature, data,
							   &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(int32_t));
}

TEST(PlatformEnvLine, EncodeAggregateTemperatureReadingDataNullData)
{
	double temperature = 25.5;
	size_t data_len = 0;

	int rc = encode_aggregate_temperature_reading_data(temperature, nullptr,
							   &data_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, EncodeAggregateTemperatureReadingDataNullDataLen)
{
	double temperature = 25.5;
	uint8_t data[4] = {0};

	int rc = encode_aggregate_temperature_reading_data(temperature, data,
							   nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateTemperatureReadingDataValid)
{
	uint8_t data[4] = {0x80, 0x19, 0x00,
			   0x00}; // 25.5 * 256 = 6528 = 0x1980 (LE)
	double temperature = 0.0;

	int rc = decode_aggregate_temperature_reading_data(data, sizeof(data),
							   &temperature);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NEAR(temperature, 25.5, 0.1);
}

TEST(PlatformEnvLine, DecodeAggregateTemperatureReadingDataNullData)
{
	double temperature = 0.0;

	int rc =
	    decode_aggregate_temperature_reading_data(nullptr, 4, &temperature);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateTemperatureReadingDataNullTemperature)
{
	uint8_t data[4] = {0x80, 0x19, 0x00, 0x00};

	int rc = decode_aggregate_temperature_reading_data(data, sizeof(data),
							   nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateTemperatureReadingDataInvalidLength)
{
	uint8_t data[2] = {0x80, 0x19};
	double temperature = 0.0;

	int rc = decode_aggregate_temperature_reading_data(data, sizeof(data),
							   &temperature);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ========== Aggregate Power Draw Reading Tests ==========

TEST(PlatformEnvLine, EncodeAggregatePowerDrawReadingValid)
{
	uint32_t reading = 12345;
	uint8_t data[4] = {0};
	size_t data_len = 0;

	int rc = encode_aggregate_get_current_power_draw_reading(reading, data,
								 &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint32_t));
}

TEST(PlatformEnvLine, EncodeAggregatePowerDrawReadingNullData)
{
	uint32_t reading = 12345;
	size_t data_len = 0;

	int rc = encode_aggregate_get_current_power_draw_reading(
	    reading, nullptr, &data_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, EncodeAggregatePowerDrawReadingNullDataLen)
{
	uint32_t reading = 12345;
	uint8_t data[4] = {0};

	int rc = encode_aggregate_get_current_power_draw_reading(reading, data,
								 nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregatePowerDrawReadingValid)
{
	uint8_t data[4] = {0x39, 0x30, 0x00, 0x00}; // 12345 in little-endian
	uint32_t reading = 0;

	int rc = decode_aggregate_get_current_power_draw_reading(
	    data, sizeof(data), &reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(reading, 12345U);
}

TEST(PlatformEnvLine, DecodeAggregatePowerDrawReadingNullData)
{
	uint32_t reading = 0;

	int rc = decode_aggregate_get_current_power_draw_reading(nullptr, 4,
								 &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregatePowerDrawReadingNullReading)
{
	uint8_t data[4] = {0x39, 0x30, 0x00, 0x00};

	int rc = decode_aggregate_get_current_power_draw_reading(
	    data, sizeof(data), nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregatePowerDrawReadingInvalidLength)
{
	uint8_t data[2] = {0x39, 0x30};
	uint32_t reading = 0;

	int rc = decode_aggregate_get_current_power_draw_reading(
	    data, sizeof(data), &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ========== Aggregate Timestamp Data Tests ==========

TEST(PlatformEnvLine, EncodeAggregateTimestampValid)
{
	uint64_t timestamp = 1234567890ULL;
	uint8_t data[8] = {0};
	size_t data_len = 0;

	int rc = encode_aggregate_timestamp_data(timestamp, data, &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint64_t));
}

TEST(PlatformEnvLine, EncodeAggregateTimestampNullData)
{
	uint64_t timestamp = 1234567890ULL;
	size_t data_len = 0;

	int rc = encode_aggregate_timestamp_data(timestamp, nullptr, &data_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, EncodeAggregateTimestampNullDataLen)
{
	uint64_t timestamp = 1234567890ULL;
	uint8_t data[8] = {0};

	int rc = encode_aggregate_timestamp_data(timestamp, data, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateTimestampValid)
{
	uint8_t data[8] = {0xD2, 0x02, 0x96, 0x49,
			   0x00, 0x00, 0x00, 0x00}; // 1234567890 LE
	uint64_t timestamp = 0;

	int rc =
	    decode_aggregate_timestamp_data(data, sizeof(data), &timestamp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(timestamp, 1234567890ULL);
}

TEST(PlatformEnvLine, DecodeAggregateTimestampNullData)
{
	uint64_t timestamp = 0;

	int rc = decode_aggregate_timestamp_data(nullptr, 8, &timestamp);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateTimestampNullTimestamp)
{
	uint8_t data[8] = {0xD2, 0x02, 0x96, 0x49, 0x00, 0x00, 0x00, 0x00};

	int rc = decode_aggregate_timestamp_data(data, sizeof(data), nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateTimestampInvalidLength)
{
	uint8_t data[4] = {0xD2, 0x02, 0x96, 0x49};
	uint64_t timestamp = 0;

	int rc =
	    decode_aggregate_timestamp_data(data, sizeof(data), &timestamp);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ========== Aggregate Energy Count Data Tests ==========

TEST(PlatformEnvLine, EncodeAggregateEnergyCountValid)
{
	uint64_t energy = 9876543210ULL;
	uint8_t data[8] = {0};
	size_t data_len = 0;

	int rc = encode_aggregate_energy_count_data(energy, data, &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint64_t));
}

TEST(PlatformEnvLine, EncodeAggregateEnergyCountNullData)
{
	uint64_t energy = 9876543210ULL;
	size_t data_len = 0;

	int rc = encode_aggregate_energy_count_data(energy, nullptr, &data_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, EncodeAggregateEnergyCountNullDataLen)
{
	uint64_t energy = 9876543210ULL;
	uint8_t data[8] = {0};

	int rc = encode_aggregate_energy_count_data(energy, data, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateEnergyCountValid)
{
	// Use encode to generate correct byte representation
	uint64_t energy_in = 9876543210ULL;
	uint8_t data[8] = {0};
	size_t data_len = 0;
	encode_aggregate_energy_count_data(energy_in, data, &data_len);

	uint64_t energy = 0;
	int rc = decode_aggregate_energy_count_data(data, data_len, &energy);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(energy, 9876543210ULL);
}

TEST(PlatformEnvLine, DecodeAggregateEnergyCountNullData)
{
	uint64_t energy = 0;

	int rc = decode_aggregate_energy_count_data(nullptr, 8, &energy);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateEnergyCountNullEnergy)
{
	uint8_t data[8] = {0xEA, 0xC0, 0xCF, 0x4C, 0x02, 0x00, 0x00, 0x00};

	int rc =
	    decode_aggregate_energy_count_data(data, sizeof(data), nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateEnergyCountInvalidLength)
{
	uint8_t data[4] = {0xEA, 0xC0, 0xCF, 0x4C};
	uint64_t energy = 0;

	int rc =
	    decode_aggregate_energy_count_data(data, sizeof(data), &energy);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ========== Aggregate Voltage Data Tests ==========

TEST(PlatformEnvLine, EncodeAggregateVoltageValid)
{
	uint32_t voltage = 3300; // 3.3V in millivolts
	uint8_t data[4] = {0};
	size_t data_len = 0;

	int rc = encode_aggregate_voltage_data(voltage, data, &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint32_t));
}

TEST(PlatformEnvLine, EncodeAggregateVoltageNullData)
{
	uint32_t voltage = 3300;
	size_t data_len = 0;

	int rc = encode_aggregate_voltage_data(voltage, nullptr, &data_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, EncodeAggregateVoltageNullDataLen)
{
	uint32_t voltage = 3300;
	uint8_t data[4] = {0};

	int rc = encode_aggregate_voltage_data(voltage, data, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateVoltageValid)
{
	uint8_t data[4] = {0xE4, 0x0C, 0x00, 0x00}; // 3300 in little-endian
	uint32_t voltage = 0;

	int rc = decode_aggregate_voltage_data(data, sizeof(data), &voltage);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(voltage, 3300U);
}

TEST(PlatformEnvLine, DecodeAggregateVoltageNullData)
{
	uint32_t voltage = 0;

	int rc = decode_aggregate_voltage_data(nullptr, 4, &voltage);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateVoltageNullVoltage)
{
	uint8_t data[4] = {0xE4, 0x0C, 0x00, 0x00};

	int rc = decode_aggregate_voltage_data(data, sizeof(data), nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateVoltageInvalidLength)
{
	uint8_t data[2] = {0xE4, 0x0C};
	uint32_t voltage = 0;

	int rc = decode_aggregate_voltage_data(data, sizeof(data), &voltage);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ========== Aggregate GPM Metric Percentage Data Tests ==========

TEST(PlatformEnvLine, EncodeAggregateGpmPercentageValid)
{
	double percentage = 85.5;
	uint8_t data[4] = {0};
	size_t data_len = 0;

	int rc = encode_aggregate_gpm_metric_percentage_data(percentage, data,
							     &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint32_t));
}

TEST(PlatformEnvLine, EncodeAggregateGpmPercentageNullData)
{
	double percentage = 85.5;
	size_t data_len = 0;

	int rc = encode_aggregate_gpm_metric_percentage_data(
	    percentage, nullptr, &data_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, EncodeAggregateGpmPercentageNullDataLen)
{
	double percentage = 85.5;
	uint8_t data[4] = {0};

	int rc = encode_aggregate_gpm_metric_percentage_data(percentage, data,
							     nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateGpmPercentageValid)
{
	uint8_t data[4] = {0x80, 0x55, 0x00,
			   0x00}; // 85.5 * 256 = 21888 = 0x5580 (LE)
	double percentage = 0.0;

	int rc = decode_aggregate_gpm_metric_percentage_data(data, sizeof(data),
							     &percentage);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NEAR(percentage, 85.5, 0.1);
}

TEST(PlatformEnvLine, DecodeAggregateGpmPercentageNullData)
{
	double percentage = 0.0;

	int rc = decode_aggregate_gpm_metric_percentage_data(nullptr, 4,
							     &percentage);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateGpmPercentageNullPercentage)
{
	uint8_t data[4] = {0x80, 0x54, 0x00, 0x00};

	int rc = decode_aggregate_gpm_metric_percentage_data(data, sizeof(data),
							     nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateGpmPercentageInvalidLength)
{
	uint8_t data[2] = {0x80, 0x54};
	double percentage = 0.0;

	int rc = decode_aggregate_gpm_metric_percentage_data(data, sizeof(data),
							     &percentage);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ========== Aggregate GPM Metric Bandwidth Data Tests ==========

TEST(PlatformEnvLine, EncodeAggregateGpmBandwidthValid)
{
	uint64_t bandwidth = 10000000000ULL; // 10 Gbps
	uint8_t data[8] = {0};
	size_t data_len = 0;

	int rc = encode_aggregate_gpm_metric_bandwidth_data(bandwidth, data,
							    &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint64_t));
}

TEST(PlatformEnvLine, EncodeAggregateGpmBandwidthNullData)
{
	uint64_t bandwidth = 10000000000ULL;
	size_t data_len = 0;

	int rc = encode_aggregate_gpm_metric_bandwidth_data(bandwidth, nullptr,
							    &data_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, EncodeAggregateGpmBandwidthNullDataLen)
{
	uint64_t bandwidth = 10000000000ULL;
	uint8_t data[8] = {0};

	int rc = encode_aggregate_gpm_metric_bandwidth_data(bandwidth, data,
							    nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateGpmBandwidthValid)
{
	// Use encode to generate correct byte representation
	uint64_t bandwidth_in = 10000000000ULL;
	uint8_t data[8] = {0};
	size_t data_len = 0;
	encode_aggregate_gpm_metric_bandwidth_data(bandwidth_in, data,
						   &data_len);

	uint64_t bandwidth = 0;
	int rc = decode_aggregate_gpm_metric_bandwidth_data(data, data_len,
							    &bandwidth);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(bandwidth, 10000000000ULL);
}

TEST(PlatformEnvLine, DecodeAggregateGpmBandwidthNullData)
{
	uint64_t bandwidth = 0;

	int rc =
	    decode_aggregate_gpm_metric_bandwidth_data(nullptr, 8, &bandwidth);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateGpmBandwidthNullBandwidth)
{
	uint8_t data[8] = {0x00, 0xCA, 0x9A, 0x3B, 0x02, 0x00, 0x00, 0x00};

	int rc = decode_aggregate_gpm_metric_bandwidth_data(data, sizeof(data),
							    nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeAggregateGpmBandwidthInvalidLength)
{
	uint8_t data[4] = {0x00, 0xCA, 0x9A, 0x3B};
	uint64_t bandwidth = 0;

	int rc = decode_aggregate_gpm_metric_bandwidth_data(data, sizeof(data),
							    &bandwidth);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ========== Altitude Pressure Tests ==========

TEST(PlatformEnvLine, EncodeGetAltitudePressureReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));
	auto *msg = reinterpret_cast<nsm_msg *>(requestMsg.data());

	int rc = encode_get_altitude_pressure_req(0, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *req = reinterpret_cast<nsm_common_req *>(msg->payload);
	EXPECT_EQ(req->command, NSM_GET_ALTITUDE_PRESSURE);
}

TEST(PlatformEnvLine, EncodeGetAltitudePressureReqNull)
{
	int rc = encode_get_altitude_pressure_req(0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, EncodeGetAltitudePressureRespValid)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_altitude_pressure_resp));
	auto *msg = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint32_t reading = 101325; // Standard atmospheric pressure in Pa

	int rc =
	    encode_get_altitude_pressure_resp(0, cc, reason_code, reading, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp =
	    reinterpret_cast<nsm_get_altitude_pressure_resp *>(msg->payload);
	EXPECT_EQ(resp->hdr.completion_code, NSM_SUCCESS);
}

TEST(PlatformEnvLine, EncodeGetAltitudePressureRespNull)
{
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint32_t reading = 101325;

	int rc = encode_get_altitude_pressure_resp(0, cc, reason_code, reading,
						   nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeGetAltitudePressureRespValid)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_altitude_pressure_resp));
	auto *msg = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// Encode a valid response to decode
	uint8_t cc_in = NSM_SUCCESS;
	uint16_t reason_in = 0;
	uint32_t reading_in = 101325;
	encode_get_altitude_pressure_resp(0, cc_in, reason_in, reading_in, msg);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint32_t reading = 0;

	int rc = decode_get_altitude_pressure_resp(msg, responseMsg.size(), &cc,
						   &reason_code, &reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 101325U);
}

TEST(PlatformEnvLine, DecodeGetAltitudePressureRespNullReading)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_altitude_pressure_resp));
	auto *msg = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t cc = 0;
	uint16_t reason_code = 0;

	int rc = decode_get_altitude_pressure_resp(msg, responseMsg.size(), &cc,
						   &reason_code, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ========== Leak Detection Info Tests ==========

TEST(PlatformEnvLine, EncodeGetLeakDetectionInfoReqValid)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_leak_detection_info_req));
	auto *msg = reinterpret_cast<nsm_msg *>(requestMsg.data());

	int rc = encode_get_leak_detection_info_req(0, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *req =
	    reinterpret_cast<nsm_get_leak_detection_info_req *>(msg->payload);
	EXPECT_EQ(req->hdr.command, NSM_GET_LEAK_DETECTION_INFO);
}

TEST(PlatformEnvLine, EncodeGetLeakDetectionInfoReqNull)
{
	int rc = encode_get_leak_detection_info_req(0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeGetLeakDetectionInfoReqValid)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_leak_detection_info_req));
	auto *msg = reinterpret_cast<nsm_msg *>(requestMsg.data());

	int rc = decode_get_leak_detection_info_req(msg, requestMsg.size());

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(PlatformEnvLine, DecodeGetLeakDetectionInfoReqNull)
{
	int rc = decode_get_leak_detection_info_req(nullptr, 100);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvLine, DecodeGetLeakDetectionInfoReqInvalidLength)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr)); // Too short
	auto *msg = reinterpret_cast<nsm_msg *>(requestMsg.data());

	int rc = decode_get_leak_detection_info_req(msg, requestMsg.size());

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
