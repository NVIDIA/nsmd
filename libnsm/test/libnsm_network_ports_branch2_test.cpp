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
 * Branch coverage batch 2 for network-ports.c
 *
 * Targets (uncovered branches not in existing branch test):
 *  L388   encode_get_port_telemetry_counter_req: pack fail
 *  L421   decode_get_port_telemetry_counter_req: data_size too small
 *  L441   encode_get_port_telemetry_counter_resp: NULL msg
 *  L519   encode_query_port_status_req: pack fail
 *  L552   decode_query_port_status_req: data_size too small
 *  L571   encode_query_port_status_resp: NULL msg
 *  L640   encode_query_port_characteristics_req: pack fail
 *  L673   decode_query_port_characteristics_req: data_size too small
 *  L692   encode_query_port_characteristics_resp: NULL msg
 *  L767   encode_query_ports_available_req: pack fail
 *  L798   decode_query_ports_available_req: data_size != 0
 *  L816   encode_query_ports_available_resp: NULL msg
 *  L926   encode_set_port_disable_future_resp: NULL msg
 *  L984   encode_get_port_disable_future_req: pack fail
 *  L1016  decode_get_port_disable_future_req: data_size != 0
 *  L1023  encode_get_port_disable_future_resp: NULL msg
 *  L1125  decode_get_power_mode_req: data_size != 0
 *  L1267  decode_set_power_mode_req: data_size wrong
 *  L1532  decode_get_fabric_manager_state_req: data_size != 0
 *  L1682  encode_get_eth_port_telemetry_counter_req: pack fail
 *  L1774  decode_aggregate_eth_port_telemetry_data: invalid tag
 *  L1829  encode_aggregate_eth_port_telemetry_data: invalid tag
 *  L1878  decode_get_network_addresses_req: data_size != sizeof(port_number)
 *  L1921  decode_aggregate_network_address_data: invalid tag
 *  L1959  encode_aggregate_network_address_data: invalid tag
 *  L1974  encode_get_port_ecc_counters_req: pack fail
 *  L2040  decode_aggregate_port_ecc_counter_data: invalid tag
 */

#include "base.h"
#include "network-ports.h"
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t kBadIid = 32; // > NSM_INSTANCE_MAX(31)

// ===========================================================================
// encode_get_port_telemetry_counter_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(NetPortsBranch2, EncodeGetPortTelemetryCounterReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_port_telemetry_counter_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_port_telemetry_counter_req: data_size too small (data_size=0 < 1)
// ===========================================================================
TEST(NetPortsBranch2, DecodeGetPortTelemetryCounterReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_req);
	std::vector<uint8_t> buf(msgLen,
				 0); // data_size=0 < sizeof(port_number)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t port = 0;
	auto rc = decode_get_port_telemetry_counter_req(msg, msgLen, &port);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_port_telemetry_counter_resp: NULL msg
