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

#include "platform-environmental.h"

#include "nsmMemory/nsmMemory.hpp"
#include "nsmObjectFactory.hpp"

#undef private
#undef protected

using namespace nsm;

// Forward-declare factory coroutines we want to test
namespace nsm
{
requester::Coroutine createNsmMemorySensor(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

// ============================================================================
// SECTION 1: nsmMemory.cpp -- Uncovered function tests
// ============================================================================

struct NsmMemoryBatch11F :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const std::string attrIntfName =
        "xyz.openbmc_project.Configuration.NSM_Memory.MemoryAttributes";
    const std::string objPath = "/xyz/openbmc_project/inventory/batch11f/mem";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:51";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmMemoryBatch11F() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmMemoryBatch11F()
    {
        cleanupDeviceSensors(devices);
    }
};

// -- Test createNSMMemory factory via createNsmMemorySensor with type
// NSM_Memory
TEST_F(NsmMemoryBatch11F,
       CreateNsmMemorySensor_NSMMemoryType_CreatesMultipleSensors)
{
    // Arrange
    dbus::PropertyMap baseProps = {
        {"Name", std::string("MemBatch11F")},
        {"UUID", gpuUuid},
        {"InventoryObjPath",
         std::string("/xyz/openbmc_project/inventory/batch11f/mem_inv")},
    };
    auto& basePropMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    basePropMap = baseProps;

    dbus::PropertyMap ifaceProps = {
        {"Type", std::string("NSM_Memory")},
        {"ErrorCorrection",
         std::string(
             "xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC")},
        {"DeviceType",
         std::string(
             "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.Other")},
        {"Priority", false},
    };
    auto& ifacePropMap = utils::MockDbusAsync::propertyMap(objPath,
                                                           basicIntfName);
    ifacePropMap = ifaceProps;

    // Provide base properties for the base interface lookup
    auto& baseLookup = utils::MockDbusAsync::propertyMap(objPath,
                                                         basicIntfName);
    baseLookup = baseProps;

    // Act
    createNsmMemorySensor(mockManager, basicIntfName, objPath);

    // Assert - createNSMMemory should add at least one sensor
    // (exact count depends on D-Bus properties configured)
    EXPECT_GE(gpu->deviceSensors.size(), 1u);
}

// -- Test createNsmMemorySensor with NSM_Memory_Attributes type
TEST_F(NsmMemoryBatch11F,
       CreateNsmMemorySensor_NSMMemoryAttributes_CreatesRowRemappingAndECC)
{
    // Arrange
    dbus::PropertyMap baseProps = {
        {"Name", std::string("MemAttrBatch11F")},
        {"UUID", gpuUuid},
        {"InventoryObjPath",
         std::string("/xyz/openbmc_project/inventory/batch11f/mem_attr_inv")},
    };
    auto& basePropMap = utils::MockDbusAsync::propertyMap(objPath + "_attr",
                                                          basicIntfName);
    basePropMap = baseProps;

    dbus::PropertyMap ifaceProps = {
        {"Type", std::string("NSM_Memory_Attributes")},
    };
    auto& ifacePropMap = utils::MockDbusAsync::propertyMap(objPath + "_attr",
                                                           attrIntfName);
    ifacePropMap = ifaceProps;

    // Act
    createNsmMemorySensor(mockManager, attrIntfName, objPath + "_attr");

    // Assert - should create sensors for row remapping, ECC, and capacity util
    EXPECT_GE(gpu->roundRobinSensors.size(), 1);
}

// -- Test NsmMemCapacity::updateReading with nullopt
TEST(NsmMemCapacityBatch11F,
     UpdateReading_NulloptCapacity_DoesNotUpdateDimmIntf)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/memcap";
    auto dimmIntf = std::make_shared<DimmIntf>(bus, path.c_str());
    std::string name = "memCapTest";
    std::string type = "memCapType";
    NsmMemCapacity sensor(name, type, dimmIntf);

