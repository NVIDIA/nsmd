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

/*
 * Branch coverage tests for nsmRetimerPort.cpp (batch 4):
 *
 * - NsmPCIeECCGroup5::handleResponseMsg success (txBytes/rxBytes calculation)
 * - NsmPCIeECCGroup5::handleResponseMsg failure (decode error)
 * - NsmPCIeECCGroup7::handleResponseMsg success with portInfoIntf:
 *   all NSM_PCIE_PORT_TYPE values: ROOT_PORT, UPSTREAM, DOWNSTREAM,
 *   ENDPOINT, and default (unknown)
 * - NsmPCIeECCGroup7::handleResponseMsg success with pcieDeviceIntf:
 *   functionIds 0..7 (each switch case)
 * - NsmPCIeECCGroup7::handleResponseMsg: only pcieDeviceIntf
 * (portInfoIntf==nullptr)
 * - NsmPCIeECCGroup7::handleResponseMsg failure (decode error)
 * - NsmPCIeECCGroup8::handleResponseMsg success (lane error counts)
 * - NsmPCIeECCGroup8::handleResponseMsg failure (decode error)
 * - NsmPCIeECCGroup5 multi-port constructor
 * - NsmPCIeECCGroup7 multi-port constructor (portInfoIntf and pcieDeviceIntf)
 * - NsmPCIeECCGroup8 multi-port constructor
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "pci-links.h"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmRetimerPort.hpp"
#include "test/commonMock.hpp"

using namespace nsm;

static auto rt4Bus = sdbusplus::bus::new_default();
static const std::string rt4InvPath = "/xyz/openbmc_project/inventory/rt4";

// ============================================================================
// Fixture
// ============================================================================

struct NsmRetimerPortBranch4Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmRetimerPortBranch4Test() : SensorManagerTest(devices) {}

    ~NsmRetimerPortBranch4Test()
    {
        cleanupDeviceSensors(devices);
    }

    // Build a minimal valid Group5 response buffer
    std::vector<uint8_t> buildGroup5Resp(uint8_t cc, uint32_t txDwords,
                                         uint32_t rxDwords)
    {
        struct nsm_query_scalar_group_telemetry_group_5 data = {};
        data.PCIeTXDwords = txDwords;
        data.PCIeRXDwords = rxDwords;
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp));
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_query_scalar_group_telemetry_v1_group5_resp(0, cc, ERR_NULL,
                                                           &data, msg);
        return buf;
    }

    // Build a minimal valid Group7 response buffer
    std::vector<uint8_t> buildGroup7Resp(uint8_t cc, uint32_t portType,
                                         uint32_t busNumber)
    {
        struct nsm_query_scalar_group_telemetry_group_7 data = {};
        data.port_type = portType;
        data.pcie_bus_number = busNumber;
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_query_scalar_group_telemetry_v1_group7_resp(0, cc, ERR_NULL,
                                                           &data, msg);
        return buf;
    }

    // Build a minimal valid Group8 response buffer
    std::vector<uint8_t> buildGroup8Resp(uint8_t cc)
    {
        struct nsm_query_scalar_group_telemetry_group_8 data = {};
        for (int i = 0; i < TOTAL_PCIE_LANE_COUNT; i++)
        {
            data.error_counts[i] = static_cast<uint32_t>(i * 10);
        }
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_8_resp));
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_query_scalar_group_telemetry_v1_group8_resp(0, cc, ERR_NULL,
                                                           &data, msg);
        return buf;
    }

    // Build a too-small buffer to cause decode failure.
    // Must be large enough for decode_reason_code_and_cc to safely read
    // the completion_code field (payload[1]) without going out of bounds,
    // but smaller than any real group response struct so length checks fail.
    std::vector<uint8_t> decodeFail()
    {
        return std::vector<uint8_t>(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    }
};

// ============================================================================
// NsmPCIeECCGroup5::handleResponseMsg SUCCESS
// Verifies txBytes = txDwords * 4 and rxBytes = rxDwords * 4
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group5_HandleResponseMsg_Success)
{
    auto intf = std::make_shared<PortMetricsOem2Intf>(rt4Bus,
                                                      rt4InvPath.c_str());
    NsmPCIeECCGroup5 group("g5_ok", "T", rt4InvPath, intf, uint8_t(1));

    uint32_t txDw = 1000, rxDw = 2000;
    auto buf = buildGroup5Resp(NSM_SUCCESS, txDw, rxDw);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    // txBytes = txDwords * BYTES_PER_DWORD (4)
    EXPECT_EQ(intf->txBytes(), static_cast<uint64_t>(txDw) * 4);
    EXPECT_EQ(intf->rxBytes(), static_cast<uint64_t>(rxDw) * 4);
}

// ============================================================================
// NsmPCIeECCGroup5::handleResponseMsg DECODE FAILURE
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group5_HandleResponseMsg_DecodeFail)
{
    auto intf = std::make_shared<PortMetricsOem2Intf>(rt4Bus,
                                                      rt4InvPath.c_str());
    NsmPCIeECCGroup5 group("g5_fail", "T", rt4InvPath, intf, uint8_t(1));

    auto buf = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup5 multi-port constructor
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group5_MultiPort_Ctor)
{
    auto intf = std::make_shared<PortMetricsOem2Intf>(rt4Bus,
                                                      rt4InvPath.c_str());
    NsmPCIeECCGroup5 group("g5_mp", "T", rt4InvPath, intf, uint8_t(1),
                           uint8_t(0), uint8_t(0));
    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmPCIeECCGroup7::handleResponseMsg SUCCESS with portInfoIntf
// Tests all NSM_PCIE_PORT_TYPE values for the portType switch statement
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test,
       Group7_HandleResponseMsg_Success_PortType_RootPort)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt4Bus,
                                                       rt4InvPath.c_str());
    NsmPCIeECCGroup7 group("g7_root", "T", rt4InvPath, portInfoIntf,
                           uint8_t(1));

    auto buf = buildGroup7Resp(NSM_SUCCESS, NSM_PCIE_PORT_TYPE_ROOT_PORT, 5);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::RootPort);
}

TEST_F(NsmRetimerPortBranch4Test,
       Group7_HandleResponseMsg_Success_PortType_Upstream)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt4Bus,
                                                       rt4InvPath.c_str());
    NsmPCIeECCGroup7 group("g7_up", "T", rt4InvPath, portInfoIntf, uint8_t(1));

    auto buf = buildGroup7Resp(NSM_SUCCESS, NSM_PCIE_PORT_TYPE_UPSTREAM, 3);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::UpstreamPort);
}

TEST_F(NsmRetimerPortBranch4Test,
       Group7_HandleResponseMsg_Success_PortType_Downstream)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt4Bus,
                                                       rt4InvPath.c_str());
    NsmPCIeECCGroup7 group("g7_dn", "T", rt4InvPath, portInfoIntf, uint8_t(1));

    auto buf = buildGroup7Resp(NSM_SUCCESS, NSM_PCIE_PORT_TYPE_DOWNSTREAM, 2);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::DownstreamPort);
}

TEST_F(NsmRetimerPortBranch4Test,
       Group7_HandleResponseMsg_Success_PortType_Endpoint)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt4Bus,
                                                       rt4InvPath.c_str());
    NsmPCIeECCGroup7 group("g7_ep", "T", rt4InvPath, portInfoIntf, uint8_t(1));

    auto buf = buildGroup7Resp(NSM_SUCCESS, NSM_PCIE_PORT_TYPE_ENDPOINT, 1);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::EndpointPort);
}

TEST_F(NsmRetimerPortBranch4Test,
       Group7_HandleResponseMsg_Success_PortType_Default)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt4Bus,
                                                       rt4InvPath.c_str());
    NsmPCIeECCGroup7 group("g7_def", "T", rt4InvPath, portInfoIntf, uint8_t(1));

    // Use an unknown port type to hit the default branch (sets DownstreamPort)
    auto buf = buildGroup7Resp(NSM_SUCCESS, 0xFF, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::DownstreamPort);
}

// ============================================================================
// NsmPCIeECCGroup7::handleResponseMsg SUCCESS with pcieDeviceIntf
// Tests all functionId cases 0..7 in the inner switch
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test,
       Group7_HandleResponseMsg_Success_PCIeDevice_AllFunctions)
{
    // Use the PCIeDeviceIntf constructor with functionIds 0..7
    auto pciPath = rt4InvPath + "/pcie_dev";
    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(rt4Bus,
                                                           pciPath.c_str());

    std::vector<uint64_t> funcIds = {0, 1, 2, 3, 4, 5, 6, 7};
    NsmPCIeECCGroup7 group("g7_pcie_all", "T", pcieDeviceIntf, funcIds,
                           uint8_t(1), uint8_t(0), uint8_t(0));

    uint32_t busNum = 42;
    auto buf = buildGroup7Resp(NSM_SUCCESS, NSM_PCIE_PORT_TYPE_UPSTREAM,
                               busNum);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    // All functions should have busNumber set to 42
    EXPECT_EQ(pcieDeviceIntf->function0BusNumber(), busNum);
    EXPECT_EQ(pcieDeviceIntf->function1BusNumber(), busNum);
    EXPECT_EQ(pcieDeviceIntf->function2BusNumber(), busNum);
    EXPECT_EQ(pcieDeviceIntf->function3BusNumber(), busNum);
    EXPECT_EQ(pcieDeviceIntf->function4BusNumber(), busNum);
    EXPECT_EQ(pcieDeviceIntf->function5BusNumber(), busNum);
    EXPECT_EQ(pcieDeviceIntf->function6BusNumber(), busNum);
    EXPECT_EQ(pcieDeviceIntf->function7BusNumber(), busNum);
}

TEST_F(NsmRetimerPortBranch4Test,
       Group7_HandleResponseMsg_Success_PCIeDevice_FuncId0Only)
{
    auto pciPath = rt4InvPath + "/pcie_dev0";
    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(rt4Bus,
                                                           pciPath.c_str());

    // Only function 0 - covers case 0 only
    std::vector<uint64_t> funcIds = {0};
    NsmPCIeECCGroup7 group("g7_pcie_f0", "T", pcieDeviceIntf, funcIds,
                           uint8_t(1), uint8_t(0), uint8_t(0));

    auto buf = buildGroup7Resp(NSM_SUCCESS, NSM_PCIE_PORT_TYPE_DOWNSTREAM, 10);
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(pcieDeviceIntf->function0BusNumber(), 10u);
}

TEST_F(NsmRetimerPortBranch4Test,
       Group7_HandleResponseMsg_Success_PCIeDevice_FuncIds135)
{
    auto pciPath = rt4InvPath + "/pcie_dev135";
    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(rt4Bus,
                                                           pciPath.c_str());

    // Functions 1, 3, 5
    std::vector<uint64_t> funcIds = {1, 3, 5};
    NsmPCIeECCGroup7 group("g7_pcie_135", "T", pcieDeviceIntf, funcIds,
                           uint8_t(1), uint8_t(0), uint8_t(0));

    auto buf = buildGroup7Resp(NSM_SUCCESS, NSM_PCIE_PORT_TYPE_ROOT_PORT, 20);
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(pcieDeviceIntf->function1BusNumber(), 20u);
    EXPECT_EQ(pcieDeviceIntf->function3BusNumber(), 20u);
    EXPECT_EQ(pcieDeviceIntf->function5BusNumber(), 20u);
}

TEST_F(NsmRetimerPortBranch4Test,
       Group7_HandleResponseMsg_Success_PCIeDevice_FuncIds246)
{
    auto pciPath = rt4InvPath + "/pcie_dev246";
    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(rt4Bus,
                                                           pciPath.c_str());

    // Functions 2, 4, 6
    std::vector<uint64_t> funcIds = {2, 4, 6};
    NsmPCIeECCGroup7 group("g7_pcie_246", "T", pcieDeviceIntf, funcIds,
                           uint8_t(1), uint8_t(0), uint8_t(0));

    auto buf = buildGroup7Resp(NSM_SUCCESS, NSM_PCIE_PORT_TYPE_ENDPOINT, 30);
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(pcieDeviceIntf->function2BusNumber(), 30u);
    EXPECT_EQ(pcieDeviceIntf->function4BusNumber(), 30u);
    EXPECT_EQ(pcieDeviceIntf->function6BusNumber(), 30u);
}

// ============================================================================
// NsmPCIeECCGroup7::handleResponseMsg DECODE FAILURE (portInfoIntf path)
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group7_HandleResponseMsg_DecodeFail_PortInfo)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt4Bus,
                                                       rt4InvPath.c_str());
    NsmPCIeECCGroup7 group("g7_fail_pi", "T", rt4InvPath, portInfoIntf,
                           uint8_t(1));

    auto buf = decodeFail();
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup7 multi-port constructors
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group7_MultiPort_PortInfoIntf_Ctor)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt4Bus,
                                                       rt4InvPath.c_str());
    NsmPCIeECCGroup7 group("g7_mp", "T", rt4InvPath, portInfoIntf, uint8_t(1),
                           uint8_t(0), uint8_t(0));
    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmPCIeECCGroup8::handleResponseMsg SUCCESS
// Verifies lane error counts are set
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group8_HandleResponseMsg_Success)
{
    auto laneErrorIntf = std::make_shared<LaneErrorIntf>(rt4Bus,
                                                         rt4InvPath.c_str());
    NsmPCIeECCGroup8 group("g8_ok", "T", laneErrorIntf, uint8_t(1), rt4InvPath);

    auto buf = buildGroup8Resp(NSM_SUCCESS);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    auto counts = laneErrorIntf->rxErrorsPerLane();
    ASSERT_EQ(static_cast<int>(counts.size()), TOTAL_PCIE_LANE_COUNT);
    // First lane: 0 * 10 = 0, second lane: 1 * 10 = 10, etc.
    EXPECT_EQ(counts[0], 0u);
    EXPECT_EQ(counts[1], 10u);
}

// ============================================================================
// NsmPCIeECCGroup8::handleResponseMsg DECODE FAILURE
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group8_HandleResponseMsg_DecodeFail)
{
    auto laneErrorIntf = std::make_shared<LaneErrorIntf>(rt4Bus,
                                                         rt4InvPath.c_str());
    NsmPCIeECCGroup8 group("g8_fail", "T", laneErrorIntf, uint8_t(1),
                           rt4InvPath);

    auto buf = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup8 multi-port constructor
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group8_MultiPort_Ctor)
{
    auto laneErrorIntf = std::make_shared<LaneErrorIntf>(rt4Bus,
                                                         rt4InvPath.c_str());
    NsmPCIeECCGroup8 group("g8_mp", "T", laneErrorIntf, rt4InvPath, uint8_t(1),
                           uint8_t(0), uint8_t(0));
    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmPCIeECCGroup8::handleResponseMsg SUCCESS with error CC
// → returns cc (non-zero) without setting lane counts
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group8_HandleResponseMsg_ErrorCC)
{
    auto laneErrorIntf = std::make_shared<LaneErrorIntf>(rt4Bus,
                                                         rt4InvPath.c_str());
    NsmPCIeECCGroup8 group("g8_errcc", "T", laneErrorIntf, uint8_t(1),
                           rt4InvPath);

    auto buf = buildGroup8Resp(NSM_ERROR);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup5 SUCCESS with error CC → cc ? cc : rc returns cc
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group5_HandleResponseMsg_ErrorCC)
{
    auto intf = std::make_shared<PortMetricsOem2Intf>(rt4Bus,
                                                      rt4InvPath.c_str());
    NsmPCIeECCGroup5 group("g5_errcc", "T", rt4InvPath, intf, uint8_t(1));

    auto buf = buildGroup5Resp(NSM_ERROR, 500, 300);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup7::handleResponseMsg: portInfoIntf NULL, pcieDeviceIntf NULL
// (default constructor path) → neither branch executes
// ============================================================================

TEST_F(NsmRetimerPortBranch4Test, Group7_HandleResponseMsg_BothNull)
{
    // portInfoIntf=nullptr, pcieDeviceIntf stays nullptr (member default):
    // neither branch executes inside handleResponseMsg
    NsmPCIeECCGroup7 group("g7_both_null", "T", rt4InvPath,
                           std::shared_ptr<PortInfoIntf>(nullptr), uint8_t(1));

    auto buf = buildGroup7Resp(NSM_SUCCESS, NSM_PCIE_PORT_TYPE_DOWNSTREAM, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}
