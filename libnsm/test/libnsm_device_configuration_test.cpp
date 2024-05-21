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

TEST(getFpgaDiagnosticsSettings, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_fpga_diagnostics_settings_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	fpga_diagnostics_settings_data_index data_index = GET_WP_SETTINGS;

	auto rc =
	    encode_get_fpga_diagnostics_settings_req(0, data_index, request);

	struct nsm_get_fpga_diagnostics_settings_req *req =
	    (struct nsm_get_fpga_diagnostics_settings_req *)request->payload;

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, req->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), req->hdr.data_size);
	EXPECT_EQ(data_index, req->data_index);
}

TEST(getFpgaDiagnosticsSettings, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    1,				       // data size
	    0				       // data_index
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	fpga_diagnostics_settings_data_index data_index;
	auto rc = decode_get_fpga_diagnostics_settings_req(request, msg_len,
							   &data_index);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, data_index);
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
	    8,
	    0 // data size
	};
	auto data = reinterpret_cast<nsm_fpga_diagnostics_settings_wp *>(
	    data_byte.data());
	auto data_test = data;
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
	EXPECT_EQ(data_test->gpu1_4, data->gpu1_4);
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