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
 * Batch 12A - Tests targeting uncovered template instantiations in
 * nsmd/nsmDevice.hpp (44 uncovered out of 216 functions).
 *
 * The constrained addSensor<SensorType> template (line 301-359) has three
 * branches that require types deriving from both NsmSensor and
 * NsmInterfaces<IntfType>:
 *   1. NsmGroupSensor path: existing sensor found + SensorType is
 * NsmGroupSensor
 *      -> groups subsensors
 *   2. moveInterfaces path: existing sensor found + non-group sensor
 *      -> merges PDI interfaces
 *   3. New sensor path: no match found -> delegates to addSensorBase
 *
 * Also covers:
 *   - addStaticSensor<T> with interface-bearing types
 *   - addSensor<T>(sensor, bool, bool) deprecated overload with typed sensors
 *   - addSensor<T>(const shared_ptr<T>&, PollingType) const-ref overload
 *   - Various PollingType branches through the constrained template
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "commonMock.hpp"
#include "nsmDevice.hpp"
#include "nsmGroupSensor.hpp"
#include "nsmInterface.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Test D-Bus interface type
// ============================================================================

// A minimal sdbusplus-compatible interface for testing. We need a type with
// a static constexpr `interface` member so NsmInterfaces<T>::interface() works.
// Using sdbusplus::server::object_t would require full D-Bus wiring, so we
// define a lightweight stub.

struct TestIntfType
{
    static constexpr auto interface = "xyz.openbmc_project.Inventory.Item.Test";

    // Minimal constructor for container usage
    explicit TestIntfType() = default;
};

// A second interface type to test different template instantiations
struct TestIntfType2
{
    static constexpr auto interface =
        "xyz.openbmc_project.Inventory.Item.Test2";

    explicit TestIntfType2() = default;
};

// A third interface type for group sensor testing
struct TestGroupIntfType
{
    static constexpr auto interface =
        "xyz.openbmc_project.Inventory.Item.TestGroup";

    explicit TestGroupIntfType() = default;
};

// ============================================================================
// Mock sensor that satisfies the requires clause:
//   std::is_base_of_v<NsmSensor, SensorType> &&
//   std::is_base_of_v<NsmInterfaces<typename SensorType::IntfType_t>,
//   SensorType>
// ============================================================================

