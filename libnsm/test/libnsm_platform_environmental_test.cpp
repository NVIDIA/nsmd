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

#include "base.h"
#include "platform-environmental.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(getInventoryInformation, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));

	uint8_t property_identifier = 0xab;
	uint8_t data_size = sizeof(property_identifier);

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_inventory_information_req(0, property_identifier,
						       request);

	struct nsm_get_inventory_information_req *req =
	    reinterpret_cast<struct nsm_get_inventory_information_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_INVENTORY_INFORMATION, req->hdr.command);
	EXPECT_EQ(data_size, req->hdr.data_size);
	EXPECT_EQ(property_identifier, req->property_identifier);
}

TEST(getInventoryInformation, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));

	auto rc = encode_get_inventory_information_req(0, 0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getInventoryInformation, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_INVENTORY_INFORMATION,   // command
	    1,				     // data size
	    0xab			     // property_identifier
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	uint8_t property_identifier = 0;
	auto rc = decode_get_inventory_information_req(request, msg_len,
						       &property_identifier);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0xab, property_identifier);
}

TEST(getInventoryInformation, testGoodEncodeResponse)
{
	std::vector<uint8_t> board_part_number{'1', '2', '3', '4'};

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 NSM_RESPONSE_CONVENTION_LEN +
					 board_part_number.size());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t *inventory_information = board_part_number.data();
	uint16_t data_size = board_part_number.size();
	uint16_t reason_code = 0;

	auto rc = encode_get_inventory_information_resp(
	    0, NSM_SUCCESS, reason_code, data_size, inventory_information,
	    response);

	struct nsm_get_inventory_information_resp *resp =
	    reinterpret_cast<struct nsm_get_inventory_information_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_INVENTORY_INFORMATION, resp->hdr.command);
	EXPECT_EQ(data_size, le16toh(resp->hdr.data_size));
	EXPECT_EQ('1', resp->inventory_information[0]);
	EXPECT_EQ('2', resp->inventory_information[1]);
	EXPECT_EQ('3', resp->inventory_information[2]);
	EXPECT_EQ('4', resp->inventory_information[3]);
}

TEST(getInventoryInformation, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_INVENTORY_INFORMATION,   // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    '1',
	    '2',
	    '3',
	    '4'};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t inventory_information[4];

	auto rc = decode_get_inventory_information_resp(
	    response, msg_len, &cc, &reason_code, &data_size,
	    inventory_information);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(4, data_size);
	EXPECT_EQ('1', inventory_information[0]);
	EXPECT_EQ('2', inventory_information[1]);
	EXPECT_EQ('3', inventory_information[2]);
	EXPECT_EQ('4', inventory_information[3]);
}

TEST(getTemperature, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t sensor_id = 0;

	auto rc = encode_get_temperature_reading_req(0, sensor_id, request);

	nsm_get_temperature_reading_req *req =
	    reinterpret_cast<nsm_get_temperature_reading_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_TEMPERATURE_READING, req->hdr.command);
	EXPECT_EQ(sizeof(sensor_id), req->hdr.data_size);
	EXPECT_EQ(sensor_id, req->sensor_id);
}

TEST(getTemperature, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    1,				     // data size
	    1				     // sensor_id
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t sensor_id = 0;
	auto rc =
	    decode_get_temperature_reading_req(request, msg_len, &sensor_id);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(sensor_id, 1);
}

TEST(getTemperature, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	double temperature_reading = 12.34;
	uint16_t reasonCode = 0;

	auto rc = encode_get_temperature_reading_resp(
	    0, NSM_SUCCESS, reasonCode, temperature_reading, response);

	struct nsm_get_temperature_reading_resp *resp =
	    reinterpret_cast<struct nsm_get_temperature_reading_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_TEMPERATURE_READING, resp->hdr.command);
	EXPECT_EQ(sizeof(resp->reading), le16toh(resp->hdr.data_size));

	uint32_t data = 0;
	memcpy(&data, &resp->reading, sizeof(uint32_t));
	data = le32toh(data);
	double reading = data / (double)(1 << 8);
	EXPECT_NEAR(temperature_reading, reading, 0.01);
}

