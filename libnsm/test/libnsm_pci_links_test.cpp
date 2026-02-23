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
#include "pci-links.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common-tests.hpp"

TEST(getPCIePortConfig, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_port_config_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t port_number = 1;
	uint8_t port_type = NSM_PORT_TYPE_UPSTREAM;
	uint8_t port_index = 1;

	auto rc = encode_get_pcie_port_config_req(0, port_number, port_type,
						  port_index, request);

	struct nsm_get_port_config_req *req =
	    reinterpret_cast<struct nsm_get_port_config_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_PORT_CONFIGURATION, req->hdr.command);
	EXPECT_EQ(sizeof(nsm_get_port_config_req) - sizeof(nsm_common_req),
		  req->hdr.data_size);
	EXPECT_EQ(port_number, req->port_number);
	EXPECT_EQ(port_type, req->port_type);
	EXPECT_EQ(port_index, req->port_index);

	// bad encode request test case
	rc = encode_get_pcie_port_config_req(0, 0, 0, 0, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getPCIePortConfig, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{0x10,
					0xDE,		   // pci_vendor_id
					0x80,		   // instance_id
					0x89,		   // ocp_version
					NSM_TYPE_PCI_LINK, // nvidia_msg_type
					NSM_GET_PORT_CONFIGURATION, // command
					0x02,			    // data_size
					0x01,			    // port_type
					0x00}; // port_index

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t port_number = 0;
	uint8_t port_type = 0;
	uint8_t port_index = 0;
	auto rc = decode_get_pcie_port_config_req(
	    request, msg_len, &port_number, &port_type, &port_index);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, port_number);
	EXPECT_EQ(0, port_type);
	EXPECT_EQ(0, port_index);

	// bad decode request test case
	rc = decode_get_pcie_port_config_req(NULL, msg_len, &port_number,
					     &port_type, &port_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_pcie_port_config_req(request, msg_len - 1, &port_number,
					     &port_type, &port_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_pcie_port_config_req(request, msg_len, NULL, &port_type,
					     &port_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_pcie_port_config_req(request, msg_len, &port_number,
					     NULL, &port_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_pcie_port_config_req(request, msg_len, &port_number,
					     &port_type, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getPCIePortConfig, testGoodEncodeResponse)
{
	// good encode response test case
	std::vector<uint8_t> sampleMsg{0x04, 0x01, 0x01};
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_config_aggregate_resp),
	    0);
	responseMsg.reserve(256);
	responseMsg.insert(responseMsg.end(), sampleMsg.begin(),
			   sampleMsg.end());

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_get_port_config_aggregate_resp(
	    0, NSM_GET_PORT_CONFIGURATION, NSM_SUCCESS, 1, response);

	struct nsm_get_port_config_aggregate_resp *resp =
	    reinterpret_cast<struct nsm_get_port_config_aggregate_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_PORT_CONFIGURATION, resp->command);
	EXPECT_EQ(0, resp->completion_code);
	EXPECT_EQ(1, resp->telemetry_count);

	// bad encode response test case
	rc = encode_get_port_config_aggregate_resp(
	    0, NSM_GET_PORT_CONFIGURATION, NSM_SUCCESS, 1, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(setPCIePortConfig, testEncodeRequest)
{
	std::vector<uint8_t> sampleMsg{
	    0x04, // tag_TxAmplitude
	    0x01, // data_size & bit field
	    0x01, // data
	    0x03, // tag_Gen3Preset
	    0x01, // data_size & bit field
	    0x11, // data
	    0x02, // tag_Gen4Preset
	    0x01, // data_size & bit field
	    0x11, // data
	    0x01, // tag_Gen5Preset
	    0x01, // data_size & bit field
	    0x11, // data
	    0x00, // tag_Gen6Preset
	    0x01, // data_size & bit field
	    0x11  // data
	};
	// Initialize the request message with the minimum size
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_config_aggregate_req) -
		1 + sampleMsg.size(),
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint8_t port_number = 0;
	uint8_t port_type = NSM_PORT_TYPE_UPSTREAM;
	uint8_t port_index = 0;
	uint16_t sample_count = 5;
	auto rc = encode_set_port_config_aggregate_req(
	    0, port_number, port_type, port_index, sample_count,
	    sampleMsg.data(), sampleMsg.size(), request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	struct nsm_set_port_config_aggregate_req *req =
	    reinterpret_cast<struct nsm_set_port_config_aggregate_req *>(
		request->payload);
	EXPECT_EQ(NSM_SET_PORT_CONFIGURATION, req->hdr.command);
	EXPECT_EQ(port_number, req->port_number);
	EXPECT_EQ(port_type, req->port_type);
	EXPECT_EQ(port_index, req->port_index);
	EXPECT_EQ(sample_count, req->sample_count);

	// bad encode request test case
	rc = encode_set_port_config_aggregate_req(
	    0, port_number, port_type, port_index, sample_count, NULL,
	    requestMsg.size(), request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_set_port_config_aggregate_req(
	    0, port_number, port_type, port_index, sample_count,
	    requestMsg.data(), requestMsg.size(), NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(setPCIePortConfig, testDecodeRequest)
{
	std::vector<uint8_t> requestMsg{0x10,
					0xDE,		   // pci_vendor_id
					0x80,		   // instance_id
					0x89,		   // ocp_version
					NSM_TYPE_PCI_LINK, // nvidia_msg_type
					NSM_SET_PORT_CONFIGURATION, // command
					0x07,			    // data_size
					0x01,			    // port_type
					0x00,  // port_index
					0x01,  // sample_count
					0x00,  // sample_data_size
					0x04,  // sample_data[0]
					0x01,  // sample_data[1]
					0x00}; // sample_data[2]

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t port_number = 0;
	uint8_t port_type = 0;
	uint8_t port_index = 0;
	uint16_t sample_count = 0;
	const uint8_t *sample_data = NULL;
	size_t sample_data_len = 0;

	auto rc = decode_set_port_config_aggregate_req(
	    request, msg_len, &port_number, &port_type, &port_index,
	    &sample_count, &sample_data, &sample_data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, port_number);
	EXPECT_EQ(NSM_PORT_TYPE_UPSTREAM, port_type);
	EXPECT_EQ(0, port_index);
	EXPECT_EQ(1, sample_count);
	EXPECT_EQ(3, sample_data_len);

	// bad decode request test case
	rc = decode_set_port_config_aggregate_req(
	    NULL, msg_len, &port_number, &port_type, &port_index, &sample_count,
	    &sample_data, &sample_data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_port_config_aggregate_req(
	    request, msg_len, &port_number, &port_type, &port_index,
	    &sample_count, NULL, &sample_data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_port_config_aggregate_req(
	    request, msg_len, &port_number, &port_type, &port_index,
	    &sample_count, &sample_data, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(setPCIePortConfig, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc =
	    encode_set_port_config_aggregate_resp(0, NSM_SUCCESS, 0, response);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SET_PORT_CONFIGURATION, resp->command);
	EXPECT_EQ(0, resp->completion_code);
	EXPECT_EQ(0, resp->reserved);
	EXPECT_EQ(0, resp->data_size);

	// bad encode response test case
	rc = encode_set_port_config_aggregate_resp(0, NSM_SUCCESS, 0, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryScalarGroupTelemetryV1, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_query_scalar_group_telemetry_v1_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t device_id = 0;
	uint8_t group_index = 0;

	auto rc = encode_query_scalar_group_telemetry_v1_req(
	    0, device_id, group_index, request);

	struct nsm_query_scalar_group_telemetry_v1_req *req =
	    (struct nsm_query_scalar_group_telemetry_v1_req *)request->payload;

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, req->hdr.command);
	EXPECT_EQ(2 * sizeof(uint8_t), req->hdr.data_size);
	EXPECT_EQ(device_id, req->device_id);
	EXPECT_EQ(group_index, req->group_index);
}

TEST(queryScalarGroupTelemetryV1, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    2,					 // data size
	    0,					 // device_id
	    0					 // group_index
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	uint8_t device_id = 0;
	uint8_t group_index = 1;
	auto rc = decode_query_scalar_group_telemetry_v1_req(
	    request, msg_len, &device_id, &group_index);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, device_id);
	EXPECT_EQ(0, group_index);
}

TEST(queryScalarGroupTelemetryV1Group0, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_0 data;
	data.pci_vendor_id = 3;
	data.pci_device_id = 3;
	data.pci_subsystem_vendor_id = 3;
	data.pci_subsystem_device_id = 3;
	struct nsm_query_scalar_group_telemetry_group_0 data_test = data;

	auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_0_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_0_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_0),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.pci_vendor_id, le32toh(data.pci_vendor_id));
	EXPECT_EQ(data_test.pci_device_id, le32toh(data.pci_device_id));
	EXPECT_EQ(data_test.pci_subsystem_vendor_id,
		  le32toh(data.pci_subsystem_vendor_id));
	EXPECT_EQ(data_test.pci_subsystem_vendor_id,
		  le32toh(data.pci_subsystem_vendor_id));
}

TEST(queryScalarGroupTelemetryV1Group0, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    16,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_0 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 16);
	EXPECT_EQ(le32toh(data_test->pci_vendor_id), data->pci_vendor_id);
	EXPECT_EQ(le32toh(data_test->pci_device_id), data->pci_device_id);
	EXPECT_EQ(le32toh(data_test->pci_subsystem_vendor_id),
		  data->pci_subsystem_vendor_id);
	EXPECT_EQ(le32toh(data_test->pci_subsystem_device_id),
		  data->pci_subsystem_device_id);
}

TEST(queryScalarGroupTelemetryV1Group0, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    15, // incorrect data size
	    0	// data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_0 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group1, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_1 data;
	data.negotiated_link_speed = 3;
	data.negotiated_link_width = 3;
	data.target_link_speed = 3;
	data.max_link_speed = 3;
	data.max_link_width = 3;
	data.max_read_request_size_bytes = 512;
	data.max_payload_size_bytes = 256;
	data.clock_mode = NSM_PCIE_CLOCK_MODE_COMMON;
	struct nsm_query_scalar_group_telemetry_group_1 data_test = data;

	auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_1_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_1_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_1),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.negotiated_link_speed,
		  le32toh(data.negotiated_link_speed));
	EXPECT_EQ(data_test.max_read_request_size_bytes,
		  le32toh(data.max_read_request_size_bytes));
	EXPECT_EQ(data_test.max_payload_size_bytes,
		  le32toh(data.max_payload_size_bytes));
	EXPECT_EQ(data_test.clock_mode, le32toh(data.clock_mode));
}

TEST(queryScalarGroupTelemetryV1Group1, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x05, 0x00, 0x00, 0x00, // negotiated_link_speed (Gen5)
	    0x10, 0x00, 0x00, 0x00, // negotiated_link_width (x16)
	    0x04, 0x00, 0x00, 0x00, // target_link_speed
	    0x05, 0x00, 0x00, 0x00, // max_link_speed (Gen5)
	    0x10, 0x00, 0x00, 0x00, // max_link_width (x16)
	    0x00, 0x02, 0x00, 0x00, // max_read_request_size_bytes (512)
	    0x00, 0x01, 0x00, 0x00, // max_payload_size_bytes (256)
	    0x01, 0x00, 0x00, 0x00, // clock_mode (common clock)
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    32,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_1 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 32);
	EXPECT_EQ(le32toh(data_test->negotiated_link_speed),
		  data->negotiated_link_speed);
	EXPECT_EQ(le32toh(data_test->max_read_request_size_bytes),
		  data->max_read_request_size_bytes);
	EXPECT_EQ(le32toh(data_test->max_payload_size_bytes),
		  data->max_payload_size_bytes);
	EXPECT_EQ(le32toh(data_test->clock_mode), data->clock_mode);
}

TEST(queryScalarGroupTelemetryV1Group1, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x05, 0x00, 0x00, 0x00, // negotiated_link_speed
	    0x10, 0x00, 0x00, 0x00, // negotiated_link_width
	    0x04, 0x00, 0x00, 0x00, // target_link_speed
	    0x05, 0x00, 0x00, 0x00, // max_link_speed
	    0x10, 0x00, 0x00, 0x00, // max_link_width
	    0x00, 0x02, 0x00, 0x00, // max_read_request_size_bytes
	    0x00, 0x01, 0x00, 0x00, // max_payload_size_bytes
	    0x01, 0x00, 0x00, 0x00, // clock_mode
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    31, // incorrect data size (should be 32)
	    0	// data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_1 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group2, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_2 data;
	data.non_fatal_errors = 1111;
	data.fatal_errors = 2222;
	data.unsupported_request_count = 3333;
	data.correctable_errors = 4444;

	struct nsm_query_scalar_group_telemetry_group_2 data_test = data;
	auto rc = encode_query_scalar_group_telemetry_v1_group2_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_2_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_2_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_2),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.correctable_errors,
		  le32toh(data.correctable_errors));
}