class MockIntfSensor :
    public NsmSensor,
    public NsmInterfaceContainer<TestIntfType>
{
  public:
    MockIntfSensor(const std::string& name, const std::string& type,
                   const std::string& objPath) :
        NsmSensor(name, type),
        NsmInterfaceContainer<TestIntfType>(Interfaces<TestIntfType>{
            {objPath, std::make_shared<TestIntfType>()}})
    {
        refreshLimitInUsec = 0;
        setLastUpdatedTimeStamp(0);
    }

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        // Return a fixed request so operator== matches for sensors with
        // same name/type
        return std::vector<uint8_t>{0xAA, 0xBB};
    }

    uint8_t handleResponseMsg(const nsm_msg* /*responseMsg*/,
                              size_t /*responseLen*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

// A second typed sensor using TestIntfType2 for different template
// instantiation
class MockIntfSensor2 :
    public NsmSensor,
    public NsmInterfaceContainer<TestIntfType2>
{
  public:
    MockIntfSensor2(const std::string& name, const std::string& type,
                    const std::string& objPath) :
        NsmSensor(name, type),
        NsmInterfaceContainer<TestIntfType2>(Interfaces<TestIntfType2>{
            {objPath, std::make_shared<TestIntfType2>()}})
    {
        refreshLimitInUsec = 0;
        setLastUpdatedTimeStamp(0);
    }

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        return std::vector<uint8_t>{0xCC, 0xDD};
    }

    uint8_t handleResponseMsg(const nsm_msg* /*responseMsg*/,
                              size_t /*responseLen*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

// A group sensor type that satisfies the requires clause AND is
// derived from NsmGroupSensor (triggers the grouping branch)
class MockGroupIntfSensor :
    public NsmGroupSensor,
    public NsmInterfaceContainer<TestGroupIntfType>
{
  public:
    MockGroupIntfSensor(const std::string& name, const std::string& type,
                        const std::string& objPath) :
        NsmGroupSensor(name, type),
        NsmInterfaceContainer<TestGroupIntfType>(Interfaces<TestGroupIntfType>{
            {objPath, std::make_shared<TestGroupIntfType>()}})
    {
        refreshLimitInUsec = 0;
        setLastUpdatedTimeStamp(0);
    }

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        // Fixed request so == matches for grouping
        return std::vector<uint8_t>{0xEE, 0xFF};
    }

    uint8_t handleResponse(const nsm_msg* /*responseMsg*/,
                           size_t /*responseLen*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

// A sensor with different request bytes (won't match existing sensor)
class MockIntfSensorUnique :
    public NsmSensor,
    public NsmInterfaceContainer<TestIntfType>
{
  public:
    MockIntfSensorUnique(const std::string& name, const std::string& type,
                         const std::string& objPath,
                         std::vector<uint8_t> reqData) :
        NsmSensor(name, type),
        NsmInterfaceContainer<TestIntfType>(Interfaces<TestIntfType>{
            {objPath, std::make_shared<TestIntfType>()}}),
        requestData(std::move(reqData))
    {
        refreshLimitInUsec = 0;
        setLastUpdatedTimeStamp(0);
    }

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        return requestData;
    }

    uint8_t handleResponseMsg(const nsm_msg* /*responseMsg*/,
                              size_t /*responseLen*/) override
    {
        return NSM_SW_SUCCESS;
    }

  private:
    std::vector<uint8_t> requestData;
};

// ============================================================================
// PART 1: Constrained addSensor<T> -- New sensor path (no match found)
// ============================================================================

TEST(NsmDeviceTemplate, AddSensor_Constrained_NewSensor_AddsToDeviceSensors)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initialCount = nsmDevice.deviceSensors.size();

    auto sensor = std::make_shared<MockIntfSensor>("TestSensor1", "TestType1",
                                                   "/test/path1");

    // Act: addSensor with constrained template (sensor is NsmSensor +
    // NsmInterfaces)
    nsmDevice.addSensor(sensor, PollingType::RoundRobin);

    // Assert: sensor should be added as new
    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialCount + 1);
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceTemplate, AddSensor_Constrained_NewSensor_Priority_SetsRefresh)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockIntfSensor>("PriSensor", "PriType",
                                                   "/test/pri_path");

    // Act
    nsmDevice.addSensor(sensor, PollingType::Priority);

    // Assert
    EXPECT_EQ(nsmDevice.prioritySensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, PRIORITY_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceTemplate, AddSensor_Constrained_NewSensor_Gpm_SetsRefresh)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockIntfSensor>("GpmSensor", "GpmType",
                                                   "/test/gpm_path");

    // Act
    nsmDevice.addSensor(sensor, PollingType::GpuPerformanceMonitoring);

    // Assert
    EXPECT_EQ(nsmDevice.gpmSensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, GPM_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceTemplate, AddSensor_Constrained_NewSensor_LongRunning_SetsRefresh)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockIntfSensor>("LRSensor", "LRType",
                                                   "/test/lr_path");

    // Act
    nsmDevice.addSensor(sensor, PollingType::LongRunning);

    // Assert
    EXPECT_EQ(nsmDevice.longRunningSensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceTemplate, AddSensor_Constrained_NewSensor_Static_SetsRefresh)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initialStaticSize = nsmDevice.staticSensors.size();

    auto sensor = std::make_shared<MockIntfSensor>("StaticSensor", "StaticType",
                                                   "/test/static_path");

    // Act
    nsmDevice.addSensor(sensor, PollingType::Static);

    // Assert
    EXPECT_EQ(nsmDevice.staticSensors.size(), initialStaticSize + 1);
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

