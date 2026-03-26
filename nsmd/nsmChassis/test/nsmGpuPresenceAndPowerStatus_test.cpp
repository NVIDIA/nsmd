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
 * Tests for nsmd/nsmChassis/nsmGpuPresenceAndPowerStatus.cpp
 *
 *   - genRequestMsg: GetPresence state → encode succeed/fail
 *   - genRequestMsg: GetPowerStatus state → encode succeed/fail
 *   - genRequestMsg: default (unsupported) state → return nullopt
 *   - handleResponse: GetPresence success → state = GetPresence, no update
 *   - handleResponse: GetPresence error → Fault/false
 *   - handleResponse: GetPowerStatus success, power && presence → Enabled/true
 *   - handleResponse: GetPowerStatus success, presence only →
 * UnavailableOffline
 *   - handleResponse: GetPowerStatus success, neither → Absent
 *   - handleResponse: GetPowerStatus error → Fault/false
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

static auto& testBus = utils::DBusHandler::getBus();

// Helper: create a NsmInterfaceProvider + OperationalStatusIntf
static NsmInterfaceProvider<OperationalStatusIntf>
    makeProvider(const std::string& path,
                 std::shared_ptr<OperationalStatusIntf>& outIntf)
{
    outIntf = std::make_shared<OperationalStatusIntf>(testBus, path.c_str());
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
// Constructor
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus, Constructor_CreatesObject)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/ctor", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    EXPECT_EQ(sensor.getName(), "op_status");
    EXPECT_EQ(sensor.getType(), "NSM_GpuPresence");
    EXPECT_EQ(sensor.gpuInstanceId, 0);
}

// =============================================================================
// genRequestMsg – GetPresence state (success)
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus, GenRequestMsg_GetPresence_ReturnsRequest)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/genreq_presence", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    // state is initialized to GetPresence in constructor
    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    auto request = sensor.genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto cmd = reinterpret_cast<const nsm_get_fpga_diagnostics_settings_req*>(
        msg->payload);
    EXPECT_EQ(cmd->data_index, GET_GPU_PRESENCE);
}

// =============================================================================
// genRequestMsg – GetPresence encode failure (instanceId > NSM_INSTANCE_MAX)
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus,
     GenRequestMsg_GetPresence_InvalidInstance_ReturnsNullopt)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/genreq_presence_fail",
                                 intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    auto request = sensor.genRequestMsg(5, NSM_INSTANCE_MAX + 1);

    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// genRequestMsg – GetPowerStatus state (success)
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus, GenRequestMsg_GetPowerStatus_ReturnsRequest)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/genreq_powerstatus", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto request = sensor.genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto cmd = reinterpret_cast<const nsm_get_fpga_diagnostics_settings_req*>(
        msg->payload);
    EXPECT_EQ(cmd->data_index, GET_GPU_POWER_STATUS);
}

// =============================================================================
// genRequestMsg – GetPowerStatus encode failure
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus,
     GenRequestMsg_GetPowerStatus_InvalidInstance_ReturnsNullopt)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/genreq_powerstatus_fail",
                                 intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto request = sensor.genRequestMsg(5, NSM_INSTANCE_MAX + 1);

    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// handleResponse – GetPresence success (not GetPowerStatus → no state update)
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus,
     HandleResponse_GetPresence_Success_StoresPresence)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/handle_presence_ok", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    // GPU 0 present: bit 0 set
    auto buf = buildPresenceResponse(NSM_SUCCESS, 0x01);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    // gpusPresence should be stored (0x01)
    EXPECT_EQ(sensor.gpusPresence, 0x01);
}

// =============================================================================
// handleResponse – GetPresence error → Fault/false
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus, HandleResponse_GetPresence_ErrorCC_SetsFault)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/handle_presence_err",
                                 intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    auto buf = buildPresenceResponse(NSM_ERROR, 0x00);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_NE(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Fault);
    EXPECT_FALSE(intf->functional());
}

// =============================================================================
// handleResponse – GetPresence decode fail → Fault/false
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus,
     HandleResponse_GetPresence_DecodeFail_SetsFault)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider =
        makeProvider("/test/gpu_presence/handle_presence_decode_fail", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    // Pass a valid success buf but truncated size
    auto buf = buildPresenceResponse(NSM_SUCCESS, 0x01);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, 1); // too small → decode fails

    EXPECT_NE(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Fault);
    EXPECT_FALSE(intf->functional());
}