TEST(queryScalarGroupTelemetryV1Group2, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    16,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_2 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 16);
	EXPECT_EQ(le32toh(data_test->correctable_errors),
		  data->correctable_errors);
}

TEST(queryScalarGroupTelemetryV1Group2, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    14, // incorrect data size
	    0	// data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_2 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group3, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;
	struct nsm_query_scalar_group_telemetry_group_3 data;
	data.L0ToRecoveryCount = 8769;
	data.eieos_timeout = 100;
	data.training_seq_errors = 200;
	data.framing_errors = 300;
	data.link_down_count = 400;
	struct nsm_query_scalar_group_telemetry_group_3 data_test = data;
	auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_3_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_3_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_3),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.L0ToRecoveryCount, le32toh(data.L0ToRecoveryCount));
	EXPECT_EQ(data_test.eieos_timeout, le32toh(data.eieos_timeout));
	EXPECT_EQ(data_test.training_seq_errors,
		  le32toh(data.training_seq_errors));
	EXPECT_EQ(data_test.framing_errors, le32toh(data.framing_errors));
	EXPECT_EQ(data_test.link_down_count, le32toh(data.link_down_count));
}

TEST(queryScalarGroupTelemetryV1Group3, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x0A, 0x00, 0x00, 0x00, // L0ToRecoveryCount (10)
	    0x05, 0x00, 0x00, 0x00, // eieos_timeout (5)
	    0x03, 0x00, 0x00, 0x00, // training_seq_errors (3)
	    0x02, 0x00, 0x00, 0x00, // framing_errors (2)
	    0x01, 0x00, 0x00, 0x00, // link_down_count (1)
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    20,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_3 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 20);
	EXPECT_EQ(le32toh(data_test->L0ToRecoveryCount),
		  data->L0ToRecoveryCount);
	EXPECT_EQ(le32toh(data_test->eieos_timeout), data->eieos_timeout);
	EXPECT_EQ(le32toh(data_test->training_seq_errors),
		  data->training_seq_errors);
	EXPECT_EQ(le32toh(data_test->framing_errors), data->framing_errors);
	EXPECT_EQ(le32toh(data_test->link_down_count), data->link_down_count);
}

