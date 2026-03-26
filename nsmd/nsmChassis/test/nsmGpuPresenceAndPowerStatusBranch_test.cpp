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
 * Branch coverage tests for nsmGpuPresenceAndPowerStatus.cpp
 *
 * Targets uncovered branches:
 *   - genRequestMsg: default case returns nullopt, verify intf state unchanged
 *   - handleResponse: default case sets Fault on intf
 *   - handleResponse: GetPowerStatus decode failure (truncated buffer)
 *   - handleResponse: GetPowerStatus with non-zero gpuInstanceId bit patterns
 *   - handleResponse: GetPresence success with higher GPU instance
 *   - update(): partial failure (GetPresence OK, GetPowerStatus IO fails)
 *   - update(): multiple children all receive Fault on error
 *   - update(): partial failure with children propagates Fault
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#include "base.h"
#include "device-configuration.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmGpuPresenceAndPowerStatus.hpp"

using namespace nsm;

static auto& branchTestBus = utils::DBusHandler::getBus();

// Helper: create a NsmInterfaceProvider + OperationalStatusIntf
static NsmInterfaceProvider<OperationalStatusIntf>
    makeProvider(const std::string& path,
                 std::shared_ptr<OperationalStatusIntf>& outIntf)
{
    outIntf = std::make_shared<OperationalStatusIntf>(branchTestBus,
                                                      path.c_str());
    return NsmInterfaceProvider<OperationalStatusIntf>(
        "op_status", "NSM_GpuPresence", std::filesystem::path(path), outIntf);
}

// Helper: build a GPU presence response
static std::vector<uint8_t> buildPresenceResponse(uint8_t cc, uint8_t presence)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpu_presence_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_gpu_presence_resp(0, cc, ERR_NULL, presence, msg);
    return buf;
}

// Helper: build a GPU power status response
static std::vector<uint8_t> buildPowerStatusResponse(uint8_t cc,
                                                     uint8_t powerStatus)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpu_power_status_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_gpu_power_status_resp(0, cc, ERR_NULL, powerStatus, msg);
    return buf;
}

// =============================================================================
// genRequestMsg – default state: verify intf state is NOT changed to Fault
// (only genRequestMsg returns nullopt; no side-effect on intf)
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatusBranch,
     GenRequestMsg_DefaultState_IntfStateUnchanged)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/br/genreq_default_intf", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    auto stateBefore = intf->state();

    sensor.state = static_cast<NsmGpuPresenceAndPowerStatus::State>(42);
    auto request = sensor.genRequestMsg(10, 0);

    EXPECT_FALSE(request.has_value());
    // genRequestMsg should not modify the intf state
    EXPECT_EQ(intf->state(), stateBefore);
}

// =============================================================================
// handleResponse – default state: verify intf is set to Fault and functional
// false (cc == NSM_SUCCESS but rc != NSM_SW_SUCCESS path)
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatusBranch,
     HandleResponse_DefaultState_SetsFaultAndFunctionalFalse)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/br/handle_default_fault", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = static_cast<NsmGpuPresenceAndPowerStatus::State>(77);

    // Build a minimal valid buffer for the default case
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_NE(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Fault);
    EXPECT_FALSE(intf->functional());
}

// =============================================================================
// handleResponse – GetPowerStatus decode failure (truncated buffer)
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatusBranch,
     HandleResponse_GetPowerStatus_DecodeFail_SetsFault)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/br/handle_power_decode_fail", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto buf = buildPowerStatusResponse(NSM_SUCCESS, 0x01);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    // Pass truncated size to force decode failure
    auto rc = sensor.handleResponse(msg, 1);

    EXPECT_NE(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Fault);
    EXPECT_FALSE(intf->functional());
}

// =============================================================================
// handleResponse – GetPowerStatus with gpuInstanceId=3
// power=0x08 (bit 3), presence=0x08 (bit 3) → Enabled
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatusBranch,
     HandleResponse_GetPowerStatus_GpuInstance3_PowerAndPresence_Enabled)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/br/handle_gpu3_enabled", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 3);

    sensor.gpusPresence = 0x08;                             // bit 3 set
    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto buf = buildPowerStatusResponse(NSM_SUCCESS, 0x08); // bit 3 set
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Enabled);
    EXPECT_TRUE(intf->functional());
}

