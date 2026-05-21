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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "network-ports.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmDeviceInventory/nsmSwitch.hpp"

#undef private
#undef protected

using namespace nsm;

// Forward declarations for factory functions
namespace nsm
{
requester::Coroutine createNsmSwitchDI(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

static auto& testBus = utils::DBusHandler::getBus();

// ============================================================================
// SECTION 1: nsmSwitch.cpp -- NsmSwitchIsolationMode genRequestMsg /
//            handleResponseMsg
// ============================================================================

TEST(NsmSwitchIsolationModeGenReq, GenRequestMsg_ValidArgs_ReturnsRequest)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso1");
    NsmSwitchIsolationMode sensor("IsoSwitch_0", "NSM_NVSwitch", isolationIntf);

    // Act
    auto request = sensor.genRequestMsg(10, 5);

    // Assert
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}

TEST(NsmSwitchIsolationModeGenReq,
     GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso2");
    NsmSwitchIsolationMode sensor("IsoSwitch_1", "NSM_NVSwitch", isolationIntf);

    // Act
    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);

    // Assert
    EXPECT_FALSE(request.has_value());
}

TEST(NsmSwitchIsolationModeHandleResp,
     HandleResponseMsg_Enabled_SetsEnabledMode)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso3");
    NsmSwitchIsolationMode sensor("IsoSwitch_2", "NSM_NVSwitch", isolationIntf);

    uint8_t isolationMode = SWITCH_COMMUNICATION_MODE_ENABLED;
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    isolationMode, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationEnabled);
}

TEST(NsmSwitchIsolationModeHandleResp,
     HandleResponseMsg_Disabled_SetsDisabledMode)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso4");
    NsmSwitchIsolationMode sensor("IsoSwitch_3", "NSM_NVSwitch", isolationIntf);

    uint8_t isolationMode = SWITCH_COMMUNICATION_MODE_DISABLED;
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    isolationMode, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationDisabled);
}

TEST(NsmSwitchIsolationModeHandleResp,
     HandleResponseMsg_UnknownValue_SetsUnknownMode)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso5");
    NsmSwitchIsolationMode sensor("IsoSwitch_4", "NSM_NVSwitch", isolationIntf);

    // Use value 0xFF which is neither ENABLED nor DISABLED
    uint8_t isolationMode = 0xFF;
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    isolationMode, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationUnknown);
}

TEST(NsmSwitchIsolationModeHandleResp,
     HandleResponseMsg_ErrorCC_DoesNotUpdateMode)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso6");
    NsmSwitchIsolationMode sensor("IsoSwitch_5", "NSM_NVSwitch", isolationIntf);

    uint8_t isolationMode = SWITCH_COMMUNICATION_MODE_ENABLED;
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_switch_isolation_mode_resp(0, NSM_ERROR, ERR_NULL,
                                                    isolationMode, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Shrink to non-success response size
    responseData.resize(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // Act
    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmSwitchIsolationModeHandleResp, HandleResponseMsg_NullMsg_ReturnsError)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso7");
    NsmSwitchIsolationMode sensor("IsoSwitch_6", "NSM_NVSwitch", isolationIntf);

    // Act
    auto rc = sensor.handleResponseMsg(nullptr, 100);

    // Assert - cc is initialized to NSM_ERROR(1), return cc?cc:rc returns cc
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmSwitchIsolationModeHandleResp, HandleResponseMsg_ShortLen_ReturnsError)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso8");
    NsmSwitchIsolationMode sensor("IsoSwitch_7", "NSM_NVSwitch", isolationIntf);

    // Buffer has cc=NSM_SUCCESS (0) so decode_reason_code_and_cc succeeds,
    // then the exact-length check in decode_get_switch_isolation_mode_resp
    // fails → returns NSM_SW_ERROR_LENGTH. handleResponseMsg returns
    // cc?cc:rc = 0?0:NSM_SW_ERROR_LENGTH = NSM_SW_ERROR_LENGTH.
    // Minimum size avoids valgrind invalid-read (payload[1] is within buffer).
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // Act
    auto rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// SECTION 2: nsmSwitch.cpp -- NsmSwitchL1PredictionMode genRequestMsg /
//            handleResponseMsg
// ============================================================================

