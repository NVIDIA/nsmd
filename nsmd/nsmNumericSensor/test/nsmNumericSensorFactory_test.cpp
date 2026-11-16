/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <algorithm>
#include <cmath>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "features/creators.hpp"
#include "nsmNumericSensorFactory.hpp"
#include "nsmNumericSensorValue_mock.hpp"
#include "nsmThresholdAggregator.hpp"

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Mock NumericSensorBuilder
// ============================================================================

class MockNumericSensorBuilder : public NumericSensorBuilder
{
  public:
    MOCK_METHOD(std::shared_ptr<NsmNumericSensor>, makeSensor,
                (const std::string&, const std::string&, sdbusplus::bus_t&,
                 const NumericSensorInfo&),
                (override));
    MOCK_METHOD(std::shared_ptr<NsmNumericAggregator>, makeAggregator,
                (const NumericSensorInfo&), (override));
};

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
// Stub classes for NumericSensorFactory::makeAggregatorAndAddSensor() tests
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
// NumericSensorFactory::makeFeatureSensorInfo() tests
// ============================================================================

namespace
{
const std::string chassisRelPath = "chassis/HGX";
const std::string chassisAbsPath =
    "/xyz/openbmc_project/inventory/system/chassis/HGX";

dbus::PropertyMap sensorParams(const std::string& name, uint64_t sensorId,
                               bool withChassis = true)
{
    dbus::PropertyMap params{{"Name", name},
                             {"SensorId", sensorId},
                             {"PhysicalContext", std::string("GPU")}};
    if (withChassis)
    {
        params["ChassisAssociationPaths"] =
            std::vector<std::string>{chassisRelPath};
    }
    return params;
}

size_t countBackward(const std::vector<utils::Association>& associations,
                     const std::string& backward)
{
    return std::ranges::count_if(associations, [&](const auto& assoc) {
        return assoc.backward == backward;
    });
}
} // namespace

TEST(NumericSensorFactoryFeatureInfoTest, NoChassisAssociation_ReturnsNullopt)
{
    features::DeviceContext dev;
    dev.name = "HGX_GPU_0";
    auto info = NumericSensorFactory::makeFeatureSensorInfo(
        dev, sensorParams("TestSensor", 1, false), "NSM_Voltage");
    EXPECT_FALSE(info.has_value());
}

TEST(NumericSensorFactoryFeatureInfoTest, EmptyChassisEntry_ReturnsNullopt)
{
    features::DeviceContext dev;
    dev.name = "HGX_GPU_0";
    auto params = sensorParams("TestSensor", 1, false);
    params["ChassisAssociationPaths"] = std::vector<std::string>{""};
    auto info = NumericSensorFactory::makeFeatureSensorInfo(dev, params,
                                                            "NSM_Voltage");
    EXPECT_FALSE(info.has_value());
}

TEST(NumericSensorFactoryFeatureInfoTest, MissingOptionalParams_UseDefaults)
{
    features::DeviceContext dev;
    dev.name = "HGX_GPU_0";
    dbus::PropertyMap params{
        {"Name", std::string("Test Sensor")},
        {"ChassisAssociationPaths", std::vector<std::string>{chassisRelPath}}};
    auto info = NumericSensorFactory::makeFeatureSensorInfo(dev, params,
                                                            "NSM_Voltage");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, "Test_Sensor");
    EXPECT_EQ(info->type, "NSM_Voltage");
    EXPECT_EQ(info->sensorId, 0);
    EXPECT_FALSE(info->priority);
    EXPECT_FALSE(info->aggregated);
    EXPECT_TRUE(info->physicalContext.empty());
    EXPECT_EQ(info->implementation, nullptr);
    EXPECT_EQ(info->readingBasis, nullptr);
    EXPECT_EQ(info->description, nullptr);
    EXPECT_EQ(info->maxAllowableValue, std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isnan(info->maxValue));
    EXPECT_TRUE(std::isnan(info->minValue));
    EXPECT_EQ(info->chassis_association, chassisAbsPath);
    ASSERT_EQ(info->associations.size(), 1u);
    EXPECT_EQ(info->associations[0].forward, "chassis");
    EXPECT_EQ(info->associations[0].backward, "all_sensors");
}

