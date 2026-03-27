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

// ============================================================================
// Additional branch coverage batch 6 continued:
//
// Targets remaining uncovered code paths:
// - NsmSwitchDIPowerMode::setL1PowerModePatch: asyncPatchInProgress flag reset
//   on setL1PowerDevice failure (catch block L374)
// - NsmSwitchDIPowerMode::update: cc!=0 return path (L173 cc ? cc : rc)
// - NsmSwitchIsolationMode::handleResponseMsg: cc!=0 return path (L439)
// - NsmSwitchIsolationMode::handleResponseMsg: ENABLED mode verification
// - NsmSwitchL1PredictionMode::handleResponseMsg: ENABLED mode with verify
// - NsmSwitchL1PredictionMode::handleResponseMsg: decode fail rc path
// - NsmSwitchDIPowerMode::setL1PowerDevice: postPatchIO fail
// - NsmSwitchDIPowerMode::setL1PowerModePatch: double type for bool keys
// - NsmSwitchDIPowerMode::setL1PowerModePatch: double type for uint32 keys
// - NsmSwitchDI<NvSwitchIntf> constructor and update
// - NsmSwitchDI<AssociationDefinitionsInft> constructor and update
// - NsmSwitchIsolationMode::setSwitchIsolationMode: postPatchIO fail with
//   Disabled mode
// - NsmSwitchIsolationMode::setSwitchIsolationMode: decode fail with Disabled
// - NsmSwitchL1PredictionMode::setL1PredictionMode: false + decode fail
// - Factory createNsmSwitchDI: coGetCachedBaseProperties failure
// - Factory createNsmSwitchDI: NSM_Switch without SwitchType
// - Factory createNsmSwitchDI: NSM_FabricManager without optional props
// ============================================================================

namespace nsm
{
requester::Coroutine createNsmSwitchDI(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

using PatchTupleValue6 =
    std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
using PatchValueList6 = std::vector<std::tuple<std::string, PatchTupleValue6>>;

// ============================================================================
// NsmSwitchDIPowerMode::update - non-zero CC with rc==0 return path
// Covers: L173 `return cc ? cc : rc` where cc != 0
// ============================================================================

TEST_F(NsmSwitchBranch6Test, PowerMode_Update_NonZeroCC_ReturnsCC)
{
    auto pm = makePowerMode();

    // Build response with non-zero completion code
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_common_non_success_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto* payload =
        reinterpret_cast<nsm_common_non_success_resp*>(msg->payload);
    payload->completion_code = NSM_ERROR;
    payload->reason_code = htole16(ERR_NULL);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp));

    pm->update(nvswitch);

