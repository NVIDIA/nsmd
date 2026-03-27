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

/*
 * Branch coverage tests for nsmPCIeFunction.cpp
 *
 * Covers branches with proper NsmInterfaceProvider construction:
 * - genRequestMsg: isMultiPciePortEnabled=false → genSinglePCIeRequestMsg
 *   - encode succeeds → has_value (TRUE path for if(rc))
 *   - encode fails (instanceId > NSM_INSTANCE_MAX) → nullopt (FALSE path)
 * - genRequestMsg: isMultiPciePortEnabled=true → genMultiPCIeRequestMsg
 *   - encode succeeds → has_value (TRUE path)
 *   - encode fails → nullopt (FALSE path)
 * - handleResponseMsg: rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS → error=false
 *   → hexFormat returns hex string; switch on functionId 0..7 each case
 * - handleResponseMsg: rc!=NSM_SW_SUCCESS or cc!=NSM_SUCCESS → error=true
 *   → hexFormat returns ""
 * - handleResponseMsg: cc ? cc : rc; TRUE (cc non-zero) and FALSE (cc=0)
 */

#include "test/mockDBusHandler.hpp"
using namespace ::testing;

#include "libnsm/pci-links.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmChassis/nsmPCIeFunction.hpp"

using namespace nsm;

static auto& pcieBus = utils::DBusHandler::getBus();

// Helper: build a NsmInterfaceProvider<PCIeDeviceIntf>
static NsmInterfaceProvider<PCIeDeviceIntf>
    makePcieProvider(const std::string& path,
                     std::shared_ptr<PCIeDeviceIntf>& outIntf)
{
    outIntf = std::make_shared<PCIeDeviceIntf>(pcieBus, path.c_str());
    return NsmInterfaceProvider<PCIeDeviceIntf>(
        "pcie_func_br", "NSM_PCIeFunction", std::filesystem::path(path),
        outIntf);
}

// Helper: build a valid group-0 response
static std::vector<uint8_t> buildGroup0Response(uint8_t cc)
{
    nsm_query_scalar_group_telemetry_group_0 data = {};
    data.pci_vendor_id = 0x10DE;
    data.pci_device_id = 0x2330;
    data.pci_subsystem_vendor_id = 0x10DE;
    data.pci_subsystem_device_id = 0x16C1;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group0_resp(0, cc, ERR_NULL, &data,
                                                       msg);
    return buf;
}

// =============================================================================
// Constructor (single port): stores deviceIndex and functionId correctly
// =============================================================================

TEST(NsmPCIeFunctionBranch, SinglePortCtor_StoresFields)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/ctor_single", intf);
    NsmPCIeFunction sensor(provider, 3u, 5u);

    EXPECT_EQ(sensor.deviceIndex, 3u);
    EXPECT_EQ(sensor.functionId, 5u);
    EXPECT_FALSE(sensor.isMultiPciePortEnabled);
}

// =============================================================================
// Constructor (multi port): stores all multi-port fields correctly
// =============================================================================

TEST(NsmPCIeFunctionBranch, MultiPortCtor_StoresFields)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/ctor_multi", intf);
    NsmPCIeFunction sensor(provider, 2u, 7u, 4u, 1u);

    EXPECT_EQ(sensor.functionId, 2u);
    EXPECT_TRUE(sensor.isMultiPciePortEnabled);
    EXPECT_EQ(sensor.multiPortType, 7u);
    EXPECT_EQ(sensor.multiPortIndex, 4u);
    EXPECT_EQ(sensor.multiPortUpstreamPortNumber, 1u);
}

// =============================================================================
// genRequestMsg: single port, valid instanceId → encode succeeds → has_value
// (the if(rc) branch is FALSE → falls through to return request)
// =============================================================================

TEST(NsmPCIeFunctionBranch, GenRequestMsg_SinglePort_ValidId_HasValue)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/single_valid", intf);
    NsmPCIeFunction sensor(provider, 1u, 0u);

    auto req = sensor.genRequestMsg(5, 0);

    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->size(), sizeof(nsm_msg_hdr) +
                               sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

// =============================================================================
// genRequestMsg: single port, invalid instanceId → encode fails → nullopt
// (the if(rc) branch is TRUE → return std::nullopt)
// =============================================================================

TEST(NsmPCIeFunctionBranch, GenRequestMsg_SinglePort_InvalidId_Nullopt)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/single_invalid", intf);
    NsmPCIeFunction sensor(provider, 1u, 0u);

    // instanceId > NSM_INSTANCE_MAX → encode returns error → if(rc) TRUE
    auto req = sensor.genRequestMsg(5, NSM_INSTANCE_MAX + 1);

    EXPECT_FALSE(req.has_value());
}

// =============================================================================
// genRequestMsg: multi port, valid instanceId → encode succeeds → has_value
// =============================================================================

