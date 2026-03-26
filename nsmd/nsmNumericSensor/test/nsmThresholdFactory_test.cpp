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
 * Branch coverage tests for nsmd/nsmNumericSensor/nsmThresholdFactory.cpp
 *
 * Covers:
 * - Constructor
 * - make(): empty serviceMap → no sensors
 * - make(): non-matching interface → no sensors
 * - make(): LowerCaution/UpperCaution, Dynamic=false → static threshold
 * - make(): LowerCritical, Dynamic=false → static critical threshold
 * - make(): LowerFatal/UpperFatal, Dynamic=false → static hard-shutdown
 * threshold
 * - make(): Dynamic=true, unsupported type → error, no sensor
 * - make(): Dynamic=true, NSM_ThermalParameter, no PeriodicUpdate →
 * addStaticSensor
 * - make(): Dynamic=true, NSM_ThermalParameter, periodicUpdate=true →
 * roundRobin
 * - make(): Dynamic=true, NSM_ThermalParameter, periodicUpdate=true, priority →
 * priority queue
 * - make(): Dynamic=true, NSM_ThermalParameter, periodicUpdate=true, aggregated
 * → aggregator
 * - make(): both lower and upper caution → two device sensors
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmNumericSensorFactory.hpp"
#include "nsmThresholdAggregator.hpp"
#include "nsmThresholdFactory.hpp"
#include "nsmThresholdValue.hpp"

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Stub NsmNumericSensor for testing the factory
// ============================================================================

class StubThresholdNumericSensor : public nsm::NsmNumericSensor
{
  public:
    StubThresholdNumericSensor(const std::string& name, uint8_t sensorId) :
        nsm::NsmNumericSensor(
            name, "stub", sensorId,
            std::make_shared<nsm::NsmNumericSensorValueAggregate>())
    {}

    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::nullopt;
    }

    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return NSM_SUCCESS;
    }

    std::string getSensorType() override
    {
        return "temperature";
    }
};

// ============================================================================
// Test fixture
// ============================================================================

struct NsmThresholdFactoryTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t sensorUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string interface = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string baseObjPath =
        "/xyz/openbmc_project/inventory/system/sensor/thresh_test";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nsmDev;

    NsmThresholdFactoryTest() : SensorManagerTest(devices)
    {
        nsmDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(sensorUuid));
        EXPECT_NE(nsmDev, nullptr);
    }

    ~NsmThresholdFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    // Returns a unique object path per test to avoid D-Bus object conflicts.
    std::string objPath(const std::string& suffix) const
    {
        return baseObjPath + suffix;
    }

    std::shared_ptr<StubThresholdNumericSensor> makeSensor() const
    {
        return std::make_shared<StubThresholdNumericSensor>("TestSensor", 0);
    }

    NumericSensorInfo makeInfo(const std::string& suffix = "") const
    {
        NumericSensorInfo info{};
        info.name = "TempSensor" + suffix;
        return info;
    }

    // Sets up the mock serviceMap and property map for a single interface
    // that represents a static (Dynamic=false) threshold.
    void setupStaticThreshold(const std::string& op, const std::string& intf,
                              const std::string& name,
                              double value = 75.0) const
    {
        utils::MockDbusAsync::serviceMap() = {
            {"xyz.openbmc_project.EntityManager", {intf}}};
        auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
        pm["Name"] = std::string(name);
        // Dynamic not set → defaults to false in production code
        pm["Value"] = double(value);
    }

    // Sets up the mock serviceMap and property map for a single interface
    // that represents a dynamic (Dynamic=true) threshold.
    void setupDynamicThreshold(const std::string& op, const std::string& intf,
                               const std::string& name,
                               const std::string& type = "NSM_ThermalParameter",
                               uint64_t sensorId = 1,
                               bool periodicUpdate = false,
                               bool priority = false,
                               bool aggregated = false) const
    {
        utils::MockDbusAsync::serviceMap() = {
            {"xyz.openbmc_project.EntityManager", {intf}}};
        auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
        pm["Name"] = std::string(name);
        pm["Dynamic"] = bool(true);
        pm["Type"] = std::string(type);
        pm["ParameterId"] = uint64_t(sensorId);
        if (periodicUpdate)
        {
            pm["PeriodicUpdate"] = bool(true);
        }
        pm["Priority"] = bool(priority);
        pm["Aggregated"] = bool(aggregated);
    }
};