// ============================================================================
// PART 2: Constrained addSensor<T> -- moveInterfaces path (duplicate sensor)
// ============================================================================

TEST(NsmDeviceTemplate, AddSensor_Constrained_DuplicateSensor_MergesInterfaces)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // First sensor with path /test/pathA
    auto sensor1 = std::make_shared<MockIntfSensor>("Sensor1", "Type1",
                                                    "/test/pathA");
    nsmDevice.addSensor(sensor1, PollingType::RoundRobin);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    // Second sensor with SAME request bytes (will match sensor1) but
    // different object path /test/pathB
    auto sensor2 = std::make_shared<MockIntfSensor>("Sensor2", "Type2",
                                                    "/test/pathB");

    // Act: addSensor should find sensor1 as a match and merge interfaces
    nsmDevice.addSensor(sensor2, PollingType::RoundRobin);

    // Assert: deviceSensors should NOT have grown (sensor2 merged into
    // sensor1)
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);

    // sensor2 should now point to sensor1 (the existing sensor)
    EXPECT_EQ(sensor2, sensor1);

    // sensor1 should now have 2 interfaces (pathA + pathB)
    EXPECT_EQ(sensor1->size(), 2u);
}

TEST(NsmDeviceTemplate,
     AddSensor_Constrained_DuplicateSensor_SamePath_NoNewInterface)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockIntfSensor>("Sensor1", "Type1",
                                                    "/test/same_path");
    nsmDevice.addSensor(sensor1, PollingType::RoundRobin);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    // Second sensor with same request bytes AND same path
    auto sensor2 = std::make_shared<MockIntfSensor>("Sensor2", "Type2",
                                                    "/test/same_path");

    // Act
    nsmDevice.addSensor(sensor2, PollingType::RoundRobin);

    // Assert: no growth in deviceSensors
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);

    // sensor2 now points to sensor1
    EXPECT_EQ(sensor2, sensor1);

    // Same path -> moveInterfaces returns false, but interface count stays 1
    EXPECT_EQ(sensor1->size(), 1u);
}

TEST(NsmDeviceTemplate, AddSensor_Constrained_DuplicateMultiplePaths_MergesAll)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockIntfSensor>("Sensor1", "Type1",
                                                    "/test/path1");
    nsmDevice.addSensor(sensor1, PollingType::Priority);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    // Second merge
    auto sensor2 = std::make_shared<MockIntfSensor>("Sensor2", "Type2",
                                                    "/test/path2");
    nsmDevice.addSensor(sensor2, PollingType::Priority);
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);
    EXPECT_EQ(sensor1->size(), 2u);

    // Third merge
    auto sensor3 = std::make_shared<MockIntfSensor>("Sensor3", "Type3",
                                                    "/test/path3");
    nsmDevice.addSensor(sensor3, PollingType::Priority);
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);
    EXPECT_EQ(sensor1->size(), 3u);

    // All should point to the same sensor
    EXPECT_EQ(sensor2, sensor1);
    EXPECT_EQ(sensor3, sensor1);
}

// ============================================================================
// PART 3: Constrained addSensor<T> -- NsmGroupSensor path (grouping)
// ============================================================================

TEST(NsmDeviceTemplate,
     AddSensor_Constrained_GroupSensor_DuplicateGroupsSensors)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto groupSensor1 = std::make_shared<MockGroupIntfSensor>(
        "Group1", "GroupType", "/test/group_pathA");
    nsmDevice.addSensor(groupSensor1, PollingType::RoundRobin);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    // Second group sensor with same request bytes -> triggers grouping
    auto groupSensor2 = std::make_shared<MockGroupIntfSensor>(
        "Group2", "GroupType", "/test/group_pathB");

    // Act
    nsmDevice.addSensor(groupSensor2, PollingType::RoundRobin);

    // Assert: deviceSensors should NOT have grown
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);

    // groupSensor2 should now be redirected to groupSensor1
    EXPECT_EQ(groupSensor2, groupSensor1);

    // groupSensor1 should have groupSensor2 as a subsensor
    EXPECT_EQ(groupSensor1->sensors.size(), 1u);
}

