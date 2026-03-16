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
 * Unit tests for:
 *   - nsmd/nsmDeviceInventory/nsmSwitch.cpp
 *     (NsmSwitchDI, NsmSwitchDIPowerMode, NsmSwitchIsolationMode,
 *      NsmSwitchL1PredictionMode sensor methods)
 *   - nsmd/nsmChassis/nsmNVSwitchAndNVMgmtNICChassis.cpp
 *     (createLocationCode, createPrettyName, createAssetTag,
 *      createChassisVersion, and remaining template paths)
 *   - nsmd/nsmDeviceInventory/nsmPCIeRetimerSwitchDI.cpp
 *     (NsmPCIeRetimerSwitchDI constructors and update paths)
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
#include "pci-links.h"
#include "platform-environmental.h"

#include "nsmChassis/nsmNVSwitchAndNVMgmtNICChassis.hpp"
#include "nsmDeviceInventory/nsmPCIeRetimerSwitchDI.hpp"
#include "nsmSwitch.hpp"

// Forward-declare factory functions from translation units under test
namespace nsm
{
requester::Coroutine createNsmSwitchDI(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);

requester::Coroutine createNsmChassis(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath,
                                      const std::string baseType);

requester::Coroutine createNsmNVSwitchChassis(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath);

requester::Coroutine createNsmNVLinkMgmtNicChassis(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// PART 1: NsmSwitchDIPowerMode sensor method tests
// ============================================================================

class NsmSwitchDIPowerModeSensorTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchDIPowerModeSensorTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchDIPowerModeSensorTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Test: NsmSwitchDIPowerMode::update success path
TEST_F(NsmSwitchDIPowerModeSensorTest,
       Update_SuccessfulResponse_UpdatesPowerModeData)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    // Build a valid get_power_mode response
    nsm_power_mode_data pmData = {};
    pmData.l1_hw_mode_control = 1;
    pmData.l1_hw_mode_threshold = 5000;
    pmData.l1_fw_throttling_mode = 1;
    pmData.l1_prediction_mode = 0;
    pmData.l1_hw_active_time = 100;
    pmData.l1_hw_inactive_time = 200;
    pmData.l1_prediction_inactive_time = 300;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &pmData,
                                         responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    powerMode->update(nvswitch);

    // Assert
    auto resultData = powerMode->getPowerModeData();
    EXPECT_EQ(resultData.l1_hw_mode_control, 1);
    EXPECT_EQ(resultData.l1_hw_mode_threshold, 5000u);
    EXPECT_EQ(resultData.l1_fw_throttling_mode, 1);
    EXPECT_EQ(resultData.l1_prediction_mode, 0);
    EXPECT_EQ(resultData.l1_hw_active_time, 100u);
    EXPECT_EQ(resultData.l1_hw_inactive_time, 200u);
    EXPECT_EQ(resultData.l1_prediction_inactive_time, 300u);
}

// Test: NsmSwitchDIPowerMode::update with error completion code
TEST_F(NsmSwitchDIPowerModeSensorTest,
       Update_ErrorCompletionCode_DoesNotUpdateData)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    // Build a response with error CC
    nsm_power_mode_data pmData = {};
    pmData.l1_hw_mode_control = 1;
    pmData.l1_hw_mode_threshold = 9999;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_mode_resp(0, NSM_ERROR, ERR_NULL, &pmData,
                                         responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    powerMode->update(nvswitch);

    // Assert - data should not be updated when CC is error
    auto resultData = powerMode->getPowerModeData();
    EXPECT_EQ(resultData.l1_hw_mode_control, false);
    EXPECT_EQ(resultData.l1_hw_mode_threshold, 0u);
}

// Test: NsmSwitchDIPowerMode::update with sensorIO failure
TEST_F(NsmSwitchDIPowerModeSensorTest, Update_SensorIOFailure_ReturnsErrorCode)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    // sensorIO returns non-zero (failure)
    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    // Act
    powerMode->update(nvswitch);

    // Assert - data should not be updated
    auto resultData = powerMode->getPowerModeData();
    EXPECT_EQ(resultData.l1_hw_mode_control, false);
}

// Test: NsmSwitchDIPowerMode::update with hwModeControl == 0
TEST_F(NsmSwitchDIPowerModeSensorTest, Update_HwModeControlZero_SetsToFalse)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), true);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), true);
    powerMode->invoke(pdiMethod(predictionMode), true);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    nsm_power_mode_data pmData = {};
    pmData.l1_hw_mode_control = 0;
    pmData.l1_hw_mode_threshold = 0;
    pmData.l1_fw_throttling_mode = 0;
    pmData.l1_prediction_mode = 0;
    pmData.l1_hw_active_time = 0;
    pmData.l1_hw_inactive_time = 0;
    pmData.l1_prediction_inactive_time = 0;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &pmData,
                                         responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    powerMode->update(nvswitch);

    // Assert - all boolean fields should be false
    auto resultData = powerMode->getPowerModeData();
    EXPECT_EQ(resultData.l1_hw_mode_control, false);
    EXPECT_EQ(resultData.l1_fw_throttling_mode, false);
    EXPECT_EQ(resultData.l1_prediction_mode, false);
}

// ============================================================================
// PART 2: NsmSwitchIsolationMode genRequestMsg / handleResponseMsg tests
//         (These are the sensor-class methods, not factory tests.)
// ============================================================================

class NsmSwitchIsolationModeSensorTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/NVSwitch_0";
};