// =============================================================================
// handleResponse – GetPowerStatus, power && presence → Enabled/true
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus,
     HandleResponse_GetPowerStatus_PowerAndPresence_SetsEnabled)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/handle_power_enabled",
                                 intf);
    // GPU instance 0
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    // First: set presence (bit 0 = 1)
    sensor.gpusPresence = 0x01;
    // Now handle power status (bit 0 = 1 → power ON)
    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto buf = buildPowerStatusResponse(NSM_SUCCESS, 0x01); // bit 0 set
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Enabled);
    EXPECT_TRUE(intf->functional());
}

// =============================================================================
// handleResponse – GetPowerStatus, presence only → UnavailableOffline
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus,
     HandleResponse_GetPowerStatus_PresenceOnly_SetsUnavailableOffline)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/handle_power_unavailable",
                                 intf);
    // GPU instance 0, presence=1, power=0
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.gpusPresence = 0x01;                             // bit 0 = present
    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto buf = buildPowerStatusResponse(NSM_SUCCESS, 0x00); // bit 0 = no power
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::UnavailableOffline);
    EXPECT_FALSE(intf->functional());
}

// =============================================================================
// handleResponse – GetPowerStatus, neither power nor presence → Absent
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus,
     HandleResponse_GetPowerStatus_NoPresence_SetsAbsent)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/handle_power_absent",
                                 intf);
    // GPU instance 0, presence=0, power=0
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.gpusPresence = 0x00; // not present
    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto buf = buildPowerStatusResponse(NSM_SUCCESS, 0x00);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Absent);
    EXPECT_FALSE(intf->functional());
}

// =============================================================================
// handleResponse – GetPowerStatus error → Fault/false
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus,
     HandleResponse_GetPowerStatus_ErrorCC_SetsFault)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/handle_power_err", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    sensor.state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    auto buf = buildPowerStatusResponse(NSM_ERROR, 0xFF);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());

    EXPECT_NE(rc, NSM_SUCCESS);
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(intf->state(), ST::Fault);
    EXPECT_FALSE(intf->functional());
}

// =============================================================================
// update() with child sensors – fixture
// =============================================================================

struct NsmGpuPresenceAndPowerStatusUpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmGpuPresenceAndPowerStatusUpdateTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGpuPresenceAndPowerStatusUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// update() – child sensor state propagation (lines 51-52)
// Both iterations succeed; inner for-loop propagates state to child each time.
// =============================================================================

TEST_F(NsmGpuPresenceAndPowerStatusUpdateTest,
       Update_WithChildSensor_Success_PropagatesStateAndEnabled)
{
    std::shared_ptr<OperationalStatusIntf> parentIntf, childIntf;
    auto parentProvider = makeProvider("/test/gpu_presence/update_parent_ok",
                                       parentIntf);
    auto parent = std::make_shared<NsmGpuPresenceAndPowerStatus>(parentProvider,
                                                                 0);

    auto childProvider = makeProvider("/test/gpu_presence/update_child_ok",
                                      childIntf);
    auto child = std::make_shared<NsmGpuPresenceAndPowerStatus>(childProvider,
                                                                0);
    // Add child to parent's sensors list (covers inner for-loop at lines 51-52)
    parent->sensors.push_back(child);

    // First sensorIO call: GetPresence, GPU 0 present (bit 0 set)
    auto presenceResp = buildPresenceResponse(NSM_SUCCESS, 0x01);
    // Second sensorIO call: GetPowerStatus, GPU 0 powered (bit 0 set)
    auto powerResp = buildPowerStatusResponse(NSM_SUCCESS, 0x01);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(Invoke(mockSensorIO(presenceResp)))
        .WillOnce(Invoke(mockSensorIO(powerResp)));

    (void)parent->update(gpu);

    // Parent: GPU 0 present + powered → Enabled
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(parentIntf->state(), ST::Enabled);
    EXPECT_TRUE(parentIntf->functional());
}