TEST(queryScalarGroupTelemetryV1Group3, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x0A, 0x00, 0x00, 0x00, // L0ToRecoveryCount (10)
	    0x05, 0x00, 0x00, 0x00, // eieos_timeout (5)
	    0x03, 0x00, 0x00, 0x00, // training_seq_errors (3)
	    0x02, 0x00, 0x00, 0x00, // framing_errors (2)
	    0x01, 0x00, 0x00, 0x00, // link_down_count (1)
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    19, // wrong data size (should be 20)
	    0	// data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_3 *>(
		data_byte.data());

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group4, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_4 data;
	data.recv_err_cnt = 100;
	data.NAK_recv_cnt = 200;
	data.NAK_sent_cnt = 300;
	data.bad_TLP_cnt = 400;
	data.replay_rollover_cnt = 500;
	data.FC_timeout_err_cnt = 600;
	data.replay_cnt = 700;
	data.dllp_crc_errors = 800;

	struct nsm_query_scalar_group_telemetry_group_4 data_test = data;
	auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_4_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_4_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_4),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.recv_err_cnt, le32toh(data.recv_err_cnt));
	EXPECT_EQ(data_test.dllp_crc_errors, le32toh(data.dllp_crc_errors));
}

TEST(queryScalarGroupTelemetryV1Group4, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x64, 0x00, 0x00, 0x00, // recv_err_cnt (100)
	    0xC8, 0x00, 0x00, 0x00, // NAK_recv_cnt (200)
	    0x2C, 0x01, 0x00, 0x00, // NAK_sent_cnt (300)
	    0x90, 0x01, 0x00, 0x00, // bad_TLP_cnt (400)
	    0xF4, 0x01, 0x00, 0x00, // replay_rollover_cnt (500)
	    0x58, 0x02, 0x00, 0x00, // FC_timeout_err_cnt (600)
	    0xBC, 0x02, 0x00, 0x00, // replay_cnt (700)
	    0x20, 0x03, 0x00, 0x00, // dllp_crc_errors (800)
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    32,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_4 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);

	EXPECT_EQ(data_size, 32);
	EXPECT_EQ(le32toh(data_test->recv_err_cnt), data->recv_err_cnt);
	EXPECT_EQ(le32toh(data_test->NAK_recv_cnt), data->NAK_recv_cnt);
	EXPECT_EQ(le32toh(data_test->NAK_sent_cnt), data->NAK_sent_cnt);
	EXPECT_EQ(le32toh(data_test->bad_TLP_cnt), data->bad_TLP_cnt);
	EXPECT_EQ(le32toh(data_test->replay_rollover_cnt),
		  data->replay_rollover_cnt);
	EXPECT_EQ(le32toh(data_test->FC_timeout_err_cnt),
		  data->FC_timeout_err_cnt);
	EXPECT_EQ(le32toh(data_test->replay_cnt), data->replay_cnt);
	EXPECT_EQ(le32toh(data_test->dllp_crc_errors), data->dllp_crc_errors);
}

TEST(queryScalarGroupTelemetryV1Group4, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x64, 0x00, 0x00, 0x00, // recv_err_cnt
	    0xC8, 0x00, 0x00, 0x00, // NAK_recv_cnt
	    0x2C, 0x01, 0x00, 0x00, // NAK_sent_cnt
	    0x90, 0x01, 0x00, 0x00, // bad_TLP_cnt
	    0xF4, 0x01, 0x00, 0x00, // replay_rollover_cnt
	    0x58, 0x02, 0x00, 0x00, // FC_timeout_err_cnt
	    0xBC, 0x02, 0x00, 0x00, // replay_cnt
	    0x20, 0x03, 0x00, 0x00, // dllp_crc_errors
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    31, // wrong data size
	    0	// data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_4 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group5, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;
	struct nsm_query_scalar_group_telemetry_group_5 data;
	data.PCIeTXDwords = 8769000;
	data.PCIeRXDwords = 876654;

	struct nsm_query_scalar_group_telemetry_group_5 data_test = data;
	auto rc = encode_query_scalar_group_telemetry_v1_group5_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_5_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_5_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_5),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.PCIeTXDwords, le32toh(data.PCIeTXDwords));
}