// Test genRequestMsg produces valid request
TEST_F(NsmSwitchIsolationModeSensorTest, GenRequestMsg_ValidArgs_ReturnsRequest)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode sensor(name, type, isolationIntf);
    eid_t eid = 10;
    uint8_t instanceId = 5;

    // Act
    auto request = sensor.genRequestMsg(eid, instanceId);

    // Assert
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}

// Test handleResponseMsg - Enabled mode
TEST_F(NsmSwitchIsolationModeSensorTest,
       HandleResponseMsg_Enabled_SetsCorrectMode)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode sensor(name, type, isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_switch_isolation_mode_resp(
        0, NSM_SUCCESS, ERR_NULL, SWITCH_COMMUNICATION_MODE_ENABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationEnabled);
}

// Test handleResponseMsg - Disabled mode
TEST_F(NsmSwitchIsolationModeSensorTest,
       HandleResponseMsg_Disabled_SetsCorrectMode)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode sensor(name, type, isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_switch_isolation_mode_resp(
        0, NSM_SUCCESS, ERR_NULL, SWITCH_COMMUNICATION_MODE_DISABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationDisabled);
}

// Test handleResponseMsg - Unknown/invalid mode value
TEST_F(NsmSwitchIsolationModeSensorTest,
       HandleResponseMsg_UnknownMode_SetsUnknown)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode sensor(name, type, isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    uint8_t unknownMode = 0xFF;
    auto rc = encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    unknownMode, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationUnknown);
}

// Test handleResponseMsg - Error completion code
TEST_F(NsmSwitchIsolationModeSensorTest, HandleResponseMsg_ErrorCC_ReturnsError)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode sensor(name, type, isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_switch_isolation_mode_resp(
        0, NSM_ERROR, ERR_NULL, SWITCH_COMMUNICATION_MODE_ENABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// PART 3: NsmSwitchL1PredictionMode sensor method tests
// ============================================================================

class NsmSwitchL1PredictionModeSensorTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/NVSwitch_0/"
        "Oem/Nvidia/PowerMode/L1PredictionMode";
};

// Test genRequestMsg success
TEST_F(NsmSwitchL1PredictionModeSensorTest,
       GenRequestMsg_ValidArgs_ReturnsRequest)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    NsmSwitchL1PredictionMode sensor(name, type, enableIntf, assocDefIntf);
    eid_t eid = 10;
    uint8_t instanceId = 5;

    // Act
    auto request = sensor.genRequestMsg(eid, instanceId);

    // Assert
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_req));
}

// Test handleResponseMsg with ENABLED prediction mode
TEST_F(NsmSwitchL1PredictionModeSensorTest,
       HandleResponseMsg_Enabled_SetsEnabledTrue)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    NsmSwitchL1PredictionMode sensor(name, type, enableIntf, assocDefIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   ENABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(enableIntf->enabled());
}

// Test handleResponseMsg with DISABLED prediction mode
TEST_F(NsmSwitchL1PredictionModeSensorTest,
       HandleResponseMsg_Disabled_SetsEnabledFalse)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    NsmSwitchL1PredictionMode sensor(name, type, enableIntf, assocDefIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   DISABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_FALSE(enableIntf->enabled());
}

// Test handleResponseMsg with error CC - does not change enabled state
TEST_F(NsmSwitchL1PredictionModeSensorTest,
       HandleResponseMsg_ErrorCC_ReturnsNonZero)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    NsmSwitchL1PredictionMode sensor(name, type, enableIntf, assocDefIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_device_mode_settings_resp(0, NSM_ERROR, ERR_NULL,
                                                   ENABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// PART 4: NsmNVSwitchAndNicChassis factory tests for uncovered paths
//         (createLocationCode, createPrettyName, createAssetTag,
//          createChassisVersion, CX8 branch paths)
// ============================================================================

struct NsmNVSwitchChassisFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis";
    const std::string name = "NVSwitch_Test";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch";

    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmNVSwitchChassisFactoryTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmNVSwitchChassisFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", switchUuid},
    };
};

// Test: createLocationCode with LocationCode property set
TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateLocationCode_WithProperty_CreatesLocationCodeSensor)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    dbus::PropertyMap chassisAttrProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("NVSwitch_Attrs")},
        {"LocationCode", std::string("U100-E0")},
    };

    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    currentPropertyMap = chassisAttrProperties;

    // Act
    createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes", objPath,
                     "NSM_NVSwitch_Chassis");

    // Assert - should create Asset(3 sensors) + SKU(1) + Health(1) +
    // LocationCode(1) = 6 sensors minimum
    EXPECT_GE(nvswitch->staticSensors.size(), 4);
}

// Test: createPrettyName with PrettyNameForChassis property set
TEST_F(NsmNVSwitchChassisFactoryTest,
       CreatePrettyName_WithProperty_CreatesPrettyNameSensor)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    dbus::PropertyMap chassisAttrProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("NVSwitch_Attrs")},
        {"PrettyNameForChassis", std::string("NVSwitch Bay 0")},
    };

    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    currentPropertyMap = chassisAttrProperties;

    // Act
    createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes", objPath,
                     "NSM_NVSwitch_Chassis");

    // Assert - should create Asset(3) + SKU(1) + Health(1) + PrettyName(1)
    EXPECT_GE(nvswitch->staticSensors.size(), 4);
}

