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
#include "network-ports.h"
#include <gtest/gtest.h>

TEST(getPortTelemetryCounter, testGoodEncodeRequest)
{
	std::vector<uint8_t> request_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_req));

	uint8_t port_number = 3;

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());

	auto rc =
	    encode_get_port_telemetry_counter_req(0, port_number, request);

	nsm_get_port_telemetry_counter_req *req =
	    reinterpret_cast<nsm_get_port_telemetry_counter_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_PORT_TELEMETRY_COUNTER, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(3, req->port_number);
}

TEST(getPortTelemetryCounter, testBadEncodeRequest)
{
	std::vector<uint8_t> request_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_req));

	auto rc = encode_get_port_telemetry_counter_req(0, 0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getPortTelemetryCounter, testGoodDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x80,			    // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    1,				    // data size
	    3				    // portNumber
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());

	size_t msg_len = request_msg.size();

	uint8_t port_number = 0;
	auto rc = decode_get_port_telemetry_counter_req(request, msg_len,
							&port_number);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(3, port_number);
}

TEST(getPortTelemetryCounter, testBadDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x80,			    // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0,				    // data size [it should be 1]
	    3				    // portNumber
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());
	uint8_t port_num = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(nsm_get_port_telemetry_counter_req);

	auto rc = decode_get_port_telemetry_counter_req(nullptr, 0, &port_num);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_port_telemetry_counter_req(request, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_port_telemetry_counter_req(request, 0, &port_num);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_port_telemetry_counter_req(request, msg_len, &port_num);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getPortTelemetryCounter, testGoodEncodeResponseCCSuccess)
{
	std::vector<uint8_t> data{
	    0xF7, 0x5A, 0x3E, 0x00, /*for supported counters, [for CX-7]*/
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00,
	}; /*for counter values, 8 bytes each*/
	auto port_tel_data =
	    reinterpret_cast<nsm_port_counter_data *>(data.data());
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// test for cc = 0x0 [NSM_SUCCESS]
	auto rc = encode_get_port_telemetry_counter_resp(
	    0, NSM_SUCCESS, reason_code, port_tel_data, response);

	struct nsm_get_port_telemetry_counter_resp *resp =
	    reinterpret_cast<struct nsm_get_port_telemetry_counter_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_PORT_TELEMETRY_COUNTER, resp->hdr.command);
	EXPECT_EQ(NSM_SUCCESS, resp->hdr.completion_code);
	EXPECT_EQ(htole16(data.size()), resp->hdr.data_size);
}

TEST(getPortTelemetryCounter, testGoodEncodeResponseCCError)
{
	std::vector<uint8_t> data{
	    0xF7, 0x5A, 0x3E, 0x00, /*for supported counters, [for CX-7]*/
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00,
	}; /*for counter values, 8 bytes each*/
	auto port_tel_data =
	    reinterpret_cast<nsm_port_counter_data *>(data.data());
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// test for cc = 0x1 [NSM_ERROR]
	auto rc = encode_get_port_telemetry_counter_resp(
	    0, NSM_ERROR, reason_code, port_tel_data, response);

	struct nsm_common_non_success_resp *resp =
	    reinterpret_cast<struct nsm_common_non_success_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_PORT_TELEMETRY_COUNTER, resp->command);
	EXPECT_EQ(NSM_ERROR, resp->completion_code);
	EXPECT_EQ(htole16(reason_code), resp->reason_code);
}

TEST(getPortTelemetryCounter, testBadEncodeResponse)
{
	uint8_t port_data[PORT_COUNTER_TELEMETRY_DATA_SIZE];
	auto port_tel_data =
	    reinterpret_cast<nsm_port_counter_data *>(port_data);
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	auto rc = encode_get_port_telemetry_counter_resp(
	    0, NSM_SUCCESS, reason_code, port_tel_data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, reason_code,
						    nullptr, response);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getPortTelemetryCounter, testGoodDecodeResponseCCSuccess)
{
	// test when CC is NSM_SUCCESS and port telemetry data payload is
	// correct
	std::vector<uint8_t> data_orig{
	    0xF7, 0x5A, 0x3E, 0x00, /*for supported counters, [for CX-7]*/
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00,
	}; /*for counter values, 8 bytes each*/
	auto port_data_orig =
	    reinterpret_cast<nsm_port_counter_data *>(data_orig.data());

	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0x00,			    // completion code
	    0x00,			    // reserved
	    0x00,
	    0xCC, // data size
	    0x00};
	response_msg.insert(response_msg.end(), data_orig.begin(),
			    data_orig.end());

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_port_counter_data port_tel_data;

	auto rc = decode_get_port_telemetry_counter_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &port_tel_data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 0x00CC);
	EXPECT_EQ(port_tel_data.port_rcv_pkts,
		  le64toh(port_data_orig->port_rcv_pkts));
	// just checking some starting data and ending data
	EXPECT_EQ(port_tel_data.xmit_wait, le64toh(port_data_orig->xmit_wait));
}