TEST(NsmDeviceTemplate,
     AddSensor_Constrained_GroupSensor_MultipleGroupings_AccumulatesSubsensors)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto groupSensor1 = std::make_shared<MockGroupIntfSensor>(
        "Group1", "GroupType", "/test/grp1");
    nsmDevice.addSensor(groupSensor1, PollingType::Priority);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    // Add 3 more group sensors that match
    auto groupSensor2 = std::make_shared<MockGroupIntfSensor>(
        "Group2", "GroupType", "/test/grp2");
    auto groupSensor3 = std::make_shared<MockGroupIntfSensor>(
        "Group3", "GroupType", "/test/grp3");
    auto groupSensor4 = std::make_shared<MockGroupIntfSensor>(
        "Group4", "GroupType", "/test/grp4");

    nsmDevice.addSensor(groupSensor2, PollingType::Priority);
    nsmDevice.addSensor(groupSensor3, PollingType::Priority);
    nsmDevice.addSensor(groupSensor4, PollingType::Priority);

    // Assert
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);
    EXPECT_EQ(groupSensor1->sensors.size(), 3u);
    EXPECT_EQ(groupSensor2, groupSensor1);
    EXPECT_EQ(groupSensor3, groupSensor1);
    EXPECT_EQ(groupSensor4, groupSensor1);
}

// ============================================================================
// PART 4: Different template type parameters (type 2 sensor)
// ============================================================================

TEST(NsmDeviceTemplate,
     AddSensor_Constrained_Type2Sensor_NewSensor_AddsCorrectly)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initialCount = nsmDevice.deviceSensors.size();

    auto sensor = std::make_shared<MockIntfSensor2>("Type2Sensor", "Type2",
                                                    "/test/type2_path");

    // Act: exercises a different template instantiation (IntfType2 vs IntfType)
    nsmDevice.addSensor(sensor, PollingType::RoundRobin);

    // Assert
    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialCount + 1);
}

TEST(NsmDeviceTemplate,
     AddSensor_Constrained_Type2Sensor_Duplicate_MergesInterfaces)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockIntfSensor2>("S1", "T",
                                                     "/test/t2_pathA");
    nsmDevice.addSensor(sensor1, PollingType::Static);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    auto sensor2 = std::make_shared<MockIntfSensor2>("S2", "T",
                                                     "/test/t2_pathB");

    // Act
    nsmDevice.addSensor(sensor2, PollingType::Static);

    // Assert
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);
    EXPECT_EQ(sensor2, sensor1);
    EXPECT_EQ(sensor1->size(), 2u);
}

// ============================================================================
// PART 5: addStaticSensor<T> with interface-bearing sensor types
// ============================================================================

TEST(NsmDeviceTemplate, AddStaticSensor_IntfSensor_AddsToStaticQueue)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initialStaticSize = nsmDevice.staticSensors.size();

    auto sensor = std::make_shared<MockIntfSensor>(
        "StaticIntfSensor", "StaticIntfType", "/test/static_intf");

    // Act: addStaticSensor delegates to addSensor(sensor, PollingType::Static)
    nsmDevice.addStaticSensor(sensor);

    // Assert
    EXPECT_EQ(nsmDevice.staticSensors.size(), initialStaticSize + 1);
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceTemplate, AddStaticSensor_IntfSensor_Duplicate_MergesInterfaces)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockIntfSensor>("StaticS1", "StaticT",
                                                    "/test/static_A");
    nsmDevice.addStaticSensor(sensor1);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    auto sensor2 = std::make_shared<MockIntfSensor>("StaticS2", "StaticT",
                                                    "/test/static_B");

    // Act
    nsmDevice.addStaticSensor(sensor2);

    // Assert: merged — sensor count should not grow
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);
}

