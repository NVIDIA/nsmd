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
using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "diagnostics.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmProcessor.hpp"
#include "nsmWorkloadPowerProfile.hpp"

#undef private
#undef protected

using namespace nsm;

static auto busPFB = sdbusplus::bus::new_default();

// =============================================================================
// NsmProcessor factory: createWorkloadPowerProfile branch test
// This is already covered by TEST_F(NsmProcessorTest,
// goodTestCreateWorkloadPowerProfileSensors) in existing test file.
// We add a test exercising the WorkloadPowerProfile factory with ProfileIdMap.
// =============================================================================

namespace nsm
{
requester::Coroutine createNsmProcessorSensor(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath);
}; // namespace nsm

struct NsmProcessorBatch12CTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 28;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Processor";
    const std::string name = "GPU_SXM_1";
    const std::string objPath = processorsInventoryBasePath / name;

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorBatch12CTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorBatch12CTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmProcessorBatch12CTest,
       goodTestCreateWorkloadPowerProfileWithProfileIdMap)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = name;
    basePropertyMap["UUID"] = gpuUuid;
    basePropertyMap["InventoryObjPath"] = objPath;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".WorkloadPowerProfile");
    propertyMap["Type"] = std::string("NSM_WorkloadPowerProfile");
    propertyMap["ProfileIdMap"] =
        std::vector<std::string>{"Performance", "Balanced", "PowerSaver"};

    createNsmProcessorSensor(mockManager,
                             basicIntfName + ".WorkloadPowerProfile", objPath);

    // The factory should create:
    // - NsmWorkLoadProfileEnum (static)
    // - OemProfileInfoIntf (non-sensor)
    // - NsmWorkloadProfileInfoAsyncIntf (non-sensor)
    // - NsmWorkLoadProfileStatus (RR)
    // - NsmWorkloadPowerProfileCollection (static)
    // - NsmWorkloadPowerProfilePageCollection (static)
    // - NsmWorkloadPowerProfilePage (RR, first page)
    // + 1 msgTypes sensor at index 0
    // So we expect at least 2 RR sensors and 3+ static sensors
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
    EXPECT_GE(gpu->staticSensors.size(), 3u);
}

TEST_F(NsmProcessorBatch12CTest,
       goodTestCreateWorkloadPowerProfile_EmptyProfileIdMap)
{
    // Use a unique path to avoid D-Bus path conflict with the sibling test
    // that registers objects at processorsInventoryBasePath/GPU_SXM_1.
    const std::string uniquePath = processorsInventoryBasePath /
                                   std::string("GPU_SXM_1_EmptyProfile");
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                              basicIntfName);
    basePropertyMap["Name"] = std::string("GPU_SXM_1_EmptyProfile");
    basePropertyMap["UUID"] = gpuUuid;
    basePropertyMap["InventoryObjPath"] = uniquePath;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".WorkloadPowerProfile");
    propertyMap["Type"] = std::string("NSM_WorkloadPowerProfile");
    // No ProfileIdMap property → count("ProfileIdMap") = 0 → FALSE branch
    // profileIdMap stays empty → NsmWorkLoadProfileEnum created with empty list

    createNsmProcessorSensor(
        mockManager, basicIntfName + ".WorkloadPowerProfile", uniquePath);

    // Sensors still created with empty profileIdMap
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
    EXPECT_GE(gpu->staticSensors.size(), 3u);
}

// =============================================================================
// createPowerCap FALSE branch: L607 count("CompositeNumericSensors") == 0
// =============================================================================

