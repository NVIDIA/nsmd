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
// NsmPCIeECCGroup1 handleResponseMsg success via deviceIndex ctor
// (pcieClockModeIntf is null -> skips clock mode branch)
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test,
       Group1_DeviceIndex_HandleResponseMsg_NullClockMode)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());

    // deviceIndex ctor: pcieClockModeIntf remains nullptr
    NsmPCIeECCGroup1 group("g1_nullclk", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    data.max_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN5;
    data.negotiated_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN4;
    data.target_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN3;
    data.max_link_width = 4;
    data.negotiated_link_width = 3;
    data.clock_mode = NSM_PCIE_CLOCK_MODE_COMMON; // should be skipped

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->maxSpeed(), NSM_PCIE_SPEED_GBPS_GEN5);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), NSM_PCIE_SPEED_GBPS_GEN4);
    EXPECT_DOUBLE_EQ(portInfoIntf->targetSpeed(), NSM_PCIE_SPEED_GBPS_GEN3);
    EXPECT_EQ(portWidthIntf->width(), 8u);       // width=4 -> 2^3 = 8
    EXPECT_EQ(portWidthIntf->activeWidth(), 4u); // width=3 -> 2^2 = 4
    // pcieClockModeIntf was null -> no crash, no clock mode set
}

// ===========================================================================
// NsmPCIeECCGroup1 convertEncodedWidthToActualWidth: valid widths 2-5
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group1_ConvertWidth_ValidRange)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_w_valid", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    // width=2 -> 2^1 = 2
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(2), 2u);
    // width=3 -> 2^2 = 4
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(3), 4u);
    // width=4 -> 2^3 = 8
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(4), 8u);
    // width=5 -> 2^4 = 16
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(5), 16u);
}

// ===========================================================================
// NsmPCIeECCGroup1 convertEncodedSizeToBytes: valid sizes 1-4
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group1_ConvertSize_ValidRange)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_sz_valid", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    // size=1 -> 256
    EXPECT_EQ(group.convertEncodedSizeToBytes(1), 256u);
    // size=2 -> 512
    EXPECT_EQ(group.convertEncodedSizeToBytes(2), 512u);
    // size=3 -> 1024
    EXPECT_EQ(group.convertEncodedSizeToBytes(3), 1024u);
    // size=4 -> 2048
    EXPECT_EQ(group.convertEncodedSizeToBytes(4), 2048u);
}

// ===========================================================================
// NsmPCIeECCGroup1 convertEncodedClockModeToEnum: Common mode
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group1_ConvertClockMode_Common)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         rt3InvPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(rt3Bus, rt3InvPath.c_str());

    NsmPCIeECCGroup1 group("g1_clk_com", "T", rt3InvPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.convertEncodedClockModeToEnum(NSM_PCIE_CLOCK_MODE_COMMON),
              ClockMode::CommonClockMode);
}

// ===========================================================================
// NsmPCIeECCGroup4 handleResponseMsg success via deviceIndex ctor
// (no dllpcrcErrorCount initialization in deviceIndex ctor)
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group4_DeviceIndex_HandleResponseMsg_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt3Bus,
                                                     rt3InvPath.c_str());

    NsmPCIeECCGroup4 group("g4_ri_ok", "T", rt3InvPath, pcieEccIntf,
                           uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_4 data = {};
    data.replay_cnt = 11;
    data.replay_rollover_cnt = 22;
    data.NAK_sent_cnt = 33;
    data.NAK_recv_cnt = 44;
    data.FC_timeout_err_cnt = 55;
    data.bad_TLP_cnt = 66;
    data.recv_err_cnt = 77;
    data.dllp_crc_errors = 88;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->replayCount(), 11u);
    EXPECT_EQ(pcieEccIntf->replayRolloverCount(), 22u);
    EXPECT_EQ(pcieEccIntf->nakSentCount(), 33u);
    EXPECT_EQ(pcieEccIntf->nakReceivedCount(), 44u);
    EXPECT_EQ(pcieEccIntf->fcTimeoutErrors(), 55u);
    EXPECT_EQ(pcieEccIntf->badTLPCount(), 66u);
    EXPECT_EQ(pcieEccIntf->receiverErrorCount(), 77u);
    EXPECT_EQ(pcieEccIntf->dllpcrcErrorCount(), 88u);
}