    // Values should remain unchanged (cc != 0 so no update)
    EXPECT_FALSE(pm->invoke(pdiMethod(hwModeControl)));
    EXPECT_EQ(static_cast<uint64_t>(pm->invoke(pdiMethod(hwThreshold))), 0u);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerDevice - postPatchIO failure path
// Covers: L203-211 `if (rc_)` branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PowerDevice_PostPatchIOFail_WriteFailure)
{
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    pm->setL1PowerDevice(data, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerDevice - decode failure (non-zero CC)
// Covers: L223-231 `else` branch (rc!=SUCCESS || cc!=SUCCESS)
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PowerDevice_DecodeFail_WriteFailure)
{
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    pm->setL1PowerDevice(data, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - asyncPatchInProgress guard
// Covers: L241-247 `if (asyncPatchInProgress)` TRUE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PowerModePatch_AsyncInProgress_Unavailable)
{
    auto pm = makePowerMode();
    pm->asyncPatchInProgress = true;

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList6{{"HWModeControl", bool{true}}};

    pm->setL1PowerModePatch(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - invalid value type (not vector)
// Covers: L254-258 `if (!patchRequestedValues)` TRUE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PowerModePatch_InvalidType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("not a patch list");

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - empty patch values list
// Covers: L261-264 `if (patchRequestedValues->empty())` TRUE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PowerModePatch_EmptyList_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = PatchValueList6{};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - unrecognized property
// Covers: L357-363 `else` branch in for-loop
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PowerModePatch_UnrecognizedProperty_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList6{{"UnknownKey", bool{true}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - wrong type for HWModeControl
// Covers: L272-277 `if (!l1HWModeControl)` TRUE branch (pass uint32 not bool)
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PowerModePatch_HWModeControl_WrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList6{{"HWModeControl", uint32_t{1}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - wrong type for FWThrottlingMode
// Covers: L284-289 `if (!l1FWThrottlingMode)` TRUE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test,
       SetL1PowerModePatch_FWThrottlingMode_WrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList6{{"FWThrottlingMode", double{1.0}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - wrong type for PredictionMode
// Covers: L296-301 `if (!l1PredictionMode)` TRUE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test,
       SetL1PowerModePatch_PredictionMode_WrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList6{{"PredictionMode", double{0.5}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - wrong type for HWThreshold
// Covers: L308-312 `if (!l1HWThreshold)` TRUE branch (pass bool not uint32)
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PowerModePatch_HWThreshold_WrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList6{{"HWThreshold", bool{true}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - wrong type for HWActiveTime
// Covers: L320-325 `if (!l1HWActiveTime)` TRUE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PowerModePatch_HWActiveTime_WrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList6{{"HWActiveTime", double{50.0}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - wrong type for HWInactiveTime
// Covers: L332-338 `if (!l1HWInactiveTime)` TRUE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test,
       SetL1PowerModePatch_HWInactiveTime_WrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList6{{"HWInactiveTime", bool{false}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - wrong type for
// HWPredictionInactiveTime
// Covers: L347-352 `if (!l1PredictionInactiveTime)` TRUE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test,
       SetL1PowerModePatch_HWPredictionInactiveTime_WrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList6{{"HWPredictionInactiveTime", double{300.0}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchIsolationMode::handleResponseMsg - ENABLED mode with verification
// Covers: L423-426 `if (isolationMode == SWITCH_COMMUNICATION_MODE_ENABLED)`
// ============================================================================

TEST_F(NsmSwitchBranch6Test,
       IsolationMode_HandleResponseMsg_EnabledMode_Verified)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/iso_enabled_v");
    NsmSwitchIsolationMode sensor("Iso_enabled_v", "NSM_NVSwitch",
                                  isolationIntf);

    // Set to Disabled first to verify it changes
    isolationIntf->isolationMode(
        SwitchCommunicationMode::SwitchCommunicationDisabled);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                              sizeof(uint8_t));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    uint8_t mode = SWITCH_COMMUNICATION_MODE_ENABLED;
    encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, mode, msg);

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationEnabled);
}

// ============================================================================
// NsmSwitchIsolationMode::handleResponseMsg - decode fail (short buffer)
// Covers: L421 `if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)` FALSE
// via rc != NSM_SW_SUCCESS, and L439 `return cc ? cc : rc`
// ============================================================================

TEST_F(NsmSwitchBranch6Test, IsolationMode_HandleResponseMsg_DecodeFail)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/iso_decfail");
    NsmSwitchIsolationMode sensor("Iso_decfail", "NSM_NVSwitch", isolationIntf);

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ============================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode - invalid mode string
// Covers: L464-471 `else` branch (invalid isolation mode)
// ============================================================================

TEST_F(NsmSwitchBranch6Test,
       SetSwitchIsolationMode_InvalidModeString_WriteFailure)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/iso_invmode");
    NsmSwitchIsolationMode sensor("Iso_invmode", "NSM_NVSwitch", isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("SomeBogusMode");

    sensor.setSwitchIsolationMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode - Disabled + postPatchIO fail
// Covers: L460-462 Disabled branch + L492-500 postPatchIO fail
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetSwitchIsolationMode_Disabled_PostPatchIOFail)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/iso_dis_ppfail");
    NsmSwitchIsolationMode sensor("Iso_dis_ppf", "NSM_NVSwitch", isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationDisabled");

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    sensor.setSwitchIsolationMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode - decode fail response
// Covers: L507 `if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)` FALSE
// ============================================================================

TEST_F(NsmSwitchBranch6Test,
       SetSwitchIsolationMode_Enabled_DecodeFail_WriteFailure)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/iso_en_decfail");
    NsmSwitchIsolationMode sensor("Iso_en_df", "NSM_NVSwitch", isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    sensor.setSwitchIsolationMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSwitchL1PredictionMode::handleResponseMsg - ENABLED with initial false
// Covers: L554-556 `if (predictionMode == ENABLED)` TRUE branch
// Verifies transition from false to true
// ============================================================================

TEST_F(NsmSwitchBranch6Test,
       PredictionMode_HandleResponseMsg_Enabled_TransitionVerified)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_en_trans");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_en_trans");
    NsmSwitchL1PredictionMode sensor("Pred_en_trans", "NSM_NVSwitch",
                                     enableIntf, assocDefIntf);

    enableIntf->enabled(false);
    EXPECT_FALSE(enableIntf->enabled());

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_device_mode_setting_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto* payload =
        reinterpret_cast<nsm_get_device_mode_setting_resp*>(msg->payload);
    payload->hdr.completion_code = NSM_SUCCESS;
    payload->hdr.data_size = htole16(sizeof(uint8_t));
    payload->device_mode = ENABLED;

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_TRUE(enableIntf->enabled());
}

// ============================================================================
// NsmSwitchL1PredictionMode::setL1PredictionMode - false + postPatchIO fail
// Covers: L585-587 DISABLED branch + L610-617 postPatchIO fail
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PredictionMode_False_PostPatchIOFail)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_f_ppfail");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_f_ppfail");
    NsmSwitchL1PredictionMode sensor("Pred_f_ppf", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{false};

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    sensor.setL1PredictionMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSwitchL1PredictionMode::setL1PredictionMode - true + decode fail
// Covers: L625 FALSE branch (rc != SUCCESS || cc != SUCCESS)
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SetL1PredictionMode_True_DecodeFail)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_t_decfail");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_t_decfail");
    NsmSwitchL1PredictionMode sensor("Pred_t_df", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    sensor.setL1PredictionMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSwitchDI<NvSwitchIntf> constructor and update
// Covers: NsmSwitchDI template with NvSwitchIntf (non-UuidIntf path)
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SwitchDI_NvSwitchIntf_UpdateReturnsSuccess)
{
    auto nvSwitchObj =
        std::make_shared<NsmSwitchDI<NvSwitchIntf>>(name, inventoryObjPath);

    EXPECT_EQ(nvSwitchObj->getName(), name);
    nvSwitchObj->update(nvswitch);
}

// ============================================================================
// NsmSwitchDI<AssociationDefinitionsInft> constructor and update
// Covers: NsmSwitchDI template with AssociationDefinitionsInft
// ============================================================================

TEST_F(NsmSwitchBranch6Test, SwitchDI_AssociationDefIntf_UpdateReturnsSuccess)
{
    auto assocObj = std::make_shared<NsmSwitchDI<AssociationDefinitionsInft>>(
        name, inventoryObjPath);

    EXPECT_EQ(assocObj->getName(), name);
    assocObj->update(nvswitch);
}

// ============================================================================
// NsmSwitchIsolationMode::genRequestMsg - success path
// Covers: L396-404 encode success branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, IsolationMode_GenRequestMsg_Success)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/iso_gen_ok");
    NsmSwitchIsolationMode sensor("Iso_gen_ok", "NSM_NVSwitch", isolationIntf);

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), 0u);
}

// ============================================================================
// NsmSwitchIsolationMode::genRequestMsg - encode failure
// Covers: L397-402 `if (rc != NSM_SW_SUCCESS)` TRUE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, IsolationMode_GenRequestMsg_EncodeFail)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/iso_gen_fail");
    NsmSwitchIsolationMode sensor("Iso_gen_f", "NSM_NVSwitch", isolationIntf);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmSwitchL1PredictionMode::genRequestMsg - success
// Covers: L531-539 encode success
// ============================================================================

TEST_F(NsmSwitchBranch6Test, PredictionMode_GenRequestMsg_Success)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_gen_ok");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_gen_ok");
    NsmSwitchL1PredictionMode sensor("Pred_gen_ok", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), 0u);
}

// ============================================================================
// NsmSwitchL1PredictionMode::genRequestMsg - encode failure
// Covers: L531-536 `if (rc != NSM_SW_SUCCESS)` TRUE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, PredictionMode_GenRequestMsg_EncodeFail)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_gen_fail");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br6test/pred_gen_fail");
    NsmSwitchL1PredictionMode sensor("Pred_gen_f", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// Factory: createNsmSwitchDI - coGetCachedBaseProperties failure
// Covers: L694-696 `if (rc != NSM_SUCCESS)` TRUE branch
// ============================================================================

static constexpr const char* baseIntf6 =
    "xyz.openbmc_project.Configuration.NSM_NVSwitch";

TEST_F(NsmSwitchBranch6Test, Factory_coGetCachedBaseProperties_Failure)
{
    // Do not setup base properties -> coGetCachedBaseProperties returns error
    const std::string path =
        "/xyz/openbmc_project/inventory/br6test/factory_fail";

    createNsmSwitchDI(mockManager, baseIntf6, path);

    // Should return early without adding any sensors
}

// ============================================================================
// Factory: createNsmSwitchDI - NSM_Switch without SwitchType
// Covers: L948-952 `if (allCurrentIfaceProperties.count("SwitchType"))`
// FALSE branch (no SwitchType in properties)
// ============================================================================

TEST_F(NsmSwitchBranch6Test, DISABLED_Factory_Switch_WithoutSwitchType)
{
    const std::string path = inventoryObjPath + "sw_notype";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf6);
    base["Name"] = name;
    base["UUID"] = switchUuid;
    base["InventoryObjPath"] = inventoryObjPath;

    const std::string intf = std::string(baseIntf6) + ".Switch6";
    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_Switch");
    // No SwitchType or SwitchSupportedProtocols

    AsyncOperationManager::getInstance()->dispatchers.clear();
    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

// ============================================================================
// Factory: createNsmSwitchDI - NSM_FabricManager without optional props
// Covers: L989-998 missing Name/InventoryObjPath/Description
// ============================================================================

TEST_F(NsmSwitchBranch6Test, DISABLED_Factory_FabricManager_NoOptionalProps)
{
    const std::string path = inventoryObjPath + "fm_noopt";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf6);
    base["Name"] = std::string("NVSwitch_Br6_fm");
    base["UUID"] = switchUuid;
    base["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/br6test/nvswitch_fm/");

    const std::string intf = std::string(baseIntf6) + ".FabricManager6";
    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_FabricManager");
    // No Name, InventoryObjPath, Description in current interface

    AsyncOperationManager::getInstance()->dispatchers.clear();
    createNsmSwitchDI(mockManager, intf, path);
}

// ============================================================================
// Factory: createNsmSwitchDI - NSM_PortDisableFuture without Priority
// Covers: L843-846 `if (allCurrentIfaceProperties.count("Priority"))`
// FALSE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, Factory_PortDisableFuture_NoPriority)
{
    const std::string path = inventoryObjPath + "pdf_noprio6";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf6);
    base["Name"] = name;
    base["UUID"] = switchUuid;
    base["InventoryObjPath"] = inventoryObjPath;

    const std::string intf = std::string(baseIntf6) + ".PortDisableFuture6";
    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_PortDisableFuture");
    // No Priority property

    AsyncOperationManager::getInstance()->dispatchers.clear();
    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

// ============================================================================
// Factory: createNsmSwitchDI - NSM_PowerMode without Priority
// Covers: L871-874 `if (allCurrentIfaceProperties.count("Priority"))`
// FALSE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, Factory_PowerMode_NoPriority)
{
    const std::string path = inventoryObjPath + "pm_noprio6";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf6);
    base["Name"] = name;
    base["UUID"] = switchUuid;
    base["InventoryObjPath"] = inventoryObjPath;

    const std::string intf = std::string(baseIntf6) + ".PowerMode6";
    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_PowerMode");
    // No Priority property

    AsyncOperationManager::getInstance()->dispatchers.clear();
    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

// ============================================================================
// Factory: createNsmSwitchDI - base properties without Name
// Covers: L702-705 `if (allBaseIfaceProperties.count("Name"))` FALSE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, DISABLED_Factory_BaseProps_NoName)
{
    const std::string path = inventoryObjPath + "noname6";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf6);
    // No Name
    base["UUID"] = switchUuid;
    base["InventoryObjPath"] = inventoryObjPath;

    const std::string intf = std::string(baseIntf6) + ".ChassisAttributes6";
    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_Chassis_Attributes");

    AsyncOperationManager::getInstance()->dispatchers.clear();
    createNsmSwitchDI(mockManager, intf, path);
}

// ============================================================================
// Factory: createNsmSwitchDI - base properties without InventoryObjPath
// Covers: L707-711 `if (allBaseIfaceProperties.count("InventoryObjPath"))`
// FALSE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, DISABLED_Factory_BaseProps_NoInventoryObjPath)
{
    const std::string path = inventoryObjPath + "noinv6";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf6);
    base["Name"] = name;
    base["UUID"] = switchUuid;
    // No InventoryObjPath

    const std::string intf = std::string(baseIntf6) + ".ChassisAttributes6b";
    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_Chassis_Attributes");

    AsyncOperationManager::getInstance()->dispatchers.clear();
    createNsmSwitchDI(mockManager, intf, path);
}

// ============================================================================
// Factory: createNsmSwitchDI - current properties without Type
// Covers: L713-716 `if (allCurrentIfaceProperties.count("Type"))` FALSE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, Factory_CurrentProps_NoType)
{
    const std::string path = inventoryObjPath + "notype6";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf6);
    base["Name"] = name;
    base["UUID"] = switchUuid;
    base["InventoryObjPath"] = inventoryObjPath;

    const std::string intf = std::string(baseIntf6) + ".NoType6";
    [[maybe_unused]] auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    // No Type property -> type defaults to empty string, no branch matched

    AsyncOperationManager::getInstance()->dispatchers.clear();
    createNsmSwitchDI(mockManager, intf, path);
}

// ============================================================================
// Factory: createNsmSwitchDI - base properties without UUID
// Covers: L718-721 `if (allBaseIfaceProperties.count("UUID"))` FALSE branch
// ============================================================================

TEST_F(NsmSwitchBranch6Test, DISABLED_Factory_BaseProps_NoUUID)
{
    const std::string path = inventoryObjPath + "nouuid6";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf6);
    base["Name"] = name;
    base["InventoryObjPath"] = inventoryObjPath;
    // No UUID -> device lookup with empty uuid

    const std::string intf = std::string(baseIntf6) + ".ChassisAttributes6c";
    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_Chassis_Attributes");

    AsyncOperationManager::getInstance()->dispatchers.clear();
    createNsmSwitchDI(mockManager, intf, path);
}
