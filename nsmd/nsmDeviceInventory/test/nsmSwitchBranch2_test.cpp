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
 * Additional branch coverage for nsmSwitch.cpp:
 * - NsmSwitchDIPowerMode::update: sensorIO fail, ternary combos
 * - NsmSwitchDIPowerMode::setL1PowerDevice: postPatchIO fail, decode fail
 * - NsmSwitchDIPowerMode::setL1PowerModePatch: asyncPatchInProgress, invalid
 *   types, empty list, unrecognized property
 * - NsmSwitchIsolationMode::setSwitchIsolationMode: invalid mode, encode fail,
 *   postPatchIO fail, decode fail
 * - NsmSwitchIsolationMode::handleResponseMsg: success, error CC, decode fail
 * - NsmSwitchL1PredictionMode::setL1PredictionMode: postPatchIO fail,
 *   decode fail
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

using PatchTupleValue =
    std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
using PatchValueList = std::vector<std::tuple<std::string, PatchTupleValue>>;

static auto& testBus = utils::DBusHandler::getBus();

// ============================================================================
// Fixture
// ============================================================================
class NsmSwitchBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const std::string name = "NVSwitch_Br2";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/br2test/nvswitch/";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:501";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchBranch2Test() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchBranch2Test()
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

    static std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ===========================================================================
// NsmSwitchDIPowerMode::update - sensorIO failure
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, PowerMode_Update_SensorIOFail)
{
    auto pm = makePowerMode();

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_INVALID_DATA));

    pm->update(nvswitch);
}

// ===========================================================================
// NsmSwitchDIPowerMode::update - success with ternary TRUE branches
// (all control values == 1)
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, PowerMode_Update_Success_AllTrue)
{
    auto pm = makePowerMode();

    nsm_power_mode_data data = {};
    data.l1_hw_mode_control = 1;
    data.l1_hw_mode_threshold = 100;
    data.l1_fw_throttling_mode = 1;
    data.l1_prediction_mode = 1;
    data.l1_hw_active_time = 10;
    data.l1_hw_inactive_time = 20;
    data.l1_prediction_inactive_time = 30;

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_power_mode_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp));

    pm->update(nvswitch);

    EXPECT_TRUE(pm->invoke(pdiMethod(hwModeControl)));
    EXPECT_TRUE(pm->invoke(pdiMethod(fwThrottlingMode)));
    EXPECT_TRUE(pm->invoke(pdiMethod(predictionMode)));
}

// ===========================================================================
// NsmSwitchDIPowerMode::update - success with ternary FALSE branches
// (all control values == 0)
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, PowerMode_Update_Success_AllFalse)
{
    auto pm = makePowerMode();

    nsm_power_mode_data data = {};
    data.l1_hw_mode_control = 0;
    data.l1_fw_throttling_mode = 0;
    data.l1_prediction_mode = 0;

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_power_mode_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp));

    pm->update(nvswitch);

    EXPECT_FALSE(pm->invoke(pdiMethod(hwModeControl)));
    EXPECT_FALSE(pm->invoke(pdiMethod(fwThrottlingMode)));
    EXPECT_FALSE(pm->invoke(pdiMethod(predictionMode)));
}

// ===========================================================================
// NsmSwitchDIPowerMode::update - non-zero CC path
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, PowerMode_Update_NonZeroCC)
{
    auto pm = makePowerMode();
    auto resp = decodeFail();

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp));

    pm->update(nvswitch);
}

