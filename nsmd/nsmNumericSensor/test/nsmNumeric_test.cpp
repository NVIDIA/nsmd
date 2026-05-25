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

#include "utils.hpp"

#include <sdbusplus/bus.hpp>
#include <tal.hpp>

#include <memory>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmNumericSensor.hpp"
#include "test/mockSensorManager.hpp"
#ifdef NVIDIA_SHMEM
#include "sharedMemCommon.hpp"
#endif
#include "nsmNumericSensorValue_mock.hpp"

// Fixture so sensor ctors (which call
// SensorManager::getInstance().getObjServer() to publish Sensor.Type via
// Boost-ASIO) succeed under gtest.
namespace
{
struct NumericDevicesStorage
{
    NsmDeviceTable nsmDevices;
};

struct NumericFixtureBase :
    private NumericDevicesStorage,
    public ::testing::Test,
    public SensorManagerTest
{
    NumericFixtureBase() : SensorManagerTest(nsmDevices) {}
    ~NumericFixtureBase() override
    {
        cleanupDeviceSensors(nsmDevices);
    }
};
} // namespace

using NsmNumericSensorDbusValueFixture = NumericFixtureBase;
using NsmNumericSensorDbusValueTimestampFixture = NumericFixtureBase;
using SMBPBIPowerSMBusSensorBytesConverterFixture = NumericFixtureBase;
using Uint64SMBusSensorBytesConverterFixture = NumericFixtureBase;
using SFxP24F8SMBusSensorBytesConverterFixture = NumericFixtureBase;
using NsmNumericSensorShmemFixture = NumericFixtureBase;
using NsmNumericSensorDbusStatusFixture = NumericFixtureBase;
using NsmNumericSensorAggregatorFixture = NumericFixtureBase;

static auto& bus = utils::DBusHandler::getBus();
static const std::string sensorName("dummy_sensor");
static const std::string sensorType("dummy_type");
static const std::vector<utils::Association>
    associations({{"chassis", "all_sensors",
                   "/xyz/openbmc_project/inventory/dummy_device"}});
static const std::string physicalContexnt("GPU");
static const double maxAllowableValue{std::numeric_limits<double>::infinity()};
static const double maxValue{std::numeric_limits<double>::infinity()};
static const double minValue{std::numeric_limits<double>::lowest()};
static const std::string readingBasis("Headroom");
static const std::string description("dummy_sensor");

static const double val{32432.8970};

TEST_F(NsmNumericSensorDbusValueFixture, GoodTest)
{
    nsm::NsmNumericSensorDbusValue value{
        bus,           sensorName,
        sensorType,    nsm::SensorUnit::DegreesC,
        associations,  physicalContexnt,
        nullptr,       maxAllowableValue,
        maxValue,      minValue,
        &readingBasis, &description};
    value.updateReading(val);

    EXPECT_EQ(value.valueIntf.value(), val);
    EXPECT_EQ(value.valueIntf.unit(), nsm::SensorUnit::DegreesC);
}

TEST_F(NsmNumericSensorDbusValueTimestampFixture, GoodTest)
{
    nsm::NsmNumericSensorDbusValueTimestamp value{
        bus,           sensorName,
        sensorType,    nsm::SensorUnit::DegreesC,
        associations,  physicalContexnt,
        nullptr,       maxAllowableValue,
        maxValue,      minValue,
        &readingBasis, &description};
    auto timestamp = utils::getCurrentSteadyClockTimestamp();
    value.updateReading(val, timestamp);

    EXPECT_EQ(value.timestampIntf.elapsed(), timestamp);
    EXPECT_EQ(value.valueIntf.value(), val);
    EXPECT_EQ(value.valueIntf.unit(), nsm::SensorUnit::DegreesC);
}

TEST_F(SMBPBIPowerSMBusSensorBytesConverterFixture, GoodTest)
{
    nsm::SMBPBIPowerSMBusSensorBytesConverter converter;

    const std::vector<double> sensorVals{780.383, 100.004};

    for (const auto& sensorVal : sensorVals)
    {
        std::vector<uint8_t> data = converter.convert(sensorVal);

        uint32_t val;
        std::memcpy(&val, data.data(), 4);
        val = le32toh(val);

        const double power = val / 1000.0;
        EXPECT_DOUBLE_EQ(sensorVal, power);
    }
}

