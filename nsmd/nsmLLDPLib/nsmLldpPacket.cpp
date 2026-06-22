/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION &
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
#include "nsmLldpPacket.hpp"

#include "network-ports.h"

#include <arpa/inet.h>

#include <phosphor-logging/lg2.hpp>

#include <chrono>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace nsm
{

namespace
{

using TlvsServer = sdbusplus::server::xyz::openbmc_project::network::lldp::TLVs;
using IdSubtype = TlvsServer::IEEE802IdSubtype;
using SysCap = TlvsServer::SystemCapabilities;

constexpr uint8_t LLDP_TLV_TYPE_END = 0;
constexpr uint8_t LLDP_TLV_TYPE_CHASSIS_ID = 1;
constexpr uint8_t LLDP_TLV_TYPE_PORT_ID = 2;
constexpr uint8_t LLDP_TLV_TYPE_TTL = 3;
constexpr uint8_t LLDP_TLV_TYPE_PORT_DESC = 4;
constexpr uint8_t LLDP_TLV_TYPE_SYSTEM_NAME = 5;
constexpr uint8_t LLDP_TLV_TYPE_SYSTEM_DESC = 6;
constexpr uint8_t LLDP_TLV_TYPE_SYSTEM_CAPS = 7;
constexpr uint8_t LLDP_TLV_TYPE_MGMT_ADDR = 8;

/* IEEE 802.1AB Chassis ID / Port ID subtype byte mappings to the
 * pdi IEEE802IdSubtype enum surfaced via the TLVs interface. */
IdSubtype chassisIdSubtypeFromByte(uint8_t b)
{
    switch (b)
    {
        case 1:
            return IdSubtype::ChassisComp;
        case 2:
            return IdSubtype::IfAlias;
        case 3:
            return IdSubtype::PortComp;
        case 4:
            return IdSubtype::MacAddr;
        case 5:
            return IdSubtype::NetworkAddr;
        case 6:
            return IdSubtype::IfName;
        case 7:
            return IdSubtype::LocalAssign;
        default:
            return IdSubtype::NotTransmitted;
    }
}

IdSubtype portIdSubtypeFromByte(uint8_t b)
{
    switch (b)
    {
        case 1:
            return IdSubtype::IfAlias;
        case 2:
            return IdSubtype::PortComp;
        case 3:
            return IdSubtype::MacAddr;
        case 4:
            return IdSubtype::NetworkAddr;
        case 5:
            return IdSubtype::IfName;
        case 6:
            return IdSubtype::AgentId;
        case 7:
            return IdSubtype::LocalAssign;
        default:
            return IdSubtype::NotTransmitted;
    }
}

/* Format an arbitrary byte buffer as a colon-separated hex string. Used
 * as a fallback for Chassis ID / Port ID values whose subtype is not a
 * simple ASCII or MAC encoding. */
std::string bytesToHexColon(const uint8_t* v, size_t len)
{
    std::ostringstream os;
    for (size_t i = 0; i < len; ++i)
    {
        if (i)
            os << ':';
        os << std::hex << std::uppercase;
        if (v[i] < 16)
            os << '0';
        os << static_cast<unsigned>(v[i]);
    }
    return os.str();
}

std::string macFromBytes(const uint8_t* v)
{
    return bytesToHexColon(v, 6);
}

uint64_t monotonicMicros()
{
    auto t = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(t).count());
}

/* Convert IEEE 802.1AB §8.5.7 System Capabilities bits to the pdi
 * SystemCapabilities enum array. */
std::vector<SysCap> decodeSystemCapabilities(uint16_t bits)
{
    std::vector<SysCap> caps;
    /* Bit positions per 802.1AB-2009 Table 8-4. */
    if (bits & (1u << 0))
        caps.push_back(SysCap::Other);
    if (bits & (1u << 1))
        caps.push_back(SysCap::Repeater);
    if (bits & (1u << 2))
        caps.push_back(SysCap::Bridge);
    if (bits & (1u << 3))
        caps.push_back(SysCap::WLANAccessPoint);
    if (bits & (1u << 4))
        caps.push_back(SysCap::Router);
    if (bits & (1u << 5))
        caps.push_back(SysCap::Telephone);
    if (bits & (1u << 6))
        caps.push_back(SysCap::DOCSISCableDevice);
    if (bits & (1u << 7))
        caps.push_back(SysCap::Station);
    if (caps.empty())
    {
        caps.push_back(SysCap::None);
    }
    return caps;
}

/* 802.1AB-2009 §8.5.9: after the management-address string, the TLV must
 * include interface subtype (1), interface number (4), OID length (1), and
 * OID (oidLen) with no trailing bytes. */
bool validateMgmtAddrTlvLength(uint16_t length, const uint8_t* val)
{
    if (length < 1)
    {
        return false;
    }
    const uint8_t addrStrLen = val[0];
    if (addrStrLen < 1)
    {
        return false;
    }
    const size_t addrBlockLen = 1u + addrStrLen;
    if (length < addrBlockLen + 6)
    {
        return false;
    }
    const uint8_t oidLen = val[addrBlockLen + 5];
    return length == addrBlockLen + 6 + oidLen;
}

enum class MandatoryPhase : uint8_t
{
    ExpectChassis = 0,
    ExpectPort,
    ExpectTtl,
    Done,
};

} // namespace

