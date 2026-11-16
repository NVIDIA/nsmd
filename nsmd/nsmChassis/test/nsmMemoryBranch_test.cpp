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

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmMemory/nsmMemory.hpp"

#undef private
#undef protected

using namespace nsm;

/** @brief Run a memory feature creator for the device `name` (platform
 *  prefix "HGX_"), on `device` (no NsmDevice if null)
 */
static requester::Coroutine
    runMemoryFeature(nsm::features::CreatorFn create, SensorManager& manager,
                     std::shared_ptr<NsmDevice> device, const uuid_t& uuid,
                     const std::string& name, const dbus::PropertyMap& params)
{
    nsm::features::DeviceContext dev;
    dev.name = name;
    dev.kind = "gpu";
    dev.staticUuid = uuid;
    dev.platformPrefix = "HGX_";
    dev.device = std::move(device);
    return create(manager, dev, params);
}

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

// -- createMemoryFeature: DIMM inventory sensors of the device
TEST_F(NsmMemoryBatch11F, CreateMemoryFeature_CreatesMultipleSensors)
{
    dbus::PropertyMap params = {
        {"Name", std::string("MemBatch11F")},
        {"ErrorCorrection",
         std::string(
             "xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC")},
        {"DeviceType",
         std::string(
             "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.Other")},
        {"Priority", false},
    };

    const size_t before = gpu->deviceSensors.size();
    auto rc = runMemoryFeature(nsm::features::createMemoryFeature, mockManager,
                               gpu, gpuUuid, "HGX_GPU_Batch11F", params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    // error correction, device type, health, location, association, min/max
    // clock limit, current clock frequency, capacity
    EXPECT_GE(gpu->deviceSensors.size(), before + 5);
}

// -- createMemoryAttributesFeature: row remapping, ECC and capacity sensors
TEST_F(NsmMemoryBatch11F,
       CreateMemoryAttributesFeature_CreatesRowRemappingAndECC)
{
    dbus::PropertyMap params = {{"Name", std::string("MemAttrBatch11F")}};

    auto rc = runMemoryFeature(nsm::features::createMemoryAttributesFeature,
                               mockManager, gpu, gpuUuid,
                               "HGX_GPU_AttrBatch11F", params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
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
    EXPECT_EQ(rowRemapIntf->rowRemappingFailureState(),
              MemoryRowRemappingIntf::RowRemappingFailureStates::True);
    EXPECT_EQ(rowRemapIntf->rowRemappingPendingState(),
              MemoryRowRemappingIntf::RowRemappingPendingStates::True);
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
    EXPECT_EQ(rowRemapIntf->rowRemappingFailureState(),
              MemoryRowRemappingIntf::RowRemappingFailureStates::False);
    EXPECT_EQ(rowRemapIntf->rowRemappingPendingState(),
              MemoryRowRemappingIntf::RowRemappingPendingStates::False);
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
// SECTION 2: memory feature creators (createMemoryFeature,
// createMemoryAttributesFeature, createMemoryLocationFeature)
// =============================================================================

struct NsmMemoryFactoryBranchTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
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

    // Complete NSM_Memory feature params, optionally omitting one of them
    static dbus::PropertyMap memParams(bool includeName = true,
                                       bool includeErrCorr = true,
                                       bool includeDevType = true,
                                       bool includePriority = true)
    {
        dbus::PropertyMap params;
        if (includeName)
        {
            params["Name"] = std::string("MemFactory");
        }
        if (includeErrCorr)
        {
            params["ErrorCorrection"] = std::string(
                "xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC");
        }
        if (includeDevType)
        {
            params["DeviceType"] = std::string(
                "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.Other");
        }
        if (includePriority)
        {
            params["Priority"] = false;
        }
        return params;
    }

    requester::Coroutine run(nsm::features::CreatorFn create,
                             const std::string& name,
                             const dbus::PropertyMap& params,
                             bool withDevice = true)
    {
        return runMemoryFeature(create, mockManager, withDevice ? gpu : nullptr,
                                gpuUuid, name, params);
    }
};

// No NsmDevice → NSM_ERROR, no sensor
TEST_F(NsmMemoryFactoryBranchTest, MemoryFeature_NoNsmDevice_Error)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createMemoryFeature, "HGX_GPU_NoDevice",
                  memParams(), false);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

TEST_F(NsmMemoryFactoryBranchTest, MemoryAttributesFeature_NoNsmDevice_Error)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto rc = run(nsm::features::createMemoryAttributesFeature,
                  "HGX_GPU_AttrNoDevice", {}, false);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(before, gpu->roundRobinSensors.size());
}

