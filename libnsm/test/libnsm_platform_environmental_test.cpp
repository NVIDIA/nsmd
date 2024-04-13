/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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

	struct nsm_get_current_power_draw_resp *resp =
	    reinterpret_cast<struct nsm_get_current_power_draw_resp *>(
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

	struct nsm_get_voltage_resp *resp =
	    reinterpret_cast<struct nsm_get_voltage_resp *>(response->payload);

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
		0				     // data size
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
	    0,                   // data size
		0				     // data size
	    };

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_MIG_mode_resp( NULL, msg_len, &cc, &data_size,
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

TEST(getCurrClockFreq, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_get_curr_clock_freq_req(0, request);
	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_CURRENT_CLOCK_FREQUENCY, req->command);
	EXPECT_EQ(0, req->data_size);
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