NsmLldpPacket::NsmLldpPacket(sdbusplus::bus_t& bus, const std::string& name,
                             const std::string& type,
                             const std::string& objectPath, uint16_t portNumber,
                             uint8_t direction) :
    NsmSensor(name, type), objectPath_(objectPath), portNumber_(portNumber),
    direction_(direction)
{
    rawIntf_ = std::make_shared<LldpRawFrameIntf>(bus, objectPath_.c_str());
    tlvsIntf_ = std::make_shared<LldpTlvsIntf>(bus, objectPath_.c_str());
    /* Initial-default state: empty buffer, never-populated timestamp. */
    rawIntf_->data(std::vector<uint8_t>{});
    rawIntf_->lastUpdateTimestamp(0);
    clearTlvs();

    lg2::info(
        "NsmLldpPacket created: name={NAME} path={PATH} port={PORT} dir={DIR}",
        "NAME", name, "PATH", objectPath_, "PORT", portNumber_, "DIR",
        direction_);
}

void NsmLldpPacket::clearTlvs()
{
    tlvsIntf_->chassisId("");
    tlvsIntf_->chassisIdSubtype(IdSubtype::NotTransmitted);
    tlvsIntf_->portId("");
    tlvsIntf_->portIdSubtype(IdSubtype::NotTransmitted);
    tlvsIntf_->systemName("");
    tlvsIntf_->systemDescription("");
    tlvsIntf_->systemCapabilities({});
    tlvsIntf_->managementAddressIPv4("");
    tlvsIntf_->managementAddressIPv6("");
    tlvsIntf_->managementAddressMAC("");
    tlvsIntf_->managementVlanId(0);
}

std::optional<std::vector<uint8_t>>
    NsmLldpPacket::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_lldp_packet_req), 0);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_lldp_packet_req(instanceId, portNumber_, direction_,
                                         requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmLldpPacket: encode_get_lldp_packet_req failed. eid={EID} port={PORT} dir={DIR} rc={RC}",
            "EID", eid, "PORT", portNumber_, "DIR", direction_, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmLldpPacket::handleResponseMsg(const nsm_msg* responseMsg,
                                         size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    std::vector<uint8_t> data(NSM_LLDP_PACKET_MAX_DATA_SIZE, 0);
    uint16_t dataSize = NSM_LLDP_PACKET_MAX_DATA_SIZE;

    auto rc = decode_get_lldp_packet_resp(responseMsg, responseLen, &cc,
                                          &reasonCode, data.data(), &dataSize);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::warning(
            "NsmLldpPacket: decode_get_lldp_packet_resp failed. port={PORT} dir={DIR} reasonCode={RCN} cc={CC} rc={RC}",
            "PORT", portNumber_, "DIR", direction_, "RCN", reasonCode, "CC", cc,
            "RC", rc);
        return cc ? cc : rc;
    }

    if (dataSize == 0)
    {
        rawIntf_->data(std::vector<uint8_t>{});
        rawIntf_->lastUpdateTimestamp(monotonicMicros());
        clearTlvs();
        lg2::debug(
            "NsmLldpPacket: empty buffer (ARCH GAP OMD-REQ-05 assumption) port={PORT} dir={DIR}",
            "PORT", portNumber_, "DIR", direction_);
        return NSM_SUCCESS;
    }

    data.resize(dataSize);
    rawIntf_->data(data);
    rawIntf_->lastUpdateTimestamp(monotonicMicros());

    if (!decodeAndPopulateTlvs(data.data(), data.size()))
    {
        clearTlvs();
        lg2::warning(
            "NsmLldpPacket: TLV decode failed; RawFrame.Data preserved. port={PORT} dir={DIR}",
            "PORT", portNumber_, "DIR", direction_);
    }
    return NSM_SUCCESS;
}

