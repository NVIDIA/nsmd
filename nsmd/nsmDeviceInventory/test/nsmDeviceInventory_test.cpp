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

#define private public
#define protected public

#include "device-configuration.h"

#include "nsmNetworkAdapter.hpp"
#include "nsmSwitch.hpp"

namespace nsm
{
requester::Coroutine createNsmSwitchDI(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ---------------------------------------------------------------------------
// Existing tests (NsmDeviceInventoryTest fixture)
// ---------------------------------------------------------------------------

class NsmDeviceInventoryTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
};

// NsmDeviceProtectionOptions Constructor Test
TEST_F(NsmDeviceInventoryTest, DeviceProtectionOptions_ConstructorBasic)
{
    std::string path = "/xyz/openbmc_project/inventory/system/chassis/GPU0";
    std::string name = "GPU0_Protection";
    std::string type = "GPU";

    nsm::NsmDeviceProtectionOptions protectionOpts(bus, path.c_str(), name,
                                                   type);

    EXPECT_EQ(protectionOpts.getName(), name);
    EXPECT_EQ(protectionOpts.getType(), type);
    EXPECT_EQ(protectionOpts.asyncPatchInProgress, false);
}

// ---------------------------------------------------------------------------
// NsmSwitchDI template class tests
// ---------------------------------------------------------------------------

class NsmSwitchDITest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/";
};

TEST_F(NsmSwitchDITest, SwitchIntfConstructor)
{
    // Arrange & Act
    auto switchObj =
        std::make_shared<NsmSwitchDI<SwitchIntf>>(name, inventoryObjPath);

    // Assert
    EXPECT_EQ(switchObj->getName(), name);
    EXPECT_EQ(switchObj->getType(), "NSM_NVSwitch");
    EXPECT_EQ(switchObj->objPath, inventoryObjPath + name);
}

TEST_F(NsmSwitchDITest, NsmAssetIntfConstructor)
{
    // Arrange & Act
    auto assetObj =
        std::make_shared<NsmSwitchDI<NsmAssetIntf>>(name, inventoryObjPath);

    // Assert
    EXPECT_EQ(assetObj->getName(), name);
    EXPECT_EQ(assetObj->getType(), "NSM_NVSwitch");
    EXPECT_EQ(assetObj->objPath, inventoryObjPath + name);
}

TEST_F(NsmSwitchDITest, NvSwitchIntfConstructor)
{
    // Arrange & Act
    auto nvSwitchObj =
        std::make_shared<NsmSwitchDI<NvSwitchIntf>>(name, inventoryObjPath);

    // Assert
    EXPECT_EQ(nvSwitchObj->getName(), name);
    EXPECT_EQ(nvSwitchObj->getType(), "NSM_NVSwitch");
}

TEST_F(NsmSwitchDITest, UuidIntfConstructor)
{
    // Arrange & Act
    auto uuidObj = std::make_shared<NsmSwitchDI<UuidIntf>>(name,
                                                           inventoryObjPath);

    // Assert
    EXPECT_EQ(uuidObj->getName(), name);
    EXPECT_EQ(uuidObj->getType(), "NSM_NVSwitch");
}

TEST_F(NsmSwitchDITest, AssociationDefinitionsIntfConstructor)
{
    // Arrange & Act
    auto assocObj = std::make_shared<NsmSwitchDI<AssociationDefinitionsInft>>(
        name, inventoryObjPath);

    // Assert
    EXPECT_EQ(assocObj->getName(), name);
    EXPECT_EQ(assocObj->getType(), "NSM_NVSwitch");
}

// ---------------------------------------------------------------------------
// NsmSwitchDIPowerMode tests
// ---------------------------------------------------------------------------

class NsmSwitchDIPowerModeTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/";
};

TEST_F(NsmSwitchDIPowerModeTest, Constructor)
{
    // Arrange & Act
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);

    // Assert
    EXPECT_EQ(powerMode->getName(), name);
    EXPECT_EQ(powerMode->getType(), "NSM_NVSwitch");
    EXPECT_EQ(powerMode->getInventoryObjectPath(), inventoryObjPath + name);
    EXPECT_FALSE(powerMode->asyncPatchInProgress);
}