TEST(NumericSensorFactoryFeatureInfoTest, AllParams_Copied)
{
    features::DeviceContext dev;
    dev.name = "HGX_GPU_0";
    auto params = sensorParams("PowerSensor", 4);
    params["Priority"] = true;
    params["Aggregated"] = true;
    params["Implementation"] = std::string("PhysicalSensor");
    params["MaxAllowableOperatingValue"] = 500.0;
    params["MaxValue"] = 1000.0;
    params["MinValue"] = 0.0;
    params["ReadingBasis"] = std::string("Headroom");
    params["Description"] = std::string("GPU Power in Watts");
    params["ProcessorAssociationPaths"] = std::vector<std::string>{"HGX_GPU_0"};
    params["MemoryAssociationPaths"] =
        std::vector<std::string>{"memory/HGX_GPU_0_DRAM_0"};

    auto info = NumericSensorFactory::makeFeatureSensorInfo(dev, params,
                                                            "NSM_Power");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->sensorId, 4);
    EXPECT_TRUE(info->priority);
    EXPECT_TRUE(info->aggregated);
    EXPECT_EQ(info->physicalContext, "GPU");
    ASSERT_NE(info->implementation, nullptr);
    EXPECT_EQ(*info->implementation, "PhysicalSensor");
    EXPECT_EQ(info->maxAllowableValue, 500.0);
    EXPECT_EQ(info->maxValue, 1000.0);
    EXPECT_EQ(info->minValue, 0.0);
    ASSERT_NE(info->readingBasis, nullptr);
    EXPECT_EQ(*info->readingBasis, "Headroom");
    ASSERT_NE(info->description, nullptr);
    EXPECT_EQ(*info->description, "GPU Power in Watts");
    ASSERT_EQ(info->associations.size(), 3u);
    EXPECT_EQ(info->associations[1].forward, "processor");
    EXPECT_EQ(info->associations[1].absolutePath,
              "/xyz/openbmc_project/inventory/system/HGX_GPU_0");
    EXPECT_EQ(info->associations[2].forward, "memory");
}

TEST(NumericSensorFactoryFeatureInfoTest, Temp_SensorId0_PrimaryTempAssocAdded)
{
    features::DeviceContext dev;
    dev.name = "HGX_GPU_0";
    auto info = NumericSensorFactory::makeFeatureSensorInfo(
        dev, sensorParams("TempSensor", 0), "NSM_Temp");
    ASSERT_TRUE(info.has_value());
    // "chassis"/"all_sensors" + "primary_temperature_sensor"
    EXPECT_EQ(info->associations.size(), 2u);
    EXPECT_EQ(countBackward(info->associations, "primary_temperature_sensor"),
              1u);
}

TEST(NumericSensorFactoryFeatureInfoTest, Temp_NonZeroSensorId_NoPrimaryTemp)
{
    features::DeviceContext dev;
    dev.name = "HGX_GPU_0";
    auto info = NumericSensorFactory::makeFeatureSensorInfo(
        dev, sensorParams("TempSensor2", 1), "NSM_Temp");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->associations.size(), 1u);
}

TEST(NumericSensorFactoryFeatureInfoTest,
     PrimaryTemperatureAssociationEnabled_AddsAssocOnce)
{
    features::DeviceContext dev;
    dev.name = "HGX_GPU_0";
    // sensorId != 0: only the param adds the association
    auto params = sensorParams("TempSensor3", 1);
    params["PrimaryTemperatureAssociationEnabled"] = true;
    auto info = NumericSensorFactory::makeFeatureSensorInfo(dev, params,
                                                            "NSM_Temp");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->associations.size(), 2u);
    EXPECT_EQ(countBackward(info->associations, "primary_temperature_sensor"),
              1u);

    // sensorId 0 already adds it: the param must not duplicate it
    params["SensorId"] = uint64_t(0);
    info = NumericSensorFactory::makeFeatureSensorInfo(dev, params, "NSM_Temp");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->associations.size(), 2u);
    EXPECT_EQ(countBackward(info->associations, "primary_temperature_sensor"),
              1u);
}

