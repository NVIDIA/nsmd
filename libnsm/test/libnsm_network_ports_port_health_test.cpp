/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

// Unit tests for the nsm_port_characteristics_data bit-field layout:
// link_health bits 17:16, attention_trigger bits 25:18, port_down_reason_code
// narrowed to bits 15:10.

#include "base.h"
#include "network-ports.h"

#include <cstring>
#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Helper: build a port_characteristics_data with a raw 32-bit status word
// ---------------------------------------------------------------------------
static struct nsm_port_characteristics_data makePortData(uint32_t statusWord,
							 uint32_t lineRate = 0,
							 uint32_t dataRate = 0,
							 uint32_t laneInfo = 0)
{
	struct nsm_port_characteristics_data d{};
	memcpy(&d.port_status, &statusWord, sizeof(uint32_t));
	d.nv_port_line_rate_mbps = lineRate;
	d.nv_port_data_rate_kbps = dataRate;
	d.status_lane_info = laneInfo;
	return d;
}

// Convenience: build a full NSM response from port_characteristics_data
static std::vector<uint8_t>
buildResponse(const struct nsm_port_characteristics_data &portData)
{
	auto mutablePortData = portData;
	uint16_t reasonCode = ERR_NULL;
	std::vector<uint8_t> responseBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp),
	    0);
	auto *response = reinterpret_cast<nsm_msg *>(responseBuf.data());
	encode_query_port_characteristics_resp(0, NSM_SUCCESS, reasonCode,
					       &mutablePortData, response);
	return responseBuf;
}

// ===========================================================================
// struct status bit-field layout — link_health (bits 17:16)
// ===========================================================================

TEST(PortHealthStructErrata, LinkHealth_NA)
{
	// bits 17:16 = 00b → link_health = 0 (NA)
	uint32_t statusWord = 0x00000000u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.link_health, 0u);
}

TEST(PortHealthStructErrata, LinkHealth_Attention)
{
	// bits 17:16 = 01b → bit 16 set → 0x00010000
	uint32_t statusWord = 0x00010000u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.link_health, 1u);
	EXPECT_EQ(d.port_status.attention_trigger, 0u);
	EXPECT_EQ(d.port_status.port_down_reason_code, 0u);
}

TEST(PortHealthStructErrata, LinkHealth_Healthy)
{
	// bits 17:16 = 10b → bit 17 set → 0x00020000
	uint32_t statusWord = 0x00020000u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.link_health, 2u);
}

TEST(PortHealthStructErrata, LinkHealth_Reserved)
{
	// bits 17:16 = 11b: reserved wire value. The 2-bit field must decode
	// it as 3 without bleeding into adjacent fields.
	uint32_t statusWord = 0x00030000u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.link_health, 3u);
}

// ===========================================================================
// struct status bit-field layout — attention_trigger (bits 25:18)
// ===========================================================================

TEST(PortHealthStructErrata, AttentionTrigger_NA)
{
	uint32_t statusWord = 0x00000000u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.attention_trigger, 0u);
}

TEST(PortHealthStructErrata, AttentionTrigger_RawBER)
{
	// value 1: 1 << 18 = 0x00040000
	uint32_t statusWord = 0x00040000u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.attention_trigger, 1u);
	EXPECT_EQ(d.port_status.link_health, 0u);
}

TEST(PortHealthStructErrata, AttentionTrigger_EffectiveBER)
{
	// value 2: 2 << 18 = 0x00080000
	uint32_t statusWord = 0x00080000u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.attention_trigger, 2u);
}

TEST(PortHealthStructErrata, AttentionTrigger_SymbolErrorCount)
{
	// value 9: 9 << 18 = 0x00240000
	uint32_t statusWord = 0x00240000u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.attention_trigger, 9u);
}

TEST(PortHealthStructErrata, AttentionTrigger_AllTenValues)
{
	// Verify all 10 trigger values (0-9) encode and decode correctly
	for (uint32_t expected = 0; expected <= 9; ++expected) {
		uint32_t statusWord = expected << 18u;
		auto d = makePortData(statusWord);
		EXPECT_EQ(d.port_status.attention_trigger, expected)
		    << "attention_trigger value " << expected << " failed";
		EXPECT_EQ(d.port_status.link_health, 0u)
		    << "link_health should be 0 when only attention_trigger is "
		       "set";
	}
}

// ===========================================================================
// Combined: link_health + attention_trigger independence
// ===========================================================================