// =============================================================================
// handleResponse – GetPowerStatus with gpuInstanceId=2
// presence=0x04 (bit 2), power=0x00 → UnavailableOffline
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatusBranch,
     HandleResponse_GetPowerStatus_GpuInstance2_PresenceOnly_Unavailable)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/br/handle_gpu2_unavailable", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 2);

    sensor.gpusPresence = 0x04;                             // bit 2 set
    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto buf = buildPowerStatusResponse(NSM_SUCCESS, 0x00); // no power
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::UnavailableOffline);
    EXPECT_FALSE(intf->functional());
}

// =============================================================================
// handleResponse – GetPowerStatus with gpuInstanceId=5
// presence=0 (bit 5 not set), power=0x20 (bit 5 set) → Absent
// Power without presence still results in Absent
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatusBranch,
     HandleResponse_GetPowerStatus_GpuInstance5_PowerNoPresence_Absent)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/br/handle_gpu5_absent", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 5);

    sensor.gpusPresence = 0x00;                             // bit 5 not set
    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto buf = buildPowerStatusResponse(NSM_SUCCESS, 0x20); // bit 5 set
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Absent);
    EXPECT_FALSE(intf->functional());
}

// =============================================================================
// handleResponse – GetPresence success stores correct value for higher instance
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatusBranch,
     HandleResponse_GetPresence_StoresPresenceBitmask)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/br/handle_presence_bitmask", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 4);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    auto buf = buildPresenceResponse(NSM_SUCCESS, 0xFF);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor.gpusPresence, 0xFF);
}

// =============================================================================
// handleResponse – GetPowerStatus error with non-zero cc returns cc
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatusBranch,
     HandleResponse_GetPowerStatus_ErrorCC_ReturnsCC)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/br/handle_power_err_cc", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto buf = buildPowerStatusResponse(NSM_ERROR, 0x00);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    // cc != NSM_SUCCESS → returns cc
    EXPECT_EQ(rc, NSM_ERROR);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Fault);
    EXPECT_FALSE(intf->functional());
}

// =============================================================================
// Fixture for update() tests
// =============================================================================

struct GpuPresenceBranchUpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    GpuPresenceBranchUpdateTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~GpuPresenceBranchUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// update() – partial failure: GetPresence succeeds, GetPowerStatus IO fails
// This covers the branch where the outer for-loop exits early on second
// iteration (rc != NSM_SW_SUCCESS after first success).
// Lines 57-68: error propagation after partial success.
// =============================================================================

TEST_F(GpuPresenceBranchUpdateTest,
       Update_PartialFailure_PresenceOkPowerFails_SetsFault)
{
    std::shared_ptr<OperationalStatusIntf> parentIntf;
    auto parentProvider = makeProvider("/test/br/update_partial_fail",
                                       parentIntf);
    auto parent = std::make_shared<NsmGpuPresenceAndPowerStatus>(parentProvider,
                                                                 0);

    auto presenceResp = buildPresenceResponse(NSM_SUCCESS, 0x01);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(Invoke(mockSensorIO(presenceResp)))
        .WillOnce(Invoke(mockSensorIO(NSM_ERROR)));

    (void)parent->update(gpu);

    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(parentIntf->state(), ST::Fault);
    EXPECT_FALSE(parentIntf->functional());
}

// =============================================================================
// update() – partial failure with child sensors: GetPresence OK, GetPowerStatus
// IO fails → both parent and child get Fault
// =============================================================================

TEST_F(GpuPresenceBranchUpdateTest,
       Update_PartialFailure_WithChild_PropagatesFault)
{
    std::shared_ptr<OperationalStatusIntf> parentIntf, childIntf;
    auto parentProvider = makeProvider("/test/br/update_partial_parent",
                                       parentIntf);
    auto parent = std::make_shared<NsmGpuPresenceAndPowerStatus>(parentProvider,
                                                                 0);

    auto childProvider = makeProvider("/test/br/update_partial_child",
                                      childIntf);
    auto child = std::make_shared<NsmGpuPresenceAndPowerStatus>(childProvider,
                                                                1);
    parent->sensors.push_back(child);

    auto presenceResp = buildPresenceResponse(NSM_SUCCESS, 0x03);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(Invoke(mockSensorIO(presenceResp)))
        .WillOnce(Invoke(mockSensorIO(NSM_ERROR)));

    (void)parent->update(gpu);

    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(parentIntf->state(), ST::Fault);
    EXPECT_FALSE(parentIntf->functional());
    EXPECT_EQ(childIntf->state(), ST::Fault);
    EXPECT_FALSE(childIntf->functional());
}

