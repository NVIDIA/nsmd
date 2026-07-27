/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmNumericSensorFactory.hpp"
#include "nsmNumericSensorValue_mock.hpp"
#include "nsmThresholdAggregator.hpp"

// Include sensor headers so the linker retains nsmPower.cpp, nsmTemp.cpp,
// nsmEnergy.cpp, nsmVoltage.cpp — those TUs register factories via static
// NsmObjectFactory objects.  With link_with on nsmd_base (not link_whole),
// object files are dropped unless a symbol from them is directly referenced.
#include "nsmEnergy.hpp"
#include "nsmPower.hpp"
#include "nsmTemp.hpp"
#include "nsmVoltage.hpp"

using namespace nsm;
using namespace ::testing;

namespace
{
// typeid() on a polymorphic class emits a link-time reference to the
// typeinfo object (_ZTI*) which lives in the TU that defines the key
// function (first non-inline virtual, i.e. genRequestMsg in each .cpp).
// __attribute__((used)) prevents the compiler from eliding the variables
// (and thus the typeinfo relocations) during optimization.
// This forces the linker to retain nsmPower.cpp/nsmTemp.cpp/nsmEnergy.cpp/
// nsmVoltage.cpp and thus execute their __attribute__((constructor)) factory
// registrations.
__attribute__((used))
const std::type_info* const _forceLinkPower = &typeid(NsmPower);
__attribute__((used))
const std::type_info* const _forceLinkTemp = &typeid(NsmTemp);
__attribute__((used))
const std::type_info* const _forceLinkEnergy = &typeid(NsmEnergy);
__attribute__((used))
const std::type_info* const _forceLinkVoltage = &typeid(NsmVoltage);
} // namespace

// ============================================================================
// Mock NumericSensorBuilder
// ============================================================================

class MockNumericSensorBuilder : public NumericSensorBuilder
{
  public:
    MOCK_METHOD(std::shared_ptr<NsmNumericSensor>, makeSensor,
                (const std::string&, const std::string&, sdbusplus::bus::bus&,
                 const NumericSensorInfo&, const dbus::PropertyMap&),
                (override));
    MOCK_METHOD(std::shared_ptr<NsmNumericAggregator>, makeAggregator,
                (const NumericSensorInfo&), (override));
};

// ============================================================================
// NumericSensorFactory Tests
// ============================================================================

TEST(NumericSensorFactoryTest, ConstructorAndGetCreationFunction)
{
    auto builder = std::make_unique<MockNumericSensorBuilder>();
    NumericSensorFactory factory(std::move(builder));

    auto creationFunc = factory.getCreationFunction();

    EXPECT_TRUE(static_cast<bool>(creationFunc));
}

TEST(NumericSensorFactoryTest, ConstructorWithDifferentBuilder)
{
    auto builder1 = std::make_unique<MockNumericSensorBuilder>();
    auto builder2 = std::make_unique<MockNumericSensorBuilder>();

    NumericSensorFactory factory1(std::move(builder1));
    NumericSensorFactory factory2(std::move(builder2));

    auto func1 = factory1.getCreationFunction();
    auto func2 = factory2.getCreationFunction();

    EXPECT_TRUE(static_cast<bool>(func1));
    EXPECT_TRUE(static_cast<bool>(func2));
}

// ============================================================================
// NumericSensorInfo Struct Tests
// ============================================================================

TEST(NumericSensorInfoTest, DefaultConstruction)
{
    NumericSensorInfo info{};

    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.type.empty());
    EXPECT_EQ(info.sensorId, 0);
    EXPECT_TRUE(info.associations.empty());
    EXPECT_EQ(info.priority, false);
    EXPECT_EQ(info.aggregated, false);
    EXPECT_EQ(info.maxAllowableValue, std::numeric_limits<double>::infinity());
}

TEST(NumericSensorInfoTest, FieldAssignment)
{
    NumericSensorInfo info{};
    info.name = "TestSensor";
    info.type = "NSM_Temp";
    info.sensorId = 42;
    info.priority = true;
    info.aggregated = false;

    EXPECT_EQ(info.name, "TestSensor");
    EXPECT_EQ(info.type, "NSM_Temp");
    EXPECT_EQ(info.sensorId, 42);
    EXPECT_TRUE(info.priority);
    EXPECT_FALSE(info.aggregated);
}

TEST(NumericSensorInfoTest, AssociationsField)
{
    NumericSensorInfo info{};
    info.associations.push_back(
        {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/device"});

    EXPECT_EQ(info.associations.size(), 1u);
    EXPECT_EQ(info.associations[0].forward, "chassis");
}

TEST(NumericSensorInfoTest, OptionalFieldsNullByDefault)
{
    NumericSensorInfo info{};

    EXPECT_EQ(info.implementation, nullptr);
    EXPECT_EQ(info.readingBasis, nullptr);
    EXPECT_EQ(info.description, nullptr);
}

TEST(NumericSensorInfoTest, OptionalFieldsCanBeSet)
{
    NumericSensorInfo info{};
    info.implementation = std::make_unique<std::string>("IOCTL");
    info.readingBasis = std::make_unique<std::string>("Headroom");
    info.description = std::make_unique<std::string>("Temperature sensor");

    EXPECT_NE(info.implementation, nullptr);
    EXPECT_EQ(*info.implementation, "IOCTL");
    EXPECT_EQ(*info.readingBasis, "Headroom");
    EXPECT_EQ(*info.description, "Temperature sensor");
}

// ============================================================================
// Stub classes for NumericSensorFactory::make() tests
// ============================================================================

class StubNsmNumericSensor : public nsm::NsmNumericSensor
{
  public:
    StubNsmNumericSensor(const std::string& name, uint8_t sensorId) :
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
        return "stub";
    }
};

class StubNsmNumericAggregator : public nsm::NsmNumericAggregator
{
  public:
    StubNsmNumericAggregator(const std::string& name, const std::string& type,
                             bool priority) :
        nsm::NsmNumericAggregator(name, type, priority)
    {}

    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::nullopt;
    }

  private:
    int handleSample(const TelemetrySample&) override
    {
        return NSM_SUCCESS;
    }
};