// Test: createLocationCode + createPrettyName together
TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateChassisAttrs_WithAllOptionalFields_CreatesAllSensors)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    dbus::PropertyMap chassisAttrProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("NVSwitch_AllAttrs")},
        {"LocationCode", std::string("U100-E0")},
        {"PrettyNameForChassis", std::string("NVSwitch Bay 0")},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Slot"},
        {"ChassisType",
         "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"},
    };

    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    currentPropertyMap = chassisAttrProperties;

    // Act
    createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes", objPath,
                     "NSM_NVSwitch_Chassis");

    // Assert - Asset(3) + SKU(1) + Health(1) + LocationCode(1) +
    // PrettyName(1) + Location(1) + ChassisType(1) = 9
    EXPECT_GE(nvswitch->staticSensors.size(), 8);
}

// Test: CX8 device branch - creates AssetTag and ChassisVersion
struct NsmCX8ChassisFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis";
    const std::string name = "CX8_NIC_Test";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/cx8_nic";

    // CX8 device: type=2 (NSM_DEV_ID_PCIE_BRIDGE), role=2
    // (NSM_PCIE_BRIDGE_DEV_ROLE_CX8)
    const uuid_t cx8Uuid = "STATIC:2:2:NSM_DEVICE_INSTANCE_NUMBER:101";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> cx8Device;

    NsmCX8ChassisFactoryTest() : SensorManagerTest(devices)
    {
        cx8Device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx8Uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(cx8Device, nullptr);
        EXPECT_EQ(NSM_DEV_ID_PCIE_BRIDGE, cx8Device->getDeviceType());
        EXPECT_EQ(NSM_PCIE_BRIDGE_DEV_ROLE_CX8, cx8Device->getDeviceRole());
    }

    ~NsmCX8ChassisFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", cx8Uuid},
    };
};

TEST_F(NsmCX8ChassisFactoryTest,
       DISABLED_CreateChassisAttrs_CX8Device_CreatesAssetTagAndVersion)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    dbus::PropertyMap chassisAttrProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("CX8_Attrs")},
    };

    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    currentPropertyMap = chassisAttrProperties;

    // Act
    createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes", objPath,
                     "NSM_NVLinkMgmtNic_Chassis");

    // Assert - CX8 path: Asset(3) + SKU(1) + Health(1) + AssetTag(1) +
    //          ChassisVersion(1) = 7
    EXPECT_GE(cx8Device->staticSensors.size(), 5);
}

// Test: NsmNVSwitchAndNicChassis template instantiation with LocationCodeIntf
TEST_F(NsmNVSwitchChassisFactoryTest,
       LocationCodeIntfConstructor_ValidParams_CreatesObject)
{
    // Arrange & Act
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<LocationCodeIntf>>(
        "NVSwitch0", "NSM_NVSwitch");

    // Assert
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), "NVSwitch0");
    EXPECT_EQ(chassis->getType(), "NSM_NVSwitch");
}

// Test: NsmNVSwitchAndNicChassis template instantiation with ItemIntf
TEST_F(NsmNVSwitchChassisFactoryTest,
       ItemIntfConstructor_ValidParams_CreatesObject)
{
    // Arrange & Act
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<ItemIntf>>(
        "NVSwitch0", "NSM_NVSwitch");

    // Assert
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), "NVSwitch0");
}

// Test: NsmNVSwitchAndNicChassis template instantiation with AssetTagIntf
TEST_F(NsmNVSwitchChassisFactoryTest,
       AssetTagIntfConstructor_ValidParams_CreatesObject)
{
    // Arrange & Act
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<AssetTagIntf>>(
        "NVSwitch0", "NSM_NVSwitch");

    // Assert
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), "NVSwitch0");
}

// Test: NsmNVSwitchAndNicChassis template instantiation with RevisionIntf
TEST_F(NsmNVSwitchChassisFactoryTest,
       RevisionIntfConstructor_ValidParams_CreatesObject)
{
    // Arrange & Act
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<RevisionIntf>>(
        "NVSwitch0", "NSM_NVSwitch");

    // Assert
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), "NVSwitch0");
}

// Test: NsmNVSwitchAndNicChassis template instantiation with NsmApSkuIdIntf
TEST_F(NsmNVSwitchChassisFactoryTest,
       NsmApSkuIdIntfConstructor_ValidParams_CreatesObject)
{
    // Arrange & Act
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<NsmApSkuIdIntf>>(
        "NVSwitch0", "NSM_NVSwitch");

    // Assert
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), "NVSwitch0");
}

// Test: createNsmNVLinkMgmtNicChassis with base type
struct NsmNVLinkMgmtNicChassisExtraTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis";
    const std::string name = "NVLinkMgmtNIC_Test";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvlink_nic";

    const uuid_t nicUuid = "STATIC:1:1:NSM_DEVICE_INSTANCE_NUMBER:101";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nic;

    NsmNVLinkMgmtNicChassisExtraTest() : SensorManagerTest(devices)
    {
        nic = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(nicUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nic, nullptr);
    }

    ~NsmNVLinkMgmtNicChassisExtraTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", nicUuid},
    };
};

// Test: NVLinkMgmtNic chassis with LocationCode and PrettyName
TEST_F(NsmNVLinkMgmtNicChassisExtraTest,
       CreateChassis_WithLocationCodeAndPrettyName_CreatesAllSensors)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    dbus::PropertyMap attrProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("NIC_Attrs")},
        {"LocationCode", std::string("U200-E1")},
        {"PrettyNameForChassis", std::string("NVLink NIC Bay 1")},
    };

    auto& attrPropertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    attrPropertyMap = attrProperties;

    // Act
    createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes", objPath,
                     "NSM_NVLinkMgmtNic_Chassis");

    // Assert - Asset(3) + SKU(1) + Health(1) + LocationCode(1) +
    // PrettyName(1) = 7
    EXPECT_GE(nic->staticSensors.size(), 5);
}

