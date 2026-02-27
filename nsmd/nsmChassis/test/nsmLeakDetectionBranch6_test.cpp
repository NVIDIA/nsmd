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
 * Branch coverage tests for nsmLeakDetection.cpp - batch 6
 *
 * Targets constructor coverage:
 * - NsmLeakDetectionThresholdsPatch constructor: stores sensorId,
 *   sensorValueIntf, thresholdIntf; sets name and type via NsmObject base
 * - NsmSetLeakDetectionThresholds constructor: stores name, type, sensorIdMap,
 *   minThresholds, criticalThresholds, maxThresholds
 * - NsmLeakDetection constructor: creates inventory, state, and sensor
 *   interfaces for each sensor in the id/name maps
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "libnsm/platform-environmental.h"

#include "nsmLeakDetection.hpp"
#include "test/commonMock.hpp"

#include <memory>
#include <tuple>

namespace nsm
{
requester::Coroutine nsmLeakDetectionCreateSensors(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath);
} // namespace nsm

using namespace nsm;

/** Assert Area.PhysicalContext on the leak detector inventory for sensorId. */
static void expectLeakDetectorPhysicalContext(
    const std::shared_ptr<NsmLeakDetection>& ld, uint8_t sensorId,
    const char* physicalContextName)
{
    const auto& inv = ld->leakDetectorInventoryIntfMap.at(sensorId);
    const auto& areaIntf = std::get<2>(inv);
    ASSERT_NE(areaIntf, nullptr);
    EXPECT_EQ(
        areaIntf->physicalContext(),
        AreaIntf::convertPhysicalContextTypeFromString(
            std::string(
                "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.") +
            physicalContextName));
}

static void
    expectLeakDetectorMoistureType(const std::shared_ptr<NsmLeakDetection>& ld,
                                   uint8_t sensorId)
{
    const auto& inv = ld->leakDetectorInventoryIntfMap.at(sensorId);
    const auto& leakDetectorIntf = std::get<0>(inv);
    EXPECT_EQ(
        leakDetectorIntf->leakDetectorType(),
        LeakDetectorIntf::convertLeakDetectorTypeEnumFromString(
            "xyz.openbmc_project.Inventory.Item.LeakDetector.LeakDetectorTypeEnum.Moisture"));
}

// ============================================================================
// Fixture
// ============================================================================

struct NsmLeakDetectionBranch6Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_B6";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:6";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmLeakDetectionBranch6Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmLeakDetectionBranch6Test()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmLeakDetection>
        makeSensor(const std::string& prefix, std::vector<uint8_t> ids,
                   const std::vector<std::string>& physicalContextMap = {})
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string n = "LeakB6_" + prefix;
        std::vector<uint64_t> idMap;
        std::vector<std::string> nameMap;
        for (auto id : ids)
        {
            idMap.push_back(id);
            nameMap.push_back(prefix + "_s" + std::to_string(id));
        }
        return std::make_shared<NsmLeakDetection>(n, "NSM_LeakDetection", bus,
                                                  idMap, nameMap, chassisPath,
                                                  5.0, 0.0, physicalContextMap);
    }
};

// ============================================================================
// NsmLeakDetectionThresholdsPatch constructor tests
// ============================================================================

TEST_F(NsmLeakDetectionBranch6Test,
       ThresholdsPatchCtor_StoresSensorIdAndInterfaces)
{
    auto ld = makeSensor("patchctor", {10});
    auto sensorIntfs = ld->getSensorInterfaces(10);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        10, sensorValueIntf, thresholdIntf);

    EXPECT_EQ(patch->sensorId, 10);
    EXPECT_EQ(patch->sensorValueIntf, sensorValueIntf);
    EXPECT_EQ(patch->thresholdIntf, thresholdIntf);
    EXPECT_EQ(patch->getName(), "LeakDetectionThresholdsPatch_10");
    EXPECT_EQ(patch->getType(), "NSM_LeakDetection_ThresholdsPatch");
    EXPECT_FALSE(patch->asyncPatchInProgress);
}

TEST_F(NsmLeakDetectionBranch6Test, ThresholdsPatchCtor_DifferentSensorId)
{
    auto ld = makeSensor("patchctor2", {42});
    auto sensorIntfs = ld->getSensorInterfaces(42);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        42, sensorValueIntf, thresholdIntf);

    EXPECT_EQ(patch->sensorId, 42);
    EXPECT_EQ(patch->getName(), "LeakDetectionThresholdsPatch_42");
}

// ============================================================================
// NsmSetLeakDetectionThresholds constructor tests
// ============================================================================