TEST_F(NsmMemoryFactoryBranchTest, MemoryLocationFeature_NoNsmDevice_Error)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createMemoryLocationFeature,
                  "HGX_GPU_LocNoDevice",
                  {{"LocationCode", std::string("SXM1")}}, false);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// ErrorCorrection absent → correctionType="" → DimmIntf::convertEccFromString
// throws out of the creator.
TEST_F(NsmMemoryFactoryBranchTest, MemoryFeature_MissingErrorCorrection_Throws)
{
    EXPECT_THROW_COROUTINE(run(nsm::features::createMemoryFeature,
                               "HGX_GPU_MissingErrCorr",
                               memParams(true, false)),
                           std::exception);
}

// Priority absent → priority defaults to false, all sensors are created.
TEST_F(NsmMemoryFactoryBranchTest, MemoryFeature_MissingPriority_Created)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createMemoryFeature, "HGX_GPU_MissingPriority",
                  memParams(true, true, true, false));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// DeviceType absent → deviceType="" → NsmMemoryDeviceType ctor calls
// DimmIntf::convertDeviceTypeFromString("") → throws out of the creator.
TEST_F(NsmMemoryFactoryBranchTest, MemoryFeature_MissingDeviceType_Throws)
{
    EXPECT_THROW_COROUTINE(run(nsm::features::createMemoryFeature,
                               "HGX_GPU_MissingDevType",
                               memParams(true, true, false)),
                           std::exception);
}

// Name absent → name="" → sensors are still created since name is just a
// label, not part of the D-Bus path.
TEST_F(NsmMemoryFactoryBranchTest, MemoryFeature_MissingName_SensorsCreated)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createMemoryFeature, "HGX_GPU_MissingName",
                  memParams(false));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// Association params → parent_processor / parent_chassis links on the DIMM;
// empty entries give no link
TEST_F(NsmMemoryFactoryBranchTest, MemoryFeature_Associations)
{
    auto params = memParams();
    params["ProcessorAssociationPaths"] =
        std::vector<std::string>{"processors/HGX_GPU_Assoc", ""};
    params["ParentChassisAssociationPaths"] =
        std::vector<std::string>{"chassis/HGX_GPU_Assoc"};

    auto rc = run(nsm::features::createMemoryFeature, "HGX_GPU_Assoc", params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    std::shared_ptr<NsmMemoryAssociation> association;
    for (const auto& sensor : gpu->deviceSensors)
    {
        if (auto s = std::dynamic_pointer_cast<NsmMemoryAssociation>(sensor))
        {
            association = s;
        }
    }
    ASSERT_NE(association, nullptr);
    auto links = association->associationDef->associations();
    ASSERT_EQ(links.size(), 2);
    EXPECT_EQ(std::get<0>(links[0]), "parent_processor");
    EXPECT_EQ(std::get<1>(links[0]), "all_memory");
    EXPECT_EQ(std::get<2>(links[0]),
              "/xyz/openbmc_project/inventory/system/processors/"
              "HGX_GPU_Assoc");
    EXPECT_EQ(std::get<0>(links[1]), "parent_chassis");
    EXPECT_EQ(std::get<1>(links[1]), "all_memory");
    EXPECT_EQ(std::get<2>(links[1]),
              "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_Assoc");
}

// createMemoryAttributesFeature → row remapping, ECC and capacity
// utilization sensors.
TEST_F(NsmMemoryFactoryBranchTest, MemoryAttributesFeature_AllProps)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto rc = run(nsm::features::createMemoryAttributesFeature, "HGX_GPU_Attrs",
                  {{"Name", std::string("MemAttrs")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// createMemoryRowRemapping adds 3 RR sensors, createMemoryECCMode adds 1,
// createMemoryMemCapacityUtil adds 1 (long running) → at least 4 new
// round-robin sensors.
TEST_F(NsmMemoryFactoryBranchTest,
       MemoryAttributesFeature_VerifiesAllThreeSensors)
{
    const size_t beforeRR = gpu->roundRobinSensors.size();
    auto rc = run(nsm::features::createMemoryAttributesFeature,
                  "HGX_GPU_AttrsFull", {{"Name", std::string("MemAttrsFull")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), beforeRR + 4);
}

// createMemoryLocationFeature: one sensor per non-empty location param
TEST_F(NsmMemoryFactoryBranchTest, MemoryLocationFeature_CodeAndContext)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createMemoryLocationFeature, "HGX_GPU_Loc",
                  {{"Name", std::string("MemLoc")},
                   {"LocationCode", std::string("SXM1")},
                   {"LocationContext", std::string("HGX_ProcessorModule_0")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before + 2);
}

TEST_F(NsmMemoryFactoryBranchTest, MemoryLocationFeature_EmptyCodeSkipped)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createMemoryLocationFeature,
                  "HGX_GPU_LocEmpty",
                  {{"Name", std::string("MemLocEmpty")},
                   {"LocationCode", std::string("")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}
