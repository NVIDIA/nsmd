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

TEST(NsmSwitchIsolationModeHandleResp,
     DISABLED_HandleResponseMsg_NullMsg_ReturnsError)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso7");
    NsmSwitchIsolationMode sensor("IsoSwitch_6", "NSM_NVSwitch", isolationIntf);

    // Act
    auto rc = sensor.handleResponseMsg(nullptr, 100);

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(NsmSwitchIsolationModeHandleResp,
     DISABLED_HandleResponseMsg_ShortLen_ReturnsError)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/iso8");
    NsmSwitchIsolationMode sensor("IsoSwitch_7", "NSM_NVSwitch", isolationIntf);

    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr), 0);
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
     DISABLED_HandleResponseMsg_NullMsg_ReturnsError)
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

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
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