TEST(queryScalarGroupTelemetryV1Group5, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    8,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_5 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 8);
	EXPECT_EQ(le32toh(data_test->PCIeRXDwords), data->PCIeRXDwords);
}

TEST(queryScalarGroupTelemetryV1Group5, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    7,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_5 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group6, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_6 data;
	data.invalid_flit_counter = 3;
	data.ltssm_state = 3;
	struct nsm_query_scalar_group_telemetry_group_6 data_test = data;

	auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_6_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_6_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_6),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.invalid_flit_counter,
		  le32toh(data.invalid_flit_counter));
	EXPECT_EQ(data_test.ltssm_state, le32toh(data.ltssm_state));
}

TEST(queryScalarGroupTelemetryV1Group6, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    8,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_6 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 8);
	EXPECT_EQ(le32toh(data_test->invalid_flit_counter),
		  data->invalid_flit_counter);
	EXPECT_EQ(le32toh(data_test->ltssm_state), data->ltssm_state);
}

TEST(queryScalarGroupTelemetryV1Group6, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x09, 0x08, 0x07,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    7, // incorrect data size
	    0  // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_6 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group8, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_8_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_8 data;
	for (int idx = 1; idx <= TOTAL_PCIE_LANE_COUNT; idx++) {
		data.error_counts[idx - 1] = 200 * idx;
	}
	struct nsm_query_scalar_group_telemetry_group_8 data_test = data;

	auto rc = encode_query_scalar_group_telemetry_v1_group8_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_8_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_8_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_8),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.error_counts[0], le32toh(data.error_counts[0]));
}

TEST(queryScalarGroupTelemetryV1Group8, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte;
	for (int idx = 0; idx < TOTAL_PCIE_LANE_COUNT; idx++) {
		data_byte.push_back(idx * 0);
		data_byte.push_back(idx * 1);
		data_byte.push_back(idx * 2);
		data_byte.push_back(idx * 3);
	}

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    TOTAL_PCIE_LANE_COUNT * 4,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_8 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group8_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);

	EXPECT_EQ(data_size, TOTAL_PCIE_LANE_COUNT * 4);
	EXPECT_EQ(le32toh(data_test->error_counts[0]), data->error_counts[0]);
}

TEST(queryScalarGroupTelemetryV1Group8, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte;
	for (int idx = 0; idx < TOTAL_PCIE_LANE_COUNT; idx++) {
		data_byte.push_back(idx * 0);
		data_byte.push_back(idx * 1);
		data_byte.push_back(idx * 2);
		data_byte.push_back(idx * 3);
	}

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    TOTAL_PCIE_LANE_COUNT * 4 - 1,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_8 *>(
		data_byte.data());

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group8_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group8_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group8_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group8_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group8_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group9, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_9 data;
	data.aer_uncorrectable_error_status = 2456;
	data.aer_correctable_error_status = 3425;
	struct nsm_query_scalar_group_telemetry_group_9 data_test = data;

	auto rc = encode_query_scalar_group_telemetry_v1_group9_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_9_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_9_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_9),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.aer_correctable_error_status,
		  le32toh(data.aer_correctable_error_status));
	EXPECT_EQ(data_test.aer_uncorrectable_error_status,
		  le32toh(data.aer_uncorrectable_error_status));
}

TEST(queryScalarGroupTelemetryV1Group9, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    8,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_9 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group9_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);

	EXPECT_EQ(data_size, 8);
	EXPECT_EQ(le32toh(data_test->aer_uncorrectable_error_status),
		  data->aer_uncorrectable_error_status);
	EXPECT_EQ(le32toh(data_test->aer_correctable_error_status),
		  data->aer_correctable_error_status);
}

TEST(queryScalarGroupTelemetryV1Group9, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    9,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_9 *>(
		data_byte.data());

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group9_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group9_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group9_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group9_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group9_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(pcieFundamentalReset, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_assert_pcie_fundamental_reset_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t device_index = 1;
	uint8_t action = 0;
	auto rc = encode_assert_pcie_fundamental_reset_req(0, device_index,
							   action, request);
	struct nsm_assert_pcie_fundamental_reset_req *req =
	    reinterpret_cast<struct nsm_assert_pcie_fundamental_reset_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_ASSERT_PCIE_FUNDAMENTAL_RESET, req->hdr.command);
	EXPECT_EQ(2, req->hdr.data_size);
	EXPECT_EQ(device_index, req->device_index);
	EXPECT_EQ(action, req->action);
}

TEST(pcieFundamentalReset, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_ASSERT_PCIE_FUNDAMENTAL_RESET, // command
	    2,				       // data size
	    1,				       // device_index
	    0				       // action
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t device_index;
	uint8_t action;
	auto rc = decode_assert_pcie_fundamental_reset_req(
	    request, msg_len, &device_index, &action);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, device_index);
	EXPECT_EQ(0, action);
}

TEST(pcieFundamentalReset, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_assert_pcie_fundamental_reset_resp(
	    0, NSM_SUCCESS, reason_code, response);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_ASSERT_PCIE_FUNDAMENTAL_RESET, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(pcieFundamentalReset, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_ASSERT_PCIE_FUNDAMENTAL_RESET, // command
	    0,				       // completion code
	    0,				       // reserved
	    0,				       // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_assert_pcie_fundamental_reset_resp(
	    response, msg_len, &cc, &data_size, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0, data_size);
}