TEST(NsmPCIeFunctionBranch, GenRequestMsg_MultiPort_ValidId_HasValue)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/multi_valid", intf);
    NsmPCIeFunction sensor(provider, 0u, 1u, 2u, 0u);

    auto req = sensor.genRequestMsg(5, 0);

    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_multiport_query_scalar_group_telemetry_v2_req));
}

// =============================================================================
// genRequestMsg: multi port, invalid instanceId → encode fails → nullopt
// =============================================================================

TEST(NsmPCIeFunctionBranch, GenRequestMsg_MultiPort_InvalidId_Nullopt)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/multi_invalid", intf);
    NsmPCIeFunction sensor(provider, 0u, 1u, 2u, 0u);

    auto req = sensor.genRequestMsg(5, NSM_INSTANCE_MAX + 1);

    EXPECT_FALSE(req.has_value());
}

// =============================================================================
// handleResponseMsg: cc==NSM_SUCCESS, rc==NSM_SW_SUCCESS → error=false
// hexFormat returns hex string; functionId=0 → switch case 0
// =============================================================================

TEST(NsmPCIeFunctionBranch, HandleResponseMsg_Success_FuncId0_HexFormatUsed)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/success_func0", intf);
    NsmPCIeFunction sensor(provider, 1u, 0u);

    auto buf = buildGroup0Response(NSM_SUCCESS);
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    // hexFormat was called with values → function0VendorId set to "0x10DE"
    EXPECT_EQ(intf->function0VendorId(), "0x10DE");
    EXPECT_EQ(intf->function0DeviceId(), "0x2330");
}

// =============================================================================
// handleResponseMsg: functionId=1 → switch case 1
// =============================================================================

TEST(NsmPCIeFunctionBranch, HandleResponseMsg_Success_FuncId1)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/success_func1", intf);
    NsmPCIeFunction sensor(provider, 1u, 1u);

    auto buf = buildGroup0Response(NSM_SUCCESS);
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->function1VendorId(), "0x10DE");
}

// =============================================================================
// handleResponseMsg: functionId=7 → switch case 7
// =============================================================================

TEST(NsmPCIeFunctionBranch, HandleResponseMsg_Success_FuncId7)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/success_func7", intf);
    NsmPCIeFunction sensor(provider, 1u, 7u);

    auto buf = buildGroup0Response(NSM_SUCCESS);
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->function7VendorId(), "0x10DE");
}

// =============================================================================
// handleResponseMsg: cc == NSM_ERROR → error=true → hexFormat returns ""
// switch case 0 sets empty strings; cc ? cc : rc → returns cc (non-zero)
// =============================================================================

TEST(NsmPCIeFunctionBranch, HandleResponseMsg_ErrorCC_HexFormatEmpty_ReturnsCC)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/err_cc", intf);
    NsmPCIeFunction sensor(provider, 1u, 0u);

    auto buf = buildGroup0Response(NSM_ERROR);
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    // cc non-zero → return cc → result != NSM_SUCCESS
    EXPECT_NE(rc, NSM_SUCCESS);
    // hexFormat returned "" for all fields
    EXPECT_EQ(intf->function0VendorId(), "");
    EXPECT_EQ(intf->function0DeviceId(), "");
}

// =============================================================================
// handleResponseMsg: decode failure → rc != NSM_SW_SUCCESS → error=true
// hexFormat returns ""; cc stays NSM_SUCCESS=0 → cc ? cc : rc returns rc
// =============================================================================

TEST(NsmPCIeFunctionBranch,
     HandleResponseMsg_DecodeFail_HexFormatEmpty_ReturnsRC)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/decode_fail", intf);
    NsmPCIeFunction sensor(provider, 1u, 0u);

    auto buf = buildGroup0Response(NSM_SUCCESS);
    // Pass size=1 → decode returns error → rc != NSM_SW_SUCCESS → error=true
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), 1);

    EXPECT_NE(rc, NSM_SUCCESS);
    // hexFormat returned "" for all fields (error=true path)
    EXPECT_EQ(intf->function0VendorId(), "");
}

// =============================================================================
// handleResponseMsg: rc==NSM_SW_SUCCESS but cc==NSM_ERROR (9-byte buffer)
// → error=true (second operand cc!=NSM_SUCCESS) → hexFormat returns ""
// =============================================================================

TEST(NsmPCIeFunctionBranch, HandleResponseMsg_DecodeSuccessNonZeroCC_ErrorTrue)
{
    std::shared_ptr<PCIeDeviceIntf> intf;
    auto provider = makePcieProvider("/test/pcie_br/cc_err_9byte", intf);
    NsmPCIeFunction sensor(provider, 1u, 0u);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code = NSM_ERROR

    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_NE(rc, NSM_SUCCESS);
    // error=true → hexFormat returned "" → function0VendorId=""
    EXPECT_EQ(intf->function0VendorId(), "");
}