TEST(NsmSwitchL1PredictionModeGenReq, GenRequestMsg_ValidArgs_ReturnsRequest)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred1");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred1");
    NsmSwitchL1PredictionMode sensor("Pred_0", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    // Act
    auto request = sensor.genRequestMsg(10, 5);

    // Assert
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_req));
}

TEST(NsmSwitchL1PredictionModeGenReq,
     GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred2");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred2");
    NsmSwitchL1PredictionMode sensor("Pred_1", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    // Act
    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);

    // Assert
    EXPECT_FALSE(request.has_value());
}

TEST(NsmSwitchL1PredictionModeHandleResp,
     HandleResponseMsg_Enabled_SetsEnabledTrue)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred3");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred3");
    NsmSwitchL1PredictionMode sensor("Pred_2", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    uint8_t deviceMode =
        static_cast<uint8_t>(nsm_l1_prediction_mode_config::ENABLED);
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   deviceMode, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(enableIntf->enabled());
}

TEST(NsmSwitchL1PredictionModeHandleResp,
     HandleResponseMsg_Disabled_SetsEnabledFalse)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred4");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred4");
    NsmSwitchL1PredictionMode sensor("Pred_3", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    uint8_t deviceMode =
        static_cast<uint8_t>(nsm_l1_prediction_mode_config::DISABLED);
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   deviceMode, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(enableIntf->enabled());
}