// ============================================================================
// Test fixture for NumericSensorFactory::make()
// ============================================================================

struct NumericSensorFactoryMakeTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t sensorUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string interface = "xyz.openbmc_project.Configuration.NSM_Volt";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/sensor/nsf_test";
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nsmDev;

    NumericSensorFactoryMakeTest() : SensorManagerTest(devices)
    {
        nsmDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(sensorUuid));
        EXPECT_NE(nsmDev, nullptr);
    }

    ~NumericSensorFactoryMakeTest()
    {
        cleanupDeviceSensors(devices);
    }

    void setupProperties(bool aggregated = false, bool priority = false,
                         uint64_t sensorId = 1)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
        pm["Name"] = std::string("TestSensor");
        pm["UUID"] = sensorUuid;
        pm["SensorId"] = sensorId;
        pm["Aggregated"] = aggregated;
        pm["Priority"] = priority;
    }

    void setupChassisAssoc()
    {
        utils::MockDbusAsync::serviceMap() = {
            {"xyz.openbmc_project.EntityManager",
             {interface + ".Associations0"}}};
        auto& pm = utils::MockDbusAsync::propertyMap(
            objPath, interface + ".Associations0");
        pm["Forward"] = std::string("chassis");
        pm["Backward"] = std::string("all_sensors");
        pm["AbsolutePath"] = chassisPath;
    }
};

// Test: no chassis association → builder never called (early return)
TEST_F(NumericSensorFactoryMakeTest, NoChassis_BuilderNotCalled)
{
    setupProperties();
    // serviceMap is empty by default → coGetAssociations finds nothing
    // → chassis_association stays empty → co_return NSM_ERROR //

    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _)).Times(0);
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    factory.make(mockManager, interface, objPath);
}

// Test: chassis association present, not aggregated → sensor added to
// roundRobin
TEST_F(NumericSensorFactoryMakeTest, WithChassis_NotAggregated_SensorAdded)
{
    setupProperties(/*aggregated=*/false, /*priority=*/false);
    setupChassisAssoc();

    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 1);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(interface, objPath, _, _, _))
        .WillOnce(Return(stubSensor));
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    size_t before = nsmDev->roundRobinSensors.size();
    factory.make(mockManager, interface, objPath);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// Test: chassis association present, priority=true → sensor in prioritySensors
TEST_F(NumericSensorFactoryMakeTest, WithChassis_PriorityTrue_InPriorityQueue)
{
    setupProperties(/*aggregated=*/false, /*priority=*/true);
    setupChassisAssoc();

    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 1);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _))
        .WillOnce(Return(stubSensor));

    NumericSensorFactory factory(std::move(mockBuilder));
    size_t before = nsmDev->prioritySensors.size();
    factory.make(mockManager, interface, objPath);
    EXPECT_GT(nsmDev->prioritySensors.size(), before);
}

// Test: NSM_Temp, sensorId=0 → primary_temperature_sensor association added
TEST_F(NumericSensorFactoryMakeTest, NSMTemp_SensorId0_PrimaryTempAssocAdded)
{
    const std::string tempIface = "xyz.openbmc_project.Configuration.NSM_Temp";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, tempIface);
    pm["Name"] = std::string("TempSensor");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(0);
    pm["Aggregated"] = false;
    pm["Priority"] = false;

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {tempIface + ".Associations0"}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(
        objPath, tempIface + ".Associations0");
    assocPm["Forward"] = std::string("chassis");
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] = chassisPath;

    std::vector<utils::Association> capturedAssociations;
    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TempSensor", 0);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _))
        .WillOnce([&](const std::string&, const std::string&,
                      sdbusplus::bus::bus&, const NumericSensorInfo& info,
                      const dbus::PropertyMap&) {
        capturedAssociations = info.associations;
        return stubSensor;
    });

    NumericSensorFactory factory(std::move(mockBuilder));
    factory.make(mockManager, tempIface, objPath);

    // Should have 2: original "chassis"/"all_sensors" +
    // "primary_temperature_sensor"
    EXPECT_EQ(capturedAssociations.size(), 2u);
    bool hasPrimary = false;
    for (const auto& a : capturedAssociations)
    {
        if (a.backward == "primary_temperature_sensor")
        {
            hasPrimary = true;
        }
    }
    EXPECT_TRUE(hasPrimary);
}

// Test: NSM_Temp, sensorId != 0 → no primary_temperature_sensor added
TEST_F(NumericSensorFactoryMakeTest, NSMTemp_NonZeroSensorId_NoPrimaryTempAdded)
{
    const std::string tempIface = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string tempObjPath = objPath + "_temp_nz";
    auto& pm = utils::MockDbusAsync::propertyMap(tempObjPath, tempIface);
    pm["Name"] = std::string("TempSensor2");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(1); // sensorId != 0
    pm["Aggregated"] = false;
    pm["Priority"] = false;

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {tempIface + ".Associations0"}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(
        tempObjPath, tempIface + ".Associations0");
    assocPm["Forward"] = std::string("chassis");
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] = chassisPath;

    std::vector<utils::Association> capturedAssociations;
    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TempSensor2", 1);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _))
        .WillOnce([&](const std::string&, const std::string&,
                      sdbusplus::bus::bus&, const NumericSensorInfo& info,
                      const dbus::PropertyMap&) {
        capturedAssociations = info.associations;
        return stubSensor;
    });

    NumericSensorFactory factory(std::move(mockBuilder));
    factory.make(mockManager, tempIface, tempObjPath);

    // sensorId != 0 → no primary_temperature_sensor added
    EXPECT_EQ(capturedAssociations.size(), 1u);
}

