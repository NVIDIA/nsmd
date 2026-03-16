/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "base.h"
#include "platform-environmental.h"

#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

// Tests for encode_get_temperature_reading_req
TEST(PlatformEnvironmentalSensorsTest, EncodeGetTemperatureReadingReqValid)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 5;
	uint8_t sensor_id = 0x12;

	int rc =
	    encode_get_temperature_reading_req(instance_id, sensor_id, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *req =
	    reinterpret_cast<nsm_get_temperature_reading_req *>(msg->payload);
	EXPECT_EQ(req->hdr.command, NSM_GET_TEMPERATURE_READING);
	EXPECT_EQ(req->sensor_id, sensor_id);
}

TEST(PlatformEnvironmentalSensorsTest, EncodeGetTemperatureReadingReqNullMsg)
{
	uint8_t instance_id = 5;
	uint8_t sensor_id = 0x12;

	int rc =
	    encode_get_temperature_reading_req(instance_id, sensor_id, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvironmentalSensorsTest,
     EncodeDecodeGetTemperatureReadingReqRoundTrip)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 7;
	uint8_t sensor_id = 0x34;

	// Encode
	int rc =
	    encode_get_temperature_reading_req(instance_id, sensor_id, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode
	uint8_t decoded_sensor_id = 0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req);

	rc = decode_get_temperature_reading_req(msg, msg_len,
						&decoded_sensor_id);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_sensor_id, sensor_id);
}

// Tests for decode_get_temperature_reading_req
TEST(PlatformEnvironmentalSensorsTest, DecodeGetTemperatureReadingReqNullMsg)
{
	uint8_t sensor_id = 0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req);

	int rc =
	    decode_get_temperature_reading_req(nullptr, msg_len, &sensor_id);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvironmentalSensorsTest,
     DecodeGetTemperatureReadingReqNullSensorId)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req);

	int rc = decode_get_temperature_reading_req(msg, msg_len, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvironmentalSensorsTest,
     DecodeGetTemperatureReadingReqInvalidLength)
{
	std::vector<uint8_t> requestMsg(10);
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t sensor_id = 0;

	int rc = decode_get_temperature_reading_req(msg, 5, &sensor_id);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_get_temperature_reading_resp
TEST(PlatformEnvironmentalSensorsTest,
     EncodeGetTemperatureReadingRespValidSuccess)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 3;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x0000;
	double temperature = 45.5; // degrees Celsius

	int rc = encode_get_temperature_reading_resp(
	    instance_id, cc, reason_code, temperature, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp =
	    reinterpret_cast<struct nsm_get_temperature_reading_resp *>(
		msg->payload);
	EXPECT_EQ(resp->hdr.command, NSM_GET_TEMPERATURE_READING);
	EXPECT_EQ(resp->hdr.completion_code, cc);
}

TEST(PlatformEnvironmentalSensorsTest, EncodeGetTemperatureReadingRespNullMsg)
{
	uint8_t instance_id = 3;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x0000;
	double temperature = 45.5;

	int rc = encode_get_temperature_reading_resp(
	    instance_id, cc, reason_code, temperature, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvironmentalSensorsTest, EncodeGetTemperatureReadingRespErrorCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 3;
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0x1234;
	double temperature = 45.5;

	// When cc != NSM_SUCCESS, function encodes reason_code only
	int rc = encode_get_temperature_reading_resp(
	    instance_id, cc, reason_code, temperature, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp = reinterpret_cast<struct nsm_common_resp *>(msg->payload);
	EXPECT_EQ(resp->completion_code, cc);
}

// Tests for decode_get_temperature_reading_resp
TEST(PlatformEnvironmentalSensorsTest, DecodeGetTemperatureReadingRespValid)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 3;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x0000;
	double temperature = 65.25;

	encode_get_temperature_reading_resp(instance_id, cc, reason_code,
					    temperature, msg);

	uint8_t decoded_cc = 0;
	uint16_t decoded_reason_code = 0;
	double decoded_temperature = 0.0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp);

	int rc = decode_get_temperature_reading_resp(msg, msg_len, &decoded_cc,
						     &decoded_reason_code,
						     &decoded_temperature);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_cc, cc);
	// Temperature reading is encoded as fixed-point with 8 fractional bits
	EXPECT_NEAR(decoded_temperature, temperature, 0.01);
}