TEST(getTemperature, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x0c,
	    0x00,
	    0x00 // temperature reading=12.34
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	double temperature_reading = 0;

	auto rc = decode_get_temperature_reading_resp(
	    response, msg_len, &cc, &reasonCode, &temperature_reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_NEAR(temperature_reading, 12.34, 0.01);
}

TEST(getTemperature, testBadDecodeResponseLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x00 // temperature reading=12.34
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	double temperature_reading = 0;

	auto rc = decode_get_temperature_reading_resp(
	    response, msg_len, &cc, &reasonCode, &temperature_reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_DOUBLE_EQ(temperature_reading, 0);
}

TEST(getTemperature, testBadDecodeResponseNull)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x0c,
	    0x00,
	    0x00 // temperature reading=12.34
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = decode_get_temperature_reading_resp(response, msg_len, &cc,
						      &reasonCode, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	EXPECT_EQ(cc, NSM_ERROR);
}

TEST(getTemperature, testBadDecodeResponseDataLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    2,
	    0, // data size
	    0x57,
	    0x0c,
	    0x00,
	    0x00 // temperature reading=12.34
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	double temperature_reading = 0;

	auto rc = decode_get_temperature_reading_resp(
	    response, msg_len, &cc, &reasonCode, &temperature_reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_DOUBLE_EQ(temperature_reading, 0);
}

TEST(readThermalParameter, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_read_thermal_parameter_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t sensor_id = 0;

	auto rc = encode_read_thermal_parameter_req(0, sensor_id, request);

	nsm_read_thermal_parameter_req *req =
	    reinterpret_cast<nsm_read_thermal_parameter_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_READ_THERMAL_PARAMETER, req->hdr.command);
	EXPECT_EQ(sizeof(sensor_id), req->hdr.data_size);
	EXPECT_EQ(sensor_id, req->parameter_id);
}

TEST(readThermalParameter, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_READ_THERMAL_PARAMETER,	     // command
	    2,				     // data size
	    6,				     // sensor_id
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t sensor_id = 0;

	auto rc =
	    decode_read_thermal_parameter_req(request, msg_len, &sensor_id);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(sensor_id, 6);
}

TEST(readThermalParameter, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_read_thermal_parameter_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	int32_t threshold = -20;
	uint16_t reasonCode = 0;

	auto rc = encode_read_thermal_parameter_resp(0, NSM_SUCCESS, reasonCode,
						     threshold, response);

	struct nsm_read_thermal_parameter_resp *resp =
	    reinterpret_cast<struct nsm_read_thermal_parameter_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_READ_THERMAL_PARAMETER, resp->hdr.command);
	EXPECT_EQ(sizeof(resp->threshold), le16toh(resp->hdr.data_size));
	EXPECT_EQ(threshold, resp->threshold);
}

TEST(readThermalParameter, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_READ_THERMAL_PARAMETER,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x45,
	    0x01,
	    0x00,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	int32_t reading = 0;

	auto rc = decode_read_thermal_parameter_resp(response, msg_len, &cc,
						     &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 325);
}

TEST(readThermalParameter, testBadDecodeResponseLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_READ_THERMAL_PARAMETER,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	int32_t reading = 0;

	auto rc = decode_read_thermal_parameter_resp(response, msg_len, &cc,
						     &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(readThermalParameter, testBadDecodeResponseNull)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = decode_read_thermal_parameter_resp(response, msg_len, &cc,
						     &reasonCode, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	EXPECT_EQ(cc, NSM_ERROR);
}

TEST(readThermalParameter, testBadDecodeResponseDataLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    3,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	int32_t reading = 0;

	auto rc = decode_read_thermal_parameter_resp(response, msg_len, &cc,
						     &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getCurrentPowerDraw, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_current_power_draw_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t sensor_id = 0;
	uint8_t averaging_interval = 0;

	auto rc = encode_get_current_power_draw_req(
	    0, sensor_id, averaging_interval, request);

	struct nsm_get_current_power_draw_req *req =
	    reinterpret_cast<struct nsm_get_current_power_draw_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_POWER, req->hdr.command);
	EXPECT_EQ(sizeof(sensor_id) + sizeof(averaging_interval),
		  req->hdr.data_size);
	EXPECT_EQ(sensor_id, req->sensor_id);
	EXPECT_EQ(averaging_interval, req->averaging_interval);
}

TEST(getCurrentPowerDraw, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_POWER,		     // command
	    2,				     // data size
	    1,				     // sensor_id
	    1				     // averaging_interval
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t sensor_id = 0;
	uint8_t averaging_interval = 0;

	auto rc = decode_get_current_power_draw_req(
	    request, msg_len, &sensor_id, &averaging_interval);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(sensor_id, 1);
	EXPECT_EQ(averaging_interval, 1);
}

TEST(getCurrentPowerDraw, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint32_t reading = 12456;
	uint16_t reasonCode = 0;

	auto rc = encode_get_current_power_draw_resp(0, NSM_SUCCESS, reasonCode,
						     reading, response);

	nsm_get_current_power_draw_resp *resp =
	    reinterpret_cast<nsm_get_current_power_draw_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_POWER, resp->hdr.command);
	EXPECT_EQ(sizeof(resp->reading), le16toh(resp->hdr.data_size));
	EXPECT_EQ(reading, resp->reading);
}

TEST(getCurrentPowerDraw, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_current_power_draw_resp(response, msg_len, &cc,
						     &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 4203351);
}

TEST(getCurrentPowerDraw, testBadDecodeResponseLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_current_power_draw_resp(response, msg_len, &cc,
						     &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getCurrentPowerDraw, testBadDecodeResponseNull)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = decode_get_current_power_draw_resp(response, msg_len, &cc,
						     &reasonCode, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	EXPECT_EQ(cc, NSM_ERROR);
}

TEST(getCurrentPowerDraw, testBadDecodeResponseDataLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    3,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_current_power_draw_resp(response, msg_len, &cc,
						     &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getMaxObservedPower, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_max_observed_power_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t sensor_id = 0;
	uint8_t averaging_interval = 0;

	auto rc = encode_get_max_observed_power_req(
	    0, sensor_id, averaging_interval, request);

	nsm_get_max_observed_power_req *req =
	    reinterpret_cast<nsm_get_max_observed_power_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_MAX_OBSERVED_POWER, req->hdr.command);
	EXPECT_EQ(sizeof(sensor_id) + sizeof(averaging_interval),
		  req->hdr.data_size);
	EXPECT_EQ(sensor_id, req->sensor_id);
	EXPECT_EQ(averaging_interval, req->averaging_interval);
}

TEST(getMaxObservedPower, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MAX_OBSERVED_POWER,	     // command
	    2,				     // data size
	    1,				     // sensor_id
	    1				     // averaging_interval
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t sensor_id = 0;
	uint8_t averaging_interval = 0;

	auto rc = decode_get_max_observed_power_req(
	    request, msg_len, &sensor_id, &averaging_interval);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(sensor_id, 1);
	EXPECT_EQ(averaging_interval, 1);
}

TEST(getMaxObservedPower, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint32_t reading = 12456;
	uint16_t reasonCode = 0;

	auto rc = encode_get_max_observed_power_resp(0, NSM_SUCCESS, reasonCode,
						     reading, response);

	nsm_get_max_observed_power_resp *resp =
	    reinterpret_cast<nsm_get_max_observed_power_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_MAX_OBSERVED_POWER, resp->hdr.command);
	EXPECT_EQ(sizeof(resp->reading), le16toh(resp->hdr.data_size));
	EXPECT_EQ(reading, resp->reading);
}

TEST(getMaxObservedPower, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MAX_OBSERVED_POWER,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_max_observed_power_resp(response, msg_len, &cc,
						     &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 4203351);
}

TEST(getMaxObservedPower, testBadDecodeResponseLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MAX_OBSERVED_POWER,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_max_observed_power_resp(response, msg_len, &cc,
						     &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getMaxObservedPower, testBadDecodeResponseNull)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MAX_OBSERVED_POWER,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = decode_get_max_observed_power_resp(response, msg_len, &cc,
						     &reasonCode, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	EXPECT_EQ(cc, NSM_ERROR);
}

TEST(getMaxObservedPower, testBadDecodeResponseDataLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MAX_OBSERVED_POWER,	     // command
	    0,				     // completion code
	    0,
	    0,
	    3,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_max_observed_power_resp(response, msg_len, &cc,
						     &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getCurrentEnergyCount, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t sensor_id = 0;

	auto rc = encode_get_current_energy_count_req(0, sensor_id, request);

	nsm_get_current_energy_count_req *req =
	    reinterpret_cast<nsm_get_current_energy_count_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ENERGY_COUNT, req->hdr.command);
	EXPECT_EQ(sizeof(sensor_id), req->hdr.data_size);
	EXPECT_EQ(sensor_id, req->sensor_id);
}

TEST(getCurrentEnergyCount, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ENERGY_COUNT,	     // command
	    1,				     // data size
	    0,				     // sensor_id
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t sensor_id = 0;

	auto rc =
	    decode_get_current_energy_count_req(request, msg_len, &sensor_id);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(sensor_id, 0);
}

TEST(getCurrentEnergyCount, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint64_t reading = 38957248;
	uint16_t reasonCode = 0;

	auto rc = encode_get_current_energy_count_resp(
	    0, NSM_SUCCESS, reasonCode, reading, response);

	struct nsm_get_current_energy_count_resp *resp =
	    reinterpret_cast<struct nsm_get_current_energy_count_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ENERGY_COUNT, resp->hdr.command);
	EXPECT_EQ(sizeof(resp->reading), le16toh(resp->hdr.data_size));
	EXPECT_EQ(reading, resp->reading);
}

TEST(getCurrentEnergyCount, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ENERGY_COUNT,	     // command
	    0,				     // completion code
	    0,
	    0,
	    8,
	    0, // data size
	    0x57,
	    0x34,
	    0x49,
	    0x23,
	    0x03,
	    0x00,
	    0x00,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint64_t reading = 0;

	auto rc = decode_get_current_energy_count_resp(response, msg_len, &cc,
						       &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 13476901975);
}

TEST(getCurrentEnergyCount, testBadDecodeResponseLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ENERGY_COUNT,	     // command
	    0,				     // completion code
	    0,
	    0,
	    8,
	    0, // data size
	    0x57,
	    0x34,
	    0x23,
	    0x03,
	    0x00,
	    0x00,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint64_t reading = 0;

	auto rc = decode_get_current_energy_count_resp(response, msg_len, &cc,
						       &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getCurrentEnergyCount, testBadDecodeResponseNull)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ENERGY_COUNT,	     // command
	    0,				     // completion code
	    0,
	    0,
	    8,
	    0, // data size
	    0x57,
	    0x34,
	    0x49,
	    0x23,
	    0x03,
	    0x00,
	    0x00,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = decode_get_current_energy_count_resp(response, msg_len, &cc,
						       &reasonCode, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	EXPECT_EQ(cc, NSM_ERROR);
}

TEST(getCurrentEnergyCount, testBadDecodeResponseDataLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ENERGY_COUNT,	     // command
	    0,				     // completion code
	    0,
	    0,
	    7,
	    0, // data size
	    0x57,
	    0x34,
	    0x49,
	    0x23,
	    0x03,
	    0x00,
	    0x00,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint64_t reading = 0;

	auto rc = decode_get_current_energy_count_resp(response, msg_len, &cc,
						       &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getVoltage, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_voltage_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t sensor_id = 1;

	auto rc = encode_get_voltage_req(0, sensor_id, request);

	nsm_get_voltage_req *req =
	    reinterpret_cast<nsm_get_voltage_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_VOLTAGE, req->hdr.command);
	EXPECT_EQ(sizeof(sensor_id), req->hdr.data_size);
	EXPECT_EQ(sensor_id, req->sensor_id);
}

TEST(getVoltage, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_VOLTAGE,		     // command
	    1,				     // data size
	    5,				     // sensor_id
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t sensor_id = 0;

	auto rc = decode_get_voltage_req(request, msg_len, &sensor_id);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(sensor_id, 5);
}

TEST(getVoltage, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_get_voltage_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint32_t reading = 23442;
	uint16_t reasonCode = 0;

	auto rc = encode_get_voltage_resp(0, NSM_SUCCESS, reasonCode, reading,
					  response);

	nsm_get_voltage_resp *resp =
	    reinterpret_cast<nsm_get_voltage_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_VOLTAGE, resp->hdr.command);
	EXPECT_EQ(sizeof(resp->reading), le16toh(resp->hdr.data_size));
	EXPECT_EQ(reading, resp->reading);
}

TEST(getVoltage, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ENERGY_COUNT,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x48,
	    0x29,
	    0x03,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_voltage_resp(response, msg_len, &cc, &reasonCode,
					  &reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 207176);
}

TEST(getVoltage, testBadDecodeResponseLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ENERGY_COUNT,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x48,
	    0x03,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_voltage_resp(response, msg_len, &cc, &reasonCode,
					  &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getVoltage, testBadDecodeResponseNull)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ENERGY_COUNT,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x48,
	    0x29,
	    0x03,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = decode_get_voltage_resp(response, msg_len, &cc, &reasonCode,
					  nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	EXPECT_EQ(cc, NSM_ERROR);
}

TEST(getVoltage, testBadDecodeResponseDataLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ENERGY_COUNT,	     // command
	    0,				     // completion code
	    0,
	    0,
	    2,
	    0, // data size
	    0x48,
	    0x29,
	    0x03,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_voltage_resp(response, msg_len, &cc, &reasonCode,
					  &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getAltitudePressure, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_altitude_pressure_req(0, request);

	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ALTITUDE_PRESSURE, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getAltitudePressure, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_altitude_pressure_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint32_t reading = 38380;
	uint16_t reasonCode = 0;

	auto rc = encode_get_altitude_pressure_resp(0, NSM_SUCCESS, reasonCode,
						    reading, response);

	nsm_get_altitude_pressure_resp *resp =
	    reinterpret_cast<nsm_get_altitude_pressure_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ALTITUDE_PRESSURE, resp->hdr.command);
	EXPECT_EQ(sizeof(resp->reading), le16toh(resp->hdr.data_size));
	EXPECT_EQ(reading, resp->reading);
}

TEST(getAltitudePressure, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ALTITUDE_PRESSURE,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_altitude_pressure_resp(response, msg_len, &cc,
						    &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 4203351);
}

TEST(getAltitudePressure, testBadDecodeResponseLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ALTITUDE_PRESSURE,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_altitude_pressure_resp(response, msg_len, &cc,
						    &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getAltitudePressure, testBadDecodeResponseNull)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ALTITUDE_PRESSURE,	     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = decode_get_altitude_pressure_resp(response, msg_len, &cc,
						    &reasonCode, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	EXPECT_EQ(cc, NSM_ERROR);
}

TEST(getAltitudePressure, testBadDecodeResponseDataLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ALTITUDE_PRESSURE,	     // command
	    0,				     // completion code
	    0,
	    0,
	    3,
	    0, // data size
	    0x57,
	    0x23,
	    0x40,
	    0x00 // reading
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t reading = 0;

	auto rc = decode_get_altitude_pressure_resp(response, msg_len, &cc,
						    &reasonCode, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reading, 0);
}

TEST(getDriverInfo, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_driver_info_req(0, request);

	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_DRIVER_INFO, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getDriverInfo, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_DRIVER_INFO,	     // command
	    0				     // data size
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();
	auto rc = decode_get_driver_info_req(request, msg_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getDriverInfo, testGoodEncodeResponse)
{
	// Prepare mock driver info data
	std::string data = "Mock";
	std::vector<uint8_t> driver_info_data;
	driver_info_data.resize(data.length() +
				2); // +2 for state and null string
	driver_info_data[0] = 2;    // driver state
	int index = 1;

	for (char &c : data) {
		driver_info_data[index++] = static_cast<uint8_t>(c);
	}

	driver_info_data[data.length() + 1] = static_cast<uint8_t>('\0');

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					     NSM_RESPONSE_CONVENTION_LEN +
					     driver_info_data.size(),
					 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = 0;

	auto rc = encode_get_driver_info_resp(
	    0, NSM_SUCCESS, reason_code, driver_info_data.size(),
	    (uint8_t *)driver_info_data.data(), response);

	struct nsm_get_driver_info_resp *resp =
	    reinterpret_cast<struct nsm_get_driver_info_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_DRIVER_INFO, resp->hdr.command);
	EXPECT_EQ(6, le16toh(resp->hdr.data_size));
	EXPECT_EQ(2, resp->driver_state);
	size_t driver_version_length = (resp->hdr.data_size - 1);
	char driverVersion[10] = {0};
	memcpy(driverVersion, resp->driver_version, driver_version_length);
	std::string version(driverVersion);
	EXPECT_STREQ("Mock", version.c_str());
}

TEST(getDriverInfo, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_DRIVER_INFO,	     // command
	    0,				     // completion code
	    0,
	    0,
	    6,
	    0, // data size
	    2,
	    'M',
	    'o',
	    'c',
	    'k',
	    '\0'};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	enum8 driverState = 0;
	char driverVersion[MAX_VERSION_STRING_SIZE] = {0};

	auto rc = decode_get_driver_info_resp(
	    response, msg_len, &cc, &reason_code, &driverState, driverVersion);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(2, driverState);
	std::string version(driverVersion);
	EXPECT_STREQ("Mock", version.c_str());
}

TEST(getDriverInfo, testNullDriverStatePointerDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_DRIVER_INFO,	     // command
	    0,				     // completion code
	    0,
	    0,
	    6,
	    0, // data size
	    2,
	    'M',
	    'o',
	    'c',
	    'k',
	    '\0'};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code;
	char driverVersion[MAX_VERSION_STRING_SIZE];

	// Attempt to decode with a null pointer for driver_state
	auto rc = decode_get_driver_info_resp(
	    response, msg_len, &cc, &reason_code, nullptr, driverVersion);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getDriverInfo, testNullDriverVersionPointerDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_DRIVER_INFO,	     // command
	    0,				     // completion code
	    0,
	    0,
	    6,
	    0, // data size
	    2,
	    'M',
	    'o',
	    'c',
	    'k',
	    '\0'};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code;
	enum8 driverState = 0;

	// Attempt to decode with a null pointer for driver_version
	auto rc = decode_get_driver_info_resp(
	    response, msg_len, &cc, &reason_code, &driverState, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getDriverInfo, testDriverVersionNotNullTerminatedDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_DRIVER_INFO,	     // command
	    0,				     // completion code
	    0,
	    0,
	    6,
	    0, // data size
	    2,
	    'M',
	    'o',
	    'c',
	    'k',
	    '!'};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code;
	enum8 driverState = 0;
	char driverVersion[MAX_VERSION_STRING_SIZE];

	auto rc = decode_get_driver_info_resp(
	    response, msg_len, &cc, &reason_code, &driverState, driverVersion);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getDriverInfo, testDriverVersionExceedsMaxSizeDecodeResponse)
{
	// Initialize a response message vector with enough space for headers
	// and a too-long driver version string
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_DRIVER_INFO,	     // command
	    0,				     // completion code
	    0,
	    0,
	    110,
	    0};

	responseMsg.push_back(2); // Driver state

	// Generate a driver version string that is MAX_VERSION_STRING_SIZE + 10
	// character too long
	for (int i = 0; i <= MAX_VERSION_STRING_SIZE; ++i) {
		responseMsg.push_back('A'); // Filling the buffer with 'A's
	}

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code;
	enum8 driverState = 0;
	char driverVersion[MAX_VERSION_STRING_SIZE + 10];

	auto rc = decode_get_driver_info_resp(
	    response, msg_len, &cc, &reason_code, &driverState, driverVersion);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getMigMode, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_MIG_mode_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_MIG_MODE, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getMigMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_MIG_mode_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	bitfield8_t flags;
	flags.byte = 1;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_MIG_mode_resp(0, NSM_SUCCESS, reason_code, &flags,
					   response);

	struct nsm_get_MIG_mode_resp *resp =
	    reinterpret_cast<struct nsm_get_MIG_mode_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_MIG_MODE, resp->hdr.command);
	EXPECT_EQ(sizeof(bitfield8_t), le16toh(resp->hdr.data_size));
	EXPECT_EQ(1, resp->flags.byte);
}

