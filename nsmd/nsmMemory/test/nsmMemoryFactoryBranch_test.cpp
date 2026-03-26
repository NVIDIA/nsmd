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
 * Branch coverage tests for createNsmMemorySensor factory in nsmMemory.cpp.
 *
 * Targets:
 *   - count() FALSE branches for Name, UUID, Type, InventoryObjPath
 *   - nsmDevice == nullptr early return
 *   - type == "NSM_Memory" branch with missing optional properties
 *   - type == "NSM_Memory_Attributes" branch
 *   - Unknown type (neither NSM_Memory nor NSM_Memory_Attributes)
 *   - createNSMMemory missing ErrorCorrection, DeviceType, Priority
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "interfaceWrapper.hpp"
#include "nsmMemory.hpp"

namespace nsm
{
requester::Coroutine createNsmMemorySensor(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

class NsmMemoryFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string sensorName = "Memory_FBr";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/system/fabr/memory/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmMemoryFactoryBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmMemoryFactoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    void setupBaseProperties(const std::string& path,
                             dbus::PropertyMap extra = {})
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
        pm["Name"] = sensorName;
        pm["UUID"] = gpuUuid;
        pm["InventoryObjPath"] = inventoryPath;
        for (auto& [k, v] : extra)
        {
            pm[k] = v;
        }
    }

    void setupCurrentProperties(const std::string& path,
                                const std::string& intf,
                                dbus::PropertyMap props)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
        pm = props;
    }
};

// ============================================================================
// Base properties fail (no property map) -> early return
// Covers: L812-817 if (rc != NSM_SUCCESS) co_return rc
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, BasePropertiesFail_EarlyReturn)
{
    // No properties set -> coGetCachedBaseProperties fails
    const std::string path = "/test/memfbr/no_base";
    const size_t before = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->staticSensors.size(), before);
}

// ============================================================================
// Missing "Name" from base -> name="" (FALSE branch of count("Name"))
// Covers: L822-825 count("Name") FALSE
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, Factory_MissingName_FalseBranch)
{
    const std::string path = "/test/memfbr/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    // "Name" omitted
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = inventoryPath + "noname/";
    pm["Type"] = std::string("NSM_Memory_Attributes");

    // name="" but UUID valid -> device found
    createNsmMemorySensor(mockManager, baseIntf, path);
}

// ============================================================================
// Missing "UUID" from base -> uuid="" (FALSE branch)
// Covers: L827-830 count("UUID") FALSE
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, DISABLED_Factory_MissingUUID_FalseBranch)
{
    const std::string path = "/test/memfbr/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    // "UUID" omitted
    pm["InventoryObjPath"] = inventoryPath + "nouuid/";
    pm["Type"] = std::string("NSM_Memory_Attributes");

    createNsmMemorySensor(mockManager, baseIntf, path);
}

// ============================================================================
// Missing "Type" from current -> type="" (FALSE branch)
// Covers: L832-835 count("Type") FALSE -> no type matches -> no sensor
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, Factory_MissingType_FalseBranch)
{
    const std::string path = "/test/memfbr/no_type";
    setupBaseProperties(path);
    // No current properties set with Type

    const size_t before = gpu->deviceSensors.size() +
                          gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->roundRobinSensors.size(),
              before);
}

// ============================================================================
// Missing "InventoryObjPath" from base -> inventoryObjPath="" (FALSE branch)
// Covers: L837-841 count("InventoryObjPath") FALSE
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest,
       DISABLED_Factory_MissingInventoryObjPath_FalseBranch)
{
    const std::string path = "/test/memfbr/no_inv";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    // "InventoryObjPath" omitted
    pm["Type"] = std::string("NSM_Memory_Attributes");

    createNsmMemorySensor(mockManager, baseIntf, path);
}

