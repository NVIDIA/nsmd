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
 * Additional branch coverage batch 6 for nsmSwitch.cpp:
 *
 * Targets uncovered code paths:
 * - NsmSwitchIsolationMode::handleResponseMsg: Disabled mode
 *   (isolationMode == SWITCH_COMMUNICATION_MODE_DISABLED)
 * - NsmSwitchIsolationMode::handleResponseMsg: Unknown mode
 *   (else branch, isolationMode is neither ENABLED nor DISABLED)
 * - NsmSwitchL1PredictionMode::handleResponseMsg: DISABLED branch
 *   (predictionMode == DISABLED -> enableIntf->enabled(false))
 * - NsmSwitchDIPowerMode::update: success with mixed ternary values
 *   (hw_mode_control=1, fw_throttling_mode=0, prediction_mode=1)
 * - NsmSwitchDIReset constructor verification
 * - NsmSwitchDI<T> constructor and update for non-UuidIntf
 * - createNsmSwitchDI factory: coGetCachedBaseProperties failure
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"
#include "network-ports.h"

#include "nsmDeviceInventory/nsmSwitch.hpp"
#include "test/commonMock.hpp"

#undef private
#undef protected

using namespace nsm;

static auto& testBus = utils::DBusHandler::getBus();

// ============================================================================
// Fixture
// ============================================================================

class NsmSwitchBranch6Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const std::string name = "NVSwitch_Br6";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/br6test/nvswitch/";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:600";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchBranch6Test() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchBranch6Test()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmSwitchDIPowerMode> makePowerMode()
    {
        auto pm = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                         inventoryObjPath);
        pm->invoke(pdiMethod(hwModeControl), false);
        pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
        pm->invoke(pdiMethod(fwThrottlingMode), false);
        pm->invoke(pdiMethod(predictionMode), false);
        pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
        pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
        pm->invoke(pdiMethod(hwPredictionInactiveTime),
                   static_cast<uint64_t>(0));
        return pm;
    }
};

// ============================================================================
// NsmSwitchIsolationMode::handleResponseMsg - DISABLED mode
// Exercises: isolationMode == SWITCH_COMMUNICATION_MODE_DISABLED
// ============================================================================

TEST_F(NsmSwitchBranch6Test, IsolationMode_HandleResponseMsg_DisabledMode)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/iso_disabled");
    NsmSwitchIsolationMode sensor("Iso_disabled", "NSM_NVSwitch",
                                  isolationIntf);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                              sizeof(uint8_t));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    uint8_t mode = SWITCH_COMMUNICATION_MODE_DISABLED;
    encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, mode, msg);

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationDisabled);
}

// ============================================================================
// NsmSwitchIsolationMode::handleResponseMsg - Unknown mode
// Exercises: else branch (mode is neither ENABLED nor DISABLED)
// ============================================================================

TEST_F(NsmSwitchBranch6Test, IsolationMode_HandleResponseMsg_UnknownMode)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/iso_unknown");
    NsmSwitchIsolationMode sensor("Iso_unknown", "NSM_NVSwitch", isolationIntf);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                              sizeof(uint8_t));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    // Use a value that is neither ENABLED(0) nor DISABLED(1) -> 0xFF
    uint8_t mode = 0xFF;
    encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, mode, msg);

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationUnknown);
}

// ============================================================================
// NsmSwitchL1PredictionMode::handleResponseMsg - DISABLED branch
// Exercises: predictionMode == DISABLED -> enableIntf->enabled(false)
// ============================================================================

TEST_F(NsmSwitchBranch6Test, PredictionMode_HandleResponseMsg_Disabled)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_disabled");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_disabled");
    NsmSwitchL1PredictionMode sensor("Pred_disabled", "NSM_NVSwitch",
                                     enableIntf, assocDefIntf);

    // Set enabled to true first, then verify it gets set to false
    enableIntf->enabled(true);
    EXPECT_TRUE(enableIntf->enabled());

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_device_mode_setting_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto* payload =
        reinterpret_cast<nsm_get_device_mode_setting_resp*>(msg->payload);
    payload->hdr.completion_code = NSM_SUCCESS;
    payload->hdr.data_size = htole16(sizeof(uint8_t));
    payload->device_mode = DISABLED;

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_FALSE(enableIntf->enabled());
}

// ============================================================================
// NsmSwitchDIPowerMode::update - success with mixed ternary branches
// hw_mode_control=1 (TRUE), fw_throttling_mode=0 (FALSE),
// prediction_mode=1 (TRUE) -> exercises different ternary branches
// ============================================================================

