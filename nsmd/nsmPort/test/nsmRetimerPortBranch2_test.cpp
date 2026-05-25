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
 * Additional branch coverage tests for nsmRetimerPort.cpp:
 * - NsmPCIeECCGroup1: all speed codes, width/size edge cases, clock modes
 * - NsmPCIeECCGroup1/2/3/4/5/7/8/9/10 retimer (deviceIndex) constructor
 * - NsmPCIeECCGroup9 handleResponseMsg success with hex values
 * - NsmPCIeECCGroup10 helper functions
 * - NsmPort constructor with associations
 * - NsmPCIeECCGroup7 PCIeDeviceIntf single function
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

static auto rt2Bus = sdbusplus::bus::new_default();

static const std::string rt2InvPath =
    "/xyz/openbmc_project/inventory/system/retimer/portBr2";

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
struct NsmRetimerPortBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:50";
    const uuid_t cx8Uuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:51";
    const uuid_t cx9Uuid = "STATIC:515:0:NSM_DEVICE_INSTANCE_NUMBER:52";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<MockNsmDevice> cx8Dev;
    std::shared_ptr<MockNsmDevice> cx9Dev;

    // For Group10
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmRetimerPortBranch2Test() :
        SensorManagerTest(devices),
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);

        cx8Dev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx8Uuid));
        EXPECT_NE(cx8Dev, nullptr);

        cx9Dev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx9Uuid));
        EXPECT_NE(cx9Dev, nullptr);

        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));
    }

    ~NsmRetimerPortBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ===========================================================================
// NsmPCIeECCGroup1 retimer (deviceIndex) ctor - genRequestMsg
// ===========================================================================

TEST_F(NsmRetimerPortBranch2Test, Group1_DeviceIndex_GenRequestMsg_Valid)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt2Bus,
                                                       rt2InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt2Bus,
                                                         rt2InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_ri", "T", rt2InvPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmRetimerPortBranch2Test, Group1_DeviceIndex_GenRequestMsg_Invalid)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt2Bus,
                                                       rt2InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt2Bus,
                                                         rt2InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_ri_bad", "T", rt2InvPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    auto request = group.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ===========================================================================
// NsmPCIeECCGroup1 handleResponseMsg success with all Gen speeds
// ===========================================================================

TEST_F(NsmRetimerPortBranch2Test,
       Group1_MultiPort_HandleResponseMsg_Success_AllGens)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt2Bus,
                                                       rt2InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt2Bus,
                                                         rt2InvPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(rt2Bus, rt2InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_gens", "T", rt2InvPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    data.max_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN1;
    data.negotiated_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN2;
    data.target_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN3;
    data.max_link_width = 1;
    data.negotiated_link_width = 2;
    data.max_read_request_size_bytes = 0;
    data.max_payload_size_bytes = 1;
    data.clock_mode = NSM_PCIE_CLOCK_MODE_COMMON;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->maxSpeed(), NSM_PCIE_SPEED_GBPS_GEN1);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), NSM_PCIE_SPEED_GBPS_GEN2);
    EXPECT_DOUBLE_EQ(portInfoIntf->targetSpeed(), NSM_PCIE_SPEED_GBPS_GEN3);
    EXPECT_EQ(portWidthIntf->width(), 1u);
    EXPECT_EQ(portWidthIntf->activeWidth(), 2u);
    EXPECT_EQ(pcieClockModeIntf->commonClockMode(), ClockMode::CommonClockMode);
}

TEST_F(NsmRetimerPortBranch2Test, Group1_MultiPort_HandleResponseMsg_Gen456)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt2Bus,
                                                       rt2InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt2Bus,
                                                         rt2InvPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(rt2Bus, rt2InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_gen456", "T", rt2InvPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    data.max_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN4;
    data.negotiated_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN5;
    data.target_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN6;
    data.max_link_width = 3;
    data.negotiated_link_width = 4;
    data.max_read_request_size_bytes = 2;
    data.max_payload_size_bytes = 3;
    data.clock_mode = NSM_PCIE_CLOCK_MODE_SEPARATE;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->maxSpeed(), NSM_PCIE_SPEED_GBPS_GEN4);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), NSM_PCIE_SPEED_GBPS_GEN5);
    EXPECT_DOUBLE_EQ(portInfoIntf->targetSpeed(), NSM_PCIE_SPEED_GBPS_GEN6);
    EXPECT_EQ(portWidthIntf->width(), 4u);
    EXPECT_EQ(portWidthIntf->activeWidth(), 8u);
    EXPECT_EQ(pcieClockModeIntf->commonClockMode(),
              ClockMode::SeparateClockMode);
}