TEST(NumericSensorFactoryResolveAssociationsTest,
     Temp_SensorId0_AlreadyHasPrimary_NoDuplicate)
{
    NumericSensorInfo info{};
    info.name = "TempSensor3";
    info.type = "NSM_Temp";
    info.sensorId = 0;
    info.associations = {
        {"chassis", "all_sensors", chassisAbsPath},
        {"chassis", "primary_temperature_sensor", chassisAbsPath}};
    NumericSensorFactory::resolveAssociations(info);
    EXPECT_EQ(info.chassis_association, chassisAbsPath);
    EXPECT_EQ(info.associations.size(), 2u);
}

TEST(NumericSensorFactoryResolveAssociationsTest, NoChassis_NoPrimaryTemp)
{
    NumericSensorInfo info{};
    info.type = "NSM_Temp";
    info.sensorId = 0;
    info.associations = {{"containing", "all_sensors", chassisAbsPath}};
    NumericSensorFactory::resolveAssociations(info);
    EXPECT_TRUE(info.chassis_association.empty());
    EXPECT_EQ(info.associations.size(), 1u);
}

// ============================================================================
// NumericSensorFactory::makeAggregatorAndAddSensor() tests
// ============================================================================

struct NumericSensorFactoryAddSensorTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t sensorUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string type = "NSM_Volt";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nsmDev;

    NumericSensorFactoryAddSensorTest() : SensorManagerTest(devices)
    {
        nsmDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(sensorUuid));
        EXPECT_NE(nsmDev, nullptr);
    }

    ~NumericSensorFactoryAddSensorTest()
    {
        cleanupDeviceSensors(devices);
    }

    NumericSensorInfo makeInfo(bool aggregated, bool priority,
                               uint8_t sensorId = 1)
    {
        NumericSensorInfo info{};
        info.name = "TestSensor";
        info.type = type;
        info.sensorId = sensorId;
        info.aggregated = aggregated;
        info.priority = priority;
        return info;
    }
};

TEST_F(NumericSensorFactoryAddSensorTest, NotAggregated_SensorInRoundRobin)
{
    auto info = makeInfo(/*aggregated=*/false, /*priority=*/false);
    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 1);
    StrictMock<MockNumericSensorBuilder> builder;
    EXPECT_CALL(builder, makeAggregator(_)).Times(0);

    size_t before = nsmDev->roundRobinSensors.size();
    NumericSensorFactory::makeAggregatorAndAddSensor(&builder, info, stubSensor,
                                                     sensorUuid, nsmDev.get());
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
    EXPECT_TRUE(nsmDev->sensorAggregators.empty());
}

TEST_F(NumericSensorFactoryAddSensorTest, NotAggregated_PriorityTrue)
{
    auto info = makeInfo(/*aggregated=*/false, /*priority=*/true);
    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 1);
    StrictMock<MockNumericSensorBuilder> builder;
    EXPECT_CALL(builder, makeAggregator(_)).Times(0);

    size_t before = nsmDev->prioritySensors.size();
    NumericSensorFactory::makeAggregatorAndAddSensor(&builder, info, stubSensor,
                                                     sensorUuid, nsmDev.get());
    EXPECT_GT(nsmDev->prioritySensors.size(), before);
}

