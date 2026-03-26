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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#define private public
#define protected public

#include "libnsm/firmware-utils.h"

#include "nsmDot.hpp"

using namespace nsm;
using namespace ::testing;

struct NsmDotStatusObjectTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:80";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmDotStatusObjectTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmDotStatusObjectTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmDotStatusObjectTest, testConstructor)
{
    std::string chassisName = "DotStatusTest";
    auto dotActionIntf = std::make_unique<NsmDotObject>(
        utils::DBusHandler::getBus(), chassisName, gpuUuid, "");

    NsmDotStatusObject dotStatus(chassisName, "NSM_Dot_Status",
                                 dotActionIntf.get(), gpuUuid);

    EXPECT_EQ(dotStatus.getName(), chassisName);
    EXPECT_EQ(dotStatus.getType(), "NSM_Dot_Status");
    EXPECT_EQ(dotStatus.dotActionIntf_, dotActionIntf.get());
}

TEST_F(NsmDotStatusObjectTest, testGenRequestMsg)
{
    std::string chassisName = "DotStatusReq";
    auto dotActionIntf = std::make_unique<NsmDotObject>(
        utils::DBusHandler::getBus(), chassisName, gpuUuid, "");

    NsmDotStatusObject dotStatus(chassisName, "NSM_Dot_Status",
                                 dotActionIntf.get(), gpuUuid);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    auto request = dotStatus.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_req));

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    EXPECT_EQ(msg->hdr.instance_id, instanceId);
}