TEST(getPortTelemetryCounter, testGoodDecodeResponseCCError)
{
	// test when CC is NSM_ERROR and port telemetry data payload is empty
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0x01,			    // completion code
	    0x00,			    // reason code
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_port_counter_data port_tel_data;

	auto rc = decode_get_port_telemetry_counter_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &port_tel_data);

	EXPECT_EQ(cc, NSM_ERROR);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(reason_code, 0x0000);
}

TEST(getPortTelemetryCounter, testBadDecodeResponseWithPayload)
{
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0x01, // completion code [0x01 - NSM_ERROR]
	    0x00, // reserved
	    0x00,
	    0x00, // data size [it should not 00]
	    0x00,
	    // port counter data
	    0xF7, 0x5A, 0x3E, 0x00, /*for supported counters, [for CX-7]*/
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00 /*for counter values, 8 bytes each*/
	};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_port_counter_data port_tel_data;

	auto rc = decode_get_port_telemetry_counter_resp(
	    nullptr, msg_len, &cc, &reason_code, &data_size, &port_tel_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_port_telemetry_counter_resp(response, msg_len, nullptr,
						    &reason_code, &data_size,
						    &port_tel_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_port_telemetry_counter_resp(
	    response, msg_len, &cc, nullptr, &data_size, &port_tel_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_port_telemetry_counter_resp(
	    response, msg_len, &cc, &reason_code, nullptr, &port_tel_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_port_telemetry_counter_resp(
	    response, msg_len, &cc, &reason_code, &data_size, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_port_telemetry_counter_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &port_tel_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_ERROR);

	response_msg[6] = 0x00; // making CC - NSM_SUCCESS
	rc = decode_get_port_telemetry_counter_resp(
	    response, msg_len - 10, &cc, &reason_code, &data_size,
	    &port_tel_data); //-10 from total size which means we should get
			     // error
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_port_telemetry_counter_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &port_tel_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getPortTelemetryCounter, testGoodhtolePortCounterData)
{
	std::vector<uint8_t> port_data_orig{
	    0xF7, 0x5A, 0x3E, 0x00, /*for supported counters, [for CX-7]*/
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00,
	}; /*for counter values, 8 bytes each*/
	std::vector<uint8_t> port_data_converted = port_data_orig;
	auto data_orig =
	    reinterpret_cast<nsm_port_counter_data *>(port_data_orig.data());
	auto data_converted = reinterpret_cast<nsm_port_counter_data *>(
	    port_data_converted.data());

	uint16_t reason_code = ERR_NULL;
	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_resp));
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// since htolePortCounterData() is a static func we cannot access it
	// directly
	auto rc = encode_get_port_telemetry_counter_resp(
	    0, NSM_SUCCESS, reason_code, data_converted, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_converted->port_rcv_pkts,
		  htole64(data_orig->port_rcv_pkts));
	// only checking first and last counters
	EXPECT_EQ(data_converted->xmit_wait, htole64(data_orig->xmit_wait));
}

TEST(getPortTelemetryCounter, testGoodletohPortCounterData)
{
	std::vector<uint8_t> data_orig{
	    0xF7, 0x5A, 0x3E, 0x00, /*for supported counters, [for CX-7]*/
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00,
	}; /*for counter values, 8 bytes each*/
	auto port_data_orig =
	    reinterpret_cast<nsm_port_counter_data *>(data_orig.data());

	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0x00,			    // completion code
	    0x00,			    // reserved
	    0x00,
	    0xCC, // data size
	    0x00};

	response_msg.insert(response_msg.end(), data_orig.begin(),
			    data_orig.end());
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_port_counter_data port_data_converted;

	// since letohPortCounterData() is a static func we cannot access it
	// directly
	auto rc = decode_get_port_telemetry_counter_resp(
	    response, msg_len, &cc, &reason_code, &data_size,
	    &port_data_converted);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(port_data_converted.port_rcv_pkts,
		  le64toh(port_data_orig->port_rcv_pkts));
	// only checking first and last counters
	EXPECT_EQ(port_data_converted.xmit_wait,
		  le64toh(port_data_orig->xmit_wait));
}

