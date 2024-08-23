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
#include "device-configuration.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <types.hpp>

void testGetFpgaDiagnosticSettingsEncodeRequest(
    fpga_diagnostics_settings_data_index dataIndex)
{

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_fpga_diagnostics_settings_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc =
	    encode_get_fpga_diagnostics_settings_req(0, dataIndex, request);

	struct nsm_get_fpga_diagnostics_settings_req *req =
	    (struct nsm_get_fpga_diagnostics_settings_req *)request->payload;

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, req->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), req->hdr.data_size);
	EXPECT_EQ(dataIndex, req->data_index);
}
void testGetFpgaDiagnosticSettingsEncodeResponse(
    fpga_diagnostics_settings_data_index expectedDataIndex)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    1,				       // data size
	    (uint8_t)expectedDataIndex	       // data_index
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	fpga_diagnostics_settings_data_index dataIndex;
	auto rc = decode_get_fpga_diagnostics_settings_req(request, msg_len,
							   &dataIndex);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(expectedDataIndex, dataIndex);
}

TEST(getFpgaDiagnosticsSettings, testRequests)
{
	for (auto di = (uint8_t)GET_WP_SETTINGS;
	     di <= (uint8_t)GET_GPU_POWER_STATUS; di++) {
		auto dataIndex = fpga_diagnostics_settings_data_index(di);
		testGetFpgaDiagnosticSettingsEncodeRequest(dataIndex);
		testGetFpgaDiagnosticSettingsEncodeResponse(dataIndex);
	}
	testGetFpgaDiagnosticSettingsEncodeRequest(GET_AGGREGATE_TELEMETRY);
	testGetFpgaDiagnosticSettingsEncodeResponse(GET_AGGREGATE_TELEMETRY);
}

TEST(getFpgaDiagnosticsSettingsWPSettings, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_fpga_diagnostics_settings_wp_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_fpga_diagnostics_settings_wp data;
	data.gpu1_4 = 1;
	struct nsm_fpga_diagnostics_settings_wp data_test = data;

	auto rc = encode_get_fpga_diagnostics_settings_wp_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_fpga_diagnostics_settings_wp_resp *resp =
	    reinterpret_cast<struct nsm_fpga_diagnostics_settings_wp_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_fpga_diagnostics_settings_wp),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.gpu1_4, data.gpu1_4);
}

TEST(getFpgaDiagnosticsSettingsWPSettings, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{0b10000000, 0x00, 0b00000100, 0x00,
				       0x00,	   0x00, 0x00,	     0x00};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    8,
	    0 // data size
	};
	auto data = reinterpret_cast<nsm_fpga_diagnostics_settings_wp *>(
	    data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    response, msg_len, &cc, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(1, data->gpu1_4);
	EXPECT_EQ(1, data->retimer3);
}

TEST(getFpgaDiagnosticsSettingsWPSettings, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    7, // incorrect data size
	    0  // data size
	};
	auto data = reinterpret_cast<nsm_fpga_diagnostics_settings_wp *>(
	    data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    NULL, msg_len, &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    response, msg_len, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_resp(
	    response, msg_len, &cc, NULL, &reason_code, (uint8_t *)data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    response, msg_len - data_byte.size(), &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    response, msg_len, &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getFpgaDiagnosticsSettingsWPJumper, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_fpga_diagnostics_settings_wp_jumper data;
	data.presence = 1;
	struct nsm_fpga_diagnostics_settings_wp_jumper data_test = data;

	auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_fpga_diagnostics_settings_wp_jumper_resp *resp =
	    reinterpret_cast<
		struct nsm_fpga_diagnostics_settings_wp_jumper_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_fpga_diagnostics_settings_wp_jumper),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.presence, data.presence);
}

TEST(getFpgaDiagnosticsSettingsWPJumper, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    1,
	    0 // data size
	};
	auto data = reinterpret_cast<nsm_fpga_diagnostics_settings_wp_jumper *>(
	    data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    response, msg_len, &cc, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_test->presence, data->presence);
}