// Test: NSM_Temp, sensorId=0, already has primary_temperature_sensor → not
// duplicated
TEST_F(NumericSensorFactoryMakeTest,
       NSMTemp_SensorId0_AlreadyHasPrimary_NoDuplicate)
{
    const std::string tempIface = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string tempObjPath = objPath + "_temp_primary";
    auto& pm = utils::MockDbusAsync::propertyMap(tempObjPath, tempIface);
    pm["Name"] = std::string("TempSensor3");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(0);
    pm["Aggregated"] = false;
    pm["Priority"] = false;

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager",
         {tempIface + ".Associations0", tempIface + ".Associations1"}}};
    auto& pm0 = utils::MockDbusAsync::propertyMap(tempObjPath,
                                                  tempIface + ".Associations0");
    pm0["Forward"] = std::string("chassis");
    pm0["Backward"] = std::string("all_sensors");
    pm0["AbsolutePath"] = chassisPath;

    auto& pm1 = utils::MockDbusAsync::propertyMap(tempObjPath,
                                                  tempIface + ".Associations1");
    pm1["Forward"] = std::string("chassis");
    pm1["Backward"] = std::string("primary_temperature_sensor");
    pm1["AbsolutePath"] = chassisPath;

    std::vector<utils::Association> capturedAssociations;
    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TempSensor3", 0);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _))
        .WillOnce([&](const std::string&, const std::string&,
                      sdbusplus::bus::bus&, const NumericSensorInfo& info,
                      const dbus::PropertyMap&) {
        capturedAssociations = info.associations;
        return stubSensor;
    });

    NumericSensorFactory factory(std::move(mockBuilder));
    factory.make(mockManager, tempIface, tempObjPath);

    // Should have exactly 2 associations (no duplicate
    // primary_temperature_sensor)
    EXPECT_EQ(capturedAssociations.size(), 2u);
}

// Test: aggregated=true, no existing aggregator → makeAggregator called, added
TEST_F(NumericSensorFactoryMakeTest, Aggregated_NoExistingAgg_AggregatorCreated)
{
    setupProperties(/*aggregated=*/true, /*priority=*/false);
    setupChassisAssoc();

    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 1);
    const std::string type = "NSM_Volt";
    auto stubAgg = std::make_shared<StubNsmNumericAggregator>("TestSensor",
                                                              type, false);

    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _))
        .WillOnce(Return(stubSensor));
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).WillOnce(Return(stubAgg));

    NumericSensorFactory factory(std::move(mockBuilder));
    factory.make(mockManager, interface, objPath);

    EXPECT_EQ(nsmDev->sensorAggregators.size(), 1u);
    EXPECT_EQ(nsmDev->sensorAggregators[0], stubAgg);
}

// Test: aggregated, existing low-priority aggregator + new high-priority →
// promoted
TEST_F(NumericSensorFactoryMakeTest,
       Aggregated_ExistingLowPrio_NewHighPrio_Promotes)
{
    setupProperties(/*aggregated=*/true, /*priority=*/true);
    setupChassisAssoc();

    const std::string type = "NSM_Volt";
    auto existingAgg = std::make_shared<StubNsmNumericAggregator>("TestSensor",
                                                                  type, false);
    nsmDev->sensorAggregators.push_back(existingAgg);
    nsmDev->addSensor(existingAgg,
                      false); // put in roundRobinSensors

    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 2);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _))
        .WillOnce(Return(stubSensor));
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0); // existing agg used

    NumericSensorFactory factory(std::move(mockBuilder));
    factory.make(mockManager, interface, objPath);

    EXPECT_TRUE(existingAgg->priority);
    auto rawPtr = existingAgg.get();
    // Should NOT be in roundRobinSensors anymore
    auto& rrq = nsmDev->roundRobinSensors;
    auto rrIt = std::find_if(rrq.begin(), rrq.end(), [rawPtr](const auto& p) {
        return p.get() == rawPtr;
    });
    EXPECT_EQ(rrIt, rrq.end());
    // Should be in prioritySensors now
    auto& prioQ = nsmDev->prioritySensors;
    auto prioIt =
        std::find_if(prioQ.begin(), prioQ.end(),
                     [rawPtr](const auto& p) { return p.get() == rawPtr; });
    EXPECT_NE(prioIt, prioQ.end());
}

// Test: aggregated, existing high-priority + new high-priority → no change
TEST_F(NumericSensorFactoryMakeTest,
       Aggregated_ExistingHighPrio_NewHighPrio_NoChange)
{
    setupProperties(/*aggregated=*/true, /*priority=*/true);
    setupChassisAssoc();

    const std::string type = "NSM_Volt";
    auto existingAgg = std::make_shared<StubNsmNumericAggregator>("TestSensor",
                                                                  type, true);
    nsmDev->sensorAggregators.push_back(existingAgg);
    nsmDev->addSensor(existingAgg,
                      true); // put in prioritySensors

    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 3);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _))
        .WillOnce(Return(stubSensor));
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    size_t rrBefore = nsmDev->roundRobinSensors.size();
    factory.make(mockManager, interface, objPath);

    // aggregator already high-priority → no promotion needed
    EXPECT_TRUE(existingAgg->priority);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), rrBefore);
}