TEST(NsmDeviceTemplate, AddStaticSensor_GroupSensor_Duplicate_GroupsSubsensors)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto groupSensor1 = std::make_shared<MockGroupIntfSensor>(
        "StaticGrp1", "StaticGrpType", "/test/static_grp1");
    nsmDevice.addStaticSensor(groupSensor1);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    auto groupSensor2 = std::make_shared<MockGroupIntfSensor>(
        "StaticGrp2", "StaticGrpType", "/test/static_grp2");

    // Act
    nsmDevice.addStaticSensor(groupSensor2);

    // Assert: merged — sensor count should not grow
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);
}

// ============================================================================
// PART 6: Deprecated addSensor<T>(sensor, bool, bool) with typed sensors
// ============================================================================

TEST(NsmDeviceTemplate,
     AddSensor_DeprecatedBool_PriorityTrue_IntfSensor_AddsToPriority)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockIntfSensorUnique>(
        "DepPriSensor", "DepPriType", "/test/dep_pri",
        std::vector<uint8_t>{0x01, 0x02});

    // Act: priority=true, isLongRunning=false -> PollingType::Priority
    nsmDevice.addSensor(sensor, true, false);

    // Assert
    EXPECT_EQ(nsmDevice.prioritySensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, PRIORITY_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceTemplate,
     AddSensor_DeprecatedBool_LongRunningTrue_IntfSensor_AddsToLR)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockIntfSensorUnique>(
        "DepLRSensor", "DepLRType", "/test/dep_lr",
        std::vector<uint8_t>{0x03, 0x04});

    // Act: priority=false, isLongRunning=true -> PollingType::LongRunning
    nsmDevice.addSensor(sensor, false, true);

    // Assert
    EXPECT_EQ(nsmDevice.longRunningSensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceTemplate,
     AddSensor_DeprecatedBool_BothFalse_IntfSensor_AddsToRoundRobin)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockIntfSensorUnique>(
        "DepRRSensor", "DepRRType", "/test/dep_rr",
        std::vector<uint8_t>{0x05, 0x06});

    // Act: priority=false, isLongRunning=false -> PollingType::RoundRobin
    nsmDevice.addSensor(sensor, false, false);

    // Assert
    EXPECT_GE(nsmDevice.roundRobinSensors.size(), 2u);
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceTemplate,
     AddSensor_DeprecatedBool_BothTrue_IntfSensor_LongRunningTakesPrecedence)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockIntfSensorUnique>(
        "DepDualSensor", "DepDualType", "/test/dep_dual",
        std::vector<uint8_t>{0x07, 0x08});

    // Act: priority=true, isLongRunning=true -> LongRunning takes precedence
    nsmDevice.addSensor(sensor, true, true);

    // Assert
    EXPECT_EQ(nsmDevice.longRunningSensors.size(), 1u);
    EXPECT_EQ(nsmDevice.prioritySensors.size(), 0u);
    EXPECT_EQ(sensor->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
}

// ============================================================================
// PART 7: const-ref addSensor<T>(const shared_ptr<T>&, PollingType) overload
// ============================================================================

TEST(NsmDeviceTemplate,
     AddSensor_ConstRef_NewSensor_AddsWithoutModifyingOriginal)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initialCount = nsmDevice.deviceSensors.size();

    // Create a const shared_ptr (simulating a temporary or const context)
    const auto sensor = std::make_shared<MockIntfSensorUnique>(
        "ConstRefSensor", "ConstRefType", "/test/const_ref",
        std::vector<uint8_t>{0x10, 0x20});

    // Act: calls addSensor(const shared_ptr<T>&, PollingType) which copies
    // the shared_ptr then delegates to the non-const overload
    nsmDevice.addSensor(sensor, PollingType::RoundRobin);

    // Assert
    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialCount + 1);
}