TEST_F(NsmLeakDetectionBranch6Test, SetThresholdsCtor_StoresAllParameters)
{
    std::string name = "SetLeakThresholds_B6";
    std::string type = "NSM_LeakDetection_SetThresholds";
    std::vector<uint64_t> sensorIdMap = {1, 2, 3};
    std::vector<uint64_t> minThresholds = {100, 200, 300};
    std::vector<uint64_t> criticalThresholds = {500, 600, 700};
    std::vector<uint64_t> maxThresholds = {900, 1000, 1100};

    auto setter = std::make_shared<NsmSetLeakDetectionThresholds>(
        name, type, sensorIdMap, minThresholds, criticalThresholds,
        maxThresholds);

    EXPECT_EQ(setter->getName(), name);
    EXPECT_EQ(setter->getType(), type);
    EXPECT_EQ(setter->sensorIdMap, sensorIdMap);
    EXPECT_EQ(setter->minThresholds, minThresholds);
    EXPECT_EQ(setter->criticalThresholds, criticalThresholds);
    EXPECT_EQ(setter->maxThresholds, maxThresholds);
}

TEST_F(NsmLeakDetectionBranch6Test, SetThresholdsCtor_EmptyVectors)
{
    std::string name = "SetLeakThresholds_Empty";
    std::string type = "NSM_LeakDetection_SetThresholds";
    std::vector<uint64_t> empty;

    auto setter = std::make_shared<NsmSetLeakDetectionThresholds>(
        name, type, empty, empty, empty, empty);

    EXPECT_EQ(setter->getName(), name);
    EXPECT_EQ(setter->getType(), type);
    EXPECT_TRUE(setter->sensorIdMap.empty());
    EXPECT_TRUE(setter->minThresholds.empty());
    EXPECT_TRUE(setter->criticalThresholds.empty());
    EXPECT_TRUE(setter->maxThresholds.empty());
}

TEST_F(NsmLeakDetectionBranch6Test, SetThresholdsCtor_SingleSensor)
{
    std::string name = "SetLeakThresholds_Single";
    std::string type = "NSM_LeakDetection_SetThresholds";
    std::vector<uint64_t> sensorIdMap = {7};
    std::vector<uint64_t> minThresholds = {50};
    std::vector<uint64_t> criticalThresholds = {250};
    std::vector<uint64_t> maxThresholds = {500};

    auto setter = std::make_shared<NsmSetLeakDetectionThresholds>(
        name, type, sensorIdMap, minThresholds, criticalThresholds,
        maxThresholds);

    EXPECT_EQ(setter->sensorIdMap.size(), 1u);
    EXPECT_EQ(setter->sensorIdMap[0], 7u);
    EXPECT_EQ(setter->minThresholds[0], 50u);
    EXPECT_EQ(setter->criticalThresholds[0], 250u);
    EXPECT_EQ(setter->maxThresholds[0], 500u);
}

// ============================================================================
// NsmLeakDetection constructor tests
// ============================================================================

TEST_F(NsmLeakDetectionBranch6Test,
       LeakDetectionCtor_SingleSensor_CreatesAllMaps)
{
    auto ld = makeSensor("ctor1", {1});

    // Verify all three maps have an entry for sensor id 1
    EXPECT_EQ(ld->leakDetectorInventoryIntfMap.size(), 1u);
    EXPECT_EQ(ld->leakDetectorStateIntfMap.size(), 1u);
    EXPECT_EQ(ld->sensorValueIntfMap.size(), 1u);

    EXPECT_NE(ld->leakDetectorInventoryIntfMap.find(1),
              ld->leakDetectorInventoryIntfMap.end());
    EXPECT_NE(ld->leakDetectorStateIntfMap.find(1),
              ld->leakDetectorStateIntfMap.end());
    EXPECT_NE(ld->sensorValueIntfMap.find(1), ld->sensorValueIntfMap.end());
}

TEST_F(NsmLeakDetectionBranch6Test,
       LeakDetectionCtor_MultipleSensors_CreatesEntriesForEach)
{
    auto ld = makeSensor("ctormulti", {3, 7, 15});

    EXPECT_EQ(ld->leakDetectorInventoryIntfMap.size(), 3u);
    EXPECT_EQ(ld->leakDetectorStateIntfMap.size(), 3u);
    EXPECT_EQ(ld->sensorValueIntfMap.size(), 3u);

    for (uint8_t id : {3, 7, 15})
    {
        EXPECT_NE(ld->leakDetectorInventoryIntfMap.find(id),
                  ld->leakDetectorInventoryIntfMap.end())
            << "Missing inventory entry for sensor " << (int)id;
        EXPECT_NE(ld->leakDetectorStateIntfMap.find(id),
                  ld->leakDetectorStateIntfMap.end())
            << "Missing state entry for sensor " << (int)id;
        EXPECT_NE(ld->sensorValueIntfMap.find(id), ld->sensorValueIntfMap.end())
            << "Missing sensor value entry for sensor " << (int)id;
    }
}