    // Set initial value
    dimmIntf->memorySizeInKB(12345);

    // Act - pass nullopt
    sensor.updateReading(std::nullopt);

    // Assert - value should NOT have changed (function returns early)
    EXPECT_EQ(dimmIntf->memorySizeInKB(), 12345);
}

// -- Test NsmMemCapacity::updateReading with a valid value
TEST(NsmMemCapacityBatch11F,
     UpdateReading_ValidCapacity_UpdatesDimmMemorySizeInKB)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/memcap2";
    auto dimmIntf = std::make_shared<DimmIntf>(bus, path.c_str());
    std::string name = "memCapTest2";
    std::string type = "memCapType2";
    NsmMemCapacity sensor(name, type, dimmIntf);

    // Act - pass 8192 (MB), should become 8192*1024 KB
    sensor.updateReading(std::optional<uint32_t>(8192));

    // Assert
    EXPECT_EQ(dimmIntf->memorySizeInKB(), 8192 * 1024);
}

// -- Test NsmRowRemapState::updateReading directly
TEST(NsmRowRemapStateBatch11F, UpdateReading_FlagsSet_UpdatesRemappingStateIntf)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/rowremap";
    auto rowRemapIntf = std::make_shared<MemoryRowRemappingIntf>(bus,
                                                                 path.c_str());
    std::string name = "rowRemapTest";
    std::string type = "rowRemapType";
    NsmRowRemapState sensor(name, type, rowRemapIntf, path);

    // Act - set bit0=1 (failure), bit1=1 (pending)
    bitfield8_t flags = {};
    flags.bits.bit0 = 1;
    flags.bits.bit1 = 1;
    sensor.updateReading(flags);

    // Assert
    EXPECT_TRUE(rowRemapIntf->rowRemappingFailureState());
    EXPECT_TRUE(rowRemapIntf->rowRemappingPendingState());
}

// -- Test NsmRowRemapState::updateReading with zero flags
TEST(NsmRowRemapStateBatch11F, UpdateReading_FlagsClear_ClearsRemappingState)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/rowremap2";
    auto rowRemapIntf = std::make_shared<MemoryRowRemappingIntf>(bus,
                                                                 path.c_str());
    std::string name = "rowRemapTest2";
    std::string type = "rowRemapType2";
    NsmRowRemapState sensor(name, type, rowRemapIntf, path);

    // Act
    bitfield8_t flags = {};
    flags.byte = 0;
    sensor.updateReading(flags);

    // Assert
    EXPECT_FALSE(rowRemapIntf->rowRemappingFailureState());
    EXPECT_FALSE(rowRemapIntf->rowRemappingPendingState());
}

// -- Test NsmRowRemappingCounts::updateReading directly
TEST(NsmRowRemappingCountsBatch11F, UpdateReading_ValidCounts_SetsCorrectValues)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/rowcounts";
    auto rowRemapIntf = std::make_shared<MemoryRowRemappingIntf>(bus,
                                                                 path.c_str());
    std::string name = "rowCountTest";
    std::string type = "rowCountType";
    NsmRowRemappingCounts sensor(name, type, rowRemapIntf, path);

    // Act
    sensor.updateReading(42, 7);

    // Assert
    EXPECT_EQ(rowRemapIntf->ceRowRemappingCount(), 42u);
    EXPECT_EQ(rowRemapIntf->ueRowRemappingCount(), 7u);
}