TEST(pcieFundamentalReset, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_ASSERT_PCIE_FUNDAMENTAL_RESET, // command
	    0,				       // completion code
	    0,				       // reserved
	    0,				       // reserved
	    0,				       // data size
	    0				       // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_assert_pcie_fundamental_reset_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_assert_pcie_fundamental_reset_resp(
	    response, msg_len, NULL, &data_size, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_assert_pcie_fundamental_reset_resp(response, msg_len, &cc,
						       NULL, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_assert_pcie_fundamental_reset_resp(
	    response, msg_len - 1, &cc, &data_size, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(clearScalarDataSource, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_clear_data_source_v1_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t device_index = 1;
	uint8_t groupId = 8;
	uint8_t dsId = 2;
	auto rc = encode_clear_data_source_v1_req(0, device_index, groupId,
						  dsId, request);
	struct nsm_clear_data_source_v1_req *req =
	    reinterpret_cast<struct nsm_clear_data_source_v1_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_CLEAR_DATA_SOURCE_V1, req->hdr.command);
	EXPECT_EQ(3, req->hdr.data_size);
	EXPECT_EQ(device_index, req->device_index);
	EXPECT_EQ(groupId, req->groupId);
	EXPECT_EQ(dsId, req->dsId);
}

TEST(clearScalarDataSource, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		      // PCI VID: NVIDIA 0x10DE
	    0x80,		      // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		      // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK,	      // NVIDIA_MSG_TYPE
	    NSM_CLEAR_DATA_SOURCE_V1, // command
	    3,			      // data size
	    1,			      // device_index
	    5,			      // groupId
	    2			      // dsId
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t device_index;
	uint8_t groupId;
	uint8_t dsId;
	auto rc = decode_clear_data_source_v1_req(
	    request, msg_len, &device_index, &groupId, &dsId);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, device_index);
	EXPECT_EQ(5, groupId);
	EXPECT_EQ(2, dsId);
}

TEST(clearScalarDataSource, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_clear_data_source_v1_resp(0, NSM_SUCCESS, reason_code,
						   response);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_CLEAR_DATA_SOURCE_V1, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(clearScalarDataSource, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		      // PCI VID: NVIDIA 0x10DE
	    0x00,		      // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		      // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK,	      // NVIDIA_MSG_TYPE
	    NSM_CLEAR_DATA_SOURCE_V1, // command
	    0,			      // completion code
	    0,			      // reserved
	    0,			      // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_clear_data_source_v1_resp(response, msg_len, &cc,
						   &data_size, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0, data_size);
}

TEST(clearScalarDataSource, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		      // PCI VID: NVIDIA 0x10DE
	    0x00,		      // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		      // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK,	      // NVIDIA_MSG_TYPE
	    NSM_CLEAR_DATA_SOURCE_V1, // command
	    0,			      // completion code
	    0,			      // reserved
	    0,			      // reserved
	    0,			      // data size
	    0			      // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_clear_data_source_v1_resp(NULL, msg_len, &cc,
						   &data_size, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_clear_data_source_v1_resp(response, msg_len, NULL,
					      &data_size, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_clear_data_source_v1_resp(response, msg_len, &cc, NULL,
					      &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_clear_data_source_v1_resp(response, msg_len - 1, &cc,
					      &data_size, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(QueryAvailableAndClearableScalarDataSource, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_query_available_clearable_scalar_data_sources_v1_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t device_index = 1;
	uint8_t groupId = 8;
	auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
	    0, device_index, groupId, request);
	struct nsm_query_available_clearable_scalar_data_sources_v1_req *req =
	    reinterpret_cast<
		struct nsm_query_available_clearable_scalar_data_sources_v1_req
		    *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES,
		  req->hdr.command);
	EXPECT_EQ(2, req->hdr.data_size);
	EXPECT_EQ(device_index, req->device_index);
	EXPECT_EQ(groupId, req->group_id);
}

TEST(QueryAvailableAndClearableScalarDataSource, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES, // command
	    2,						       // data size
	    1,						       // device_index
	    5						       // groupId
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t device_index;
	uint8_t groupId;
	auto rc = decode_query_available_clearable_scalar_data_sources_v1_req(
	    request, msg_len, &device_index, &groupId);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, device_index);
	EXPECT_EQ(5, groupId);
}

TEST(QueryAvailableAndClearableScalarDataSource, testGoodEncodeResponse)
{
	uint16_t data_size = 5;
	bitfield8_t available_source[2];
	bitfield8_t clearable_source[2];
	uint8_t mask_length = 2;

	available_source[0].byte = 25;
	available_source[1].byte = 95;
	clearable_source[0].byte = 75;
	clearable_source[1].byte = 35;

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(
		    struct
		    nsm_query_available_clearable_scalar_data_sources_v1_resp) +
		mask_length * 2,
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_query_available_clearable_scalar_data_sources_v1_resp(
	    0, NSM_SUCCESS, reason_code, data_size, mask_length,
	    (uint8_t *)available_source, (uint8_t *)clearable_source, response);

	struct nsm_query_available_clearable_scalar_data_sources_v1_resp *resp =
	    reinterpret_cast<
		struct nsm_query_available_clearable_scalar_data_sources_v1_resp
		    *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES,
		  resp->hdr.command);
	EXPECT_EQ(5, le16toh(resp->hdr.data_size));
	EXPECT_EQ(2, resp->mask_length);
	EXPECT_EQ(25, resp->data[0]);
	EXPECT_EQ(95, resp->data[1]);
	EXPECT_EQ(75, resp->data[2]);
	EXPECT_EQ(35, resp->data[3]);
}

TEST(QueryAvailableAndClearableScalarDataSource, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES, // command
	    0, // completion code
	    0, // reserved
	    0, // reserved
	    5,
	    0,	// data size
	    2,	// mask length
	    25, // available data source
	    95, // available data source
	    35, // clearable data source
	    75	// clearable data source
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t mask_length;
	bitfield8_t available_source[5];
	bitfield8_t clearable_source[5];

	auto rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &mask_length,
	    (uint8_t *)available_source, (uint8_t *)clearable_source);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(5, data_size);
	EXPECT_EQ(25, available_source[0].byte);
	EXPECT_EQ(95, available_source[1].byte);
	EXPECT_EQ(35, clearable_source[0].byte);
	EXPECT_EQ(75, clearable_source[1].byte);
}

TEST(QueryAvailableAndClearableScalarDataSource, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES, // command
	    0, // completion code
	    0, // reserved
	    0, // reserved
	    4,
	    0,	// data size
	    2,	// mask length
	    25, // available data source
	    95, // available data source
	    35, // clearable data source
	    75	// clearable data source
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t mask_length;
	bitfield8_t available_source[2];
	bitfield8_t clearable_source[2];

	auto rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &mask_length,
	    (uint8_t *)available_source, (uint8_t *)clearable_source);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, NULL,
	    (uint8_t *)available_source, (uint8_t *)clearable_source);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &mask_length,
	    NULL, (uint8_t *)clearable_source);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &mask_length,
	    (uint8_t *)available_source, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    response, msg_len, NULL, &data_size, &reason_code, &mask_length,
	    (uint8_t *)available_source, (uint8_t *)clearable_source);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    response, msg_len, &cc, NULL, &reason_code, &mask_length,
	    (uint8_t *)available_source, (uint8_t *)clearable_source);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    response, msg_len - 5, &cc, &data_size, &reason_code, &mask_length,
	    (uint8_t *)available_source, (uint8_t *)clearable_source);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &mask_length,
	    (uint8_t *)available_source, (uint8_t *)clearable_source);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(ListAvailablePciePorts, testRequest)
{
	testEncodeCommonRequest(&encode_list_available_pcie_ports_req,
				NSM_TYPE_PCI_LINK,
				NSM_LIST_AVAILABLE_PCIE_PORTS);
	testDecodeCommonRequest(&decode_list_available_pcie_ports_req,
				NSM_TYPE_PCI_LINK,
				NSM_LIST_AVAILABLE_PCIE_PORTS);
}