TEST_F(Uint64SMBusSensorBytesConverterFixture, GoodTest)
{
    nsm::Uint64SMBusSensorBytesConverter converter;

    const std::vector<double> sensorVals{3494028, 89};

    for (const auto& sensorVal : sensorVals)
    {
        std::vector<uint8_t> data = converter.convert(sensorVal);

        uint64_t val;
        std::memcpy(&val, data.data(), 8);
        val = le64toh(val);

        const double energy = val;
        EXPECT_DOUBLE_EQ(sensorVal, energy);
    }
}

TEST_F(SFxP24F8SMBusSensorBytesConverterFixture, GoodTest)
{
    nsm::SFxP24F8SMBusSensorBytesConverter converter;

    const std::vector<double> sensorVals{35.470, -8.347};

    for (const auto& sensorVal : sensorVals)
    {
        std::vector<uint8_t> data = converter.convert(sensorVal);

        int32_t val;
        std::memcpy(&val, data.data(), 4);
        val = le32toh(val);

        const double temp = val / static_cast<double>(1 << 8);
        EXPECT_NEAR(sensorVal, temp, 0.01);
    }
}

#ifdef NVIDIA_SHMEM
TEST_F(NsmNumericSensorShmemFixture, GoodTest)
{
    nsm::NsmNumericSensorShmem value{
        sensorName, sensorType, associations[0].absolutePath,
        std::make_unique<nsm::SMBPBITempSMBusSensorBytesConverter>()};

    EXPECT_EQ(value.objPath,
              "/xyz/openbmc_project/sensors/dummy_type/dummy_sensor");
    EXPECT_EQ(value.association, "/xyz/openbmc_project/inventory/dummy_device");
}

TEST_F(NsmNumericSensorShmemFixture, NanReadingSetsRcToOne)
{
    nsm::NsmNumericSensorShmem value{
        sensorName, sensorType, associations[0].absolutePath,
        std::make_unique<nsm::SMBPBITempSMBusSensorBytesConverter>()};

    nsm_shmem_utils::SharedMemoryManager::telemetryData.clear();
    value.updateReading(std::numeric_limits<double>::quiet_NaN());

    ASSERT_EQ(nsm_shmem_utils::SharedMemoryManager::telemetryData.size(), 1u);
    EXPECT_EQ(nsm_shmem_utils::SharedMemoryManager::telemetryData.back().rc, 1);
}

TEST_F(NsmNumericSensorShmemFixture, ValidReadingKeepsRcZero)
{
    nsm::NsmNumericSensorShmem value{
        sensorName, sensorType, associations[0].absolutePath,
        std::make_unique<nsm::SMBPBITempSMBusSensorBytesConverter>()};

    nsm_shmem_utils::SharedMemoryManager::telemetryData.clear();
    value.updateReading(42.5);

    ASSERT_EQ(nsm_shmem_utils::SharedMemoryManager::telemetryData.size(), 1u);
    EXPECT_EQ(nsm_shmem_utils::SharedMemoryManager::telemetryData.back().rc, 0);
}
#endif

TEST_F(NsmNumericSensorDbusStatusFixture, GoodTest)
{
    nsm::NsmNumericSensorDbusStatus status{bus, sensorName, sensorType};
    status.updateStatus(true, false);

    EXPECT_EQ(status.availabilityIntf.available(), true);
    EXPECT_EQ(status.operationalStatusIntf.functional(), false);
}

TEST_F(NsmNumericSensorAggregatorFixture, GoodTest)
{
    auto elem1 = std::make_unique<MockNsmNumericSensorValue>();
    auto elem1Ptr = elem1.get();
    auto elem2 = std::make_unique<MockNsmNumericSensorValue>();
    auto elem2Ptr = elem2.get();

    nsm::NsmNumericSensorValueAggregate aggregator{std::move(elem1),
                                                   std::move(elem2)};

    EXPECT_EQ(aggregator.objects.size(), 2);

    auto timestamp = utils::getCurrentSteadyClockTimestamp();
    EXPECT_CALL(*elem1Ptr, updateReading(val, timestamp)).Times(1);
    EXPECT_CALL(*elem2Ptr, updateReading(val, timestamp)).Times(1);

    aggregator.updateReading(val, timestamp);
}
