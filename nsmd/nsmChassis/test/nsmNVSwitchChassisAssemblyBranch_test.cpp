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

#include "nsmChassis/nsmNVSwitchAndNVMgmtNICChassisAssembly.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// SECTION 4: nsmNVSwitchAndNVMgmtNICChassisAssembly.cpp
// ============================================================================

struct NsmChassisAssemblyBatch11F :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "NVSwitch_Batch11F";
    const std::string name = "NVSwitch0_Asm_B11F";
    const std::string baseType = "NSM_NVSwitch_ChassisAssembly";
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:55";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmChassisAssemblyBatch11F() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmChassisAssemblyBatch11F()
    {
        cleanupDeviceSensors(devices);
    }
};

// -- NsmNVSwitchAndNicChassisAssembly<NsmAssetIntf>: asset object construction
TEST_F(NsmChassisAssemblyBatch11F,
       AssetAssembly_Constructor_CreatesValidAssetObject)
{
    // Arrange & Act - construct asset assembly object directly via public API
    auto assetObj =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<NsmAssetIntf>>(
            chassisName, name, baseType);

    // Assert
    EXPECT_NE(assetObj, nullptr);
    EXPECT_EQ(assetObj->getName(), name);
    EXPECT_EQ(assetObj->getType(), baseType);
}

// -- NsmNVSwitchAndNicChassisAssembly<HealthIntf>: health object construction
TEST_F(NsmChassisAssemblyBatch11F,
       HealthAssembly_Constructor_CreatesValidHealthObject)
{
    // Arrange & Act
    auto healthObj =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<HealthIntf>>(
            chassisName, name, baseType);

    // Assert
    EXPECT_NE(healthObj, nullptr);
    EXPECT_EQ(healthObj->getName(), name);
    EXPECT_EQ(healthObj->getType(), baseType);

    // Verify the object can be added as a static sensor
    size_t initialStatic = gpu->staticSensors.size();
    gpu->addStaticSensor(healthObj);
    EXPECT_EQ(gpu->staticSensors.size(), initialStatic + 1);
}

// -- NsmNVSwitchAndNicChassisAssembly<AreaIntf>: physical context construction
TEST_F(NsmChassisAssemblyBatch11F,
       AreaAssembly_Constructor_CreatesValidAreaObject)
{
    // Arrange & Act
    auto areaObj = std::make_shared<NsmNVSwitchAndNicChassisAssembly<AreaIntf>>(
        chassisName, name, baseType);

    // Assert
    EXPECT_NE(areaObj, nullptr);
    EXPECT_EQ(areaObj->getName(), name);
    EXPECT_EQ(areaObj->getType(), baseType);

    // Verify the object can be added as a static sensor
    size_t initialStatic = gpu->staticSensors.size();
    gpu->addStaticSensor(areaObj);
    EXPECT_EQ(gpu->staticSensors.size(), initialStatic + 1);
}

// -- NsmNVSwitchAndNicChassisAssembly<LocationIntf>: location type
TEST_F(NsmChassisAssemblyBatch11F,
       LocationAssembly_Constructor_CreatesValidLocationObject)
{
    // Arrange & Act
    auto locObj =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<LocationIntf>>(
            chassisName, name, baseType);

    // Assert
    EXPECT_NE(locObj, nullptr);
    EXPECT_EQ(locObj->getName(), name);
    EXPECT_EQ(locObj->getType(), baseType);

    // Verify the object can be added as a static sensor
    size_t initialStatic = gpu->staticSensors.size();
    gpu->addStaticSensor(locObj);
    EXPECT_EQ(gpu->staticSensors.size(), initialStatic + 1);
}

// -- NsmNVSwitchAndNicChassisAssembly with RevisionIntf
TEST_F(NsmChassisAssemblyBatch11F, Constructor_RevisionIntf_CreatesValidObject)
{
    // Arrange & Act
    auto revisionObj =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<RevisionIntf>>(
            chassisName, name, baseType);

    // Assert
    EXPECT_NE(revisionObj, nullptr);
    EXPECT_EQ(revisionObj->getName(), name);
    EXPECT_EQ(revisionObj->getType(), baseType);
}

// ============================================================================
// Factory function FALSE-branch tests for createNsmChassisAssembly
// (covers count("Name"), count("Type"), count("ChassisName"), count("UUID"),
//  count("PhysicalContext"), count("LocationType") FALSE branches)
// ============================================================================

namespace nsm
{
requester::Coroutine createNsmNVSwitchChassisAssembly(SensorManager& manager,
                                                      const std::string& intf,
                                                      const std::string& path);
requester::Coroutine createNsmNVLinkMgmtNicChassisAssembly(
    SensorManager& manager, const std::string& intf, const std::string& path);
} // namespace nsm