// (uses instance_id & INSTANCEID_MASK masking so pack never fails)
// ===========================================================================
TEST(NetPortsBranch2, EncodeGetPortTelemetryCounterResp_NullMsg)
{
	struct nsm_port_counter_data data = {};
	auto rc = encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, 0,
							 &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_query_port_status_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(NetPortsBranch2, EncodeQueryPortStatusReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_status_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_query_port_status_req: data_size too small (data_size=0 < 1)
// ===========================================================================
TEST(NetPortsBranch2, DecodeQueryPortStatusReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_req);
	std::vector<uint8_t> buf(msgLen,
				 0); // data_size=0 < sizeof(port_number)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t port = 0;
	auto rc = decode_query_port_status_req(msg, msgLen, &port);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_query_port_status_resp: NULL msg
// (uses instance_id & INSTANCEID_MASK masking so pack never fails)
// ===========================================================================
TEST(NetPortsBranch2, EncodeQueryPortStatusResp_NullMsg)
{
	auto rc =
	    encode_query_port_status_resp(0, NSM_SUCCESS, 0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_query_port_characteristics_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(NetPortsBranch2, EncodeQueryPortCharacteristicsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_characteristics_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_query_port_characteristics_req: data_size too small
// ===========================================================================
TEST(NetPortsBranch2, DecodeQueryPortCharacteristicsReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_req);
	std::vector<uint8_t> buf(msgLen,
				 0); // data_size=0 < sizeof(port_number)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t port = 0;
	auto rc = decode_query_port_characteristics_req(msg, msgLen, &port);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_query_port_characteristics_resp: NULL msg
// (uses instance_id & INSTANCEID_MASK masking so pack never fails)
// ===========================================================================
TEST(NetPortsBranch2, EncodeQueryPortCharacteristicsResp_NullMsg)
{
	struct nsm_port_characteristics_data data = {};
	auto rc = encode_query_port_characteristics_resp(0, NSM_SUCCESS, 0,
							 &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_query_ports_available_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(NetPortsBranch2, EncodeQueryPortsAvailableReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_ports_available_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_query_ports_available_req: data_size != 0
// nsm_query_ports_available_req = nsm_common_req {command, data_size (uint8_t)}
// Set payload[1] = 1 to make data_size=1 != 0
// ===========================================================================
TEST(NetPortsBranch2, DecodeQueryPortsAvailableReq_DataSizeNonZero)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_ports_available_req);
	std::vector<uint8_t> buf(msgLen, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 1; // data_size = 1 != 0
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_query_ports_available_req(msg, msgLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_query_ports_available_resp: NULL msg
// (uses instance_id & INSTANCEID_MASK masking so pack never fails)
// ===========================================================================
TEST(NetPortsBranch2, EncodeQueryPortsAvailableResp_NullMsg)
{
	auto rc =
	    encode_query_ports_available_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_port_disable_future_resp: NULL msg
// (uses instance & INSTANCEID_MASK masking so pack never fails)
// ===========================================================================
TEST(NetPortsBranch2, EncodeSetPortDisableFutureResp_NullMsg)
{
	auto rc =
	    encode_set_port_disable_future_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_port_disable_future_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(NetPortsBranch2, EncodeGetPortDisableFutureReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_port_disable_future_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_port_disable_future_req: data_size != 0
// Set payload[1] = 1 (data_size field of nsm_common_req)
// ===========================================================================
TEST(NetPortsBranch2, DecodeGetPortDisableFutureReq_DataSizeNonZero)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_disable_future_req);
	std::vector<uint8_t> buf(msgLen, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 1; // data_size = 1 != 0
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_port_disable_future_req(msg, msgLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_port_disable_future_resp: NULL msg
// (uses instance & INSTANCEID_MASK masking so pack never fails)
// ===========================================================================
TEST(NetPortsBranch2, EncodeGetPortDisableFutureResp_NullMsg)
{
	bitfield8_t mask[PORT_MASK_DATA_SIZE / sizeof(bitfield8_t)] = {};
	auto rc = encode_get_port_disable_future_resp(0, NSM_SUCCESS, 0, mask,
						      nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_power_mode_req: data_size != 0
// Set payload[1] = 1 (data_size field of nsm_common_req)
// ===========================================================================
TEST(NetPortsBranch2, DecodeGetPowerModeReq_DataSizeNonZero)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_req);
	std::vector<uint8_t> buf(msgLen, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 1; // data_size = 1 != 0
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_power_mode_req(msg, msgLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_set_power_mode_req: data_size wrong
// zero-filled buffer → data_size=0 !=
// sizeof(nsm_power_mode_data)+sizeof(uint8_t)
// ===========================================================================
TEST(NetPortsBranch2, DecodeSetPowerModeReq_DataSizeWrong)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_set_power_mode_req);
	std::vector<uint8_t> buf(msgLen, 0); // data_size=0 != expected
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_power_mode_data data = {};
	auto rc = decode_set_power_mode_req(msg, msgLen, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_get_fabric_manager_state_req: data_size != 0
// Set payload[1] = 1 (data_size field of nsm_common_req)
// ===========================================================================
TEST(NetPortsBranch2, DecodeGetFabricManagerStateReq_DataSizeNonZero)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_fabric_manager_state_req);
	std::vector<uint8_t> buf(msgLen, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 1; // data_size = 1 != 0
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_fabric_manager_state_req(msg, msgLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_eth_port_telemetry_counter_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(NetPortsBranch2, EncodeGetEthPortTelemetryCounterReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_eth_port_telemetry_counter_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_aggregate_eth_port_telemetry_data: invalid tag → default case
// Signature: (const uint8_t *data, size_t *data_len, uint8_t tag, counter*)
// tag=0xFF is not in any case → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(NetPortsBranch2, DecodeAggregateEthPortTelemetryData_InvalidTag)
{
	uint8_t data[8] = {0};
	size_t data_len = sizeof(data);
	nsm_ethernet_port_counter_data counter = {};
	auto rc = decode_aggregate_eth_port_telemetry_data(data, &data_len,
							   0xFF, &counter);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_aggregate_eth_port_telemetry_data: invalid tag → default case
// ===========================================================================
TEST(NetPortsBranch2, EncodeAggregateEthPortTelemetryData_InvalidTag)
{
	uint8_t data[8] = {0};
	size_t data_len = sizeof(data);
	nsm_ethernet_port_counter_data counter = {};
	counter.ethernet_port_counter_data_64bit = 42;
	auto rc = encode_aggregate_eth_port_telemetry_data(0xFF, &counter, data,
							   &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_get_network_addresses_req: data_size != sizeof(port_number)=2
// zero-filled buffer → data_size=0 != 2 → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(NetPortsBranch2, DecodeGetNetworkAddressesReq_DataSizeMismatch)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_network_addresses_req);
	std::vector<uint8_t> buf(msgLen,
				 0); // data_size=0 != sizeof(uint16_t)=2
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t port = 0;
	auto rc = decode_get_network_addresses_req(msg, msgLen, &port);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_aggregate_network_address_data: invalid tag → default case
// ===========================================================================
TEST(NetPortsBranch2, DecodeAggregateNetworkAddressData_InvalidTag)
{
	uint8_t data[8] = {0};
	size_t data_len = sizeof(data);
	network_address_sample_data address = {};
	auto rc = decode_aggregate_network_address_data(0xFF, data, data_len,
							&address);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_aggregate_network_address_data: invalid tag → default case
// ===========================================================================
TEST(NetPortsBranch2, EncodeAggregateNetworkAddressData_InvalidTag)
{
	uint8_t data[8] = {0};
	size_t data_len = sizeof(data);
	network_address_sample_data address = {};
	auto rc = encode_aggregate_network_address_data(0xFF, &address, data,
							&data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_port_ecc_counters_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(NetPortsBranch2, EncodeGetPortEccCountersReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_port_ecc_counters_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_aggregate_port_ecc_counter_data: invalid tag → default case
// (existing test covers encode; this covers decode)
// ===========================================================================
TEST(NetPortsBranch2, DecodeAggregatePortEccCounterData_InvalidTag)
{
	uint8_t data[8] = {0};
	size_t data_len = sizeof(data);
	uint64_t counter = 0;
	auto rc = decode_aggregate_port_ecc_counter_data(0xFF, data, data_len,
							 &counter);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}