TEST(NsmSwitchL1PredictionModeHandleResp,
     HandleResponseMsg_ErrorCC_DoesNotUpdate)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred5");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred5");
    NsmSwitchL1PredictionMode sensor("Pred_4", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    uint8_t deviceMode =
        static_cast<uint8_t>(nsm_l1_prediction_mode_config::ENABLED);
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_device_mode_settings_resp(0, NSM_ERROR, ERR_NULL,
                                                   deviceMode, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    responseData.resize(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // Act
    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmSwitchL1PredictionModeHandleResp,
     HandleResponseMsg_NullMsg_ReturnsError)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred6");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred6");
    NsmSwitchL1PredictionMode sensor("Pred_5", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    // Act
    auto rc = sensor.handleResponseMsg(nullptr, 100);

    // Assert - cc is initialized to NSM_ERROR(1), return cc?cc:rc returns cc
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// SECTION 3: nsmSwitch.cpp -- NsmSwitchDIPowerMode update + decode error
// ============================================================================

class NsmSwitchDIPowerModeUpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const std::string name = "NVSwitch_B12E";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/b12e/nvswitch/";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:200";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchDIPowerModeUpdateTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchDIPowerModeUpdateTest()
    {
        cleanupDeviceSensors(devices);
        nvswitch.reset();
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

TEST_F(NsmSwitchDIPowerModeUpdateTest, Update_SuccessResponse_UpdatesAllFields)
{
    // Arrange
    auto pm = makePowerMode();

    nsm_power_mode_data data = {};
    data.l1_hw_mode_control = 1;
    data.l1_hw_mode_threshold = 5000;
    data.l1_fw_throttling_mode = 1;
    data.l1_prediction_mode = 0;
    data.l1_hw_active_time = 100;
    data.l1_hw_inactive_time = 200;
    data.l1_prediction_inactive_time = 300;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                         responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    pm->update(nvswitch);

    // Assert
    EXPECT_TRUE(pm->invoke(pdiMethod(hwModeControl)));
    EXPECT_EQ(pm->invoke(pdiMethod(hwThreshold)), static_cast<uint64_t>(5000));
    EXPECT_TRUE(pm->invoke(pdiMethod(fwThrottlingMode)));
    EXPECT_FALSE(pm->invoke(pdiMethod(predictionMode)));
    EXPECT_EQ(pm->invoke(pdiMethod(hwActiveTime)), static_cast<uint64_t>(100));
    EXPECT_EQ(pm->invoke(pdiMethod(hwInactiveTime)),
              static_cast<uint64_t>(200));
    EXPECT_EQ(pm->invoke(pdiMethod(hwPredictionInactiveTime)),
              static_cast<uint64_t>(300));
}

TEST_F(NsmSwitchDIPowerModeUpdateTest, Update_ErrorCC_DoesNotUpdateFields)
{
    // Arrange
    auto pm = makePowerMode();

    nsm_power_mode_data data = {};
    data.l1_hw_mode_control = 1;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_mode_resp(0, NSM_ERROR, ERR_NULL, &data,
                                         responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    responseData.resize(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    pm->update(nvswitch);

    // Assert - hwModeControl should remain false (unchanged)
    EXPECT_FALSE(pm->invoke(pdiMethod(hwModeControl)));
}

TEST_F(NsmSwitchDIPowerModeUpdateTest, Update_SensorIOFailure_ReturnsEarly)
{
    // Arrange
    auto pm = makePowerMode();

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    // Act - should not crash
    EXPECT_NO_THROW(pm->update(nvswitch));

    // Assert - hwModeControl should remain false
    EXPECT_FALSE(pm->invoke(pdiMethod(hwModeControl)));
}

// Test setL1PowerDevice with decode failure (CC error in response)
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerDevice_DecodeFailure_SetsWriteFailure)
{
    // Arrange
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    // Encode a success first then corrupt the CC
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);
    // Set completion code to error
    auto* payload =
        reinterpret_cast<nsm_common_non_success_resp*>(responseMsg->payload);
    payload->completion_code = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    pm->setL1PowerDevice(data, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setL1PowerDevice: postPatchIO succeeds, decode_set_power_mode_resp returns
// NSM_SW_SUCCESS with cc=NSM_ERROR (buffer exactly nsm_msg_hdr +
// nsm_common_non_success_resp so decode_reason_code_and_cc succeeds) →
// nsmSwitch.cpp L219 `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` FALSE via
// cc!=NSM_SUCCESS branch.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerDevice_DecodeSuccessNonZeroCC_ElseBranch)
{
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};

    // Buffer exactly sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp)
    // with completion_code = NSM_ERROR (byte index nsm_msg_hdr+1)
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    responseData[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    pm->setL1PowerDevice(data, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// SECTION 11: nsmSwitch.cpp -- createNsmSwitchDI factory additional branches
// ============================================================================

struct NsmSwitchDIFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:230";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/b12e/switch_factory";
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitchDev;

    NsmSwitchDIFactoryTest() : SensorManagerTest(devices)
    {
        nvswitchDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitchDev, nullptr);
    }

    ~NsmSwitchDIFactoryTest()
    {
        cleanupDeviceSensors(devices);
        nvswitchDev.reset();
    }
};

TEST_F(NsmSwitchDIFactoryTest, Factory_NSMSwitch_CreatesSwitch)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_B12E_Fac");
    basePropertyMap["UUID"] = switchUuid;
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/switch_fac/");

    std::string switchIntf = baseIntf + ".SwitchType";
    auto& ifacePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                               switchIntf);
    ifacePropertyMap["Type"] = std::string("NSM_Switch");
    ifacePropertyMap["SwitchType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Switch."
        "SwitchType.NVLink");
    ifacePropertyMap["SwitchSupportedProtocols"] = std::vector<std::string>{
        "xyz.openbmc_project.Inventory.Item.Switch."
        "SwitchType.NVLink"};

    // Act
    createNsmSwitchDI(mockManager, switchIntf, objPath);

    // Assert - creates a SwitchIntf sensor
    EXPECT_GE(nvswitchDev->deviceSensors.size(), 1u);
}

TEST_F(NsmSwitchDIFactoryTest, Factory_NSMChassisAttributes_CreatesAssetSensor)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath + "_attr",
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_B12E_Attr");
    basePropertyMap["UUID"] = switchUuid;
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/switch_attr/");

    std::string attrIntf = baseIntf + ".ChassisAttributes";
    auto& ifacePropertyMap =
        utils::MockDbusAsync::propertyMap(objPath + "_attr", attrIntf);
    ifacePropertyMap["Type"] = std::string("NSM_Chassis_Attributes");

    // Act
    createNsmSwitchDI(mockManager, attrIntf, objPath + "_attr");

    // Assert - creates an NsmAssetIntf sensor
    EXPECT_GE(nvswitchDev->deviceSensors.size(), 1u);
}

TEST_F(NsmSwitchDIFactoryTest, Factory_NSMPortDisableFuture_CreatesSensor)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath + "_pdf",
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_B12E_PDF");
    basePropertyMap["UUID"] = switchUuid;
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/switch_pdf/");

    std::string pdfIntf = baseIntf + ".PortDisableFuture";
    auto& ifacePropertyMap = utils::MockDbusAsync::propertyMap(objPath + "_pdf",
                                                               pdfIntf);
    ifacePropertyMap["Type"] = std::string("NSM_PortDisableFuture");
    ifacePropertyMap["Priority"] = false;

    // Act
    createNsmSwitchDI(mockManager, pdfIntf, objPath + "_pdf");

    // Assert - creates a PortDisableFuture sensor
    EXPECT_GE(nvswitchDev->deviceSensors.size(), 1u);
}

TEST_F(NsmSwitchDIFactoryTest, Factory_NSMPowerMode_CreatesPowerModeSensor)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath + "_pm",
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_B12E_PM");
    basePropertyMap["UUID"] = switchUuid;
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/switch_pm/");

    std::string pmIntf = baseIntf + ".PowerMode";
    auto& ifacePropertyMap = utils::MockDbusAsync::propertyMap(objPath + "_pm",
                                                               pmIntf);
    ifacePropertyMap["Type"] = std::string("NSM_PowerMode");
    ifacePropertyMap["Priority"] = false;

    // Act
    createNsmSwitchDI(mockManager, pmIntf, objPath + "_pm");

    // Assert - creates a PowerMode sensor
    EXPECT_GE(nvswitchDev->deviceSensors.size(), 1u);
}