TEST(getFpgaDiagnosticsSettingsWPJumper, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    0, // incorrect data size
	    0  // data size
	};
	auto data = reinterpret_cast<nsm_fpga_diagnostics_settings_wp_jumper *>(
	    data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    NULL, msg_len, &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    response, msg_len, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_resp(
	    response, msg_len, &cc, NULL, &reason_code, (uint8_t *)data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    response, msg_len - data_byte.size(), &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    response, msg_len, &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getPowerSupplyStatus, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_supply_status_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t status = 0x02;
	uint16_t reasonCode = ERR_NULL;

	auto rc = encode_get_power_supply_status_resp(
	    0, NSM_SUCCESS, reasonCode, status, response);

	struct nsm_get_power_supply_status_resp *resp =
	    reinterpret_cast<struct nsm_get_power_supply_status_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), le16toh(resp->hdr.data_size));
	EXPECT_EQ(status, resp->power_supply_status);
}

TEST(getPowerSupplyStatus, testGoodDecodeResponse)
{
#define EXPECTED_POWER_SUPPLY_STATUS_LSB 0x02
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    1,
	    0,				      // data size
	    EXPECTED_POWER_SUPPLY_STATUS_LSB, // status
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint8_t status = 0;

	auto rc = decode_get_power_supply_status_resp(response, msg_len, &cc,
						      &reasonCode, &status);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(status, EXPECTED_POWER_SUPPLY_STATUS_LSB);
}

TEST(getPowerSupplyStatus, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    0,
	    0, // incorrect data size
	    0};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t status = 0;

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_power_supply_status_resp(NULL, msg_len, &cc,
						      &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_power_supply_status_resp(response, msg_len, NULL,
						 &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_power_supply_status_resp(response, msg_len, &cc,
						 &reason_code, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_power_supply_status_resp(response, msg_len - 1, &cc,
						 &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_power_supply_status_resp(response, msg_len, &cc,
						 &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getGpusPresence, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_get_gpu_presence_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t presence = 0b00111001;
	uint16_t reasonCode = ERR_NULL;

	auto rc = encode_get_gpu_presence_resp(0, NSM_SUCCESS, reasonCode,
					       presence, response);

	struct nsm_get_gpu_presence_resp *resp =
	    reinterpret_cast<struct nsm_get_gpu_presence_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), le16toh(resp->hdr.data_size));
	EXPECT_EQ(presence, resp->presence);
}

TEST(getGpusPresence, testGoodDecodeResponse)
{
#define EXPECTED_PRESENCE_LSB 0b00111001
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    1,
	    0,			   // data size
	    EXPECTED_PRESENCE_LSB, // status
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint8_t presence = 0;

	auto rc = decode_get_gpu_presence_resp(response, msg_len, &cc,
					       &reasonCode, &presence);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(presence, EXPECTED_PRESENCE_LSB);
}

TEST(getGpusPresence, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    0,
	    0, // incorrect data size
	    0};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t presence = 0;

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_gpu_presence_resp(NULL, msg_len, &cc, &reason_code,
					       &presence);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_presence_resp(response, msg_len, NULL, &reason_code,
					  &presence);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_presence_resp(response, msg_len, &cc, &reason_code,
					  NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_presence_resp(response, msg_len - 1, &cc,
					  &reason_code, &presence);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_gpu_presence_resp(response, msg_len, &cc, &reason_code,
					  &presence);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getGpusPowerStatus, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_get_gpu_power_status_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t status = 0x02;
	uint16_t reasonCode = ERR_NULL;

	auto rc = encode_get_gpu_power_status_resp(0, NSM_SUCCESS, reasonCode,
						   status, response);

	struct nsm_get_gpu_power_status_resp *resp =
	    reinterpret_cast<struct nsm_get_gpu_power_status_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), le16toh(resp->hdr.data_size));
	EXPECT_EQ(status, resp->power_status);
}

