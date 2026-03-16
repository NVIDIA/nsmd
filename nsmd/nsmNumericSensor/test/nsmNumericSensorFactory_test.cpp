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

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Mock NumericSensorBuilder
// ============================================================================

class MockNumericSensorBuilder : public NumericSensorBuilder
{
  public:
    MOCK_METHOD(std::shared_ptr<NsmNumericSensor>, makeSensor,
                (const std::string&, const std::string&, sdbusplus::bus::bus&,
                 const NumericSensorInfo&),
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
// Minimal concrete NsmNumericSensor for makeAggregatorAndAddSensor tests
// ============================================================================

class MinimalNumericSensor : public NsmNumericSensor
{
  public:
    MinimalNumericSensor(
        const std::string& name, const std::string& type, uint8_t sensorId,
        std::shared_ptr<NsmNumericSensorValueAggregate> sensorValue) :
        NsmNumericSensor(name, type, sensorId, sensorValue)
    {}

    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::nullopt;
    }

    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return NSM_SW_SUCCESS;
    }

    std::string getSensorType() override
    {
        return "MinimalType";
    }
};

static std::shared_ptr<MinimalNumericSensor>
    makeMinimalSensor(uint8_t sensorId = 1)
{
    auto sensorValue = std::make_shared<NsmNumericSensorValueAggregate>();
    return std::make_shared<MinimalNumericSensor>("test_sensor", "NSM_Temp",
                                                  sensorId, sensorValue);
}

static NumericSensorInfo makeAggInfo(const std::string& type, uint8_t sensorId,
                                     bool aggregated, bool priority)
{
    NumericSensorInfo info{};
    info.name = "test_sensor";
    info.type = type;
    info.sensorId = sensorId;
    info.aggregated = aggregated;
    info.priority = priority;
    return info;
}

// ============================================================================
// NumericSensorFactory::makeAggregatorAndAddSensor tests
// ============================================================================

// Non-aggregated sensor is added directly via addSensor(sensor, priority).
// No aggregator should be created or consulted.
TEST(MakeAggregatorAndAddSensorTest, NonAggregated_AddsSensorDirectly)
{
    NiceMock<MockNsmDevice> device(0, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0",
                                   NSM_DEV_ROLE_RESERVED);
    NiceMock<MockNumericSensorBuilder> builder;
    EXPECT_CALL(builder, makeAggregator(_)).Times(0);

    auto sensor = makeMinimalSensor(1);
    auto info = makeAggInfo("NSM_Temp", 1, false, false);

    const auto sizeBefore = device.deviceSensors.size();
    NumericSensorFactory::makeAggregatorAndAddSensor(&builder, info, sensor,
                                                     "uuid1", &device);
    EXPECT_EQ(device.deviceSensors.size(), sizeBefore + 1u);
    EXPECT_TRUE(device.sensorAggregators.empty());
}

// Aggregated sensor with no existing aggregator: builder->makeAggregator()
// is called, and the new aggregator is stored in sensorAggregators.
TEST(MakeAggregatorAndAddSensorTest, Aggregated_NoExisting_CreatesAggregator)
{
    NiceMock<MockNsmDevice> device(0, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0",
                                   NSM_DEV_ROLE_RESERVED);
    NiceMock<MockNumericSensorBuilder> builder;
    auto newAgg = std::make_shared<NsmThresholdAggregator>("new_agg",
                                                           "NSM_Temp", false);
    EXPECT_CALL(builder, makeAggregator(_)).WillOnce(Return(newAgg));

    auto sensor = makeMinimalSensor(1);
    auto info = makeAggInfo("NSM_Temp", 1, true, false);

    EXPECT_TRUE(device.sensorAggregators.empty());
    NumericSensorFactory::makeAggregatorAndAddSensor(&builder, info, sensor,
                                                     "uuid1", &device);
    EXPECT_EQ(device.sensorAggregators.size(), 1u);
    EXPECT_EQ(device.sensorAggregators[0], newAgg);
}

// Aggregated sensor with an existing aggregator of the same type and same
// low priority: no priority upgrade, makeAggregator() is NOT called.
TEST(MakeAggregatorAndAddSensorTest,
     Aggregated_ExistingAggregator_NoPriorityUpgrade)
{
    NiceMock<MockNsmDevice> device(0, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0",
                                   NSM_DEV_ROLE_RESERVED);
    NiceMock<MockNumericSensorBuilder> builder;
    auto existingAgg = std::make_shared<NsmThresholdAggregator>(
        "existing_agg", "NSM_Temp", false);
    device.sensorAggregators.push_back(existingAgg);

    EXPECT_CALL(builder, makeAggregator(_)).Times(0);

    auto sensor = makeMinimalSensor(1);
    auto info = makeAggInfo("NSM_Temp", 1, true, false);

    NumericSensorFactory::makeAggregatorAndAddSensor(&builder, info, sensor,
                                                     "uuid1", &device);
    EXPECT_FALSE(existingAgg->priority);            // priority unchanged
    EXPECT_EQ(device.sensorAggregators.size(), 1u); // no new aggregator
    EXPECT_FALSE(device.deviceSensors.empty());     // sensor was registered
}

// Aggregated sensor with existing low-priority aggregator and a new
// high-priority sensor: the aggregator is upgraded to priority=true and
// re-queued in the priority polling lane.
TEST(MakeAggregatorAndAddSensorTest,
     Aggregated_ExistingLowPriorityAgg_NewHighPriority_Upgrades)
{
    NiceMock<MockNsmDevice> device(0, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0",
                                   NSM_DEV_ROLE_RESERVED);
    NiceMock<MockNumericSensorBuilder> builder;
    auto existingAgg = std::make_shared<NsmThresholdAggregator>(
        "existing_agg", "NSM_Temp", false);
    device.sensorAggregators.push_back(existingAgg);

    EXPECT_CALL(builder, makeAggregator(_)).Times(0);

    auto sensor = makeMinimalSensor(1);
    auto info = makeAggInfo("NSM_Temp", 1, true,
                            true); // new sensor is high priority

    NumericSensorFactory::makeAggregatorAndAddSensor(&builder, info, sensor,
                                                     "uuid1", &device);
    EXPECT_TRUE(existingAgg->priority); // aggregator upgraded to high priority
}

// Aggregated sensor whose sensorId exceeds
// NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE (0xEF=239):
// aggregator->addSensor() returns NSM_SW_ERROR_DATA and the error branch is
// taken (logged). No exception should be thrown.
TEST(MakeAggregatorAndAddSensorTest,
     Aggregated_AddSensorFails_ErrorBranchReached)
{
    NiceMock<MockNsmDevice> device(0, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0",
                                   NSM_DEV_ROLE_RESERVED);
    NiceMock<MockNumericSensorBuilder> builder;
    auto newAgg = std::make_shared<NsmThresholdAggregator>("test_agg",
                                                           "NSM_Temp", false);
    EXPECT_CALL(builder, makeAggregator(_)).WillOnce(Return(newAgg));

    // sensorId 0xF0 (240) > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE (239)
    // → addSensor returns NSM_SW_ERROR_DATA
    auto sensor = makeMinimalSensor(0xF0);
    auto info = makeAggInfo("NSM_Temp", 0xF0, true, false);

    EXPECT_NO_THROW(NumericSensorFactory::makeAggregatorAndAddSensor(
        &builder, info, sensor, "uuid1", &device));
}
