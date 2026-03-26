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
 * - NsmPCIeECCGroup1::convertEncodedSpeedToGbps: unknown speed code (default)
 * - NsmPCIeECCGroup1::convertEncodedWidthToActualWidth: out-of-range (0 and 7)
 * - NsmPCIeECCGroup1::convertEncodedSizeToBytes: out-of-range (>5)
 * - NsmPCIeECCGroup1::convertEncodedClockModeToEnum: unknown (default)
 * - NsmPCIeECCGroup1: handleResponseMsg decode failure
 * - NsmPCIeECCGroup2: handleResponseMsg decode failure
 * - NsmPCIeECCGroup3: handleResponseMsg decode failure
 * - NsmPCIeECCGroup4: handleResponseMsg decode failure
 * - NsmPort constructor: verify associations
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

static auto rt3Bus = sdbusplus::bus::new_default();

static const std::string rt3InvPath =
    "/xyz/openbmc_project/inventory/system/retimer/portBr3";

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
struct NsmRetimerPortBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:90";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmRetimerPortBranch3Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmRetimerPortBranch3Test()
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
// NsmPCIeECCGroup1::convertEncodedSpeedToGbps - unknown speed code (default)
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group1_ConvertSpeed_Unknown_ReturnsZero)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_spd_unk", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    // Speed code 0xFF is not a valid gen code
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(0xFF), 0.0);
    // Speed code 0 is also unknown
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(0), 0.0);
    // Speed code 7 (beyond Gen6=6) is unknown
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(7), 0.0);
}

// ===========================================================================
// NsmPCIeECCGroup1::convertEncodedWidthToActualWidth - edge cases
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group1_ConvertWidth_OutOfRange_ReturnsZero)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_w_oor", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    // width=0 -> 0
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(0), 0u);
    // width=7 -> 0 (> 6)
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(7), 0u);
    // width=1 -> 1 (2^0)
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(1), 1u);
    // width=6 -> 32 (2^5)
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(6), 32u);
}

// ===========================================================================
// NsmPCIeECCGroup1::convertEncodedSizeToBytes - edge cases
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group1_ConvertSize_OutOfRange_ReturnsZero)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_sz_oor", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    // size=6 -> 0 (> 5)
    EXPECT_EQ(group.convertEncodedSizeToBytes(6), 0u);
    // size=0 -> 128
    EXPECT_EQ(group.convertEncodedSizeToBytes(0), 128u);
    // size=5 -> 4096
    EXPECT_EQ(group.convertEncodedSizeToBytes(5), 4096u);
}

// ===========================================================================
// NsmPCIeECCGroup1::convertEncodedClockModeToEnum - unknown (default)
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group1_ConvertClockMode_Unknown)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(rt3Bus, rt3InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_clk_unk", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    // clock mode 0xFF is unknown
    EXPECT_EQ(group.convertEncodedClockModeToEnum(0xFF), ClockMode::Unknown);
    // clock mode 2 is unknown (only 0=Separate, 1=Common are valid)
    EXPECT_EQ(group.convertEncodedClockModeToEnum(2), ClockMode::Unknown);
}

// ===========================================================================
// NsmPCIeECCGroup1::convertEncodedClockModeToEnum - Separate mode
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group1_ConvertClockMode_Separate)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(rt3Bus, rt3InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_clk_sep", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.convertEncodedClockModeToEnum(NSM_PCIE_CLOCK_MODE_SEPARATE),
              ClockMode::SeparateClockMode);
}

// ===========================================================================
// NsmPCIeECCGroup1: handleResponseMsg decode failure (valgrind-safe)
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group1_HandleResponseMsg_DecodeFail)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(rt3Bus, rt3InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_decfail", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    auto buf = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ===========================================================================
// NsmPCIeECCGroup1: handleResponseMsg with data where speed/width/size
// trigger edge-case branches in helpers
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test,
       Group1_HandleResponseMsg_EdgeCaseSpeedWidthSize)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(rt3Bus, rt3InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_edge", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    data.max_link_speed = 0xFF;           // unknown -> 0
    data.negotiated_link_speed = 0;       // unknown -> 0
    data.target_link_speed = 7;           // unknown -> 0
    data.max_link_width = 0;              // -> 0
    data.negotiated_link_width = 7;       // -> 0 (> 6)
    data.max_read_request_size_bytes = 6; // -> 0 (> 5)
    data.max_payload_size_bytes = 6;      // -> 0 (> 5)
    data.clock_mode = 99;                 // unknown -> ClockMode::Unknown

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->maxSpeed(), 0.0);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), 0.0);
    EXPECT_DOUBLE_EQ(portInfoIntf->targetSpeed(), 0.0);
    EXPECT_EQ(portWidthIntf->width(), 0u);
    EXPECT_EQ(portWidthIntf->activeWidth(), 0u);
    EXPECT_EQ(portInfoIntf->maxReadRequestSizeBytes(), 0u);
    EXPECT_EQ(portInfoIntf->maxPayloadSizeBytes(), 0u);
    EXPECT_EQ(pcieClockModeIntf->commonClockMode(), ClockMode::Unknown);
}