TEST_F(NsmRetimerPortBranch2Test, Group1_MultiPort_HandleResponseMsg_ErrorCC)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt2Bus,
                                                       rt2InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt2Bus,
                                                         rt2InvPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(rt2Bus, rt2InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_err", "T", rt2InvPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

// ===========================================================================
// NsmPCIeECCGroup1 width/size edge cases
// ===========================================================================

TEST_F(NsmRetimerPortBranch2Test, Group1_ConvertWidth_EdgeCases)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt2Bus,
                                                       rt2InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt2Bus,
                                                         rt2InvPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(rt2Bus, rt2InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_width", "T", rt2InvPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.convertEncodedWidthToActualWidth(0), 0u);
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(5), 16u);
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(6), 32u);
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(7), 0u);
}

TEST_F(NsmRetimerPortBranch2Test, Group1_ConvertSize_EdgeCases)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt2Bus,
                                                       rt2InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt2Bus,
                                                         rt2InvPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(rt2Bus, rt2InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_size", "T", rt2InvPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.convertEncodedSizeToBytes(0), 128u);
    EXPECT_EQ(group.convertEncodedSizeToBytes(4), 2048u);
    EXPECT_EQ(group.convertEncodedSizeToBytes(5), 4096u);
    EXPECT_EQ(group.convertEncodedSizeToBytes(6), 0u);
}

// ===========================================================================
// NsmPCIeECCGroup2/3/4/5/7/8/9/10 retimer (deviceIndex) ctors
// ===========================================================================