// ===========================================================================
// NsmPCIeECCGroup5 handleResponseMsg success via deviceIndex ctor
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group5_DeviceIndex_HandleResponseMsg_Success)
{
    auto intf = std::make_shared<PortMetricsOem2Intf>(rt3Bus,
                                                      rt3InvPath.c_str());

    NsmPCIeECCGroup5 group("g5_ri_ok", "T", rt3InvPath, intf, uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_5 data = {};
    data.PCIeTXDwords = 500;
    data.PCIeRXDwords = 600;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group5_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(intf->txBytes(), uint64_t(500) * BYTES_PER_DWORD);
    EXPECT_EQ(intf->rxBytes(), uint64_t(600) * BYTES_PER_DWORD);
}

// ===========================================================================
// NsmPCIeECCGroup7 handleResponseMsg success via deviceIndex ctor
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group7_DeviceIndex_HandleResponseMsg_Success)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus,
                                                       rt3InvPath.c_str());

    NsmPCIeECCGroup7 group("g7_ri_ok", "T", rt3InvPath, portInfoIntf,
                           uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.port_type = NSM_PCIE_PORT_TYPE_UPSTREAM;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group7_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::UpstreamPort);
}

// ===========================================================================
// NsmPCIeECCGroup7 PCIeDeviceIntf: funcId out of range (>7 default case)
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group7_PCIeDevice_FuncIdOutOfRange)
{
    std::string devPath = rt3InvPath + "/pciedev_g7_oor";
    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(rt3Bus,
                                                           devPath.c_str());
    // funcId 8 and 255 are out of the switch range (0-7)
    std::vector<uint64_t> funcIds = {8, 255};

    NsmPCIeECCGroup7 group("g7_oor", "T", pcieDeviceIntf, funcIds,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.pcie_bus_number = 50;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group7_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    // Bus numbers should remain at 0 (unchanged) since funcIds are out of range
    EXPECT_EQ(pcieDeviceIntf->function0BusNumber(), 0u);
}

// ===========================================================================
// NsmPCIeECCGroup7: portInfoIntf is null but pcieDeviceIntf is set
// (covers the if(portInfoIntf) false branch)
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group7_PCIeDevice_NullPortInfoIntf)
{
    std::string devPath = rt3InvPath + "/pciedev_g7_nullpi";
    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(rt3Bus,
                                                           devPath.c_str());
    std::vector<uint64_t> funcIds = {0, 1};

    NsmPCIeECCGroup7 group("g7_nullpi", "T", pcieDeviceIntf, funcIds,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.port_type = NSM_PCIE_PORT_TYPE_ROOT_PORT; // should be skipped
    data.pcie_bus_number = 77;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group7_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieDeviceIntf->function0BusNumber(), 77u);
    EXPECT_EQ(pcieDeviceIntf->function1BusNumber(), 77u);
}

// ===========================================================================
// NsmPCIeECCGroup8 handleResponseMsg success via deviceIndex ctor
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group8_DeviceIndex_HandleResponseMsg_Success)
{
    auto laneErrorIntf = std::make_shared<LaneErrorIntf>(rt3Bus,
                                                         rt3InvPath.c_str());

    NsmPCIeECCGroup8 group("g8_ri_ok", "T", laneErrorIntf, uint8_t(1),
                           rt3InvPath);

    nsm_query_scalar_group_telemetry_group_8 data = {};
    for (int i = 0; i < TOTAL_PCIE_LANE_COUNT; i++)
    {
        data.error_counts[i] = static_cast<uint32_t>(i * 10);
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_8_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group8_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);

    auto errors = laneErrorIntf->rxErrorsPerLane();
    ASSERT_EQ(errors.size(), static_cast<size_t>(TOTAL_PCIE_LANE_COUNT));
    for (int i = 0; i < TOTAL_PCIE_LANE_COUNT; i++)
    {
        EXPECT_EQ(errors[i], static_cast<uint32_t>(i * 10));
    }
}