TEST_F(NumericSensorFactoryAddSensorTest, Aggregated_NoExistingAgg_Created)
{
    auto info = makeInfo(/*aggregated=*/true, /*priority=*/false);
    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 1);
    auto stubAgg = std::make_shared<StubNsmNumericAggregator>("TestSensor",
                                                              type, false);
    StrictMock<MockNumericSensorBuilder> builder;
    EXPECT_CALL(builder, makeAggregator(_)).WillOnce(Return(stubAgg));

    NumericSensorFactory::makeAggregatorAndAddSensor(&builder, info, stubSensor,
                                                     sensorUuid, nsmDev.get());

    ASSERT_EQ(nsmDev->sensorAggregators.size(), 1u);
    EXPECT_EQ(nsmDev->sensorAggregators[0], stubAgg);
    EXPECT_NE(std::find(nsmDev->deviceSensors.begin(),
                        nsmDev->deviceSensors.end(), stubSensor),
              nsmDev->deviceSensors.end());
}

TEST_F(NumericSensorFactoryAddSensorTest,
       Aggregated_ExistingLowPrio_NewHighPrio_Promotes)
{
    auto existingAgg = std::make_shared<StubNsmNumericAggregator>("TestSensor",
                                                                  type, false);
    nsmDev->sensorAggregators.push_back(existingAgg);
    nsmDev->addSensor(existingAgg, false); // put in roundRobinSensors

    auto info = makeInfo(/*aggregated=*/true, /*priority=*/true, 2);
    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 2);
    StrictMock<MockNumericSensorBuilder> builder;
    EXPECT_CALL(builder, makeAggregator(_)).Times(0); // existing agg used

    NumericSensorFactory::makeAggregatorAndAddSensor(&builder, info, stubSensor,
                                                     sensorUuid, nsmDev.get());

    EXPECT_TRUE(existingAgg->priority);
    auto rawPtr = existingAgg.get();
    auto& rrq = nsmDev->roundRobinSensors;
    auto rrIt = std::find_if(rrq.begin(), rrq.end(), [rawPtr](const auto& p) {
        return p.get() == rawPtr;
    });
    EXPECT_EQ(rrIt, rrq.end());
    auto& prioQ = nsmDev->prioritySensors;
    auto prioIt =
        std::find_if(prioQ.begin(), prioQ.end(),
                     [rawPtr](const auto& p) { return p.get() == rawPtr; });
    EXPECT_NE(prioIt, prioQ.end());
}

TEST_F(NumericSensorFactoryAddSensorTest,
       Aggregated_ExistingHighPrio_NewHighPrio_NoChange)
{
    auto existingAgg = std::make_shared<StubNsmNumericAggregator>("TestSensor",
                                                                  type, true);
    nsmDev->sensorAggregators.push_back(existingAgg);
    nsmDev->addSensor(existingAgg, true); // put in prioritySensors

    auto info = makeInfo(/*aggregated=*/true, /*priority=*/true, 3);
    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 3);
    StrictMock<MockNumericSensorBuilder> builder;
    EXPECT_CALL(builder, makeAggregator(_)).Times(0);

    size_t rrBefore = nsmDev->roundRobinSensors.size();
    NumericSensorFactory::makeAggregatorAndAddSensor(&builder, info, stubSensor,
                                                     sensorUuid, nsmDev.get());

    EXPECT_TRUE(existingAgg->priority);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), rrBefore);
    EXPECT_EQ(nsmDev->sensorAggregators.size(), 1u);
}

TEST_F(NumericSensorFactoryAddSensorTest,
       Aggregated_ExistingLowPrio_NewLowPrio_NoPromotion)
{
    auto existingAgg = std::make_shared<StubNsmNumericAggregator>("TestSensor",
                                                                  type, false);
    nsmDev->sensorAggregators.push_back(existingAgg);
    nsmDev->addSensor(existingAgg, false);

    auto info = makeInfo(/*aggregated=*/true, /*priority=*/false, 4);
    auto stubSensor = std::make_shared<StubNsmNumericSensor>("TestSensor", 4);
    StrictMock<MockNumericSensorBuilder> builder;
    EXPECT_CALL(builder, makeAggregator(_)).Times(0);

    NumericSensorFactory::makeAggregatorAndAddSensor(&builder, info, stubSensor,
                                                     sensorUuid, nsmDev.get());

    EXPECT_FALSE(existingAgg->priority);
    EXPECT_EQ(nsmDev->sensorAggregators.size(), 1u);
}

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
// Numeric sensor feature creators (createPowerFeature, createTempFeature,
// createEnergyFeature, createVoltageFeature)
// =============================================================================