struct NsmChassisAssemblyFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string baseIfaceName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/fabrics/NVSwitch0_asm";
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:77";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmChassisAssemblyFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmChassisAssemblyFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// count("Name") FALSE, count("ChassisName") FALSE: both absent from base map
// → name="", chassisName="" → type==baseType → sensor ctor throws (empty D-Bus
// path)
// Only runs in coverage build: in real-coroutine mode the throw occurs inside
// the nested createNsmChassisAssembly coroutine and is
// stored in its promise; the outer coroutine's await_resume() is noexcept →
// outer promise is empty.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmChassisAssemblyFactoryTest, Factory_BaseInterface_NoNameNoChassisName)
{
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIfaceName);
    // Omit "Name" and "ChassisName" → their count() returns 0 (FALSE branches)
    baseMap["UUID"] = deviceUuid;
    baseMap["Type"] = std::string("NSM_NVSwitch_ChassisAssembly");

    // Empty name/chassisName → sd_bus rejects empty D-Bus path component →
    // throws
    EXPECT_THROW_COROUTINE(nsm::createNsmNVSwitchChassisAssembly(
                               mockManager, baseIfaceName, objPath),
                           std::exception);
}
#endif // COVERAGE_DISABLE_COROUTINES

// count("Type") FALSE: Type absent → type="" → no branch matches →
// no sensors added (device accessed but not used)
TEST_F(NsmChassisAssemblyFactoryTest,
       Factory_BaseInterface_MissingType_NoSensors)
{
    const std::string uniquePath = objPath + "_notype";
    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      baseIfaceName);
    // "Type" intentionally absent → type="" → neither type branch matches
    baseMap["UUID"] = deviceUuid;
    baseMap["Name"] = std::string("NVSwitch0_NoType");
    baseMap["ChassisName"] = std::string("NVSwitch_Chassis_NoType");

    const size_t before = gpu->staticSensors.size() + gpu->deviceSensors.size();
    nsm::createNsmNVSwitchChassisAssembly(mockManager, baseIfaceName,
                                          uniquePath);
    // No sensors added when type doesn't match any branch
    EXPECT_EQ(gpu->staticSensors.size() + gpu->deviceSensors.size(), before);
}

// count("UUID") FALSE: UUID absent → uuid="" →
// getNsmDeviceFromStaticUUID("") throws std::runtime_error
// Only runs in coverage build: same reason as
// Factory_BaseInterface_NoNameNoChassisName.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmChassisAssemblyFactoryTest, Factory_BaseInterface_MissingUUID_Throws)
{
    const std::string uniquePath = objPath + "_nouuid";
    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      baseIfaceName);
    // "UUID" intentionally absent → uuid="" → parseStaticUuid fails → throws
    baseMap["Name"] = std::string("NVSwitch0_NoUUID");
    baseMap["ChassisName"] = std::string("NVSwitch_Chassis_NoUUID");
    baseMap["Type"] = std::string("NSM_NVSwitch_ChassisAssembly");

    EXPECT_THROW_COROUTINE(nsm::createNsmNVSwitchChassisAssembly(
                               mockManager, baseIfaceName, uniquePath),
                           std::runtime_error);
}
#endif // COVERAGE_DISABLE_COROUTINES

// count("PhysicalContext") FALSE, count("LocationType") FALSE:
// both absent from ChassisAttributes sub-interface → no area/location sensor
TEST_F(NsmChassisAssemblyFactoryTest,
       Factory_ChassisAttributes_NoPhysicalContextNoLocationType)
{
    const std::string subIface = baseIfaceName + ".ChassisAttributes";
    const std::string uniquePath = objPath + "_attrs_minimal";

    // Register base interface map
    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      baseIfaceName);
    baseMap["UUID"] = deviceUuid;
    baseMap["Name"] = std::string("NVSwitch0_MinAttrs");
    baseMap["ChassisName"] = std::string("NVSwitch_Chassis_MinAttrs");

    // Register ChassisAttributes sub-interface map: only Type and Name present
    // (PhysicalContext and LocationType intentionally absent → FALSE branches)
    auto& attrMap = utils::MockDbusAsync::propertyMap(uniquePath, subIface);
    attrMap["Type"] = std::string("NSM_Chassis_Attributes");
    attrMap["Name"] = std::string("NVSwitch0_MinAttrs");

    const size_t before = gpu->staticSensors.size();
    nsm::createNsmNVSwitchChassisAssembly(mockManager, subIface, uniquePath);
    // Health + asset sensors created; no physical context or location type
    EXPECT_GT(gpu->staticSensors.size(), before);
}

// Same FALSE branches via NVLinkMgmtNic variant (exercises the sibling factory)
// count("Name") FALSE → name="" → sd_bus rejects empty D-Bus path → throws
// Only runs in coverage build: same reason as
// Factory_BaseInterface_NoNameNoChassisName.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmChassisAssemblyFactoryTest,
       Factory_NVLinkMgmtNic_BaseInterface_NoName)
{
    const std::string nicBaseIface =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly";
    const std::string uniquePath = objPath + "_nic_noname";

    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath, nicBaseIface);
    // "Name" and "ChassisName" absent → FALSE branches; empty name → throws
    baseMap["UUID"] = deviceUuid;
    baseMap["Type"] = std::string("NSM_NVLinkMgmtNic_ChassisAssembly");

    EXPECT_THROW_COROUTINE(nsm::createNsmNVLinkMgmtNicChassisAssembly(
                               mockManager, nicBaseIface, uniquePath),
                           std::exception);
}
#endif // COVERAGE_DISABLE_COROUTINES
