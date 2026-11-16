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
 * Tests for the AdaptiveTGPMode (Dual Part Number) sensor in
 * nsmd/nsmProcessor/nsmProcessor.cpp.
 *
 * Covers:
 *   NsmAdaptiveTGPMode::genRequestMsg      - success and encode-failure paths
 *   NsmAdaptiveTGPMode::handleResponseMsg  - current/pending decode, the
 *                                            missing-pending-payload guard,
 *                                            error CC and decode failure
 *   NsmAdaptiveTGPMode::updateReading      - both PDI properties are set
 *   createReconfigPermissionsFeature       - the sensor is created only when
 *                                            "AdaptiveTGPMode" is an available
 *                                            and not disabled knob (PRC knob
 *                                            25)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmProcessor.hpp"

#undef private
#undef protected

#include "features/creators.hpp"
#include "features/feature.hpp"

#include <algorithm>

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================
struct NsmAdaptiveTGPModeTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    std::string sensorName{"adaptive_tgpmode_sensor"};
    std::string sensorType{"NSM_AdaptiveTGPMode"};
    std::string inventoryObjPath{
        "/xyz/openbmc_project/inventory/adaptive_tgpmode_device"};

    NsmAdaptiveTGPModeTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmAdaptiveTGPModeTest()
    {
        cleanupDeviceSensors(devices);
    }

    auto& bus()
    {
        return utils::DBusHandler::getBus();
    }

    // Buffer large enough for a device-mode-settings v2 response carrying a
    // 1-byte current payload and a 1-byte pending payload.
    std::vector<uint8_t> makeRespBuf()
    {
        return std::vector<uint8_t>(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
                sizeof(uint8_t) * 2,
            0);
    }

    std::vector<uint8_t> makeDecodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ============================================================================
// genRequestMsg
// ============================================================================

TEST_F(NsmAdaptiveTGPModeTest, GenReq_Success)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 0);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) +
                                   sizeof(nsm_get_device_mode_settings_v2_req));
}

TEST_F(NsmAdaptiveTGPModeTest, GenReq_EncodeFail)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// handleResponseMsg
// ============================================================================

TEST_F(NsmAdaptiveTGPModeTest, HandleResp_CurrentEnabled_PendingEnabled)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    auto responseMsg = makeRespBuf();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t current = NSM_ADAPTIVE_TGPMODE_ENABLED;
    uint8_t pending = NSM_ADAPTIVE_TGPMODE_ENABLED;

    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &current, sizeof(current), &pending,
        sizeof(pending), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->adaptiveTGPModeEnabled());
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->pendingAdaptiveTGPMode());
}

TEST_F(NsmAdaptiveTGPModeTest, HandleResp_CurrentDisabled_PendingDisabled)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    auto responseMsg = makeRespBuf();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t current = NSM_ADAPTIVE_TGPMODE_DISABLED;
    uint8_t pending = NSM_ADAPTIVE_TGPMODE_DISABLED;

    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &current, sizeof(current), &pending,
        sizeof(pending), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.adaptiveTGPModeIntf->adaptiveTGPModeEnabled());
    EXPECT_FALSE(sensor.adaptiveTGPModeIntf->pendingAdaptiveTGPMode());
}

// A staged mode change: current still disabled, pending already enabled.
TEST_F(NsmAdaptiveTGPModeTest,
       HandleResp_StagedChange_PendingDiffersFromCurrent)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    auto responseMsg = makeRespBuf();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t current = NSM_ADAPTIVE_TGPMODE_DISABLED;
    uint8_t pending = NSM_ADAPTIVE_TGPMODE_ENABLED;

    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &current, sizeof(current), &pending,
        sizeof(pending), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.adaptiveTGPModeIntf->adaptiveTGPModeEnabled());
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->pendingAdaptiveTGPMode());
}

// No pending payload (pending_mode_length == 0) -> the response is incomplete,
// so neither property is updated and the stale state is preserved.
TEST_F(NsmAdaptiveTGPModeTest,
       HandleResp_NoPendingPayload_LeavesPropertiesUntouched)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    // Seed a known state so we can prove the short response does not
    // overwrite it.
    sensor.updateReading(false, false);

    auto responseMsg = makeRespBuf();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t current = NSM_ADAPTIVE_TGPMODE_ENABLED;

    auto rc = encode_get_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL,
                                                      &current, sizeof(current),
                                                      nullptr, 0, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // cc is NSM_SUCCESS and decode succeeded, so the sensor still reports
    // success; only the property update is suppressed.
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.adaptiveTGPModeIntf->adaptiveTGPModeEnabled());
    EXPECT_FALSE(sensor.adaptiveTGPModeIntf->pendingAdaptiveTGPMode());
}

// A current payload with no pending payload must not be treated as a partial
// update: the current byte says enabled, but nothing is published.
TEST_F(NsmAdaptiveTGPModeTest,
       HandleResp_NoPendingPayload_DoesNotPublishCurrent)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    sensor.updateReading(true, true);

    auto responseMsg = makeRespBuf();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t current = NSM_ADAPTIVE_TGPMODE_DISABLED;

    auto rc = encode_get_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL,
                                                      &current, sizeof(current),
                                                      nullptr, 0, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Current decoded as disabled, but the seeded enabled state survives.
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->adaptiveTGPModeEnabled());
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->pendingAdaptiveTGPMode());
}