TEST_F(NsmSwitchDIFactoryTest, Factory_NSMFabricManager_CreatesFMSensor)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath + "_fm",
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_B12E_FM");
    basePropertyMap["UUID"] = switchUuid;
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/switch_fm/");

    std::string fmIntf = baseIntf + ".FabricManager";
    auto& ifacePropertyMap = utils::MockDbusAsync::propertyMap(objPath + "_fm",
                                                               fmIntf);
    ifacePropertyMap["Type"] = std::string("NSM_FabricManager");
    ifacePropertyMap["Name"] = std::string("FabMgr_0");
    ifacePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/fm/");
    ifacePropertyMap["Description"] = std::string("Fabric Manager 0");

    // Act
    createNsmSwitchDI(mockManager, fmIntf, objPath + "_fm");

    // Assert - creates a FabricManager sensor
    EXPECT_GE(nvswitchDev->deviceSensors.size(), 1u);
}

// createNsmSwitchDI: base interface not registered → coGetCachedBaseProperties
// returns error → co_return rc (early-return branch in nsmSwitch.cpp line 694).
//
TEST_F(NsmSwitchDIFactoryTest, Factory_BasePropertiesFail_NoSensors)
{
    const std::string uniquePath = "/xyz/test/switch/base_fail_unique";
    // Register a sub-interface at uniquePath, but NOT the baseIntf itself.
    // coGetCachedBaseProperties looks for baseIntf and fails → early return.
    auto& other = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    baseIntf + ".Sub");
    other["Name"] = std::string("NVSwitch_Fail");

    const size_t before = nvswitchDev->deviceSensors.size();
    createNsmSwitchDI(mockManager, baseIntf + ".Sub", uniquePath);
    EXPECT_EQ(before, nvswitchDev->deviceSensors.size());
}

// =============================================================================
// Branch coverage: FALSE branches for property count() checks in
// createNsmSwitchDI (nsmSwitch.cpp lines 843-846, 871-874, 949-953,
// 989-998, 1001-1005)
// =============================================================================

// NSM_PortDisableFuture: Priority absent → FALSE branch for
// count("Priority") → priority=false (default bool{}) → roundRobin sensor.
TEST_F(NsmSwitchDIFactoryTest,
       Factory_PortDisableFuture_MissingPriority_RoundRobinSensor)
{
    const std::string testPath = objPath + "_pdf_noprio";
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_PDF_noprio");
    basePropertyMap["UUID"] = switchUuid;
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/switch_pdf_noprio/");

    const std::string pdfIntf = baseIntf + ".PortDisableFuture";
    auto& ifacePropertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                               pdfIntf);
    ifacePropertyMap["Type"] = std::string("NSM_PortDisableFuture");
    // Intentionally omit "Priority" → FALSE branch → priority=false →
    // addSensor(sensor, false) → roundRobin

    const size_t rrBefore = nvswitchDev->roundRobinSensors.size();
    createNsmSwitchDI(mockManager, pdfIntf, testPath);
    EXPECT_GT(nvswitchDev->roundRobinSensors.size(), rrBefore);
}