TEST_F(NsmRetimerPortBranch2Test, Group2_DeviceIndex_Ctor)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt2Bus,
                                                     rt2InvPath.c_str());
    NsmPCIeECCGroup2 group("g2_ri", "T", rt2InvPath, pcieEccIntf, uint8_t(1));

    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmRetimerPortBranch2Test, Group3_DeviceIndex_Ctor)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt2Bus,
                                                     rt2InvPath.c_str());
    NsmPCIeECCGroup3 group("g3_ri", "T", rt2InvPath, pcieEccIntf, uint8_t(1));

    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmRetimerPortBranch2Test, Group4_DeviceIndex_Ctor)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt2Bus,
                                                     rt2InvPath.c_str());
    NsmPCIeECCGroup4 group("g4_ri", "T", rt2InvPath, pcieEccIntf, uint8_t(1));

    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmRetimerPortBranch2Test, Group5_DeviceIndex_Ctor)
{
    auto intf = std::make_shared<PortMetricsOem2Intf>(rt2Bus,
                                                      rt2InvPath.c_str());
    NsmPCIeECCGroup5 group("g5_ri", "T", rt2InvPath, intf, uint8_t(1));

    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmRetimerPortBranch2Test, Group7_DeviceIndex_Ctor)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt2Bus,
                                                       rt2InvPath.c_str());
    NsmPCIeECCGroup7 group("g7_ri", "T", rt2InvPath, portInfoIntf, uint8_t(1));

    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmRetimerPortBranch2Test, Group8_DeviceIndex_Ctor)
{
    auto laneErrorIntf = std::make_shared<LaneErrorIntf>(rt2Bus,
                                                         rt2InvPath.c_str());
    NsmPCIeECCGroup8 group("g8_ri", "T", laneErrorIntf, uint8_t(1), rt2InvPath);

    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmRetimerPortBranch2Test, Group9_DeviceIndex_Ctor)
{
    auto aerIntf = std::make_shared<AERErrorStatusIntf>(rt2Bus,
                                                        rt2InvPath.c_str());
    NsmPCIeECCGroup9 group("g9_ri", "T", rt2InvPath, aerIntf, uint8_t(1));

    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ===========================================================================
// NsmPCIeECCGroup9 handleResponseMsg success
// ===========================================================================

TEST_F(NsmRetimerPortBranch2Test, Group9_HandleResponseMsg_Success)
{
    auto aerIntf = std::make_shared<AERErrorStatusIntf>(rt2Bus,
                                                        rt2InvPath.c_str());
    NsmPCIeECCGroup9 group("g9_ok2", "T", rt2InvPath, aerIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_9 data = {};
    data.aer_uncorrectable_error_status = 0xDEADBEEF;
    data.aer_correctable_error_status = 0xCAFEBABE;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group9_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);

    auto uncStr = aerIntf->aerUncorrectableErrorStatus();
    EXPECT_NE(uncStr.find("DEADBEEF"), std::string::npos);
    auto corrStr = aerIntf->aerCorrectableErrorStatus();
    EXPECT_NE(corrStr.find("CAFEBABE"), std::string::npos);
}

// ===========================================================================
// NsmPCIeECCGroup10 retimer (deviceIndex) ctors
// ===========================================================================

TEST_F(NsmRetimerPortBranch2Test, Group10_DeviceIndex_NoInbound_Ctor)
{
    std::string g10Path = rt2InvPath + "/g10_ri";
    NsmPCIeECCGroup10 group("g10_ri", "T", g10Path, uint8_t(1), false);

    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmRetimerPortBranch2Test, Group10_DeviceIndex_WithInbound_Ctor)
{
    std::string g10Path = rt2InvPath + "/g10_ri_in";
    NsmPCIeECCGroup10 group("g10_ri_in", "T", g10Path, uint8_t(1), true);

    auto request = group.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ===========================================================================
// NsmPort constructor with associations
// ===========================================================================

TEST_F(NsmRetimerPortBranch2Test, NsmPort_Constructor)
{
    std::string portName = "RT_Port_B2";
    std::string type = "T";
    std::string objPath = rt2InvPath + "/port_ctor";

    std::vector<utils::Association> associations;
    associations.push_back({"fwd", "bck", "/xyz/openbmc_project/some/path"});

    NsmPort port(rt2Bus, portName, type, associations, objPath);
    EXPECT_EQ(port.portName, portName);
}

// ===========================================================================
// NsmPCIeECCGroup7 with PCIeDeviceIntf: single function
// ===========================================================================

TEST_F(NsmRetimerPortBranch2Test, Group7_PCIeDevice_SingleFunction)
{
    std::string devPath = rt2InvPath + "/pciedev_g7_single";
    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(rt2Bus,
                                                           devPath.c_str());
    std::vector<uint64_t> funcIds = {3};

    NsmPCIeECCGroup7 group("g7_single", "T", pcieDeviceIntf, funcIds,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.pcie_bus_number = 99;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieDeviceIntf->function3BusNumber(), 99u);
}

// ===========================================================================
// NsmPCIeECCGroup10: join64BitCounterLowHigh and convertDwordsSizeToBytes
// ===========================================================================

TEST_F(NsmRetimerPortBranch2Test, Group10_Join64BitCounter)
{
    std::string g10Path = rt2InvPath + "/g10_join";
    NsmPCIeECCGroup10 group("g10_join", "T", g10Path, NSM_PORT_TYPE_UPSTREAM,
                            uint8_t(0), uint8_t(1), false);

    EXPECT_EQ(group.join64BitCounterLowHigh(0x1, 0x2),
              (uint64_t(0x1) << 32) | uint64_t(0x2));
    EXPECT_EQ(group.join64BitCounterLowHigh(0, 0), 0u);
}

TEST_F(NsmRetimerPortBranch2Test, Group10_ConvertDwordsToBytes)
{
    std::string g10Path = rt2InvPath + "/g10_dw";
    NsmPCIeECCGroup10 group("g10_dw", "T", g10Path, NSM_PORT_TYPE_UPSTREAM,
                            uint8_t(0), uint8_t(1), false);

    EXPECT_EQ(group.convertDwordsSizeToBytes(10), 10u * BYTES_PER_DWORD);
    EXPECT_EQ(group.convertDwordsSizeToBytes(0), 0u);
}