TEST(queryPortCharacteristics, testGoodEncodeRequest)
{
	std::vector<uint8_t> request_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_req));

	uint8_t port_number = 2;

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());

	auto rc =
	    encode_query_port_characteristics_req(0, port_number, request);

	nsm_query_port_characteristics_req *req =
	    reinterpret_cast<nsm_query_port_characteristics_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_PORT_CHARACTERISTICS, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(2, req->port_number);
}

TEST(queryPortCharacteristics, testBadEncodeRequest)
{
	std::vector<uint8_t> request_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_req));

	auto rc = encode_query_port_characteristics_req(0, 0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryPortCharacteristics, testGoodDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x80,			    // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_QUERY_PORT_CHARACTERISTICS, // command
	    1,				    // data size
	    2				    // portNumber
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());

	size_t msg_len = request_msg.size();

	uint8_t port_number = 0;
	auto rc = decode_query_port_characteristics_req(request, msg_len,
							&port_number);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(2, port_number);
}

TEST(queryPortCharacteristics, testBadDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x80,			    // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0,				    // data size [it should be 1]
	    2				    // portNumber
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());
	uint8_t port_num = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(nsm_query_port_characteristics_req);

	auto rc = decode_query_port_characteristics_req(nullptr, 0, &port_num);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_characteristics_req(request, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_characteristics_req(request, msg_len - 2,
						   &port_num);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_port_characteristics_req(request, msg_len, &port_num);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(queryPortCharacteristics, testGoodEncodeResponseCCSuccess)
{
	std::vector<uint8_t> data{0x09, 0x00, 0x00, 0x00, 0x67, 0x00,
				  0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
				  0x06, 0x00, 0x00, 0x00};
	auto port_cha_data =
	    reinterpret_cast<nsm_port_characteristics_data *>(data.data());
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// test for cc = 0x0 [NSM_SUCCESS]
	auto rc = encode_query_port_characteristics_resp(
	    0, NSM_SUCCESS, reason_code, port_cha_data, response);

	struct nsm_query_port_characteristics_resp *resp =
	    reinterpret_cast<struct nsm_query_port_characteristics_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_PORT_CHARACTERISTICS, resp->hdr.command);
	EXPECT_EQ(NSM_SUCCESS, resp->hdr.completion_code);
	EXPECT_EQ(htole16(data.size()), resp->hdr.data_size);
}

TEST(queryPortCharacteristics, testGoodEncodeResponseCCError)
{
	std::vector<uint8_t> data{0x09, 0x00, 0x00, 0x00, 0x67, 0x00,
				  0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
				  0x06, 0x00, 0x00, 0x00};
	auto port_cha_data =
	    reinterpret_cast<nsm_port_characteristics_data *>(data.data());
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// test for cc = 0x1 [NSM_ERROR]
	auto rc = encode_query_port_characteristics_resp(
	    0, NSM_ERROR, reason_code, port_cha_data, response);

	struct nsm_common_non_success_resp *resp =
	    reinterpret_cast<struct nsm_common_non_success_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_PORT_CHARACTERISTICS, resp->command);
	EXPECT_EQ(NSM_ERROR, resp->completion_code);
	EXPECT_EQ(htole16(reason_code), resp->reason_code);
}