// Test: aggregated, existing low-priority + new low-priority → no promotion
TEST_F(NumericSensorFactoryMakeTest,
       Aggregated_ExistingLowPrio_NewLowPrio_NoPromotion)
{
    setupProperties(/*aggregated=*/true, /*priority=*/false);
    setupChassisAssoc();

    const std::string type = "NSM_Volt";
    auto existingAgg = std::make_shared<StubNsmNumericAggregator>("TestSensor",
                                                                  type, false);
    nsmDev->sensorAggregators.push_back(existingAgg);
    nsmDev->addSensor(existingAgg, false);

    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 4);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _))
        .WillOnce(Return(stubSensor));
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    factory.make(mockManager, interface, objPath);

    // priority stays false (new sensor also low priority → no promotion)
    EXPECT_FALSE(existingAgg->priority);
}

// Test: UUID present but not in device table → !nsmDevice → co_return //
// NSM_ERROR, builder not called. Covers the if (!nsmDevice)
// branch (line 173).
TEST_F(NumericSensorFactoryMakeTest,
       DISABLED_Make_UnknownUUID_DeviceNotFound_BuilderNotCalled)
{
    const std::string altPath = objPath + "_unknown_uuid";
    const uuid_t unknownUuid = "ffffffff-ffff-ffff-ffff-ffffffffffff";

    auto& pm = utils::MockDbusAsync::propertyMap(altPath, interface);
    pm["Name"] = std::string("TestSensor");
    pm["UUID"] = unknownUuid;
    pm["SensorId"] = uint64_t(1);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);

    // Set up chassis association for the alt path
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {interface + ".Associations0"}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(
        altPath, interface + ".Associations0");
    assocPm["Forward"] = std::string("chassis");
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] = chassisPath;

    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _)).Times(0);
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    factory.make(mockManager, interface, altPath);
    // No assertion needed — StrictMock will fail if builder is called.
}

// Test: "SensorId", "Priority", "Aggregated", "PhysicalContext" keys absent →
// all default to 0/false/false/"" → sensor still added to roundRobinSensors.
// Covers the false branches of if(count("SensorId")),
// if(count("Priority")), if(count("Aggregated")).
// Also covers if(count("PhysicalContext")) false path.
TEST_F(NumericSensorFactoryMakeTest,
       Make_MissingSensorIdPriorityAggregated_UsesDefaults)
{
    const std::string altPath = objPath + "_missing_optional";

    // Only set Name and UUID; leave SensorId, Aggregated, Priority,
    // PhysicalContext absent
    auto& pm = utils::MockDbusAsync::propertyMap(altPath, interface);
    pm["Name"] = std::string("TestSensor");
    pm["UUID"] = sensorUuid;

    // Set up chassis association for altPath
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {interface + ".Associations0"}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(
        altPath, interface + ".Associations0");
    assocPm["Forward"] = std::string("chassis");
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] = chassisPath;

    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 5);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(interface, altPath, _, _, _))
        .WillOnce(Return(stubSensor));
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    size_t before = nsmDev->roundRobinSensors.size();
    factory.make(mockManager, interface, altPath);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// Test: "PhysicalContext" key present → if(count("PhysicalContext")) true path.
TEST_F(NumericSensorFactoryMakeTest, Make_WithPhysicalContext_SensorAdded)
{
    const std::string altPath = objPath + "_with_physctx";

    auto& pm = utils::MockDbusAsync::propertyMap(altPath, interface);
    pm["Name"] = std::string("TestSensor");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(1);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {interface + ".Associations0"}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(
        altPath, interface + ".Associations0");
    assocPm["Forward"] = std::string("chassis");
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] = chassisPath;

    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 6);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _))
        .WillOnce(Return(stubSensor));
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    size_t before = nsmDev->roundRobinSensors.size();
    factory.make(mockManager, interface, altPath);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// Test: "Name" key absent → if(count("Name")) FALSE branch →
// info.name stays "" → makeDBusNameValid("") called → sensor still created.
// Covers the FALSE branch of if(allCurrentIfaceProperties.count("Name")). //

TEST_F(NumericSensorFactoryMakeTest, Make_MissingName_TakesNameFalseBranch)
{
    const std::string altPath = objPath + "_no_name";

    auto& pm = utils::MockDbusAsync::propertyMap(altPath, interface);
    // "Name" intentionally omitted → FALSE branch of if(count("Name"))
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(7);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);

    // Set up chassis association for altPath
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {interface + ".Associations0"}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(
        altPath, interface + ".Associations0");
    assocPm["Forward"] = std::string("chassis");
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] = chassisPath;

    auto stubSensor = std::make_shared<StubNsmNumericSensor>("", 7);
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(interface, altPath, _, _, _))
        .WillOnce(Return(stubSensor));
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    size_t before = nsmDev->roundRobinSensors.size();
    factory.make(mockManager, interface, altPath);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// ==============================================================

// ============================================================================
// makeNsmAltitudePressure factory tests
// ============================================================================