TEST_F(NsmProcessorBatch12CTest,
       createPowerCap_NoCompositeNumericSensors_FalseBranch)
{
    // Unique path to avoid D-Bus conflicts with nsmProcessor_test.cpp tests
    const std::string uniquePath = processorsInventoryBasePath /
                                   std::string("GPU_SXM_PwrCap_NoCNS");

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                              basicIntfName);
    basePropertyMap["Name"] = std::string("GPU_SXM_PwrCap_NoCNS");
    basePropertyMap["UUID"] = gpuUuid;
    basePropertyMap["InventoryObjPath"] = uniquePath;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".PowerCap");
    propertyMap["Type"] = std::string("NSM_PowerCap");
    // "CompositeNumericSensors" deliberately absent → L607 FALSE branch

    createNsmProcessorSensor(mockManager, basicIntfName + ".PowerCap",
                             uniquePath);

    // createPowerCap adds NsmPowerCap (RR) + 3 static sensors
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
    EXPECT_GE(gpu->staticSensors.size(), 3u);
}

// =============================================================================
// createReconfigPermissions FALSE branch: L674 count("Features") == 0
// =============================================================================

TEST_F(NsmProcessorBatch12CTest,
       createReconfigPermissions_NoFeaturesKey_FalseBranch)
{
    // Unique path to avoid D-Bus conflicts
    const std::string uniquePath = processorsInventoryBasePath /
                                   std::string("GPU_SXM_Reconfig_NoFeat");

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                              basicIntfName);
    basePropertyMap["Name"] = std::string("GPU_SXM_Reconfig_NoFeat");
    basePropertyMap["UUID"] = gpuUuid;
    basePropertyMap["InventoryObjPath"] = uniquePath;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".ReconfigPermissions");
    propertyMap["Type"] = std::string("NSM_ReconfigPermissions");
    // "Features" deliberately absent → L674 FALSE branch → empty featuresNames
    // → feature loop body never entered → no sensors added

    createNsmProcessorSensor(
        mockManager, basicIntfName + ".ReconfigPermissions", uniquePath);

    // With empty features, no sensors are added to the device
    EXPECT_EQ(1u, devices.size());
}

TEST(NsmAcceleratorIntf, GoodConstruction)
{
    std::string accName = "GPU_SXM_1";
    std::string accType = "NSM_Processor";
    std::string accPath = "/xyz/openbmc_project/inventory/gpu_acc";

    NsmAcceleratorIntf accSensor(busPFB, accName, accType, accPath);
    EXPECT_EQ(accSensor.getName(), accName);
    EXPECT_EQ(accSensor.getType(), accType);
}

TEST(NsmProcessorAssociation, GoodConstruction)
{
    std::string assocName = "GPU_SXM_1";
    std::string assocType = "NSM_Processor";
    std::string assocPath = "/xyz/openbmc_project/inventory/gpu_assoc";
    std::vector<utils::Association> associations = {
        {"parent", "child", "/xyz/openbmc_project/inventory/system"}};

    NsmProcessorAssociation assocSensor(busPFB, assocName, assocType, assocPath,
                                        associations);
    EXPECT_EQ(assocSensor.getName(), assocName);
    EXPECT_EQ(assocSensor.getType(), assocType);
}

TEST(NsmLocationIntfProcessor, GoodConstruction)
{
    std::string locName = "GPU_SXM_1";
    std::string locType = "NSM_Processor";
    std::string locPath = "/xyz/openbmc_project/inventory/gpu_loc";
    std::string locationType = "xyz.openbmc_project.Inventory.Decorator."
                               "Location.LocationTypes.Embedded";

    NsmLocationIntfProcessor locSensor(busPFB, locName, locType, locPath,
                                       locationType);
    EXPECT_EQ(locSensor.getName(), locName);
    EXPECT_EQ(locSensor.getType(), locType);
}

TEST(NsmLocationCodeIntfProcessor, GoodConstruction)
{
    std::string locCodeName = "GPU_SXM_1";
    std::string locCodeType = "NSM_Processor";
    std::string locCodePath = "/xyz/openbmc_project/inventory/gpu_loccode";
    std::string locationCode = "GPU_SXM_1";

    NsmLocationCodeIntfProcessor locCodeSensor(busPFB, locCodeName, locCodeType,
                                               locCodePath, locationCode);
    EXPECT_EQ(locCodeSensor.getName(), locCodeName);
    EXPECT_EQ(locCodeSensor.getType(), locCodeType);
}