TEST(PortHealthStructErrata, LinkHealthAndTriggerCombined_AttentionRawBER)
{
	// link_health=1 (Attention) + attention_trigger=1 (RawBER)
	// 0x00010000 | 0x00040000 = 0x00050000
	uint32_t statusWord = 0x00050000u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.link_health, 1u);
	EXPECT_EQ(d.port_status.attention_trigger, 1u);
}

TEST(PortHealthStructErrata, LinkHealthAndTriggerCombined_AttentionSymbolError)
{
	// link_health=1 (Attention) + attention_trigger=9 (SymbolErrorCount)
	// 0x00010000 | 0x00240000 = 0x00250000
	uint32_t statusWord = 0x00250000u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.link_health, 1u);
	EXPECT_EQ(d.port_status.attention_trigger, 9u);
}

TEST(PortHealthStructErrata, AllLinkHealthWithEachTrigger)
{
	// Each of the 3 valid link_health values with attention_trigger=5
	const uint32_t trigger5 = 5u << 18u; // PLRRXBandwidthLoss
	for (uint32_t health = 0; health <= 2; ++health) {
		uint32_t statusWord = (health << 16u) | trigger5;
		auto d = makePortData(statusWord);
		EXPECT_EQ(d.port_status.link_health, health);
		EXPECT_EQ(d.port_status.attention_trigger, 5u);
	}
}

// ===========================================================================
// port_down_reason_code is 6 bits (bits 15:10) — corrected from old 8-bit
// ===========================================================================

TEST(PortHealthStructErrata, PortDownReasonCode_SixBits)
{
	// Set port_down_reason_code = 1 at bits 15:10: 1 << 10 = 0x00000400
	uint32_t statusWord = 0x00000400u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.port_down_reason_code, 1u);
	EXPECT_EQ(d.port_status.link_health, 0u);
	EXPECT_EQ(d.port_status.attention_trigger, 0u);
}

TEST(PortHealthStructErrata, PortDownReasonCode_MaxSixBitValue)
{
	// Max 6-bit value = 63 = 0x3F at bits 15:10: 63 << 10 = 0x0000FC00
	uint32_t statusWord = 0x0000FC00u;
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.port_down_reason_code, 63u);
	EXPECT_EQ(d.port_status.link_health, 0u);
}

TEST(PortHealthStructErrata, PortDownReasonCodeDoesNotBleedIntoLinkHealth)
{
	// Old layout had 8 bits (bits 17:10). With the corrected 6-bit layout,
	// bits 17:16 belong to link_health exclusively.
	// Set bits 15:10 to max (6 bits = 63). Bits 17:16 remain 0.
	uint32_t statusWord =
	    0x0000FC00u; // bits 15:10 = 111111, bits 17:16 = 00
	auto d = makePortData(statusWord);
	EXPECT_EQ(d.port_status.port_down_reason_code, 63u);
	EXPECT_EQ(d.port_status.link_health, 0u); // must not be polluted
}

// ===========================================================================
// Round-trip: encode → decode preserves link_health and attention_trigger
// ===========================================================================

TEST(PortHealthStructErrata, RoundTripEncodeDecodeHealthyNoTrigger)
{
	// link_health=2 (Healthy), attention_trigger=0
	auto portData = makePortData(0x00020000u, 100000, 50000, 4);
	auto responseBuf = buildResponse(portData);

	auto *response = reinterpret_cast<const nsm_msg *>(responseBuf.data());
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	uint16_t dataSize = 0;
	struct nsm_port_characteristics_data decoded{};

	auto rc = decode_query_port_characteristics_resp(
	    response, responseBuf.size(), &cc, &reasonCode, &dataSize,
	    &decoded);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(decoded.port_status.link_health, 2u);
	EXPECT_EQ(decoded.port_status.attention_trigger, 0u);
}

TEST(PortHealthStructErrata, RoundTripEncodeDecodeAttentionRawBER)
{
	// link_health=1 (Attention), attention_trigger=1 (RawBER)
	auto portData = makePortData(0x00050000u, 0, 0, 0);
	auto responseBuf = buildResponse(portData);

	auto *response = reinterpret_cast<const nsm_msg *>(responseBuf.data());
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	uint16_t dataSize = 0;
	struct nsm_port_characteristics_data decoded{};

	auto rc = decode_query_port_characteristics_resp(
	    response, responseBuf.size(), &cc, &reasonCode, &dataSize,
	    &decoded);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(decoded.port_status.link_health, 1u);
	EXPECT_EQ(decoded.port_status.attention_trigger, 1u);
}