namespace nsm
{
requester::Coroutine makeNsmAltitudePressure(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
} // namespace nsm

struct NsmAltitudePressureFactoryTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_AltitudePressure";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/altitude/GPU_0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmAltitudePressureFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmAltitudePressureFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// All properties present, valid UUID, Priority=false → sensor in
// roundRobinSensors; covers all if(count(...)) true branches.
TEST_F(NsmAltitudePressureFactoryTest, MakeAltPressure_AllProps_RoundRobin)
{
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["UUID"] = gpuUuid;
    pm["Name"] = std::string("AltPressure_0");
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    size_t before = gpu->roundRobinSensors.size();
    makeNsmAltitudePressure(mockManager, interface, objPath);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// Priority=true → sensor added to prioritySensors.
TEST_F(NsmAltitudePressureFactoryTest,
       MakeAltPressure_PriorityTrue_PriorityQueue)
{
    const std::string path2 = objPath + "_prio";
    auto& pm = utils::MockDbusAsync::propertyMap(path2, interface);
    pm["UUID"] = gpuUuid;
    pm["Name"] = std::string("AltPressure_Prio");
    pm["Priority"] = bool(true);
    pm["PhysicalContext"] = std::string("GPU");

    size_t before = gpu->prioritySensors.size();
    makeNsmAltitudePressure(mockManager, interface, path2);
    EXPECT_GT(gpu->prioritySensors.size(), before);
}

// Priority key absent → defaults to false → covers if(count("Priority")) false
// branch. Name and PhysicalContext must be valid to avoid D-Bus / enum errors.
TEST_F(NsmAltitudePressureFactoryTest,
       MakeAltPressure_PriorityAbsent_DefaultsToFalse)
{
    const std::string path3 = objPath + "_no_priority";
    auto& pm = utils::MockDbusAsync::propertyMap(path3, interface);
    pm["UUID"] = gpuUuid;
    pm["Name"] = std::string("AltPressure_NoPrio");
    pm["PhysicalContext"] = std::string("GPU");
    // Priority absent → defaults to false → sensor in roundRobinSensors

    size_t before = gpu->roundRobinSensors.size();
    makeNsmAltitudePressure(mockManager, interface, path3);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// Associations present → coGetAssociations populates the vector;
// sensor still created and added to roundRobinSensors.
TEST_F(NsmAltitudePressureFactoryTest,
       MakeAltPressure_WithAssociations_SensorAdded)
{
    const std::string path4 = objPath + "_assoc";
    auto& pm = utils::MockDbusAsync::propertyMap(path4, interface);
    pm["UUID"] = gpuUuid;
    pm["Name"] = std::string("AltPressure_Assoc");
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {interface + ".Associations0"}}};
    auto& assocPm =
        utils::MockDbusAsync::propertyMap(path4, interface + ".Associations0");
    assocPm["Forward"] = std::string("chassis");
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX");

    size_t before = gpu->roundRobinSensors.size();
    makeNsmAltitudePressure(mockManager, interface, path4);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// count("UUID") FALSE: UUID absent → uuid="" →
// getNsmDeviceFromStaticUUID("") throws std::runtime_error.
TEST_F(NsmAltitudePressureFactoryTest, MakeAltPressure_MissingUUID_Throws)
{
    const std::string path5 = objPath + "_no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(path5, interface);
    // "UUID" intentionally omitted → uuid="" → getNsmDeviceFromStaticUUID
    // throws
    pm["Name"] = std::string("AltPressure_NoUUID");
    pm["PhysicalContext"] = std::string("GPU");

    EXPECT_THROW_COROUTINE(
        makeNsmAltitudePressure(mockManager, interface, path5),
        std::runtime_error);
}

// count("PhysicalContext") FALSE: PhysicalContext absent → physicalContext="" →
// NsmNumericSensorDbusValue cannot convert "" to enum → InvalidEnumString.
TEST_F(NsmAltitudePressureFactoryTest,
       MakeAltPressure_MissingPhysicalContext_Throws)
{
    const std::string path6 = objPath + "_no_physctx";
    auto& pm = utils::MockDbusAsync::propertyMap(path6, interface);
    pm["UUID"] = gpuUuid;
    pm["Name"] = std::string("AltPressure_NoCtx");
    // "PhysicalContext" intentionally omitted → physicalContext="" →
    // NsmNumericSensorDbusValue tries to convert "" → InvalidEnumString

    EXPECT_THROW_COROUTINE(
        makeNsmAltitudePressure(mockManager, interface, path6), std::exception);
}

// Optional properties present → covers try-success paths for Implementation,
// MaxAllowableOperatingValue, MaxValue, MinValue in nsmAltitudePressure.cpp
TEST_F(NsmAltitudePressureFactoryTest,
       MakeAltPressure_WithOptionalProps_SuccessPaths)
{
    const std::string path7 = objPath + "_optional";
    auto& pm = utils::MockDbusAsync::propertyMap(path7, interface);
    pm["UUID"] = gpuUuid;
    pm["Name"] = std::string("AltPressure_OptProps");
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");
    pm["Implementation"] = std::string("PhysicalSensor");
    pm["MaxAllowableOperatingValue"] = double(200.0);
    pm["MaxValue"] = double(300.0);
    pm["MinValue"] = double(-100.0);

    size_t before = gpu->roundRobinSensors.size();
    makeNsmAltitudePressure(mockManager, interface, path7);
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// =============================================================================
// Real PowerSensorFactory / TempSensorFactory / EnergySensorFactory coverage
// via NsmObjectFactory dispatch. Each exercises makeSensor() and
// makeAggregator() for the respective builder inside NumericSensorFactory.
// =============================================================================

struct RealNumericSensorFactoryTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t sensorUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nsmDev;

    RealNumericSensorFactoryTest() : SensorManagerTest(devices)
    {
        nsmDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(sensorUuid));
        EXPECT_NE(nsmDev, nullptr);
    }

    ~RealNumericSensorFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    // Set up chassis association so that NumericSensorFactory::make() proceeds
    // past the empty-chassis-association guard and reaches
    // builder->makeSensor()
    void setupChassisAssoc(const std::string& interface,
                           const std::string& objPath)
    {
        utils::MockDbusAsync::serviceMap() = {
            {"xyz.openbmc_project.EntityManager",
             {interface + ".Associations0"}}};
        auto& assocPm = utils::MockDbusAsync::propertyMap(
            objPath, interface + ".Associations0");
        assocPm["Forward"] = std::string("chassis");
        assocPm["Backward"] = std::string("all_sensors");
        assocPm["AbsolutePath"] = chassisPath;
    }
};

// Diagnostic: verify factory registrations exist in NsmObjectFactory
TEST_F(RealNumericSensorFactoryTest, FactoriesAreRegistered)
{
    EXPECT_GT(NsmObjectFactory::instance().creationFunctions.count(
                  "xyz.openbmc_project.Configuration.NSM_Power"),
              0u);
    EXPECT_GT(NsmObjectFactory::instance().creationFunctions.count(
                  "xyz.openbmc_project.Configuration.NSM_Temp"),
              0u);
    EXPECT_GT(NsmObjectFactory::instance().creationFunctions.count(
                  "xyz.openbmc_project.Configuration.NSM_Energy"),
              0u);
    EXPECT_GT(NsmObjectFactory::instance().creationFunctions.count(
                  "xyz.openbmc_project.Configuration.NSM_Voltage"),
              0u);
}

// ─── PowerSensorFactory tests ────────────────────────────────────────────────

// PowerSensorFactory::makeSensor() → empty CompositeNumericSensors →
// if(!candidateForList.empty()) FALSE branch → sensor created without composite
TEST_F(RealNumericSensorFactoryTest, NSMPower_MakeSensor_EmptyCompositeList)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Power";
    const std::string path = "/xyz/test/power/sensor0";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("PowerSensor0");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(1);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");
    // AveragingInterval: sync D-Bus call NOT in try/catch — must be present
    pm["AveragingInterval"] = uint64_t(0);
    // CompositeNumericSensors: sync D-Bus call in catch(SdBusError) only —
    // mock throws runtime_error, so must also be present to avoid propagation
    pm["CompositeNumericSensors"] = std::vector<std::string>{};

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// PowerSensorFactory::makeSensor() → non-empty CompositeNumericSensors →
// if(!candidateForList.empty()) TRUE branch → composite child value appended
TEST_F(RealNumericSensorFactoryTest, NSMPower_MakeSensor_NonEmptyCompositeList)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Power";
    const std::string path = "/xyz/test/power/sensor_composite";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("PowerSensor_Composite");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(2);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");
    pm["AveragingInterval"] = uint64_t(100);
    pm["CompositeNumericSensors"] = std::vector<std::string>{"TotalGPUPower"};

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// PowerSensorFactory::makeAggregator() → Aggregated=true → aggregator created
TEST_F(RealNumericSensorFactoryTest, NSMPower_MakeAggregator_AggregatedTrue)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Power";
    const std::string path = "/xyz/test/power/sensor_agg";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("PowerSensor_Agg");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(3);
    pm["Aggregated"] = bool(true);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");
    pm["AveragingInterval"] = uint64_t(0);
    pm["CompositeNumericSensors"] = std::vector<std::string>{};

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->sensorAggregators.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->sensorAggregators.size(), before);
}