struct NumericSensorFeatureTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t sensorUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nsmDev;

    NumericSensorFeatureTest() : SensorManagerTest(devices)
    {
        nsmDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(sensorUuid));
        EXPECT_NE(nsmDev, nullptr);
    }

    ~NumericSensorFeatureTest()
    {
        cleanupDeviceSensors(devices);
    }

    features::DeviceContext makeDev(bool withDevice = true)
    {
        features::DeviceContext dev;
        dev.name = "HGX_GPU_0";
        dev.kind = "gpu";
        dev.staticUuid = sensorUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = nsmDev;
        }
        return dev;
    }

    static dbus::PropertyMap params(const std::string& name, uint64_t sensorId,
                                    bool aggregated = false,
                                    bool priority = false)
    {
        auto p = sensorParams(name, sensorId);
        p["Aggregated"] = aggregated;
        p["Priority"] = priority;
        return p;
    }
};

// ─── Power ──────────────────────────────────────────────────────────────────

TEST_F(NumericSensorFeatureTest, Power_SensorAdded)
{
    auto p = params("PowerSensor0", 1);
    p["AveragingInterval"] = uint64_t(0);
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createPowerFeature(mockManager, makeDev(), p);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Power_CompositeEnabled_SensorAdded)
{
    auto p = params("PowerSensor_Composite", 2);
    p["AveragingInterval"] = uint64_t(100);
    p["CompositeNumericSensorsEnabled"] = true;
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createPowerFeature(mockManager, makeDev(), p);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Power_Aggregated_AggregatorCreated)
{
    const size_t before = nsmDev->sensorAggregators.size();
    auto rc = features::createPowerFeature(mockManager, makeDev(),
                                           params("PowerSensor_Agg", 3, true));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->sensorAggregators.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Power_WithOptionalParams_SensorAdded)
{
    auto p = params("PowerSensor_OptProps", 4);
    p["Implementation"] = std::string("PhysicalSensor");
    p["MaxAllowableOperatingValue"] = 500.0;
    p["MaxValue"] = 1000.0;
    p["MinValue"] = 0.0;
    p["ReadingBasis"] = std::string("Headroom");
    p["Description"] = std::string("GPU Power in Watts");
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createPowerFeature(mockManager, makeDev(), p);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Power_PeakValueEnabled_TwoSensorsAdded)
{
    auto p = params("PowerSensor_WithPeak", 5);
    p["PeakValueEnabled"] = true;
    p["PeakValueSensorId"] = uint64_t(5);
    p["PeakValuePriority"] = false;
    p["PeakValueAggregated"] = false;
    p["PeakValueAveragingInterval"] = uint64_t(0);
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createPowerFeature(mockManager, makeDev(), p);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Power sensor + peak power sensor
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before + 2);
}

TEST_F(NumericSensorFeatureTest, Power_AggregatedTwice_ExistingAggReused)
{
    auto rc1 = features::createPowerFeature(
        mockManager, makeDev(), params("PowerAgg_NoUpgrade1", 20, true));
    EXPECT_EQ(rc1.data(), NSM_SUCCESS);
    const size_t aggBefore = nsmDev->sensorAggregators.size();
    auto rc2 = features::createPowerFeature(
        mockManager, makeDev(), params("PowerAgg_NoUpgrade2", 21, true));
    EXPECT_EQ(rc2.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->sensorAggregators.size(), aggBefore);
}

