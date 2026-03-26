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
 * Branch coverage for pci-links.c partial branches.
 *
 * Covers 1/2 and 3/4 partial branches not exercised by existing tests.
 * Blocked branches (pack-fail via masked instance_id): L535, L642, L807, L1114.
 */

#include "base.h"
#include "pci-links.h"
#include <gtest/gtest.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: 9-byte buffer with error CC — for 3/4 branches
//   if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
// decode_reason_code_and_cc: cc=0xFF, len=9==9 → SUCCESS with *cc=0xFF
// Second condition TRUE → returns ret (=0)
// ---------------------------------------------------------------------------
static std::vector<uint8_t>
errCcBuf(size_t sz = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp))
{
	std::vector<uint8_t> buf(sz, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // completion_code != NSM_SUCCESS
	return buf;
}

// ===========================================================================
// L42 (3/4): decode_query_scalar_group_telemetry_v1_req
// group_index == NULL (msg valid, device_id valid) → NSM_SW_ERROR_NULL
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryScalarGroupV1Req_NullGroupIndex)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t device_id = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_req(
	    msg, buf.size(), &device_id, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L61 (1/2): decode_query_scalar_group_telemetry_v1_req
// request->hdr.data_size < sizeof(req) - NSM_REQUEST_CONVENTION_LEN → TRUE
// Encode valid req, then corrupt data_size = 0 (< 2)
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryScalarGroupV1Req_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_req),
	    0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	encode_query_scalar_group_telemetry_v1_req(0, 0, 0, msg);
	// corrupt data_size to 0 (< 2)
	auto *req = reinterpret_cast<nsm_query_scalar_group_telemetry_v1_req *>(
	    msg->payload);
	req->hdr.data_size = 0;
	uint8_t device_id = 0, group_index = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_req(
	    msg, buf.size(), &device_id, &group_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L214 (3/4): decode_query_scalar_group_telemetry_v1_group2_resp
// *cc != NSM_SUCCESS → second condition TRUE
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryScalarGroup2Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_2 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L239 (3/4): decode_query_scalar_group_telemetry_v1_group3_resp
// *cc != NSM_SUCCESS → second condition TRUE
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryScalarGroup3Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_3 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L265 (3/4): decode_query_scalar_group_telemetry_v1_group4_resp
// *cc != NSM_SUCCESS → second condition TRUE
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryScalarGroup4Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_4 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L340 (3/4): decode_query_scalar_group_telemetry_v1_group7_resp
// *cc != NSM_SUCCESS → second condition TRUE
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryScalarGroup7Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_7 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group7_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L365 (3/4): decode_query_scalar_group_telemetry_v1_group8_resp
// *cc != NSM_SUCCESS → second condition TRUE
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryScalarGroup8Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_8 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group8_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L436 (1/2): decode_query_scalar_group_telemetry_v1_group10_resp
// data_size != sizeof(group10) → NSM_SW_ERROR_LENGTH
// Use buffer large enough with cc=SUCCESS but data_size=0 at resp field
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryScalarGroup10Resp_DataSizeMismatch)
{
	// Buffer = nsm_msg_hdr(5) + nsm_common_resp(6) + group10(44) = 55
	// cc at payload[1] = 0 (NSM_SUCCESS), data_size at payload[4..5] = 0
	// decode_reason_code_and_cc: cc=0 → returns SUCCESS immediately
	// inner resp check: msg_len=55 >= 12 → passes
	// outer group10 check: msg_len=55 >= 55 → passes
	// data_size=0 != sizeof(group10)=44 → L436 TRUE
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_10_resp),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_query_scalar_group_telemetry_group_10 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group10_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L459 (1/2): decode_query_scalar_group_telemetry_v1_group10_extended_resp
// data_size != sizeof(group10_extended) → NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(PciLinksBranchCoverage,
     DecodeQueryScalarGroup10ExtendedResp_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(
		    nsm_query_scalar_group_telemetry_v1_group_10_extended_resp),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_query_scalar_group_telemetry_group_10_extended data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group10_extended_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L499 (3/4): decode_assert_pcie_fundamental_reset_req
// action == NULL (msg valid, device_index valid) → NSM_SW_ERROR_NULL
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeAssertPcieFundamentalResetReq_NullAction)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t device_index = 0;
	auto rc = decode_assert_pcie_fundamental_reset_req(
	    msg, buf.size(), &device_index, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L503 (1/2): decode_assert_pcie_fundamental_reset_req
// msg_len != expected → NSM_SW_ERROR_LENGTH (wrong size)
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeAssertPcieFundamentalResetReq_WrongMsgLen)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_assert_pcie_fundamental_reset_req),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t device_index = 0, action = 0;
	// msg_len+1 != expected → L503 TRUE
	auto rc = decode_assert_pcie_fundamental_reset_req(
	    msg, buf.size() + 1, &device_index, &action);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L512 (1/2): decode_assert_pcie_fundamental_reset_req