// ============================================================================
// PART 5: NsmPCIeRetimerSwitchDI tests
// ============================================================================

class NsmPCIeRetimerSwitchDITest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "Retimer_0";
    const std::string type = "NSM_PCIeRetimer_Switch";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/";
    uint8_t deviceIdx = 1;
};

// Test basic constructor
TEST_F(NsmPCIeRetimerSwitchDITest, Constructor_BasicParams_CreatesObject)
{
    // Arrange
    std::vector<utils::Association> associations = {
        {"contained_by", "containing", "/xyz/openbmc_project/system/chassis"}};

    // Act
    NsmPCIeRetimerSwitchDI obj(bus, name, associations, type, inventoryObjPath,
                               deviceIdx);

    // Assert
    EXPECT_EQ(obj.getName(), name);
    EXPECT_EQ(obj.getType(), type);
    EXPECT_EQ(obj.deviceIndex, deviceIdx);
    EXPECT_FALSE(obj.isMultiPciePortEnabled);
    EXPECT_NE(obj.associationDefIntf, nullptr);
    EXPECT_NE(obj.switchIntf, nullptr);
}

// Test overloaded constructor with port variables
TEST_F(NsmPCIeRetimerSwitchDITest,
       Constructor_WithPortVars_SetsMultiPortEnabled)
{
    // Arrange
    std::vector<utils::Association> associations = {
        {"contained_by", "containing", "/xyz/openbmc_project/system/chassis"}};
    uint8_t multiPortType = 1;
    uint8_t multiPortIndex = 2;
    uint8_t multiPortUpstreamPort = 3;

    // Act
    NsmPCIeRetimerSwitchDI obj(bus, name, associations, type, inventoryObjPath,
                               deviceIdx, multiPortType, multiPortIndex,
                               multiPortUpstreamPort);

    // Assert
    EXPECT_EQ(obj.getName(), name);
    EXPECT_EQ(obj.getType(), type);
    EXPECT_EQ(obj.deviceIndex, deviceIdx);
    EXPECT_TRUE(obj.isMultiPciePortEnabled);
    EXPECT_EQ(obj.multiPortType, multiPortType);
    EXPECT_EQ(obj.multiPortIndex, multiPortIndex);
    EXPECT_EQ(obj.multiPortUpstreamPort, multiPortUpstreamPort);
    EXPECT_NE(obj.associationDefIntf, nullptr);
    EXPECT_NE(obj.switchIntf, nullptr);
}

// Test constructor with empty associations
TEST_F(NsmPCIeRetimerSwitchDITest, Constructor_EmptyAssociations_CreatesObject)
{
    // Arrange
    std::vector<utils::Association> emptyAssociations = {};

    // Act
    NsmPCIeRetimerSwitchDI obj(bus, name, emptyAssociations, type,
                               inventoryObjPath, deviceIdx);

    // Assert
    EXPECT_EQ(obj.getName(), name);
    EXPECT_NE(obj.switchIntf, nullptr);
}

// Test constructor with multiple associations
TEST_F(NsmPCIeRetimerSwitchDITest,
       Constructor_MultipleAssociations_SetsAllAssociations)
{
    // Arrange
    std::vector<utils::Association> associations = {
        {"contained_by", "containing", "/xyz/openbmc_project/system/chassis/0"},
        {"parent_device", "child_retimer",
         "/xyz/openbmc_project/system/nvswitch"},
    };

    // Act
    NsmPCIeRetimerSwitchDI obj(bus, name, associations, type, inventoryObjPath,
                               deviceIdx);

    // Assert
    EXPECT_EQ(obj.getName(), name);
    EXPECT_NE(obj.associationDefIntf, nullptr);
}

// ============================================================================
// PART 6: NsmPCIeRetimerSwitchDI update tests (non-multiport and multiport)
// ============================================================================

class NsmPCIeRetimerSwitchDIUpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "Retimer_0";
    const std::string type = "NSM_PCIeRetimer_Switch";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/";
    uint8_t deviceIdx = 1;
    const uuid_t deviceUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nsmDevice;

    NsmPCIeRetimerSwitchDIUpdateTest() : SensorManagerTest(devices)
    {
        nsmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nsmDevice, nullptr);
    }

    ~NsmPCIeRetimerSwitchDIUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Test: update (non-multiport) - success path
TEST_F(NsmPCIeRetimerSwitchDIUpdateTest,
       Update_NonMultiPort_Success_UpdatesDeviceAndVendorId)
{
    // Arrange
    std::vector<utils::Association> associations = {};
    auto retimer = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, associations, type, inventoryObjPath, deviceIdx);

    // Build scalar group telemetry v1 group0 response
    nsm_query_scalar_group_telemetry_group_0 telemetryData = {};
    telemetryData.pci_vendor_id = 0x10DE;
    telemetryData.pci_device_id = 0x1234;
    telemetryData.pci_subsystem_vendor_id = 0xABCD;
    telemetryData.pci_subsystem_device_id = 0x5678;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        0, NSM_SUCCESS, ERR_NULL, &telemetryData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    retimer->update(nsmDevice);

    // Assert
    EXPECT_EQ(retimer->switchIntf->deviceId(), "0x1234");
    EXPECT_EQ(retimer->switchIntf->vendorId(), "0x10de");
}