// Any non-zero current byte reads as enabled (out-of-spec values must not
// silently report disabled).
TEST_F(NsmAdaptiveTGPModeTest, HandleResp_NonZeroCurrentReadsEnabled)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    auto responseMsg = makeRespBuf();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t current = 0x7F;
    uint8_t pending = 0x7F;

    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &current, sizeof(current), &pending,
        sizeof(pending), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->adaptiveTGPModeEnabled());
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->pendingAdaptiveTGPMode());
}

TEST_F(NsmAdaptiveTGPModeTest, HandleResp_ErrorCC_LeavesPropertiesUntouched)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    // Seed a known state so we can prove the error path does not overwrite it.
    sensor.updateReading(true, true);

    auto responseMsg = makeRespBuf();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t current = NSM_ADAPTIVE_TGPMODE_DISABLED;

    auto rc = encode_get_device_mode_settings_v2_resp(0, NSM_ERROR, ERR_NULL,
                                                      &current, sizeof(current),
                                                      nullptr, 0, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->adaptiveTGPModeEnabled());
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->pendingAdaptiveTGPMode());
}

TEST_F(NsmAdaptiveTGPModeTest, HandleResp_DecodeFail)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// updateReading
// ============================================================================

TEST_F(NsmAdaptiveTGPModeTest, UpdateReading_SetsBothProperties)
{
    NsmAdaptiveTGPMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    sensor.updateReading(true, false);
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->adaptiveTGPModeEnabled());
    EXPECT_FALSE(sensor.adaptiveTGPModeIntf->pendingAdaptiveTGPMode());

    sensor.updateReading(false, true);
    EXPECT_FALSE(sensor.adaptiveTGPModeIntf->adaptiveTGPModeEnabled());
    EXPECT_TRUE(sensor.adaptiveTGPModeIntf->pendingAdaptiveTGPMode());
}

// ============================================================================
// createReconfigPermissionsFeature gate (PRC knob 25)
// ============================================================================

struct NsmAdaptiveTGPModeFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmAdaptiveTGPModeFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmAdaptiveTGPModeFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    /** @brief Run the ReconfigPermissions feature of the device `name` */
    requester::Coroutine
        createReconfig(const std::string& name,
                       const std::vector<std::string>& availableKnobs,
                       const std::vector<std::string>& disabledKnobs = {})
    {
        nsm::features::DeviceContext dev;
        dev.name = name;
        dev.kind = "gpu";
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        dev.device = gpu;
        dbus::PropertyMap params{{"Name", name},
                                 {"AvailableKnobs", availableKnobs},
                                 {"DisabledKnobs", disabledKnobs}};
        return nsm::features::createReconfigPermissionsFeature(mockManager, dev,
                                                               params);
    }

    size_t adaptiveTGPModeSensorCount() const
    {
        return std::count_if(gpu->roundRobinSensors.begin(),
                             gpu->roundRobinSensors.end(), [](const auto& s) {
            return std::dynamic_pointer_cast<NsmAdaptiveTGPMode>(s) != nullptr;
        });
    }
};

// "AdaptiveTGPMode" among the available knobs -> the sensor is created.
TEST_F(NsmAdaptiveTGPModeFactoryTest, Created_WhenFeatureAdvertised)
{
    const auto before = gpu->roundRobinSensors.size();
    auto rc = createReconfig("HGX_GPU_ATGP_ON", {"CCMode", "AdaptiveTGPMode"});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // One NsmReconfigPermissions per knob (2) plus the AdaptiveTGPMode
    // sensor itself.
    EXPECT_EQ(gpu->roundRobinSensors.size(), before + 3u);
    EXPECT_EQ(adaptiveTGPModeSensorCount(), 1u);
}

// "AdaptiveTGPMode" absent from the knobs -> the sensor is NOT created.
TEST_F(NsmAdaptiveTGPModeFactoryTest, NotCreated_WhenFeatureAbsent)
{
    auto rc = createReconfig("HGX_GPU_ATGP_OFF", {"CCMode", "ECCEnable"});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(adaptiveTGPModeSensorCount(), 0u);
}

// "AdaptiveTGPMode" available but disabled -> the sensor is NOT created.
TEST_F(NsmAdaptiveTGPModeFactoryTest, NotCreated_WhenFeatureDisabled)
{
    auto rc = createReconfig("HGX_GPU_ATGP_DIS", {"CCMode", "AdaptiveTGPMode"},
                             {"AdaptiveTGPMode"});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(adaptiveTGPModeSensorCount(), 0u);
}

// Empty knob list -> the sensor is NOT created.
TEST_F(NsmAdaptiveTGPModeFactoryTest, NotCreated_WhenFeaturesEmpty)
{
    const auto before = gpu->roundRobinSensors.size();
    auto rc = createReconfig("HGX_GPU_ATGP_EMPTY", {});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(gpu->roundRobinSensors.size(), before);
    EXPECT_EQ(adaptiveTGPModeSensorCount(), 0u);
}

// ============================================================================
// libnsm enum / index regression
// ============================================================================

TEST(AdaptiveTGPModeEnums, DeviceModeIndexAndPrcKnob)
{
    EXPECT_EQ(DEVICE_MODE_ADAPTIVE_TGPMODE, 21);
    EXPECT_EQ(RP_DUAL_PART_NUMBERS, 25);
    EXPECT_EQ(NSM_ADAPTIVE_TGPMODE_DISABLED, 0);
    EXPECT_EQ(NSM_ADAPTIVE_TGPMODE_ENABLED, 1);
}