TEST(getMigMode, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MIG_MODE,		     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    1,
	    0, // data size
	    1};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	bitfield8_t flags;

	auto rc = decode_get_MIG_mode_resp(response, msg_len, &cc, &data_size,
					   &reason_code, &flags);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(1, data_size);
	EXPECT_EQ(1, flags.byte);
}

TEST(getMigMode, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MIG_MODE,		     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0, // data size. Here it should not be 0. Negative case
	    0, // data size
	    1};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	bitfield8_t flags;

	auto rc = decode_get_MIG_mode_resp(NULL, msg_len, &cc, &data_size,
					   &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_MIG_mode_resp(response, msg_len, NULL, &data_size,
				      &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_MIG_mode_resp(response, msg_len, &cc, NULL,
				      &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_MIG_mode_resp(response, msg_len - 1, &cc, &data_size,
				      &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_MIG_mode_resp(response, msg_len, &cc, &data_size,
				      &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(setMigMode, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_set_MIG_mode_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t requested_mode = 1;
	auto rc = encode_set_MIG_mode_req(0, requested_mode, request);
	struct nsm_set_MIG_mode_req *req =
	    reinterpret_cast<struct nsm_set_MIG_mode_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_MIG_MODE, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(requested_mode, req->requested_mode);
}

TEST(setMigMode, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_MIG_MODE,		     // command
	    1,				     // data size
	    1				     // requested Mode
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t requested_mode;
	auto rc = decode_set_MIG_mode_req(request, msg_len, &requested_mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, requested_mode);
}

TEST(setMigMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    encode_set_MIG_mode_resp(0, NSM_SUCCESS, reason_code, response);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_MIG_MODE, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(setMigMode, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_MIG_MODE,		     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_MIG_mode_resp(response, msg_len, &cc, &data_size,
					   &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0, data_size);
}

TEST(setMigMode, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MIG_MODE,		     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0,				     // data size
	    0				     // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_MIG_mode_resp(NULL, msg_len, &cc, &data_size,
					   &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_MIG_mode_resp(response, msg_len, NULL, &data_size,
				      &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_MIG_mode_resp(response, msg_len, &cc, NULL,
				      &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_MIG_mode_resp(response, msg_len - 1, &cc, &data_size,
				      &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getEccMode, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_ECC_mode_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ECC_MODE, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getEccMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_mode_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	bitfield8_t flags;
	flags.byte = 1;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_ECC_mode_resp(0, NSM_SUCCESS, reason_code, &flags,
					   response);

	struct nsm_get_ECC_mode_resp *resp =
	    reinterpret_cast<struct nsm_get_ECC_mode_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ECC_MODE, resp->hdr.command);
	EXPECT_EQ(1, le16toh(resp->hdr.data_size));
	EXPECT_EQ(1, resp->flags.byte);
}

TEST(getEccMode, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ECC_MODE,		     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    1,
	    0, // data size
	    1  // data
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	bitfield8_t flags;

	auto rc = decode_get_ECC_mode_resp(response, msg_len, &cc, &data_size,
					   &reason_code, &flags);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(1, data_size);
	EXPECT_EQ(1, flags.byte);
}

TEST(getEccMode, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ECC_MODE,		     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0, // data size. Here it should not be 0. Negative case
	    0, // data size
	    1};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	bitfield8_t flags;

	auto rc = decode_get_ECC_mode_resp(NULL, msg_len, &cc, &data_size,
					   &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_ECC_mode_resp(response, msg_len, NULL, &data_size,
				      &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_ECC_mode_resp(response, msg_len, &cc, NULL,
				      &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_ECC_mode_resp(response, msg_len - 1, &cc, &data_size,
				      &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_ECC_mode_resp(response, msg_len, &cc, &data_size,
				      &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(setEccMode, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_set_ECC_mode_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t requested_mode = 1;
	auto rc = encode_set_ECC_mode_req(0, requested_mode, request);
	struct nsm_set_ECC_mode_req *req =
	    reinterpret_cast<struct nsm_set_ECC_mode_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_ECC_MODE, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(requested_mode, req->requested_mode);
}

TEST(setEccMode, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_ECC_MODE,		     // command
	    1,				     // data size
	    1				     // requested Mode
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t requested_mode;
	auto rc = decode_set_ECC_mode_req(request, msg_len, &requested_mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, requested_mode);
}

TEST(setEccMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    encode_set_ECC_mode_resp(0, NSM_SUCCESS, reason_code, response);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_ECC_MODE, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(setEccMode, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_ECC_MODE,		     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_ECC_mode_resp(response, msg_len, &cc, &data_size,
					   &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0, data_size);
}

TEST(setEccMode, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_ECC_MODE,		     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0,				     // data size
	    0				     // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_ECC_mode_resp(NULL, msg_len, &cc, &data_size,
					   &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_ECC_mode_resp(response, msg_len, NULL, &data_size,
				      &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_ECC_mode_resp(response, msg_len, &cc, NULL,
				      &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_ECC_mode_resp(response, msg_len - 1, &cc, &data_size,
				      &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getEccErrorCounts, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_ECC_error_counts_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ECC_ERROR_COUNTS, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getEccErrorCounts, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_error_counts_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	struct nsm_ECC_error_counts errorCounts;
	errorCounts.flags.byte = 132;
	errorCounts.sram_corrected = 1234;
	errorCounts.sram_uncorrected_secded = 4532;
	errorCounts.sram_uncorrected_parity = 6567;
	errorCounts.dram_corrected = 9876;
	errorCounts.dram_uncorrected = 9654;

	uint16_t reason_code = ERR_NULL;
	struct nsm_ECC_error_counts errorCounts_test = errorCounts;
	auto rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, reason_code,
						   &errorCounts, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_get_ECC_error_counts_resp *resp =
	    reinterpret_cast<struct nsm_get_ECC_error_counts_resp *>(
		response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ECC_ERROR_COUNTS, resp->hdr.command);
	EXPECT_EQ(sizeof(struct nsm_ECC_error_counts),
		  le16toh(resp->hdr.data_size));

	EXPECT_EQ(le32toh(errorCounts.flags.byte), errorCounts_test.flags.byte);
	EXPECT_EQ(le32toh(errorCounts.dram_corrected),
		  errorCounts_test.dram_corrected);
}

TEST(getEccErrorCounts, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_orig{
	    0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00, 0x02, 0x03, 0x0B,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x07, 0x08, 0x09, 0x0A};

	struct nsm_ECC_error_counts *errorCounts =
	    reinterpret_cast<struct nsm_ECC_error_counts *>(data_orig.data());

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ECC_ERROR_COUNTS,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    22,
	    0 // data size
	};

	responseMsg.insert(responseMsg.end(), data_orig.begin(),
			   data_orig.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_ECC_error_counts errorCounts_test;

	auto rc =
	    decode_get_ECC_error_counts_resp(response, msg_len, &cc, &data_size,
					     &reason_code, &errorCounts_test);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(22, data_size);
	EXPECT_EQ(htole16(errorCounts->flags.byte),
		  errorCounts_test.flags.byte);
	EXPECT_EQ(htole32(errorCounts->sram_corrected),
		  errorCounts_test.sram_corrected);
}

TEST(getEccErrorCounts, testBadDecodeResponse)
{
	std::vector<uint8_t> data_orig{
	    0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00, 0x02, 0x03, 0x0B,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x07, 0x08, 0x09, 0x0A};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ECC_ERROR_COUNTS,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    20, // data size should be 22. Negative test
	    0	// data size
	};

	responseMsg.insert(responseMsg.end(), data_orig.begin(),
			   data_orig.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_ECC_error_counts errorCounts_test;

	auto rc = decode_get_ECC_error_counts_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &errorCounts_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_ECC_error_counts_resp(response, msg_len, NULL,
					      &data_size, &reason_code,
					      &errorCounts_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_ECC_error_counts_resp(response, msg_len, &cc, NULL,
					      &reason_code, &errorCounts_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_ECC_error_counts_resp(response, msg_len - 1, &cc,
					      &data_size, &reason_code,
					      &errorCounts_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc =
	    decode_get_ECC_error_counts_resp(response, msg_len, &cc, &data_size,
					     &reason_code, &errorCounts_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getEDPpScalingFactor, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_programmable_EDPp_scaling_factor_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getEDPpScalingFactor, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_programmable_EDPp_scaling_factor_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;
	struct nsm_EDPp_scaling_factors scaling_factors;
	scaling_factors.default_scaling_factor = 70;
	scaling_factors.maximum_scaling_factor = 90;
	scaling_factors.minimum_scaling_factor = 60;

	struct nsm_EDPp_scaling_factors scaling_factors_test = scaling_factors;

	auto rc = encode_get_programmable_EDPp_scaling_factor_resp(
	    0, NSM_SUCCESS, reason_code, &scaling_factors_test, response);

	struct nsm_get_programmable_EDPp_scaling_factor_resp *resp =
	    reinterpret_cast<
		struct nsm_get_programmable_EDPp_scaling_factor_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR, resp->hdr.command);
	EXPECT_EQ(sizeof(struct nsm_EDPp_scaling_factors),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(scaling_factors.default_scaling_factor,
		  resp->scaling_factors.default_scaling_factor);
}

TEST(getEDPpScalingFactor, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR, // command
	    0,					      // completion code
	    0,					      // reserved
	    0,					      // reserved
	    3,
	    0, // data size
	    1, // data
	    5,
	    3};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_EDPp_scaling_factors scaling_factors;

	auto rc = decode_get_programmable_EDPp_scaling_factor_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &scaling_factors);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(3, data_size);
	EXPECT_EQ(scaling_factors.default_scaling_factor, 1);
	EXPECT_EQ(scaling_factors.minimum_scaling_factor, 3);
}

TEST(getEDPpScalingFactor, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR, // command
	    0,					      // completion code
	    0,					      // reserved
	    0,					      // reserved
	    2, // wrong data size for test case
	    0, // data size
	    1, // data
	    5,
	    3};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_EDPp_scaling_factors scaling_factors;

	auto rc = decode_get_programmable_EDPp_scaling_factor_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &scaling_factors);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_programmable_EDPp_scaling_factor_resp(
	    response, msg_len, NULL, &data_size, &reason_code,
	    &scaling_factors);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_programmable_EDPp_scaling_factor_resp(
	    response, msg_len, &cc, NULL, &reason_code, &scaling_factors);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_programmable_EDPp_scaling_factor_resp(
	    response, msg_len - 1, &cc, &data_size, &reason_code,
	    &scaling_factors);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_programmable_EDPp_scaling_factor_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &scaling_factors);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getClockLimit, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_clock_limit_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t clock_id = 0;
	auto rc = encode_get_clock_limit_req(0, clock_id, request);
	struct nsm_get_clock_limit_req *req =
	    reinterpret_cast<struct nsm_get_clock_limit_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_CLOCK_LIMIT, req->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), req->hdr.data_size);
}

TEST(getClockLimit, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CLOCK_LIMIT,	     // command
	    1,				     // data size
	    1				     // clock_id
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t clock_id;
	auto rc = decode_get_clock_limit_req(request, msg_len, &clock_id);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getClockLimit, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_clock_limit_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	struct nsm_clock_limit clock_limit;
	clock_limit.present_limit_max = 200;
	clock_limit.present_limit_min = 100;
	clock_limit.requested_limit_max = 170;
	clock_limit.requested_limit_min = 120;

	uint16_t reason_code = ERR_NULL;
	struct nsm_clock_limit clock_limit_test = clock_limit;
	auto rc = encode_get_clock_limit_resp(0, NSM_SUCCESS, reason_code,
					      &clock_limit, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_get_clock_limit_resp *resp =
	    reinterpret_cast<struct nsm_get_clock_limit_resp *>(
		response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_CLOCK_LIMIT, resp->hdr.command);
	EXPECT_EQ(sizeof(struct nsm_clock_limit), le16toh(resp->hdr.data_size));

	EXPECT_EQ(le32toh(resp->clock_limit.present_limit_max),
		  clock_limit_test.present_limit_max);
}

TEST(getClockLimit, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	    0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	struct nsm_clock_limit *clockLimit =
	    reinterpret_cast<struct nsm_clock_limit *>(data_byte.data());

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CLOCK_LIMIT,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    16,
	    0 // data size
	};

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_clock_limit clockLimitTest;

	auto rc = decode_get_clock_limit_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &clockLimitTest);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(16, data_size);
	EXPECT_EQ(le32toh(clockLimit->present_limit_max),
		  clockLimitTest.present_limit_max);
}

TEST(getClockLimit, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	    0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CLOCK_LIMIT,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    14,				     // wrong data size
	    0				     // data size
	};

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_clock_limit clockLimitTest;

	auto rc = decode_get_clock_limit_resp(NULL, msg_len, &cc, &data_size,
					      &reason_code, &clockLimitTest);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_clock_limit_resp(response, msg_len, NULL, &data_size,
					 &reason_code, &clockLimitTest);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_clock_limit_resp(response, msg_len, &cc, NULL,
					 &reason_code, &clockLimitTest);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_clock_limit_resp(response, msg_len - 1, &cc, &data_size,
					 &reason_code, &clockLimitTest);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_clock_limit_resp(response, msg_len, &cc, &data_size,
					 &reason_code, &clockLimitTest);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(setClockLimit, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_set_clock_limit_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t clock_id = 0;
	uint8_t flags = 11;
	uint32_t limit_max = 40;
	uint32_t limit_min = 29;
	auto rc = encode_set_clock_limit_req(0, clock_id, flags, limit_min, limit_max, request);
	struct nsm_set_clock_limit_req *req =
	    reinterpret_cast<struct nsm_set_clock_limit_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_CLOCK_LIMIT, req->hdr.command);
	EXPECT_EQ(10, req->hdr.data_size);
	EXPECT_EQ(limit_max, le32toh(req->limit_max));
    EXPECT_EQ(limit_min, le32toh(req->limit_min));
}

TEST(setClockLimit, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_CLOCK_LIMIT,	     // command
	    10,				     // data size
	    01,
	    02,
	    14,
	    00,
	    00,
	    00,
	    28,
	    00,
	    00,
	    00,
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t clock_id;
	uint8_t flags;
	uint32_t limit_min;
	uint32_t limit_max;
	auto rc = decode_set_clock_limit_req(request, msg_len, &clock_id,
					     &flags, &limit_min, &limit_max);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(28, limit_max);
	EXPECT_EQ(14, limit_min);
}

TEST(setClockLimit, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    encode_set_clock_limit_resp(0, NSM_SUCCESS, reason_code, response);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_CLOCK_LIMIT, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(setClockLimit, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_CLOCK_LIMIT,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_clock_limit_resp(response, msg_len, &cc,
					      &data_size, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0, data_size);
}

TEST(setClockLimit, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_CLOCK_LIMIT,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0,				     // data size
	    0				     // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_clock_limit_resp(NULL, msg_len, &cc, &data_size,
					      &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_clock_limit_resp(response, msg_len, NULL, &data_size,
					 &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_clock_limit_resp(response, msg_len, &cc, NULL,
					 &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_clock_limit_resp(response, msg_len - 1, &cc, &data_size,
					 &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getCurrClockFreq, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_curr_clock_freq_req));
	uint8_t clock_id = GRAPHICS_CLOCK;
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_get_curr_clock_freq_req(0, clock_id, request);
	struct nsm_get_curr_clock_freq_req *req =
	    reinterpret_cast<struct nsm_get_curr_clock_freq_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_CURRENT_CLOCK_FREQUENCY, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
}

TEST(getCurrClockFreq, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CURRENT_CLOCK_FREQUENCY, // command
	    1,				     // data size
	    1				     // clock_id
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t clock_id;
	auto rc = decode_get_curr_clock_freq_req(request, msg_len, &clock_id);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getCurrClockFreq, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_curr_clock_freq_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint32_t clockFreq = 2000;
	uint32_t clockFreq_test = clockFreq;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, reason_code,
						  &clockFreq, response);

	struct nsm_get_curr_clock_freq_resp *resp =
	    reinterpret_cast<struct nsm_get_curr_clock_freq_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_CURRENT_CLOCK_FREQUENCY, resp->hdr.command);
	EXPECT_EQ(4, le16toh(resp->hdr.data_size));
	EXPECT_EQ(clockFreq_test, le32toh(resp->clockFreq));
}

TEST(getCurrClockFreq, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01,
	    0x0A,
	    0x00,
	    0x01,
	};

	uint32_t *clockFreq = reinterpret_cast<uint32_t *>(data_byte.data());
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CURRENT_CLOCK_FREQUENCY, // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    4,
	    0 // data size
	};

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t clockFreq_test;

	auto rc = decode_get_curr_clock_freq_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &clockFreq_test);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(4, data_size);
	EXPECT_EQ(le32toh(*clockFreq), clockFreq_test);
}

TEST(getCurrClockFreq, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01,
	    0x0A,
	    0x00,
	    0x01,
	};

	// uint32_t* clockFreq = reinterpret_cast<uint32_t* >(data_byte.data());
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CURRENT_CLOCK_FREQUENCY, // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    2,				     // wrong data size
	    0				     // data size
	};

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t clockFreq_test;

	auto rc = decode_get_curr_clock_freq_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &clockFreq_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_curr_clock_freq_resp(
	    response, msg_len, NULL, &data_size, &reason_code, &clockFreq_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_curr_clock_freq_resp(response, msg_len, &cc, NULL,
					     &reason_code, &clockFreq_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_curr_clock_freq_resp(response, msg_len - 1, &cc,
					     &data_size, &reason_code,
					     &clockFreq_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_curr_clock_freq_resp(response, msg_len, &cc, &data_size,
					     &reason_code, &clockFreq_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getClockEventReason, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_current_clock_event_reason_code_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_CLOCK_EVENT_REASON_CODES, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getClockEventReason, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_current_clock_event_reason_code_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	bitfield32_t flags;
	flags.byte = 3;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_current_clock_event_reason_code_resp(
	    0, NSM_SUCCESS, reason_code, &flags, response);

	struct nsm_get_current_clock_event_reason_code_resp *resp =
	    reinterpret_cast<
		struct nsm_get_current_clock_event_reason_code_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_CLOCK_EVENT_REASON_CODES, resp->hdr.command);
	EXPECT_EQ(sizeof(bitfield32_t), le16toh(resp->hdr.data_size));
	EXPECT_EQ(3, resp->flags.byte);
}

TEST(getClockEventReason, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CLOCK_EVENT_REASON_CODES, // command
	    0,				      // completion code
	    0,				      // reserved
	    0,				      // reserved
	    4,
	    0, // data size
	    3,
	    0,
	    0,
	    0};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	bitfield32_t flags;

	auto rc = decode_get_current_clock_event_reason_code_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &flags);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(4, data_size);
	EXPECT_EQ(3, flags.byte);
}

TEST(getClockEventReason, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CLOCK_EVENT_REASON_CODES, // command
	    0,				      // completion code
	    0,				      // reserved
	    0,				      // reserved
	    3, // data size. Here it should not be 0. Negative case
	    0, // data size
	    3,
	    0,
	    0,
	    0};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	bitfield32_t flags;

	auto rc = decode_get_current_clock_event_reason_code_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_current_clock_event_reason_code_resp(
	    response, msg_len, NULL, &data_size, &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_current_clock_event_reason_code_resp(
	    response, msg_len, &cc, NULL, &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_current_clock_event_reason_code_resp(
	    response, msg_len - 1, &cc, &data_size, &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_current_clock_event_reason_code_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(setPowerLimit, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_set_power_limit_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint32_t id = 1;
	uint8_t action = NEW_LIMIT;
	uint8_t persistence = ONE_SHOT;
	uint32_t power_limit = 2000;
	auto rc = encode_set_power_limit_req(0, id, action, persistence,
					     power_limit, request);
	struct nsm_set_power_limit_req *req =
	    reinterpret_cast<struct nsm_set_power_limit_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_POWER_LIMITS, req->hdr.command);
	EXPECT_EQ(10, req->hdr.data_size);
	EXPECT_EQ(action, req->action);
	EXPECT_EQ(persistence, req->persistance);
	EXPECT_EQ(htole32(power_limit), req->power_limit);
}

TEST(setDevicePowerLimit, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_set_power_limit_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t action = NEW_LIMIT;
	uint8_t persistence = ONE_SHOT;
	uint32_t power_limit = 2000;
	auto rc = encode_set_device_power_limit_req(0, action, persistence,
						    power_limit, request);
	struct nsm_set_power_limit_req *req =
	    reinterpret_cast<struct nsm_set_power_limit_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_POWER_LIMITS, req->hdr.command);
	EXPECT_EQ(10, req->hdr.data_size);
	EXPECT_EQ(action, req->action);
	EXPECT_EQ(persistence, req->persistance);
	EXPECT_EQ(htole32(power_limit), req->power_limit);
	EXPECT_EQ(htole32(0), req->id);
}

TEST(setModulePowerLimit, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_set_power_limit_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t action = NEW_LIMIT;
	uint8_t persistence = ONE_SHOT;
	uint32_t power_limit = 2000;
	auto rc = encode_set_module_power_limit_req(0, action, persistence,
						    power_limit, request);
	struct nsm_set_power_limit_req *req =
	    reinterpret_cast<struct nsm_set_power_limit_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_POWER_LIMITS, req->hdr.command);
	EXPECT_EQ(10, req->hdr.data_size);
	EXPECT_EQ(action, req->action);
	EXPECT_EQ(persistence, req->persistance);
	EXPECT_EQ(htole32(power_limit), req->power_limit);
	EXPECT_EQ(htole32(1), req->id);
}

TEST(setPowerLimit, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_POWER_LIMITS,	     // command
	    10,				     // data size
	    1,
	    0,
	    0,
	    0,
	    DEFAULT_LIMIT, // action
	    PERSISTENT,	   // persistence
	    30,		   // power limit
	    0,
	    0,
	    0};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint32_t id;
	uint8_t action;
	uint8_t persistence;
	uint32_t power_limit;
	auto rc = decode_set_power_limit_req(request, msg_len, &id, &action,
					     &persistence, &power_limit);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(DEFAULT_LIMIT, action);
	EXPECT_EQ(PERSISTENT, persistence);
	EXPECT_EQ(power_limit, 30);
}

TEST(setPowerLimit, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    encode_set_power_limit_resp(0, NSM_SUCCESS, reason_code, response);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_POWER_LIMITS, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(setPowerLimit, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_POWER_LIMITS,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_power_limit_resp(response, msg_len, &cc,
					      &data_size, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0, data_size);
}

TEST(setPowerLimit, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_SET_POWER_LIMITS,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0,				     // data size
	    0				     // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_power_limit_resp(NULL, msg_len, &cc, &data_size,
					      &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_power_limit_resp(response, msg_len, NULL, &data_size,
					 &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_power_limit_resp(response, msg_len, &cc, NULL,
					 &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_power_limit_resp(response, msg_len - 1, &cc, &data_size,
					 &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getPowerLimit, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_power_limit_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint32_t id = 1;
	auto rc = encode_get_power_limit_req(0, id, request);
	struct nsm_get_power_limit_req *req =
	    reinterpret_cast<struct nsm_get_power_limit_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_POWER_LIMITS, req->hdr.command);
	EXPECT_EQ(4, req->hdr.data_size);
	EXPECT_EQ(htole32(id), req->id);
}

TEST(getDevicePowerLimit, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_power_limit_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_get_device_power_limit_req(0, request);
	struct nsm_get_power_limit_req *req =
	    reinterpret_cast<struct nsm_get_power_limit_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_POWER_LIMITS, req->hdr.command);
	EXPECT_EQ(4, req->hdr.data_size);
	EXPECT_EQ(htole32(0), req->id);
}

TEST(getModulePowerLimit, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_power_limit_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_get_module_power_limit_req(0, request);
	struct nsm_get_power_limit_req *req =
	    reinterpret_cast<struct nsm_get_power_limit_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_POWER_LIMITS, req->hdr.command);
	EXPECT_EQ(4, req->hdr.data_size);
	EXPECT_EQ(htole32(1), req->id);
}

TEST(getPowerLimit, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_POWER_LIMITS,	     // command
	    4,				     // data size
	    1,
	    0,
	    0,
	    0,
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint32_t id;
	auto rc = decode_get_power_limit_req(request, msg_len, &id);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, id);
}

TEST(getPowerLimit, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_power_limit_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;
	uint32_t requested_persistent_limit = 100;
	uint32_t requested_oneshot_limit = 150;
	uint32_t enforced_limit = 125;
	auto rc = encode_get_power_limit_resp(
	    0, NSM_SUCCESS, reason_code, requested_persistent_limit,
	    requested_oneshot_limit, enforced_limit, response);

	struct nsm_get_power_limit_resp *resp =
	    reinterpret_cast<struct nsm_get_power_limit_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_POWER_LIMITS, resp->hdr.command);
	EXPECT_EQ(3 * sizeof(uint32_t), le16toh(resp->hdr.data_size));
	EXPECT_EQ(htole32(requested_persistent_limit),
		  resp->requested_persistent_limit);
	EXPECT_EQ(requested_oneshot_limit,
		  le32toh(resp->requested_oneshot_limit));
	EXPECT_EQ(enforced_limit, le32toh(resp->enforced_limit));
}

TEST(getPowerLimit, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_POWER_LIMITS,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    12,
	    0, // data size
	    100,
	    0,
	    0,
	    0,
	    120,
	    0,
	    0,
	    0,
	    150,
	    0,
	    0,
	    0,
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t requested_persistent_limit;
	uint32_t requested_oneshot_limit;
	uint32_t enforced_limit;

	auto rc = decode_get_power_limit_resp(
	    response, msg_len, &cc, &data_size, &reason_code,
	    &requested_persistent_limit, &requested_oneshot_limit,
	    &enforced_limit);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(12, data_size);
	EXPECT_EQ(requested_persistent_limit, 100);
}

TEST(getPowerLimit, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_POWER_LIMITS,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    10,
	    0, // wrong data size
	    100,
	    0,
	    0,
	    0,
	    120,
	    0,
	    0,
	    0,
	    150,
	    0,
	    0,
	    0,
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t requested_persistent_limit;
	uint32_t requested_oneshot_limit;
	uint32_t enforced_limit;

	auto rc = decode_get_power_limit_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code,
	    &requested_persistent_limit, &requested_oneshot_limit,
	    &enforced_limit);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	rc = decode_get_power_limit_resp(
	    response, msg_len, NULL, &data_size, &reason_code,
	    &requested_persistent_limit, &requested_oneshot_limit,
	    &enforced_limit);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_power_limit_resp(
	    response, msg_len, &cc, &data_size, &reason_code, NULL,
	    &requested_oneshot_limit, &enforced_limit);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_power_limit_resp(
	    response, msg_len, &cc, &data_size, &reason_code,
	    &requested_persistent_limit, NULL, &enforced_limit);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_power_limit_resp(
	    response, msg_len - 1, &cc, &data_size, &reason_code,
	    &requested_persistent_limit, &requested_oneshot_limit,
	    &enforced_limit);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_power_limit_resp(
	    response, msg_len, &cc, &data_size, &reason_code,
	    &requested_persistent_limit, &requested_oneshot_limit,
	    &enforced_limit);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getAccumGpuUtilTime, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_get_accum_GPU_util_time_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getAccumGpuUtilTime, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_accum_GPU_util_time_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint32_t contextUtilTime = 100;
	uint32_t smUtilTime = 200;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_accum_GPU_util_time_resp(
	    0, NSM_SUCCESS, reason_code, &contextUtilTime, &smUtilTime,
	    response);

	struct nsm_get_accum_GPU_util_time_resp *resp =
	    reinterpret_cast<struct nsm_get_accum_GPU_util_time_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME, resp->hdr.command);
	EXPECT_EQ(8, le16toh(resp->hdr.data_size));
	EXPECT_EQ(contextUtilTime, le32toh(resp->context_util_time));
	EXPECT_EQ(smUtilTime, le32toh(resp->SM_util_time));
}

TEST(getAccumGpuUtilTime, testGoodDecodeResponse)
{
	std::vector<uint8_t> contextUtilTime_byte{
	    0x01,
	    0x0A,
	    0x00,
	    0x01,
	};
	std::vector<uint8_t> smUtilTime_byte{0x0A, 0x12, 0x1A, 0x00};

	uint32_t *contextUtilTime =
	    reinterpret_cast<uint32_t *>(contextUtilTime_byte.data());
	uint32_t *smUtilTime =
	    reinterpret_cast<uint32_t *>(smUtilTime_byte.data());
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME, // command
	    0,					      // completion code
	    0,					      // reserved
	    0,					      // reserved
	    8,
	    0 // data size
	};

	responseMsg.insert(responseMsg.end(), contextUtilTime_byte.begin(),
			   contextUtilTime_byte.end());
	responseMsg.insert(responseMsg.end(), smUtilTime_byte.begin(),
			   smUtilTime_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t contextUtil_test;
	uint32_t smUtil_test;

	auto rc = decode_get_accum_GPU_util_time_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &contextUtil_test,
	    &smUtil_test);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(8, data_size);
	EXPECT_EQ(le32toh(*contextUtilTime), contextUtil_test);
	EXPECT_EQ(le32toh(*smUtilTime), smUtil_test);
}

TEST(getAccumGpuUtilTime, testBadDecodeResponse)
{
	std::vector<uint8_t> contextUtilTime_byte{
	    0x01,
	    0x0A,
	    0x00,
	    0x01,
	};
	std::vector<uint8_t> smUtilTime_byte{0x0A, 0x12, 0x1A, 0x00};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME, // command
	    0,					      // completion code
	    0,					      // reserved
	    0,					      // reserved
	    7,					      // wrong data size
	    0					      // data size
	};

	responseMsg.insert(responseMsg.end(), contextUtilTime_byte.begin(),
			   contextUtilTime_byte.end());
	responseMsg.insert(responseMsg.end(), smUtilTime_byte.begin(),
			   smUtilTime_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t contextUtil_test;
	uint32_t smUtil_test;

	auto rc = decode_get_accum_GPU_util_time_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &contextUtil_test,
	    &smUtil_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	rc = decode_get_accum_GPU_util_time_resp(
	    response, msg_len, NULL, &data_size, &reason_code,
	    &contextUtil_test, &smUtil_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_accum_GPU_util_time_resp(
	    response, msg_len, &cc, NULL, &reason_code, &contextUtil_test,
	    &smUtil_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_accum_GPU_util_time_resp(
	    response, msg_len - 1, &cc, &data_size, &reason_code,
	    &contextUtil_test, &smUtil_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_accum_GPU_util_time_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &contextUtil_test,
	    &smUtil_test);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getRowRemapState, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_row_remap_state_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ROW_REMAP_STATE_FLAGS, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getRowRemapState, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_row_remap_state_req(0, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	rc = encode_get_row_remap_state_req(0, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getRowRemapState, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ROW_REMAP_STATE_FLAGS,   // command
	    0				     // data size
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	auto rc = decode_get_row_remap_state_req(request, msg_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getRowRemapState, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remap_state_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	bitfield8_t flags;
	flags.byte = 1;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, reason_code,
						  &flags, response);

	struct nsm_get_row_remap_state_resp *resp =
	    reinterpret_cast<struct nsm_get_row_remap_state_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ROW_REMAP_STATE_FLAGS, resp->hdr.command);
	EXPECT_EQ(sizeof(bitfield8_t), le16toh(resp->hdr.data_size));
	EXPECT_EQ(1, resp->flags.byte);
}

TEST(getRowRemapState, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ROW_REMAP_STATE_FLAGS,   // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    1,
	    0, // data size
	    1};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	bitfield8_t flags;

	auto rc = decode_get_row_remap_state_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &flags);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(1, data_size);
	EXPECT_EQ(1, flags.byte);
}

TEST(getRowRemapState, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ROW_REMAP_STATE_FLAGS,   // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0, // data size. Here it should not be 0. Negative case
	    0, // data size
	    1};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	bitfield8_t flags;

	auto rc = decode_get_row_remap_state_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_row_remap_state_resp(response, msg_len, NULL,
					     &data_size, &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_row_remap_state_resp(response, msg_len, &cc, NULL,
					     &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_row_remap_state_resp(response, msg_len - 1, &cc,
					     &data_size, &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_row_remap_state_resp(response, msg_len, &cc, &data_size,
					     &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getRowRemappingCounts, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_row_remapping_counts_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ROW_REMAPPING_COUNTS, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getRowRemappingCounts, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ROW_REMAPPING_COUNTS,    // command
	    0				     // data size
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	auto rc = decode_get_row_remapping_counts_req(request, msg_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getRowRemappingCounts, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_row_remapping_counts_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint32_t correctable_error = 4987;
	uint32_t uncorrectable_error = 2564;
	uint32_t correctable_error_test = correctable_error;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_row_remapping_counts_resp(
	    0, NSM_SUCCESS, reason_code, correctable_error, uncorrectable_error,
	    response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_get_row_remapping_counts_resp *resp =
	    reinterpret_cast<struct nsm_get_row_remapping_counts_resp *>(
		response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ROW_REMAPPING_COUNTS, resp->hdr.command);
	EXPECT_EQ(2 * sizeof(uint32_t), le16toh(resp->hdr.data_size));

	EXPECT_EQ(le32toh(correctable_error_test), correctable_error);
}

TEST(getRowRemappingCounts, testGoodDecodeResponse)
{
	std::vector<uint8_t> correctable_error_byte{0x01, 0x0A, 0x00, 0x01};
	std::vector<uint8_t> uncorrectable_error_byte{
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ECC_ERROR_COUNTS,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    8,
	    0 // data size
	};

	responseMsg.insert(responseMsg.end(), correctable_error_byte.begin(),
			   correctable_error_byte.end());
	responseMsg.insert(responseMsg.end(), uncorrectable_error_byte.begin(),
			   uncorrectable_error_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t correctable_error;
	uint32_t uncorrectable_error;

	auto rc = decode_get_row_remapping_counts_resp(
	    response, msg_len, &cc, &data_size, &reason_code,
	    &correctable_error, &uncorrectable_error);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(8, data_size);
}

TEST(getRowRemappingCounts, testBadDecodeResponse)
{
	std::vector<uint8_t> correctable_error_byte{0x01, 0x0A, 0x00, 0x01};
	std::vector<uint8_t> uncorrectable_error_byte{
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ECC_ERROR_COUNTS,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    5,				     // incorrect data size
	    0				     // data size
	};

	responseMsg.insert(responseMsg.end(), correctable_error_byte.begin(),
			   correctable_error_byte.end());
	responseMsg.insert(responseMsg.end(), uncorrectable_error_byte.begin(),
			   uncorrectable_error_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t correctable_error;
	uint32_t uncorrectable_error;

	auto rc = decode_get_row_remapping_counts_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &correctable_error,
	    &uncorrectable_error);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_row_remapping_counts_resp(
	    response, msg_len, NULL, &data_size, &reason_code,
	    &correctable_error, &uncorrectable_error);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_row_remapping_counts_resp(
	    response, msg_len, &cc, NULL, &reason_code, &correctable_error,
	    &uncorrectable_error);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_row_remapping_counts_resp(
	    response, msg_len - 1, &cc, &data_size, &reason_code,
	    &correctable_error, &uncorrectable_error);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_row_remapping_counts_resp(
	    response, msg_len, &cc, &data_size, &reason_code,
	    &correctable_error, &uncorrectable_error);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getRowRemapAvailability, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_row_remap_availability_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ROW_REMAP_AVAILABILITY, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getRowRemapAvailability, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_row_remap_availability_req(0, NULL);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	rc = encode_get_row_remap_availability_req(0, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getRowRemapAvailability, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ROW_REMAP_AVAILABILITY,  // command
	    0				     // data size
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	auto rc = decode_get_row_remap_availability_req(request, msg_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getRowRemapAvailability, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_row_remap_availability_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	struct nsm_row_remap_availability data;
	data.high_remapping = 100;
	data.low_remapping = 200;
	data.max_remapping = 300;
	data.no_remapping = 400;
	data.partial_remapping = 500;

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_row_remap_availability_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_get_row_remap_availability_resp *resp =
	    reinterpret_cast<struct nsm_get_row_remap_availability_resp *>(
		response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_ROW_REMAP_AVAILABILITY, resp->hdr.command);
	EXPECT_EQ(5 * sizeof(uint16_t), le16toh(resp->hdr.data_size));

	EXPECT_EQ(htole16(data.low_remapping), resp->data.low_remapping);
}

TEST(getRowRemapAvailability, testBadEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_row_remap_availability_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	struct nsm_row_remap_availability data;
	data.high_remapping = 100;
	data.low_remapping = 200;
	data.max_remapping = 300;
	data.no_remapping = 400;
	data.partial_remapping = 500;

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_row_remap_availability_resp(
	    0, NSM_SUCCESS, reason_code, &data, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	encode_get_row_remap_availability_resp(0, NSM_SUCCESS, reason_code,
					       NULL, response);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getRowRemapAvailability, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ECC_ERROR_COUNTS,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    10,
	    0, // data size
	    100,
	    0,
	    200,
	    0,
	    150,
	    0,
	    160,
	    0,
	    170,
	    0,};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_row_remap_availability data;
	auto rc = decode_get_row_remap_availability_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(10, data_size);
	EXPECT_EQ(100, data.no_remapping);
}

TEST(getRowRemapAvailability, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_ECC_ERROR_COUNTS,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    9,
	    0, // data size
	    100,
	    0,
	    200,
	    0,
	    150,
	    0,
	    160,
	    0,
	    170,
	    0};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_row_remap_availability data;

	auto rc = decode_get_row_remap_availability_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_row_remap_availability_resp(
	    response, msg_len, NULL, &data_size, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_row_remap_availability_resp(response, msg_len, &cc,
						    NULL, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_row_remap_availability_resp(
	    response, msg_len - 1, &cc, &data_size, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_row_remap_availability_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getMemCapacityUtil, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_memory_capacity_util_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_MEMORY_CAPACITY_UTILIZATION, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getMemCapacityUtil, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MEMORY_CAPACITY_UTILIZATION, // command
	    0					 // data size
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	auto rc = decode_get_memory_capacity_util_req(request, msg_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getMemCapacityUtil, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_memory_capacity_util_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	struct nsm_memory_capacity_utilization data;
	data.reserved_memory = 200;
	data.used_memory = 150;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_memory_capacity_util_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_get_memory_capacity_util_resp *resp =
	    reinterpret_cast<struct nsm_get_memory_capacity_util_resp *>(
		response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_MEMORY_CAPACITY_UTILIZATION, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_memory_capacity_utilization),
		  le16toh(resp->hdr.data_size));

	EXPECT_EQ(le32toh(resp->data.reserved_memory), data.reserved_memory);
}

TEST(getMemCapacityUtil, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	};

	struct nsm_memory_capacity_utilization *data =
	    reinterpret_cast<struct nsm_memory_capacity_utilization *>(
		data_byte.data());

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MEMORY_CAPACITY_UTILIZATION, // command
	    0,					 // completion code
	    0,					 // reserved
	    0,					 // reserved
	    8,
	    0 // data size
	};

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_memory_capacity_utilization data_test;

	auto rc = decode_get_memory_capacity_util_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &data_test);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(8, data_size);
	EXPECT_EQ(le32toh(data->reserved_memory), data_test.reserved_memory);
}

TEST(getMemCapacityUtil, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MEMORY_CAPACITY_UTILIZATION, // command
	    0,					 // completion code
	    0,					 // reserved
	    0,					 // reserved
	    9,
	    0 // data size
	};

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_memory_capacity_utilization data;

	auto rc = decode_get_memory_capacity_util_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_memory_capacity_util_resp(
	    response, msg_len, NULL, &data_size, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_memory_capacity_util_resp(response, msg_len, &cc, NULL,
						  &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_memory_capacity_util_resp(
	    response, msg_len - 1, &cc, &data_size, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_memory_capacity_util_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getCurrentUtilization, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CURRENT_UTILIZATION,     // command
	    0				     // data size
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	auto rc = decode_get_memory_capacity_util_req(request, msg_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getCurrentUtilization, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_current_utilization_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint32_t gpu_utilization = 36;
	uint32_t memory_utilization = 75;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_current_utilization_resp(
	    0, NSM_SUCCESS, reason_code, gpu_utilization, memory_utilization,
	    response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_get_current_utilization_resp *resp =
	    reinterpret_cast<struct nsm_get_current_utilization_resp *>(
		response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_CURRENT_UTILIZATION, resp->hdr.command);
	EXPECT_EQ(sizeof(gpu_utilization) + sizeof(memory_utilization),
		  le16toh(resp->hdr.data_size));

	EXPECT_EQ(le32toh(resp->gpu_utilization), gpu_utilization);
	EXPECT_EQ(le32toh(resp->memory_utilization), memory_utilization);
}

TEST(getCurrentUtilization, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MEMORY_CAPACITY_UTILIZATION, // command
	    0,					 // completion code
	    0,					 // reserved
	    0,					 // reserved
	    8,
	    0, // data size
	    0x10,
	    0x01,
	    0x00,
	    0x00,
	    0x11,
	    0x00,
	    0x00,
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	uint32_t gpu_utilization;
	uint32_t memory_utilization;

	auto rc = decode_get_current_utilization_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &gpu_utilization,
	    &memory_utilization);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(8, data_size);
	EXPECT_EQ(272, gpu_utilization);
	EXPECT_EQ(17, memory_utilization);
}

TEST(getCurrentUtilization, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_MEMORY_CAPACITY_UTILIZATION, // command
	    0,					 // completion code
	    0,					 // reserved
	    0,					 // reserved
	    8,
	    0, // data size
	    0x10,
	    0x01,
	    0x00,
	    0x00,
	    0x11,
	    0x00,
	    0x00,
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	uint32_t gpu_utilization;
	uint32_t memory_utilization;

	auto rc = decode_get_current_utilization_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &gpu_utilization,
	    &memory_utilization);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_current_utilization_resp(
	    response, msg_len, NULL, &data_size, &reason_code, &gpu_utilization,
	    &memory_utilization);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_current_utilization_resp(response, msg_len, &cc, NULL,
						 &reason_code, &gpu_utilization,
						 &memory_utilization);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_current_utilization_resp(response, msg_len, &cc, NULL,
						 &reason_code, NULL,
						 &memory_utilization);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_current_utilization_resp(
	    response, msg_len - 1, &cc, &data_size, &reason_code,
	    &gpu_utilization, &memory_utilization);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	responseMsg[9] = 6;
	rc = decode_get_current_utilization_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &gpu_utilization,
	    &memory_utilization);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getClockOutputEnableState, testGoodEncodeRequest)
{
	std::vector<uint8_t> request_msg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_clock_output_enabled_state_req));

	uint8_t index = PCIE_CLKBUF_INDEX;

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());

	auto rc = encode_get_clock_output_enable_state_req(0, index, request);

	struct nsm_get_clock_output_enabled_state_req *req =
	    reinterpret_cast<nsm_get_clock_output_enabled_state_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_CLOCK_OUTPUT_ENABLE_STATE, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(PCIE_CLKBUF_INDEX, req->index);
}

TEST(getClockOutputEnableState, testBadEncodeRequest)
{
	std::vector<uint8_t> request_msg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_clock_output_enabled_state_req));

	auto rc = encode_get_clock_output_enable_state_req(0, 0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getClockOutputEnableState, testGoodDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CLOCK_OUTPUT_ENABLE_STATE, // command
	    1,				       // data size
	    4				       // index
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());
	size_t msg_len = request_msg.size();

	uint8_t index = 0;
	auto rc =
	    decode_get_clock_output_enable_state_req(request, msg_len, &index);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(4, index);
}

TEST(getClockOutputEnableState, testBadDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CLOCK_OUTPUT_ENABLE_STATE, // command
	    0,				       // data size [it should be 1]
	    4				       // index
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());
	uint8_t index = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_clock_output_enabled_state_req);

	auto rc = decode_get_clock_output_enable_state_req(nullptr, 0, &index);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_clock_output_enable_state_req(request, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_clock_output_enable_state_req(request, msg_len - 6,
						      &index);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_clock_output_enable_state_req(request, msg_len, &index);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getClockOutputEnableState, testGoodEncodeResponseCCSuccess)
{
	uint32_t clk_buf = 0xFFFFFFFF;
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_get_clock_output_enabled_state_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// test for cc = 0x0 [NSM_SUCCESS]
	auto rc = encode_get_clock_output_enable_state_resp(
	    0, NSM_SUCCESS, reason_code, clk_buf, response);

	struct nsm_get_clock_output_enabled_state_resp *resp =
	    reinterpret_cast<struct nsm_get_clock_output_enabled_state_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_CLOCK_OUTPUT_ENABLE_STATE, resp->hdr.command);
	EXPECT_EQ(NSM_SUCCESS, resp->hdr.completion_code);
	EXPECT_EQ(htole16(sizeof(clk_buf)), resp->hdr.data_size);
	EXPECT_EQ(htole32(clk_buf), resp->clk_buf_data);
}

TEST(getClockOutputEnableState, testGoodEncodeResponseCCError)
{
	uint32_t clk_buf = 0xFFFFFFFF;
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// test for cc = 0x1 [NSM_ERROR]
	auto rc = encode_get_clock_output_enable_state_resp(
	    0, NSM_ERROR, reason_code, clk_buf, response);

	struct nsm_common_non_success_resp *resp =
	    reinterpret_cast<struct nsm_common_non_success_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_CLOCK_OUTPUT_ENABLE_STATE, resp->command);
	EXPECT_EQ(NSM_ERROR, resp->completion_code);
	EXPECT_EQ(htole16(reason_code), resp->reason_code);
}

TEST(getClockOutputEnableState, testBadEncodeResponse)
{
	uint32_t clk_buf = 0xFFFFFFFF;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_clock_output_enable_state_resp(
	    0, NSM_SUCCESS, reason_code, clk_buf, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getClockOutputEnableState, testGoodDecodeResponseCCSuccess)
{
	// test when CC is NSM_SUCCESS and data payload is correct
	std::vector<uint8_t> response_msg{
	    0x10, // PCI VID: NVIDIA 0x10DE
	    0xDE,
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CLOCK_OUTPUT_ENABLE_STATE, // command
	    0x00,			       // completion code
	    0x00,			       // reserved
	    0x00,
	    0x04, // data size
	    0x00,
	    0xFF, // clock buffer data
	    0xFF,
	    0xFF,
	    0xFF};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint32_t clk_buf = 0;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_get_clock_output_enable_state_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &clk_buf);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 0x0004);
	EXPECT_EQ(clk_buf, 0xFFFFFFFF);
}

TEST(getClockOutputEnableState, testGoodDecodeResponseCCError)
{
	// test when CC is NSM_ERROR and data payload is empty
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CLOCK_OUTPUT_ENABLE_STATE, // command
	    0x01,			       // completion code
	    0x00,			       // reason code
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint32_t clk_buf = 0;
	uint16_t data_size = 0;

	auto rc = decode_get_clock_output_enable_state_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &clk_buf);

	EXPECT_EQ(cc, NSM_ERROR);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(reason_code, 0x0000);
}

TEST(getClockOutputEnableState, testBadDecodeResponseWithPayload)
{
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_CLOCK_OUTPUT_ENABLE_STATE, // command
	    0x01, // completion code [0x01 - NSM_ERROR]
	    0x00, // reserved
	    0x00,
	    0x00, // data size [should not be zero]
	    0x00,
	    0xFF, // clock buffer data
	    0xFF,
	    0xFF,
	    0xFF};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t clk_buf = 0;

	auto rc = decode_get_clock_output_enable_state_resp(
	    nullptr, msg_len, &cc, &reason_code, &data_size, &clk_buf);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_clock_output_enable_state_resp(
	    response, msg_len, nullptr, &reason_code, &data_size, &clk_buf);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_clock_output_enable_state_resp(
	    response, msg_len, &cc, nullptr, &data_size, &clk_buf);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_clock_output_enable_state_resp(
	    response, msg_len, &cc, &reason_code, nullptr, &clk_buf);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_clock_output_enable_state_resp(
	    response, msg_len, &cc, &reason_code, &data_size, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_clock_output_enable_state_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &clk_buf);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_ERROR);

	response_msg[6] = 0x00; // making CC - NSM_SUCCESS
	rc = decode_get_clock_output_enable_state_resp(
	    response, msg_len - 4, &cc, &reason_code, &data_size, &clk_buf);
	//-4 from total size which means we should get error
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_clock_output_enable_state_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &clk_buf);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(XIDEvent, testGoodEncodeResponse)
{
	const std::string message_text{"XID Event"};

	std::vector<uint8_t> event_msg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
				       sizeof(nsm_xid_event_payload) +
				       message_text.size());
	auto msg = reinterpret_cast<nsm_msg *>(event_msg.data());

	const nsm_xid_event_payload payload_data{.flag = 0x3A,
						 .reserved = {},
						 .reason = 0x29FB,
						 .sequence_number = 11490,
						 .timestamp = 2483710479};

	auto rc =
	    encode_nsm_xid_event(0, true, payload_data, message_text.data(),
				 message_text.size(), msg);

	auto response = reinterpret_cast<nsm_msg *>(event_msg.data());

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, response->hdr.request);
	EXPECT_EQ(1, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	struct nsm_event *event = (struct nsm_event *)response->payload;

	EXPECT_EQ(NSM_XID_EVENT, event->event_id);
	EXPECT_EQ(true, event->ackr);
	EXPECT_EQ(NSM_EVENT_VERSION, event->version);
	EXPECT_EQ(NSM_GENERAL_EVENT_CLASS, event->event_class);
	EXPECT_EQ(0, event->event_state);
	EXPECT_EQ(sizeof(nsm_xid_event_payload) + message_text.size(),
		  event->data_size);

	nsm_xid_event_payload *payload =
	    (struct nsm_xid_event_payload *)event->data;

	EXPECT_EQ(payload_data.flag, payload->flag);
	EXPECT_EQ(payload_data.reason, payload->reason);
	EXPECT_EQ(payload_data.sequence_number, payload->sequence_number);
	EXPECT_EQ(payload_data.timestamp, payload->timestamp);

	std::string text(
	    (const char *)&event->data[sizeof(nsm_xid_event_payload)],
	    event->data_size - sizeof(nsm_xid_event_payload));
	EXPECT_EQ(text, message_text.data());
}