TEST(ListAvailablePciePorts, testResponse)
{
	const size_t payloadSize =
	    sizeof(uint16_t) + sizeof(nsm_pcie_upstream_port_info) * 2;
	Response responseMsg(sizeof(nsm_common_resp) + payloadSize);
	auto &resp = *reinterpret_cast<nsm_list_available_pcie_ports_resp *>(
	    responseMsg.data());
	Response expectedResponseMsg(sizeof(nsm_common_resp) + payloadSize);
	auto &info = reinterpret_cast<nsm_list_available_pcie_ports_resp *>(
			 expectedResponseMsg.data())
			 ->port_info;
	nsm_pcie_upstream_port_info ports[2] = {
	    {.type = 1, .downstream_ports_count = 2},
	    {.type = 0, .downstream_ports_count = 2}};
	info.ports_count = 2;
	memcpy(info.ports, ports,
	       info.ports_count * sizeof(nsm_pcie_upstream_port_info));

	testEncodeResponse<nsm_list_available_pcie_ports_info>(
	    &encode_list_available_pcie_ports_resp, NSM_TYPE_PCI_LINK,
	    NSM_LIST_AVAILABLE_PCIE_PORTS, info, resp.port_info, payloadSize);
	EXPECT_EQ(resp.port_info.ports_count, info.ports_count);
	EXPECT_EQ(resp.port_info.ports[0].type, info.ports[0].type);
	EXPECT_EQ(resp.port_info.ports[0].downstream_ports_count,
		  info.ports[0].downstream_ports_count);

	testDecodeResponse<nsm_list_available_pcie_ports_info>(
	    &decode_list_available_pcie_ports_resp, NSM_TYPE_PCI_LINK,
	    NSM_LIST_AVAILABLE_PCIE_PORTS, info, resp.port_info, payloadSize);
	EXPECT_EQ(resp.port_info.ports_count, info.ports_count);
	EXPECT_EQ(resp.port_info.ports[0].type, info.ports[0].type);
	EXPECT_EQ(resp.port_info.ports[0].downstream_ports_count,
		  info.ports[0].downstream_ports_count);
}

TEST(ListAvailablePciePorts, testBadDataResponse)
{
	Response responseMsg(NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN);
	auto resp = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	nsm_list_available_pcie_ports_info info{1, {{2, 2}}};

	auto rc = encode_list_available_pcie_ports_resp(
	    0, NSM_SUCCESS, reasonCode, &info, resp);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_list_available_pcie_ports_resp(resp, responseMsg.size(),
						   &cc, &reasonCode, &info);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(MultiportQueryScalarGroupTelemetry, testRequest)
{
	const nsm_multiport_query_scalar_group_telemetry_v2_req_data data = {
	    1, NSM_PORT_TYPE_DOWNSTREAM, 0, GROUP_ID_10};
	nsm_multiport_query_scalar_group_telemetry_v2_req req;

	testEncodeRequest<
	    nsm_multiport_query_scalar_group_telemetry_v2_req_data>(
	    &encode_multiport_query_scalar_group_telemetry_v2_req,
	    NSM_TYPE_PCI_LINK, NSM_MULTIPORT_QUERY_SCALAR_GROUP_TELEMETRY_V2,
	    data, req.data);

	testDecodeRequest<
	    nsm_multiport_query_scalar_group_telemetry_v2_req_data>(
	    &decode_multiport_query_scalar_group_telemetry_v1_req,
	    NSM_TYPE_PCI_LINK, NSM_MULTIPORT_QUERY_SCALAR_GROUP_TELEMETRY_V2,
	    data, req.data);

	EXPECT_EQ(NSM_PORT_TYPE_DOWNSTREAM, req.data.type);
	EXPECT_EQ(1, req.data.upstream_port_index);
	EXPECT_EQ(0, req.data.index);
	EXPECT_EQ(GROUP_ID_10, req.data.group_index);
}

TEST(QueryScalarGroupTelemetryGroup10, testResponse)
{
	const nsm_query_scalar_group_telemetry_group_10 data = {
	    DS_ID_0, DS_ID_1, DS_ID_2, DS_ID_3, DS_ID_4,  DS_ID_5,
	    DS_ID_6, DS_ID_7, DS_ID_8, DS_ID_9, DS_ID_10,
	};
	nsm_query_scalar_group_telemetry_v1_group_10_resp resp;

	testEncodeResponse<nsm_query_scalar_group_telemetry_group_10>(
	    &encode_query_scalar_group_telemetry_v1_group10_resp,
	    NSM_TYPE_PCI_LINK, NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, data,
	    resp.data);

	testDecodeResponse<nsm_query_scalar_group_telemetry_group_10>(
	    &decode_query_scalar_group_telemetry_v1_group10_resp,
	    NSM_TYPE_PCI_LINK, NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, data,
	    resp.data);

	EXPECT_EQ(DS_ID_0, resp.data.outbound_read_tlp_count);
	EXPECT_EQ(DS_ID_1,
		  resp.data.dwords_transferred_in_outbound_read_tlp_high);
	EXPECT_EQ(DS_ID_2,
		  resp.data.dwords_transferred_in_outbound_read_tlp_low);
	EXPECT_EQ(DS_ID_3, resp.data.outbound_write_tlp_count);
	EXPECT_EQ(DS_ID_4,
		  resp.data.dwords_transferred_in_outbound_write_tlp_high);
	EXPECT_EQ(DS_ID_5,
		  resp.data.dwords_transferred_in_outbound_write_tlp_low);
	EXPECT_EQ(DS_ID_6, resp.data.outbound_completion_tlp_count);
	EXPECT_EQ(DS_ID_7, resp.data.dwords_transferred_in_outbound_completion);
	EXPECT_EQ(DS_ID_8, resp.data.read_requests_dropped_tag_unavailable);
	EXPECT_EQ(DS_ID_9, resp.data.read_requests_dropped_credit_exhaustion);
	EXPECT_EQ(DS_ID_10, resp.data.read_requests_dropped_credit_not_posted);
}

TEST(queryScalarGroupTelemetryV1Group7, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_7 data;
	data.bar0_base_addr_low = 0x12345678;
	data.bar0_base_addr_high = 0x9ABCDEF0;
	data.bar0_size = 0x1000;
	data.port_type = NSM_PCIE_PORT_TYPE_UPSTREAM;
	data.pcie_bus_number = 0x42;
	struct nsm_query_scalar_group_telemetry_group_7 data_test = data;

	auto rc = encode_query_scalar_group_telemetry_v1_group7_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_7_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_7_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_7),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.bar0_base_addr_low,
		  le32toh(data.bar0_base_addr_low));
	EXPECT_EQ(data_test.bar0_base_addr_high,
		  le32toh(data.bar0_base_addr_high));
	EXPECT_EQ(data_test.bar0_size, le32toh(data.bar0_size));
	EXPECT_EQ(data_test.port_type, le32toh(data.port_type));
	EXPECT_EQ(data_test.pcie_bus_number, le32toh(data.pcie_bus_number));
}