// ===========================================================================
// NsmPCIeECCGroup2: handleResponseMsg decode failure
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group2_HandleResponseMsg_DecodeFail)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt3Bus,
                                                     rt3InvPath.c_str());

    NsmPCIeECCGroup2 group("g2_decfail", "T", rt3InvPath, pcieEccIntf,
                           uint8_t(1));

    auto buf = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ===========================================================================
// NsmPCIeECCGroup3: handleResponseMsg decode failure
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group3_HandleResponseMsg_DecodeFail)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt3Bus,
                                                     rt3InvPath.c_str());

    NsmPCIeECCGroup3 group("g3_decfail", "T", rt3InvPath, pcieEccIntf,
                           uint8_t(1));

    auto buf = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ===========================================================================
// NsmPCIeECCGroup4: handleResponseMsg decode failure
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group4_HandleResponseMsg_DecodeFail)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt3Bus,
                                                     rt3InvPath.c_str());

    NsmPCIeECCGroup4 group("g4_decfail", "T", rt3InvPath, pcieEccIntf,
                           uint8_t(1));

    auto buf = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ===========================================================================
// NsmPCIeECCGroup2: multiPort constructor + handleResponseMsg success
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group2_MultiPort_HandleResponseMsg_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt3Bus,
                                                     rt3InvPath.c_str());

    NsmPCIeECCGroup2 group("g2_mp_ok", "T", rt3InvPath, pcieEccIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_2 data = {};
    data.non_fatal_errors = 10;
    data.fatal_errors = 20;
    data.correctable_errors = 30;
    data.unsupported_request_count = 40;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->nonfeCount(), 10u);
    EXPECT_EQ(pcieEccIntf->feCount(), 20u);
    EXPECT_EQ(pcieEccIntf->ceCount(), 30u);
    EXPECT_EQ(pcieEccIntf->unsupportedRequestCount(), 40u);
}

// ===========================================================================
// NsmPCIeECCGroup3: multiPort constructor + handleResponseMsg success
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group3_MultiPort_HandleResponseMsg_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt3Bus,
                                                     rt3InvPath.c_str());

    NsmPCIeECCGroup3 group("g3_mp_ok", "T", rt3InvPath, pcieEccIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_3 data = {};
    data.L0ToRecoveryCount = 100;
    data.training_seq_errors = 200;
    data.framing_errors = 300;
    data.link_down_count = 400;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 100u);
    EXPECT_EQ(pcieEccIntf->trainingSequenceErrorCount(), 200u);
    EXPECT_EQ(pcieEccIntf->framingErrorCount(), 300u);
    EXPECT_EQ(pcieEccIntf->linkDownedCount(), 400u);
}

// ===========================================================================
// NsmPort constructor: verify associations are properly set
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, NsmPort_Constructor_VerifiesAssociations)
{
    std::string portName = "TestPort";
    std::string type = "PortType";
    std::string invObj = rt3InvPath + "/testport";
    std::vector<utils::Association> assocs;
    assocs.push_back({"forward1", "backward1", "/path1"});
    assocs.push_back({"forward2", "backward2", "/path2"});

    NsmPort port(rt3Bus, portName, type, assocs, invObj);

    EXPECT_EQ(port.portName, portName);
    auto assocList = port.associationDefIntf->associations();
    EXPECT_EQ(assocList.size(), 2u);
}

// ===========================================================================
// NsmRetimerAERErrorStatusIntf::clearAERStatus - returns empty path
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, AERErrorStatusIntf_ClearReturnsEmptyPath)
{
    NsmRetimerAERErrorStatusIntf aerIntf(rt3Bus, rt3InvPath.c_str());
    auto result = aerIntf.clearAERStatus();
    EXPECT_EQ(result, sdbusplus::message::object_path());
}