// ===========================================================================
// NsmPCIeECCGroup9 handleResponseMsg success via deviceIndex ctor
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group9_DeviceIndex_HandleResponseMsg_Success)
{
    auto aerIntf = std::make_shared<AERErrorStatusIntf>(rt3Bus,
                                                        rt3InvPath.c_str());

    NsmPCIeECCGroup9 group("g9_ri_ok", "T", rt3InvPath, aerIntf, uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_9 data = {};
    data.aer_uncorrectable_error_status = 0x12345678;
    data.aer_correctable_error_status = 0xABCDEF00;

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
    EXPECT_NE(uncStr.find("12345678"), std::string::npos);
    auto corrStr = aerIntf->aerCorrectableErrorStatus();
    EXPECT_NE(corrStr.find("ABCDEF00"), std::string::npos);
}

// ===========================================================================
// NsmPCIeECCGroup9 handleResponseMsg success with zero values
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group9_HandleResponseMsg_Success_ZeroValues)
{
    auto aerIntf = std::make_shared<AERErrorStatusIntf>(rt3Bus,
                                                        rt3InvPath.c_str());

    NsmPCIeECCGroup9 group("g9_zero", "T", rt3InvPath, aerIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_9 data = {};
    data.aer_uncorrectable_error_status = 0x00000000;
    data.aer_correctable_error_status = 0x00000000;

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
    EXPECT_NE(uncStr.find("00000000"), std::string::npos);
}

// ===========================================================================
// NsmPCIeECCGroup10 handleResponseMsg with high dword values
// (tests join64BitCounterLowHigh with non-zero high bits)
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test,
       DISABLED_Group10_HandleResponseMsg_NoInbound_HighDwords)
{
    std::string g10Path = rt3InvPath + "/g10highNoIn";
    NsmPCIeECCGroup10 group("g10_highNoIn", "T", g10Path,
                            NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1),
                            false);

    nsm_query_scalar_group_telemetry_group_10 data = {};
    data.outbound_read_tlp_count = 1;
    data.outbound_write_tlp_count = 2;
    data.outbound_completion_tlp_count = 3;
    data.dwords_transferred_in_outbound_completion = 4;
    data.dwords_transferred_in_outbound_read_tlp_high = 1;
    data.dwords_transferred_in_outbound_read_tlp_low = 0;
    data.dwords_transferred_in_outbound_write_tlp_high = 2;
    data.dwords_transferred_in_outbound_write_tlp_low = 0;
    data.read_requests_dropped_tag_unavailable = 5;
    data.read_requests_dropped_credit_exhaustion = 6;
    data.read_requests_dropped_credit_not_posted = 7;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group10_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(group.outboundReadPktCount, 1u);
    EXPECT_EQ(group.outboundWritePktCount, 2u);
    EXPECT_EQ(group.outboundTLPCount, 3u);
    // high=1, low=0 -> (1<<32 | 0) * 4 bytes
    EXPECT_EQ(group.outboundReadTransfer,
              (uint64_t(1) << 32) * BYTES_PER_DWORD);
    EXPECT_EQ(group.outboundWriteTransfer,
              (uint64_t(2) << 32) * BYTES_PER_DWORD);
    EXPECT_EQ(group.outboundTLPsTransfer, 4u * BYTES_PER_DWORD);
    EXPECT_EQ(group.reqDroppedTag, 5u);
    EXPECT_EQ(group.reqDroppedCreditCompletion, 6u);
    EXPECT_EQ(group.reqDroppedNonPostCredit, 7u);
}

// ===========================================================================
// NsmPCIeECCGroup10 handleResponseMsg with inbound and high dword values
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test,
       DISABLED_Group10_HandleResponseMsg_WithInbound_HighDwords)
{
    std::string g10Path = rt3InvPath + "/g10highIn";
    NsmPCIeECCGroup10 group("g10_highIn", "T", g10Path, NSM_PORT_TYPE_UPSTREAM,
                            uint8_t(0), uint8_t(1), true);

    nsm_query_scalar_group_telemetry_group_10_extended data = {};
    data.outbound_write_tlp_count = 10;
    data.dwords_transferred_in_outbound_write_tlp_high = 3;
    data.dwords_transferred_in_outbound_write_tlp_low = 100;
    data.read_requests_dropped_tag_unavailable = 20;
    data.read_requests_dropped_credit_exhaustion = 30;
    data.read_requests_dropped_credit_not_posted = 40;
    data.inbound_completion_tlp_count = 50;
    data.inbound_completion_tlp_bytes_high = 1;
    data.inbound_completion_tlp_bytes_low = 200;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_extended_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group10_extended_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(group.outboundWritePktCount, 10u);
    // high=3, low=100 -> ((3<<32)|100) * 4
    EXPECT_EQ(group.outboundWriteTransfer,
              ((uint64_t(3) << 32) | 100) * BYTES_PER_DWORD);
    EXPECT_EQ(group.reqDroppedTag, 20u);
    EXPECT_EQ(group.reqDroppedCreditCompletion, 30u);
    EXPECT_EQ(group.reqDroppedNonPostCredit, 40u);
    EXPECT_EQ(group.inboundTLPCount, 50u);
    EXPECT_EQ(group.inboundTLPsTransfer,
              ((uint64_t(1) << 32) | 200) * BYTES_PER_DWORD);
}