class XIDEventDecode : public testing::Test
{
      protected:
	XIDEventDecode()
	    : message_text{"XID Event"},
	      event_msg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
			sizeof(nsm_xid_event_payload) + message_text.size()),
	      payload_data{.flag = 0x3A,
			   .reserved = {},
			   .reason = 0x29FB,
			   .sequence_number = 11490,
			   .timestamp = 2483710479}
	{
		auto rc = encode_nsm_xid_event(
		    0, true, payload_data, message_text.data(),
		    message_text.size(),
		    reinterpret_cast<nsm_msg *>(event_msg.data()));

		EXPECT_EQ(rc, NSM_SW_SUCCESS);
		response = reinterpret_cast<nsm_msg *>(event_msg.data());
	};

	const std::string message_text;
	std::vector<uint8_t> event_msg;
	const nsm_msg *response;
	const nsm_xid_event_payload payload_data;
};

TEST_F(XIDEventDecode, testGoodDecodeResponse)
{
	uint8_t event_class{};
	uint16_t event_state{};
	nsm_xid_event_payload payload{};
	char text[NSM_EVENT_DATA_MAX_LEN]{};
	size_t message_text_size{};

	auto rc = decode_nsm_xid_event(response, event_msg.size(), &event_class,
				       &event_state, &payload, text,
				       &message_text_size);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	std::string message_text_decoded(text, message_text_size);

	EXPECT_EQ(NSM_GENERAL_EVENT_CLASS, event_class);
	EXPECT_EQ(0, event_state);

	EXPECT_EQ(payload_data.flag, payload.flag);
	EXPECT_EQ(payload_data.reason, payload.reason);
	EXPECT_EQ(payload_data.sequence_number, payload.sequence_number);
	EXPECT_EQ(payload_data.timestamp, payload.timestamp);

	EXPECT_EQ(message_text, message_text_decoded);
}

