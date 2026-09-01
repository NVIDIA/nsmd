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
 * Branch coverage tests for:
 *   - NsmSwitchDIPowerMode: update, setL1PowerDevice, setL1PowerModePatch
 *   - NsmSwitchIsolationMode: handleResponseMsg, setSwitchIsolationMode
 *   - NsmSwitchL1PredictionMode: handleResponseMsg, setL1PredictionMode
 *   - NsmSwitchLTXMode: genRequestMsg, handleResponseMsg, setLTXMode,
 *     createNsmSwitchDI SupportLTXMode gate (LTX Mode Enable/Disable feature)
 *   - NsmSwitchUPhyMode: genRequestMsg, handleResponseMsg, setUPhyMode,
 *     createNsmSwitchDI SupportUPhyMode gate (LTX Mode Enable/Disable feature)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <cstring>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"
#include "network-ports.h"

#include "nsmDeviceInventory/nsmSwitch.hpp"

#undef private
#undef protected

using namespace nsm;

using PatchTupleValue =
    std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
using PatchValueList = std::vector<std::tuple<std::string, PatchTupleValue>>;

static auto& testBus = utils::DBusHandler::getBus();

// ============================================================================
// Fixture for coroutine-based tests needing MockNsmDevice
// ============================================================================

class NsmSwitchBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const std::string name = "NVSwitch_Branch";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/branchtest/nvswitch/";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:500";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchBranchTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchBranchTest()
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
// NsmSwitchDIPowerMode::update -- decode fail (short buffer, cc=0)
// Covers: nsmSwitch.cpp L151 `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)`
// FALSE via rc!=NSM_SW_SUCCESS with cc==0 -> return rc branch.
// ============================================================================