TEST(getGpusPowerStatus, testGoodDecodeResponse)
{
#define EXPECTED_STATUS_LSB 0b11001011
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    1,
	    0,			 // data size
	    EXPECTED_STATUS_LSB, // status
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint8_t status = 0;

	auto rc = decode_get_gpu_power_status_resp(response, msg_len, &cc,
						   &reasonCode, &status);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(status, EXPECTED_STATUS_LSB);
}

TEST(getGpusPowerStatus, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    0,
	    0, // incorrect data size
	    0};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t status = 0;

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_gpu_power_status_resp(NULL, msg_len, &cc,
						   &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_power_status_resp(response, msg_len, NULL,
					      &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_power_status_resp(response, msg_len, &cc,
					      &reason_code, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_power_status_resp(response, msg_len - 1, &cc,
					      &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_gpu_power_status_resp(response, msg_len, &cc,
					      &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getFpgaDiagnosticsSettingsGpuIstMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpu_ist_mode_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	uint8_t data = 0b01111001;
	uint8_t data_test = data;

	auto rc = encode_get_gpu_ist_mode_resp(0, NSM_SUCCESS, reason_code,
					       data, response);

	auto resp = reinterpret_cast<nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->command);
	EXPECT_EQ(sizeof(uint8_t), le16toh(resp->data_size));
	EXPECT_EQ(data_test, data);
}

TEST(getFpgaDiagnosticsSettingsGpuIstMode, testGoodDecodeResponse)
{
	uint8_t data = 0x01;
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    1,
	    0, // data size
	    data,
	};
	auto data_test = data;
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_get_gpu_ist_mode_resp(response, msg_len, &cc,
					       &reason_code, &data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_test, data);
}

TEST(getFpgaDiagnosticsSettingsGpuIstMode, testBadDecodeResponse)
{
	uint8_t data = 0;
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    0, // incorrect data size
	    0, // data size
	    data,
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_gpu_ist_mode_resp(NULL, msg_len, &cc, &reason_code,
					       &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_ist_mode_resp(response, msg_len, NULL, &reason_code,
					  &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_ist_mode_resp(response, msg_len, &cc, &reason_code,
					  NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_ist_mode_resp(response, msg_len - 1, &cc,
					  &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_gpu_ist_mode_resp(response, msg_len, &cc, &reason_code,
					  &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(enableDisableGpuIstMode, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_gpu_ist_mode_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t device_index = ALL_GPUS_DEVICE_INDEX;
	uint8_t value = 0;

	auto rc = encode_enable_disable_gpu_ist_mode_req(0, device_index, value,
							 request);

	auto req = reinterpret_cast<nsm_enable_disable_gpu_ist_mode_req *>(
	    request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_ENABLE_DISABLE_GPU_IST_MODE, req->hdr.command);
	EXPECT_EQ(2, req->hdr.data_size);
	EXPECT_EQ(device_index, req->device_index);
	EXPECT_EQ(value, req->value);
}

TEST(enableDisableGpuIstMode, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
	    NSM_ENABLE_DISABLE_GPU_IST_MODE, // command
	    2,				     // data size
	    0,				     // device_index
	    1,				     // set
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	uint8_t device_index;
	uint8_t value;
	auto rc = decode_enable_disable_gpu_ist_mode_req(request, msg_len,
							 &device_index, &value);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, device_index);
	EXPECT_EQ(1, value);
}

TEST(enableDisableGpuIstMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_enable_disable_gpu_ist_mode_resp(
	    0, NSM_SUCCESS, reason_code, response);

	auto resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_ENABLE_DISABLE_GPU_IST_MODE, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(enableDisableGpuIstMode, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
	    NSM_ENABLE_DISABLE_GPU_IST_MODE, // command
	    0,				     // completion code
	    0,
	    0,
	    0,
	    0 // data size
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_enable_disable_gpu_ist_mode_resp(response, msg_len,
							  &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(enableDisableGpuIstMode, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
	    NSM_ENABLE_DISABLE_GPU_IST_MODE, // command
	    0,				     // completion code
	    0,
	    0,
	    1, // incorrect data size
	    0, // data size
	    0, // invalid data byte
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_enable_disable_gpu_ist_mode_resp(NULL, msg_len, &cc,
							  &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_enable_disable_gpu_ist_mode_resp(response, msg_len, NULL,
						     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_enable_disable_gpu_ist_mode_resp(response, msg_len, &cc,
						     NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_enable_disable_gpu_ist_mode_resp(response, msg_len, &cc,
						     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	rc = decode_enable_disable_gpu_ist_mode_resp(response, msg_len - 1, &cc,
						     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

void testGetReconfigurationPermissionsV1EncodeRequest(
    reconfiguration_permissions_v1_index settingIndex)
{

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_reconfiguration_permissions_v1_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_reconfiguration_permissions_v1_req(0, settingIndex,
								request);

	auto req =
	    reinterpret_cast<nsm_get_reconfiguration_permissions_v1_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_RECONFIGURATION_PERMISSIONS_V1, req->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), req->hdr.data_size);
	EXPECT_EQ(settingIndex, req->setting_index);
}
void testGetReconfigurationPermissionsV1EncodeResponse(
    reconfiguration_permissions_v1_index expectedSettingIndex)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_RECONFIGURATION_PERMISSIONS_V1, // command
	    1,					    // data size
	    (uint8_t)expectedSettingIndex	    // data_index
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	reconfiguration_permissions_v1_index settingIndex;
	auto rc = decode_get_reconfiguration_permissions_v1_req(
	    request, msg_len, &settingIndex);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(expectedSettingIndex, settingIndex);
}

TEST(getReconfigurationPermissionsV1, testRequests)
{
	for (auto di = (uint8_t)RP_IN_SYSTEM_TEST;
	     di <= (uint8_t)RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2; di++) {
		auto settingIndex = reconfiguration_permissions_v1_index(di);
		testGetReconfigurationPermissionsV1EncodeRequest(settingIndex);
		testGetReconfigurationPermissionsV1EncodeResponse(settingIndex);
	}
}

TEST(getReconfigurationPermissionsV1, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_get_reconfiguration_permissions_v1_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	nsm_reconfiguration_permissions_v1 data;
	data.persistent = 1;

	auto rc = encode_get_reconfiguration_permissions_v1_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	auto resp =
	    reinterpret_cast<nsm_get_reconfiguration_permissions_v1_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_RECONFIGURATION_PERMISSIONS_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_reconfiguration_permissions_v1),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(1, data.persistent);
}

TEST(getReconfigurationPermissionsV1, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_RECONFIGURATION_PERMISSIONS_V1, // command
	    0,					    // completion code
	    0,
	    0,
	    1,
	    0,		// data size
	    0b00000110, // data
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	nsm_reconfiguration_permissions_v1 data;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_get_reconfiguration_permissions_v1_resp(
	    response, msg_len, &cc, &reason_code, &data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0, data.oneshot);
	EXPECT_EQ(1, data.persistent);
	EXPECT_EQ(1, data.flr_persistent);
}

TEST(getReconfigurationPermissionsV1, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_RECONFIGURATION_PERMISSIONS_V1, // command
	    0,					    // completion code
	    0,
	    0,
	    0, // incorrect data size
	    0  // data size
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	nsm_reconfiguration_permissions_v1 data;
	auto rc = decode_get_reconfiguration_permissions_v1_resp(
	    NULL, msg_len, &cc, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_reconfiguration_permissions_v1_resp(
	    response, msg_len, NULL, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_reconfiguration_permissions_v1_resp(response, msg_len,
							    &cc, NULL, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_reconfiguration_permissions_v1_resp(
	    response, msg_len, &cc, &reason_code, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_reconfiguration_permissions_v1_resp(
	    response, msg_len - 1, &cc, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_reconfiguration_permissions_v1_resp(
	    response, msg_len, &cc, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

void testSetReconfigurationPermissionsV1EncodeRequest(
    reconfiguration_permissions_v1_index settingIndex,
    reconfiguration_permissions_v1_setting configuration, uint8_t permission)
{
	Request requestMsg(sizeof(nsm_msg_hdr) +
			   sizeof(nsm_set_reconfiguration_permissions_v1_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_set_reconfiguration_permissions_v1_req(
	    0, settingIndex, configuration, permission, request);

	auto req =
	    reinterpret_cast<nsm_set_reconfiguration_permissions_v1_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_RECONFIGURATION_PERMISSIONS_V1, req->hdr.command);
	EXPECT_EQ(3, req->hdr.data_size);
	EXPECT_EQ(settingIndex, req->setting_index);
	EXPECT_EQ(configuration, req->configuration);
	EXPECT_EQ(permission, req->permission);
}

TEST(setReconfigurationPermissionsV1, testGoodEncodeRequest)
{
	for (auto si = 0; si <= int(RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2);
	     si++) {
		for (auto ci = 0; ci < int(RP_ONESHOT_FLR); ci++) {
			auto settingIndex =
			    reconfiguration_permissions_v1_index(si);
			auto configuration =
			    reconfiguration_permissions_v1_setting(ci);
			testSetReconfigurationPermissionsV1EncodeRequest(
			    settingIndex, configuration, 1);
			testSetReconfigurationPermissionsV1EncodeRequest(
			    settingIndex, configuration, 0);
		}
	}
}

TEST(setReconfigurationPermissionsV1, testGoodDecodeRequest)
{
	Request requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_RECONFIGURATION_PERMISSIONS_V1, // command
	    3,					    // data size
	    3,					    // settingIndex
	    1,					    // configuration
	    1,					    // set
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto msg_len = requestMsg.size();

	auto settingIndex = RP_IN_SYSTEM_TEST;
	auto configuration = RP_ONESHOOT_HOT_RESET;
	uint8_t permission = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len, &settingIndex, &configuration, &permission);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(RP_BAR0_FIREWALL, settingIndex);
	EXPECT_EQ(RP_PERSISTENT, configuration);
	EXPECT_EQ(1, permission);
}

TEST(setReconfigurationPermissionsV1, testBadDecodeRequest)
{
	Request requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_RECONFIGURATION_PERMISSIONS_V1, // command
	    0,					    // incorect data size
	};
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto msg_len = requestMsg.size();

	auto settingIndex = RP_IN_SYSTEM_TEST;
	auto configuration = RP_ONESHOOT_HOT_RESET;
	uint8_t permission = 0;

	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    NULL, msg_len, &settingIndex, &configuration, &permission);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len, NULL, &configuration, &permission);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len, &settingIndex, NULL, &permission);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len, &settingIndex, &configuration, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len, &settingIndex, &configuration, &permission);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len - 1, &settingIndex, &configuration, &permission);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(setReconfigurationPermissionsV1, testGoodEncodeResponse)
{
	Response responseMsg(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_set_reconfiguration_permissions_v1_resp(
	    0, NSM_SUCCESS, reason_code, response);

	auto resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_RECONFIGURATION_PERMISSIONS_V1, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(setReconfigurationPermissionsV1, testGoodDecodeResponse)
{
	Response responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_RECONFIGURATION_PERMISSIONS_V1, // command
	    0,					    // completion code
	    0,
	    0,
	    0,
	    0 // data size
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_set_reconfiguration_permissions_v1_resp(
	    response, msg_len, &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(setReconfigurationPermissionsV1, testBadDecodeResponse)
{
	Response responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_RECONFIGURATION_PERMISSIONS_V1, // command
	    0,					    // completion code
	    0,
	    0,
	    1, // incorrect data size
	    0, // data size
	    0, // invalid data byte
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_set_reconfiguration_permissions_v1_resp(
	    NULL, msg_len, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_resp(response, msg_len,
							    NULL, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_resp(response, msg_len,
							    &cc, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_resp(response, msg_len,
							    &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	rc = decode_set_reconfiguration_permissions_v1_resp(
	    response, msg_len - 1, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}