TEST(queryPortCharacteristics, testBadEncodeResponse)
{
	std::vector<uint8_t> data(sizeof(nsm_port_characteristics_data), 0);
	auto port_cha_data =
	    reinterpret_cast<nsm_port_characteristics_data *>(data.data());
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	auto rc = encode_query_port_characteristics_resp(
	    0, NSM_SUCCESS, reason_code, port_cha_data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_query_port_characteristics_resp(0, NSM_SUCCESS, reason_code,
						    nullptr, response);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryPortCharacteristics, testGoodDecodeResponseCCSuccess)
{
	// test when CC is NSM_SUCCESS and data payload is correct
	std::vector<uint8_t> data_orig{0x09, 0x00, 0x00, 0x00, 0x67, 0x00,
				       0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
				       0x06, 0x00, 0x00, 0x00};
	auto port_data_orig =
	    reinterpret_cast<nsm_port_characteristics_data *>(data_orig.data());

	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_QUERY_PORT_CHARACTERISTICS, // command
	    0x00,			    // completion code
	    0x00,			    // reserved
	    0x00,
	    0x10, // data size
	    0x00};
	response_msg.insert(response_msg.end(), data_orig.begin(),
			    data_orig.end());

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_port_characteristics_data port_cha_data;

	auto rc = decode_query_port_characteristics_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &port_cha_data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 0x0010);
	EXPECT_EQ(port_cha_data.status, le64toh(port_data_orig->status));
	// just checking some starting data and ending data
	EXPECT_EQ(port_cha_data.status_lane_info,
		  le64toh(port_data_orig->status_lane_info));
}

TEST(queryPortCharacteristics, testGoodDecodeResponseCCError)
{
	// test when CC is NSM_ERROR and data payload is empty
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_QUERY_PORT_CHARACTERISTICS, // command
	    0x01,			    // completion code
	    0x00,			    // reason code
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_port_characteristics_data port_cha_data;

	auto rc = decode_query_port_characteristics_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &port_cha_data);

	EXPECT_EQ(cc, NSM_ERROR);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(reason_code, 0x0000);
}

TEST(queryPortCharacteristics, testBadDecodeResponseWithPayload)
{
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0x01, // completion code [0x01 - NSM_ERROR]
	    0x00, // reserved
	    0x00,
	    0x00, // data size [it should not 00]
	    0x00,
	    0x09,
	    0x00,
	    0x00,
	    0x00,
	    0x67,
	    0x00,
	    0x00,
	    0x00,
	    0x13,
	    0x00,
	    0x00,
	    0x00,
	    0x06,
	    0x00,
	    0x00,
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_port_characteristics_data port_cha_data;

	auto rc = decode_query_port_characteristics_resp(
	    nullptr, msg_len, &cc, &reason_code, &data_size, &port_cha_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_characteristics_resp(response, msg_len, nullptr,
						    &reason_code, &data_size,
						    &port_cha_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_characteristics_resp(
	    response, msg_len, &cc, nullptr, &data_size, &port_cha_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_characteristics_resp(
	    response, msg_len, &cc, &reason_code, nullptr, &port_cha_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_characteristics_resp(
	    response, msg_len, &cc, &reason_code, &data_size, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_characteristics_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &port_cha_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_ERROR);

	response_msg[6] = 0x00; // making CC - NSM_SUCCESS
	rc = decode_query_port_characteristics_resp(
	    response, msg_len - 3, &cc, &reason_code, &data_size,
	    &port_cha_data); //-3 from total size which means we should get
			     // error
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_port_characteristics_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &port_cha_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(queryPortCharacteristics, testGoodhtolePortCharacteristicsData)
{
	std::vector<uint8_t> port_data_orig{0x09, 0x00, 0x00, 0x00, 0x67, 0x00,
					    0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
					    0x06, 0x00, 0x00, 0x00};
	std::vector<uint8_t> port_data_converted = port_data_orig;
	auto data_orig = reinterpret_cast<nsm_port_characteristics_data *>(
	    port_data_orig.data());
	auto data_converted = reinterpret_cast<nsm_port_characteristics_data *>(
	    port_data_converted.data());

	uint16_t reason_code = ERR_NULL;
	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp));
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// since htolePortCharacteristicsData() is a static func we cannot
	// access it directly
	auto rc = encode_query_port_characteristics_resp(
	    0, NSM_SUCCESS, reason_code, data_converted, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_converted->status, htole64(data_orig->status));
	// only checking first and last counters
	EXPECT_EQ(data_converted->status_lane_info,
		  htole64(data_orig->status_lane_info));
}

TEST(queryPortCharacteristics, testGoodletohPortCharacteristicsData)
{
	std::vector<uint8_t> data_orig{0x09, 0x00, 0x00, 0x00, 0x67, 0x00,
				       0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
				       0x06, 0x00, 0x00, 0x00};
	auto port_data_orig =
	    reinterpret_cast<nsm_port_characteristics_data *>(data_orig.data());

	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_QUERY_PORT_CHARACTERISTICS, // command
	    0x00,			    // completion code
	    0x00,			    // reserved
	    0x00,
	    0x10, // data size
	    0x00};

	response_msg.insert(response_msg.end(), data_orig.begin(),
			    data_orig.end());
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	struct nsm_port_characteristics_data port_data_converted;

	// since letohPortCharacteristicsData() is a static func we cannot
	// access it directly
	auto rc = decode_query_port_characteristics_resp(
	    response, msg_len, &cc, &reason_code, &data_size,
	    &port_data_converted);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(port_data_converted.status, le64toh(port_data_orig->status));
	// only checking first and last counters
	EXPECT_EQ(port_data_converted.status_lane_info,
		  le64toh(port_data_orig->status_lane_info));
}

TEST(queryPortStatus, testGoodEncodeRequest)
{
	std::vector<uint8_t> request_msg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_query_port_status_req));

	uint8_t port_number = 4;

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());

	auto rc = encode_query_port_status_req(0, port_number, request);

	nsm_query_port_status_req *req =
	    reinterpret_cast<nsm_query_port_status_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_PORT_STATUS, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(4, req->port_number);
}