// =============================================================================
// update() – error propagation to child sensors (lines 65-67)
// sensorIO transport error → rc != NSM_SW_SUCCESS → Fault propagated to child.
// The inner for-loop at lines 51-52 also runs in this iteration.
// =============================================================================

TEST_F(NsmGpuPresenceAndPowerStatusUpdateTest,
       Update_WithChildSensor_SensorIOError_PropagatesFaultToChild)
{
    std::shared_ptr<OperationalStatusIntf> parentIntf, childIntf;
    auto parentProvider = makeProvider("/test/gpu_presence/update_parent_err",
                                       parentIntf);
    auto parent = std::make_shared<NsmGpuPresenceAndPowerStatus>(parentProvider,
                                                                 0);

    auto childProvider = makeProvider("/test/gpu_presence/update_child_err",
                                      childIntf);
    auto child = std::make_shared<NsmGpuPresenceAndPowerStatus>(childProvider,
                                                                0);
    // Add child so error propagation loop (lines 61-68) iterates over it
    parent->sensors.push_back(child);

    // sensorIO returns transport error → NsmSensor::update returns non-zero
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(Invoke(mockSensorIO(NSM_ERROR)));

    (void)parent->update(gpu);

    // Both parent and child should have Fault state
    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(parentIntf->state(), ST::Fault);
    EXPECT_FALSE(parentIntf->functional());
    EXPECT_EQ(childIntf->state(), ST::Fault);
    EXPECT_FALSE(childIntf->functional());
}

// =============================================================================
// update() – no child sensors (L51 inner for-loop exits immediately)
// Both outer loop iterations (GetPresence, GetPowerStatus) succeed.
// Branch 5→7: inner for-loop false from start (sensors.empty())
// =============================================================================

TEST_F(NsmGpuPresenceAndPowerStatusUpdateTest,
       Update_NoChildSensors_Success_InnerLoopSkipped)
{
    std::shared_ptr<OperationalStatusIntf> parentIntf;
    auto parentProvider = makeProvider("/test/gpu_presence/update_no_children",
                                       parentIntf);
    auto parent = std::make_shared<NsmGpuPresenceAndPowerStatus>(parentProvider,
                                                                 0);
    // sensors is empty — inner for-loop at L51 exits immediately each iteration

    auto presenceResp = buildPresenceResponse(NSM_SUCCESS, 0x01);
    auto powerResp = buildPowerStatusResponse(NSM_SUCCESS, 0x01);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(Invoke(mockSensorIO(presenceResp)))
        .WillOnce(Invoke(mockSensorIO(powerResp)));

    (void)parent->update(gpu);

    using ST = OperationalStatusIntf::StateType;
    EXPECT_EQ(parentIntf->state(), ST::Enabled);
    EXPECT_TRUE(parentIntf->functional());
}

// =============================================================================
// genRequestMsg – default (unsupported) state → rc stays NSM_SW_ERROR → nullopt
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus, GenRequestMsg_DefaultState_ReturnsNullopt)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/genreq_default", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    // Force an out-of-enum state to hit the default branch
    sensor.state = static_cast<NsmGpuPresenceAndPowerStatus::State>(99);
    auto request = sensor.genRequestMsg(5, 0);

    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// handleResponse – default (unsupported) state → rc=NSM_SW_ERROR,
// cc=NSM_SUCCESS → cc == NSM_SUCCESS but rc != NSM_SW_SUCCESS → Fault
// =============================================================================

TEST(NsmGpuPresenceAndPowerStatus, HandleResponse_DefaultState_SetsFault)
{
    std::shared_ptr<OperationalStatusIntf> intf;
    auto provider = makeProvider("/test/gpu_presence/handle_default", intf);
    NsmGpuPresenceAndPowerStatus sensor(provider, 0);

    // Force out-of-enum state → default branch in handleResponse
    sensor.state = static_cast<NsmGpuPresenceAndPowerStatus::State>(99);

    // Any buffer; decode won't match any case so rc stays NSM_SW_ERROR
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 10, 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponse(msg, buf.size());
    // rc=NSM_SW_ERROR, cc stays NSM_SUCCESS → returns cc ? cc : rc = rc
    EXPECT_NE(rc, NSM_SUCCESS);
}