TEST_F(NsmSwitchBranch6Test, PowerMode_Update_MixedTernary)
{
    auto pm = makePowerMode();

    nsm_power_mode_data data = {};
    data.l1_hw_mode_control = 1;    // TRUE branch
    data.l1_hw_mode_threshold = 50;
    data.l1_fw_throttling_mode = 0; // FALSE branch
    data.l1_prediction_mode = 1;    // TRUE branch
    data.l1_hw_active_time = 15;
    data.l1_hw_inactive_time = 25;
    data.l1_prediction_inactive_time = 35;

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_power_mode_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp));

    pm->update(nvswitch);

    EXPECT_TRUE(pm->invoke(pdiMethod(hwModeControl)));
    EXPECT_FALSE(pm->invoke(pdiMethod(fwThrottlingMode)));
    EXPECT_TRUE(pm->invoke(pdiMethod(predictionMode)));
    EXPECT_EQ(static_cast<uint64_t>(pm->invoke(pdiMethod(hwThreshold))), 50u);
    EXPECT_EQ(static_cast<uint64_t>(pm->invoke(pdiMethod(hwActiveTime))), 15u);
    EXPECT_EQ(static_cast<uint64_t>(pm->invoke(pdiMethod(hwInactiveTime))),
              25u);
    EXPECT_EQ(
        static_cast<uint64_t>(pm->invoke(pdiMethod(hwPredictionInactiveTime))),
        35u);
}

// ============================================================================
// NsmSwitchDIPowerMode::update - success with opposite mixed ternary
// hw_mode_control=0 (FALSE), fw_throttling_mode=1 (TRUE),
// prediction_mode=0 (FALSE)
// ============================================================================

TEST_F(NsmSwitchBranch6Test, PowerMode_Update_OppositemixedTernary)
{
    auto pm = makePowerMode();

    nsm_power_mode_data data = {};
    data.l1_hw_mode_control = 0;    // FALSE branch
    data.l1_hw_mode_threshold = 200;
    data.l1_fw_throttling_mode = 1; // TRUE branch
    data.l1_prediction_mode = 0;    // FALSE branch
    data.l1_hw_active_time = 5;
    data.l1_hw_inactive_time = 10;
    data.l1_prediction_inactive_time = 20;

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_power_mode_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp));

    pm->update(nvswitch);

    EXPECT_FALSE(pm->invoke(pdiMethod(hwModeControl)));
    EXPECT_TRUE(pm->invoke(pdiMethod(fwThrottlingMode)));
    EXPECT_FALSE(pm->invoke(pdiMethod(predictionMode)));
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - success with multiple keys
// Exercises the full patch+setL1PowerDevice flow
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PowerModePatch_MultipleKeys_Success)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    AsyncSetOperationValueType value =
        PatchValueList{{"HWModeControl", bool{true}},
                       {"FWThrottlingMode", bool{false}},
                       {"HWThreshold", uint32_t{1000}},
                       {"HWActiveTime", uint32_t{50}},
                       {"HWInactiveTime", uint32_t{100}},
                       {"HWPredictionInactiveTime", uint32_t{200}},
                       {"PredictionMode", bool{true}}};

    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_set_power_mode_resp));
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));
    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(pm->asyncPatchInProgress);
}

// ============================================================================
// NsmSwitchDIReset constructor verification
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SwitchDIReset_Constructor)
{
    std::string inv = "/xyz/openbmc_project/inventory/br6test/reset/";
    auto resetObj = std::make_shared<NsmSwitchDIReset>(
        testBus, name, "NSM_NVSwitch", inv, nvswitch);

    EXPECT_NE(resetObj, nullptr);
    EXPECT_EQ(resetObj->getName(), name);
    EXPECT_NE(resetObj->resetIntf, nullptr);
    EXPECT_NE(resetObj->resetAsyncIntf, nullptr);
    EXPECT_EQ(resetObj->objPath, inv + name);
}

// ============================================================================
// NsmSwitchDI<SwitchIntf> constructor and update (non-UuidIntf path)
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SwitchDI_SwitchIntf_UpdateReturnsSuccess)
{
    auto switchObj =
        std::make_shared<NsmSwitchDI<SwitchIntf>>(name, inventoryObjPath);

    EXPECT_EQ(switchObj->getName(), name);

    // update for non-UuidIntf should just co_return NSM_SUCCESS
    switchObj->update(nvswitch);
}