// NSM_PowerMode: Priority absent → FALSE branch for count("Priority") →
// priority=false (default bool{}) → roundRobin sensor.
TEST_F(NsmSwitchDIFactoryTest,
       Factory_PowerMode_MissingPriority_RoundRobinSensor)
{
    const std::string testPath = objPath + "_pm_noprio";
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_PM_noprio");
    basePropertyMap["UUID"] = switchUuid;
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/switch_pm_noprio/");

    const std::string pmIntf = baseIntf + ".PowerMode";
    auto& ifacePropertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                               pmIntf);
    ifacePropertyMap["Type"] = std::string("NSM_PowerMode");
    // Intentionally omit "Priority" → FALSE branch → priority=false →
    // addSensor(sensor, false) → roundRobin

    const size_t rrBefore = nvswitchDev->roundRobinSensors.size();
    createNsmSwitchDI(mockManager, pmIntf, testPath);
    EXPECT_GT(nvswitchDev->roundRobinSensors.size(), rrBefore);
}

// NSM_Switch: SwitchType absent → FALSE branch for count("SwitchType") →
// switchType="" → convertSwitchTypeFromString("") throws InvalidEnumString
// (extends std::exception) → propagates out.
TEST_F(NsmSwitchDIFactoryTest, Factory_NSMSwitch_MissingSwitchType_Throws)
{
    const std::string testPath = objPath + "_sw_noswtype";
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_miss_swtype");
    basePropertyMap["UUID"] = switchUuid;
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/switch_miss_swtype/");

    const std::string swIntf = baseIntf + ".SwitchType";
    auto& ifacePropertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                               swIntf);
    ifacePropertyMap["Type"] = std::string("NSM_Switch");
    // Intentionally omit "SwitchType" → FALSE branch → switchType="" →
    // convertSwitchTypeFromString("") throws InvalidEnumString

    EXPECT_THROW_COROUTINE(createNsmSwitchDI(mockManager, swIntf, testPath),
                           std::exception);
}

// NSM_FabricManager: Name absent in current iface → FALSE branch for
// count("Name") → nameFM="" → inventoryObjPathFM = fmPath + "" = fmPath →
// NsmFabricManagerState created → sensor IS added.
TEST_F(NsmSwitchDIFactoryTest,
       Factory_FabricManager_MissingFMName_SensorCreated)
{
    const std::string testPath = objPath + "_fm_noname";
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_FM_noname");
    basePropertyMap["UUID"] = switchUuid;
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/switch_fm_noname/");

    const std::string fmIntf = baseIntf + ".FabricManager";
    auto& ifacePropertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                               fmIntf);
    ifacePropertyMap["Type"] = std::string("NSM_FabricManager");
    // Intentionally omit "Name" → FALSE branch for count("Name") →
    // nameFM="" → inventoryObjPathFM = fmPath + "" = fmPath (unchanged)
    ifacePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/fm/");
    ifacePropertyMap["Description"] = std::string("Fabric Manager noname");

    const size_t before = nvswitchDev->deviceSensors.size();
    createNsmSwitchDI(mockManager, fmIntf, testPath);
    EXPECT_GT(nvswitchDev->deviceSensors.size(), before);
}

// NSM_FabricManager: Description absent → FALSE branch for
// count("Description") → description="" → NsmFabricManagerState created with
// empty description → sensor IS added.
TEST_F(NsmSwitchDIFactoryTest,
       Factory_FabricManager_MissingDescription_SensorCreated)
{
    const std::string testPath = objPath + "_fm_nodesc";
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_FM_nodesc");
    basePropertyMap["UUID"] = switchUuid;
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/switch_fm_nodesc/");

    const std::string fmIntf = baseIntf + ".FabricManager";
    auto& ifacePropertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                               fmIntf);
    ifacePropertyMap["Type"] = std::string("NSM_FabricManager");
    ifacePropertyMap["Name"] = std::string("FabMgr_nodesc");
    ifacePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/fm/");
    // Intentionally omit "Description" → FALSE branch → description="" →
    // NsmFabricManagerState created with empty description

    const size_t before = nvswitchDev->deviceSensors.size();
    createNsmSwitchDI(mockManager, fmIntf, testPath);
    EXPECT_GT(nvswitchDev->deviceSensors.size(), before);
}