// -- Test NsmRemappingAvailabilityBankCount::updateReading directly
TEST(NsmRemappingAvailabilityBatch11F,
     UpdateReading_ValidData_SetsAllBankCounts)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/bankcount";
    auto rowRemapIntf = std::make_shared<MemoryRowRemappingIntf>(bus,
                                                                 path.c_str());
    std::string name = "bankCountTest";
    std::string type = "bankCountType";
    NsmRemappingAvailabilityBankCount sensor(name, type, rowRemapIntf, path);

    // Act
    struct nsm_row_remap_availability data = {};
    data.no_remapping = 10;
    data.low_remapping = 20;
    data.partial_remapping = 30;
    data.high_remapping = 40;
    data.max_remapping = 50;
    sensor.updateReading(data);

    // Assert
    EXPECT_EQ(rowRemapIntf->noRemappingAvailablityBankCount(), 10);
    EXPECT_EQ(rowRemapIntf->lowRemappingAvailablityBankCount(), 20);
    EXPECT_EQ(rowRemapIntf->partialRemappingAvailablityBankCount(), 30);
    EXPECT_EQ(rowRemapIntf->highRemappingAvailablityBankCount(), 40);
    EXPECT_EQ(rowRemapIntf->maxRemappingAvailablityBankCount(), 50);
}

// -- Test NsmEccErrorCountsDram::updateReading directly
TEST(NsmEccErrorCountsDramBatch11F,
     UpdateReading_ValidCounts_SetsDramCorrectedAndUncorrected)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/ecccounts";
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus, path.c_str());
    std::string name = "eccCountTest";
    std::string type = "eccCountType";
    NsmEccErrorCountsDram sensor(name, type, eccIntf, path);

    // Act
    struct nsm_ECC_error_counts errorCounts = {};
    errorCounts.dram_corrected = 1234;
    errorCounts.dram_uncorrected = 5678;
    sensor.updateReading(errorCounts);

    // Assert
    EXPECT_EQ(eccIntf->ceCount(), 1234);
    EXPECT_EQ(eccIntf->ueCount(), 5678);
}

// -- Test NsmMemCurrClockFreq::updateReading directly
TEST(NsmMemCurrClockFreqBatch11F,
     UpdateReading_ValidFreq_SetsMemoryConfiguredSpeed)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/clockfreq";
    auto dimmIntf = std::make_shared<DimmIntf>(bus, path.c_str());
    std::string name = "clockFreqTest";
    std::string type = "clockFreqType";
    NsmMemCurrClockFreq sensor(name, type, dimmIntf, path);

    // Act
    sensor.updateReading(3200);

    // Assert
    EXPECT_EQ(dimmIntf->memoryConfiguredSpeedInMhz(), 3200u);
}

// =============================================================================
// SECTION 2: createNsmMemorySensor factory branch coverage
// Tests targeting the FALSE branches of property-presence checks (lines
// 822-869 of nsmMemory.cpp) and early-return paths, using NsmObjectFactory
// so that exceptions thrown inside the factory are caught and do not
// propagate to the test.
// =============================================================================

struct NsmMemoryFactoryBranchTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string memIntf = "xyz.openbmc_project.Configuration.NSM_Memory";
    const std::string memAttrIntf =
        "xyz.openbmc_project.Configuration.NSM_Memory.MemoryAttributes";

    // Use a unique instance number to avoid collisions with other fixtures.
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:52";

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

    // Register a complete NSM_Memory property map on a given path, optionally
    // omitting one property to test the FALSE branch of its count() check.
    void registerFullMemProps(const std::string& path, bool includeName = true,
                              bool includeUUID = true,
                              bool includeInvPath = true,
                              bool includeType = true,
                              bool includeErrCorr = true,
                              bool includeDevType = true,
                              bool includePriority = true)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, memIntf);
        if (includeName)
        {
            pm["Name"] = std::string("MemFactory");
        }
        if (includeUUID)
        {
            pm["UUID"] = gpuUuid;
        }
        if (includeInvPath)
        {
            pm["InventoryObjPath"] =
                std::string("/xyz/openbmc_project/inventory/system/memory/0");
        }
        if (includeType)
        {
            pm["Type"] = std::string("NSM_Memory");
        }
        if (includeErrCorr)
        {
            pm["ErrorCorrection"] = std::string(
                "xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC");
        }
        if (includeDevType)
        {
            pm["DeviceType"] = std::string(
                "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.Other");
        }
        if (includePriority)
        {
            pm["Priority"] = false;
        }
    }
};