TEST(queryScalarGroupTelemetryV1Group7, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x78, 0x56, 0x34, 0x12, // bar0_base_addr_low
	    0xF0, 0xDE, 0xBC, 0x9A, // bar0_base_addr_high
	    0x00, 0x10, 0x00, 0x00, // bar0_size
	    0x05, 0x00, 0x00, 0x00, // port_type (upstream)
	    0x42, 0x00, 0x00, 0x00, // pcie_bus_number
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    20,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_7 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group7_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 20);
	EXPECT_EQ(le32toh(data_test->bar0_base_addr_low),
		  data->bar0_base_addr_low);
	EXPECT_EQ(le32toh(data_test->bar0_base_addr_high),
		  data->bar0_base_addr_high);
	EXPECT_EQ(le32toh(data_test->bar0_size), data->bar0_size);
	EXPECT_EQ(le32toh(data_test->port_type), data->port_type);
	EXPECT_EQ(le32toh(data_test->pcie_bus_number), data->pcie_bus_number);
}

TEST(queryScalarGroupTelemetryV1Group7, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x78, 0x56, 0x34, 0x12, // bar0_base_addr_low
	    0xF0, 0xDE, 0xBC, 0x9A, // bar0_base_addr_high
	    0x00, 0x10, 0x00, 0x00, // bar0_size
	    0x05, 0x00, 0x00, 0x00, // port_type
	    0x42, 0x00, 0x00, 0x00, // pcie_bus_number
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    19, // incorrect data size
	    0	// data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_7 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group7_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group7_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group7_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group7_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group7_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(QueryScalarGroupTelemetryGroup10Extended, testResponse)
{
	const nsm_query_scalar_group_telemetry_group_10_extended data = {
	    DS_ID_0, DS_ID_1, DS_ID_2, DS_ID_3,	 DS_ID_4,  DS_ID_5,  DS_ID_6,
	    DS_ID_7, DS_ID_8, DS_ID_9, DS_ID_10, DS_ID_11, DS_ID_12, DS_ID_13,
	};
	nsm_query_scalar_group_telemetry_v1_group_10_extended_resp resp;

	testEncodeResponse<nsm_query_scalar_group_telemetry_group_10_extended>(
	    &encode_query_scalar_group_telemetry_v1_group10_extended_resp,
	    NSM_TYPE_PCI_LINK, NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, data,
	    resp.data);

	testDecodeResponse<nsm_query_scalar_group_telemetry_group_10_extended>(
	    &decode_query_scalar_group_telemetry_v1_group10_extended_resp,
	    NSM_TYPE_PCI_LINK, NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, data,
	    resp.data);

	EXPECT_EQ(DS_ID_0, resp.data.outbound_read_tlp_count);
	EXPECT_EQ(DS_ID_1,
		  resp.data.dwords_transferred_in_outbound_read_tlp_high);
	EXPECT_EQ(DS_ID_2,
		  resp.data.dwords_transferred_in_outbound_read_tlp_low);
	EXPECT_EQ(DS_ID_3, resp.data.outbound_write_tlp_count);
	EXPECT_EQ(DS_ID_4,
		  resp.data.dwords_transferred_in_outbound_write_tlp_high);
	EXPECT_EQ(DS_ID_5,
		  resp.data.dwords_transferred_in_outbound_write_tlp_low);
	EXPECT_EQ(DS_ID_6, resp.data.outbound_completion_tlp_count);
	EXPECT_EQ(DS_ID_7, resp.data.dwords_transferred_in_outbound_completion);
	EXPECT_EQ(DS_ID_8, resp.data.read_requests_dropped_tag_unavailable);
	EXPECT_EQ(DS_ID_9, resp.data.read_requests_dropped_credit_exhaustion);
	EXPECT_EQ(DS_ID_10, resp.data.read_requests_dropped_credit_not_posted);
	EXPECT_EQ(DS_ID_11, resp.data.inbound_completion_tlp_count);
	EXPECT_EQ(DS_ID_12, resp.data.inbound_completion_tlp_bytes_high);
	EXPECT_EQ(DS_ID_13, resp.data.inbound_completion_tlp_bytes_low);
}

TEST(queryVectorGroupTelemetryV2, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_query_vector_group_telemetry_v2_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t port_number = 1;
	uint8_t port_type = NSM_PORT_TYPE_UPSTREAM;
	uint8_t index = 0;
	uint8_t group_id = GROUP_ID_1;
	uint8_t selector_0 = 5; // lane number
	uint8_t selector_1 = 0;

	auto rc = encode_query_vector_group_telemetry_v2_req(
	    0, port_number, port_type, index, group_id, selector_0, selector_1,
	    request);

	struct nsm_query_vector_group_telemetry_v2_req *req =
	    reinterpret_cast<struct nsm_query_vector_group_telemetry_v2_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_VECTOR_DATA_SOURCES_V2, req->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_vector_group_telemetry_v2_req) -
		      sizeof(nsm_common_req),
		  req->hdr.data_size);
	EXPECT_EQ(port_number, req->port_number);
	EXPECT_EQ(port_type, req->port_type);
	EXPECT_EQ(index, req->index);
	EXPECT_EQ(group_id, req->group_id);
	EXPECT_EQ(selector_0, req->selector_0);
	EXPECT_EQ(selector_1, req->selector_1);

	// bad encode request test case
	rc = encode_query_vector_group_telemetry_v2_req(0, 0, 0, 0, 0, 0, 0,
							NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryVectorGroupTelemetryV2, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			      // pci_vendor_id
	    0x80,			      // instance_id
	    0x89,			      // ocp_version
	    NSM_TYPE_PCI_LINK,		      // nvidia_msg_type
	    NSM_QUERY_VECTOR_DATA_SOURCES_V2, // command
	    0x05,			      // data_size
	    0x01, // port_number (7 bits) | port_type (1 bit)
	    0x00, // index
	    0x01, // group_id
	    0x05, // selector_0
	    0x00  // selector_1
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t port_number = 0;
	uint8_t port_type = 0;
	uint8_t index = 0;
	uint8_t group_id = 0;
	uint8_t selector_0 = 0;
	uint8_t selector_1 = 0;

	auto rc = decode_query_vector_group_telemetry_v2_req(
	    request, msg_len, &port_number, &port_type, &index, &group_id,
	    &selector_0, &selector_1);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, port_number);
	EXPECT_EQ(0, port_type);
	EXPECT_EQ(0, index);
	EXPECT_EQ(1, group_id);
	EXPECT_EQ(5, selector_0);
	EXPECT_EQ(0, selector_1);

	// bad decode request test case
	rc = decode_query_vector_group_telemetry_v2_req(
	    NULL, msg_len, &port_number, &port_type, &index, &group_id,
	    &selector_0, &selector_1);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_req(
	    request, msg_len - 1, &port_number, &port_type, &index, &group_id,
	    &selector_0, &selector_1);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_vector_group_telemetry_v2_req(
	    request, msg_len, NULL, &port_type, &index, &group_id, &selector_0,
	    &selector_1);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_req(
	    request, msg_len, &port_number, NULL, &index, &group_id,
	    &selector_0, &selector_1);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_req(
	    request, msg_len, &port_number, &port_type, NULL, &group_id,
	    &selector_0, &selector_1);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_req(
	    request, msg_len, &port_number, &port_type, &index, NULL,
	    &selector_0, &selector_1);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_req(
	    request, msg_len, &port_number, &port_type, &index, &group_id, NULL,
	    &selector_1);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_req(
	    request, msg_len, &port_number, &port_type, &index, &group_id,
	    &selector_0, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryVectorGroupTelemetryV2, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_vector_data_sources_v2_resp) +
		sizeof(uint32_t) * 2,
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;
	uint16_t data_count = 2;
	uint32_t data_values[2] = {0x12345678, 0x9ABCDEF0};

	auto rc = encode_query_vector_group_telemetry_v2_resp(
	    0, NSM_SUCCESS, reason_code, data_count, data_values, response);

	struct nsm_query_vector_data_sources_v2_resp *resp =
	    reinterpret_cast<struct nsm_query_vector_data_sources_v2_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_VECTOR_DATA_SOURCES_V2, resp->hdr.command);
	EXPECT_EQ(0, resp->hdr.completion_code);
	EXPECT_EQ(data_count * sizeof(uint32_t), le16toh(resp->hdr.data_size));

	// bad encode response test case
	rc = encode_query_vector_group_telemetry_v2_resp(0, NSM_SUCCESS, 0, 2,
							 data_values, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_query_vector_group_telemetry_v2_resp(0, NSM_SUCCESS, 0, 2,
							 NULL, response);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryVectorGroupTelemetryV2, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_VECTOR_DATA_SOURCES_V2, // command
	    0,				      // completion code
	    0,
	    0,
	    8,
	    0, // data size (2 * 4 bytes)
	    0x78,
	    0x56,
	    0x34,
	    0x12, // data value 1
	    0xF0,
	    0xDE,
	    0xBC,
	    0x9A, // data value 2
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t data_values[2] = {0};

	auto rc = decode_query_vector_group_telemetry_v2_resp(
	    response, msg_len, &cc, &data_size, &reason_code,
	    reinterpret_cast<uint8_t *>(data_values));

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 2 * sizeof(uint32_t));
	EXPECT_EQ(data_values[0], 0x12345678u);
	EXPECT_EQ(data_values[1], 0x9ABCDEF0u);
}