// ============================================================================
// SECTION 12: setSwitchIsolationMode / setL1PredictionMode error paths
// ============================================================================

// setSwitchIsolationMode: invalid mode string → else branch (lines 464-471) →
// status = WriteFailure; postPatchIO must NOT be called.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetSwitchIsolationMode_InvalidMode_SetsWriteFailure)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso_inv_mode");
    NsmSwitchIsolationMode sensor("IsoSwitch_inv", "NSM_NVSwitch",
                                  isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("UnknownMode");

    EXPECT_CALL(*nvswitch, postPatchIO).Times(0);
    sensor.setSwitchIsolationMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setSwitchIsolationMode: postPatchIO fails → if (rc_) TRUE branch (line 492)
// → status = WriteFailure.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetSwitchIsolationMode_PostPatchIOFail_SetsWriteFailure)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso_pp_fail");
    NsmSwitchIsolationMode sensor("IsoSwitch_ppfail", "NSM_NVSwitch",
                                  isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));
    sensor.setSwitchIsolationMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setSwitchIsolationMode: postPatchIO succeeds, decode sees cc=NSM_ERROR →
// else branch (lines 513-519) → status = WriteFailure.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetSwitchIsolationMode_DecodeErrorCC_SetsWriteFailure)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso_dec_fail");
    NsmSwitchIsolationMode sensor("IsoSwitch_decfail", "NSM_NVSwitch",
                                  isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_switch_isolation_mode_resp(0, NSM_ERROR, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));
    sensor.setSwitchIsolationMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setL1PredictionMode: postPatchIO fails → if (rc_) TRUE branch (line 610) →
// status = WriteFailure.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PredictionMode_PostPatchIOFail_SetsWriteFailure)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred_pp_fail");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred_pp_fail");
    NsmSwitchL1PredictionMode sensor("Pred_ppfail", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));
    sensor.setL1PredictionMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setL1PredictionMode: postPatchIO succeeds, decode sees cc=NSM_ERROR →
// else branch (lines 631-637) → status = WriteFailure.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PredictionMode_DecodeErrorCC_SetsWriteFailure)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred_dec_fail");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/b12e/pred_dec_fail");
    NsmSwitchL1PredictionMode sensor("Pred_decfail", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};

    // Build a response with cc=NSM_ERROR to trigger the else branch
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    reinterpret_cast<nsm_common_resp*>(
        reinterpret_cast<nsm_msg*>(responseData.data())->payload)
        ->completion_code = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));
    sensor.setL1PredictionMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// SECTION 13: setL1PowerDevice and setL1PowerModePatch error/branch paths
// ============================================================================

using PatchTupleValue =
    std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
using PatchValueList = std::vector<std::tuple<std::string, PatchTupleValue>>;

// setL1PowerDevice: postPatchIO fails → if (rc_) TRUE branch (line 203) →
// status = WriteFailure.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerDevice_PostPatchIOFail_SetsWriteFailure)
{
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    pm->setL1PowerDevice(data, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setL1PowerModePatch: asyncPatchInProgress=true → status=Unavailable, early
// return without calling postPatchIO.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_AsyncInProgress_ReturnsUnavailable)
{
    auto pm = makePowerMode();
    pm->asyncPatchInProgress = true;

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWModeControl", bool{true}}};

    EXPECT_CALL(*nvswitch, postPatchIO).Times(0);
    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// setL1PowerModePatch: value holds wrong variant type (string, not
// PatchValueList) → !patchRequestedValues TRUE → throws InvalidArgument.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_InvalidValueType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("not a list");

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// setL1PowerModePatch: empty patch list → patchRequestedValues->empty() TRUE
// → throws InvalidArgument.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_EmptyPatchList_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = PatchValueList{};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// setL1PowerModePatch: all 7 valid keys with correct types → covers each
// key-match branch TRUE and reaches setL1PowerDevice successfully.
TEST_F(NsmSwitchDIPowerModeUpdateTest, SetL1PowerModePatch_AllValidKeys_Success)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    PatchValueList patch = {
        {"HWModeControl", bool{true}},
        {"FWThrottlingMode", bool{false}},
        {"PredictionMode", bool{true}},
        {"HWThreshold", uint32_t{1000}},
        {"HWActiveTime", uint32_t{200}},
        {"HWInactiveTime", uint32_t{300}},
        {"HWPredictionInactiveTime", uint32_t{400}},
    };
    AsyncSetOperationValueType value = patch;

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

// setL1PowerModePatch: unrecognized key → else branch (line 357) → throws.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_UnrecognizedKey_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"UnknownKey", bool{true}}};

    EXPECT_CALL(*nvswitch, postPatchIO).Times(0);
    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// setL1PowerModePatch: postPatchIO fails inside setL1PowerDevice →
// asyncPatchInProgress reset to false, status=WriteFailure.
TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_PostPatchIOFail_SetsWriteFailure)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWModeControl", bool{true}}};

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));
    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(pm->asyncPatchInProgress);
}

