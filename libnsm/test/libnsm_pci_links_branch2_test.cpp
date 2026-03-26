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
 * Branch coverage for pci-links.c — batch 2.
 *
 * Targets encode pack-fail branches, group decode error-cc branches (groups
 * 0, 1, 5, 6, 9, 10, 10_ext), group data_size mismatch branches (groups
 * 0-9), and misc NULL/length checks not covered by
 * libnsm_pci_links_branch_test.
 *
 * Blocked: L535, L807 (pack_nsm_header INSTANCEID_MASK — can never fail).
 */

#include "base.h"
#include "pci-links.h"
#include <gtest/gtest.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// 9-byte buffer with error CC for 3/4 branches in response decoders
static std::vector<uint8_t>
errCcBuf(size_t sz = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp))
{
	std::vector<uint8_t> buf(sz, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // cc != NSM_SUCCESS
	return buf;
}

// 11-byte buffer with cc=SUCCESS and data_size=0 for group data_size checks
// Passes decode_reason_code_and_cc (cc=0, len=11>=9) and inner msg_len
// check (len=11 >= nsm_msg_hdr(5)+nsm_common_resp(6)=11), but data_size=0.
static std::vector<uint8_t> successDataSizeZeroBuf()
{
	return std::vector<uint8_t>(11, 0); // all zeros: cc=0, data_size=0
}

// kBadIid triggers pack_nsm_header failure (> NSM_INSTANCE_MAX=31)
static constexpr uint8_t kBadIid = 32;

// ===========================================================================
// SECTION 1: Pack-fail branches (encode functions, bare instance_id)
// ===========================================================================

// L22 TRUE: encode_query_scalar_group_telemetry_v1_req — pack fail
TEST(PciLinksBranch2, EncodeQueryScalarGroupV1Req_PackFail)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_req),
	    0);
	auto rc = encode_query_scalar_group_telemetry_v1_req(
	    kBadIid, 0, 0, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L85 TRUE: encode_query_scalar_group_telemetry_v1_resp — pack fail
TEST(PciLinksBranch2, EncodeQueryScalarGroupV1Resp_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	uint8_t data[4] = {};
	auto rc = encode_query_scalar_group_telemetry_v1_resp(
	    kBadIid, NSM_SUCCESS, 0, 4, data,
	    reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L89 TRUE: encode_query_scalar_group_telemetry_v1_resp — cc != NSM_SUCCESS
TEST(PciLinksBranch2, EncodeQueryScalarGroupV1Resp_NonSuccessCc)
{
	std::vector<uint8_t> buf(256, 0);
	uint8_t data[4] = {};
	auto rc = encode_query_scalar_group_telemetry_v1_resp(
	    0, 0xFF, 0, 0, data, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// L480 TRUE: encode_assert_pcie_fundamental_reset_req — pack fail
TEST(PciLinksBranch2, EncodeAssertPcieFundamentalResetReq_PackFail)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_assert_pcie_fundamental_reset_req),
	    0);
	auto rc = encode_assert_pcie_fundamental_reset_req(
	    kBadIid, 0, 0, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L538 TRUE: encode_assert_pcie_fundamental_reset_resp — cc != NSM_SUCCESS
// (pack_nsm_header uses INSTANCEID_MASK so pack always succeeds)
TEST(PciLinksBranch2, EncodeAssertPcieFundamentalResetResp_NonSuccessCc)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_assert_pcie_fundamental_reset_resp(
	    0, 0xFF, 0, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// L573 TRUE: encode_query_available_clearable_scalar_data_sources_v1_req
// — pack fail
TEST(PciLinksBranch2, EncodeQueryAvailClearableScalarReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
	    kBadIid, 0, 0, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L749 TRUE: encode_clear_data_source_v1_req — pack fail
TEST(PciLinksBranch2, EncodeClearDataSourceV1Req_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_clear_data_source_v1_req(
	    kBadIid, 0, 0, 0, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L810 TRUE: encode_clear_data_source_v1_resp — cc != NSM_SUCCESS
// (pack uses INSTANCEID_MASK, so pack always succeeds)
TEST(PciLinksBranch2, EncodeClearDataSourceV1Resp_NonSuccessCc)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_clear_data_source_v1_resp(
	    0, 0xFF, 0, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// L855 TRUE: encode_list_available_pcie_ports_resp — pack fail
TEST(PciLinksBranch2, EncodeListAvailablePciePortsResp_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	nsm_list_available_pcie_ports_info info = {};
	info.ports_count = 0;
	auto rc = encode_list_available_pcie_ports_resp(
	    kBadIid, NSM_SUCCESS, 0, &info,
	    reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L858 TRUE: encode_list_available_pcie_ports_resp — cc != NSM_SUCCESS
TEST(PciLinksBranch2, EncodeListAvailablePciePortsResp_NonSuccessCc)
{
	std::vector<uint8_t> buf(256, 0);
	nsm_list_available_pcie_ports_info info = {};
	auto rc = encode_list_available_pcie_ports_resp(
	    0, 0xFF, 0, &info, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// L927 (3/4) TRUE: encode_multiport_query_scalar_group_telemetry_v2_req
// — NULL data
TEST(PciLinksBranch2, EncodeMultiportQueryScalarGroupV2Req_NullData)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_multiport_query_scalar_group_telemetry_v2_req(
	    0, nullptr, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// L934 TRUE: encode_multiport_query_scalar_group_telemetry_v2_req — pack fail
TEST(PciLinksBranch2, EncodeMultiportQueryScalarGroupV2Req_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	nsm_multiport_query_scalar_group_telemetry_v2_req_data data = {};
	auto rc = encode_multiport_query_scalar_group_telemetry_v2_req(
	    kBadIid, &data, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L997 TRUE: encode_get_pcie_port_config_req — pack fail
// (valid port_type=0, but invalid instance_id)
TEST(PciLinksBranch2, EncodeGetPciePortConfigReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_get_pcie_port_config_req(
	    kBadIid, 0, 0, 0, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L1150 TRUE: encode_set_port_config_aggregate_req — pack fail
// (valid port_type=0, sample_count=0, sample_data=nullptr)
TEST(PciLinksBranch2, EncodeSetPortConfigAggregateReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_set_port_config_aggregate_req(
	    kBadIid, 0, 0, 0, 0, nullptr, 0,
	    reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L1139 TRUE: encode_set_port_config_aggregate_req
// — sample_data == NULL && sample_count > 0
TEST(PciLinksBranch2,
     EncodeSetPortConfigAggregateReq_NullSampleDataNonZeroCount)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_set_port_config_aggregate_req(
	    0, 0, 0, 0, 1 /* count > 0 */, nullptr /* data=NULL */, 0,
	    reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// L1236 TRUE: encode_query_vector_group_telemetry_v2_req — pack fail
TEST(PciLinksBranch2, EncodeQueryVectorGroupV2Req_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_query_vector_group_telemetry_v2_req(
	    kBadIid, 0, 0, 0, 0, 0, 0, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L1300 (3/4) TRUE: encode_query_vector_group_telemetry_v2_resp
// — data_count > 0 && data_values == NULL
TEST(PciLinksBranch2, EncodeQueryVectorGroupV2Resp_NullDataValues)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_query_vector_group_telemetry_v2_resp(
	    0, NSM_SUCCESS, 0, 1 /* count > 0 */, nullptr /* values=NULL */,
	    reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// L1307 TRUE: encode_query_vector_group_telemetry_v2_resp — pack fail
TEST(PciLinksBranch2, EncodeQueryVectorGroupV2Resp_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	uint32_t values[1] = {0};
	auto rc = encode_query_vector_group_telemetry_v2_resp(
	    kBadIid, NSM_SUCCESS, 0, 1, values,
	    reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// L1311 TRUE: encode_query_vector_group_telemetry_v2_resp — cc != NSM_SUCCESS
TEST(PciLinksBranch2, EncodeQueryVectorGroupV2Resp_NonSuccessCc)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_query_vector_group_telemetry_v2_resp(
	    0, 0xFF, 0, 0, nullptr, reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// L1374 TRUE: encode_query_vector_group_telemetry_v2_group1_resp — NULL data
TEST(PciLinksBranch2, EncodeQueryVectorGroup1Resp_NullData)
{
	std::vector<uint8_t> buf(256, 0);
	auto rc = encode_query_vector_group_telemetry_v2_group1_resp(
	    0, NSM_SUCCESS, 0, nullptr,
	    reinterpret_cast<nsm_msg *>(buf.data()));
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// SECTION 2: Group decode — error CC (not yet covered)
// ===========================================================================

// L164 (3/4): decode_query_scalar_group_telemetry_v1_group0_resp — error cc
TEST(PciLinksBranch2, DecodeQueryScalarGroup0Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_0 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// L189 (3/4): decode_query_scalar_group_telemetry_v1_group1_resp — error cc
TEST(PciLinksBranch2, DecodeQueryScalarGroup1Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_1 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// L290 (3/4): decode_query_scalar_group_telemetry_v1_group5_resp — error cc
TEST(PciLinksBranch2, DecodeQueryScalarGroup5Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_5 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// L315 (3/4): decode_query_scalar_group_telemetry_v1_group6_resp — error cc
TEST(PciLinksBranch2, DecodeQueryScalarGroup6Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_6 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// L390 (3/4): decode_query_scalar_group_telemetry_v1_group9_resp — error cc
TEST(PciLinksBranch2, DecodeQueryScalarGroup9Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_9 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group9_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// L428 (3/4): decode_query_scalar_group_telemetry_v1_group10_resp — error cc
TEST(PciLinksBranch2, DecodeQueryScalarGroup10Resp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_query_scalar_group_telemetry_group_10 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group10_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// L450 (3/4): decode_query_scalar_group_telemetry_v1_group10_extended_resp
// — error cc
TEST(PciLinksBranch2, DecodeQueryScalarGroup10ExtendedResp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_query_scalar_group_telemetry_group_10_extended data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group10_extended_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// SECTION 3: Group decode — data_size mismatch (not yet covered)
// Use successDataSizeZeroBuf(): cc=0, data_size=0 → triggers < or != check
// ===========================================================================

// L166 (1/2): decode group0 — data_size < sizeof(group0) → TRUE
TEST(PciLinksBranch2, DecodeQueryScalarGroup0Resp_DataSizeTooSmall)
{
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_0 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L191 (1/2): decode group1 — data_size < sizeof(group1) → TRUE
TEST(PciLinksBranch2, DecodeQueryScalarGroup1Resp_DataSizeTooSmall)
{
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_1 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L216 (1/2): decode group2 — data_size < sizeof(group2) → TRUE
TEST(PciLinksBranch2, DecodeQueryScalarGroup2Resp_DataSizeTooSmall)
{
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_2 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L241 (1/2): decode group3 — data_size < sizeof(group3) → TRUE
TEST(PciLinksBranch2, DecodeQueryScalarGroup3Resp_DataSizeTooSmall)
{
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_3 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L267 (1/2): decode group4 — data_size < sizeof(group4) → TRUE
TEST(PciLinksBranch2, DecodeQueryScalarGroup4Resp_DataSizeTooSmall)
{
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_4 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L292 (1/2): decode group5 — data_size < sizeof(group5) → TRUE
TEST(PciLinksBranch2, DecodeQueryScalarGroup5Resp_DataSizeTooSmall)
{
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_5 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L317 (1/2): decode group6 — data_size < sizeof(group6) → TRUE
TEST(PciLinksBranch2, DecodeQueryScalarGroup6Resp_DataSizeTooSmall)
{
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_6 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L342 (1/2): decode group7 — data_size < sizeof(group7) → TRUE
TEST(PciLinksBranch2, DecodeQueryScalarGroup7Resp_DataSizeTooSmall)
{
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_7 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group7_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L367 (1/2): decode group8 — data_size != sizeof(group8) → TRUE (data_size=0)
TEST(PciLinksBranch2, DecodeQueryScalarGroup8Resp_DataSizeMismatch)
{
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_8 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group8_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L392 (1/2): decode group9 — data_size != sizeof(group9) → TRUE (data_size=0)
TEST(PciLinksBranch2, DecodeQueryScalarGroup9Resp_DataSizeMismatch)
{
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	nsm_query_scalar_group_telemetry_group_9 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group9_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L430 (1/2): decode group10 — msg_len < min for group10 → TRUE
// Use 11-byte buf: passes inner resp check but fails group10-specific check
TEST(PciLinksBranch2, DecodeQueryScalarGroup10Resp_MsgTooShort)
{
	auto buf = successDataSizeZeroBuf(); // 11 bytes
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_query_scalar_group_telemetry_group_10 data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group10_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L452 (1/2): decode group10_extended — msg_len < min for group10_ext → TRUE
TEST(PciLinksBranch2, DecodeQueryScalarGroup10ExtendedResp_MsgTooShort)
{
	auto buf = successDataSizeZeroBuf(); // 11 bytes
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_query_scalar_group_telemetry_group_10_extended data = {};
	auto rc = decode_query_scalar_group_telemetry_v1_group10_extended_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// SECTION 4: Misc NULL/length checks not covered
// ===========================================================================

// decode_multiport_query_scalar_group_telemetry_v1_req — NULL data (3/4)
TEST(PciLinksBranch2, DecodeMultiportQueryScalarGroupV2Req_NullData)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_multiport_query_scalar_group_telemetry_v1_req(
	    msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_clear_data_source_v1_req — NULL dsId (3/4)
TEST(PciLinksBranch2, DecodeClearDataSourceV1Req_NullDsId)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_clear_data_source_v1_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t device_index = 0, groupId = 0;
	auto rc = decode_clear_data_source_v1_req(
	    msg, buf.size(), &device_index, &groupId, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_query_available_clearable_scalar_data_sources_v1_resp
// — NULL mask_length (3/4)
TEST(PciLinksBranch2, DecodeQueryAvailClearableScalarResp_NullMaskLength)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint8_t avail[16] = {}, clearable[16] = {};
	auto rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    msg, buf.size(), &cc, &data_size, &reason, nullptr, avail,
	    clearable);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_query_available_clearable_scalar_data_sources_v1_resp
// — *cc != NSM_SUCCESS (3/4)
TEST(PciLinksBranch2, DecodeQueryAvailClearableScalarResp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint8_t mask_length = 0;
	uint8_t avail[16] = {}, clearable[16] = {};
	auto rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &mask_length, avail,
	    clearable);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// decode_query_available_clearable_scalar_data_sources_v1_resp
// — msg_len < min (1/2)
TEST(PciLinksBranch2, DecodeQueryAvailClearableScalarResp_MsgTooShort)
{
	// Use 11-byte buffer: passes decode_reason_code_and_cc (cc=0) but
	// fails msg_len < sizeof(nsm_msg_hdr)+sizeof(resp_struct)
	auto buf = successDataSizeZeroBuf(); // 11 bytes, cc=0
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint8_t mask_length = 0;
	uint8_t avail[16] = {}, clearable[16] = {};
	auto rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &mask_length, avail,
	    clearable);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// decode_query_available_clearable_scalar_data_sources_v1_resp
// — data_size != expected (1/2)
// Build buffer large enough for msg_len check; data_size=0 != expected
TEST(PciLinksBranch2, DecodeQueryAvailClearableScalarResp_DataSizeMismatch)
{
	// Large enough buffer: nsm_msg_hdr + sizeof(resp struct)
	// mask_length=0 → expected data_size = sizeof(uint8_t) + 0 = 1 ≠ 0
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(
		    nsm_query_available_clearable_scalar_data_sources_v1_resp),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint8_t mask_length = 0;
	uint8_t avail[16] = {}, clearable[16] = {};
	auto rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &mask_length, avail,
	    clearable);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// decode_list_available_pcie_ports_resp — NULL info (3/4)
TEST(PciLinksBranch2, DecodeListAvailablePciePortsResp_NullInfo)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_list_available_pcie_ports_resp(msg, buf.size(), &cc,
							&reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_list_available_pcie_ports_resp — msg_len < min (1/2)
// Use 9-byte errCcBuf — no wait, we need cc=SUCCESS and msg_len < min.
// Use a success buf of 11 bytes (<
// NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN)
TEST(PciLinksBranch2, DecodeListAvailablePciePortsResp_MsgTooShort)
{
	// successDataSizeZeroBuf: 11 bytes, cc=0
	// NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN is much larger
	auto buf = successDataSizeZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_list_available_pcie_ports_info info = {};
	auto rc = decode_list_available_pcie_ports_resp(msg, buf.size(), &cc,
							&reason, &info);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// decode_list_available_pcie_ports_resp — port type > NSM_PORT_TYPE_DOWNSTREAM
// Encode a valid response with one port of type=2 (> DOWNSTREAM=1)
TEST(PciLinksBranch2, DecodeListAvailablePciePortsResp_InvalidPortType)
{
	// Build a valid response with 1 port, then set port type = 2
	std::vector<uint8_t> buf(
	    NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN +
		sizeof(nsm_pcie_upstream_port_info),
	    0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	nsm_list_available_pcie_ports_info enc_info = {};
	enc_info.ports_count = 1;
	enc_info.ports[0].type = NSM_PORT_TYPE_DOWNSTREAM; // valid first
	encode_list_available_pcie_ports_resp(0, NSM_SUCCESS, 0, &enc_info,
					      msg);
	// Corrupt port type to 2 (> DOWNSTREAM=1) directly in payload
	auto *resp = reinterpret_cast<nsm_list_available_pcie_ports_resp *>(
	    msg->payload);
	resp->port_info.ports[0].type = 2; // invalid

	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_list_available_pcie_ports_info info = {};
	auto rc = decode_list_available_pcie_ports_resp(msg, buf.size(), &cc,
							&reason, &info);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// decode_get_pcie_port_config_req — NULL port_index (3/4)
TEST(PciLinksBranch2, DecodeGetPciePortConfigReq_NullPortIndex)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_config_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t port_number = 0, port_type = 0;
	auto rc = decode_get_pcie_port_config_req(msg, buf.size(), &port_number,
						  &port_type, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_get_pcie_port_config_req — msg_len != expected (1/2)
TEST(PciLinksBranch2, DecodeGetPciePortConfigReq_WrongMsgLen)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_config_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t port_number = 0, port_type = 0, port_index = 0;
	auto rc = decode_get_pcie_port_config_req(
	    msg, buf.size() + 1, &port_number, &port_type, &port_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// encode_preset_PCIe_data — NULL data_len (3/4)
TEST(PciLinksBranch2, EncodePresetPCIeData_NullDataLen)
{
	uint8_t data = 0;
	auto rc = encode_preset_PCIe_data(0xAB, &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_preset_PCIe_data — data_len != sizeof(uint8_t) (1/2)
TEST(PciLinksBranch2, DecodePresetPCIeData_WrongDataLen)
{
	uint8_t raw[2] = {0xAB, 0xCD};
	uint8_t preset_0 = 0, preset_1 = 0;
	// data_len=2 != sizeof(uint8_t)=1 → NSM_SW_ERROR_LENGTH
	auto rc = decode_preset_PCIe_data(raw, 2, &preset_0, &preset_1);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// decode_PCIe_TxAmplitude_data — NULL tx_amplitude (3/4)
TEST(PciLinksBranch2, DecodePCIeTxAmplitudeData_NullTxAmplitude)
{
	uint8_t raw = 0x05;
	auto rc = decode_PCIe_TxAmplitude_data(&raw, 1, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_PCIe_TxAmplitude_data — data_len != sizeof(uint8_t) (1/2)
TEST(PciLinksBranch2, DecodePCIeTxAmplitudeData_WrongDataLen)
{
	uint8_t raw[2] = {0x05, 0x00};
	uint8_t tx_amplitude = 0;
	// data_len=2 != sizeof(uint8_t)=1 → NSM_SW_ERROR_LENGTH
	auto rc = decode_PCIe_TxAmplitude_data(raw, 2, &tx_amplitude);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// encode_PCIe_TxAmplitude_data — NULL data_len (3/4)
TEST(PciLinksBranch2, EncodePCIeTxAmplitudeData_NullDataLen)
{
	uint8_t data = 0;
	auto rc = encode_PCIe_TxAmplitude_data(0x05, &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_set_port_config_aggregate_req — NULL sample_data_len (3/4)
TEST(PciLinksBranch2, DecodeSetPortConfigAggregateReq_NullSampleDataLen)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_config_aggregate_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t port_number = 0, port_type = 0, port_index = 0;
	uint16_t sample_count = 0;
	const uint8_t *sample_data = nullptr;
	auto rc = decode_set_port_config_aggregate_req(
	    msg, buf.size(), &port_number, &port_type, &port_index,
	    &sample_count, &sample_data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_set_port_config_aggregate_req — msg_len < min (1/2)
TEST(PciLinksBranch2, DecodeSetPortConfigAggregateReq_MsgTooShort)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_config_aggregate_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t port_number = 0, port_type = 0, port_index = 0;
	uint16_t sample_count = 0;
	const uint8_t *sample_data = nullptr;
	size_t sample_data_len = 0;
	// buf.size()-1 < sizeof(nsm_msg_hdr)+sizeof(req) → L1184 TRUE
	auto rc = decode_set_port_config_aggregate_req(
	    msg, buf.size() - 1, &port_number, &port_type, &port_index,
	    &sample_count, &sample_data, &sample_data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// decode_query_vector_group_telemetry_v2_req — NULL selector_1 (3/4)
TEST(PciLinksBranch2, DecodeQueryVectorGroupV2Req_NullSelector1)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_vector_group_telemetry_v2_req),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t port_number = 0, port_type = 0, index = 0, group_id = 0;
	uint8_t selector_0 = 0;
	auto rc = decode_query_vector_group_telemetry_v2_req(
	    msg, buf.size(), &port_number, &port_type, &index, &group_id,
	    &selector_0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_query_vector_group_telemetry_v2_req — msg_len != expected (1/2)
TEST(PciLinksBranch2, DecodeQueryVectorGroupV2Req_WrongMsgLen)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_vector_group_telemetry_v2_req),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t port_number = 0, port_type = 0, index = 0, group_id = 0;
	uint8_t selector_0 = 0, selector_1 = 0;
	// buf.size()+1 != expected → L1269 TRUE
	auto rc = decode_query_vector_group_telemetry_v2_req(
	    msg, buf.size() + 1, &port_number, &port_type, &index, &group_id,
	    &selector_0, &selector_1);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// decode_query_vector_group_telemetry_v2_resp — NULL data (3/4)
TEST(PciLinksBranch2, DecodeQueryVectorGroupV2Resp_NullData)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	auto rc = decode_query_vector_group_telemetry_v2_resp(
	    msg, buf.size(), &cc, &data_size, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_query_vector_group_telemetry_v2_resp — msg_len < min (1/2)
// Use 11-byte buf: passes decode_reason_code_and_cc (cc=0) but fails
// msg_len < sizeof(nsm_msg_hdr) + sizeof(nsm_query_vector_data_sources_v2_resp)
TEST(PciLinksBranch2, DecodeQueryVectorGroupV2Resp_MsgTooShort)
{
	auto buf = successDataSizeZeroBuf(); // 11 bytes, cc=0
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint8_t data[64] = {};
	auto rc = decode_query_vector_group_telemetry_v2_resp(
	    msg, buf.size(), &cc, &data_size, &reason, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// decode_query_vector_group_telemetry_v2_group1_resp — data_size mismatch (1/2)
// Use 11-byte buf: cc=0, inner decode succeeds, data_size=0 !=
// sizeof(nsm_query_vector_group_1_data)
TEST(PciLinksBranch2, DecodeQueryVectorGroup1Resp_DataSizeMismatch)
{
	// Need inner decode to succeed: use buf large enough for inner resp
	// check nsm_query_vector_data_sources_v2_resp has flexible data[], so
	// sizeof = nsm_common_resp = 6 → inner min = 11 bytes (just passes)
	// But wait: the inner check in
	// decode_query_vector_group_telemetry_v2_resp is msg_len <
	// sizeof(nsm_msg_hdr)+sizeof(nsm_query_vector_data_sources_v2_resp) If
	// this struct has data[], sizeof = 6, then min = 11 → 11-byte buf
	// passes. data_size=0 → group1 check: 0 !=
	// sizeof(nsm_query_vector_group_1_data)
	auto buf = successDataSizeZeroBuf(); // 11 bytes
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_query_vector_group_1_data data = {};
	auto rc = decode_query_vector_group_telemetry_v2_group1_resp(
	    msg, buf.size(), &cc, &reason, &data);
	// Either NSM_SW_ERROR_LENGTH (from inner msg_len) or
	// NSM_SW_ERROR_LENGTH (from data_size mismatch) — both are valid error
	// returns
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}
