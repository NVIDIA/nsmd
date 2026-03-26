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
 * Branch coverage for network-ports.c partial branches.
 *
 * Targets 1/2 and 3/4 branches not exercised by existing tests.
 */

#include "base.h"
#include "network-ports.h"
#include <gtest/gtest.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: build a buffer with cc=error at payload[1], used for 3/4 branches:
//   if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
// decode_reason_code_and_cc with this buffer: cc=0xFF, len=9 == 9 →
//   returns NSM_SW_SUCCESS; *cc=0xFF → second condition TRUE
// ---------------------------------------------------------------------------
static std::vector<uint8_t>
errCcBuf(size_t sz = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp))
{
	std::vector<uint8_t> buf(sz, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // completion_code != NSM_SUCCESS
	return buf;
}

// ===========================================================================
// L320: decode_get_nvlink_agg_led_status_resp
// if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) second condition TRUE
// rc=SUCCESS but *cc=0xFF (error)
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeGetNvlinkAggLedStatusResp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum nsm_nvlink_led_state led_state = NSM_NVLINK_LED_ERROR;
	auto rc = decode_get_nvlink_agg_led_status_resp(msg, buf.size(), &cc,
							&reason, &led_state);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L905/L917: decode_set_port_disable_future_req
// L905: msg != NULL but mask == NULL → NSM_SW_ERROR_NULL (3/4 branch)
// L917: data_size != PORT_MASK_DATA_SIZE → NSM_SW_ERROR_DATA (1/2 branch)
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeSetPortDisableFutureReq_NullMask)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_disable_future_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	// msg != NULL, mask == NULL → L905 second condition TRUE
	auto rc = decode_set_port_disable_future_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(NetPortsBranchCoverage, DecodeSetPortDisableFutureReq_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_disable_future_req), 0);
	// data_size at payload[1] = 0 != PORT_MASK_DATA_SIZE (32) → L917 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	bitfield8_t mask[PORT_MASK_DATA_SIZE] = {};
	auto rc = decode_set_port_disable_future_req(msg, buf.size(), mask);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L964: decode_set_port_disable_future_resp