TEST_F(NsmDotStatusObjectTest, testHandleResponseUninitialized)
{
    std::string chassisName = "DotStatusUninit";
    auto dotActionIntf = std::make_unique<NsmDotObject>(
        utils::DBusHandler::getBus(), chassisName, gpuUuid, "");

    NsmDotStatusObject dotStatus(chassisName, "NSM_Dot_Status",
                                 dotActionIntf.get(), gpuUuid);

    Response dotStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(dotStatusResp.data());

    uint8_t status = 0x00; // DOT_STATUS_UNINITIALIZED

    auto rc = encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, status,
                                             msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = dotStatus.handleResponseMsg(msg, dotStatusResp.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(dotActionIntf->dotState(),
              DotActionIntf::DOTStates::Uninitialized);
}

TEST_F(NsmDotStatusObjectTest, testHandleResponseVolatile)
{
    std::string chassisName = "DotStatusVolatile";
    auto dotActionIntf = std::make_unique<NsmDotObject>(
        utils::DBusHandler::getBus(), chassisName, gpuUuid, "");

    NsmDotStatusObject dotStatus(chassisName, "NSM_Dot_Status",
                                 dotActionIntf.get(), gpuUuid);

    Response dotStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(dotStatusResp.data());

    uint8_t status = 0x01; // DOT_STATUS_VOLATILE

    auto rc = encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, status,
                                             msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = dotStatus.handleResponseMsg(msg, dotStatusResp.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(dotActionIntf->dotState(), DotActionIntf::DOTStates::Volatile);
}

TEST_F(NsmDotStatusObjectTest, testHandleResponseDisabled)
{
    std::string chassisName = "DotStatusDisabled";
    auto dotActionIntf = std::make_unique<NsmDotObject>(
        utils::DBusHandler::getBus(), chassisName, gpuUuid, "");

    NsmDotStatusObject dotStatus(chassisName, "NSM_Dot_Status",
                                 dotActionIntf.get(), gpuUuid);

    Response dotStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(dotStatusResp.data());

    uint8_t status = 0x03; // DOT_STATUS_DISABLED

    auto rc = encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, status,
                                             msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = dotStatus.handleResponseMsg(msg, dotStatusResp.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(dotActionIntf->dotState(), DotActionIntf::DOTStates::Disabled);
}

TEST_F(NsmDotStatusObjectTest, badTestHandleResponseWrongSize)
{
    std::string chassisName = "DotStatusBadSize";
    auto dotActionIntf = std::make_unique<NsmDotObject>(
        utils::DBusHandler::getBus(), chassisName, gpuUuid, "");

    NsmDotStatusObject dotStatus(chassisName, "NSM_Dot_Status",
                                 dotActionIntf.get(), gpuUuid);

    // Create response with wrong size
    Response badResp(sizeof(nsm_msg_hdr), 0);
    auto msg = reinterpret_cast<nsm_msg*>(badResp.data());

    auto rc = dotStatus.handleResponseMsg(msg, badResp.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotStatusObjectTest, badTestHandleResponseReasonCodeError)
{
    std::string chassisName = "DotStatusReasonErr";
    auto dotActionIntf = std::make_unique<NsmDotObject>(
        utils::DBusHandler::getBus(), chassisName, gpuUuid, "");

    NsmDotStatusObject dotStatus(chassisName, "NSM_Dot_Status",
                                 dotActionIntf.get(), gpuUuid);

    Response dotStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(dotStatusResp.data());

    uint8_t status = 0x00;

    auto rc = encode_nsm_dot_get_status_resp(0, NSM_SUCCESS,
                                             0xABCD, // reason code error
                                             status, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = dotStatus.handleResponseMsg(msg, dotStatusResp.size());
    // Should still succeed but log the reason code
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotStatusObjectTest, testUpdateCoroutine)
{
    std::string chassisName = "DotStatusUpdate";
    auto dotActionIntf = std::make_unique<NsmDotObject>(
        utils::DBusHandler::getBus(), chassisName, gpuUuid, "");

    NsmDotStatusObject dotStatus(chassisName, "NSM_Dot_Status",
                                 dotActionIntf.get(), gpuUuid);

    Response dotStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(dotStatusResp.data());

    uint8_t status = 0x02; // DOT_STATUS_LOCKED

    auto rc = encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, status,
                                             msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(dotStatusResp, Response{}));

    dotStatus.update(gpu);
}

// Error-CC path: decode succeeds but cc = NSM_ERROR (non-zero)
// → if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS) is true
// → return cc ? cc : rc; takes the true (cc != 0) branch.
TEST_F(NsmDotStatusObjectTest, HandleResponseMsg_ErrorCC_CoversBranch)
{
    std::string chassisName = "DotStatusErrorCC";
    auto dotActionIntf = std::make_unique<NsmDotObject>(
        utils::DBusHandler::getBus(), chassisName, gpuUuid, "");

    NsmDotStatusObject dotStatus(chassisName, "NSM_Dot_Status",
                                 dotActionIntf.get(), gpuUuid);

    Response dotStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(dotStatusResp.data());

    uint8_t status = 0x00;
    auto rc = encode_nsm_dot_get_status_resp(0, NSM_ERROR, ERR_NULL, status,
                                             msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = dotStatus.handleResponseMsg(msg, dotStatusResp.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotStatusObjectTest, testStatusTransitions)
{
    std::string chassisName = "DotStatusTransitions";
    auto dotActionIntf = std::make_unique<NsmDotObject>(
        utils::DBusHandler::getBus(), chassisName, gpuUuid, "");

    NsmDotStatusObject dotStatus(chassisName, "NSM_Dot_Status",
                                 dotActionIntf.get(), gpuUuid);

    // Test state transitions: Uninitialized -> Volatile -> Locked -> Disabled
    std::vector<uint8_t> stateSequence = {0x00, 0x01, 0x02, 0x03};
    std::vector<DotActionIntf::DOTStates> expectedStates = {
        DotActionIntf::DOTStates::Uninitialized,
        DotActionIntf::DOTStates::Volatile, DotActionIntf::DOTStates::Locked,
        DotActionIntf::DOTStates::Disabled};

    for (size_t i = 0; i < stateSequence.size(); i++)
    {
        Response dotStatusResp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(dotStatusResp.data());

        auto rc = encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 stateSequence[i], msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        rc = dotStatus.handleResponseMsg(msg, dotStatusResp.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        EXPECT_EQ(dotActionIntf->dotState(), expectedStates[i]);
    }
}