// ===========================================================================
// NsmPCIeVectorGroup1 genRequestMsg: valid encode (success path)
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, VectorGroup1_GenRequestMsg_Success)
{
    std::string lanePath = rt3InvPath + "/vg1_gen";
    auto laneErrorIntf = std::make_shared<PCIeLaneErrorIntf>(rt3Bus,
                                                             lanePath.c_str());
    auto laneAssocIntf = std::make_shared<AssociationDefIntf>(rt3Bus,
                                                              lanePath.c_str());

    NsmPCIeVectorGroup1 sensor("vg1_gen", "T", lanePath, laneErrorIntf,
                               laneAssocIntf, uint8_t(0),
                               NSM_PCIE_LINK_SPEED_CODE_GEN5,
                               NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    auto request = sensor.genRequestMsg(10, 0);
    EXPECT_TRUE(request.has_value());
}

// ===========================================================================
// NsmPCIeLaneManager: createLaneSensor with duplicate laneIdx (skip path)
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, LaneManager_CreateDuplicateLane_Skipped)
{
    std::string lmPath = rt3InvPath + "/lm_dup";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus, lmPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         lmPath.c_str());

    portWidthIntf->activeWidth(2);
    portInfoIntf->currentSpeed(NSM_PCIE_SPEED_GBPS_GEN5);

    NsmPCIeLaneManager mgr("lm_dup", "T", lmPath, portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    // First update creates 2 lanes
    mgr.update(gpu);
    EXPECT_EQ(mgr.getLaneSensorCount(), 2u);
    EXPECT_TRUE(mgr.hasLaneSensor(0));
    EXPECT_TRUE(mgr.hasLaneSensor(1));

    // Manually attempt to create duplicate lane sensor (should be skipped)
    mgr.createLaneSensor(gpu, 0, NSM_PCIE_LINK_SPEED_CODE_GEN5);
    EXPECT_EQ(mgr.getLaneSensorCount(), 2u); // unchanged
}

// ===========================================================================
// NsmPCIeLaneManager: update after speed change triggers clear+recreate
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test,
       DISABLED_LaneManager_SpeedChange_ClearsAndRecreates)
{
    std::string lmPath = rt3InvPath + "/lm_spdchg";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus, lmPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         lmPath.c_str());

    portWidthIntf->activeWidth(1);
    portInfoIntf->currentSpeed(NSM_PCIE_SPEED_GBPS_GEN3);

    NsmPCIeLaneManager mgr("lm_spdchg", "T", lmPath, portInfoIntf,
                           portWidthIntf, NSM_PORT_TYPE_UPSTREAM, uint8_t(0),
                           uint8_t(1));

    mgr.update(gpu);
    EXPECT_EQ(mgr.getLaneSensorCount(), 1u);
    EXPECT_EQ(mgr.currentSpeedCode, NSM_PCIE_LINK_SPEED_CODE_GEN3);

    // Change speed, trigger clearAllLaneSensors + recreate
    mgr.clearAllLaneSensors();
    portInfoIntf->currentSpeed(NSM_PCIE_SPEED_GBPS_GEN5);

    mgr.update(gpu);
    EXPECT_EQ(mgr.getLaneSensorCount(), 1u);
    EXPECT_EQ(mgr.currentSpeedCode, NSM_PCIE_LINK_SPEED_CODE_GEN5);
}

