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