// ===========================================================================
// NsmSwitchDIPowerMode::setL1PowerDevice - postPatchIO failure
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetL1PowerDevice_PostPatchIOFail)
{
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    pm->setL1PowerDevice(data, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// NsmSwitchDIPowerMode::setL1PowerDevice - decode failure (non-zero CC)
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetL1PowerDevice_DecodeFail)
{
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};

    auto resp = decodeFail();
    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    pm->setL1PowerDevice(data, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - asyncPatchInProgress
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetL1PowerModePatch_AsyncInProgress)
{
    auto pm = makePowerMode();
    pm->asyncPatchInProgress = true;

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWModeControl", bool{true}}};

    pm->setL1PowerModePatch(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// ===========================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - invalid value type (not vector)
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetL1PowerModePatch_InvalidValueType)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("not a patch list");

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ===========================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - empty patch list
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetL1PowerModePatch_EmptyPatchList)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = PatchValueList{};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ===========================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - invalid type for each key
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetL1PowerModePatch_HWModeControl_InvalidType)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass uint32_t instead of bool for HWModeControl
    AsyncSetOperationValueType value =
        PatchValueList{{"HWModeControl", uint32_t{1}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchBranch2Test, SetL1PowerModePatch_FWThrottlingMode_InvalidType)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"FWThrottlingMode", uint32_t{1}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchBranch2Test, SetL1PowerModePatch_PredictionMode_InvalidType)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"PredictionMode", uint32_t{1}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchBranch2Test, SetL1PowerModePatch_HWThreshold_InvalidType)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass bool instead of uint32_t for HWThreshold
    AsyncSetOperationValueType value =
        PatchValueList{{"HWThreshold", bool{true}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchBranch2Test, SetL1PowerModePatch_HWActiveTime_InvalidType)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWActiveTime", bool{true}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchBranch2Test, SetL1PowerModePatch_HWInactiveTime_InvalidType)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWInactiveTime", bool{true}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchBranch2Test,
       SetL1PowerModePatch_HWPredictionInactiveTime_InvalidType)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWPredictionInactiveTime", bool{true}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ===========================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - unrecognized property
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetL1PowerModePatch_UnrecognizedProperty)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"BogusProperty", bool{true}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ===========================================================================
// NsmSwitchIsolationMode::handleResponseMsg - success, error CC, decode fail
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, IsolationMode_HandleResponseMsg_Success)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/iso_success");
    NsmSwitchIsolationMode sensor("Iso_success", "NSM_NVSwitch", isolationIntf);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                              sizeof(uint8_t));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    uint8_t mode = SWITCH_COMMUNICATION_MODE_ENABLED;
    encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, mode, msg);

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST_F(NsmSwitchBranch2Test, IsolationMode_HandleResponseMsg_ErrorCC)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/iso_errcc");
    NsmSwitchIsolationMode sensor("Iso_errcc", "NSM_NVSwitch", isolationIntf);

    auto resp = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST_F(NsmSwitchBranch2Test, IsolationMode_GenRequestMsg_Success)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/iso_gen_ok");
    NsmSwitchIsolationMode sensor("Iso_gen", "NSM_NVSwitch", isolationIntf);

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmSwitchBranch2Test, IsolationMode_GenRequestMsg_EncodeFail)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/iso_gen_fail");
    NsmSwitchIsolationMode sensor("Iso_gen_f", "NSM_NVSwitch", isolationIntf);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ===========================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode - invalid mode string
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetSwitchIsolationMode_InvalidMode)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/iso_invalid");
    NsmSwitchIsolationMode sensor("Iso_invalid", "NSM_NVSwitch", isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("InvalidModeString");

    // Invalid mode -> sets status=WriteFailure, returns error (no throw)
    sensor.setSwitchIsolationMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode - postPatchIO failure
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetSwitchIsolationMode_PostPatchIOFail)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/iso_ppfail");
    NsmSwitchIsolationMode sensor("Iso_ppfail", "NSM_NVSwitch", isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    sensor.setSwitchIsolationMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode - decode fail (non-zero CC)
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetSwitchIsolationMode_DecodeFail)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/iso_decfail");
    NsmSwitchIsolationMode sensor("Iso_decfail", "NSM_NVSwitch", isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    auto resp = decodeFail();
    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    sensor.setSwitchIsolationMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// NsmSwitchL1PredictionMode - postPatchIO failure
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetL1PredictionMode_PostPatchIOFail)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_ppfail");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_ppfail");
    NsmSwitchL1PredictionMode sensor("Pred_ppfail", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    sensor.setL1PredictionMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// NsmSwitchL1PredictionMode - decode fail (non-zero CC)
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, SetL1PredictionMode_DecodeFail)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_decfail");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_decfail");
    NsmSwitchL1PredictionMode sensor("Pred_decfail", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};

    auto resp = decodeFail();
    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    sensor.setL1PredictionMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// NsmSwitchL1PredictionMode::handleResponseMsg - success
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, PredictionMode_HandleResponseMsg_Success)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_hr_succ");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_hr_succ");
    NsmSwitchL1PredictionMode sensor("Pred_hr_succ", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    // Manually construct response: nsm_msg_hdr +
    // nsm_get_device_mode_setting_resp
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

TEST_F(NsmSwitchBranch2Test, PredictionMode_HandleResponseMsg_ErrorCC)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_hr_errcc");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_hr_errcc");
    NsmSwitchL1PredictionMode sensor("Pred_hr_errcc", "NSM_NVSwitch",
                                     enableIntf, assocDefIntf);

    auto resp = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// NsmSwitchL1PredictionMode::genRequestMsg - success and failure
// ===========================================================================
TEST_F(NsmSwitchBranch2Test, PredictionMode_GenRequestMsg_Success)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_gen_ok");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_gen_ok");
    NsmSwitchL1PredictionMode sensor("Pred_gen_ok", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmSwitchBranch2Test, PredictionMode_GenRequestMsg_EncodeFail)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_gen_fail");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/br2test/pred_gen_fail");
    NsmSwitchL1PredictionMode sensor("Pred_gen_f", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