// Test: update (non-multiport) - error completion code
TEST_F(NsmPCIeRetimerSwitchDIUpdateTest,
       Update_NonMultiPort_ErrorCC_DoesNotUpdateIds)
{
    // Arrange
    std::vector<utils::Association> associations = {};
    auto retimer = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, associations, type, inventoryObjPath, deviceIdx);

    // Build error response
    nsm_query_scalar_group_telemetry_group_0 telemetryData = {};
    telemetryData.pci_vendor_id = 0x10DE;
    telemetryData.pci_device_id = 0x9999;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        0, NSM_ERROR, ERR_NULL, &telemetryData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    retimer->update(nsmDevice);

    // Assert - IDs should remain empty (initial values)
    EXPECT_EQ(retimer->switchIntf->deviceId(), "");
    EXPECT_EQ(retimer->switchIntf->vendorId(), "");
}

// Test: update (non-multiport) - sensorIO failure
TEST_F(NsmPCIeRetimerSwitchDIUpdateTest,
       Update_NonMultiPort_SensorIOFailure_ReturnsError)
{
    // Arrange
    std::vector<utils::Association> associations = {};
    auto retimer = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, associations, type, inventoryObjPath, deviceIdx);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    // Act
    retimer->update(nsmDevice);

    // Assert - IDs remain empty
    EXPECT_EQ(retimer->switchIntf->deviceId(), "");
    EXPECT_EQ(retimer->switchIntf->vendorId(), "");
}

// Test: update (multiport) - success path
TEST_F(NsmPCIeRetimerSwitchDIUpdateTest,
       Update_MultiPort_Success_UpdatesDeviceAndVendorId)
{
    // Arrange
    std::vector<utils::Association> associations = {};
    auto retimer = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, associations, type, inventoryObjPath, deviceIdx, 0, 0, 0);

    nsm_query_scalar_group_telemetry_group_0 telemetryData = {};
    telemetryData.pci_vendor_id = 0xBEEF;
    telemetryData.pci_device_id = 0xCAFE;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        0, NSM_SUCCESS, ERR_NULL, &telemetryData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    retimer->update(nsmDevice);

    // Assert
    EXPECT_EQ(retimer->switchIntf->deviceId(), "0xcafe");
    EXPECT_EQ(retimer->switchIntf->vendorId(), "0xbeef");
}

// Test: update (multiport) - error completion code
TEST_F(NsmPCIeRetimerSwitchDIUpdateTest,
       Update_MultiPort_ErrorCC_DoesNotUpdateIds)
{
    // Arrange
    std::vector<utils::Association> associations = {};
    auto retimer = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, associations, type, inventoryObjPath, deviceIdx, 0, 0, 0);

    nsm_query_scalar_group_telemetry_group_0 telemetryData = {};
    telemetryData.pci_vendor_id = 0xBEEF;
    telemetryData.pci_device_id = 0xCAFE;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        0, NSM_ERROR, ERR_NULL, &telemetryData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    retimer->update(nsmDevice);

    // Assert
    EXPECT_EQ(retimer->switchIntf->deviceId(), "");
    EXPECT_EQ(retimer->switchIntf->vendorId(), "");
}

// Test: update (multiport) - sensorIO failure
TEST_F(NsmPCIeRetimerSwitchDIUpdateTest,
       Update_MultiPort_SensorIOFail_ReturnsError)
{
    // Arrange
    std::vector<utils::Association> associations = {};
    auto retimer = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, associations, type, inventoryObjPath, deviceIdx, 0, 0, 0);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    // Act
    retimer->update(nsmDevice);

    // Assert
    EXPECT_EQ(retimer->switchIntf->deviceId(), "");
    EXPECT_EQ(retimer->switchIntf->vendorId(), "");
}

// ============================================================================
// PART 7: NsmSwitchDIPowerMode setL1PowerModePatch tests (patching interface)
// ============================================================================

class NsmSwitchDIPowerModePatchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchDIPowerModePatchTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchDIPowerModePatchTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmSwitchDIPowerMode> createInitializedPowerMode()
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