TEST(NsmDeviceTemplate,
     AddSensor_ConstRef_DuplicateSensor_MergesButOriginalUnchanged)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockIntfSensor>("ConstS1", "ConstT",
                                                    "/test/const_pathA");
    nsmDevice.addSensor(sensor1, PollingType::Static);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    // Create a second sensor with same request bytes, pass as const ref
    const auto sensor2 = std::make_shared<MockIntfSensor>("ConstS2", "ConstT",
                                                          "/test/const_pathB");

    // Save the original pointer value
    auto originalSensor2Ptr = sensor2.get();

    // Act: const-ref overload makes a copy, so sensor2 stays unchanged
    nsmDevice.addSensor(sensor2, PollingType::Static);

    // Assert: deviceSensors did not grow (merge happened)
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);

    // sensor2 is const, so it was NOT updated to point to sensor1.
    // The const-ref overload works on a copy.
    EXPECT_EQ(sensor2.get(), originalSensor2Ptr);
}

// ============================================================================
// PART 8: Mixed types in same device - no cross-type matching
// ============================================================================

TEST(NsmDeviceTemplate, AddSensor_DifferentTypes_NoMatch_EachAddedSeparately)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initialCount = nsmDevice.deviceSensors.size();

    // MockIntfSensor and MockIntfSensor2 have different request bytes,
    // so they should never match even though both are in the same device.
    auto sensor1 = std::make_shared<MockIntfSensor>("TypeASensor", "TypeA",
                                                    "/test/typeA");
    auto sensor2 = std::make_shared<MockIntfSensor2>("TypeBSensor", "TypeB",
                                                     "/test/typeB");

    // Act
    nsmDevice.addSensor(sensor1, PollingType::RoundRobin);
    nsmDevice.addSensor(sensor2, PollingType::RoundRobin);

    // Assert: both added separately
    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialCount + 2);
}

TEST(NsmDeviceTemplate, AddSensor_MixedGroupAndNonGroup_NoMatch_BothAdded)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initialCount = nsmDevice.deviceSensors.size();

    // Group sensor and non-group sensor have different types, so no match
    auto groupSensor = std::make_shared<MockGroupIntfSensor>(
        "GrpSensor", "GrpType", "/test/grp");
    auto plainSensor = std::make_shared<MockIntfSensor>(
        "PlainSensor", "PlainType", "/test/plain");

    // Act
    nsmDevice.addSensor(groupSensor, PollingType::RoundRobin);
    nsmDevice.addSensor(plainSensor, PollingType::RoundRobin);

    // Assert
    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialCount + 2);
}

// ============================================================================
// PART 9: Unique request data prevents matching (no merge)
// ============================================================================

TEST(NsmDeviceTemplate,
     AddSensor_Constrained_DifferentRequestData_NoMerge_BothAdded)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initialCount = nsmDevice.deviceSensors.size();

    auto sensor1 = std::make_shared<MockIntfSensorUnique>(
        "UniqueS1", "UniqueT", "/test/unique1", std::vector<uint8_t>{0x01});
    auto sensor2 = std::make_shared<MockIntfSensorUnique>(
        "UniqueS2", "UniqueT", "/test/unique2", std::vector<uint8_t>{0x02});

    // Act: different request data means != comparison, no merge
    nsmDevice.addSensor(sensor1, PollingType::Priority);
    nsmDevice.addSensor(sensor2, PollingType::Priority);

    // Assert: both added as separate sensors
    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialCount + 2);
    EXPECT_NE(sensor1, sensor2);
    EXPECT_EQ(nsmDevice.prioritySensors.size(), 2u);
}

// ============================================================================
// PART 10: addSensor<T> with const-ref for group sensor type
// ============================================================================

TEST(NsmDeviceTemplate, AddSensor_ConstRef_GroupSensor_NewSensor_AddsCorrectly)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initialCount = nsmDevice.deviceSensors.size();

    const auto groupSensor = std::make_shared<MockGroupIntfSensor>(
        "ConstGrp", "ConstGrpType", "/test/const_grp");

    // Act: const-ref overload with group sensor
    nsmDevice.addSensor(groupSensor, PollingType::LongRunning);

    // Assert
    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialCount + 1);
    EXPECT_EQ(nsmDevice.longRunningSensors.size(), 1u);
}