TEST(queryVectorGroupTelemetryV2, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_VECTOR_DATA_SOURCES_V2, // command
	    0,				      // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0x78,
	    0x56,
	    0x34,
	    0x12, // data value
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint32_t data_values[1] = {0};
	auto data = reinterpret_cast<uint8_t *>(data_values);

	auto rc = decode_query_vector_group_telemetry_v2_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_resp(
	    response, msg_len, &cc, &data_size, NULL, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_resp(
	    response, msg_len, &cc, &data_size, &reason_code, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_resp(
	    response, msg_len - 10, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryVectorGroupTelemetryV2Group1, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_vector_data_sources_v2_resp) +
		sizeof(uint32_t),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_vector_group_1_data data;
	data.cdr_error_per_lane = 12345;

	auto rc = encode_query_vector_group_telemetry_v2_group1_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_vector_data_sources_v2_resp *resp =
	    reinterpret_cast<struct nsm_query_vector_data_sources_v2_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_VECTOR_DATA_SOURCES_V2, resp->hdr.command);
	EXPECT_EQ(sizeof(uint32_t), le16toh(resp->hdr.data_size));

	// bad encode response test case
	rc = encode_query_vector_group_telemetry_v2_group1_resp(
	    0, NSM_SUCCESS, 0, NULL, response);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(queryVectorGroupTelemetryV2Group1, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_VECTOR_DATA_SOURCES_V2, // command
	    0,				      // completion code
	    0,
	    0,
	    4,
	    0, // data size (1 * 4 bytes)
	    0x39,
	    0x30,
	    0x00,
	    0x00, // cdr_error_per_lane = 12345 (0x00003039)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	struct nsm_query_vector_group_1_data data;

	auto rc = decode_query_vector_group_telemetry_v2_group1_resp(
	    response, msg_len, &cc, &reason_code, &data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data.cdr_error_per_lane, 12345u);
}

/*   Buggy test case. To be fixed later.
TEST(queryVectorGroupTelemetryV2Group1, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x39,
	    0x30,
	    0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_VECTOR_DATA_SOURCES_V2, // command
	    0,				      // completion code
	    0,
	    0,
	    3, // incorrect data size (should be 4)
	    0,
	};
	auto data =
	    reinterpret_cast<nsm_query_vector_group_1_data *>(data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_query_vector_group_telemetry_v2_group1_resp(
	    NULL, msg_len, &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_group1_resp(
	    response, msg_len, &cc, &reason_code, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_vector_group_telemetry_v2_group1_resp(
	    response, msg_len - data_byte.size(), &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_vector_group_telemetry_v2_group1_resp(
	    response, msg_len, &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
} */