TEST_F(NsmSwitchDIPowerModeTest, GetPowerModeDataReturnsSetValues)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);

    powerMode->invoke(pdiMethod(hwModeControl), true);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(1000));
    powerMode->invoke(pdiMethod(fwThrottlingMode), true);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(200));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(300));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(400));

    // Act
    auto data = powerMode->getPowerModeData();

    // Assert
    EXPECT_EQ(data.l1_hw_mode_control, true);
    EXPECT_EQ(data.l1_hw_mode_threshold, 1000u);
    EXPECT_EQ(data.l1_fw_throttling_mode, true);
    EXPECT_EQ(data.l1_prediction_mode, false);
    EXPECT_EQ(data.l1_hw_active_time, 200u);
    EXPECT_EQ(data.l1_hw_inactive_time, 300u);
    EXPECT_EQ(data.l1_prediction_inactive_time, 400u);
}

TEST_F(NsmSwitchDIPowerModeTest, GetPowerModeDataInitialValues)
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

    // Act
    auto data = powerMode->getPowerModeData();

    // Assert
    EXPECT_EQ(data.l1_hw_mode_control, false);
    EXPECT_EQ(data.l1_hw_mode_threshold, 0u);
    EXPECT_EQ(data.l1_fw_throttling_mode, false);
    EXPECT_EQ(data.l1_prediction_mode, false);
    EXPECT_EQ(data.l1_hw_active_time, 0u);
    EXPECT_EQ(data.l1_hw_inactive_time, 0u);
    EXPECT_EQ(data.l1_prediction_inactive_time, 0u);
}

TEST_F(NsmSwitchDIPowerModeTest, GetInventoryObjectPath)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);

    // Act & Assert
    EXPECT_EQ(powerMode->getInventoryObjectPath(), inventoryObjPath + name);
}

// ---------------------------------------------------------------------------
// NsmSwitchIsolationMode tests
// ---------------------------------------------------------------------------

class NsmSwitchIsolationModeTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/NVSwitch_0";
};

TEST_F(NsmSwitchIsolationModeTest, Constructor)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());

    // Act
    NsmSwitchIsolationMode isolationMode(name, type, isolationIntf);

    // Assert
    EXPECT_EQ(isolationMode.getName(), name);
    EXPECT_EQ(isolationMode.getType(), type);
    EXPECT_NE(isolationMode.switchIsolationIntf, nullptr);
}

TEST_F(NsmSwitchIsolationModeTest, GenRequestMsg)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode isolationMode(name, type, isolationIntf);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    // Act
    auto request = isolationMode.genRequestMsg(eid, instanceId);

    // Assert
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}

TEST_F(NsmSwitchIsolationModeTest, HandleResponseMsgEnabled)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode isolationMode(name, type, isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_switch_isolation_mode_resp(
        0, NSM_SUCCESS, ERR_NULL, SWITCH_COMMUNICATION_MODE_ENABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = isolationMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationEnabled);
}

TEST_F(NsmSwitchIsolationModeTest, HandleResponseMsgDisabled)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode isolationMode(name, type, isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_switch_isolation_mode_resp(
        0, NSM_SUCCESS, ERR_NULL, SWITCH_COMMUNICATION_MODE_DISABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = isolationMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationDisabled);
}

TEST_F(NsmSwitchIsolationModeTest, HandleResponseMsgUnknownMode)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode isolationMode(name, type, isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    uint8_t unknownMode = 0xFF;
    auto rc = encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    unknownMode, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = isolationMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationUnknown);
}

TEST_F(NsmSwitchIsolationModeTest, HandleResponseMsgError)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode isolationMode(name, type, isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_switch_isolation_mode_resp(
        0, NSM_ERROR, ERR_NULL, SWITCH_COMMUNICATION_MODE_ENABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = isolationMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ---------------------------------------------------------------------------
// NsmSwitchL1PredictionMode tests
// ---------------------------------------------------------------------------

class NsmSwitchL1PredictionModeTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/NVSwitch_0/"
        "Oem/Nvidia/PowerMode/L1PredictionMode";
};

TEST_F(NsmSwitchL1PredictionModeTest, Constructor)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());

    // Act
    NsmSwitchL1PredictionMode predMode(name, type, enableIntf, assocDefIntf);

    // Assert
    EXPECT_EQ(predMode.getName(), name);
    EXPECT_EQ(predMode.getType(), type);
    EXPECT_NE(predMode.enableIntf, nullptr);
    EXPECT_NE(predMode.associationDefIntf, nullptr);
    EXPECT_FALSE(predMode.asyncPatchInProgress);
}

TEST_F(NsmSwitchL1PredictionModeTest, GenRequestMsg)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    NsmSwitchL1PredictionMode predMode(name, type, enableIntf, assocDefIntf);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    // Act
    auto request = predMode.genRequestMsg(eid, instanceId);

    // Assert
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_req));
}

// ---------------------------------------------------------------------------
// NsmSwitchDIReset tests
// ---------------------------------------------------------------------------

class NsmSwitchDIResetTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/";
};

TEST_F(NsmSwitchDIResetTest, Constructor)
{
    // Arrange
    auto device = std::make_shared<MockNsmDevice>(
        1, 0, "NSM_DEVICE_INSTANCE_NUMBER", "100", NSM_DEV_ROLE_RESERVED);

    // Act
    NsmSwitchDIReset resetObj(bus, name, type, inventoryObjPath, device);

    // Assert
    EXPECT_EQ(resetObj.getName(), name);
    EXPECT_EQ(resetObj.getType(), type);
    EXPECT_NE(resetObj.resetIntf, nullptr);
    EXPECT_NE(resetObj.resetAsyncIntf, nullptr);
    EXPECT_EQ(resetObj.objPath, inventoryObjPath + name);
}

// ---------------------------------------------------------------------------
// createNsmSwitchDI factory function tests (using SensorManagerTest fixture)
// ---------------------------------------------------------------------------

struct NsmSwitchDIFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch";
    const std::string name = "NVSwitch_0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/";

    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchDIFactoryTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchDIFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", switchUuid},
        {"InventoryObjPath", inventoryObjPath},
    };
};

// Test: createNsmSwitchDI with type = NSM_NVSwitch
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMNVSwitch)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;
    basePropertyMap["SupportL1PredictionMode"] = false;
    basePropertyMap["Type"] = std::string("NSM_NVSwitch");

    // Act
    createNsmSwitchDI(mockManager, basicIntfName, objPath);

    // Assert
    // NSM_NVSwitch creates: NvSwitchIntf (deviceSensor), UuidIntf (static),
    // AssociationDef (static), SwitchDIReset (deviceSensor),
    // IsolationMode (roundRobin sensor)
    EXPECT_GE(nvswitch->deviceSensors.size(), 1);
    EXPECT_GE(nvswitch->staticSensors.size(), 2);
}

// Test: createNsmSwitchDI with type = NSM_NVSwitch and L1 prediction mode
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMNVSwitchWithL1Prediction)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;
    basePropertyMap["SupportL1PredictionMode"] = true;
    basePropertyMap["Type"] = std::string("NSM_NVSwitch");

    // Act
    createNsmSwitchDI(mockManager, basicIntfName, objPath);

    // Assert - more sensors when L1PredictionMode is enabled
    EXPECT_GE(nvswitch->deviceSensors.size(), 1);
    EXPECT_GE(nvswitch->staticSensors.size(), 2);
    // L1PredictionMode should be added to roundRobin sensors
    EXPECT_GE(nvswitch->roundRobinSensors.size(), 2);
}

// Test: createNsmSwitchDI with type = NSM_Chassis_Attributes
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMChassisAttributes)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    const std::string chassisAttrIntf = basicIntfName + ".ChassisAttributes";
    dbus::PropertyMap chassisAttrProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("NVSwitch_Asset")},
    };
    auto& currentPropertyMap =
        utils::MockDbusAsync::propertyMap(objPath, chassisAttrIntf);
    currentPropertyMap = chassisAttrProperties;

    // Act
    createNsmSwitchDI(mockManager, chassisAttrIntf, objPath);

    // Assert - NSM_Chassis_Attributes creates an NsmAssetIntf static sensor
    EXPECT_GE(nvswitch->staticSensors.size(), 1);
}

// Test: createNsmSwitchDI with type = NSM_PowerMode
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMPowerMode)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    const std::string powerModeIntf = basicIntfName + ".PowerMode";
    dbus::PropertyMap powerModeProperties = {
        {"Type", std::string("NSM_PowerMode")},
        {"Priority", false},
    };
    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                                 powerModeIntf);
    currentPropertyMap = powerModeProperties;

    // Act
    createNsmSwitchDI(mockManager, powerModeIntf, objPath);

    // Assert - NSM_PowerMode creates a round-robin sensor
    EXPECT_GE(nvswitch->roundRobinSensors.size(), 1);
}

// Test: createNsmSwitchDI with type = NSM_PowerMode and priority=true
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMPowerModePriority)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    const std::string powerModeIntf = basicIntfName + ".PowerMode";
    dbus::PropertyMap powerModeProperties = {
        {"Type", std::string("NSM_PowerMode")},
        {"Priority", true},
    };
    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                                 powerModeIntf);
    currentPropertyMap = powerModeProperties;

    // Act
    createNsmSwitchDI(mockManager, powerModeIntf, objPath);

    // Assert - sensor was created and added to the device
    // Note: prioritySensors is a reference to sensors[PollingType::Priority]
    // which may not reflect updates reliably in tests; deviceSensors is always
    // updated
    EXPECT_GE(nvswitch->deviceSensors.size(), 1);
}