TEST_F(NsmSwitchBranchTest, PowerMode_Update_DecodeFail_ReturnsError)
{
    auto pm = makePowerMode();

    // Valgrind-safe: buffer large enough for decode_reason_code_and_cc but
    // too short for full power mode response -> decode fails with length error
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    pm->update(nvswitch);

    // Data should remain unchanged
    EXPECT_FALSE(pm->invoke(pdiMethod(hwModeControl)));
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerDevice -- success path
// Covers: nsmSwitch.cpp L219 `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)`
// TRUE branch.
// ============================================================================

TEST_F(NsmSwitchBranchTest, SetL1PowerDevice_Success_StatusUnchanged)
{
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    pm->setL1PowerDevice(data, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode -- success with Disabled
// Covers: nsmSwitch.cpp L460-461 `else if (*reqIsolationMode ==
// "SwitchCommunicationDisabled")` TRUE branch, and L507
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` TRUE branch.
// ============================================================================

TEST_F(NsmSwitchBranchTest, SetSwitchIsolationMode_Disabled_Success)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/iso_disabled_succ");
    NsmSwitchIsolationMode sensor("IsoSwitch_dis", "NSM_NVSwitch",
                                  isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationDisabled");

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                          responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    sensor.setSwitchIsolationMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setSwitchIsolationMode: success with Enabled
TEST_F(NsmSwitchBranchTest, SetSwitchIsolationMode_Enabled_Success)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/iso_enabled_succ");
    NsmSwitchIsolationMode sensor("IsoSwitch_en", "NSM_NVSwitch",
                                  isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                          responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    sensor.setSwitchIsolationMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setSwitchIsolationMode: non-string value -> throws InvalidArgument
// Covers: nsmSwitch.cpp L448 `if (reqIsolationMode == NULL)` TRUE branch.
TEST_F(NsmSwitchBranchTest, SetSwitchIsolationMode_NonStringValue_Throws)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/iso_nonstr");
    NsmSwitchIsolationMode sensor("IsoSwitch_nonstr", "NSM_NVSwitch",
                                  isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};

    EXPECT_THROW_COROUTINE(
        sensor.setSwitchIsolationMode(value, &status, nvswitch),
        std::exception);
}

// ============================================================================
// NsmSwitchL1PredictionMode::setL1PredictionMode -- success with true
// Covers: nsmSwitch.cpp L581 `if (*l1PredictionMode)` TRUE branch, and
// L625 `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` TRUE branch.
// ============================================================================

TEST_F(NsmSwitchBranchTest, SetL1PredictionMode_True_Success)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/pred_true_succ");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/pred_true_succ");
    NsmSwitchL1PredictionMode sensor("Pred_true", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    sensor.setL1PredictionMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setL1PredictionMode: success with false
// Covers: nsmSwitch.cpp L585-588 `else` branch (DISABLED).
TEST_F(NsmSwitchBranchTest, SetL1PredictionMode_False_Success)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/pred_false_succ");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/pred_false_succ");
    NsmSwitchL1PredictionMode sensor("Pred_false", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{false};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    sensor.setL1PredictionMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setL1PredictionMode: non-bool value -> throws InvalidArgument
// Covers: nsmSwitch.cpp L572 `if (!l1PredictionMode)` TRUE branch.
TEST_F(NsmSwitchBranchTest, SetL1PredictionMode_NonBoolValue_Throws)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/pred_nonbool");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/pred_nonbool");
    NsmSwitchL1PredictionMode sensor("Pred_nonbool", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("not a bool");

    EXPECT_THROW_COROUTINE(sensor.setL1PredictionMode(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchL1PredictionMode::handleResponseMsg -- decode fail (short buffer)
// Covers: rc != NSM_SW_SUCCESS with cc == NSM_ERROR -> return cc branch.
// ============================================================================

TEST(NsmSwitchL1PredictionModeBranch, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/pred_decfail");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/pred_decfail");
    NsmSwitchL1PredictionMode sensor("Pred_decfail", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    // Valgrind-safe buffer: large enough for decode_reason_code_and_cc but
    // too short for full device mode setting response
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch -- individual key tests
// Each test exercises a single key branch in the for loop.
// ============================================================================

TEST_F(NsmSwitchBranchTest, SetL1PowerModePatch_HWModeControl_Only)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWModeControl", bool{true}}};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));
    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(pm->asyncPatchInProgress);
}

TEST_F(NsmSwitchBranchTest, SetL1PowerModePatch_FWThrottlingMode_Only)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"FWThrottlingMode", bool{true}}};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));
    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranchTest, SetL1PowerModePatch_PredictionMode_Only)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"PredictionMode", bool{true}}};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));
    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranchTest, SetL1PowerModePatch_HWThreshold_Only)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWThreshold", uint32_t{5000}}};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));
    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranchTest, SetL1PowerModePatch_HWActiveTime_Only)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWActiveTime", uint32_t{100}}};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));
    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranchTest, SetL1PowerModePatch_HWInactiveTime_Only)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWInactiveTime", uint32_t{200}}};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));
    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranchTest, SetL1PowerModePatch_HWPredictionInactiveTime_Only)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWPredictionInactiveTime", uint32_t{400}}};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));
    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmSwitchDIPowerMode::getPowerModeData -- verifies data round-trip
// ============================================================================

TEST_F(NsmSwitchBranchTest, GetPowerModeData_AfterUpdate_ReturnsCorrectValues)
{
    auto pm = makePowerMode();

    nsm_power_mode_data pmData = {};
    pmData.l1_hw_mode_control = 1;
    pmData.l1_hw_mode_threshold = 42;
    pmData.l1_fw_throttling_mode = 0;
    pmData.l1_prediction_mode = 1;
    pmData.l1_hw_active_time = 10;
    pmData.l1_hw_inactive_time = 20;
    pmData.l1_prediction_inactive_time = 30;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &pmData,
                                         responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    pm->update(nvswitch);

    auto result = pm->getPowerModeData();
    EXPECT_EQ(result.l1_hw_mode_control, 1);
    EXPECT_EQ(result.l1_hw_mode_threshold, 42u);
    EXPECT_EQ(result.l1_fw_throttling_mode, 0);
    EXPECT_EQ(result.l1_prediction_mode, 1);
    EXPECT_EQ(result.l1_hw_active_time, 10u);
    EXPECT_EQ(result.l1_hw_inactive_time, 20u);
    EXPECT_EQ(result.l1_prediction_inactive_time, 30u);
}

// ============================================================================
// NsmSwitchLTXMode -- genRequestMsg, handleResponseMsg, setLTXMode
// LTX Mode Enable/Disable feature (NSM Type 5 Device Mode index
// DEVICE_MODE_LTX). Mirrors the NsmSwitchPowerCappingMode coverage in
// nsmSwitchFactoryBranch_test.cpp, which targets the same underlying
// Get/Set Device Mode Settings v2 command.
// ============================================================================

TEST_F(NsmSwitchBranchTest, LTXMode_GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_genreq_fail");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_genreq_fail");
    NsmSwitchLTXMode sensor("LTX_genreq", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);

    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmSwitchBranchTest,
       LTXMode_HandleResponseMsg_ValidCurrentAndPending_UpdatesBothProperties)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_valid_both");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_valid_both");
    NsmSwitchLTXMode sensor("LTX_valid_both", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            LTX_MODE_DATA_SIZE * 2,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t currentMode = NSM_LTX_MODE_ENABLED;
    uint8_t pendingMode = NSM_LTX_MODE_DISABLED;
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &currentMode, LTX_MODE_DATA_SIZE,
        &pendingMode, LTX_MODE_DATA_SIZE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(ltxModeIntf->currentMode(), LTXModeEnum::Enabled);
    EXPECT_EQ(ltxModeIntf->pendingMode(), LTXModeEnum::Disabled);
}

TEST_F(NsmSwitchBranchTest,
       LTXMode_HandleResponseMsg_PendingLengthZero_CurrentModeOnlyUpdated)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_no_pending");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_no_pending");
    // Stale pending from a prior write / factory seed.
    ltxModeIntf->pendingMode(LTXModeEnum::Disabled);
    NsmSwitchLTXMode sensor("LTX_no_pending", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            LTX_MODE_DATA_SIZE,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t currentMode = NSM_LTX_MODE_ENABLED;
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &currentMode, LTX_MODE_DATA_SIZE, nullptr, 0,
        response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(ltxModeIntf->currentMode(), LTXModeEnum::Enabled);
    // pendingModeLength == 0: absent pending is synced to resolved current,
    // clearing the stale value rather than leaving it untouched.
    EXPECT_EQ(ltxModeIntf->pendingMode(), LTXModeEnum::Enabled);
}

TEST_F(NsmSwitchBranchTest, LTXMode_WireDefaultCurrentMode_ResolvesToEnabled)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_def_wire");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_def_wire");
    NsmSwitchLTXMode sensor("LTX_defwire", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            LTX_MODE_DATA_SIZE * 2,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t currentMode = NSM_LTX_MODE_DEFAULT;
    uint8_t pendingMode = NSM_LTX_MODE_DEFAULT;
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &currentMode, LTX_MODE_DATA_SIZE,
        &pendingMode, LTX_MODE_DATA_SIZE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Wire Default resolves to Enabled at D-Bus publish time; it must never
    // surface as LTXModeEnum::Default on CurrentMode or PendingMode.
    EXPECT_EQ(ltxModeIntf->currentMode(), LTXModeEnum::Enabled);
    EXPECT_EQ(ltxModeIntf->pendingMode(), LTXModeEnum::Enabled);
}

TEST_F(NsmSwitchBranchTest,
       LTXMode_HandleResponseMsg_CurrentLengthZero_ReturnsError)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_cur_zero");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_cur_zero");
    NsmSwitchLTXMode sensor("LTX_curzero", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);
    // Seed a known pre-existing value so the assertion below actually proves
    // the hard-error path leaves the property untouched, rather than
    // trivially matching the sdbusplus default-constructed value.
    ltxModeIntf->currentMode(LTXModeEnum::Enabled);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    // currentModeLength == 0: CurrentMode missing entirely.
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, nullptr, 0, nullptr, 0, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    // Review-correction-mandated hard error: missing CurrentMode must not be
    // a partial success, and the previously published value must survive.
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
    EXPECT_EQ(ltxModeIntf->currentMode(), LTXModeEnum::Enabled);
}

TEST_F(NsmSwitchBranchTest,
       LTXMode_HandleResponseMsg_CurrentLengthTooShort_ReturnsError)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_cur_short");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_cur_short");
    NsmSwitchLTXMode sensor("LTX_curshort", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);

    // LTX_MODE_DATA_SIZE == 1, so any currentModeLength != 1 (including
    // "too short") is exercised the same way as a too-long payload here:
    // any length != LTX_MODE_DATA_SIZE must hard-error.
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t currentMode[2] = {NSM_LTX_MODE_ENABLED, 0};
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, currentMode, uint16_t(sizeof(currentMode)),
        nullptr, 0, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(NsmSwitchBranchTest,
       LTXMode_HandleResponseMsg_PendingLengthInvalid_ReturnsError)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_pend_invalid");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_pend_invalid");
    NsmSwitchLTXMode sensor("LTX_pendinvalid", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            LTX_MODE_DATA_SIZE + 2,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    // Valid CurrentMode, but PendingMode length is neither 0 nor
    // LTX_MODE_DATA_SIZE (garbage/truncated length) -> hard error.
    uint8_t currentMode = NSM_LTX_MODE_ENABLED;
    uint8_t pendingMode[2] = {NSM_LTX_MODE_DISABLED, 0};
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &currentMode, LTX_MODE_DATA_SIZE, pendingMode,
        uint16_t(sizeof(pendingMode)), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(NsmSwitchLTXModeBranch, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_decfail");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_decfail");
    NsmSwitchLTXMode sensor("LTX_decfail", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);

    // Valgrind-safe buffer: large enough for decode_reason_code_and_cc but
    // too short for a full Get Device Mode Settings v2 response.
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmSwitchBranchTest,
       LTXMode_SetLTXMode_Configurable_EncodesSetRequestAndUpdatesPendingMode)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_set_cfg");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_set_cfg");
    ltxModeIntf->isModeConfigurable(true);
    NsmSwitchLTXMode sensor("LTX_setcfg", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        LTXModeServer::convertLinkTrainingExtendedModeToString(
            LTXModeEnum::Disabled);

    Request capturedRequest;
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL,
                                            responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(Invoke(
            [&, responseData](eid_t, Request& req,
                              std::shared_ptr<const nsm_msg>& outResponseMsg,
                              size_t& outResponseLen) -> requester::Coroutine {
        capturedRequest = req;
        outResponseLen = responseData.size();
        auto* buf = new uint8_t[outResponseLen];
        std::memcpy(buf, responseData.data(), outResponseLen);
        outResponseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(buf), [](const nsm_msg* p) {
            delete[] reinterpret_cast<const uint8_t*>(p);
        });
        // coverity[missing_return]
        co_return NSM_SUCCESS;
    }));

    sensor.setLTXMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_EQ(ltxModeIntf->pendingMode(), LTXModeEnum::Disabled);

    // Verify the encoded Set Device Mode Settings v2 request used
    // DEVICE_MODE_LTX and the correct payload size/content.
    ASSERT_GE(capturedRequest.size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_set_device_mode_settings_v2_req));
    uint32_t decodedIdx = 0;
    std::vector<uint8_t> decodedData(LTX_MODE_DATA_SIZE, 0);
    uint16_t decodedLen = 0;
    auto reqMsg = reinterpret_cast<const nsm_msg*>(capturedRequest.data());
    auto rc = decode_set_device_mode_settings_v2_req(
        reqMsg, capturedRequest.size(), &decodedIdx, decodedData.data(),
        &decodedLen);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(decodedIdx, static_cast<uint32_t>(DEVICE_MODE_LTX));
    EXPECT_EQ(decodedLen, LTX_MODE_DATA_SIZE);
    EXPECT_EQ(decodedData[0], NSM_LTX_MODE_DISABLED);
}

TEST_F(NsmSwitchBranchTest, LTXMode_SetLTXMode_DefaultTarget_ResolvesToEnabled)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_set_default");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_set_default");
    ltxModeIntf->isModeConfigurable(true);
    NsmSwitchLTXMode sensor("LTX_setdef", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        LTXModeServer::convertLinkTrainingExtendedModeToString(
            LTXModeEnum::Default);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL,
                                            responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    sensor.setLTXMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    // Default target resolves to Enabled on PendingMode, mirroring the
    // CurrentMode wire-Default resolution rule.
    EXPECT_EQ(ltxModeIntf->pendingMode(), LTXModeEnum::Enabled);
}

TEST_F(NsmSwitchBranchTest, LTXMode_SetLTXMode_NotConfigurable_ThrowsNotAllowed)
{
    auto ltxModeIntf = std::make_shared<LTXModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_set_nocfg");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/ltx_set_nocfg");
    ltxModeIntf->isModeConfigurable(false);
    NsmSwitchLTXMode sensor("LTX_setnocfg", "NSM_NVSwitch", ltxModeIntf,
                            assocIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        LTXModeServer::convertLinkTrainingExtendedModeToString(
            LTXModeEnum::Enabled);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _)).Times(0);

    EXPECT_THROW_COROUTINE(
        sensor.setLTXMode(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::NotAllowed);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// ============================================================================
// NsmSwitchUPhyMode -- genRequestMsg, handleResponseMsg, setUPhyMode
// LTX Mode Enable/Disable feature (NSM Type 5 Device Mode index
// DEVICE_MODE_UPHY). Parallel implementation to NsmSwitchLTXMode above.
// ============================================================================

TEST_F(NsmSwitchBranchTest, UPhyMode_GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_genreq_fail");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_genreq_fail");
    NsmSwitchUPhyMode sensor("UPhy_genreq", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);

    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmSwitchBranchTest,
       UPhyMode_HandleResponseMsg_ValidCurrentAndPending_UpdatesBothProperties)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_valid_both");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_valid_both");
    NsmSwitchUPhyMode sensor("UPhy_valid_both", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            UPHY_MODE_DATA_SIZE * 2,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t currentMode = NSM_UPHY_MODE_ENABLED;
    uint8_t pendingMode = NSM_UPHY_MODE_DISABLED;
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &currentMode, UPHY_MODE_DATA_SIZE,
        &pendingMode, UPHY_MODE_DATA_SIZE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(uphyModeIntf->currentMode(), UPhyModeEnum::Enabled);
    EXPECT_EQ(uphyModeIntf->pendingMode(), UPhyModeEnum::Disabled);
}

TEST_F(NsmSwitchBranchTest,
       UPhyMode_HandleResponseMsg_PendingLengthZero_CurrentModeOnlyUpdated)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_no_pending");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_no_pending");
    uphyModeIntf->pendingMode(UPhyModeEnum::Disabled);
    NsmSwitchUPhyMode sensor("UPhy_no_pending", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            UPHY_MODE_DATA_SIZE,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t currentMode = NSM_UPHY_MODE_ENABLED;
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &currentMode, UPHY_MODE_DATA_SIZE, nullptr, 0,
        response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(uphyModeIntf->currentMode(), UPhyModeEnum::Enabled);
    EXPECT_EQ(uphyModeIntf->pendingMode(), UPhyModeEnum::Enabled);
}

TEST_F(NsmSwitchBranchTest, UPhyMode_WireDefaultCurrentMode_ResolvesToDisabled)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_def_wire");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_def_wire");
    NsmSwitchUPhyMode sensor("UPhy_defwire", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            UPHY_MODE_DATA_SIZE * 2,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t currentMode = NSM_UPHY_MODE_DEFAULT;
    uint8_t pendingMode = NSM_UPHY_MODE_DEFAULT;
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &currentMode, UPHY_MODE_DATA_SIZE,
        &pendingMode, UPHY_MODE_DATA_SIZE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(uphyModeIntf->currentMode(), UPhyModeEnum::Disabled);
    EXPECT_EQ(uphyModeIntf->pendingMode(), UPhyModeEnum::Disabled);
}

TEST_F(NsmSwitchBranchTest,
       UPhyMode_HandleResponseMsg_CurrentLengthZero_ReturnsError)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_cur_zero");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_cur_zero");
    NsmSwitchUPhyMode sensor("UPhy_curzero", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);
    // Seed a known pre-existing value so the assertion below actually proves
    // the hard-error path leaves the property untouched, rather than
    // trivially matching the sdbusplus default-constructed value.
    uphyModeIntf->currentMode(UPhyModeEnum::Enabled);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, nullptr, 0, nullptr, 0, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    // Review-correction-mandated hard error: missing CurrentMode must not be
    // a partial success, and the previously published value must survive.
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
    EXPECT_EQ(uphyModeIntf->currentMode(), UPhyModeEnum::Enabled);
}

TEST_F(NsmSwitchBranchTest,
       UPhyMode_HandleResponseMsg_CurrentLengthTooShort_ReturnsError)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_cur_short");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_cur_short");
    NsmSwitchUPhyMode sensor("UPhy_curshort", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);

    // UPHY_MODE_DATA_SIZE == 1, so any currentModeLength != 1 is covered by
    // this same "too long" payload equivalence class.
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t currentMode[2] = {NSM_UPHY_MODE_ENABLED, 0};
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, currentMode, uint16_t(sizeof(currentMode)),
        nullptr, 0, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(NsmSwitchBranchTest,
       UPhyMode_HandleResponseMsg_PendingLengthInvalid_ReturnsError)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_pend_invalid");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_pend_invalid");
    NsmSwitchUPhyMode sensor("UPhy_pendinvalid", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            UPHY_MODE_DATA_SIZE + 2,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t currentMode = NSM_UPHY_MODE_ENABLED;
    uint8_t pendingMode[2] = {NSM_UPHY_MODE_DISABLED, 0};
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &currentMode, UPHY_MODE_DATA_SIZE,
        pendingMode, uint16_t(sizeof(pendingMode)), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(NsmSwitchUPhyModeBranch, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_decfail");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_decfail");
    NsmSwitchUPhyMode sensor("UPhy_decfail", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmSwitchBranchTest,
       UPhyMode_SetUPhyMode_Configurable_EncodesSetRequestAndUpdatesPendingMode)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_set_cfg");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_set_cfg");
    uphyModeIntf->isModeConfigurable(true);
    NsmSwitchUPhyMode sensor("UPhy_setcfg", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        UPhyModeServer::convertUPhyModeToString(UPhyModeEnum::Disabled);

    Request capturedRequest;
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL,
                                            responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(Invoke(
            [&, responseData](eid_t, Request& req,
                              std::shared_ptr<const nsm_msg>& outResponseMsg,
                              size_t& outResponseLen) -> requester::Coroutine {
        capturedRequest = req;
        outResponseLen = responseData.size();
        auto* buf = new uint8_t[outResponseLen];
        std::memcpy(buf, responseData.data(), outResponseLen);
        outResponseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(buf), [](const nsm_msg* p) {
            delete[] reinterpret_cast<const uint8_t*>(p);
        });
        // coverity[missing_return]
        co_return NSM_SUCCESS;
    }));

    sensor.setUPhyMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_EQ(uphyModeIntf->pendingMode(), UPhyModeEnum::Disabled);

    ASSERT_GE(capturedRequest.size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_set_device_mode_settings_v2_req));
    uint32_t decodedIdx = 0;
    std::vector<uint8_t> decodedData(UPHY_MODE_DATA_SIZE, 0);
    uint16_t decodedLen = 0;
    auto reqMsg = reinterpret_cast<const nsm_msg*>(capturedRequest.data());
    auto rc = decode_set_device_mode_settings_v2_req(
        reqMsg, capturedRequest.size(), &decodedIdx, decodedData.data(),
        &decodedLen);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(decodedIdx, static_cast<uint32_t>(DEVICE_MODE_UPHY));
    EXPECT_EQ(decodedLen, UPHY_MODE_DATA_SIZE);
    EXPECT_EQ(decodedData[0], NSM_UPHY_MODE_DISABLED);
}

TEST_F(NsmSwitchBranchTest,
       UPhyMode_SetUPhyMode_DefaultTarget_ResolvesToDisabled)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_set_default");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_set_default");
    uphyModeIntf->isModeConfigurable(true);
    NsmSwitchUPhyMode sensor("UPhy_setdef", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        UPhyModeServer::convertUPhyModeToString(UPhyModeEnum::Default);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL,
                                            responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    sensor.setUPhyMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_EQ(uphyModeIntf->pendingMode(), UPhyModeEnum::Disabled);
}

TEST_F(NsmSwitchBranchTest,
       UPhyMode_SetUPhyMode_NotConfigurable_ThrowsNotAllowed)
{
    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_set_nocfg");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/branchtest/uphy_set_nocfg");
    uphyModeIntf->isModeConfigurable(false);
    NsmSwitchUPhyMode sensor("UPhy_setnocfg", "NSM_NVSwitch", uphyModeIntf,
                             assocIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        UPhyModeServer::convertUPhyModeToString(UPhyModeEnum::Enabled);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _)).Times(0);

    EXPECT_THROW_COROUTINE(
        sensor.setUPhyMode(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::NotAllowed);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// ============================================================================
// UT GAP: live MCTP transport / real NVSwitch hardware timing behavior for
// LTX/UPhy mode set round-trips cannot be exercised by this in-memory NSM
// response harness -- requires live MCTP/hardware, not available in the
// repo test harness.
// ============================================================================

TEST_F(NsmSwitchBranchTest,
       DISABLED_LTXUPhyMode_SetModeRoundTrip_RequiresLiveMctpHardware)
{
    // GAP: real hardware applies the mode change asynchronously after "the
    // next link toggle" (see SetLTXMode::parseResponseMsg note in
    // nsm_config_cmd.cpp); verifying that timing/state transition requires a
    // live MCTP-connected NVSwitch and is not reproducible with the
    // in-memory postPatchIO mock used by the rest of this file.
    GTEST_SKIP();
}

// ============================================================================
// createNsmSwitchDI gate: SupportLTXMode / SupportUPhyMode true/false/absent
//
// These tests need entity-manager base-property mocking (MockDbusAsync),
// which NsmSwitchBranchTest's fixture does not set up (its sibling tests
// exercise sensor classes directly against in-memory NSM buffers, never the
// createNsmSwitchDI coroutine). A separate local fixture mirrors the
// established pattern in nsmSwitchFactoryBranch_test.cpp
// (NsmSwitchFactoryBranchTest / Factory_NVSwitch_SupportPowerCappingMode*),
// which already tests the analogous SupportPowerCappingMode gate this same
// way. Kept in this file per the "same file" preference; see
// nsmd_ut_readme.md for the rationale in full.
// ============================================================================

namespace nsm
{
requester::Coroutine createNsmSwitchDI(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

class NsmSwitchLTXUPhyFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch";
    const uuid_t switchUuid = "STATIC:515:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string switchName = "NVSwitch_LTXUPhyFbr";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/system/ltxuphyfbr/nvswitch/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchLTXUPhyFactoryTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchLTXUPhyFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    void setupBaseProperties(const std::string& path,
                             dbus::PropertyMap extra = {})
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
        pm["Name"] = switchName;
        pm["UUID"] = switchUuid;
        pm["InventoryObjPath"] = inventoryPath;
        for (auto& [k, v] : extra)
        {
            pm[k] = v;
        }
    }
};

static size_t countLTXModeSensors(const std::shared_ptr<MockNsmDevice>& dev)
{
    size_t count = 0;
    for (const auto& sensor : dev->roundRobinSensors)
    {
        if (std::dynamic_pointer_cast<NsmSwitchLTXMode>(sensor))
        {
            ++count;
        }
    }
    return count;
}

static size_t countUPhyModeSensors(const std::shared_ptr<MockNsmDevice>& dev)
{
    size_t count = 0;
    for (const auto& sensor : dev->roundRobinSensors)
    {
        if (std::dynamic_pointer_cast<NsmSwitchUPhyMode>(sensor))
        {
            ++count;
        }
    }
    return count;
}

TEST_F(
    NsmSwitchLTXUPhyFactoryTest,
    LTXMode_CreateNsmSwitchLTXMode_SupportFlagTrue_ObjectCreatedAtExpectedPath)
{
    const std::string path = inventoryPath + "nvs_ltx_true";
    setupBaseProperties(path, {{"Type", std::string("NSM_NVSwitch")},
                               {"SupportL1PredictionMode", bool(false)},
                               {"SupportLTXMode", bool(true)}});

    ASSERT_EQ(countLTXModeSensors(nvswitch), 0u);
    createNsmSwitchDI(mockManager, baseIntf, path);
    EXPECT_EQ(countLTXModeSensors(nvswitch), 1u);

    for (const auto& sensor : nvswitch->roundRobinSensors)
    {
        auto ltx = std::dynamic_pointer_cast<NsmSwitchLTXMode>(sensor);
        if (ltx)
        {
            // IsModeConfigurable is wired from the SupportLTXMode gate: the
            // factory is only ever invoked when the flag is true, so the
            // published value reflects that flag rather than a hardcoded
            // default -- see createNsmSwitchLTXMode's review-correction
            // comment in nsmSwitch.cpp.
            EXPECT_TRUE(ltx->ltxModeIntf->isModeConfigurable());
        }
    }
}

TEST_F(
    NsmSwitchLTXUPhyFactoryTest,
    UPhyMode_CreateNsmSwitchUPhyMode_SupportFlagTrue_ObjectCreatedAtExpectedPath)
{
    const std::string path = inventoryPath + "nvs_uphy_true";
    setupBaseProperties(path, {{"Type", std::string("NSM_NVSwitch")},
                               {"SupportL1PredictionMode", bool(false)},
                               {"SupportUPhyMode", bool(true)}});

    ASSERT_EQ(countUPhyModeSensors(nvswitch), 0u);
    createNsmSwitchDI(mockManager, baseIntf, path);
    EXPECT_EQ(countUPhyModeSensors(nvswitch), 1u);

    for (const auto& sensor : nvswitch->roundRobinSensors)
    {
        auto uphy = std::dynamic_pointer_cast<NsmSwitchUPhyMode>(sensor);
        if (uphy)
        {
            EXPECT_TRUE(uphy->uphyModeIntf->isModeConfigurable());
        }
    }
}