TEST(PlatformEnvironmentalSensorsTest,
     DecodeGetTemperatureReadingRespNullTemperature)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp);

	int rc = decode_get_temperature_reading_resp(msg, msg_len, &cc,
						     &reason_code, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for encode_get_current_power_draw_req
TEST(PlatformEnvironmentalSensorsTest, EncodeGetCurrentPowerDrawReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_current_power_draw_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 9;
	uint8_t sensor_id = 0x56;
	uint8_t averaging_interval = 10;

	int rc = encode_get_current_power_draw_req(instance_id, sensor_id,
						   averaging_interval, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *req = reinterpret_cast<struct nsm_get_current_power_draw_req *>(
	    msg->payload);
	EXPECT_EQ(req->hdr.command, NSM_GET_POWER);
	EXPECT_EQ(req->sensor_id, sensor_id);
	EXPECT_EQ(req->averaging_interval, averaging_interval);
}

TEST(PlatformEnvironmentalSensorsTest, EncodeGetCurrentPowerDrawReqNullMsg)
{
	uint8_t instance_id = 9;
	uint8_t sensor_id = 0x56;
	uint8_t averaging_interval = 10;

	int rc = encode_get_current_power_draw_req(instance_id, sensor_id,
						   averaging_interval, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvironmentalSensorsTest,
     EncodeDecodeGetCurrentPowerDrawReqRoundTrip)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_current_power_draw_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 11;
	uint8_t sensor_id = 0x78;
	uint8_t averaging_interval = 20;

	// Encode
	int rc = encode_get_current_power_draw_req(instance_id, sensor_id,
						   averaging_interval, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode
	uint8_t decoded_sensor_id = 0;
	uint8_t decoded_averaging_interval = 0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_req);

	rc = decode_get_current_power_draw_req(msg, msg_len, &decoded_sensor_id,
					       &decoded_averaging_interval);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_sensor_id, sensor_id);
	EXPECT_EQ(decoded_averaging_interval, averaging_interval);
}

// Tests for decode_get_current_power_draw_req
TEST(PlatformEnvironmentalSensorsTest, DecodeGetCurrentPowerDrawReqNullMsg)
{
	uint8_t sensor_id = 0;
	uint8_t averaging_interval = 0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_req);

	int rc = decode_get_current_power_draw_req(nullptr, msg_len, &sensor_id,
						   &averaging_interval);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvironmentalSensorsTest, DecodeGetCurrentPowerDrawReqNullOutputs)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_current_power_draw_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_req);

	int rc =
	    decode_get_current_power_draw_req(msg, msg_len, nullptr, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvironmentalSensorsTest,
     DecodeGetCurrentPowerDrawReqInvalidLength)
{
	std::vector<uint8_t> requestMsg(10);
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t sensor_id = 0;
	uint8_t averaging_interval = 0;

	int rc = decode_get_current_power_draw_req(msg, 5, &sensor_id,
						   &averaging_interval);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_get_current_power_draw_resp
TEST(PlatformEnvironmentalSensorsTest,
     EncodeGetCurrentPowerDrawRespValidSuccess)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 13;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x0000;
	uint32_t reading = 350; // 350 watts

	int rc = encode_get_current_power_draw_resp(instance_id, cc,
						    reason_code, reading, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp =
	    reinterpret_cast<nsm_get_current_power_draw_resp *>(msg->payload);
	EXPECT_EQ(resp->hdr.command, NSM_GET_POWER);
	EXPECT_EQ(resp->hdr.completion_code, cc);
}

TEST(PlatformEnvironmentalSensorsTest, EncodeGetCurrentPowerDrawRespNullMsg)
{
	uint8_t instance_id = 13;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x0000;
	uint32_t reading = 350;

	int rc = encode_get_current_power_draw_resp(
	    instance_id, cc, reason_code, reading, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvironmentalSensorsTest, EncodeGetCurrentPowerDrawRespErrorCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 13;
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0xABCD;
	uint32_t reading = 350;

	// When cc != NSM_SUCCESS, function encodes reason_code only
	int rc = encode_get_current_power_draw_resp(instance_id, cc,
						    reason_code, reading, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp = reinterpret_cast<struct nsm_common_resp *>(msg->payload);
	EXPECT_EQ(resp->completion_code, cc);
}

// Tests for decode_get_current_power_draw_resp
TEST(PlatformEnvironmentalSensorsTest, DecodeGetCurrentPowerDrawRespValid)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 13;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x0000;
	uint32_t reading = 450;

	encode_get_current_power_draw_resp(instance_id, cc, reason_code,
					   reading, msg);

	uint8_t decoded_cc = 0;
	uint16_t decoded_reason_code = 0;
	uint32_t decoded_reading = 0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp);

	int rc = decode_get_current_power_draw_resp(
	    msg, msg_len, &decoded_cc, &decoded_reason_code, &decoded_reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_cc, cc);
	EXPECT_EQ(decoded_reading, reading);
}

TEST(PlatformEnvironmentalSensorsTest, DecodeGetCurrentPowerDrawRespNullReading)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp);

	int rc = decode_get_current_power_draw_resp(msg, msg_len, &cc,
						    &reason_code, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}