// ============================================================================
// PART 11: addStaticSensor with Type2 sensor (different template param)
// ============================================================================

TEST(NsmDeviceTemplate, AddStaticSensor_Type2Sensor_AddsToStaticQueue)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initialStaticSize = nsmDevice.staticSensors.size();

    auto sensor = std::make_shared<MockIntfSensor2>("StaticType2", "StaticT2",
                                                    "/test/static_t2");

    // Act: addStaticSensor exercises a different template instantiation
    nsmDevice.addStaticSensor(sensor);

    // Assert
    EXPECT_EQ(nsmDevice.staticSensors.size(), initialStaticSize + 1);
}

// ============================================================================
// PART 12: Verify interface() static method from NsmInterfaces is accessible
// ============================================================================

TEST(NsmDeviceTemplate, AddSensor_Constrained_InterfaceNameIsCorrect_AfterAdd)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockIntfSensor>("IntfNameS1", "IntfNameT",
                                                    "/test/intf_name_1");
    auto sensor2 = std::make_shared<MockIntfSensor2>("IntfNameS2", "IntfNameT2",
                                                     "/test/intf_name_2");

    // Act
    nsmDevice.addSensor(sensor1, PollingType::RoundRobin);
    nsmDevice.addSensor(sensor2, PollingType::RoundRobin);

    // Assert: verify the static interface() method returns correct value
    EXPECT_EQ(MockIntfSensor::interface(),
              "xyz.openbmc_project.Inventory.Item.Test");
    EXPECT_EQ(MockIntfSensor2::interface(),
              "xyz.openbmc_project.Inventory.Item.Test2");
}

// ============================================================================
// PART 13: Edge case - adding sensor to device with pre-existing sensors
// ============================================================================

TEST(NsmDeviceTemplate,
     AddSensor_Constrained_DeviceWithManySensors_FindCorrectMatch)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Add several unique sensors first to populate deviceSensors
    for (int i = 0; i < 5; i++)
    {
        auto unique = std::make_shared<MockIntfSensorUnique>(
            "Filler" + std::to_string(i), "FillerType",
            "/test/filler" + std::to_string(i),
            std::vector<uint8_t>{static_cast<uint8_t>(0x50 + i)});
        nsmDevice.addSensor(unique, PollingType::RoundRobin);
    }

    size_t countBefore = nsmDevice.deviceSensors.size();

    // Add a sensor with specific request data
    auto target = std::make_shared<MockIntfSensorUnique>(
        "Target", "TargetType", "/test/targetA", std::vector<uint8_t>{0x99});
    nsmDevice.addSensor(target, PollingType::RoundRobin);

    size_t countAfterTarget = nsmDevice.deviceSensors.size();
    EXPECT_EQ(countAfterTarget, countBefore + 1);

    // Now add a duplicate that should match the target
    auto duplicate = std::make_shared<MockIntfSensorUnique>(
        "TargetDup", "TargetType", "/test/targetB", std::vector<uint8_t>{0x99});
    nsmDevice.addSensor(duplicate, PollingType::RoundRobin);

    // Assert: finds the correct match among many sensors
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterTarget);
    EXPECT_EQ(duplicate, target);
    EXPECT_EQ(target->size(), 2u);
}

// ============================================================================
// PART 14: Group sensor with pre-existing subsensors
// ============================================================================