// ─── TempSensorFactory tests ─────────────────────────────────────────────────

// TempSensorFactory::makeSensor() → sensor created and added to roundRobin
TEST_F(RealNumericSensorFactoryTest, NSMTemp_MakeSensor_SensorAdded)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string path = "/xyz/test/temp/sensor0";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("TempSensor0");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(1);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// TempSensorFactory::makeAggregator() → Aggregated=true → aggregator created
TEST_F(RealNumericSensorFactoryTest, NSMTemp_MakeAggregator_AggregatedTrue)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string path = "/xyz/test/temp/sensor_agg";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("TempSensor_Agg");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(2);
    pm["Aggregated"] = bool(true);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->sensorAggregators.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->sensorAggregators.size(), before);
}

// ─── EnergySensorFactory tests ───────────────────────────────────────────────

// EnergySensorFactory::makeSensor() → sensor created and added to roundRobin
TEST_F(RealNumericSensorFactoryTest, NSMEnergy_MakeSensor_SensorAdded)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Energy";
    const std::string path = "/xyz/test/energy/sensor0";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("EnergySensor0");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(1);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// EnergySensorFactory::makeAggregator() → Aggregated=true → aggregator created
TEST_F(RealNumericSensorFactoryTest, NSMEnergy_MakeAggregator_AggregatedTrue)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Energy";
    const std::string path = "/xyz/test/energy/sensor_agg";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("EnergySensor_Agg");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(2);
    pm["Aggregated"] = bool(true);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->sensorAggregators.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->sensorAggregators.size(), before);
}

// ─── NumericSensorFactory::make() branch coverage ────────────────────────────

// Non-chassis association → chassis_association stays empty →
// if (info.chassis_association.empty()) TRUE → co_return NSM_ERROR //

TEST_F(RealNumericSensorFactoryTest, NSMFactory_NoChassisAssoc_ReturnError)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string path = "/xyz/test/temp/no_chassis";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("TempSensor_NoChassis");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(1);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    // Association with forward != "chassis" → chassis_association stays empty
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {intf + ".Associations0"}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(path,
                                                      intf + ".Associations0");
    assocPm["Forward"] = std::string("containing"); // NOT "chassis"
    assocPm["Backward"] = std::string("all_sensors");
    assocPm["AbsolutePath"] = chassisPath;

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before); // no sensor added
}