// ============================================================================
// Constructor test
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Constructor_Succeeds)
{
    auto sensor = makeSensor();
    auto info = makeInfo();
    EXPECT_NO_THROW(NsmThresholdFactory(
        mockManager, interface, objPath("_ctor"), sensor, info, sensorUuid));
}

// ============================================================================
// make() – empty serviceMap → no threshold interfaces found → no sensors
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_EmptyServiceMap_NoSensorsAdded)
{
    const std::string op = objPath("_esm");
    auto sensor = makeSensor();
    auto info = makeInfo("_esm");
    // serviceMap is empty (default from DBusTest::~DBusTest cleanup)
    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t beforeDev = nsmDev->deviceSensors.size();
    size_t beforeCap = nsmDev->capabilityRefreshSensors.size();
    size_t beforeStatic = nsmDev->staticSensors.size();
    size_t beforeRr = nsmDev->roundRobinSensors.size();
    factory.make();

    EXPECT_EQ(nsmDev->deviceSensors.size(), beforeDev);
    EXPECT_EQ(nsmDev->capabilityRefreshSensors.size(), beforeCap);
    EXPECT_EQ(nsmDev->staticSensors.size(), beforeStatic);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), beforeRr);
}

// ============================================================================
// make() – serviceMap has interface that doesn't contain ThermalParameters →
// no threshold interfaces found → no sensors
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_NonMatchingInterface_NoSensorsAdded)
{
    const std::string op = objPath("_nmi");
    auto sensor = makeSensor();
    auto info = makeInfo("_nmi");
    // Interface name does not contain interface + ".ThermalParameters"
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager",
         {"xyz.openbmc_project.SomeOtherInterface"}}};

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t beforeDev = nsmDev->deviceSensors.size();
    size_t beforeStatic = nsmDev->staticSensors.size();
    factory.make();

    EXPECT_EQ(nsmDev->deviceSensors.size(), beforeDev);
    EXPECT_EQ(nsmDev->staticSensors.size(), beforeStatic);
}

// ============================================================================
// make() – LowerCaution, no Dynamic key → static threshold → deviceSensors
// grows
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_LowerCaution_NoDynamic_DeviceSensorAdded)
{
    const std::string op = objPath("_lcnd");
    auto sensor = makeSensor();
    auto info = makeInfo("_lcnd");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {intf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
    pm["Name"] = std::string("LowerCaution");
    // "Dynamic" intentionally absent → dynamic=false, "Value" absent →
    // threshold=0.0

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->deviceSensors.size(), before);
}

// ============================================================================
// make() – LowerCaution, Dynamic=false, Value set → static with value
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_LowerCaution_StaticWithValue_DeviceSensorAdded)
{
    const std::string op = objPath("_lcwv");
    auto sensor = makeSensor();
    auto info = makeInfo("_lcwv");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";
    setupStaticThreshold(op, intf, "LowerCaution", 85.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->deviceSensors.size(), before);
}

// ============================================================================
// make() – UpperCaution, Dynamic=false → static upper warning threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_UpperCaution_Static_DeviceSensorAdded)
{
    const std::string op = objPath("_uc");
    auto sensor = makeSensor();
    auto info = makeInfo("_uc");
    const std::string intf = interface + ".ThermalParameters.UpperCaution";
    setupStaticThreshold(op, intf, "UpperCaution", 95.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->deviceSensors.size(), before);
}

// ============================================================================
// make() – LowerCritical, Dynamic=false → static critical threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_LowerCritical_Static_DeviceSensorAdded)
{
    const std::string op = objPath("_lcrit");
    auto sensor = makeSensor();
    auto info = makeInfo("_lcrit");
    const std::string intf = interface + ".ThermalParameters.LowerCritical";
    setupStaticThreshold(op, intf, "LowerCritical", 70.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->deviceSensors.size(), before);
}