// if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) second condition TRUE
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeSetPortDisableFutureResp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_set_port_disable_future_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L1296: decode_set_power_mode_resp
// if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) second condition TRUE
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeSetPowerModeResp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_power_mode_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L1452: decode_set_switch_isolation_mode_req
// unpack_nsm_header fails (zero-filled header → invalid PCI VID 0x10DE)
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeSetSwitchIsolationModeReq_UnpackHeaderFail)
{
	// Zero-filled buffer: unpack_nsm_header sees 0x0000 != 0x10DE → fail
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_switch_isolation_mode_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t isolation_mode = 0;
	auto rc = decode_set_switch_isolation_mode_req(msg, buf.size(),
						       &isolation_mode);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L1464: decode_set_switch_isolation_mode_req
// req->hdr.data_size != expected → NSM_SW_ERROR_DATA
// Build valid header, exact size, then set data_size = 0 (wrong)
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeSetSwitchIsolationModeReq_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_switch_isolation_mode_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// encode to get a valid NSM header
	auto enc_rc = encode_set_switch_isolation_mode_req(0, 1, msg);
	ASSERT_EQ(enc_rc, NSM_SW_SUCCESS);
	// Corrupt data_size to 0 (expected = 1) → L1464 TRUE
	auto *req =
	    reinterpret_cast<nsm_set_switch_isolation_mode_req *>(msg->payload);
	req->hdr.data_size = 0;
	uint8_t isolation_mode = 0;
	auto rc = decode_set_switch_isolation_mode_req(msg, buf.size(),
						       &isolation_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1553: decode_get_fabric_manager_state_resp
// if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) second condition TRUE
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeGetFabricManagerStateResp_ErrorCc)
{
	auto buf = errCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t data_size = 0;
	nsm_fabric_manager_state_data data = {};
	auto rc = decode_get_fabric_manager_state_resp(
	    msg, buf.size(), &cc, &reason, &data_size, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L1640: decode_nsm_get_fabric_manager_state_event
// payload == NULL (msg/event_class/event_state all valid) → NSM_SW_ERROR_NULL
// The 3/4 branch: previous tests cover msg==NULL and event_class==NULL;
// this covers payload==NULL in the compound condition.
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeGetFabricManagerStateEvent_NullPayload)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t event_class = 0;
	uint16_t event_state = 0;
	auto rc = decode_nsm_get_fabric_manager_state_event(
	    msg, buf.size(), &event_class, &event_state, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L1651: decode_nsm_get_fabric_manager_state_event
// event->data_size > msg_len - sizeof(hdr) - NSM_EVENT_MIN_LEN → TRUE
// Use minimum-size buffer (0 available bytes) with data_size=1 → 1 > 0
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeGetFabricManagerStateEvent_DataSizeOverflow)
{
	// Buffer: nsm_msg_hdr(5) + NSM_EVENT_MIN_LEN(6) = 11 bytes
	// available_space = 11 - 5 - 6 = 0
	// event->data_size at payload[5] = 1 → 1 > 0 → L1651 TRUE
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
	buf[sizeof(nsm_msg_hdr) + 5] = 1; // event->data_size = 1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t event_class = 0;
	uint16_t event_state = 0;
	nsm_get_fabric_manager_state_event_payload payload = {};
	auto rc = decode_nsm_get_fabric_manager_state_event(
	    msg, buf.size(), &event_class, &event_state, &payload);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1658: decode_nsm_get_fabric_manager_state_event
// event->data_size < sizeof(*payload) → TRUE
// sizeof(nsm_fabric_manager_state_data) = 18; set data_size = 1
// Buffer large enough to pass L1651 check: nsm_msg_hdr + NSM_EVENT_MIN_LEN + 1
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeGetFabricManagerStateEvent_DataSizeTooSmall)
{
	// Buffer: 5 + 6 + 1 = 12; available_space = 12 - 5 - 6 = 1
	// data_size = 1 → 1 <= 1 (passes L1651) → 1 < 18 (L1658 TRUE)
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + 1,
				 0);
	buf[sizeof(nsm_msg_hdr) + 5] = 1; // event->data_size = 1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t event_class = 0;
	uint16_t event_state = 0;
	nsm_get_fabric_manager_state_event_payload payload = {};
	auto rc = decode_nsm_get_fabric_manager_state_event(
	    msg, buf.size(), &event_class, &event_state, &payload);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1765: decode_aggregate_eth_port_telemetry_data (32-bit case)
// *data_len != sizeof(uint32_t) → TRUE
// Use RX_FCS_ERRORS (32-bit tag), pass data_len = 3 (not 4)
// ===========================================================================
TEST(NetPortsBranchCoverage,
     DecodeAggregateEthPortTelemetry32Bit_DataLenMismatch)
{
	uint8_t data[4] = {0};
	size_t data_len = 3; // != sizeof(uint32_t)=4 → L1765 TRUE
	nsm_ethernet_port_counter_data counter = {};
	auto rc = decode_aggregate_eth_port_telemetry_data(
	    data, &data_len, ETHERNET_PORT_COUNTER_TAG_RX_FCS_ERRORS, &counter);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L1798: encode_aggregate_eth_port_telemetry_data (64-bit case)
// *data_len != sizeof(uint64_t) → TRUE
// Use RX_BYTES (64-bit tag), pass data_len = 4 (not 8)
// ===========================================================================
TEST(NetPortsBranchCoverage,
     EncodeAggregateEthPortTelemetry64Bit_DataLenMismatch)
{
	nsm_ethernet_port_counter_data counter = {};
	counter.ethernet_port_counter_data_64bit = 0x12345678;
	uint8_t data[8] = {0};
	size_t data_len = 4; // != sizeof(uint64_t)=8 → L1798 TRUE
	auto rc = encode_aggregate_eth_port_telemetry_data(
	    ETHERNET_PORT_COUNTER_TAG_RX_BYTES, &counter, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L1897: decode_aggregate_network_address_data (NSM_TAG_LINK_TYPE case)
// data_len != sizeof(uint8_t) → TRUE
// Pass data_len = 2 (not 1)
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeAggregateNetworkAddressData_LinkTypeBadLen)
{
	uint8_t data[2] = {0};
	network_address_sample_data address = {};
	// data_len = 2 != 1 → L1897 TRUE
	auto rc = decode_aggregate_network_address_data(NSM_TAG_LINK_TYPE, data,
							2, &address);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L1932: encode_aggregate_network_address_data
// data == NULL (address != NULL) → secondary null condition
// ===========================================================================
TEST(NetPortsBranchCoverage, EncodeAggregateNetworkAddressData_NullData)
{
	network_address_sample_data address = {};
	size_t data_len = 0;
	auto rc = encode_aggregate_network_address_data(
	    NSM_TAG_LINK_TYPE, &address, nullptr, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(NetPortsBranchCoverage, EncodeAggregateNetworkAddressData_NullDataLen)
{
	network_address_sample_data address = {};
	uint8_t data[8] = {0};
	auto rc = encode_aggregate_network_address_data(
	    NSM_TAG_LINK_TYPE, &address, data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L2007: decode_get_port_ecc_counters_req
// request->hdr.data_size < sizeof(request->port_number) → TRUE
// data_size = 0 < 2 → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(NetPortsBranchCoverage, DecodeGetPortEccCountersReq_DataSizeTooSmall)
{
	// Zero-fill: data_size at payload[1] = 0 < sizeof(uint16_t)=2
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_ecc_counters_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t port_number = 0;
	auto rc =
	    decode_get_port_ecc_counters_req(msg, buf.size(), &port_number);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L2054: encode_aggregate_port_ecc_counter_data
// switch(tag) default case → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(NetPortsBranchCoverage, EncodeAggregatePortEccCounterData_InvalidTag)
{
	uint8_t data[8] = {0};
	size_t data_len = sizeof(uint64_t);
	// tag = 0xFF is not in valid ECC tag list → default → NSM_SW_ERROR_DATA
	auto rc =
	    encode_aggregate_port_ecc_counter_data(0xFF, 42, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}