TEST(queryPortStatus, testBadEncodeRequest)
{
	std::vector<uint8_t> request_msg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_query_port_status_req));

	auto rc = encode_query_port_status_req(0, 0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryPortStatus, testGoodDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,		   // PCI VID: NVIDIA 0x10DE
	    0x80,		   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		   // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_NETWORK_PORT, // NVIDIA_MSG_TYPE
	    NSM_QUERY_PORT_STATUS, // command
	    1,			   // data size
	    4			   // portNumber
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());

	size_t msg_len = request_msg.size();

	uint8_t port_number = 0;
	auto rc = decode_query_port_status_req(request, msg_len, &port_number);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(4, port_number);
}

TEST(queryPortStatus, testBadDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x80,			    // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0,				    // data size [it should be 1]
	    4				    // portNumber
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());
	uint8_t port_num = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(nsm_get_port_telemetry_counter_req);

	auto rc = decode_query_port_status_req(nullptr, 0, &port_num);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_status_req(request, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_status_req(request, msg_len - 2, &port_num);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_port_status_req(request, msg_len, &port_num);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(queryPortStatus, testGoodEncodeResponseCCSuccess)
{
	uint8_t port_state = 2;
	uint8_t port_status = 1;
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// test for cc = 0x0 [NSM_SUCCESS]
	auto rc = encode_query_port_status_resp(
	    0, NSM_SUCCESS, reason_code, port_state, port_status, response);

	struct nsm_query_port_status_resp *resp =
	    reinterpret_cast<struct nsm_query_port_status_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_PORT_STATUS, resp->hdr.command);
	EXPECT_EQ(NSM_SUCCESS, resp->hdr.completion_code);
	EXPECT_EQ(htole16(sizeof(port_state) + sizeof(port_status)),
		  resp->hdr.data_size);
}

TEST(queryPortStatus, testGoodEncodeResponseCCError)
{
	uint8_t port_state = 2;
	uint8_t port_status = 1;
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// test for cc = 0x1 [NSM_ERROR]
	auto rc = encode_query_port_status_resp(
	    0, NSM_ERROR, reason_code, port_state, port_status, response);

	struct nsm_common_non_success_resp *resp =
	    reinterpret_cast<struct nsm_common_non_success_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_PORT_STATUS, resp->command);
	EXPECT_EQ(NSM_ERROR, resp->completion_code);
	EXPECT_EQ(htole16(reason_code), resp->reason_code);
}