TEST_F(XIDEventDecode, testBadDecodeResponseLength)
{
	uint8_t event_class{};
	uint16_t event_state{};
	nsm_xid_event_payload payload{};
	char text[NSM_EVENT_DATA_MAX_LEN]{};
	size_t message_text_size{};

	auto rc = decode_nsm_xid_event(response, 10, &event_class, &event_state,
				       &payload, text, &message_text_size);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(XIDEventDecode, testBadDecodeResponseNull)
{
	uint8_t event_class{};
	uint16_t event_state{};
	char text[NSM_EVENT_DATA_MAX_LEN]{};
	size_t message_text_size{};

	auto rc = decode_nsm_xid_event(response, event_msg.size(), &event_class,
				       &event_state, nullptr, text,
				       &message_text_size);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST_F(XIDEventDecode, testBadDecodeResponseDataLength)
{
	uint8_t event_class{};
	uint16_t event_state{};
	nsm_xid_event_payload payload{};
	char text[NSM_EVENT_DATA_MAX_LEN]{};
	size_t message_text_size{};

	auto rc = decode_nsm_xid_event(response, event_msg.size() - 1,
				       &event_class, &event_state, &payload,
				       text, &message_text_size);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);

	struct nsm_event *event = (struct nsm_event *)response->payload;
	event->data_size -= message_text.size() - 1;

	rc = decode_nsm_xid_event(
	    response, event_msg.size() - message_text.size() - 1, &event_class,
	    &event_state, &payload, text, &message_text_size);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(queryAggregateGPMMetrics, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_aggregate_gpm_metrics_req) +
	    2);

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	const uint8_t retrieval_source = 1;
	const uint8_t gpu_instance = 0xFF;
	const uint8_t compute_instance = 38;
	const uint8_t metrics_bitfield[3]{0x23, 0x84, 0x05};

	auto rc = encode_query_aggregate_gpm_metrics_req(
	    0, retrieval_source, gpu_instance, compute_instance,
	    metrics_bitfield, 3, request);

	struct nsm_query_aggregate_gpm_metrics_req *req =
	    reinterpret_cast<struct nsm_query_aggregate_gpm_metrics_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_AGGREGATE_GPM_METRICS, req->hdr.command);
	EXPECT_EQ(6, req->hdr.data_size);
	EXPECT_EQ(retrieval_source, req->retrieval_source);
	EXPECT_EQ(gpu_instance, req->gpu_instance);
	EXPECT_EQ(compute_instance, req->compute_instance);
	EXPECT_EQ(0x23, req->metrics_bitfield[0]);
	EXPECT_EQ(0x84, req->metrics_bitfield[1]);
	EXPECT_EQ(0x05, req->metrics_bitfield[2]);
}