// ===========================================================================
// NsmPCIeLaneManager: convertSpeedGbpsToLinkSpeedCode edge between gens
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, LaneManager_ConvertSpeed_BetweenGens)
{
    std::string lmPath = rt3InvPath + "/lm_spdbtwn";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus, lmPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         lmPath.c_str());

    NsmPCIeLaneManager mgr("lm_spdbtwn", "T", lmPath, portInfoIntf,
                           portWidthIntf, NSM_PORT_TYPE_UPSTREAM, uint8_t(0),
                           uint8_t(1));

    // Speed below Gen1 threshold -> UNKNOWN
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(0.5),
              NSM_PCIE_LINK_SPEED_CODE_UNKNOWN);
    // Speed between Gen1 and Gen2 -> Gen1
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(NSM_PCIE_SPEED_GBPS_GEN1),
              NSM_PCIE_LINK_SPEED_CODE_GEN1);
    // Speed between Gen2 and Gen3 -> Gen2
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(NSM_PCIE_SPEED_GBPS_GEN2),
              NSM_PCIE_LINK_SPEED_CODE_GEN2);
    // Speed between Gen4 and Gen5 -> Gen4
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(NSM_PCIE_SPEED_GBPS_GEN4),
              NSM_PCIE_LINK_SPEED_CODE_GEN4);
    // Speed above Gen6 -> Gen6
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(200.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN6);
}

// ===========================================================================
// NsmPCIeLaneManager: hasLaneSensor returns false for non-existent lane
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, LaneManager_HasLaneSensor_NonExistent)
{
    std::string lmPath = rt3InvPath + "/lm_nohas";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus, lmPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         lmPath.c_str());

    NsmPCIeLaneManager mgr("lm_nohas", "T", lmPath, portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_FALSE(mgr.hasLaneSensor(0));
    EXPECT_FALSE(mgr.hasLaneSensor(255));
    EXPECT_EQ(mgr.getLaneSensorCount(), 0u);
}

// ===========================================================================
// NsmPCIeLaneManager: clearAllLaneSensors on empty manager
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, LaneManager_ClearEmpty)
{
    std::string lmPath = rt3InvPath + "/lm_clrempty";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(rt3Bus, lmPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(rt3Bus,
                                                         lmPath.c_str());

    NsmPCIeLaneManager mgr("lm_clrempty", "T", lmPath, portInfoIntf,
                           portWidthIntf, NSM_PORT_TYPE_UPSTREAM, uint8_t(0),
                           uint8_t(1));

    mgr.clearAllLaneSensors();
    EXPECT_EQ(mgr.getLaneSensorCount(), 0u);
    EXPECT_FALSE(mgr.lanesInitialized);
    EXPECT_EQ(mgr.currentLaneCount, 0u);
}

// ===========================================================================
// NsmPCIeECCGroup2 handleResponseMsg success via deviceIndex ctor
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group2_DeviceIndex_HandleResponseMsg_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt3Bus,
                                                     rt3InvPath.c_str());

    NsmPCIeECCGroup2 group("g2_ri_ok", "T", rt3InvPath, pcieEccIntf,
                           uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_2 data = {};
    data.non_fatal_errors = 111;
    data.fatal_errors = 222;
    data.correctable_errors = 333;
    data.unsupported_request_count = 444;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->nonfeCount(), 111u);
    EXPECT_EQ(pcieEccIntf->feCount(), 222u);
    EXPECT_EQ(pcieEccIntf->ceCount(), 333u);
    EXPECT_EQ(pcieEccIntf->unsupportedRequestCount(), 444u);
}

// ===========================================================================
// NsmPCIeECCGroup3 handleResponseMsg success via deviceIndex ctor
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, Group3_DeviceIndex_HandleResponseMsg_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(rt3Bus,
                                                     rt3InvPath.c_str());

    NsmPCIeECCGroup3 group("g3_ri_ok", "T", rt3InvPath, pcieEccIntf,
                           uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_3 data = {};
    data.L0ToRecoveryCount = 500;
    data.training_seq_errors = 600;
    data.framing_errors = 700;
    data.link_down_count = 800;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 500u);
}

// ===========================================================================
// NsmPort constructor: empty associations list
// ===========================================================================
TEST_F(NsmRetimerPortBranch3Test, NsmPort_Constructor_EmptyAssociations)
{
    std::string portName = "EmptyAssocPort";
    std::string type = "PortType";
    std::string invObj = rt3InvPath + "/emptyassoc";
    std::vector<utils::Association> assocs; // empty

    NsmPort port(rt3Bus, portName, type, assocs, invObj);

    EXPECT_EQ(port.portName, portName);
    auto assocList = port.associationDefIntf->associations();
    EXPECT_EQ(assocList.size(), 0u);
}