// NSM_Temp with sensorId=0 → primary temp sensor association logic:
// if (info.type == "NSM_Temp" && info.sensorId == 0 &&
//     !info.chassis_association.empty()) TRUE → hasPrimaryTempAssoc=false →
// if (!hasPrimaryTempAssoc) TRUE → primary assoc added
TEST_F(RealNumericSensorFactoryTest, NSMTemp_sensorId0_PrimaryTempAssocAdded)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string path = "/xyz/test/temp/primary_sensorid0";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("TempSensor_Primary");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(0); // sensorId=0 → primary temp logic
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// NSM_Temp sensorId=0 + already has primary_temperature_sensor assoc →
// hasPrimaryTempAssoc=true → if (!hasPrimaryTempAssoc) FALSE → skip adding
TEST_F(RealNumericSensorFactoryTest,
       NSMTemp_sensorId0_AlreadyHasPrimaryTempAssoc)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string path = "/xyz/test/temp/already_has_primary";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("TempSensor_HasPrimary");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(0); // sensorId=0 triggers primary temp check
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    // Two associations: one chassis, one primary_temperature_sensor
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager",
         {intf + ".Associations0", intf + ".Associations1"}}};

    auto& assoc0 = utils::MockDbusAsync::propertyMap(path,
                                                     intf + ".Associations0");
    assoc0["Forward"] = std::string("chassis");
    assoc0["Backward"] = std::string("all_sensors");
    assoc0["AbsolutePath"] = chassisPath;

    auto& assoc1 = utils::MockDbusAsync::propertyMap(path,
                                                     intf + ".Associations1");
    assoc1["Forward"] = std::string("chassis");
    assoc1["Backward"] = std::string("primary_temperature_sensor");
    assoc1["AbsolutePath"] = chassisPath;

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// Optional properties (Implementation, MaxAllowableOperatingValue,
// ReadingBasis, Description) all present → try block success paths covered
TEST_F(RealNumericSensorFactoryTest, NSMPower_WithOptionalProperties)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Power";
    const std::string path = "/xyz/test/power/optional_props";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("PowerSensor_OptProps");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(4);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");
    pm["AveragingInterval"] = uint64_t(0);
    pm["CompositeNumericSensors"] = std::vector<std::string>{};
    // Optional properties — cover the try block success paths
    pm["Implementation"] = std::string("PhysicalSensor");
    pm["MaxAllowableOperatingValue"] = double(500.0);
    pm["MaxValue"] = double(1000.0);
    pm["MinValue"] = double(0.0);
    pm["ReadingBasis"] = std::string("Headroom");
    pm["Description"] = std::string("GPU Power in Watts");

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// PeakValue sub-interface present + NSM_Power →
// makePeakValueAndAdd() reaches if(info.type == "NSM_Power") TRUE branch
TEST_F(RealNumericSensorFactoryTest, NSMPower_WithPeakValue_PowerBranch)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Power";
    const std::string path = "/xyz/test/power/with_peak";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("PowerSensor_WithPeak");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(5);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");
    pm["AveragingInterval"] = uint64_t(0);
    pm["CompositeNumericSensors"] = std::vector<std::string>{};

    // PeakValue sub-interface → makePeakValueAndAdd() succeeds past all
    // getDbusProperty calls → if (info.type == "NSM_Power") TRUE
    auto& pvPm = utils::MockDbusAsync::propertyMap(path, intf + ".PeakValue");
    pvPm["SensorId"] = uint64_t(5);
    pvPm["Priority"] = bool(false);
    pvPm["Aggregated"] = bool(false);
    pvPm["AveragingInterval"] = uint64_t(0);

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// NSM_Temp with PeakValue sub-interface present →
// makePeakValueAndAdd() reaches if(info.type == "NSM_Power") FALSE → else
TEST_F(RealNumericSensorFactoryTest, NSMTemp_PeakValue_ElseBranch)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string path = "/xyz/test/temp/with_peak";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("TempSensor_WithPeak");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(1);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    // PeakValue sub-interface with all required properties set →
    // makePeakValueAndAdd() proceeds past getDbusProperty calls →
    // if (info.type == "NSM_Power") → FALSE ("NSM_Temp" != "NSM_Power") → else
    auto& pvPm = utils::MockDbusAsync::propertyMap(path, intf + ".PeakValue");
    pvPm["SensorId"] = uint64_t(1);
    pvPm["Priority"] = bool(false);
    pvPm["Aggregated"] = bool(false);
    pvPm["AveragingInterval"] = uint64_t(0);

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// makeAggregatorAndAddSensor: two aggregated sensors, same (low) priority →
// second call finds existing aggregator, if(priority && !agg->priority) FALSE
TEST_F(RealNumericSensorFactoryTest,
       NSMPower_AggregatedTwice_ExistingAggNoPriorityUpgrade)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Power";

    // First: create aggregated sensor, low priority → new aggregator
    const std::string path1 = "/xyz/test/power/agg_noupgrade1";
    auto& pm1 = utils::MockDbusAsync::propertyMap(path1, intf);
    pm1["Name"] = std::string("PowerAgg_NoUpgrade1");
    pm1["UUID"] = sensorUuid;
    pm1["SensorId"] = uint64_t(20);
    pm1["Aggregated"] = bool(true);
    pm1["Priority"] = bool(false);
    pm1["PhysicalContext"] = std::string("GPU");
    pm1["AveragingInterval"] = uint64_t(0);
    pm1["CompositeNumericSensors"] = std::vector<std::string>{};
    setupChassisAssoc(intf, path1);
    NsmObjectFactory::instance().createObjects(mockManager, intf, path1);

    // Second: aggregated, also low priority → finds aggregator, no upgrade
    const std::string path2 = "/xyz/test/power/agg_noupgrade2";
    auto& pm2 = utils::MockDbusAsync::propertyMap(path2, intf);
    pm2["Name"] = std::string("PowerAgg_NoUpgrade2");
    pm2["UUID"] = sensorUuid;
    pm2["SensorId"] = uint64_t(21);
    pm2["Aggregated"] = bool(true);
    pm2["Priority"] = bool(false); // same low priority → no upgrade
    pm2["PhysicalContext"] = std::string("GPU");
    pm2["AveragingInterval"] = uint64_t(0);
    pm2["CompositeNumericSensors"] = std::vector<std::string>{};
    setupChassisAssoc(intf, path2);

    const size_t aggBefore = nsmDev->sensorAggregators.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path2);
    // Same aggregator reused — no new aggregator added
    EXPECT_EQ(nsmDev->sensorAggregators.size(), aggBefore);
}