TEST(queryAggregateGPMMetrics, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_QUERY_AGGREGATE_GPM_METRICS, // command
	    7,				     // data size
	    4,				     // retrieval_source
	    8,				     // gpu_instance
	    9,				     // compute_instance
	    35,				     // metrics_bitfield
	    94,				     // metrics_bitfield
	    148,			     // metrics_bitfield
	    249,			     // metrics_bitfield
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t retrieval_source;
	uint8_t gpu_instance;
	uint8_t compute_instance;
	const uint8_t *metrics_bitfield;
	size_t metrics_bitfield_length;

	auto rc = decode_query_aggregate_gpm_metrics_req(
	    request, msg_len, &retrieval_source, &gpu_instance,
	    &compute_instance, &metrics_bitfield, &metrics_bitfield_length);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(retrieval_source, 4);
	EXPECT_EQ(gpu_instance, 8);
	EXPECT_EQ(compute_instance, 9);
	EXPECT_EQ(metrics_bitfield_length, 4);
	EXPECT_EQ(metrics_bitfield[0], 35);
	EXPECT_EQ(metrics_bitfield[1], 94);
	EXPECT_EQ(metrics_bitfield[2], 148);
	EXPECT_EQ(metrics_bitfield[3], 249);
}

TEST(queryPerInstanceGPMMetrics, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_query_per_instance_gpm_metrics_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	const uint8_t retrieval_source = 1;
	const uint8_t gpu_instance = 0xFF;
	const uint8_t compute_instance = 38;
	const uint8_t metric_id = 12;
	const uint32_t instance_bitfield = 0x23954800;

	auto rc = encode_query_per_instance_gpm_metrics_req(
	    0, retrieval_source, gpu_instance, compute_instance, metric_id,
	    instance_bitfield, request);

	struct nsm_query_per_instance_gpm_metrics_req *req =
	    reinterpret_cast<struct nsm_query_per_instance_gpm_metrics_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_PER_INSTANCE_GPM_METRICS, req->hdr.command);
	EXPECT_EQ(8, req->hdr.data_size);
	EXPECT_EQ(retrieval_source, req->retrieval_source);
	EXPECT_EQ(gpu_instance, req->gpu_instance);
	EXPECT_EQ(compute_instance, req->compute_instance);
	EXPECT_EQ(metric_id, req->metric_id);
	EXPECT_EQ(instance_bitfield, req->instance_bitmask);
}