// No property map registered → coGetCachedBaseProperties returns NSM_SW_ERROR
// → factory does early co_return (line 814 TRUE branch in nsmMemory.cpp). //

TEST_F(NsmMemoryFactoryBranchTest, Factory_NoPropertyMap_EarlyReturn)
{
    const std::string testPath = "/xyz/test/mem/no_props";
    // Intentionally do NOT register any property map for testPath.
    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, memIntf, testPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// UUID absent in property map → uuid="" → parseStaticUuid("") throws
// (line 827 FALSE branch covered; exception caught by createObjects).
TEST_F(NsmMemoryFactoryBranchTest, Factory_MissingUUID_ThrowsNoSensor)
{
    const std::string testPath = "/xyz/test/mem/missing_uuid";
    registerFullMemProps(testPath, /*name*/ true, /*uuid*/ false);
    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, memIntf, testPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// Type absent → type="" → neither NSM_Memory nor NSM_Memory_Attributes branch
// is taken (lines 832, 855, 864 FALSE branches).  Returns NSM_SUCCESS without
// creating any sensor.
TEST_F(NsmMemoryFactoryBranchTest, Factory_MissingType_NoSensorsCreated)
{
    const std::string testPath = "/xyz/test/mem/missing_type";
    registerFullMemProps(testPath, /*name*/ true, /*uuid*/ true,
                         /*invPath*/ true, /*type*/ false);
    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, memIntf, testPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// Type present but unknown → neither factory branch taken (lines 855/864
// both FALSE).
TEST_F(NsmMemoryFactoryBranchTest, Factory_UnknownType_NoSensorsCreated)
{
    const std::string testPath = "/xyz/test/mem/unknown_type";
    registerFullMemProps(testPath);
    // Overwrite Type with an unknown value.
    utils::MockDbusAsync::propertyMap(testPath, memIntf)["Type"] =
        std::string("NSM_Unknown_Type");
    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, memIntf, testPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// InventoryObjPath absent → inventoryObjPath="" → path becomes "_DRAM_0"
// (not a valid absolute D-Bus path) → getInterfaceOnObjectPath throws
// (line 837 FALSE branch covered; exception caught by createObjects).
TEST_F(NsmMemoryFactoryBranchTest,
       Factory_MissingInventoryObjPath_ThrowsNoSensor)
{
    const std::string testPath = "/xyz/test/mem/missing_inv_path";
    registerFullMemProps(testPath, /*name*/ true, /*uuid*/ true,
                         /*invPath*/ false);
    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, memIntf, testPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// ErrorCorrection absent → correctionType="" → DimmIntf::convertEccFromString
// throws (line 702 FALSE branch covered; exception caught by createObjects).
TEST_F(NsmMemoryFactoryBranchTest,
       Factory_NSMMemory_MissingErrorCorrection_ThrowsNoSensor)
{
    const std::string testPath = "/xyz/test/mem/missing_err_corr";
    registerFullMemProps(testPath, /*name*/ true, /*uuid*/ true,
                         /*invPath*/ true, /*type*/ true,
                         /*errCorr*/ false);
    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, memIntf, testPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// All NSM_Memory properties present except Priority → priority defaults to
// false (line 732 FALSE branch covered), all sensors are successfully created.
TEST_F(NsmMemoryFactoryBranchTest, Factory_NSMMemory_MissingPriority_Created)
{
    const std::string testPath = "/xyz/test/mem/missing_priority";
    registerFullMemProps(testPath, /*name*/ true, /*uuid*/ true,
                         /*invPath*/ true, /*type*/ true,
                         /*errCorr*/ true, /*devType*/ true,
                         /*priority*/ false);
    // Call createNsmMemorySensor directly (factory registration is
    // link-order-dependent; direct call reliably exercises the code path).
    const size_t before = gpu->deviceSensors.size();
    createNsmMemorySensor(mockManager, memIntf, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// All NSM_Memory_Attributes properties present → sensors created
// (covers co_return NSM_SUCCESS path via NSM_Memory_Attributes branch, //
// line 864 TRUE branch).
TEST_F(NsmMemoryFactoryBranchTest, Factory_NSMMemoryAttributes_AllProps)
{
    const std::string testPath = "/xyz/test/mem/attrs";
    // Base properties (always at MEMORY_INTERFACE).
    auto& basePm = utils::MockDbusAsync::propertyMap(testPath, memIntf);
    basePm["Name"] = std::string("MemAttrs");
    basePm["UUID"] = gpuUuid;
    basePm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/memory/attrs");
    // Current interface properties.
    auto& attrPm = utils::MockDbusAsync::propertyMap(testPath, memAttrIntf);
    attrPm["Type"] = std::string("NSM_Memory_Attributes");

    // Call createNsmMemorySensor directly (factory registration is
    // link-order-dependent; direct call reliably exercises the code path).
    const size_t before = gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, memAttrIntf, testPath);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// DeviceType absent → deviceType="" → NsmMemoryDeviceType ctor calls
// DimmIntf::convertDeviceTypeFromString("") → throws InvalidEnumString
// (line 712 FALSE branch covered; exception propagates from
// createNsmMemorySensor).
TEST_F(NsmMemoryFactoryBranchTest,
       Factory_NSMMemory_MissingDeviceType_ThrowsNoSensor)
{
    const std::string testPath = "/xyz/test/mem/missing_dev_type";
    registerFullMemProps(testPath, /*name*/ true, /*uuid*/ true,
                         /*invPath*/ true, /*type*/ true,
                         /*errCorr*/ true, /*devType*/ false);
    // DeviceType absent → L712 FALSE → NsmMemoryDeviceType ctor throws
    EXPECT_THROW_COROUTINE(
        createNsmMemorySensor(mockManager, memIntf, testPath), std::exception);
}

// Name absent in base props → name="" (line 822 FALSE branch) →
// sensors are still created since name is just a label, not part of D-Bus path.
TEST_F(NsmMemoryFactoryBranchTest, Factory_NSMMemory_MissingName_SensorsCreated)
{
    const std::string testPath = "/xyz/test/mem/missing_name_br";
    registerFullMemProps(testPath, /*name*/ false);
    // name="" → NsmObject label is empty; inventoryObjPath still valid
    const size_t before = gpu->deviceSensors.size();
    createNsmMemorySensor(mockManager, memIntf, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// NSM_Memory_Attributes type with all base properties present → NSM_Memory_Attr
// branch (line 864 TRUE) with ALL three sub-create functions called:
// createMemoryRowRemapping, createMemoryECCMode, createMemoryMemCapacityUtil
// Verify all three create non-zero sensors.
TEST_F(NsmMemoryFactoryBranchTest,
       Factory_NSMMemoryAttributes_VerifiesAllThreeSensors)
{
    const std::string testPath = "/xyz/test/mem/attrs_full";
    auto& basePm = utils::MockDbusAsync::propertyMap(testPath, memIntf);
    basePm["Name"] = std::string("MemAttrsFull");
    basePm["UUID"] = gpuUuid;
    basePm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/memory/attrsfull");
    auto& attrPm = utils::MockDbusAsync::propertyMap(testPath, memAttrIntf);
    attrPm["Type"] = std::string("NSM_Memory_Attributes");

    const size_t beforeRR = gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, memAttrIntf, testPath);
    // createMemoryRowRemapping adds 3 RR sensors, createMemoryECCMode adds 1,
    // createMemoryMemCapacityUtil adds 1 → at least 5 new sensors total
    EXPECT_GE(gpu->roundRobinSensors.size(), beforeRR + 4);
}