// ============================================================================
// make() – UpperCritical, Dynamic=false → static critical high threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_UpperCritical_Static_DeviceSensorAdded)
{
    const std::string op = objPath("_ucrit");
    auto sensor = makeSensor();
    auto info = makeInfo("_ucrit");
    const std::string intf = interface + ".ThermalParameters.UpperCritical";
    setupStaticThreshold(op, intf, "UpperCritical", 105.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->deviceSensors.size(), before);
}

// ============================================================================
// make() – LowerFatal, Dynamic=false → static hard-shutdown low threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_LowerFatal_Static_DeviceSensorAdded)
{
    const std::string op = objPath("_lf");
    auto sensor = makeSensor();
    auto info = makeInfo("_lf");
    const std::string intf = interface + ".ThermalParameters.LowerFatal";
    setupStaticThreshold(op, intf, "LowerFatal", 60.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->deviceSensors.size(), before);
}

// ============================================================================
// make() – UpperFatal, Dynamic=false → static hard-shutdown high threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_UpperFatal_Static_DeviceSensorAdded)
{
    const std::string op = objPath("_uf");
    auto sensor = makeSensor();
    auto info = makeInfo("_uf");
    const std::string intf = interface + ".ThermalParameters.UpperFatal";
    setupStaticThreshold(op, intf, "UpperFatal", 120.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->deviceSensors.size(), before);
}

// ============================================================================
// make() – both LowerCaution and UpperCaution → two device sensors added
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_BothWarningThresholds_TwoSensorsAdded)
{
    const std::string op = objPath("_bw");
    auto sensor = makeSensor();
    auto info = makeInfo("_bw");
    const std::string lowerIntf = interface + ".ThermalParameters.LowerCaution";
    const std::string upperIntf = interface + ".ThermalParameters.UpperCaution";

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {lowerIntf, upperIntf}}};

    auto& lpm = utils::MockDbusAsync::propertyMap(op, lowerIntf);
    lpm["Name"] = std::string("LowerCaution");
    lpm["Dynamic"] = bool(false);
    lpm["Value"] = double(75.0);

    auto& upm = utils::MockDbusAsync::propertyMap(op, upperIntf);
    upm["Name"] = std::string("UpperCaution");
    upm["Dynamic"] = bool(false);
    upm["Value"] = double(95.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    // Both lower and upper threshold → two device sensors added
    EXPECT_GE(nsmDev->deviceSensors.size(), before + 2u);
}

// ============================================================================
// make() – Dynamic=true, unsupported type → error path, no sensor added
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_DynamicUnsupportedType_NoSensorAdded)
{
    const std::string op = objPath("_ust");
    auto sensor = makeSensor();
    auto info = makeInfo("_ust");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";
    setupDynamicThreshold(op, intf, "LowerCaution",
                          /*type=*/"UNSUPPORTED_TYPE");

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t beforeDev = nsmDev->deviceSensors.size();
    size_t beforeStatic = nsmDev->staticSensors.size();
    size_t beforeRr = nsmDev->roundRobinSensors.size();
    factory.make();

    EXPECT_EQ(nsmDev->deviceSensors.size(), beforeDev);
    EXPECT_EQ(nsmDev->staticSensors.size(), beforeStatic);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), beforeRr);
}

// ============================================================================
// make() – Dynamic=true, NSM_ThermalParameter, PeriodicUpdate not in map
// (getDbusProperty throws, exception caught) → addStaticSensor called
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_DynamicNSMType_NoPeriodicUpdate_StaticSensorAdded)
{
    const std::string op = objPath("_dns");
    auto sensor = makeSensor();
    auto info = makeInfo("_dns");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";
    // periodicUpdate=false → "PeriodicUpdate" not in map → exception caught →
    // non-periodic path
    setupDynamicThreshold(op, intf, "LowerCaution", "NSM_ThermalParameter",
                          /*sensorId=*/1, /*periodicUpdate=*/false);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t beforeStatic = nsmDev->staticSensors.size();
    size_t beforeCapRefresh = nsmDev->capabilityRefreshSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->staticSensors.size(), beforeStatic);
    EXPECT_GT(nsmDev->capabilityRefreshSensors.size(), beforeCapRefresh);
}

