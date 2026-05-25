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
 * nsmBatch16_test.cpp
 *
 * Coverage targets:
 *   1. nsmd/nsmSensor.cpp
 *      - NsmSensor::equals() (line 66-73): comparison returns true when
 *        both sensors produce identical genRequestMsg output; returns false
 *        when outputs differ or either returns nullopt.
 *      - NsmSensor::operator==() (line 75-78): delegates to equals().
 */

#include "base.h"
#include "platform-environmental.h"

#include "utils.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmNumericSensor/nsmTemp.hpp"
#include "test/mockSensorManager.hpp"

#undef private
#undef protected

using namespace nsm;
using namespace ::testing;

// Fixture that initializes SensorManager singleton so sensor ctors (which
// publish Sensor.Type via Boost-ASIO and call
// SensorManager::getInstance().getObjServer()) succeed.
namespace
{
struct EqualityDevicesStorage
{
    NsmDeviceTable devices;
};

struct EqualityFixtureBase :
    private EqualityDevicesStorage,
    public ::testing::Test,
    public SensorManagerTest
{
    EqualityFixtureBase() : SensorManagerTest(devices) {}
    ~EqualityFixtureBase() override
    {
        cleanupDeviceSensors(devices);
    }
};
} // namespace

using NsmSensorEqualsFixture = EqualityFixtureBase;
using NsmSensorOperatorEqualFixture = EqualityFixtureBase;

// ============================================================================
// Test helpers
// ============================================================================

static auto& bus = utils::DBusHandler::getBus();
static const std::vector<utils::Association>
    associations({{"chassis", "all_sensors",
                   "/xyz/openbmc_project/inventory/dummy_device"}});
static const std::string physicalContext("GPU");
static const double maxAllowableValue{std::numeric_limits<double>::infinity()};
static const double maxValue{std::numeric_limits<double>::quiet_NaN()};
static const double minValue{std::numeric_limits<double>::quiet_NaN()};
static const std::string readingBasis("Headroom");
static const std::string description("test_sensor");