// =============================================================================
// update() – multiple children all receive Fault on IO error
// Covers iteration over multiple children in error propagation loop (L61-68)
// =============================================================================

TEST_F(GpuPresenceBranchUpdateTest, Update_IOError_MultipleChildren_AllGetFault)
{
    std::shared_ptr<OperationalStatusIntf> parentIntf, child1Intf, child2Intf;
    auto parentProvider = makeProvider("/test/br/update_multi_parent",
                                       parentIntf);
    auto parent = std::make_shared<NsmGpuPresenceAndPowerStatus>(parentProvider,
                                                                 0);

    auto child1Provider = makeProvider("/test/br/update_multi_child1",
                                       child1Intf);
    auto child1 = std::make_shared<NsmGpuPresenceAndPowerStatus>(child1Provider,
                                                                 1);
    auto child2Provider = makeProvider("/test/br/update_multi_child2",
                                       child2Intf);
    auto child2 = std::make_shared<NsmGpuPresenceAndPowerStatus>(child2Provider,
                                                                 2);

    parent->sensors.push_back(child1);
    parent->sensors.push_back(child2);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(Invoke(mockSensorIO(NSM_ERROR)));

    (void)parent->update(gpu);

    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(parentIntf->state(), ST::Fault);
    EXPECT_FALSE(parentIntf->functional());
    EXPECT_EQ(child1Intf->state(), ST::Fault);
    EXPECT_FALSE(child1Intf->functional());
    EXPECT_EQ(child2Intf->state(), ST::Fault);
    EXPECT_FALSE(child2Intf->functional());
}

// =============================================================================
// update() – success with multiple children: all get state propagated
// Covers inner for-loop iterating over >1 children (L49-53) for both states
// =============================================================================

TEST_F(GpuPresenceBranchUpdateTest,
       Update_Success_MultipleChildren_StatesPropagated)
{
    std::shared_ptr<OperationalStatusIntf> parentIntf, child1Intf, child2Intf;
    auto parentProvider = makeProvider("/test/br/update_ok_multi_parent",
                                       parentIntf);
    auto parent = std::make_shared<NsmGpuPresenceAndPowerStatus>(parentProvider,
                                                                 0);

    auto child1Provider = makeProvider("/test/br/update_ok_multi_child1",
                                       child1Intf);
    auto child1 = std::make_shared<NsmGpuPresenceAndPowerStatus>(child1Provider,
                                                                 1);
    auto child2Provider = makeProvider("/test/br/update_ok_multi_child2",
                                       child2Intf);
    auto child2 = std::make_shared<NsmGpuPresenceAndPowerStatus>(child2Provider,
                                                                 2);

    parent->sensors.push_back(child1);
    parent->sensors.push_back(child2);

    auto presenceResp = buildPresenceResponse(NSM_SUCCESS, 0x07);
    auto powerResp = buildPowerStatusResponse(NSM_SUCCESS, 0x07);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(Invoke(mockSensorIO(presenceResp)))
        .WillOnce(Invoke(mockSensorIO(powerResp)));

    (void)parent->update(gpu);

    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(parentIntf->state(), ST::Enabled);
    EXPECT_TRUE(parentIntf->functional());
}

// =============================================================================
// handleResponse – GetPresence error with non-success cc value
// Ensures the cc path (cc != NSM_SUCCESS) triggers Fault
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatusBranch,
     HandleResponse_GetPresence_ErrorCC_ReturnsCCValue)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/br/handle_presence_cc", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    auto buf = buildPresenceResponse(NSM_ERROR, 0x00);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    // cc is NSM_ERROR → returned via cc ? cc : rc
    EXPECT_EQ(rc, NSM_ERROR);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Fault);
    EXPECT_FALSE(intf->functional());
}