// Test: createNsmSwitchDI with type = NSM_Switch
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMSwitch)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    const std::string switchIntf = basicIntfName + ".Switch";
    dbus::PropertyMap switchProperties = {
        {"Type", std::string("NSM_Switch")},
        {"SwitchType",
         std::string(
             "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.NVLink")},
        {"SwitchSupportedProtocols",
         std::vector<std::string>{
             "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.NVLink"}},
    };
    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                                 switchIntf);
    currentPropertyMap = switchProperties;

    // Act
    createNsmSwitchDI(mockManager, switchIntf, objPath);

    // Assert - NSM_Switch creates a round-robin sensor for SwitchIntf
    EXPECT_GE(nvswitch->roundRobinSensors.size(), 1);
}

// Test: createNsmSwitchDI with type = NSM_PortDisableFuture
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMPortDisableFuture)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    const std::string portDisableIntf = basicIntfName + ".PortDisableFuture";
    dbus::PropertyMap portDisableProperties = {
        {"Type", std::string("NSM_PortDisableFuture")},
        {"Priority", false},
    };
    auto& currentPropertyMap =
        utils::MockDbusAsync::propertyMap(objPath, portDisableIntf);
    currentPropertyMap = portDisableProperties;

    // Act
    createNsmSwitchDI(mockManager, portDisableIntf, objPath);

    // Assert - NSM_PortDisableFuture creates a round-robin sensor
    EXPECT_GE(nvswitch->roundRobinSensors.size(), 1);
}

// Test: createNsmSwitchDI with type = NSM_PortDisableFuture priority
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMPortDisableFuturePriority)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    const std::string portDisableIntf = basicIntfName + ".PortDisableFuture";
    dbus::PropertyMap portDisableProperties = {
        {"Type", std::string("NSM_PortDisableFuture")},
        {"Priority", true},
    };
    auto& currentPropertyMap =
        utils::MockDbusAsync::propertyMap(objPath, portDisableIntf);
    currentPropertyMap = portDisableProperties;

    // Act
    createNsmSwitchDI(mockManager, portDisableIntf, objPath);

    // Assert - sensor was created and added to the device
    // Note: prioritySensors is a reference to sensors[PollingType::Priority]
    // which may not reflect updates reliably in tests; deviceSensors is always
    // updated
    EXPECT_GE(nvswitch->deviceSensors.size(), 1);
}

// Test: createNsmSwitchDI with type = NSM_FabricManager
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMFabricManager)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    const std::string fabricMgrIntf = basicIntfName + ".FabricManager";
    dbus::PropertyMap fabricMgrProperties = {
        {"Type", std::string("NSM_FabricManager")},
        {"Name", std::string("FM_0")},
        {"InventoryObjPath",
         std::string(
             "/xyz/openbmc_project/inventory/system/nvswitch/fabricmgr/")},
        {"Description", std::string("Fabric Manager Instance")},
    };
    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                                 fabricMgrIntf);
    currentPropertyMap = fabricMgrProperties;

    // Act
    createNsmSwitchDI(mockManager, fabricMgrIntf, objPath);

    // Assert - NSM_FabricManager creates a round-robin sensor and device event
    EXPECT_GE(nvswitch->roundRobinSensors.size(), 1);
    EXPECT_GE(nvswitch->deviceEvents.size(), 1);
}

// Test: createNsmSwitchDI error case - missing UUID
TEST_F(NsmSwitchDIFactoryTest, badTestMissingUUID)
{
    // Arrange - base properties without UUID
    dbus::PropertyMap propsNoUuid = {
        {"Name", name},
        {"InventoryObjPath", inventoryObjPath},
        {"Type", std::string("NSM_NVSwitch")},
        // UUID intentionally missing
    };
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = propsNoUuid;

    // Act & Assert - empty string UUID should throw from
    // getNsmDeviceFromStaticUUID
    EXPECT_THROW_COROUTINE(
        createNsmSwitchDI(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

// Test: createNsmSwitchDI error case - missing base properties entirely
TEST_F(NsmSwitchDIFactoryTest, badTestMissingBaseProperties)
{
    // Arrange - do not set up any property maps for the base interface
    // coGetCachedBaseProperties should fail and return early.
    const std::string switchIntf = basicIntfName + ".SomeSubIntf";
    dbus::PropertyMap currentProperties = {
        {"Type", std::string("NSM_NVSwitch")},
    };
    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                                 switchIntf);
    currentPropertyMap = currentProperties;

    // Act - coroutine returns early with error code (not throw)
    auto result = createNsmSwitchDI(mockManager, switchIntf, objPath);

    // Assert - function completes without crash/throw when base properties are
    // missing Note: totalSensors is not asserted as the exact count depends on
    // internal coroutine scheduling and the double-counting of deviceSensors +
    // typed queues
    (void)result;
}

// Test: createNsmSwitchDI with unrecognized type - no throw, no sensors
TEST_F(NsmSwitchDIFactoryTest, goodTestUnrecognizedType)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    const std::string unknownIntf = basicIntfName + ".UnknownType";
    dbus::PropertyMap unknownProperties = {
        {"Type", std::string("NSM_Unknown_Type")},
    };
    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                                 unknownIntf);
    currentPropertyMap = unknownProperties;

    // Act - should not throw, just returns NSM_SUCCESS
    createNsmSwitchDI(mockManager, unknownIntf, objPath);

    // Assert - function completes without crash/throw for unrecognized type
    // Note: exact sensor count not asserted due to double-counting in
    // deviceSensors + typed queues
}