// Minimal NsmSensor subclass that always returns nullopt from genRequestMsg
class NullRequestSensor : public NsmSensor
{
  public:
    NullRequestSensor(const std::string& name, const std::string& type) :
        NsmSensor(name, type)
    {}

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        return std::nullopt;
    }

    uint8_t handleResponseMsg(const struct nsm_msg* /*responseMsg*/,
                              size_t /*responseLen*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

// Minimal NsmSensor subclass that returns a valid (non-nullopt) request
class ValidRequestSensor : public NsmSensor
{
  public:
    ValidRequestSensor(const std::string& name, const std::string& type) :
        NsmSensor(name, type)
    {}

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        // Return a minimal 20-byte non-nullopt request buffer
        return std::vector<uint8_t>(20, 0);
    }

    uint8_t handleResponseMsg(const struct nsm_msg* /*responseMsg*/,
                              size_t /*responseLen*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

// ============================================================================
// NsmSensor::equals() tests
// ============================================================================

// Two NsmTemp sensors with the same sensorId produce identical request
// messages (same NSM command + same sensor_id field). equals() must return
// true.
TEST_F(NsmSensorEqualsFixture, SameSensorType_SameSensorId_ReturnsTrue)
{
    NsmTemp sensorA{bus,
                    "temp_sensor_A",
                    "NSM_Temp",
                    5,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};
    NsmTemp sensorB{bus,
                    "temp_sensor_B",
                    "NSM_Temp",
                    5,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};

    EXPECT_TRUE(sensorA.equals(sensorB));
    EXPECT_TRUE(sensorB.equals(sensorA));
}

// Two NsmTemp sensors with different sensorIds produce different request
// messages. equals() must return false.
TEST_F(NsmSensorEqualsFixture, SameSensorType_DifferentSensorId_ReturnsFalse)
{
    NsmTemp sensorA{bus,
                    "temp_sensor_C",
                    "NSM_Temp",
                    1,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};
    NsmTemp sensorB{bus,
                    "temp_sensor_D",
                    "NSM_Temp",
                    2,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};

    EXPECT_FALSE(sensorA.equals(sensorB));
    EXPECT_FALSE(sensorB.equals(sensorA));
}

// Comparing a sensor to itself must return true.
TEST_F(NsmSensorEqualsFixture, SelfComparison_ReturnsTrue)
{
    NsmTemp sensor{bus,
                   "temp_sensor_self",
                   "NSM_Temp",
                   3,
                   associations,
                   associations[0].absolutePath,
                   physicalContext,
                   nullptr,
                   maxAllowableValue,
                   maxValue,
                   minValue,
                   &readingBasis,
                   &description};

    EXPECT_TRUE(sensor.equals(sensor));
}

// When either sensor returns nullopt from genRequestMsg, equals() returns
// false because the short-circuit "requestMsg && sensorRequestMsg" fails.
TEST_F(NsmSensorEqualsFixture, NullRequestSensor_ReturnsFalse)
{
    NsmTemp normalSensor{bus,
                         "temp_sensor_normal",
                         "NSM_Temp",
                         7,
                         associations,
                         associations[0].absolutePath,
                         physicalContext,
                         nullptr,
                         maxAllowableValue,
                         maxValue,
                         minValue,
                         &readingBasis,
                         &description};
    NullRequestSensor nullSensor("null_sensor", "NSM_Null");

    EXPECT_FALSE(normalSensor.equals(nullSensor));
    EXPECT_FALSE(nullSensor.equals(normalSensor));
}

// Two NullRequestSensors: both return nullopt; result is false because
// "requestMsg && sensorRequestMsg" evaluates to false.
TEST_F(NsmSensorEqualsFixture, BothNullRequest_ReturnsFalse)
{
    NullRequestSensor sensorA("null_A", "NSM_Null");
    NullRequestSensor sensorB("null_B", "NSM_Null");

    EXPECT_FALSE(sensorA.equals(sensorB));
}

// ============================================================================
// NsmSensor::operator==() tests — delegates to equals()
// ============================================================================

TEST_F(NsmSensorOperatorEqualFixture, SameSensorId_ReturnsTrue)
{
    NsmTemp sensorA{bus,
                    "temp_op_A",
                    "NSM_Temp",
                    10,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};
    NsmTemp sensorB{bus,
                    "temp_op_B",
                    "NSM_Temp",
                    10,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};

    EXPECT_TRUE(sensorA == sensorB);
}

TEST_F(NsmSensorOperatorEqualFixture, DifferentSensorId_ReturnsFalse)
{
    NsmTemp sensorA{bus,
                    "temp_op_C",
                    "NSM_Temp",
                    11,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};
    NsmTemp sensorB{bus,
                    "temp_op_D",
                    "NSM_Temp",
                    12,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};

    EXPECT_FALSE(sensorA == sensorB);
}

TEST_F(NsmSensorOperatorEqualFixture, SelfComparison_ReturnsTrue)
{
    NsmTemp sensor{bus,
                   "temp_op_self",
                   "NSM_Temp",
                   4,
                   associations,
                   associations[0].absolutePath,
                   physicalContext,
                   nullptr,
                   maxAllowableValue,
                   maxValue,
                   minValue,
                   &readingBasis,
                   &description};

    EXPECT_TRUE(sensor == sensor);
}

// ============================================================================
// NsmSensor::update() branch coverage
// ============================================================================

// Fixture providing a MockNsmDevice for update() tests.
struct NsmSensorUpdateTest : public testing::Test, public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;

    NsmSensorUpdateTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(
                "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0"));
        ASSERT_NE(mockDevice, nullptr);
    }

    ~NsmSensorUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// NsmSensor::update(): genRequestMsg returns nullopt
// → if (!requestMsg.has_value()) TRUE → co_return NSM_SW_ERROR //

TEST_F(NsmSensorUpdateTest, Update_NulloptRequest_ReturnsError)
{
    NullRequestSensor sensor("null_sensor", "test_type");
    auto rc = sensor.update(mockDevice).data();
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

// NsmSensor::update(): sensorIO returns non-zero error code
// → if (rc) TRUE branch (nsmSensor.cpp line 49-53) → early co_return rc //

TEST_F(NsmSensorUpdateTest, Update_SensorIOFails_EarlyReturnWithError)
{
    ValidRequestSensor sensor("valid_sensor", "test_type");

    // Make sensorIO return a non-zero completion code (error)
    EXPECT_CALL(*mockDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    auto rc = sensor.update(mockDevice).data();
    // rc should be the error code from sensorIO (non-zero)
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// NsmSensor::update(): sensorIO returns success → falls through to
// handleResponseMsg (nsmSensor.cpp line 55) → co_return its rc //

TEST_F(NsmSensorUpdateTest, Update_SensorIOSuccess_CallsHandleResponse)
{
    ValidRequestSensor sensor("valid_sensor2", "test_type");

    // Provide a minimal response buffer so allocMessage doesn't crash
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr), 0);
    EXPECT_CALL(*mockDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp, NSM_SUCCESS));

    auto rc = sensor.update(mockDevice).data();
    // handleResponseMsg always returns NSM_SW_SUCCESS in ValidRequestSensor
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