TEST(queryPerInstanceGPMMetrics, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_QUERY_AGGREGATE_GPM_METRICS, // command
	    8,				     // data size
	    4,				     // retrieval_source
	    8,				     // gpu_instance
	    9,				     // compute_instance
	    45,				     // metric_id
	    0x34,			     // instance_bitmask
	    0x45,			     // instance_bitmask
	    0x00,			     // instance_bitmask
	    0x00,			     // instance_bitmask
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t retrieval_source;
	uint8_t gpu_instance;
	uint8_t compute_instance;
	uint8_t metric_id;
	uint32_t instance_bitfield;

	auto rc = decode_query_per_instance_gpm_metrics_req(
	    request, msg_len, &retrieval_source, &gpu_instance,
	    &compute_instance, &metric_id, &instance_bitfield);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(retrieval_source, 4);
	EXPECT_EQ(gpu_instance, 8);
	EXPECT_EQ(compute_instance, 9);
	EXPECT_EQ(metric_id, 45);
	EXPECT_EQ(instance_bitfield, 17716);
}

TEST(getViolationDuration, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_violation_duration_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_VIOLATION_DURATION, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getViolationDuration, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_VIOLATION_DURATION,   // command
	    0				     // data size
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	auto rc = decode_get_violation_duration_req(request, msg_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getViolationDuration, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_violation_duration_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	struct nsm_violation_duration data;
	data.supported_counter.byte = 255;
	data.hw_violation_duration = 20000;
	data.global_sw_violation_duration = 30000;
	data.power_violation_duration = 40000;
	data.thermal_violation_duration = 50000;
	data.counter4 = 60000;
	data.counter5 = 70000;
	data.counter6 = 80000;
	data.counter7 = 90000;

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_violation_duration_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_get_violation_duration_resp *resp =
	    reinterpret_cast<struct nsm_get_violation_duration_resp *>(
		response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_VIOLATION_DURATION, resp->hdr.command);
	EXPECT_EQ(sizeof(struct nsm_violation_duration),
		  le16toh(resp->hdr.data_size));

	EXPECT_EQ(resp->data.supported_counter.byte,
		  htole64(data.supported_counter.byte));
	EXPECT_EQ(resp->data.thermal_violation_duration,
		  htole64(data.thermal_violation_duration));
	EXPECT_EQ(resp->data.power_violation_duration,
		  htole64(data.power_violation_duration));
	EXPECT_EQ(resp->data.hw_violation_duration,
		  htole64(data.hw_violation_duration));
	EXPECT_EQ(resp->data.counter4, htole64(data.counter4));
}


TEST(getViolationDuration, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	    0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	    0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	    0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	    0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	struct nsm_violation_duration* data =
	      reinterpret_cast<struct nsm_violation_duration *>(data_byte.data());

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_VIOLATION_DURATION,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    72,
	    0 // data size
	};

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_violation_duration data_resp;

	auto rc = decode_get_violation_duration_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &data_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(72, data_size);
    EXPECT_EQ(le64toh(data->supported_counter.byte),
	 	  data_resp.supported_counter.byte);
	EXPECT_EQ(le64toh(data->thermal_violation_duration),
	 	  data_resp.thermal_violation_duration);
	EXPECT_EQ(le64toh(data->power_violation_duration),
		  data_resp.power_violation_duration);
	EXPECT_EQ(le64toh(data->hw_violation_duration),
		  data_resp.hw_violation_duration);
	EXPECT_EQ(le64toh(data->counter4),
		  data_resp.counter4);
	EXPECT_EQ(le64toh(data->counter7),
		  data_resp.counter7); 
}


TEST(getViolationDuration, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	    0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	    0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	    0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x0A, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00,
	    0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_VIOLATION_DURATION,	     // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    77,				     // wrong data size
	    0				     // data size
	};

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_violation_duration data_resp;

	auto rc = decode_get_violation_duration_resp(NULL, msg_len, &cc, &data_size,
					      &reason_code, &data_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_violation_duration_resp(response, msg_len, NULL, &data_size,
					 &reason_code, &data_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_violation_duration_resp(response, msg_len, &cc, NULL,
					 &reason_code, &data_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_violation_duration_resp(response, msg_len - 1, &cc, &data_size,
					 &reason_code, &data_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_violation_duration_resp(response, msg_len, &cc, &data_size,
					 &reason_code, &data_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}