// Type-mismatch tests: cover the !get_if<T> TRUE branch for each key.
// Each test passes the correct key string but a wrong variant type.

TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_HWModeControlWrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // HWModeControl expects bool; pass uint32_t instead
    AsyncSetOperationValueType value =
        PatchValueList{{"HWModeControl", uint32_t{1}}};
    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_FWThrottlingModeWrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"FWThrottlingMode", uint32_t{1}}};
    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_PredictionModeWrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"PredictionMode", uint32_t{1}}};
    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_HWThresholdWrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // HWThreshold expects uint32_t; pass bool instead
    AsyncSetOperationValueType value =
        PatchValueList{{"HWThreshold", bool{true}}};
    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_HWActiveTimeWrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWActiveTime", bool{true}}};
    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_HWInactiveTimeWrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWInactiveTime", bool{true}}};
    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchDIPowerModeUpdateTest,
       SetL1PowerModePatch_HWPredictionInactiveTimeWrongType_Throws)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWPredictionInactiveTime", bool{true}}};
    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// SECTION 14: NSM_NVSwitch factory branches (SupportL1PredictionMode)
// ============================================================================

// Factory creates NSM_NVSwitch type sensors; SupportL1PredictionMode NOT set
// → if (SupportL1PredictionMode) FALSE branch → no L1PredictionMode sensor.
TEST_F(NsmSwitchDIFactoryTest, Factory_NSMNVSwitch_NoL1Prediction_Creates)
{
    const std::string testPath = objPath + "_nvswitch_noL1";
    auto& propMap = utils::MockDbusAsync::propertyMap(testPath, baseIntf);
    propMap["Name"] = std::string("NVSw_noL1");
    propMap["UUID"] = switchUuid;
    propMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/nvswitch_noL1/");
    propMap["Type"] = std::string("NSM_NVSwitch");
    // SupportL1PredictionMode NOT set → returns false (default bool{})

    const size_t before = nvswitchDev->staticSensors.size();
    createNsmSwitchDI(mockManager, baseIntf, testPath);
    // UuidIntf and AssociationDefinitionsInft sensors are added as static
    EXPECT_GT(nvswitchDev->staticSensors.size(), before);
}

// Factory creates NSM_NVSwitch type sensors; SupportL1PredictionMode=true
// → if (SupportL1PredictionMode) TRUE branch → createNsmSwitchL1PredictionMode
// is called, adding an additional roundRobin sensor.
TEST_F(NsmSwitchDIFactoryTest, Factory_NSMNVSwitch_WithL1Prediction_AddsSensor)
{
    const std::string testPath = objPath + "_nvswitch_l1";
    auto& propMap = utils::MockDbusAsync::propertyMap(testPath, baseIntf);
    propMap["Name"] = std::string("NVSw_l1");
    propMap["UUID"] = switchUuid;
    propMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/b12e/nvswitch_l1/");
    propMap["Type"] = std::string("NSM_NVSwitch");
    propMap["SupportL1PredictionMode"] = bool{true};

    const size_t rrBefore = nvswitchDev->roundRobinSensors.size();
    createNsmSwitchDI(mockManager, baseIntf, testPath);
    // L1PredictionMode sensor is added with priority=false → roundRobin
    EXPECT_GT(nvswitchDev->roundRobinSensors.size(), rrBefore);
}