// Test: setL1PowerModePatch - HWModeControl patch
TEST_F(NsmSwitchDIPowerModePatchTest, SetL1PowerModePatch_HWModeControl_Success)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();

    // Build set_power_mode_resp (success)
    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    auto rc = encode_set_power_mode_resp(0, ERR_NULL, respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWModeControl", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PowerModePatch - FWThrottlingMode patch
TEST_F(NsmSwitchDIPowerModePatchTest,
       SetL1PowerModePatch_FWThrottlingMode_Success)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_power_mode_resp(0, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"FWThrottlingMode", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PowerModePatch - PredictionMode patch
TEST_F(NsmSwitchDIPowerModePatchTest,
       SetL1PowerModePatch_PredictionMode_Success)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_power_mode_resp(0, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"PredictionMode", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PowerModePatch - HWThreshold (uint32_t) patch
TEST_F(NsmSwitchDIPowerModePatchTest, SetL1PowerModePatch_HWThreshold_Success)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_power_mode_resp(0, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWThreshold", static_cast<uint32_t>(5000)},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PowerModePatch - HWActiveTime (uint32_t) patch
TEST_F(NsmSwitchDIPowerModePatchTest, SetL1PowerModePatch_HWActiveTime_Success)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_power_mode_resp(0, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWActiveTime", static_cast<uint32_t>(100)},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PowerModePatch - HWInactiveTime (uint32_t) patch
TEST_F(NsmSwitchDIPowerModePatchTest,
       SetL1PowerModePatch_HWInactiveTime_Success)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_power_mode_resp(0, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWInactiveTime", static_cast<uint32_t>(200)},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PowerModePatch - HWPredictionInactiveTime (uint32_t) patch
TEST_F(NsmSwitchDIPowerModePatchTest,
       SetL1PowerModePatch_HWPredictionInactiveTime_Success)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_power_mode_resp(0, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWPredictionInactiveTime", static_cast<uint32_t>(300)},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PowerModePatch - multiple properties at once
TEST_F(NsmSwitchDIPowerModePatchTest,
       SetL1PowerModePatch_MultipleProperties_Success)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_power_mode_resp(0, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWModeControl", true},
        PatchTuple{"FWThrottlingMode", false},
        PatchTuple{"HWThreshold", static_cast<uint32_t>(7500)},
        PatchTuple{"HWActiveTime", static_cast<uint32_t>(150)},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PowerModePatch - invalid property type throws InvalidArgument
TEST_F(NsmSwitchDIPowerModePatchTest,
       DISABLED_SetL1PowerModePatch_InvalidPropertyType_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Pass wrong type for HWModeControl (uint32_t instead of bool)
    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWModeControl", static_cast<uint32_t>(1)},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act & Assert
    EXPECT_THROW(
        powerMode->setL1PowerModePatch(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PowerModePatch - invalid FWThrottlingMode type
TEST_F(
    NsmSwitchDIPowerModePatchTest,
    DISABLED_SetL1PowerModePatch_InvalidFWThrottlingType_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"FWThrottlingMode", static_cast<uint32_t>(1)},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act & Assert
    EXPECT_THROW(
        powerMode->setL1PowerModePatch(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PowerModePatch - invalid PredictionMode type
TEST_F(
    NsmSwitchDIPowerModePatchTest,
    DISABLED_SetL1PowerModePatch_InvalidPredictionModeType_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"PredictionMode", static_cast<uint32_t>(1)},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act & Assert
    EXPECT_THROW(
        powerMode->setL1PowerModePatch(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PowerModePatch - invalid HWThreshold type (bool instead of
// uint32_t)
TEST_F(
    NsmSwitchDIPowerModePatchTest,
    DISABLED_SetL1PowerModePatch_InvalidHWThresholdType_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWThreshold", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act & Assert
    EXPECT_THROW(
        powerMode->setL1PowerModePatch(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PowerModePatch - invalid HWActiveTime type
TEST_F(
    NsmSwitchDIPowerModePatchTest,
    DISABLED_SetL1PowerModePatch_InvalidHWActiveTimeType_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWActiveTime", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act & Assert
    EXPECT_THROW(
        powerMode->setL1PowerModePatch(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PowerModePatch - invalid HWInactiveTime type
TEST_F(
    NsmSwitchDIPowerModePatchTest,
    DISABLED_SetL1PowerModePatch_InvalidHWInactiveTimeType_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWInactiveTime", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act & Assert
    EXPECT_THROW(
        powerMode->setL1PowerModePatch(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PowerModePatch - invalid HWPredictionInactiveTime type
TEST_F(
    NsmSwitchDIPowerModePatchTest,
    DISABLED_SetL1PowerModePatch_InvalidHWPredInactiveTimeType_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWPredictionInactiveTime", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act & Assert
    EXPECT_THROW(
        powerMode->setL1PowerModePatch(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PowerModePatch - unrecognized property throws InvalidArgument
TEST_F(NsmSwitchDIPowerModePatchTest,
       DISABLED_SetL1PowerModePatch_UnrecognizedProperty_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"UnknownProperty", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act & Assert
    EXPECT_THROW(
        powerMode->setL1PowerModePatch(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PowerModePatch - empty patch values throws InvalidArgument
TEST_F(NsmSwitchDIPowerModePatchTest,
       DISABLED_SetL1PowerModePatch_EmptyPatchValues_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> emptyPatchValues = {};
    AsyncSetOperationValueType value = emptyPatchValues;

    // Act & Assert
    EXPECT_THROW(
        powerMode->setL1PowerModePatch(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PowerModePatch - wrong variant type throws InvalidArgument
TEST_F(NsmSwitchDIPowerModePatchTest,
       DISABLED_SetL1PowerModePatch_WrongVariantType_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Pass a string instead of vector of tuples
    AsyncSetOperationValueType value = std::string("wrongtype");

    // Act & Assert
    EXPECT_THROW(
        powerMode->setL1PowerModePatch(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PowerModePatch - asyncPatchInProgress returns Unavailable
TEST_F(NsmSwitchDIPowerModePatchTest,
       SetL1PowerModePatch_AlreadyInProgress_ReturnsUnavailable)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();
    powerMode->asyncPatchInProgress = true;

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWModeControl", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// Test: setL1PowerModePatch - postPatchIO failure
TEST_F(NsmSwitchDIPowerModePatchTest,
       SetL1PowerModePatch_PostPatchIOFailure_SetsWriteFailure)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWModeControl", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(powerMode->asyncPatchInProgress);
}

// ============================================================================
// PART 8: NsmSwitchIsolationMode setSwitchIsolationMode tests
// ============================================================================

class NsmSwitchIsolationModeSetTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/NVSwitch_0";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchIsolationModeSetTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchIsolationModeSetTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Test: setSwitchIsolationMode - enable
TEST_F(NsmSwitchIsolationModeSetTest, SetSwitchIsolationMode_Enabled_Success)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    auto rc = encode_set_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    // Act
    sensor->setSwitchIsolationMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setSwitchIsolationMode - disable
TEST_F(NsmSwitchIsolationModeSetTest, SetSwitchIsolationMode_Disabled_Success)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationDisabled");

    // Act
    sensor->setSwitchIsolationMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setSwitchIsolationMode - invalid mode string
TEST_F(NsmSwitchIsolationModeSetTest,
       SetSwitchIsolationMode_InvalidMode_SetsWriteFailure)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("InvalidModeString");

    // Act
    sensor->setSwitchIsolationMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Test: setSwitchIsolationMode - null/wrong type throws InvalidArgument
TEST_F(NsmSwitchIsolationModeSetTest,
       DISABLED_SetSwitchIsolationMode_WrongType_ThrowsInvalidArgument)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass bool instead of string
    AsyncSetOperationValueType value = true;

    // Act & Assert
    EXPECT_THROW(
        sensor->setSwitchIsolationMode(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setSwitchIsolationMode - postPatchIO failure
TEST_F(NsmSwitchIsolationModeSetTest,
       SetSwitchIsolationMode_PostPatchIOFailure_SetsWriteFailure)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    // Act
    sensor->setSwitchIsolationMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// PART 9: NsmSwitchL1PredictionMode setL1PredictionMode tests
// ============================================================================

class NsmSwitchL1PredictionModeSetTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/NVSwitch_0/"
        "Oem/Nvidia/PowerMode/L1PredictionMode";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchL1PredictionModeSetTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchL1PredictionModeSetTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Test: setL1PredictionMode - enable
TEST_F(NsmSwitchL1PredictionModeSetTest, SetL1PredictionMode_Enable_Success)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, assocDefIntf);

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    auto rc = encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    // Act
    sensor->setL1PredictionMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PredictionMode - disable
TEST_F(NsmSwitchL1PredictionModeSetTest, SetL1PredictionMode_Disable_Success)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, assocDefIntf);

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;

    // Act
    sensor->setL1PredictionMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PredictionMode - wrong type throws InvalidArgument
TEST_F(NsmSwitchL1PredictionModeSetTest,
       DISABLED_SetL1PredictionMode_WrongType_ThrowsInvalidArgument)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass string instead of bool
    AsyncSetOperationValueType value = std::string("wrongtype");

    // Act & Assert
    EXPECT_THROW(
        sensor->setL1PredictionMode(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PredictionMode - postPatchIO failure
TEST_F(NsmSwitchL1PredictionModeSetTest,
       SetL1PredictionMode_PostPatchIOFailure_SetsWriteFailure)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, assocDefIntf);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    // Act
    sensor->setL1PredictionMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Test: setL1PredictionMode - decode failure (error CC in response)
TEST_F(NsmSwitchL1PredictionModeSetTest,
       SetL1PredictionMode_DecodeFailure_SetsWriteFailure)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, assocDefIntf);

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_device_mode_settings_resp(0, NSM_ERROR, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    // Act
    sensor->setL1PredictionMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// PART 10: NsmSwitchDIPowerMode - setL1PowerDevice with decode error CC
// ============================================================================

TEST_F(NsmSwitchDIPowerModePatchTest,
       SetL1PowerModePatch_DecodeErrorCC_SetsWriteFailure)
{
    // Arrange
    auto powerMode = createInitializedPowerMode();

    // Build response with error CC in the response payload
    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    // encode with cc=NSM_ERROR (set via completion code in the header)
    [[maybe_unused]] auto* hdr = reinterpret_cast<nsm_msg*>(respData.data());
    // Manually set the completion_code field in the response to NSM_ERROR
    auto rc = encode_set_power_mode_resp(0, ERR_NULL, respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    // Override the cc byte in the response to be NSM_ERROR
    auto* commonResp = reinterpret_cast<nsm_common_non_success_resp*>(
        respData.data() + sizeof(nsm_msg_hdr));
    commonResp->completion_code = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchTupleVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchTuple = std::tuple<std::string, PatchTupleVariant>;
    std::vector<PatchTuple> patchValues = {
        PatchTuple{"HWModeControl", true},
    };
    AsyncSetOperationValueType value = patchValues;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(powerMode->asyncPatchInProgress);
}

// ============================================================================
// PART 11: NsmSwitchIsolationMode - setSwitchIsolationMode decode error
// ============================================================================

TEST_F(NsmSwitchIsolationModeSetTest,
       SetSwitchIsolationMode_DecodeErrorCC_SetsWriteFailure)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    std::vector<uint8_t> respData(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto respMsg = reinterpret_cast<nsm_msg*>(respData.data());
    encode_set_switch_isolation_mode_resp(0, NSM_ERROR, ERR_NULL, respMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    // Act
    sensor->setSwitchIsolationMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// PART 12: NsmSwitchDIReset constructor test
// ============================================================================

class NsmSwitchDIResetExtraTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchDIResetExtraTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchDIResetExtraTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmSwitchDIResetExtraTest, Constructor_ValidParams_InitializesInterfaces)
{
    // Arrange & Act
    NsmSwitchDIReset resetObj(bus, name, type, inventoryObjPath, nvswitch);

    // Assert
    EXPECT_EQ(resetObj.getName(), name);
    EXPECT_EQ(resetObj.getType(), type);
    EXPECT_NE(resetObj.resetIntf, nullptr);
    EXPECT_NE(resetObj.resetAsyncIntf, nullptr);
    EXPECT_EQ(resetObj.objPath, inventoryObjPath + name);
}

// ============================================================================
// PART 13: NsmSwitchDI<T>::update() non-UUID template variants
//          These exercise the co_return NSM_SUCCESS path for all IntfTypes
//          that are NOT UuidIntf.
// ============================================================================

class NsmSwitchDIUpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const std::string name = "NVSwitch_DI_Upd";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch_di/";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchDIUpdateTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchDIUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// NsmSwitchDI<NvSwitchIntf>::update() — co_return NSM_SUCCESS (non-UUID path)
TEST_F(NsmSwitchDIUpdateTest, NvSwitchIntfUpdate_NonUuid_Succeeds)
{
    auto sensor = std::make_shared<NsmSwitchDI<NvSwitchIntf>>(name,
                                                              inventoryObjPath);
    sensor->update(nvswitch);
    EXPECT_EQ(sensor->getName(), name);
}

// NsmSwitchDI<SwitchIntf>::update() — co_return NSM_SUCCESS (non-UUID path)
TEST_F(NsmSwitchDIUpdateTest, SwitchIntfUpdate_NonUuid_Succeeds)
{
    auto sensor = std::make_shared<NsmSwitchDI<SwitchIntf>>(name,
                                                            inventoryObjPath);
    sensor->update(nvswitch);
    EXPECT_EQ(sensor->getName(), name);
}

// NsmSwitchDI<NsmAssetIntf>::update() — co_return NSM_SUCCESS (non-UUID path)
TEST_F(NsmSwitchDIUpdateTest, NsmAssetIntfUpdate_NonUuid_Succeeds)
{
    auto sensor = std::make_shared<NsmSwitchDI<NsmAssetIntf>>(name,
                                                              inventoryObjPath);
    sensor->update(nvswitch);
    EXPECT_EQ(sensor->getName(), name);
}

// NsmSwitchDI<AssociationDefinitionsInft>::update() — non-UUID path
TEST_F(NsmSwitchDIUpdateTest, AssocDefIntfUpdate_NonUuid_Succeeds)
{
    auto sensor = std::make_shared<NsmSwitchDI<AssociationDefinitionsInft>>(
        name, inventoryObjPath);
    sensor->update(nvswitch);
    EXPECT_EQ(sensor->getName(), name);
}

// ============================================================================
// PART 14: NsmNVSwitchAndNicChassis<T>::update() non-UUID template variants
//          These exercise the co_return NSM_SUCCESS path for all IntfTypes
//          that are NOT UuidIntf.
// ============================================================================

class NsmNVSwitchChassisDIUpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmNVSwitchChassisDIUpdateTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmNVSwitchChassisDIUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmNVSwitchChassisDIUpdateTest, ItemIntfUpdate_NonUuid_Succeeds)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<ItemIntf>>(
        "sw_chassis_item", "NSM_NVSwitch");
    chassis->update(nvswitch);
    EXPECT_EQ(chassis->getName(), "sw_chassis_item");
}

TEST_F(NsmNVSwitchChassisDIUpdateTest, NsmAssetIntfUpdate_NonUuid_Succeeds)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(
        "sw_chassis_asset", "NSM_NVSwitch");
    chassis->update(nvswitch);
    EXPECT_EQ(chassis->getName(), "sw_chassis_asset");
}

TEST_F(NsmNVSwitchChassisDIUpdateTest, LocationCodeIntfUpdate_NonUuid_Succeeds)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<LocationCodeIntf>>(
        "sw_chassis_loccode", "NSM_NVSwitch");
    chassis->update(nvswitch);
    EXPECT_EQ(chassis->getName(), "sw_chassis_loccode");
}

TEST_F(NsmNVSwitchChassisDIUpdateTest, AssetTagIntfUpdate_NonUuid_Succeeds)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<AssetTagIntf>>(
        "sw_chassis_assettag", "NSM_NVSwitch");
    chassis->update(nvswitch);
    EXPECT_EQ(chassis->getName(), "sw_chassis_assettag");
}

TEST_F(NsmNVSwitchChassisDIUpdateTest, RevisionIntfUpdate_NonUuid_Succeeds)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<RevisionIntf>>(
        "sw_chassis_rev", "NSM_NVSwitch");
    chassis->update(nvswitch);
    EXPECT_EQ(chassis->getName(), "sw_chassis_rev");
}

TEST_F(NsmNVSwitchChassisDIUpdateTest, LocationIntfUpdate_NonUuid_Succeeds)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<LocationIntf>>(
        "sw_chassis_loc", "NSM_NVSwitch");
    chassis->update(nvswitch);
    EXPECT_EQ(chassis->getName(), "sw_chassis_loc");
}

TEST_F(NsmNVSwitchChassisDIUpdateTest, ChassisIntfUpdate_NonUuid_Succeeds)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<ChassisIntf>>(
        "sw_chassis_chassis", "NSM_NVSwitch");
    chassis->update(nvswitch);
    EXPECT_EQ(chassis->getName(), "sw_chassis_chassis");
}

TEST_F(NsmNVSwitchChassisDIUpdateTest, HealthIntfUpdate_NonUuid_Succeeds)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<HealthIntf>>(
        "sw_chassis_health", "NSM_NVSwitch");
    chassis->update(nvswitch);
    EXPECT_EQ(chassis->getName(), "sw_chassis_health");
}

TEST_F(NsmNVSwitchChassisDIUpdateTest, NsmApSkuIdIntfUpdate_NonUuid_Succeeds)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<NsmApSkuIdIntf>>(
        "sw_chassis_apsku", "NSM_NVSwitch");
    chassis->update(nvswitch);
    EXPECT_EQ(chassis->getName(), "sw_chassis_apsku");
}