TEST(queryPortStatus, testBadEncodeResponse)
{
	uint8_t port_state = 2;
	uint8_t port_status = 1;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_query_port_status_resp(
	    0, NSM_SUCCESS, reason_code, port_state, port_status, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryPortStatus, testGoodDecodeResponseCCSuccess)
{
	// test when CC is NSM_SUCCESS and data payload is correct
	std::vector<uint8_t> response_msg{
	    0x10, // PCI VID: NVIDIA 0x10DE
	    0xDE,
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0x00,			    // completion code
	    0x00,			    // reserved
	    0x00,
	    0x02, // data size
	    0x00,
	    0x03,  // port state
	    0x02}; // port status

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint8_t port_state = NSM_PORTSTATE_DOWN;
	uint8_t port_status = NSM_PORTSTATUS_DISABLED;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_port_status_resp(response, msg_len, &cc,
						&reason_code, &data_size,
						&port_state, &port_status);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 0x0002);
	EXPECT_EQ(port_state, 0x03);
	EXPECT_EQ(port_status, 0x02);
}

TEST(queryPortStatus, testGoodDecodeResponseCCError)
{
	// test when CC is NSM_ERROR and data payload is empty
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0x01,			    // completion code
	    0x00,			    // reason code
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t port_state = NSM_PORTSTATE_DOWN;
	uint8_t port_status = NSM_PORTSTATUS_DISABLED;

	auto rc = decode_query_port_status_resp(response, msg_len, &cc,
						&reason_code, &data_size,
						&port_state, &port_status);

	EXPECT_EQ(cc, NSM_ERROR);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(reason_code, 0x0000);
}

TEST(queryPortStatus, testBadDecodeResponseWithPayload)
{
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0x01, // completion code [0x01 - NSM_ERROR]
	    0x00, // reserved
	    0x00,
	    0x00, // data size [it should not be 00]
	    0x00,
	    0x02, // port state
	    0x02  // port status
	};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t port_state = NSM_PORTSTATE_DOWN;
	uint8_t port_status = NSM_PORTSTATUS_DISABLED;

	auto rc = decode_query_port_status_resp(nullptr, msg_len, &cc,
						&reason_code, &data_size,
						&port_state, &port_status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_status_resp(response, msg_len, nullptr,
					   &reason_code, &data_size,
					   &port_state, &port_status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_status_resp(response, msg_len, &cc, nullptr,
					   &data_size, &port_state,
					   &port_status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_status_resp(response, msg_len, &cc, &reason_code,
					   nullptr, &port_state, &port_status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_status_resp(response, msg_len, &cc, &reason_code,
					   &data_size, nullptr, &port_status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_status_resp(response, msg_len, &cc, &reason_code,
					   &data_size, &port_state, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_port_status_resp(response, msg_len, &cc, &reason_code,
					   &data_size, &port_state,
					   &port_status);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_ERROR);

	response_msg[6] = 0x00; // making CC - NSM_SUCCESS
	rc = decode_query_port_status_resp(
	    response, msg_len - 4, &cc, &reason_code, &data_size, &port_state,
	    &port_status); //-4 from total size which means we should get
			   // error
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_port_status_resp(response, msg_len, &cc, &reason_code,
					   &data_size, &port_state,
					   &port_status);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(queryPortsAvailable, testGoodEncodeRequest)
{
	std::vector<uint8_t> request_msg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_query_ports_available_req));

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());

	auto rc = encode_query_ports_available_req(0, request);

	nsm_query_ports_available_req *req =
	    reinterpret_cast<nsm_query_ports_available_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_PORTS_AVAILABLE, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(queryPortsAvailable, testBadEncodeRequest)
{
	std::vector<uint8_t> request_msg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_query_ports_available_req));

	auto rc = encode_query_ports_available_req(0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryPortsAvailable, testGoodDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x80,		       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		       // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_NETWORK_PORT,     // NVIDIA_MSG_TYPE
	    NSM_QUERY_PORTS_AVAILABLE, // command
	    0			       // data size
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());

	size_t msg_len = request_msg.size();

	auto rc = decode_query_ports_available_req(request, msg_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(queryPortsAvailable, testBadDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x80,		       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		       // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_NETWORK_PORT,     // NVIDIA_MSG_TYPE
	    NSM_QUERY_PORTS_AVAILABLE, // command
	    1			       // data size [it should not be 1]
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());
	size_t msg_len =
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_query_ports_available_req);

	auto rc = decode_query_ports_available_req(nullptr, 0);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_ports_available_req(request, msg_len - 2);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_ports_available_req(request, msg_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(queryPortsAvailable, testGoodEncodeResponseCCSuccess)
{
	uint16_t reason_code = ERR_NULL;
	uint8_t number_of_ports = 0;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_ports_available_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// test for cc = 0x0 [NSM_SUCCESS]
	auto rc = encode_query_ports_available_resp(0, NSM_SUCCESS, reason_code,
						    number_of_ports, response);

	struct nsm_query_ports_available_resp *resp =
	    reinterpret_cast<struct nsm_query_ports_available_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_PORTS_AVAILABLE, resp->hdr.command);
	EXPECT_EQ(NSM_SUCCESS, resp->hdr.completion_code);
	EXPECT_EQ(htole16(sizeof(number_of_ports)), resp->hdr.data_size);
}

TEST(queryPortsAvailable, testGoodEncodeResponseCCError)
{
	uint8_t number_of_ports = 0;
	uint16_t reason_code = ERR_NULL;

	std::vector<uint8_t> response_msg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());

	// test for cc = 0x1 [NSM_ERROR]
	auto rc = encode_query_ports_available_resp(0, NSM_ERROR, reason_code,
						    number_of_ports, response);

	struct nsm_common_non_success_resp *resp =
	    reinterpret_cast<struct nsm_common_non_success_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_PORTS_AVAILABLE, resp->command);
	EXPECT_EQ(NSM_ERROR, resp->completion_code);
	EXPECT_EQ(htole16(reason_code), resp->reason_code);
}