TEST_F(NsmLeakDetectionBranch6Test, LeakDetectionCtor_SensorValueInitialization)
{
    auto ld = makeSensor("ctorinit", {5});

    auto& [sensorValueIntf, assocIntf,
           thresholdIntf] = ld->sensorValueIntfMap[5];

    // Verify initial sensor value properties
    EXPECT_EQ(sensorValueIntf->value(), 0);
    EXPECT_EQ(sensorValueIntf->maxValue(), 5.0);
    EXPECT_EQ(sensorValueIntf->minValue(), 0.0);
    EXPECT_EQ(sensorValueIntf->unit(),
              SensorValueIntf::convertUnitFromString(
                  "xyz.openbmc_project.Sensor.Value.Unit.Volts"));
}

TEST_F(NsmLeakDetectionBranch6Test,
       LeakDetectionCtor_OperationalStatusInitialization)
{
    auto ld = makeSensor("ctorops", {2});

    auto& [assocIntf, opIntf, stateIntf] = ld->leakDetectorStateIntfMap[2];

    EXPECT_FALSE(opIntf->functional());
    EXPECT_EQ(
        opIntf->state(),
        OperationalStatusIntf::convertStateTypeFromString(
            "xyz.openbmc_project.State.Decorator.OperationalStatus.StateType.None"));
    EXPECT_EQ(
        stateIntf->detectorState(),
        StateLeakDetectorIntf::convertDetectorStateEnumFromString(
            "xyz.openbmc_project.State.LeakDetector.DetectorStateEnum.Unavailable"));
}

TEST_F(NsmLeakDetectionBranch6Test,
       LeakDetectionCtor_LeakDetectorTypeInitialization)
{
    // Single sensor id 8: physicalContextMap index 0 applies to that sensor.
    auto ld = makeSensor("ctortype", {8}, {"Board"});

    expectLeakDetectorPhysicalContext(ld, 8, "Board");
    expectLeakDetectorMoistureType(ld, 8);
}

TEST_F(NsmLeakDetectionBranch6Test,
       LeakDetectionCtor_EmptyPhysicalContextMap_DefaultsToBoard)
{
    auto ld = makeSensor("empty_pc", {8});

    expectLeakDetectorPhysicalContext(ld, 8, "Board");
    expectLeakDetectorMoistureType(ld, 8);
}

TEST_F(NsmLeakDetectionBranch6Test,
       LeakDetectionCtor_InvalidPhysicalContextMapEntry_FallbackToBoard)
{
    auto ld = makeSensor("bad_pc", {8}, {"NotARealPhysicalContext"});

    expectLeakDetectorPhysicalContext(ld, 8, "Board");
    expectLeakDetectorMoistureType(ld, 8);
}

TEST_F(NsmLeakDetectionBranch6Test,
       LeakDetectionCtor_MultipleSensorsWithDistinctContexts)
{
    auto ld = makeSensor("distinct_pc", {8, 9}, {"Board", "CPU"});

    expectLeakDetectorPhysicalContext(ld, 8, "Board");
    expectLeakDetectorPhysicalContext(ld, 9, "CPU");
    expectLeakDetectorMoistureType(ld, 8);
    expectLeakDetectorMoistureType(ld, 9);
}

TEST_F(NsmLeakDetectionBranch6Test,
       LeakDetectionCtor_PhysicalContextMapSizeMismatch_ShortAndLong)
{
    {
        // Shorter map than sensor count: first entry applies; missing -> Board.
        auto ld = makeSensor("short_pc", {8, 9}, {"Board"});
        expectLeakDetectorPhysicalContext(ld, 8, "Board");
        expectLeakDetectorPhysicalContext(ld, 9, "Board");
        expectLeakDetectorMoistureType(ld, 8);
        expectLeakDetectorMoistureType(ld, 9);
    }
    {
        // Longer map than sensor count: extras ignored; only index 0 used.
        auto ld = makeSensor("long_pc", {8}, {"Board", "CPU", "Fan"});
        expectLeakDetectorPhysicalContext(ld, 8, "Board");
        expectLeakDetectorMoistureType(ld, 8);
    }
}

TEST_F(NsmLeakDetectionBranch6Test, LeakDetectionCtor_NameAndType)
{
    auto ld = makeSensor("ctorname", {1});

    EXPECT_EQ(ld->getName(), "LeakB6_ctorname");
    EXPECT_EQ(ld->getType(), "NSM_LeakDetection");
}
