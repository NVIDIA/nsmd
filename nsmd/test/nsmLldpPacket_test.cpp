/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

// Unit tests for NsmLldpPacket: request encoding, response handling and the
// IEEE 802.1AB TLV decoder behind it.

// The "#define private public" shim below must not rewrite libstdc++ access
// specifiers (hard error on GCC 15), so parse <sstream> and <any> first.
// Do not reorder these includes below the macro.
#include "network-ports.h"

#include <any>
#include <sstream>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmLLDPLib/nsmLldpPacket.hpp"

#undef protected
#undef private

#include <cstdint>
#include <string>
#include <vector>

using namespace nsm;

namespace
{

using TlvsServer = sdbusplus::server::xyz::openbmc_project::network::lldp::TLVs;
using IdSubtype = TlvsServer::IEEE802IdSubtype;
using SysCap = TlvsServer::SystemCapabilities;
using Bytes = std::vector<uint8_t>;

auto& testBus()
{
    static auto b = sdbusplus::bus::new_default();
    return b;
}

/** @brief One LLDP TLV: 7-bit type, 9-bit length, value */
Bytes tlv(uint8_t type, const Bytes& value)
{
    Bytes out{static_cast<uint8_t>((type << 1) | ((value.size() >> 8) & 0x01)),
              static_cast<uint8_t>(value.size() & 0xFF)};
    out.insert(out.end(), value.begin(), value.end());
    return out;
}

Bytes concat(const std::vector<Bytes>& parts)
{
    Bytes out;
    for (const auto& part : parts)
    {
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}

/** @brief Ethernet header (+ optional VLAN tag) followed by the LLDPDU */
Bytes frameOf(const Bytes& lldpdu, bool vlan = false)
{
    Bytes out(12, 0xAA);
    if (vlan)
    {
        out.insert(out.end(), {0x81, 0x00, 0x00, 0x01});
    }
    out.insert(out.end(), {0x88, 0xCC});
    out.insert(out.end(), lldpdu.begin(), lldpdu.end());
    return out;
}

Bytes chassisMac()
{
    return tlv(1, {4, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55});
}

Bytes portIfName()
{
    return tlv(2, {5, 'e', 't', 'h', '0'});
}

Bytes ttl()
{
    return tlv(3, {0x00, 0x78});
}

Bytes mandatory()
{
    return concat({chassisMac(), portIfName(), ttl()});
}

Bytes endTlv()
{
    return tlv(0, {});
}

struct NsmLldpPacketTest : public ::testing::Test
{
    static inline int counter = 0;
    std::string path =
        "/xyz/openbmc_project/inventory/system/chassis/CX_NIC_0/Ports/Port_1/"
        "LLDP/T" +
        std::to_string(counter++);

    NsmLldpPacket makePacket(uint8_t direction = NSM_LLDP_DIRECTION_RX)
    {
        return NsmLldpPacket(testBus(), "LldpPacket", "NSM_LLDP", path,
                             "/xyz/openbmc_project/inventory/system/chassis/"
                             "CX_NIC_0/Ports/Port_1",
                             1, direction);
    }

    static bool decode(NsmLldpPacket& pkt, const Bytes& frame)
    {
        return pkt.decodeAndPopulateTlvs(frame.data(), frame.size());
    }

    static Bytes response(uint8_t cc, const Bytes& data,
                          uint16_t reasonCode = 0)
    {
        Bytes buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + data.size() +
                      16,
                  0);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_EQ(encode_get_lldp_packet_resp(
                      0, cc, reasonCode, data.empty() ? nullptr : data.data(),
                      static_cast<uint16_t>(data.size()), msg, buf.size()),
                  NSM_SW_SUCCESS);
        return buf;
    }

    static uint8_t handle(NsmLldpPacket& pkt, const Bytes& buf, size_t len)
    {
        return pkt.handleResponseMsg(
            reinterpret_cast<const nsm_msg*>(buf.data()), len);
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Construction and request
// ---------------------------------------------------------------------------

TEST_F(NsmLldpPacketTest, Constructor_StartsEmpty)
{
    auto pkt = makePacket();

    EXPECT_EQ(pkt.dbusObjectPath(), path);
    EXPECT_TRUE(pkt.rawIntf_->data().empty());
    EXPECT_EQ(pkt.rawIntf_->lastUpdateTimestamp(), 0u);
    EXPECT_EQ(pkt.tlvsIntf_->chassisId(), "");
    EXPECT_EQ(pkt.tlvsIntf_->chassisIdSubtype(), IdSubtype::NotTransmitted);
    EXPECT_EQ(pkt.tlvsIntf_->managementVlanId(), 0);
}

TEST_F(NsmLldpPacketTest, Constructor_AssociationFollowsDirection)
{
    auto rx = makePacket(NSM_LLDP_DIRECTION_RX);
    ASSERT_EQ(rx.assocIntf_->associations().size(), 1u);
    EXPECT_EQ(std::get<1>(rx.assocIntf_->associations()[0]), "lldp_rx_data");

    path += "tx";
    auto tx = makePacket(NSM_LLDP_DIRECTION_TX);
    ASSERT_EQ(tx.assocIntf_->associations().size(), 1u);
    EXPECT_EQ(std::get<1>(tx.assocIntf_->associations()[0]), "lldp_tx_data");
}

TEST_F(NsmLldpPacketTest, GenRequestMsg_EncodesPortAndDirection)
{
    auto pkt = makePacket(NSM_LLDP_DIRECTION_TX);

    auto request = pkt.genRequestMsg(8, 3);

    ASSERT_TRUE(request.has_value());
    uint16_t port = 0;
    uint8_t direction = 0xFF;
    EXPECT_EQ(decode_get_lldp_packet_req(
                  reinterpret_cast<const nsm_msg*>(request->data()),
                  request->size(), &port, &direction),
              NSM_SW_SUCCESS);
    EXPECT_EQ(port, 1);
    EXPECT_EQ(direction, NSM_LLDP_DIRECTION_TX);
}

// ---------------------------------------------------------------------------
// Frame location (Ethernet header, VLAN tag)
// ---------------------------------------------------------------------------

TEST_F(NsmLldpPacketTest, FindPduOffset_PlainFrame)
{
    auto pkt = makePacket();
    auto frame = frameOf(mandatory());

    auto offset = pkt.findLldpPduOffset(frame.data(), frame.size());

    ASSERT_TRUE(offset.has_value());
    EXPECT_EQ(*offset, 14u);
}

TEST_F(NsmLldpPacketTest, FindPduOffset_VlanTaggedFrame)
{
    auto pkt = makePacket();
    auto frame = frameOf(mandatory(), true);

    auto offset = pkt.findLldpPduOffset(frame.data(), frame.size());

    ASSERT_TRUE(offset.has_value());
    EXPECT_EQ(*offset, 18u);
}

TEST_F(NsmLldpPacketTest, FindPduOffset_TooShortForEthernetHeader)
{
    auto pkt = makePacket();
    Bytes frame(13, 0);

    EXPECT_FALSE(pkt.findLldpPduOffset(frame.data(), frame.size()));
}

TEST_F(NsmLldpPacketTest, FindPduOffset_UnknownEtherType)
{
    auto pkt = makePacket();
    auto frame = frameOf(mandatory());
    frame[12] = 0x08;
    frame[13] = 0x00;

    EXPECT_FALSE(pkt.findLldpPduOffset(frame.data(), frame.size()));
}

TEST_F(NsmLldpPacketTest, FindPduOffset_VlanWithWrongInnerType)
{
    auto pkt = makePacket();
    auto frame = frameOf(mandatory(), true);
    frame[16] = 0x08;
    frame[17] = 0x00;

    EXPECT_FALSE(pkt.findLldpPduOffset(frame.data(), frame.size()));
}

TEST_F(NsmLldpPacketTest, FindPduOffset_VlanTypeButTruncatedTag)
{
    auto pkt = makePacket();
    Bytes frame(14, 0);
    frame[12] = 0x81;
    frame[13] = 0x00;

    EXPECT_FALSE(pkt.findLldpPduOffset(frame.data(), frame.size()));
}

// ---------------------------------------------------------------------------
// TLV decode: well-formed frames
// ---------------------------------------------------------------------------

TEST_F(NsmLldpPacketTest, Decode_FullFrame_PopulatesEveryTlv)
{
    auto pkt = makePacket();
    auto lldpdu = concat(
        {mandatory(), tlv(4, {'p', 'o', 'r', 't'}), tlv(5, {'s', 'w', '1'}),
         tlv(6, {'d', 'e', 's', 'c'}), tlv(7, {0x00, 0x14, 0x00, 0x14}),
         tlv(8, {5, 1, 192, 168, 1, 10, 2, 0, 0, 0, 1, 0}), endTlv()});

    ASSERT_TRUE(decode(pkt, frameOf(lldpdu)));

    EXPECT_EQ(pkt.tlvsIntf_->chassisIdSubtype(), IdSubtype::MacAddr);
    EXPECT_EQ(pkt.tlvsIntf_->chassisId(), "00:11:22:33:44:55");
    EXPECT_EQ(pkt.tlvsIntf_->portIdSubtype(), IdSubtype::IfName);
    EXPECT_EQ(pkt.tlvsIntf_->portId(), "eth0");
    EXPECT_EQ(pkt.tlvsIntf_->systemName(), "sw1");
    EXPECT_EQ(pkt.tlvsIntf_->systemDescription(), "desc");
    EXPECT_EQ(pkt.tlvsIntf_->systemCapabilities(),
              (std::vector<SysCap>{SysCap::Bridge, SysCap::Router}));
    EXPECT_EQ(pkt.tlvsIntf_->managementAddressIPv4(), "192.168.1.10");
}

TEST_F(NsmLldpPacketTest, Decode_VlanTaggedFrame)
{
    auto pkt = makePacket();

    ASSERT_TRUE(decode(pkt, frameOf(mandatory(), true)));
    EXPECT_EQ(pkt.tlvsIntf_->portId(), "eth0");
}

TEST_F(NsmLldpPacketTest, Decode_NoEndTlv_StillValid)
{
    auto pkt = makePacket();

    EXPECT_TRUE(decode(pkt, frameOf(mandatory())));
}

TEST_F(NsmLldpPacketTest, Decode_ClearsValuesOfThePreviousFrame)
{
    auto pkt = makePacket();
    ASSERT_TRUE(decode(
        pkt,
        frameOf(concat({mandatory(), tlv(5, {'s', 'w', '1'}), endTlv()}))));
    ASSERT_EQ(pkt.tlvsIntf_->systemName(), "sw1");

    ASSERT_TRUE(decode(pkt, frameOf(concat({mandatory(), endTlv()}))));

    EXPECT_EQ(pkt.tlvsIntf_->systemName(), "");
}

TEST_F(NsmLldpPacketTest, Decode_OptionalTlvsWithoutDecoder_AreSkipped)
{
    auto pkt = makePacket();
    auto lldpdu = concat({mandatory(), tlv(127, {0x00, 0x80, 0xC2, 0x01}),
                          tlv(4, {'p'}), endTlv()});

    EXPECT_TRUE(decode(pkt, frameOf(lldpdu)));
}

TEST_F(NsmLldpPacketTest, Decode_ChassisIdSubtypes)
{
    struct Case
    {
        uint8_t byte;
        IdSubtype expected;
    };
    const std::vector<Case> cases{
        {1, IdSubtype::ChassisComp},    {2, IdSubtype::IfAlias},
        {3, IdSubtype::PortComp},       {4, IdSubtype::MacAddr},
        {5, IdSubtype::NetworkAddr},    {6, IdSubtype::IfName},
        {7, IdSubtype::LocalAssign},    {0, IdSubtype::NotTransmitted},
        {9, IdSubtype::NotTransmitted},
    };
    for (const auto& c : cases)
    {
        path += "c";
        auto pkt = makePacket();
        auto lldpdu =
            concat({tlv(1, {c.byte, 'a', 'b'}), portIfName(), ttl(), endTlv()});

        EXPECT_TRUE(decode(pkt, frameOf(lldpdu))) << int(c.byte);
        EXPECT_EQ(pkt.tlvsIntf_->chassisIdSubtype(), c.expected) << int(c.byte);
    }
}

TEST_F(NsmLldpPacketTest, Decode_ChassisId_TextAndHexFallbacks)
{
    auto pkt = makePacket();

    // IfName: the bytes are text
    ASSERT_TRUE(decode(pkt, frameOf(concat({tlv(1, {6, 'e', '1'}), portIfName(),
                                            ttl(), endTlv()}))));
    EXPECT_EQ(pkt.tlvsIntf_->chassisId(), "e1");

    // NetworkAddr: hex with colons
    ASSERT_TRUE(decode(pkt, frameOf(concat({tlv(1, {5, 0x0A, 0x00, 0x01}),
                                            portIfName(), ttl(), endTlv()}))));
    EXPECT_EQ(pkt.tlvsIntf_->chassisId(), "0A:00:01");

    // MacAddr with a length other than 7: hex
    ASSERT_TRUE(decode(pkt, frameOf(concat({tlv(1, {4, 0x01, 0x02}),
                                            portIfName(), ttl(), endTlv()}))));
    EXPECT_EQ(pkt.tlvsIntf_->chassisId(), "01:02");
}

TEST_F(NsmLldpPacketTest, Decode_PortIdSubtypes)
{
    struct Case
    {
        uint8_t byte;
        IdSubtype expected;
    };
    const std::vector<Case> cases{
        {1, IdSubtype::IfAlias},        {2, IdSubtype::PortComp},
        {3, IdSubtype::MacAddr},        {4, IdSubtype::NetworkAddr},
        {5, IdSubtype::IfName},         {6, IdSubtype::AgentId},
        {7, IdSubtype::LocalAssign},    {0, IdSubtype::NotTransmitted},
        {9, IdSubtype::NotTransmitted},
    };
    for (const auto& c : cases)
    {
        path += "p";
        auto pkt = makePacket();
        auto lldpdu =
            concat({chassisMac(), tlv(2, {c.byte, 'a', 'b'}), ttl(), endTlv()});

        EXPECT_TRUE(decode(pkt, frameOf(lldpdu))) << int(c.byte);
        EXPECT_EQ(pkt.tlvsIntf_->portIdSubtype(), c.expected) << int(c.byte);
    }
}

TEST_F(NsmLldpPacketTest, Decode_PortId_MacAndHexFallbacks)
{
    auto pkt = makePacket();

    ASSERT_TRUE(decode(
        pkt, frameOf(concat({chassisMac(),
                             tlv(2, {3, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01}),
                             ttl(), endTlv()}))));
    EXPECT_EQ(pkt.tlvsIntf_->portId(), "DE:AD:BE:EF:00:01");

    ASSERT_TRUE(
        decode(pkt, frameOf(concat({chassisMac(), tlv(2, {4, 0x0A, 0x00}),
                                    ttl(), endTlv()}))));
    EXPECT_EQ(pkt.tlvsIntf_->portId(), "0A:00");
}

TEST_F(NsmLldpPacketTest, Decode_SystemCapabilities_EveryBit)
{
    auto pkt = makePacket();
    auto lldpdu =
        concat({mandatory(), tlv(7, {0x00, 0xFF, 0x00, 0x00}), endTlv()});

    ASSERT_TRUE(decode(pkt, frameOf(lldpdu)));

    EXPECT_EQ(pkt.tlvsIntf_->systemCapabilities(),
              (std::vector<SysCap>{
                  SysCap::Other, SysCap::Repeater, SysCap::Bridge,
                  SysCap::WLANAccessPoint, SysCap::Router, SysCap::Telephone,
                  SysCap::DOCSISCableDevice, SysCap::Station}));
}

TEST_F(NsmLldpPacketTest, Decode_SystemCapabilities_NoBitsMeansNone)
{
    auto pkt = makePacket();
    auto lldpdu =
        concat({mandatory(), tlv(7, {0x00, 0x00, 0x00, 0x00}), endTlv()});

    ASSERT_TRUE(decode(pkt, frameOf(lldpdu)));

    EXPECT_EQ(pkt.tlvsIntf_->systemCapabilities(),
              (std::vector<SysCap>{SysCap::None}));
}

TEST_F(NsmLldpPacketTest, Decode_ManagementAddresses)
{
    auto pkt = makePacket();

    Bytes v6{17, 2};
    for (uint8_t i = 0; i < 15; ++i)
    {
        v6.push_back(0);
    }
    v6.push_back(1); // ::1
    v6.insert(v6.end(), {2, 0, 0, 0, 1, 0});
    ASSERT_TRUE(
        decode(pkt, frameOf(concat({mandatory(), tlv(8, v6), endTlv()}))));
    EXPECT_EQ(pkt.tlvsIntf_->managementAddressIPv6(), "::1");

    Bytes mac{7, 6, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 2, 0, 0, 0, 1, 0};
    ASSERT_TRUE(
        decode(pkt, frameOf(concat({mandatory(), tlv(8, mac), endTlv()}))));
    EXPECT_EQ(pkt.tlvsIntf_->managementAddressMAC(), "AA:BB:CC:DD:EE:FF");

    // Unknown address family: accepted, nothing published
    Bytes other{3, 9, 1, 2, 2, 0, 0, 0, 1, 0};
    ASSERT_TRUE(
        decode(pkt, frameOf(concat({mandatory(), tlv(8, other), endTlv()}))));
}

// ---------------------------------------------------------------------------
// TLV decode: malformed frames
// ---------------------------------------------------------------------------

TEST_F(NsmLldpPacketTest, Decode_NullOrTooShort_Fails)
{
    auto pkt = makePacket();
    Bytes one{0x02};

    EXPECT_FALSE(pkt.decodeAndPopulateTlvs(nullptr, 20));
    EXPECT_FALSE(pkt.decodeAndPopulateTlvs(one.data(), one.size()));
}

TEST_F(NsmLldpPacketTest, Decode_NoLldpduAfterHeader_Fails)
{
    auto pkt = makePacket();

    EXPECT_FALSE(decode(pkt, frameOf({0x00})));
}

TEST_F(NsmLldpPacketTest, Decode_TlvLongerThanFrame_Fails)
{
    auto pkt = makePacket();
    Bytes truncated{0x02, 0x10, 4, 0x00};

    EXPECT_FALSE(decode(pkt, frameOf(truncated)));
}

TEST_F(NsmLldpPacketTest, Decode_MandatoryTlvOutOfOrder_Fails)
{
    auto pkt = makePacket();

    // Port ID first
    EXPECT_FALSE(decode(
        pkt, frameOf(concat({portIfName(), chassisMac(), ttl(), endTlv()}))));
    // Chassis ID twice
    EXPECT_FALSE(decode(
        pkt, frameOf(concat({chassisMac(), chassisMac(), ttl(), endTlv()}))));
    // TTL before Port ID
    EXPECT_FALSE(decode(pkt, frameOf(concat({chassisMac(), ttl(), endTlv()}))));
}

TEST_F(NsmLldpPacketTest, Decode_MandatoryTlvTooShort_Fails)
{
    auto pkt = makePacket();

    EXPECT_FALSE(decode(
        pkt, frameOf(concat({tlv(1, {4}), portIfName(), ttl(), endTlv()}))));
    EXPECT_FALSE(decode(
        pkt, frameOf(concat({chassisMac(), tlv(2, {5}), ttl(), endTlv()}))));
}

TEST_F(NsmLldpPacketTest, Decode_TtlWithWrongLength_Fails)
{
    auto pkt = makePacket();

    EXPECT_FALSE(decode(pkt, frameOf(concat({chassisMac(), portIfName(),
                                             tlv(3, {0x00}), endTlv()}))));
}

TEST_F(NsmLldpPacketTest, Decode_EndTlvProblems_Fail)
{
    auto pkt = makePacket();

    // End before the mandatory triple is complete
    EXPECT_FALSE(decode(pkt, frameOf(concat({chassisMac(), endTlv()}))));
    // End with a value
    EXPECT_FALSE(decode(pkt, frameOf(concat({mandatory(), tlv(0, {0x01})}))));
}

TEST_F(NsmLldpPacketTest, Decode_OptionalTlvBeforeMandatoryTriple_Fails)
{
    auto pkt = makePacket();

    for (uint8_t type : {4, 5, 6, 7, 8, 127})
    {
        EXPECT_FALSE(
            decode(pkt, frameOf(concat({chassisMac(), tlv(type, {0, 0, 0, 0}),
                                        portIfName(), ttl()}))))
            << int(type);
    }
}

TEST_F(NsmLldpPacketTest, Decode_SystemCapabilitiesWithWrongLength_Fails)
{
    auto pkt = makePacket();

    EXPECT_FALSE(decode(
        pkt, frameOf(concat({mandatory(), tlv(7, {0x00, 0x14}), endTlv()}))));
}

TEST_F(NsmLldpPacketTest, Decode_BadManagementAddressLength_Fails)
{
    auto pkt = makePacket();

    // Empty value
    EXPECT_FALSE(
        decode(pkt, frameOf(concat({mandatory(), tlv(8, {}), endTlv()}))));
    // Address string length 0
    EXPECT_FALSE(
        decode(pkt, frameOf(concat({mandatory(), tlv(8, {0, 1, 2, 3, 4, 5, 6}),
                                    endTlv()}))));
    // Value shorter than the fixed fields after the address
    EXPECT_FALSE(decode(
        pkt,
        frameOf(concat({mandatory(), tlv(8, {5, 1, 1, 2, 3, 4}), endTlv()}))));
    // OID length disagrees with the TLV length
    EXPECT_FALSE(decode(
        pkt, frameOf(concat({mandatory(),
                             tlv(8, {5, 1, 1, 2, 3, 4, 2, 0, 0, 0, 1, 3}),
                             endTlv()}))));
}

TEST_F(NsmLldpPacketTest, Decode_TrailingByteAfterTlvs_Fails)
{
    auto pkt = makePacket();
    auto lldpdu = concat({mandatory(), Bytes{0x00}});

    EXPECT_FALSE(decode(pkt, frameOf(lldpdu)));
}

// ---------------------------------------------------------------------------
// Response handling
// ---------------------------------------------------------------------------

TEST_F(NsmLldpPacketTest, HandleResponse_ValidFrame_PublishesRawAndTlvs)
{
    auto pkt = makePacket();
    auto frame = frameOf(concat({mandatory(), endTlv()}));
    auto buf = response(NSM_SUCCESS, frame);

    EXPECT_EQ(handle(pkt, buf, buf.size()), NSM_SUCCESS);

    EXPECT_EQ(pkt.rawIntf_->data(), frame);
    EXPECT_GT(pkt.rawIntf_->lastUpdateTimestamp(), 0u);
    EXPECT_EQ(pkt.tlvsIntf_->portId(), "eth0");
}

TEST_F(NsmLldpPacketTest, HandleResponse_EmptyBuffer_ClearsEverything)
{
    auto pkt = makePacket();
    auto frame = frameOf(concat({mandatory(), endTlv()}));
    auto full = response(NSM_SUCCESS, frame);
    ASSERT_EQ(handle(pkt, full, full.size()), NSM_SUCCESS);

    auto empty = response(NSM_SUCCESS, {});
    EXPECT_EQ(handle(pkt, empty, empty.size()), NSM_SUCCESS);

    EXPECT_TRUE(pkt.rawIntf_->data().empty());
    EXPECT_EQ(pkt.tlvsIntf_->portId(), "");
}

TEST_F(NsmLldpPacketTest, HandleResponse_MalformedFrame_KeepsRawClearsTlvs)
{
    auto pkt = makePacket();
    Bytes garbage(20, 0x55);
    auto buf = response(NSM_SUCCESS, garbage);

    EXPECT_EQ(handle(pkt, buf, buf.size()), NSM_SUCCESS);

    EXPECT_EQ(pkt.rawIntf_->data(), garbage);
    EXPECT_EQ(pkt.tlvsIntf_->chassisId(), "");
}

TEST_F(NsmLldpPacketTest, HandleResponse_ErrorCompletionCode_IsReturned)
{
    auto pkt = makePacket();
    auto buf = response(NSM_ERROR, {}, 0x0102);

    EXPECT_EQ(handle(pkt, buf, buf.size()), NSM_ERROR);
    EXPECT_TRUE(pkt.rawIntf_->data().empty());
}

TEST_F(NsmLldpPacketTest, HandleResponse_TooShortMessage_Fails)
{
    auto pkt = makePacket();
    auto buf = response(NSM_SUCCESS, {});

    EXPECT_NE(handle(pkt, buf, 2), NSM_SUCCESS);
}