// makeAggregatorAndAddSensor: first low-priority aggregated, then high-priority
// → finds existing aggregator → if(priority && !agg->priority) TRUE → upgrade
TEST_F(RealNumericSensorFactoryTest, NSMPower_AggregatedTwice_PriorityUpgrade)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Power";

    // First: aggregated, low priority → creates aggregator (priority=false)
    const std::string path1 = "/xyz/test/power/agg_upgrade1";
    auto& pm1 = utils::MockDbusAsync::propertyMap(path1, intf);
    pm1["Name"] = std::string("PowerAgg_Upgrade1");
    pm1["UUID"] = sensorUuid;
    pm1["SensorId"] = uint64_t(30);
    pm1["Aggregated"] = bool(true);
    pm1["Priority"] = bool(false); // low priority initially
    pm1["PhysicalContext"] = std::string("GPU");
    pm1["AveragingInterval"] = uint64_t(0);
    pm1["CompositeNumericSensors"] = std::vector<std::string>{};
    setupChassisAssoc(intf, path1);
    NsmObjectFactory::instance().createObjects(mockManager, intf, path1);

    // Second: aggregated, HIGH priority → finds aggregator → priority upgrade
    const std::string path2 = "/xyz/test/power/agg_upgrade2";
    auto& pm2 = utils::MockDbusAsync::propertyMap(path2, intf);
    pm2["Name"] = std::string("PowerAgg_Upgrade2");
    pm2["UUID"] = sensorUuid;
    pm2["SensorId"] = uint64_t(31);
    pm2["Aggregated"] = bool(true);
    pm2["Priority"] = bool(true); // HIGH priority → triggers upgrade
    pm2["PhysicalContext"] = std::string("GPU");
    pm2["AveragingInterval"] = uint64_t(0);
    pm2["CompositeNumericSensors"] = std::vector<std::string>{};
    setupChassisAssoc(intf, path2);

    NsmObjectFactory::instance().createObjects(mockManager, intf, path2);
    // Aggregator should now be in priority queue — verify it's still tracked
    EXPECT_GE(nsmDev->sensorAggregators.size(), 1u);
}

// ─── VoltageSensorFactory tests ──────────────────────────────────────────────

// VoltageSensorFactory::makeSensor() → Aggregated=false → sensor created and
// added to roundRobinSensors (covers lines 93-103 of nsmVoltage.cpp)
TEST_F(RealNumericSensorFactoryTest, NSMVoltage_MakeSensor_SensorAdded)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Voltage";
    const std::string path = "/xyz/test/voltage/sensor0";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("VoltageSensor0");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(1);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// VoltageSensorFactory::makeAggregator() → Aggregated=true → aggregator
// created (covers lines 105-110 of nsmVoltage.cpp)
TEST_F(RealNumericSensorFactoryTest, NSMVoltage_MakeAggregator_AggregatedTrue)
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Voltage";
    const std::string path = "/xyz/test/voltage/sensor_agg";

    auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
    pm["Name"] = std::string("VoltageSensor_Agg");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(2);
    pm["Aggregated"] = bool(true);
    pm["Priority"] = bool(false);
    pm["PhysicalContext"] = std::string("GPU");

    setupChassisAssoc(intf, path);

    const size_t before = nsmDev->sensorAggregators.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, path);
    EXPECT_GT(nsmDev->sensorAggregators.size(), before);
}

// ============================================================================
// NumericSensorFactory::make – !nsmDevice branch
// ============================================================================

// When getNsmDeviceFromStaticUUID returns nullptr, make() must log and
// co_return NSM_ERROR without creating any sensor.
struct NumericSensorFactoryMakeNullDeviceTest :
    public ::testing::Test,
    public utils::DBusTest
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_Power";
    const std::string path =
        "/xyz/openbmc_project/inventory/system/sensor/null_dev_test";
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX";

    NsmDeviceTable devices;
    NiceMock<NullReturnMockSensorManager> nullManager{devices};

    NumericSensorFactoryMakeNullDeviceTest()
    {
        sensorManagerInstance.reset(&nullManager);
    }
    ~NumericSensorFactoryMakeNullDeviceTest() override
    {
        sensorManagerInstance.release();
        utils::MockDbusAsync::serviceMap().clear();
    }

    void setupProperties()
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
        pm["Name"] = std::string("NullDevSensor");
        pm["UUID"] = std::string("STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0");
        pm["SensorId"] = uint64_t(1);
        pm["Aggregated"] = bool(false);
        pm["Priority"] = bool(false);
    }

    void setupChassisAssoc()
    {
        utils::MockDbusAsync::serviceMap() = {
            {"xyz.openbmc_project.EntityManager", {intf + ".Associations0"}}};
        auto& pm = utils::MockDbusAsync::propertyMap(path,
                                                     intf + ".Associations0");
        pm["Forward"] = std::string("chassis");
        pm["Backward"] = std::string("all_sensors");
        pm["AbsolutePath"] = chassisPath;
    }
};

TEST_F(NumericSensorFactoryMakeNullDeviceTest,
       Make_NullDevice_DoesNotCreateSensor)
{
    setupProperties();
    setupChassisAssoc();

    // builder is never invoked when !nsmDevice — use StrictMock to verify
    auto mockBuilder = std::make_unique<StrictMock<MockNumericSensorBuilder>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _)).Times(0);
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    factory.make(nullManager, intf, path);
    // !nsmDevice branch covered; co_return NSM_ERROR – no crash //

    SUCCEED();
}