TEST_F(NumericSensorFeatureTest, Power_AggregatedTwice_PriorityUpgrade)
{
    auto rc1 = features::createPowerFeature(
        mockManager, makeDev(), params("PowerAgg_Upgrade1", 30, true, false));
    EXPECT_EQ(rc1.data(), NSM_SUCCESS);
    ASSERT_EQ(nsmDev->sensorAggregators.size(), 1u);
    EXPECT_FALSE(nsmDev->sensorAggregators[0]->priority);

    auto rc2 = features::createPowerFeature(
        mockManager, makeDev(), params("PowerAgg_Upgrade2", 31, true, true));
    EXPECT_EQ(rc2.data(), NSM_SUCCESS);
    ASSERT_EQ(nsmDev->sensorAggregators.size(), 1u);
    EXPECT_TRUE(nsmDev->sensorAggregators[0]->priority);
}

TEST_F(NumericSensorFeatureTest, Power_NoChassisAssoc_ReturnsError)
{
    auto p = params("PowerSensor_NoChassis", 1);
    p.erase("ChassisAssociationPaths");
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createPowerFeature(mockManager, makeDev(), p);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before);
}

TEST_F(NumericSensorFeatureTest, Power_NullDevice_ReturnsError)
{
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createPowerFeature(mockManager, makeDev(false),
                                           params("NullDevSensor", 1));
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before);
}

// ─── Temp ───────────────────────────────────────────────────────────────────

TEST_F(NumericSensorFeatureTest, Temp_SensorAdded)
{
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createTempFeature(mockManager, makeDev(),
                                          params("TempSensor0", 1));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Temp_SensorId0_SensorAdded)
{
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createTempFeature(mockManager, makeDev(),
                                          params("TempSensor_Primary", 0));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Temp_Aggregated_AggregatorCreated)
{
    const size_t before = nsmDev->sensorAggregators.size();
    auto rc = features::createTempFeature(mockManager, makeDev(),
                                          params("TempSensor_Agg", 2, true));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->sensorAggregators.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Temp_NoChassisAssoc_ReturnsError)
{
    auto p = params("TempSensor_NoChassis", 1);
    p.erase("ChassisAssociationPaths");
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createTempFeature(mockManager, makeDev(), p);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before);
}

TEST_F(NumericSensorFeatureTest, Temp_NullDevice_ReturnsError)
{
    auto rc = features::createTempFeature(mockManager, makeDev(false),
                                          params("TempSensor_Null", 1));
    EXPECT_EQ(rc.data(), NSM_ERROR);
}

// ─── Energy ─────────────────────────────────────────────────────────────────

TEST_F(NumericSensorFeatureTest, Energy_SensorAdded)
{
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createEnergyFeature(mockManager, makeDev(),
                                            params("EnergySensor0", 1));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Energy_Aggregated_AggregatorCreated)
{
    const size_t before = nsmDev->sensorAggregators.size();
    auto rc = features::createEnergyFeature(
        mockManager, makeDev(), params("EnergySensor_Agg", 2, true));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->sensorAggregators.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Energy_NullDevice_ReturnsError)
{
    auto rc = features::createEnergyFeature(mockManager, makeDev(false),
                                            params("EnergySensor_Null", 1));
    EXPECT_EQ(rc.data(), NSM_ERROR);
}

// ─── Voltage ────────────────────────────────────────────────────────────────

TEST_F(NumericSensorFeatureTest, Voltage_SensorAdded)
{
    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createVoltageFeature(mockManager, makeDev(),
                                             params("VoltageSensor0", 1));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Voltage_Aggregated_AggregatorCreated)
{
    const size_t before = nsmDev->sensorAggregators.size();
    auto rc = features::createVoltageFeature(
        mockManager, makeDev(), params("VoltageSensor_Agg", 2, true));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(nsmDev->sensorAggregators.size(), before + 1);
}

TEST_F(NumericSensorFeatureTest, Voltage_NullDevice_ReturnsError)
{
    auto rc = features::createVoltageFeature(mockManager, makeDev(false),
                                             params("VoltageSensor_Null", 1));
    EXPECT_EQ(rc.data(), NSM_ERROR);
}