TEST(queryPortsAvailable, testBadEncodeResponse)
{
	uint8_t number_of_ports = 0;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_query_ports_available_resp(0, NSM_SUCCESS, reason_code,
						    number_of_ports, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryPortsAvailable, testGoodDecodeResponseCCSuccess)
{
	// test when CC is NSM_SUCCESS and data payload is correct
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,     // NVIDIA_MSG_TYPE
	    NSM_QUERY_PORTS_AVAILABLE, // command
	    0x00,		       // completion code
	    0x00,		       // reserved
	    0x00,
	    0x01, // data size
	    0x00,
	    0x02 // number of ports
	};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t number_of_ports = 0;

	auto rc = decode_query_ports_available_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &number_of_ports);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 0x0001);
	EXPECT_EQ(number_of_ports, 0x02);
}

TEST(queryPortsAvailable, testGoodDecodeResponseCCError)
{
	// test when CC is NSM_ERROR and data payload is empty
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,     // NVIDIA_MSG_TYPE
	    NSM_QUERY_PORTS_AVAILABLE, // command
	    0x01,		       // completion code
	    0x00,		       // reason code
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t number_of_ports = 0;

	auto rc = decode_query_ports_available_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &number_of_ports);

	EXPECT_EQ(cc, NSM_ERROR);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(reason_code, 0x0000);
}

TEST(queryPortsAvailable, testBadDecodeResponseWithPayload)
{
	std::vector<uint8_t> response_msg{
	    0x10,
	    0xDE,			    // PCI VID: NVIDIA 0x10DE
	    0x00,			    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_NETWORK_PORT,	    // NVIDIA_MSG_TYPE
	    NSM_GET_PORT_TELEMETRY_COUNTER, // command
	    0x01, // completion code [0x01 - NSM_ERROR]
	    0x00, // reserved
	    0x00,
	    0x00, // data size [it should not 00]
	    0x00,
	    0x04};

	auto response = reinterpret_cast<nsm_msg *>(response_msg.data());
	size_t msg_len = response_msg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t number_of_ports = 0;

	auto rc = decode_query_ports_available_resp(
	    nullptr, msg_len, &cc, &reason_code, &data_size, &number_of_ports);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_ports_available_resp(response, msg_len, nullptr,
					       &reason_code, &data_size,
					       &number_of_ports);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_ports_available_resp(response, msg_len, &cc, nullptr,
					       &data_size, &number_of_ports);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_ports_available_resp(
	    response, msg_len, &cc, &reason_code, nullptr, &number_of_ports);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_ports_available_resp(
	    response, msg_len, &cc, &reason_code, &data_size, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_ports_available_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &number_of_ports);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_ERROR);

	response_msg[6] = 0x00; // making CC - NSM_SUCCESS
	rc = decode_query_ports_available_resp(
	    response, msg_len - 4, &cc, &reason_code, &data_size,
	    &number_of_ports); //-4 from total size which means we should get
			       // error
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_ports_available_resp(
	    response, msg_len, &cc, &reason_code, &data_size, &number_of_ports);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}