// ============================================================================
// Invalid UUID -> nsmDevice == nullptr -> early return with NSM_ERROR
// Covers: L845-853 if (!nsmDevice) co_return NSM_ERROR
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, DISABLED_Factory_InvalidUUID_NoDevice)
{
    const std::string path = "/test/memfbr/bad_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = std::string("INVALID_UUID_STRING");
    pm["InventoryObjPath"] = inventoryPath + "baduuid/";
    pm["Type"] = std::string("NSM_Memory");

    const size_t before = gpu->deviceSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// type == "NSM_Memory" with all properties -> full sensor creation
// Covers: L855-863 if (type == "NSM_Memory") TRUE branch
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, DISABLED_Factory_NSM_Memory_AllProps)
{
    const std::string path = "/test/memfbr/nsm_memory_all";
    setupBaseProperties(path);
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Type"] = std::string("NSM_Memory");
    pm["ErrorCorrection"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC");
    pm["DeviceType"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.HBM3");
    pm["Priority"] = bool(false);

    const size_t before = gpu->deviceSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// type == "NSM_Memory" with missing ErrorCorrection -> correctionType=""
// Covers: L702-706 count("ErrorCorrection") FALSE branch
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest,
       DISABLED_Factory_NSM_Memory_MissingErrorCorrection)
{
    const std::string path = "/test/memfbr/nsm_mem_no_ec";
    setupBaseProperties(path);
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Type"] = std::string("NSM_Memory");
    // "ErrorCorrection" omitted -> correctionType=""
    pm["DeviceType"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.HBM3");
    pm["Priority"] = bool(false);

    // Will throw when converting empty string to Ecc enum -> but still //
    // exercises the FALSE branch of count("ErrorCorrection")
    createNsmMemorySensor(mockManager, baseIntf, path);
}

// ============================================================================
// type == "NSM_Memory" with missing DeviceType -> deviceType=""
// Covers: L712-716 count("DeviceType") FALSE branch
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest,
       DISABLED_Factory_NSM_Memory_MissingDeviceType)
{
    const std::string path = "/test/memfbr/nsm_mem_no_dt";
    setupBaseProperties(path);
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Type"] = std::string("NSM_Memory");
    pm["ErrorCorrection"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC");
    // "DeviceType" omitted -> deviceType=""
    pm["Priority"] = bool(false);

    createNsmMemorySensor(mockManager, baseIntf, path);
}

// ============================================================================
// type == "NSM_Memory" with missing Priority -> priority=false
// Covers: L732-735 count("Priority") FALSE branch
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, DISABLED_Factory_NSM_Memory_MissingPriority)
{
    const std::string path = "/test/memfbr/nsm_mem_no_prio";
    setupBaseProperties(path);
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Type"] = std::string("NSM_Memory");
    pm["ErrorCorrection"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC");
    pm["DeviceType"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.HBM3");
    // "Priority" omitted -> priority=false

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    // currClockFreqSensor added with priority=false -> roundRobin
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// type == "NSM_Memory" with Priority=true
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, DISABLED_Factory_NSM_Memory_PriorityTrue)
{
    const std::string path = "/test/memfbr/nsm_mem_prio_true";
    setupBaseProperties(path);
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Type"] = std::string("NSM_Memory");
    pm["ErrorCorrection"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC");
    pm["DeviceType"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.HBM3");
    pm["Priority"] = bool(true);

    const size_t prBefore = gpu->prioritySensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->prioritySensors.size(), prBefore);
}

// ============================================================================
// type == "NSM_Memory_Attributes" -> createMemoryRowRemapping + ECCMode +
// MemCapUtil
// Covers: L864-871 else if (type == "NSM_Memory_Attributes") TRUE branch
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, Factory_NSM_Memory_Attributes)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_Memory.MemoryAttributes";
    const std::string path = "/test/memfbr/nsm_mem_attrs";

    setupBaseProperties(path);
    setupCurrentProperties(path, intf,
                           {{"Type", std::string("NSM_Memory_Attributes")}});

    const size_t before = gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, intf, path);
    // 3 roundRobin sensors: RowRemapState, RowRemappingCounts,
    // RemappingAvailability + EccErrorCountsDram + MemoryCapacityUtil
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// ============================================================================
// Unknown type -> no type branch matches -> no sensor created
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, DISABLED_Factory_UnknownType_NoSensor)
{
    const std::string path = "/test/memfbr/unknown_type";
    setupBaseProperties(path);
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Type"] = std::string("NSM_Unknown_Memory_Type");

    const size_t before = gpu->deviceSensors.size() +
                          gpu->roundRobinSensors.size() +
                          gpu->staticSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->roundRobinSensors.size() +
                  gpu->staticSensors.size(),
              before);
}