TEST(NsmDeviceTemplate,
     AddSensor_Constrained_GroupSensor_PreExistingSubsensors_Accumulates)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto grp1 = std::make_shared<MockGroupIntfSensor>("GrpAcc1", "GrpAccType",
                                                      "/test/grp_acc1");

    // Manually add a subsensor to the first group sensor
    auto mockSubSensor = std::make_shared<MockGroupIntfSensor>(
        "PreExisting", "PreExistingType", "/test/pre_existing");
    grp1->sensors.emplace_back(mockSubSensor);
    EXPECT_EQ(grp1->sensors.size(), 1u);

    nsmDevice.addSensor(grp1, PollingType::Static);

    // Add another matching group sensor
    auto grp2 = std::make_shared<MockGroupIntfSensor>("GrpAcc2", "GrpAccType",
                                                      "/test/grp_acc2");

    // Act
    nsmDevice.addSensor(grp2, PollingType::Static);

    // Assert: grp1 now has 2 subsensors (pre-existing + grp2)
    EXPECT_EQ(grp1->sensors.size(), 2u);
    EXPECT_EQ(grp2, grp1);
}

// ============================================================================
// PART 15: addSensor with GPM polling type through constrained template
// ============================================================================

TEST(NsmDeviceTemplate, AddSensor_Constrained_GpmPolling_DuplicateMerges)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockIntfSensor>("GpmS1", "GpmT",
                                                    "/test/gpm1");
    nsmDevice.addSensor(sensor1, PollingType::GpuPerformanceMonitoring);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    auto sensor2 = std::make_shared<MockIntfSensor>("GpmS2", "GpmT",
                                                    "/test/gpm2");

    // Act: duplicate should be merged even with GPM polling type
    nsmDevice.addSensor(sensor2, PollingType::GpuPerformanceMonitoring);

    // Assert
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);
    EXPECT_EQ(sensor2, sensor1);
    EXPECT_EQ(sensor1->size(), 2u);
}

// ============================================================================
// PART 16: addSensor with LongRunning polling through constrained template
// ============================================================================

TEST(NsmDeviceTemplate,
     AddSensor_Constrained_LongRunningPolling_DuplicateMerges)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockIntfSensor2>("LRS1", "LRT",
                                                     "/test/lr1");
    nsmDevice.addSensor(sensor1, PollingType::LongRunning);
    size_t countAfterFirst = nsmDevice.deviceSensors.size();

    auto sensor2 = std::make_shared<MockIntfSensor2>("LRS2", "LRT",
                                                     "/test/lr2");

    // Act
    nsmDevice.addSensor(sensor2, PollingType::LongRunning);

    // Assert
    EXPECT_EQ(nsmDevice.deviceSensors.size(), countAfterFirst);
    EXPECT_EQ(sensor2, sensor1);
    EXPECT_EQ(sensor1->size(), 2u);
}

// ============================================================================
// PART 17: Sensor size() after multiple merges
// ============================================================================

TEST(NsmDeviceTemplate, AddSensor_Constrained_SizeGrowsWithEachMerge)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockIntfSensor>("SizeS1", "SizeT",
                                                    "/test/size1");
    nsmDevice.addSensor(sensor1, PollingType::RoundRobin);
    EXPECT_EQ(sensor1->size(), 1u);

    // Merge 4 more interfaces
    for (int i = 2; i <= 5; i++)
    {
        auto sensorN = std::make_shared<MockIntfSensor>(
            "SizeS" + std::to_string(i), "SizeT",
            "/test/size" + std::to_string(i));
        nsmDevice.addSensor(sensorN, PollingType::RoundRobin);
    }

    // Assert: sensor1 should have 5 interfaces total
    EXPECT_EQ(sensor1->size(), 5u);
}

// ============================================================================
// PART 18: Device identifier set through constrained template path
// ============================================================================

TEST(NsmDeviceTemplate, AddSensor_Constrained_NewSensor_SetsDeviceIdentifier)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(NSM_DEV_ID_GPU, 3, "MCTP_UUID", uuid, 0);

    auto sensor = std::make_shared<MockIntfSensorUnique>(
        "DevIdSensor", "DevIdType", "/test/dev_id", std::vector<uint8_t>{0xAB});

    // Act
    nsmDevice.addSensor(sensor, PollingType::RoundRobin);

    // Assert: deviceIdentifier should be set by addSensorBase
    EXPECT_FALSE(sensor->getDeviceIdentifier().empty());
}