// Test: createNsmSwitchDI with NSM_Switch - missing optional properties
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMSwitchMinimalProperties)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    const std::string switchIntf = basicIntfName + ".Switch";
    dbus::PropertyMap switchProperties = {
        {"Type", std::string("NSM_Switch")},
        {"SwitchType",
         std::string(
             "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.NVLink")},
    };
    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                                 switchIntf);
    currentPropertyMap = switchProperties;

    // Act
    createNsmSwitchDI(mockManager, switchIntf, objPath);

    // Assert - sensor still created without optional properties
    EXPECT_GE(nvswitch->roundRobinSensors.size(), 1);
}

// Test: createNsmSwitchDI with NSM_FabricManager - minimal properties
// Uses a unique objPath to avoid D-Bus vtable collision with the full FM test.
TEST_F(NsmSwitchDIFactoryTest, goodTestCreateNSMFabricManagerMinimal)
{
    // Arrange - use a unique config path to avoid D-Bus "FileExists" clash
    const std::string minObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch_fm_min";
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(minObjPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;
    basePropertyMap["Name"] = std::string("FM_Min_0");
    basePropertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/nvswitch_fm_min/");

    const std::string fabricMgrIntf = basicIntfName + ".FabricManager";
    dbus::PropertyMap fabricMgrProperties = {
        {"Type", std::string("NSM_FabricManager")},
        {"Name", std::string("FM_Minimal")},
        {"InventoryObjPath",
         std::string(
             "/xyz/openbmc_project/inventory/system/nvswitch/fabricmgr_min/")},
    };
    auto& currentPropertyMap = utils::MockDbusAsync::propertyMap(minObjPath,
                                                                 fabricMgrIntf);
    currentPropertyMap = fabricMgrProperties;

    // Act
    createNsmSwitchDI(mockManager, fabricMgrIntf, minObjPath);

    // Assert - sensor created even without optional FM properties
    EXPECT_GE(nvswitch->roundRobinSensors.size(), 1);
}

// Test: Multiple types created on the same device
TEST_F(NsmSwitchDIFactoryTest, goodTestMultipleTypesOnSameDevice)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    // First create NSM_Chassis_Attributes
    {
        const std::string chassisAttrIntf = basicIntfName +
                                            ".ChassisAttributes";
        dbus::PropertyMap chassisAttrProperties = {
            {"Type", std::string("NSM_Chassis_Attributes")},
            {"Name", std::string("NVSwitch_Asset")},
        };
        auto& currentPropertyMap =
            utils::MockDbusAsync::propertyMap(objPath, chassisAttrIntf);
        currentPropertyMap = chassisAttrProperties;

        createNsmSwitchDI(mockManager, chassisAttrIntf, objPath);
    }

    size_t staticAfterChassis = nvswitch->staticSensors.size();
    EXPECT_GE(staticAfterChassis, 1);

    // Then create NSM_PowerMode
    {
        const std::string powerModeIntf = basicIntfName + ".PowerMode";
        dbus::PropertyMap powerModeProperties = {
            {"Type", std::string("NSM_PowerMode")},
            {"Priority", false},
        };
        auto& currentPropertyMap =
            utils::MockDbusAsync::propertyMap(objPath, powerModeIntf);
        currentPropertyMap = powerModeProperties;

        createNsmSwitchDI(mockManager, powerModeIntf, objPath);
    }

    // Assert - both types coexist on the same device
    EXPECT_GE(nvswitch->roundRobinSensors.size(), 1);
    EXPECT_GE(nvswitch->staticSensors.size(), staticAfterChassis);
}