// ============================================================================
// make() – Dynamic=true, NSM_ThermalParameter, periodicUpdate=true,
// not aggregated → roundRobinSensors grows
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_DynamicPeriodicNotAggregated_RoundRobinAdded)
{
    const std::string op = objPath("_dpu");
    auto sensor = makeSensor();
    auto info = makeInfo("_dpu");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";
    setupDynamicThreshold(op, intf, "LowerCaution", "NSM_ThermalParameter",
                          /*sensorId=*/1, /*periodicUpdate=*/true,
                          /*priority=*/false, /*aggregated=*/false);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->roundRobinSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// ============================================================================
// make() – Dynamic=true, NSM_ThermalParameter, periodicUpdate=true,
// priority=true → prioritySensors grows
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_DynamicPeriodicPriority_PrioritySensorAdded)
{
    const std::string op = objPath("_dpp");
    auto sensor = makeSensor();
    auto info = makeInfo("_dpp");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";
    setupDynamicThreshold(op, intf, "LowerCaution", "NSM_ThermalParameter",
                          /*sensorId=*/2, /*periodicUpdate=*/true,
                          /*priority=*/true, /*aggregated=*/false);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->prioritySensors.size();
    factory.make();

    EXPECT_GT(nsmDev->prioritySensors.size(), before);
}

// ============================================================================
// make() – Dynamic=true, NSM_ThermalParameter, periodicUpdate=true,
// aggregated=true → new aggregator created (NsmThresholdAggregatorBuilder path)
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_DynamicPeriodicAggregated_AggregatorCreated)
{
    const std::string op = objPath("_dpa");
    auto sensor = makeSensor();
    auto info = makeInfo("_dpa");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";
    setupDynamicThreshold(op, intf, "LowerCaution", "NSM_ThermalParameter",
                          /*sensorId=*/3, /*periodicUpdate=*/true,
                          /*priority=*/false, /*aggregated=*/true);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->sensorAggregators.size();
    factory.make();

    EXPECT_GT(nsmDev->sensorAggregators.size(), before);
}

// make(): Dynamic=false, "Value" key absent →
// if (count("Value")) = false → threshold defaults to 0.0 → sensor still added.
TEST_F(NsmThresholdFactoryTest, Make_StaticThreshold_MissingValue_UsesDefault)
{
    const std::string op = objPath("_smv");
    auto sensor = makeSensor();
    auto info = makeInfo("_smv");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";

    // Set up property map WITHOUT "Value" key; Dynamic absent → false
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {intf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
    pm["Name"] = std::string("LowerCaution");
    // "Value" intentionally absent → threshold = 0.0 (default)

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    // Threshold sensor still added with default value 0.0
    EXPECT_GT(nsmDev->deviceSensors.size(), before);
}

// make(): Dynamic=true, periodicUpdate=true, "Priority" key absent →
// if (count("Priority")) = false → priority=false → roundRobinSensors grows.
TEST_F(NsmThresholdFactoryTest,
       Make_DynamicPeriodic_MissingPriority_DefaultsToRoundRobin)
{
    const std::string op = objPath("_mpr");
    auto sensor = makeSensor();
    auto info = makeInfo("_mpr");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {intf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
    pm["Name"] = std::string("LowerCaution");
    pm["Dynamic"] = bool(true);
    pm["Type"] = std::string("NSM_ThermalParameter");
    pm["ParameterId"] = uint64_t(10);
    pm["PeriodicUpdate"] = bool(true);
    // "Priority" intentionally absent → defaults to false
    pm["Aggregated"] = bool(false);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->roundRobinSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// make(): Dynamic=true, periodicUpdate=true, "Aggregated" key absent →
// if (count("Aggregated")) = false → aggregated=false → roundRobinSensors
// grows.
TEST_F(NsmThresholdFactoryTest,
       Make_DynamicPeriodic_MissingAggregated_DefaultsToRoundRobin)
{
    const std::string op = objPath("_mag");
    auto sensor = makeSensor();
    auto info = makeInfo("_mag");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {intf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
    pm["Name"] = std::string("LowerCaution");
    pm["Dynamic"] = bool(true);
    pm["Type"] = std::string("NSM_ThermalParameter");
    pm["ParameterId"] = uint64_t(11);
    pm["PeriodicUpdate"] = bool(true);
    pm["Priority"] = bool(false);
    // "Aggregated" intentionally absent → defaults to false

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->roundRobinSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}