bool NsmLldpPacket::decodeAndPopulateTlvs(const uint8_t* frame, size_t frameLen)
{
    /* Clear prior TLV state so optional fields omitted in this frame are
     * not left stale from a previous poll. */
    clearTlvs();

    /* IEEE 802.1AB-2009 §8.1 TLV header:
     *   2 bytes: bit15:9 = Type (7 bits, top of byte 0),
     *            bit8:0  = Length (9 bits) — bit 8 is the LSB of byte 0,
     *            bit7:0  is byte 1.
     * Followed by `Length` value bytes.
     */
    if (frame == nullptr || frameLen < 2)
    {
        return false;
    }

    size_t cursor = 0;
    bool sawEnd = false;
    MandatoryPhase phase = MandatoryPhase::ExpectChassis;

    while (cursor + 2 <= frameLen)
    {
        uint8_t b0 = frame[cursor];
        uint8_t b1 = frame[cursor + 1];
        uint8_t type = static_cast<uint8_t>((b0 >> 1) & 0x7Fu);
        uint16_t length = static_cast<uint16_t>(((b0 & 0x01u) << 8) | b1);
        cursor += 2;
        if (cursor + length > frameLen)
        {
            return false;
        }
        const uint8_t* val = frame + cursor;

        switch (type)
        {
            case LLDP_TLV_TYPE_END:
                /* End-of-LLDPDU marker; must have length 0 per §8.5.1 and
                 * must follow the mandatory Chassis→Port→TTL triple. */
                if (length != 0 || phase != MandatoryPhase::Done)
                {
                    return false;
                }
                sawEnd = true;
                break;
            case LLDP_TLV_TYPE_CHASSIS_ID:
            {
                if (phase != MandatoryPhase::ExpectChassis || length < 2)
                {
                    return false;
                }
                IdSubtype st = chassisIdSubtypeFromByte(val[0]);
                tlvsIntf_->chassisIdSubtype(st);
                std::string idStr;
                if (st == IdSubtype::MacAddr && length == 7)
                {
                    idStr = macFromBytes(val + 1);
                }
                else if (st == IdSubtype::IfName || st == IdSubtype::IfAlias ||
                         st == IdSubtype::PortComp ||
                         st == IdSubtype::ChassisComp ||
                         st == IdSubtype::LocalAssign)
                {
                    idStr.assign(reinterpret_cast<const char*>(val + 1),
                                 length - 1);
                }
                else
                {
                    idStr = bytesToHexColon(val + 1, length - 1);
                }
                tlvsIntf_->chassisId(idStr);
                phase = MandatoryPhase::ExpectPort;
                break;
            }
            case LLDP_TLV_TYPE_PORT_ID:
            {
                if (phase != MandatoryPhase::ExpectPort || length < 2)
                {
                    return false;
                }
                IdSubtype st = portIdSubtypeFromByte(val[0]);
                tlvsIntf_->portIdSubtype(st);
                std::string idStr;
                if (st == IdSubtype::MacAddr && length == 7)
                {
                    idStr = macFromBytes(val + 1);
                }
                else if (st == IdSubtype::IfName || st == IdSubtype::IfAlias ||
                         st == IdSubtype::PortComp ||
                         st == IdSubtype::AgentId ||
                         st == IdSubtype::LocalAssign)
                {
                    idStr.assign(reinterpret_cast<const char*>(val + 1),
                                 length - 1);
                }
                else
                {
                    idStr = bytesToHexColon(val + 1, length - 1);
                }
                tlvsIntf_->portId(idStr);
                phase = MandatoryPhase::ExpectTtl;
                break;
            }
            case LLDP_TLV_TYPE_TTL:
                /* TTL (length 2, uint16 seconds) — surfaced on the
                 * upstream TLVs interface as part of frame freshness;
                 * the pdi TLVs schema does not expose TTL as a discrete
                 * property, so we accept and ignore the value here but
                 * still validate length for parse correctness. */
                if (phase != MandatoryPhase::ExpectTtl || length != 2)
                {
                    return false;
                }
                phase = MandatoryPhase::Done;
                break;
            case LLDP_TLV_TYPE_PORT_DESC:
                if (phase != MandatoryPhase::Done)
                {
                    return false;
                }
                /* No discrete PortDescription property in the upstream
                 * TLVs interface; raw bytes remain accessible via
                 * RawFrame.Data. */
                break;
            case LLDP_TLV_TYPE_SYSTEM_NAME:
                if (phase != MandatoryPhase::Done)
                {
                    return false;
                }
                tlvsIntf_->systemName(
                    std::string(reinterpret_cast<const char*>(val), length));
                break;
            case LLDP_TLV_TYPE_SYSTEM_DESC:
                if (phase != MandatoryPhase::Done)
                {
                    return false;
                }
                tlvsIntf_->systemDescription(
                    std::string(reinterpret_cast<const char*>(val), length));
                break;
            case LLDP_TLV_TYPE_SYSTEM_CAPS:
            {
                if (phase != MandatoryPhase::Done || length != 4)
                {
                    return false;
                }
                uint16_t caps = static_cast<uint16_t>((val[0] << 8) | val[1]);
                tlvsIntf_->systemCapabilities(decodeSystemCapabilities(caps));
                break;
            }
            case LLDP_TLV_TYPE_MGMT_ADDR:
            {
                if (phase != MandatoryPhase::Done ||
                    !validateMgmtAddrTlvLength(length, val))
                {
                    return false;
                }
                /* 802.1AB-2009 §8.5.9. Format:
                 *   1 byte mgmt addr str length (= 1 + addr len)
                 *   1 byte address subtype (IANA address family)
                 *   N bytes address
                 *   1 byte interface subtype
                 *   4 bytes interface number
                 *   1 byte OID length
                 *   N bytes OID
                 */
                uint8_t addrStrLen = val[0];
                uint8_t addrSubtype = val[1];
                uint8_t addrLen = static_cast<uint8_t>(addrStrLen - 1);
                const uint8_t* addr = val + 2;
                if (addrSubtype == 1 && addrLen == 4)
                {
                    /* IPv4 */
                    char buf[INET_ADDRSTRLEN] = {};
                    if (inet_ntop(AF_INET, addr, buf, sizeof(buf)))
                    {
                        tlvsIntf_->managementAddressIPv4(std::string(buf));
                    }
                }
                else if (addrSubtype == 2 && addrLen == 16)
                {
                    /* IPv6 */
                    char buf[INET6_ADDRSTRLEN] = {};
                    if (inet_ntop(AF_INET6, addr, buf, sizeof(buf)))
                    {
                        tlvsIntf_->managementAddressIPv6(std::string(buf));
                    }
                }
                else if (addrSubtype == 6 && addrLen == 6)
                {
                    /* IEEE 802 MAC */
                    tlvsIntf_->managementAddressMAC(macFromBytes(addr));
                }
                break;
            }
            default:
                if (phase != MandatoryPhase::Done)
                {
                    return false;
                }
                /* Optional / OEM TLVs not decoded; raw bytes remain
                 * accessible via RawFrame.Data. */
                break;
        }
        cursor += length;
        if (sawEnd)
        {
            break;
        }
    }

    /* IEEE 802.1AB requires Chassis ID, Port ID, TTL in that order, followed
     * by an End TLV that terminates the LLDPDU with no trailing bytes. */
    if (!sawEnd || phase != MandatoryPhase::Done || cursor != frameLen)
    {
        return false;
    }
    return true;
}

} // namespace nsm