// request->hdr.data_size != 2 → NSM_SW_ERROR_DATA
// Use correct msg_len but data_size=0 (zero-fill) != 2
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeAssertPcieFundamentalResetReq_DataSizeBad)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_assert_pcie_fundamental_reset_req),
	    0);
	// data_size at payload[1] = 0 != 2 → L512 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t device_index = 0, action = 0;
	auto rc = decode_assert_pcie_fundamental_reset_req(
	    msg, buf.size(), &device_index, &action);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L596 (3/4): decode_query_available_clearable_scalar_data_sources_v1_req
// group_id == NULL (msg valid, device_index valid) → NSM_SW_ERROR_NULL
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryAvailClearableScalarReq_NullGroupId)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t device_index = 0;
	auto rc = decode_query_available_clearable_scalar_data_sources_v1_req(
	    msg, buf.size(), &device_index, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L600 (1/2): decode_query_available_clearable_scalar_data_sources_v1_req
// msg_len != expected → NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryAvailClearableScalarReq_WrongMsgLen)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(
		    nsm_query_available_clearable_scalar_data_sources_v1_req),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t device_index = 0, group_id = 0;
	// buf.size()+1 != expected → L600 TRUE
	auto rc = decode_query_available_clearable_scalar_data_sources_v1_req(
	    msg, buf.size() + 1, &device_index, &group_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L613 (1/2): decode_query_available_clearable_scalar_data_sources_v1_req
// request->hdr.data_size != expected → NSM_SW_ERROR_DATA
// Zero-fill: data_size=0 != expected
// ===========================================================================
TEST(PciLinksBranchCoverage,
     DecodeQueryAvailClearableScalarReq_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(
		    nsm_query_available_clearable_scalar_data_sources_v1_req),
	    0);
	// data_size at payload[1] = 0 != sizeof(req)-2 → L613 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t device_index = 0, group_id = 0;
	auto rc = decode_query_available_clearable_scalar_data_sources_v1_req(
	    msg, buf.size(), &device_index, &group_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L631 (3/4): encode_query_available_clearable_scalar_data_sources_v1_resp
// clearable_data_source_mask == NULL (available != NULL) → NSM_SW_ERROR_NULL
// ===========================================================================
TEST(PciLinksBranchCoverage,
     EncodeQueryAvailClearableScalarResp_NullClearableMask)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t available[1] = {0};
	auto rc = encode_query_available_clearable_scalar_data_sources_v1_resp(
	    0, NSM_SUCCESS, 0, 1, 1, available, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L646 (1/2): encode_query_available_clearable_scalar_data_sources_v1_resp
// cc != NSM_SUCCESS → encode_reason_code path (TRUE branch)
// ===========================================================================
TEST(PciLinksBranchCoverage, EncodeQueryAvailClearableScalarResp_NonSuccessCc)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t available[1] = {0}, clearable[1] = {0};
	// cc=0xFF != NSM_SUCCESS → L646 TRUE → encode_reason_code
	auto rc = encode_query_available_clearable_scalar_data_sources_v1_resp(
	    0, 0xFF, 0, 1, 1, available, clearable, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L774 (1/2): decode_clear_data_source_v1_req
// msg_len != expected → NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeClearDataSourceV1Req_WrongMsgLen)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_clear_data_source_v1_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t device_index = 0, groupId = 0, dsId = 0;
	// buf.size()+1 != expected → L774 TRUE
	auto rc = decode_clear_data_source_v1_req(
	    msg, buf.size() + 1, &device_index, &groupId, &dsId);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L782 (1/2): decode_clear_data_source_v1_req
// request->hdr.data_size != expected → NSM_SW_ERROR_DATA
// Zero-fill: data_size=0 != sizeof(req)-2
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeClearDataSourceV1Req_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_clear_data_source_v1_req), 0);
	// data_size at payload[1] = 0 != sizeof(req)-2 → L782 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t device_index = 0, groupId = 0, dsId = 0;
	auto rc = decode_clear_data_source_v1_req(
	    msg, buf.size(), &device_index, &groupId, &dsId);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L887 (3/4): decode_list_available_pcie_ports_resp
// *cc != NSM_SUCCESS → second condition TRUE
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeListAvailablePciePortsResp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_list_available_pcie_ports_info info = {};
	auto rc = decode_list_available_pcie_ports_resp(msg, buf.size(), &cc,
							&reason, &info);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L905 (1/2): decode_list_available_pcie_ports_resp
// data_size != expected_data_size → NSM_SW_ERROR_LENGTH
// Build buffer with cc=SUCCESS, ports_count=0, data_size=0 != expected(2)
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeListAvailablePciePortsResp_DataSizeMismatch)
{
	// NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN =
	//   nsm_msg_hdr(5) + nsm_common_resp(6) + uint16_t(2) + port_info(8)
	// Build a large enough buffer with cc=0 (SUCCESS)
	// ports_count = 0 → expected_data_size = sizeof(uint16_t) = 2
	// data_size at resp->hdr.data_size (payload[4..5]) = 0 != 2
	std::vector<uint8_t> buf(NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN,
				 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_list_available_pcie_ports_info info = {};
	auto rc = decode_list_available_pcie_ports_resp(msg, buf.size(), &cc,
							&reason, &info);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L975 (1/2): decode_multiport_query_scalar_group_telemetry_v2_req
// request->hdr.data_size != sizeof(req_data) → NSM_SW_ERROR_LENGTH
// Zero-fill after passing the msg_len check
// ===========================================================================
TEST(PciLinksBranchCoverage,
     DecodeMultiportQueryScalarGroupV2Req_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_multiport_query_scalar_group_telemetry_v2_req),
	    0);
	// Need to pass the unpack_nsm_header check first — leave as zeros
	// (unpack fails), so just check we don't crash
	// Actually, let's try: the function first calls unpack_nsm_header.
	// With zeros, unpack fails → returns error before L975.
	// We need a valid header. Use encode to build it first.
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// Encode a valid multiport req, then corrupt data_size
	nsm_multiport_query_scalar_group_telemetry_v2_req_data enc_data = {};
	encode_multiport_query_scalar_group_telemetry_v2_req(0, &enc_data, msg);
	auto *req = reinterpret_cast<
	    nsm_multiport_query_scalar_group_telemetry_v2_req *>(msg->payload);
	req->hdr.data_size = 0; // != sizeof(req_data) → L975 TRUE
	nsm_multiport_query_scalar_group_telemetry_v2_req_data data = {};
	auto rc = decode_multiport_query_scalar_group_telemetry_v1_req(
	    msg, buf.size(), &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L1001 (1/2): encode_get_pcie_port_config_req
// port_type > NSM_PORT_TYPE_DOWNSTREAM (1) → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(PciLinksBranchCoverage, EncodeGetPciePortConfigReq_InvalidPortType)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// port_type=2 > NSM_PORT_TYPE_DOWNSTREAM(1) → L1001 TRUE
	auto rc = encode_get_pcie_port_config_req(0, 0, 2, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1057 (3/4): decode_preset_PCIe_data
// preset_1 == NULL (data valid, preset_0 valid) → NSM_SW_ERROR_NULL
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodePresetPCIeData_NullPreset1)
{
	uint8_t raw = 0xAB;
	uint8_t preset_0 = 0;
	auto rc = decode_preset_PCIe_data(&raw, 1, &preset_0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L1143 (1/2): encode_set_port_config_aggregate_req
// port_type > NSM_PORT_TYPE_DOWNSTREAM → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(PciLinksBranchCoverage, EncodeSetPortConfigAggregateReq_InvalidPortType)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// port_type=2 > NSM_PORT_TYPE_DOWNSTREAM(1) → L1143 TRUE
	auto rc = encode_set_port_config_aggregate_req(0, 0, 2, 0, 0, nullptr,
						       0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1192 (1/2): decode_set_port_config_aggregate_req
// request->hdr.data_size < sizeof(port_index)+sizeof(sample_count)+1 → TRUE
// Zero-fill: data_size=0 < 4 → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeSetPortConfigAggregateReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_config_aggregate_req), 0);
	// data_size at payload[1] = 0 < 4 → L1192 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t port_number = 0, port_type = 0, port_index = 0;
	uint16_t sample_count = 0;
	const uint8_t *sample_data = nullptr;
	size_t sample_data_len = 0;
	auto rc = decode_set_port_config_aggregate_req(
	    msg, buf.size(), &port_number, &port_type, &port_index,
	    &sample_count, &sample_data, &sample_data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1278 (1/2): decode_query_vector_group_telemetry_v2_req
// request->hdr.data_size != sizeof(req)-sizeof(common_req) → NSM_SW_ERROR_DATA
// Encode valid req, then corrupt data_size
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryVectorGroupV2Req_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_vector_group_telemetry_v2_req),
	    0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// Encode to get valid header
	encode_query_vector_group_telemetry_v2_req(0, 0, 0, 0, 0, 0, 0, msg);
	auto *req = reinterpret_cast<nsm_query_vector_group_telemetry_v2_req *>(
	    msg->payload);
	req->hdr.data_size = 0; // != sizeof(req)-2 → L1278 TRUE
	uint8_t port_number = 0, port_type = 0, index = 0, group_id = 0;
	uint8_t selector_0 = 0, selector_1 = 0;
	auto rc = decode_query_vector_group_telemetry_v2_req(
	    msg, buf.size(), &port_number, &port_type, &index, &group_id,
	    &selector_0, &selector_1);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1345 (3/4): decode_query_vector_group_telemetry_v2_resp
// *cc != NSM_SUCCESS → second condition TRUE
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryVectorGroupV2Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint8_t data[64] = {};
	auto rc = decode_query_vector_group_telemetry_v2_resp(
	    msg, buf.size(), &cc, &data_size, &reason, data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L1390 (3/4): decode_query_vector_group_telemetry_v2_group1_resp
// *cc != NSM_SUCCESS → second condition TRUE
// ===========================================================================
TEST(PciLinksBranchCoverage, DecodeQueryVectorGroup1Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_query_vector_group_1_data data = {};
	auto rc = decode_query_vector_group_telemetry_v2_group1_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}
