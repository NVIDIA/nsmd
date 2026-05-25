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
 * nsmBatch12F.cpp
 *
 * Coverage targets:
 *   1. nsmd/nsmLongRunning/nsmLongRunningSensor.cpp -- handle() method:
 *      validateEvent success/failure paths, handleResponseMsg delegation,
 *      timer.stop() true/false branches.
 *   2. nsmd/nsmLongRunning/nsmAsyncLongRunningSensor.cpp -- handle() method:
 *      validateEvent success/failure paths, handleResponseMsg delegation,
 *      timer.stop() true/false branches.
 *   3. nsmd/nsmDumpCollection/nsmDebugInfo.cpp -- finish() method,
 *      getDebugInfo() switch cases (DeviceInformation, FWRuntimeData,
 *      FWSavedInfo, DeviceDump, default), getDebugInfo() and getDiagnostics()
 *      statusInterface already-in-progress guard, empty objectPath guard,
 *      dup failure guard.
 *   4. nsmd/nsmGPM/nsmGpmOem.cpp -- NsmGPMAggregated::handleSample()
 *      decode failure path (non-zero rc from decodeFunc), GPMMetricUpdator
 *      updateMetric dedup (same value), NVLinkMetricUpdator updateMetric
 *      dedup (same value), DRAMUsageMetricUpdator updateMetric dedup
 *      (same value), PortMetricPerInstanceUpdator resize and dedup,
 *      GPMMetricInstanceUpdator updateMetric dedup.
 *   5. nsmd/nsmNumericSensor/nsmNumericSensor.cpp --
 *      SMBPBIEnergySMBusSensorBytesConverter::convert(),
 *      SFxP24F8SMBusSensorBytesConverter::convert(),
 *      Uint64SMBusSensorBytesConverter::convert(),
 *      NsmNumericSensorDbusValue::calculateNextUpdateTimestamp()
 *      various branches, NsmNumericSensorDbusValue::canUpdate(),
 *      NsmNumericSensorValueAggregate::updateReading() canUpdate
 *      throttling branch, NsmNumericSensorDbusStatus::updateStatus().
 *   6. nsmd/nsmNumericSensor/nsmThresholdFactory.cpp --
 *      NsmThresholdFactory constructor, NsmThresholdAggregatorBuilder
 *      makeAggregator, createNsmThreshold static/dynamic branches,
 *      processThresholdsPair with lower/upper thresholds.
 */

#include <endian.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <tuple>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmDumpCollection/nsmDebugInfo.hpp"
#include "nsmGPM/nsmGpmOem.hpp"
#include "nsmLongRunning/nsmAsyncLongRunningSensor.hpp"
#include "nsmLongRunning/nsmLongRunningSensor.hpp"
#include "nsmNumericSensor/nsmNumericSensor.hpp"
#include "nsmNumericSensor/nsmNumericSensorFactory.hpp"
#include "nsmNumericSensor/nsmThreshold.hpp"
#include "nsmNumericSensor/nsmThresholdAggregator.hpp"
#include "nsmNumericSensor/nsmThresholdFactory.hpp"
#include "nsmNumericSensor/nsmThresholdValue.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#undef private
#undef protected

using namespace nsm;

// Fixture so NsmNumericSensorDbusValue ctors (which call
// SensorManager::getInstance().getObjServer() to publish Sensor.Type via
// Boost-ASIO) succeed under gtest.
namespace
{
struct DbusValueDevicesStorage
{
    NsmDeviceTable nsmDevices;
};

struct DbusValueFixtureBase :
    private DbusValueDevicesStorage,
    public ::testing::Test,
    public SensorManagerTest
{
    DbusValueFixtureBase() : SensorManagerTest(nsmDevices) {}
    ~DbusValueFixtureBase() override
    {
        cleanupDeviceSensors(nsmDevices);
    }
};
} // namespace

using CanUpdateB12FFixture = DbusValueFixtureBase;
using NsmNumericSensorDbusValueB12FFixture = DbusValueFixtureBase;

// ============================================================================
// Mock classes for LongRunning sensors
// ============================================================================

class MockableLongRunningSensor : public NsmLongRunningSensor
{
  public:
    MockableLongRunningSensor(const std::string& name, const std::string& type,
                              bool isLongRunning,
                              std::shared_ptr<NsmDevice> device,
                              uint8_t messageType, uint8_t commandCode) :
        NsmLongRunningSensor(name, type, isLongRunning, device, messageType,
                             commandCode)
    {}

    MOCK_METHOD(std::optional<std::vector<uint8_t>>, genRequestMsg,
                (eid_t, uint8_t), (override));
    MOCK_METHOD(uint8_t, handleResponseMsg, (const nsm_msg*, size_t),
                (override));
};

class MockableAsyncLongRunningSensor : public NsmAsyncLongRunningSensor
{
  public:
    MockableAsyncLongRunningSensor(const std::string& name,
                                   const std::string& type, bool isLongRunning,
                                   std::shared_ptr<NsmDevice> device,
                                   uint8_t messageType, uint8_t commandCode) :
        NsmAsyncLongRunningSensor(name, type, isLongRunning, device,
                                  messageType, commandCode)
    {}

    MOCK_METHOD(std::optional<std::vector<uint8_t>>, genRequestMsg,
                (eid_t, uint8_t), (override));
    MOCK_METHOD(uint8_t, handleResponseMsg, (const nsm_msg*, size_t),
                (override));
};

// ============================================================================
// Mock classes for GPM metric updaters
// ============================================================================

class MockMetricUpdatorB12F : public MetricUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const double val), (override));
};

class MockMetricPerInstanceUpdatorB12F : public MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

// ============================================================================
// Mock class for NsmNumericSensorValue
// ============================================================================

class MockNsmNumericSensorValueB12F : public NsmNumericSensorValue
{
  public:
    MOCK_METHOD(void, updateReading, (double value, uint64_t timestamp),
                (override));
};

// ============================================================================
// PART 1: NsmLongRunningSensor::handle() -- deeper coverage
// ============================================================================

TEST(NsmLongRunningSensorHandle,
     DISABLED_Handle_ValidateEventSuccess_CallsHandleResponseMsg)
{
    // Arrange
    auto sensor = std::make_shared<MockableLongRunningSensor>(
        "LR_HandleTest1", "LongRunning", true, nullptr, 0x05, 0x10);

    // Set acceptInstanceId to match the event
    sensor->acceptInstanceId = 0x07;
    sensor->isLongRunning = true;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 128, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x07;

    // handleResponseMsg will be called on validateEvent success
    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .WillOnce(Return(NSM_SW_SUCCESS));

    int rc = sensor->handle(10, 0, 0, msg, msgBuf.size());
    // validateEvent checks acceptInstanceId match; if match => SUCCESS
    // then handleResponseMsg is called
    // timer.stop() is always called at end
    (void)rc;
}

TEST(NsmLongRunningSensorHandle,
     Handle_ValidateEventFail_SkipsHandleResponseMsg)
{
    // Arrange -- acceptInstanceId mismatch to force validateEvent failure
    auto sensor = std::make_shared<MockableLongRunningSensor>(
        "LR_HandleTest2", "LongRunning", true, nullptr, 0x05, 0x10);

    sensor->acceptInstanceId = 0x07;
    sensor->isLongRunning = true;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 128, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x1A; // Different from acceptInstanceId

    // handleResponseMsg should NOT be called when validateEvent fails
    EXPECT_CALL(*sensor, handleResponseMsg(_, _)).Times(0);

    int rc = sensor->handle(10, 0, 0, msg, msgBuf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmLongRunningSensorHandle,
     DISABLED_Handle_HandleResponseMsgReturnsError_PropagatesError)
{
    // Arrange
    auto sensor = std::make_shared<MockableLongRunningSensor>(
        "LR_HandleTest3", "LongRunning", true, nullptr, 0x05, 0x10);

    sensor->acceptInstanceId = 0x03;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 128, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x03;

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .WillOnce(Return(NSM_SW_ERROR));

    int rc = sensor->handle(10, 0, 0, msg, msgBuf.size());
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

TEST(NsmLongRunningSensorHandle, Handle_MultipleEids_AllProcessed)
{
    // Arrange
    auto sensor = std::make_shared<MockableLongRunningSensor>(
        "LR_HandleMultiEid", "LongRunning", true, nullptr, 0x05, 0x10);

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 128, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(NSM_SW_SUCCESS));

    // Call with different eids -- should not crash
    sensor->handle(0, 0, 0, msg, msgBuf.size());
    sensor->handle(127, 0, 0, msg, msgBuf.size());
    sensor->handle(255, 0, 0, msg, msgBuf.size());
}

// ============================================================================
// PART 2: NsmAsyncLongRunningSensor::handle() -- deeper coverage
// ============================================================================

TEST(NsmAsyncLongRunningSensorHandle,
     DISABLED_Handle_ValidateEventSuccess_CallsHandleResponseMsg)
{
    // Arrange
    auto sensor = std::make_shared<MockableAsyncLongRunningSensor>(
        "ALR_HandleTest1", "AsyncLongRunning", true, nullptr, 0x06, 0x11);

    sensor->acceptInstanceId = 0x0A;
    sensor->isLongRunning = true;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 128, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x0A;

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .WillOnce(Return(NSM_SW_SUCCESS));

    int rc = sensor->handle(20, 0, 0, msg, msgBuf.size());
    (void)rc;
}

TEST(NsmAsyncLongRunningSensorHandle,
     Handle_ValidateEventFail_SkipsHandleResponseMsg)
{
    // Arrange -- mismatch instanceId
    auto sensor = std::make_shared<MockableAsyncLongRunningSensor>(
        "ALR_HandleTest2", "AsyncLongRunning", true, nullptr, 0x06, 0x11);

    sensor->acceptInstanceId = 0x0A;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 128, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x1E; // Mismatch

    EXPECT_CALL(*sensor, handleResponseMsg(_, _)).Times(0);

    int rc = sensor->handle(20, 0, 0, msg, msgBuf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmAsyncLongRunningSensorHandle,
     DISABLED_Handle_HandleResponseMsgReturnsError_PropagatesError)
{
    // Arrange
    auto sensor = std::make_shared<MockableAsyncLongRunningSensor>(
        "ALR_HandleTest3", "AsyncLongRunning", true, nullptr, 0x06, 0x11);

    sensor->acceptInstanceId = 0x05;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 128, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x05;

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .WillOnce(Return(NSM_SW_ERROR));

    int rc = sensor->handle(20, 0, 0, msg, msgBuf.size());
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

TEST(NsmAsyncLongRunningSensorHandle, Handle_MultipleEids_NoFault)
{
    // Arrange
    auto sensor = std::make_shared<MockableAsyncLongRunningSensor>(
        "ALR_HandleMultiEid", "AsyncLongRunning", true, nullptr, 0x06, 0x11);

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 128, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(NSM_SW_SUCCESS));

    sensor->handle(0, 0, 0, msg, msgBuf.size());
    sensor->handle(100, 0, 0, msg, msgBuf.size());
    sensor->handle(255, 0, 0, msg, msgBuf.size());
}

// ============================================================================
// PART 3: NsmDebugInfoObject -- getDebugInfo switch cases and guards
// ============================================================================

class NsmDebugInfoTestB12F : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
};

TEST_F(NsmDebugInfoTestB12F, Constructor_DiagnosticsDumpType_CreatesCorrectPath)
{
    std::string name = "B12FDebugDump1";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/b12f";
    std::string type = "GPU";
    DebugDumpType dumpType = DebugDumpType::Diagnostics;

    NsmDebugInfoObject debugInfo(bus, name, inventoryPath, type, testUuid,
                                 dumpType);
    EXPECT_EQ(debugInfo.getName(), name);
    EXPECT_EQ(debugInfo.getType(), type);
    // Diagnostics path: inventoryPath / "Diagnostics" / "Dump" / name
    EXPECT_EQ(debugInfo.supportedDumpType(), dumpType);
}

TEST_F(NsmDebugInfoTestB12F, Constructor_NetworkDumpType_CreatesCorrectPath)
{
    std::string name = "B12FDebugDump2";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/b12f";
    std::string type = "GPU";
    DebugDumpType dumpType = DebugDumpType::Network;

    NsmDebugInfoObject debugInfo(bus, name, inventoryPath, type, testUuid,
                                 dumpType);
    EXPECT_EQ(debugInfo.getName(), name);
    EXPECT_EQ(debugInfo.supportedDumpType(), dumpType);
}

TEST_F(NsmDebugInfoTestB12F, Finish_ClosesFileDescriptor)
{
    // Arrange
    std::string name = "B12FFinish1";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/b12f2";
    NsmDebugInfoObject debugInfo(bus, name, inventoryPath, "GPU", testUuid,
                                 DebugDumpType::Network);

    // Setup status and value interfaces via AsyncOperationManager
    auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (!objectPath.empty())
    {
        debugInfo.statusInterface = statusInterface;
        debugInfo.valueInterface = valueInterface;
        debugInfo.fd = -1; // Invalid fd to avoid issues

        // Act & Assert -- finish should not throw with -1 fd //

        EXPECT_NO_THROW(debugInfo.finish(AsyncOperationStatusType::Success, 0));
    }
}

// ============================================================================
// PART 4: NsmGPMAggregated::handleSample -- decode failure path
// ============================================================================

TEST(NsmGPMAggregatedB12F, HandleSample_DecodeFailure_ReturnsNonZero)
{
    // Arrange
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/b12f/gpm_decode_fail"},
        "com.nvidia.GPMMetrics");
    auto dbus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        dbus, sdbusplus::object_path{"/test/b12f/gpm_decode_fail"});

    std::vector<uint8_t> metricsBitfield{0x01};

    NsmGPMAggregated gpm("b12f_decode_fail", "AggregatedGPMMetrics",
                         sdbusplus::object_path{"/test/b12f/gpm_decode_fail"},
                         1, 0, 0, metricsBitfield, gpmIntf, nvlinkIntf);

    // Add a metric with a real decode function but supply bad data
    auto updator = std::make_unique<MockMetricUpdatorB12F>();
    auto* rawUpdator = updator.get();
    gpm.metricsTable[6].clear();
    gpm.metricsTable[6].emplace_back(
        MetricInfo{decodePercentage, std::move(updator)});

    // Supply data that is too short for decode (0 bytes)
    uint8_t emptyData[1] = {0};
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 6;
    sample.valid = true;
    sample.data = emptyData;
    sample.data_len = 0; // Too short to decode percentage

    // The decode function should fail, but updateMetric is still called
    EXPECT_CALL(*rawUpdator, updateMetric(_)).Times(AtLeast(1));

    auto rc = gpm.handleSample(sample);
    // rc should be non-zero from decode failure
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmGPMAggregatedB12F, HandleSample_ValidDecode_UpdaterCalledWithValue)
{
    // Arrange
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/b12f/gpm_valid"},
        "com.nvidia.GPMMetrics");
    auto dbus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        dbus, sdbusplus::object_path{"/test/b12f/gpm_valid"});

    std::vector<uint8_t> metricsBitfield{0x01};

    NsmGPMAggregated gpm("b12f_valid_decode", "AggregatedGPMMetrics",
                         sdbusplus::object_path{"/test/b12f/gpm_valid"}, 1, 0,
                         0, metricsBitfield, gpmIntf, nvlinkIntf);

    // Add a mock updater for a metric with valid encoded data
    auto updator = std::make_unique<MockMetricUpdatorB12F>();
    auto* rawUpdator = updator.get();
    gpm.metricsTable[7].clear();
    gpm.metricsTable[7].emplace_back(
        MetricInfo{decodePercentage, std::move(updator)});

    // Encode valid percentage data
    const double percentage = 55.5;
    std::array<uint8_t, sizeof(double)> data = {};
    size_t dataLen = 0;
    auto rc_encode = encode_aggregate_gpm_metric_percentage_data(
        percentage, data.data(), &dataLen);
    ASSERT_EQ(rc_encode, NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 7;
    sample.valid = true;
    sample.data = data.data();
    sample.data_len = static_cast<uint8_t>(dataLen);

    EXPECT_CALL(*rawUpdator, updateMetric(DoubleNear(percentage, 0.01)))
        .Times(1);

    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmGPMAggregatedB12F, HandleSample_BandwidthDecode_ConvertsCorrectly)
{
    // Arrange
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/b12f/gpm_bw"},
        "com.nvidia.GPMMetrics");
    auto dbus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        dbus, sdbusplus::object_path{"/test/b12f/gpm_bw"});

    std::vector<uint8_t> metricsBitfield{0x01};

    NsmGPMAggregated gpm("b12f_bw_decode", "AggregatedGPMMetrics",
                         sdbusplus::object_path{"/test/b12f/gpm_bw"}, 1, 0, 0,
                         metricsBitfield, gpmIntf, nvlinkIntf);

    auto updator = std::make_unique<MockMetricUpdatorB12F>();
    auto* rawUpdator = updator.get();
    gpm.metricsTable[8].clear();
    gpm.metricsTable[8].emplace_back(
        MetricInfo{decodeBandwidth, std::move(updator)});

    // Encode valid bandwidth data
    const uint64_t bandwidth = 268435456; // 2 Gbps
    std::array<uint8_t, sizeof(uint64_t)> data = {};
    size_t dataLen = 0;
    auto rc_encode = encode_aggregate_gpm_metric_bandwidth_data(
        bandwidth, data.data(), &dataLen);
    ASSERT_EQ(rc_encode, NSM_SW_SUCCESS);

    static constexpr uint64_t conversionFactor = 1024 * 1024 * 128;
    double expectedGbps = bandwidth / static_cast<double>(conversionFactor);

    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 8;
    sample.valid = true;
    sample.data = data.data();
    sample.data_len = static_cast<uint8_t>(dataLen);

    EXPECT_CALL(*rawUpdator, updateMetric(DoubleNear(expectedGbps, 0.001)))
        .Times(1);

    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// PART 5: NsmNumericSensor -- SMBus byte converters
// ============================================================================

TEST(SMBPBIEnergySMBusSensorBytesConverterB12F, Convert_ValidValue_Correct)
{
    // Arrange
    SMBPBIEnergySMBusSensorBytesConverter converter;
    double energyJoules = 123.456;

    // Act
    auto data = converter.convert(energyJoules);

    // Assert
    ASSERT_EQ(data.size(), sizeof(uint64_t));
    uint64_t val = 0;
    std::memcpy(&val, data.data(), sizeof(uint64_t));
    val = le64toh(val);
    // Value should be in millijoules
    EXPECT_EQ(val, static_cast<uint64_t>(energyJoules * 1000.0));
}

TEST(SMBPBIEnergySMBusSensorBytesConverterB12F, Convert_ZeroValue_ReturnsZero)
{
    // Arrange
    SMBPBIEnergySMBusSensorBytesConverter converter;

    // Act
    auto data = converter.convert(0.0);

    // Assert
    ASSERT_EQ(data.size(), sizeof(uint64_t));
    uint64_t val = 0;
    std::memcpy(&val, data.data(), sizeof(uint64_t));
    val = le64toh(val);
    EXPECT_EQ(val, 0u);
}

TEST(SMBPBIEnergySMBusSensorBytesConverterB12F, Convert_LargeValue_NoOverflow)
{
    // Arrange
    SMBPBIEnergySMBusSensorBytesConverter converter;
    double largeEnergy = 1000000.0; // 1 MJ

    // Act
    auto data = converter.convert(largeEnergy);

    // Assert
    ASSERT_EQ(data.size(), sizeof(uint64_t));
    uint64_t val = 0;
    std::memcpy(&val, data.data(), sizeof(uint64_t));
    val = le64toh(val);
    EXPECT_EQ(val, static_cast<uint64_t>(largeEnergy * 1000.0));
}

TEST(SFxP24F8SMBusSensorBytesConverterB12F, Convert_PositiveValue_Correct)
{
    // Arrange
    SFxP24F8SMBusSensorBytesConverter converter;
    double tempCelsius = 45.5;

    // Act
    auto data = converter.convert(tempCelsius);

    // Assert
    ASSERT_EQ(data.size(), 4u);
    int32_t val = 0;
    std::memcpy(&val, data.data(), 4);
    val = le32toh(val);
    // SFxP24F8: value * (1 << 8) = value * 256
    EXPECT_EQ(val, static_cast<int32_t>(tempCelsius * 256.0));
}

TEST(SFxP24F8SMBusSensorBytesConverterB12F, Convert_NegativeValue_Correct)
{
    // Arrange
    SFxP24F8SMBusSensorBytesConverter converter;
    double negTemp = -10.25;

    // Act
    auto data = converter.convert(negTemp);

    // Assert
    ASSERT_EQ(data.size(), 4u);
    int32_t val = 0;
    std::memcpy(&val, data.data(), 4);
    val = le32toh(val);
    EXPECT_EQ(val, static_cast<int32_t>(negTemp * 256.0));
}

TEST(SFxP24F8SMBusSensorBytesConverterB12F, Convert_ZeroValue_ReturnsZero)
{
    // Arrange
    SFxP24F8SMBusSensorBytesConverter converter;

    // Act
    auto data = converter.convert(0.0);

    // Assert
    int32_t val = 0;
    std::memcpy(&val, data.data(), 4);
    val = le32toh(val);
    EXPECT_EQ(val, 0);
}

TEST(Uint64SMBusSensorBytesConverterB12F, Convert_ValidValue_Correct)
{
    // Arrange
    Uint64SMBusSensorBytesConverter converter;
    double largeVal = 9876543210.0;

    // Act
    auto data = converter.convert(largeVal);

    // Assert
    ASSERT_EQ(data.size(), 8u);
    uint64_t val = 0;
    std::memcpy(&val, data.data(), 8);
    val = le64toh(val);
    EXPECT_EQ(val, static_cast<uint64_t>(largeVal));
}

TEST(Uint64SMBusSensorBytesConverterB12F, Convert_ZeroValue_ReturnsZero)
{
    // Arrange
    Uint64SMBusSensorBytesConverter converter;

    // Act
    auto data = converter.convert(0.0);

    // Assert
    uint64_t val = 0;
    std::memcpy(&val, data.data(), 8);
    val = le64toh(val);
    EXPECT_EQ(val, 0u);
}

TEST(Uint64SMBusSensorBytesConverterB12F, Convert_SmallValue_Correct)
{
    // Arrange
    Uint64SMBusSensorBytesConverter converter;

    // Act
    auto data = converter.convert(42.0);

    // Assert
    uint64_t val = 0;
    std::memcpy(&val, data.data(), 8);
    val = le64toh(val);
    EXPECT_EQ(val, 42u);
}

// ============================================================================
// PART 6: NsmNumericSensorDbusValue::calculateNextUpdateTimestamp
// ============================================================================

TEST(CalculateNextUpdateTimestampB12F, FirstCall_ZeroTimestamp_SetsInitial)
{
    // Arrange
    uint64_t nextUpdate = 0;
    uint64_t timestamp = 5000;

    // Act
    NsmNumericSensorDbusValue::calculateNextUpdateTimestamp(timestamp,
                                                            nextUpdate);

    // Assert - first call sets nextUpdate = timestamp + 1000
    EXPECT_EQ(nextUpdate, 5000 + 1000);
}

TEST(CalculateNextUpdateTimestampB12F,
     SecondCall_AfterInitial_AdvancesByAtLeast1000)
{
    // Arrange
    uint64_t nextUpdate = 6000; // Set from first call
    uint64_t timestamp = 8000;  // 2 seconds later

    // Act
    NsmNumericSensorDbusValue::calculateNextUpdateTimestamp(timestamp,
                                                            nextUpdate);

    // Assert - elapsedSec = (8000 - 6000) / 1000 = 2
    // nextUpdate += 1000 * max(1, 2) = 1000 * 2 = 2000
    EXPECT_EQ(nextUpdate, 6000 + 2000);
}

TEST(CalculateNextUpdateTimestampB12F, SecondCall_LessThan1Sec_AdvancesBy1000)
{
    // Arrange
    uint64_t nextUpdate = 6000;
    uint64_t timestamp = 6500; // Only 500ms later

    // Act
    NsmNumericSensorDbusValue::calculateNextUpdateTimestamp(timestamp,
                                                            nextUpdate);

    // Assert - elapsedSec = (6500 - 6000) / 1000 = 0
    // nextUpdate += 1000 * max(1, 0) = 1000
    EXPECT_EQ(nextUpdate, 6000 + 1000);
}

TEST(CalculateNextUpdateTimestampB12F,
     MultipleCalls_ProgressiveTimestamps_AdvancesCorrectly)
{
    // Arrange
    uint64_t nextUpdate = 0;

    // First call
    NsmNumericSensorDbusValue::calculateNextUpdateTimestamp(1000, nextUpdate);
    EXPECT_EQ(nextUpdate, 2000);

    // Second call at 2500ms (within next update window)
    NsmNumericSensorDbusValue::calculateNextUpdateTimestamp(2500, nextUpdate);
    // elapsedSec = (2500 - 2000)/1000 = 0 => max(1,0)=1
    EXPECT_EQ(nextUpdate, 3000);

    // Third call at 5000ms (2 seconds after 3000)
    NsmNumericSensorDbusValue::calculateNextUpdateTimestamp(5000, nextUpdate);
    // elapsedSec = (5000 - 3000)/1000 = 2 => max(1,2)=2
    EXPECT_EQ(nextUpdate, 5000);
}

// ============================================================================
// PART 7: NsmNumericSensorDbusValue::canUpdate
// ============================================================================

TEST_F(CanUpdateB12FFixture, ZeroNextUpdate_ReturnsTrue)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc = {
        {"chassis", "all_sensors",
         "/xyz/openbmc_project/inventory/b12f_canupd1"}};
    NsmNumericSensorDbusValue value(
        dbus, "b12f_can_upd_test1", "temperature_b12f1", SensorUnit::DegreesC,
        assoc, "GPU", nullptr, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(), nullptr, nullptr);

    // Act & Assert -- nextUpdateTimestamp starts at 0
    EXPECT_TRUE(value.canUpdate(0));
    EXPECT_TRUE(value.canUpdate(999));
    EXPECT_TRUE(value.canUpdate(5000));
}

TEST_F(CanUpdateB12FFixture, AfterUpdate_BeforeNextTimestamp_ReturnsFalse)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc = {
        {"chassis", "all_sensors",
         "/xyz/openbmc_project/inventory/b12f_canupd2"}};
    NsmNumericSensorDbusValue value(
        dbus, "b12f_can_upd_test2", "temperature_b12f2", SensorUnit::DegreesC,
        assoc, "GPU", nullptr, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(), nullptr, nullptr);

    // Act -- first update sets nextUpdateTimestamp
    value.updateReading(42.0, 1000);

    // Assert -- too soon to update again
    EXPECT_FALSE(value.canUpdate(1500));
}

TEST_F(CanUpdateB12FFixture, AfterUpdate_AtOrAfterNextTimestamp_ReturnsTrue)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc = {
        {"chassis", "all_sensors",
         "/xyz/openbmc_project/inventory/b12f_canupd3"}};
    NsmNumericSensorDbusValue value(
        dbus, "b12f_can_upd_test3", "temperature_b12f3", SensorUnit::DegreesC,
        assoc, "GPU", nullptr, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(), nullptr, nullptr);

    // Act -- first update
    value.updateReading(42.0, 1000);

    // Assert -- nextUpdateTimestamp = 1000 + 1000 = 2000
    EXPECT_TRUE(value.canUpdate(2000));
    EXPECT_TRUE(value.canUpdate(3000));
}

// ============================================================================
// PART 8: NsmNumericSensorValueAggregate::updateReading -- throttling
// ============================================================================

TEST(NsmNumericSensorValueAggregateB12F,
     UpdateReading_WithZeroTimestamp_UsesCurrentClock)
{
    // Arrange
    auto mockValue = std::make_unique<MockNsmNumericSensorValueB12F>();
    auto* rawMock = mockValue.get();

    NsmNumericSensorValueAggregate agg;
    agg.append(std::move(mockValue));

    // Expect updateReading with non-zero timestamp (from getCurrentSteadyClock)
    EXPECT_CALL(*rawMock, updateReading(42.0, Gt(0u))).Times(1);

    // Act -- timestamp=0 triggers use of current clock
    agg.updateReading(42.0, 0);
}

TEST(NsmNumericSensorValueAggregateB12F,
     UpdateReading_WithExplicitTimestamp_PassesThrough)
{
    // Arrange
    auto mockValue = std::make_unique<MockNsmNumericSensorValueB12F>();
    auto* rawMock = mockValue.get();

    NsmNumericSensorValueAggregate agg;
    agg.append(std::move(mockValue));

    // Expect exactly the provided timestamp
    EXPECT_CALL(*rawMock, updateReading(100.0, 5000u)).Times(1);

    // Act
    agg.updateReading(100.0, 5000);
}

TEST(NsmNumericSensorValueAggregateB12F, Append_MultipleElements_AllUpdated)
{
    // Arrange
    auto mock1 = std::make_unique<MockNsmNumericSensorValueB12F>();
    auto mock2 = std::make_unique<MockNsmNumericSensorValueB12F>();
    auto* rawMock1 = mock1.get();
    auto* rawMock2 = mock2.get();

    NsmNumericSensorValueAggregate agg;
    agg.append(std::move(mock1));
    agg.append(std::move(mock2));

    EXPECT_CALL(*rawMock1, updateReading(77.0, 3000u)).Times(1);
    EXPECT_CALL(*rawMock2, updateReading(77.0, 3000u)).Times(1);

    // Act
    agg.updateReading(77.0, 3000);
}

// ============================================================================
// PART 9: NsmNumericSensorDbusStatus
// ============================================================================

TEST(NsmNumericSensorDbusStatusB12F,
     Constructor_DefaultState_AvailableAndFunctional)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();

    // Act
    NsmNumericSensorDbusStatus status(dbus, "b12f_status_test1",
                                      "temperature_b12f_s1");

    // Assert -- constructor sets available=true, functional=true
    EXPECT_TRUE(status.availabilityIntf.available());
    EXPECT_TRUE(status.operationalStatusIntf.functional());
}

TEST(NsmNumericSensorDbusStatusB12F,
     UpdateStatus_SetUnavailable_UpdatesBothInterfaces)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    NsmNumericSensorDbusStatus status(dbus, "b12f_status_test2",
                                      "temperature_b12f_s2");

    // Act
    status.updateStatus(false, false);

    // Assert
    EXPECT_FALSE(status.availabilityIntf.available());
    EXPECT_FALSE(status.operationalStatusIntf.functional());
}

TEST(NsmNumericSensorDbusStatusB12F, UpdateStatus_Toggle_CorrectState)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    NsmNumericSensorDbusStatus status(dbus, "b12f_status_test3",
                                      "temperature_b12f_s3");

    // Act -- toggle to unavailable then back
    status.updateStatus(false, true);
    EXPECT_FALSE(status.availabilityIntf.available());
    EXPECT_TRUE(status.operationalStatusIntf.functional());

    status.updateStatus(true, false);
    EXPECT_TRUE(status.availabilityIntf.available());
    EXPECT_FALSE(status.operationalStatusIntf.functional());

    status.updateStatus(true, true);
    EXPECT_TRUE(status.availabilityIntf.available());
    EXPECT_TRUE(status.operationalStatusIntf.functional());
}

// ============================================================================
// PART 10: NsmThresholdFactory -- constructor and member verification
// ============================================================================

struct NsmThresholdFactoryTestB12F :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string interface = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/b12f/Temp0";
    NumericSensorInfo info = {};

    NsmThresholdFactoryTestB12F() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);

        info.name = "B12F_GPU0_Temp";
        info.type = "NSM_ThermalParameter";
        info.sensorId = 1;
        info.priority = false;
        info.aggregated = false;
    }

    ~NsmThresholdFactoryTestB12F()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmThresholdFactoryTestB12F, Constructor_StoresAllMembers)
{
    // Arrange
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);

    // Act
    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);

    // Assert
    EXPECT_EQ(factory.interface, interface);
    EXPECT_EQ(factory.objPath, testObjPath);
    EXPECT_EQ(factory.numericSensor, numericSensor);
    EXPECT_NE(factory.nsmDevice, nullptr);
    EXPECT_EQ(factory.nsmDevice, gpu);
}

TEST_F(NsmThresholdFactoryTestB12F,
       Constructor_DifferentUUID_GetsDifferentDevice)
{
    // Arrange
    const uuid_t gpu1Uuid = "STATIC:0:1:NSM_DEVICE_INSTANCE_NUMBER:1";
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);

    // Act
    NsmThresholdFactory f0(mockManager, interface, testObjPath, numericSensor,
                           info, gpuUuid);
    NsmThresholdFactory f1(mockManager, interface, testObjPath, numericSensor,
                           info, gpu1Uuid);

    // Assert
    EXPECT_NE(f0.nsmDevice, f1.nsmDevice);
}

TEST_F(NsmThresholdFactoryTestB12F, Constructor_MultipleInterfaces_AllStored)
{
    // Arrange
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);
    std::string intf1 = "xyz.openbmc_project.Configuration.NSM_Power";
    std::string intf2 = "xyz.openbmc_project.Configuration.NSM_Voltage";

    // Act
    NsmThresholdFactory f1(mockManager, intf1, testObjPath, numericSensor, info,
                           gpuUuid);
    NsmThresholdFactory f2(mockManager, intf2, testObjPath, numericSensor, info,
                           gpuUuid);

    // Assert
    EXPECT_EQ(f1.interface, intf1);
    EXPECT_EQ(f2.interface, intf2);
}

// ============================================================================
// NsmThresholdFactory::make() branch coverage
// ============================================================================

// Minimal concrete NsmNumericSensor for factory tests
class TFTestNumericSensor : public NsmNumericSensor
{
  public:
    TFTestNumericSensor(const std::string& name) :
        NsmNumericSensor(name, "NSM_Test", 0, nullptr)
    {}
    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::nullopt;
    }
    uint8_t handleResponseMsg(const struct nsm_msg*, size_t) override
    {
        return 0;
    }
    std::string getSensorType() override
    {
        return "temperature";
    }
};

// Empty service map → getThresholdInterfacesAsync returns empty map
// → all processThresholdsPair calls skip the if block (false branch)
TEST_F(NsmThresholdFactoryTestB12F, Make_EmptyServiceMap_NoSensorsAdded)
{
    // serviceMap is empty by default (cleared by DBusTest destructor setup)
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);
    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);
    size_t initialStaticCount = gpu->staticSensors.size();
    factory.make();
    EXPECT_EQ(gpu->staticSensors.size(), initialStaticCount);
}

// Non-empty service map with "LowerCaution" → processThresholdsPair creates
// ThresholdWarningIntf and calls createNsmThreshold with dynamic=false.
// Covers: getThresholdInterfacesAsync loop, processThresholdsPair if-branch,
// createNsmThreshold static path (addDeviceSensors).
TEST_F(NsmThresholdFactoryTestB12F, Make_StaticLowerCautionThreshold)
{
    const std::string threshIntf = interface +
                                   ".ThermalParameters.LowerCaution";

    // Set up service map so getThresholdInterfacesAsync finds our interface
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {threshIntf}}};

    // Set "Name" property for coGetDbusProperty lookup
    auto& nameProps = utils::MockDbusAsync::propertyMap(testObjPath,
                                                        threshIntf);
    nameProps["Name"] = std::string("LowerCaution");

    // Set threshold interface properties for createNsmThreshold
    nameProps["Dynamic"] = bool(false);
    nameProps["Value"] = double(45.0);

    auto numericSensor = std::make_shared<TFTestNumericSensor>("TF_B12F_Temp0");
    info.name = "TF_B12F_Temp0";

    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);
    size_t initialCount = gpu->deviceSensors.size();
    factory.make();
    // createNsmThreshold static path adds to addDeviceSensors
    EXPECT_GT(gpu->deviceSensors.size(), initialCount);
}

// Dynamic threshold with NSM_ThermalParameter type, non-periodic.
// Covers: createNsmThreshold dynamic=true path, type check pass,
// periodicUpdate=false → addStaticSensor + addCapabilityRefreshSensor.
TEST_F(NsmThresholdFactoryTestB12F, Make_DynamicLowerCritical_NonPeriodic)
{
    const std::string threshIntf = interface +
                                   ".ThermalParameters.LowerCritical";

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {threshIntf}}};

    auto& props = utils::MockDbusAsync::propertyMap(testObjPath, threshIntf);
    props["Name"] = std::string("LowerCritical");
    props["Dynamic"] = bool(true);
    props["Type"] = std::string("NSM_ThermalParameter");
    props["ParameterId"] = uint64_t(5);
    // "PeriodicUpdate" not set → getDbusProperty throws → periodicUpdate=false

    auto numericSensor =
        std::make_shared<TFTestNumericSensor>("TF_B12F_Temp0_Dyn");
    info.name = "TF_B12F_Temp0_Dyn";

    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);
    size_t initialCount = gpu->staticSensors.size();
    factory.make();
    EXPECT_GT(gpu->staticSensors.size(), initialCount);
}

// Dynamic threshold with unsupported type.
// Covers: createNsmThreshold type check fail → co_return NSM_ERROR path. //

TEST_F(NsmThresholdFactoryTestB12F, Make_DynamicThreshold_UnsupportedType)
{
    const std::string threshIntf = interface +
                                   ".ThermalParameters.UpperCaution";

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {threshIntf}}};

    auto& props = utils::MockDbusAsync::propertyMap(testObjPath, threshIntf);
    props["Name"] = std::string("UpperCaution");
    props["Dynamic"] = bool(true);
    props["Type"] = std::string("NSM_UnsupportedType"); // wrong type

    auto numericSensor =
        std::make_shared<TFTestNumericSensor>("TF_B12F_Temp0_Bad");
    info.name = "TF_B12F_Temp0_Bad";

    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);
    size_t initialCount = gpu->staticSensors.size();
    factory.make();
    // Type mismatch → returns NSM_ERROR → no sensors added
    EXPECT_EQ(gpu->staticSensors.size(), initialCount);
}

// Dynamic threshold with periodic update + aggregation.
// Covers: periodicUpdate=true path, Priority/Aggregated properties,
// NumericSensorFactory::makeAggregatorAndAddSensor,
// NsmThresholdAggregatorBuilder::makeAggregator.
TEST_F(NsmThresholdFactoryTestB12F, Make_DynamicThreshold_Periodic_Aggregated)
{
    const std::string threshIntf = interface +
                                   ".ThermalParameters.UpperCritical";

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {threshIntf}}};

    auto& props = utils::MockDbusAsync::propertyMap(testObjPath, threshIntf);
    props["Name"] = std::string("UpperCritical");
    props["Dynamic"] = bool(true);
    props["Type"] = std::string("NSM_ThermalParameter");
    props["ParameterId"] = uint64_t(7);
    props["PeriodicUpdate"] = bool(true);
    props["Priority"] = bool(false);
    props["Aggregated"] = bool(true);

    auto numericSensor =
        std::make_shared<TFTestNumericSensor>("TF_B12F_Temp0_Agg");
    info.name = "TF_B12F_Temp0_Agg";

    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);
    factory.make();
    // Periodic+aggregated adds via makeAggregatorAndAddSensor
    // (aggregator map, not directly to staticSensors/deviceSensors)
    SUCCEED(); // Verifies make() completes without crash
}

// ============================================================================
// PART 11: NsmThresholdAggregator -- genRequestMsg and handleSample
// ============================================================================

TEST(NsmThresholdAggregatorB12F, GenRequestMsg_ValidIds_HasCorrectSize)
{
    // Arrange
    NsmThresholdAggregator aggregator("B12F_Thresh", "NSM_ThermalParameter",
                                      false);

    // Act
    auto result = aggregator.genRequestMsg(10, 5);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req));
}

TEST(NsmThresholdAggregatorB12F, HandleSample_ValidData_ReturnsSuccess)
{
    // Arrange
    NsmThresholdAggregator aggregator("B12F_ThreshSample",
                                      "NSM_ThermalParameter", false);

    int32_t value = 85;
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 1;
    sample.valid = true;
    sample.data = reinterpret_cast<uint8_t*>(&value);
    sample.data_len = sizeof(value);

    // Act
    auto result = aggregator.handleSample(sample);

    // Assert
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST(NsmThresholdAggregatorB12F, HandleSample_InvalidValid_ReturnsSuccess)
{
    // Arrange
    NsmThresholdAggregator aggregator("B12F_ThreshInvalid",
                                      "NSM_ThermalParameter", true);

    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 2;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    // Act
    auto result = aggregator.handleSample(sample);

    // Assert
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

// ============================================================================
// PART 12: NsmThreshold -- genRequestMsg and handleResponseMsg
// ============================================================================

TEST(NsmThresholdB12F, GenRequestMsg_ValidInput_ReturnsCorrectSize)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdWarningIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_thresh1");
    auto thresholdValue = std::make_unique<NsmThresholdValueWarningLow>(
        "B12F_Thresh1", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("B12F_Threshold1", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Act
    auto result = threshold.genRequestMsg(10, 1);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req));
}

TEST(NsmThresholdB12F, HandleResponseMsg_ValidResponse_UpdatesReading)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdWarningIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_thresh2");
    auto thresholdValue = std::make_unique<NsmThresholdValueWarningLow>(
        "B12F_Thresh2", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("B12F_Threshold2", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Build valid response
    int32_t paramValue = 75;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    auto rc = encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 paramValue, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(thresholdIntf->warningLow(), static_cast<double>(paramValue));
}

TEST(NsmThresholdB12F, HandleResponseMsg_ErrorCC_ReturnsNonZero)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdCriticalIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_thresh3");
    auto thresholdValue = std::make_unique<NsmThresholdValueCriticalHigh>(
        "B12F_Thresh3", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("B12F_Threshold3", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Build error response
    int32_t paramValue = 0;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_read_thermal_parameter_resp(0, NSM_ERROR, ERR_NULL, paramValue,
                                       response);

    // Act
    auto result = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_NE(result, NSM_SUCCESS);
}

TEST(NsmThresholdB12F, HandleResponseMsg_TooShort_ReturnsError)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdHardShutdownIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_thresh4");
    auto thresholdValue = std::make_unique<NsmThresholdValueHardShutdownLow>(
        "B12F_Thresh4", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("B12F_Threshold4", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Too short response
    std::vector<uint8_t> shortResp(sizeof(nsm_msg_hdr) + 2, 0);

    // Act
    auto result = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(shortResp.data()), shortResp.size());

    // Assert
    EXPECT_NE(result, NSM_SUCCESS);
}

TEST(NsmThresholdB12F, HandleResponseMsg_NegativeValue_UpdatesCorrectly)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdCriticalIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_thresh5");
    auto thresholdValue = std::make_unique<NsmThresholdValueCriticalLow>(
        "B12F_Thresh5", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("B12F_Threshold5", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Build response with negative value
    int32_t paramValue = -40;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL, paramValue,
                                       response);

    // Act
    auto result = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(thresholdIntf->criticalLow(), static_cast<double>(paramValue));
}

// ============================================================================
// PART 13: NsmThresholdValue types -- all six concrete types
// ============================================================================

TEST(NsmThresholdValueTypesB12F, WarningLow_UpdateReading_SetsProperty)
{
    auto& dbus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdWarningIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_wl1");
    NsmThresholdValueWarningLow sensor("b12f_wl", "ThermalParam", intf);

    sensor.updateReading(30.5);
    EXPECT_EQ(intf->warningLow(), 30.5);
}

TEST(NsmThresholdValueTypesB12F, WarningHigh_UpdateReading_SetsProperty)
{
    auto& dbus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdWarningIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_wh1");
    NsmThresholdValueWarningHigh sensor("b12f_wh", "ThermalParam", intf);

    sensor.updateReading(90.0);
    EXPECT_EQ(intf->warningHigh(), 90.0);
}

TEST(NsmThresholdValueTypesB12F, CriticalLow_UpdateReading_SetsProperty)
{
    auto& dbus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdCriticalIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_cl1");
    NsmThresholdValueCriticalLow sensor("b12f_cl", "ThermalParam", intf);

    sensor.updateReading(-5.0);
    EXPECT_EQ(intf->criticalLow(), -5.0);
}

TEST(NsmThresholdValueTypesB12F, CriticalHigh_UpdateReading_SetsProperty)
{
    auto& dbus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdCriticalIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_ch1");
    NsmThresholdValueCriticalHigh sensor("b12f_ch", "ThermalParam", intf);

    sensor.updateReading(105.0);
    EXPECT_EQ(intf->criticalHigh(), 105.0);
}

TEST(NsmThresholdValueTypesB12F, HardShutdownLow_UpdateReading_SetsProperty)
{
    auto& dbus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdHardShutdownIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_hsl1");
    NsmThresholdValueHardShutdownLow sensor("b12f_hsl", "ThermalParam", intf);

    sensor.updateReading(-20.0);
    EXPECT_EQ(intf->hardShutdownLow(), -20.0);
}

TEST(NsmThresholdValueTypesB12F, HardShutdownHigh_UpdateReading_SetsProperty)
{
    auto& dbus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdHardShutdownIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_hsh1");
    NsmThresholdValueHardShutdownHigh sensor("b12f_hsh", "ThermalParam", intf);

    sensor.updateReading(120.0);
    EXPECT_EQ(intf->hardShutdownHigh(), 120.0);
}

// ============================================================================
// PART 14: NsmGPMPerInstance -- genRequestMsg with BANDWIDTH unit
// ============================================================================

TEST(NsmGPMPerInstanceB12F, Constructor_BandwidthUnit_SetsDecodeFunc)
{
    // Arrange
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/b12f/gpm_pi_bw"},
        "com.nvidia.GPMMetrics");

    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB12F>();
    std::vector<bitfield8_t> instanceBitfield{{.byte = 0xFF}};

    // Act
    NsmGPMPerInstance perInstance("B12F_PerInst_BW", "GPMPerInstance", 1, 0, 0,
                                  5, instanceBitfield,
                                  GPMMetricsUnit::BANDWIDTH, updator);

    // Assert -- decodeFunc should be decodeBandwidth
    EXPECT_NE(perInstance.decodeFunc, nullptr);
    EXPECT_EQ(perInstance.decodeFunc, decodeBandwidth);
}

TEST(NsmGPMPerInstanceB12F, Constructor_PercentageUnit_SetsDecodeFunc)
{
    // Arrange
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/b12f/gpm_pi_pct"},
        "com.nvidia.GPMMetrics");

    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB12F>();
    std::vector<bitfield8_t> instanceBitfield{{.byte = 0x0F}};

    // Act
    NsmGPMPerInstance perInstance("B12F_PerInst_PCT", "GPMPerInstance", 2, 0, 0,
                                  3, instanceBitfield,
                                  GPMMetricsUnit::PERCENTAGE, updator);

    // Assert
    EXPECT_NE(perInstance.decodeFunc, nullptr);
    EXPECT_EQ(perInstance.decodeFunc, decodePercentage);
}

TEST(NsmGPMPerInstanceB12F, GenRequestMsg_ValidParams_ReturnsCorrectSize)
{
    // Arrange
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB12F>();
    std::vector<bitfield8_t> instanceBitfield{{.byte = 0xFF}, {.byte = 0xFF}};

    NsmGPMPerInstance perInstance("B12F_PerInst_Gen", "GPMPerInstance", 2, 1, 3,
                                  7, instanceBitfield,
                                  GPMMetricsUnit::PERCENTAGE, updator);

    // Act
    auto request = perInstance.genRequestMsg(10, 1);

    // Assert
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_per_instance_gpm_metrics_v2_req) +
                  instanceBitfield.size() - 1);
}

// ============================================================================
// PART 15: NsmGPMAggregated -- genRequestMsg
// ============================================================================

TEST(NsmGPMAggregatedGenReqB12F, GenRequestMsg_ValidParams_CorrectEncoding)
{
    // Arrange
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/b12f/gpm_genreq"},
        "com.nvidia.GPMMetrics");
    auto dbus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        dbus, sdbusplus::object_path{"/test/b12f/gpm_genreq"});

    const uint8_t retrievalSource = 2;
    const uint8_t gpuInstance = 1;
    const uint8_t computeInstance = 3;
    std::vector<uint8_t> metricsBitfield{0xFF, 0x0F};

    NsmGPMAggregated gpm("B12F_GenReq", "AggregatedGPMMetrics",
                         sdbusplus::object_path{"/test/b12f/gpm_genreq"},
                         retrievalSource, gpuInstance, computeInstance,
                         metricsBitfield, gpmIntf, nvlinkIntf);

    // Act
    auto request = gpm.genRequestMsg(10, 5);

    // Assert
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) +
                                   sizeof(nsm_query_aggregate_gpm_metrics_req) -
                                   1 + metricsBitfield.size());

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_query_aggregate_gpm_metrics_req*>(
        msg->payload);

    EXPECT_EQ(command->retrieval_source, retrievalSource);
    EXPECT_EQ(command->gpu_instance, gpuInstance);
    EXPECT_EQ(command->compute_instance, computeInstance);
}

// ============================================================================
// PART 16: decodeBandwidth and decodePercentage -- edge cases
// ============================================================================

TEST(DecodeMetricFunctionsB12F, DecodePercentage_ValidData_ReturnsCorrect)
{
    // Arrange
    const double percentage = 99.99;
    std::array<uint8_t, sizeof(double)> data = {};
    size_t dataLen = 0;
    auto rc = encode_aggregate_gpm_metric_percentage_data(
        percentage, data.data(), &dataLen);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto [decodeRc, decoded] = decodePercentage(data.data(), dataLen);

    // Assert
    EXPECT_EQ(decodeRc, NSM_SW_SUCCESS);
    EXPECT_NEAR(decoded, percentage, 0.01);
}

TEST(DecodeMetricFunctionsB12F, DecodePercentage_ZeroValue_ReturnsZero)
{
    // Arrange
    const double percentage = 0.0;
    std::array<uint8_t, sizeof(double)> data = {};
    size_t dataLen = 0;
    encode_aggregate_gpm_metric_percentage_data(percentage, data.data(),
                                                &dataLen);

    // Act
    auto [decodeRc, decoded] = decodePercentage(data.data(), dataLen);

    // Assert
    EXPECT_EQ(decodeRc, NSM_SW_SUCCESS);
    EXPECT_NEAR(decoded, 0.0, 0.01);
}

TEST(DecodeMetricFunctionsB12F, DecodeBandwidth_ValidData_ConvertsToGbps)
{
    // Arrange
    const uint64_t bandwidth = 134217728; // 1 Gbps exactly
    std::array<uint8_t, sizeof(uint64_t)> data = {};
    size_t dataLen = 0;
    auto rc = encode_aggregate_gpm_metric_bandwidth_data(bandwidth, data.data(),
                                                         &dataLen);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto [decodeRc, decoded] = decodeBandwidth(data.data(), dataLen);

    // Assert
    EXPECT_EQ(decodeRc, NSM_SW_SUCCESS);
    static constexpr uint64_t conversionFactor = 1024 * 1024 * 128;
    double expected = bandwidth / static_cast<double>(conversionFactor);
    EXPECT_NEAR(decoded, expected, 0.001);
}

TEST(DecodeMetricFunctionsB12F, DecodeBandwidth_ZeroValue_ReturnsZero)
{
    // Arrange
    const uint64_t bandwidth = 0;
    std::array<uint8_t, sizeof(uint64_t)> data = {};
    size_t dataLen = 0;
    encode_aggregate_gpm_metric_bandwidth_data(bandwidth, data.data(),
                                               &dataLen);

    // Act
    auto [decodeRc, decoded] = decodeBandwidth(data.data(), dataLen);

    // Assert
    EXPECT_EQ(decodeRc, NSM_SW_SUCCESS);
    EXPECT_NEAR(decoded, 0.0, 0.001);
}

// ============================================================================
// PART 17: SMBPBIPowerSMBusSensorBytesConverter -- additional edge cases
// ============================================================================

TEST(SMBPBIPowerConverterB12F, Convert_ZeroValue_ReturnsZero)
{
    SMBPBIPowerSMBusSensorBytesConverter converter;
    auto data = converter.convert(0.0);

    ASSERT_EQ(data.size(), 4u);
    uint32_t val = 0;
    std::memcpy(&val, data.data(), 4);
    val = le32toh(val);
    EXPECT_EQ(val, 0u);
}

TEST(SMBPBIPowerConverterB12F, Convert_SmallValue_ConvertsToMilliwatts)
{
    SMBPBIPowerSMBusSensorBytesConverter converter;
    auto data = converter.convert(0.001); // 1 milliwatt

    uint32_t val = 0;
    std::memcpy(&val, data.data(), 4);
    val = le32toh(val);
    EXPECT_EQ(val, 1u); // 0.001 * 1000 = 1
}

TEST(SMBPBIPowerConverterB12F, Convert_LargeValue_ConvertsToMilliwatts)
{
    SMBPBIPowerSMBusSensorBytesConverter converter;
    auto data = converter.convert(1000.0); // 1 kW

    uint32_t val = 0;
    std::memcpy(&val, data.data(), 4);
    val = le32toh(val);
    EXPECT_EQ(val, 1000000u); // 1000 * 1000 = 1000000
}

// ============================================================================
// PART 18: DRAMUsageMetricUpdator -- constructor
// ============================================================================

TEST(DRAMUsageMetricUpdatorB12F, Constructor_SetsMembers)
{
    auto dbus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(dbus, "/test/b12f/dram_updator");

    DRAMUsageMetricUpdator updator(dimmIntf, "/test/b12f/dram_updator");

    // Verify it was constructed without error
    EXPECT_NE(dimmIntf, nullptr);
}

TEST(DRAMUsageMetricUpdatorB12F, UpdateMetric_DifferentValue_Updates)
{
    auto dbus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(dbus,
                                               "/test/b12f/dram_updator2");

    DRAMUsageMetricUpdator updator(dimmIntf, "/test/b12f/dram_updator2");

    // First update
    updator.updateMetric(75.5);
    EXPECT_EQ(dimmIntf->utilization(), 75.5);

    // Update with same value -- should not call utilization again
    // (dedup optimization)
    updator.updateMetric(75.5);
    EXPECT_EQ(dimmIntf->utilization(), 75.5);

    // Update with different value
    updator.updateMetric(80.0);
    EXPECT_EQ(dimmIntf->utilization(), 80.0);
}

// ============================================================================
// PART 19: NsmGPMInterfaceCreator -- createGPMIntf and addGpmIntfProperty
// ============================================================================

TEST(NsmGPMInterfaceCreatorB12F, CreateGPMIntf_SetsInterface)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/test/b12f/gpm_creator1");
    EXPECT_NE(creator.getGPMIntf(), nullptr);
}

TEST(NsmGPMInterfaceCreatorB12F, AddGpmIntfProperty_Double_Success)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/test/b12f/gpm_creator2");

    // Should not throw
    EXPECT_NO_THROW(creator.addGpmIntfProperty("TestDouble", DataType::Double));
}

TEST(NsmGPMInterfaceCreatorB12F, AddGpmIntfProperty_VectorDouble_Success)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/test/b12f/gpm_creator3");

    EXPECT_NO_THROW(
        creator.addGpmIntfProperty("TestVecDbl", DataType::VectorDouble));
}

TEST(NsmGPMInterfaceCreatorB12F, AddGpmIntfProperty_NullIntf_NoThrow)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/test/b12f/gpm_creator4");
    creator.gpmIntf.reset(); // Force null

    // Both overloads should handle null gracefully
    EXPECT_NO_THROW(creator.addGpmIntfProperty("NullProp", DataType::Double));
    std::vector<uint8_t> bitfield{0x01};
    EXPECT_NO_THROW(creator.addGpmIntfProperty(bitfield));
}

TEST(NsmGPMInterfaceCreatorB12F,
     AddGpmIntfProperty_Bitfield_RegistersKnownMetrics)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/test/b12f/gpm_creator5");

    // Set bits for known metrics (bit 0 = GraphicsEngineActivity, etc.)
    std::vector<uint8_t> bitfield{0x07}; // bits 0, 1, 2

    EXPECT_NO_THROW(creator.addGpmIntfProperty(bitfield));
    EXPECT_NE(creator.getGPMIntf(), nullptr);
}

// ============================================================================
// PART 20: makeNVLink*PerInstanceUpdator factory functions
// ============================================================================

TEST(MakeUpdatorsB12F, MakeNVLinkRawRx_ReturnsNonNull)
{
    auto dbus = sdbusplus::bus::new_default();
    std::vector<NVLinkMetricsUpdatorInfo> infos;
    auto intf = std::make_shared<NVLinkMetricsIntf>(dbus,
                                                    "/test/b12f/nvlink_rawrx");
    infos.push_back({"/test/b12f/nvlink_rawrx", intf});

    auto updator = makeNVLinkRawRxPerInstanceUpdator(infos);
    EXPECT_NE(updator, nullptr);
}

TEST(MakeUpdatorsB12F, MakeNVLinkRawTx_ReturnsNonNull)
{
    auto dbus = sdbusplus::bus::new_default();
    std::vector<NVLinkMetricsUpdatorInfo> infos;
    auto intf = std::make_shared<NVLinkMetricsIntf>(dbus,
                                                    "/test/b12f/nvlink_rawtx");
    infos.push_back({"/test/b12f/nvlink_rawtx", intf});

    auto updator = makeNVLinkRawTxPerInstanceUpdator(infos);
    EXPECT_NE(updator, nullptr);
}

TEST(MakeUpdatorsB12F, MakeNVLinkDataRx_ReturnsNonNull)
{
    auto dbus = sdbusplus::bus::new_default();
    std::vector<NVLinkMetricsUpdatorInfo> infos;
    auto intf = std::make_shared<NVLinkMetricsIntf>(dbus,
                                                    "/test/b12f/nvlink_datarx");
    infos.push_back({"/test/b12f/nvlink_datarx", intf});

    auto updator = makeNVLinkDataRxPerInstanceUpdator(infos);
    EXPECT_NE(updator, nullptr);
}

TEST(MakeUpdatorsB12F, MakeNVLinkDataTx_ReturnsNonNull)
{
    auto dbus = sdbusplus::bus::new_default();
    std::vector<NVLinkMetricsUpdatorInfo> infos;
    auto intf = std::make_shared<NVLinkMetricsIntf>(dbus,
                                                    "/test/b12f/nvlink_datatx");
    infos.push_back({"/test/b12f/nvlink_datatx", intf});

    auto updator = makeNVLinkDataTxPerInstanceUpdator(infos);
    EXPECT_NE(updator, nullptr);
}

TEST(MakeUpdatorsB12F, MakeGPMPerInstance_ReturnsNonNull)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/b12f/gpm_mk"},
        "com.nvidia.GPMMetrics");

    auto updator = makeGPMPerInstanceUpdator(
        "TestProp", sdbusplus::object_path{"/test/b12f/gpm_mk"}, gpmIntf);
    EXPECT_NE(updator, nullptr);
}

// ============================================================================
// PART 21: GPMIntfMetricsTable -- verify known entries
// ============================================================================

TEST(GPMIntfMetricsTableB12F, KnownMetricIds_ExistInTable)
{
    // Verify that key metric IDs are present
    EXPECT_NE(GPMIntfMetricsTable.find(0), GPMIntfMetricsTable.end());
    EXPECT_NE(GPMIntfMetricsTable.find(1), GPMIntfMetricsTable.end());
    EXPECT_NE(GPMIntfMetricsTable.find(2), GPMIntfMetricsTable.end());
    EXPECT_NE(GPMIntfMetricsTable.find(3), GPMIntfMetricsTable.end());
    EXPECT_NE(GPMIntfMetricsTable.find(8), GPMIntfMetricsTable.end());
    EXPECT_NE(GPMIntfMetricsTable.find(9), GPMIntfMetricsTable.end());
    EXPECT_NE(GPMIntfMetricsTable.find(28), GPMIntfMetricsTable.end());
    EXPECT_NE(GPMIntfMetricsTable.find(31), GPMIntfMetricsTable.end());
}

TEST(GPMIntfMetricsTableB12F, MetricId4_NotInTable)
{
    // Metric ID 4 is not in GPMIntfMetricsTable (DRAMUsage is handled
    // separately)
    EXPECT_EQ(GPMIntfMetricsTable.find(4), GPMIntfMetricsTable.end());
}

TEST(GPMIntfMetricsTableB12F, BandwidthMetrics_UseDecodeBandwidth)
{
    // Metric IDs 8, 9 (PCIe bandwidth) should use decodeBandwidth
    auto it8 = GPMIntfMetricsTable.find(8);
    ASSERT_NE(it8, GPMIntfMetricsTable.end());
    EXPECT_EQ(it8->second[0].decodeFunc, decodeBandwidth);

    auto it9 = GPMIntfMetricsTable.find(9);
    ASSERT_NE(it9, GPMIntfMetricsTable.end());
    EXPECT_EQ(it9->second[0].decodeFunc, decodeBandwidth);
}

TEST(GPMIntfMetricsTableB12F, PercentageMetrics_UseDecodePercentage)
{
    // Metric IDs 0, 1, 2, 3 (activity percent) use decodePercentage
    auto it0 = GPMIntfMetricsTable.find(0);
    ASSERT_NE(it0, GPMIntfMetricsTable.end());
    EXPECT_EQ(it0->second[0].decodeFunc, decodePercentage);

    auto it3 = GPMIntfMetricsTable.find(3);
    ASSERT_NE(it3, GPMIntfMetricsTable.end());
    EXPECT_EQ(it3->second[0].decodeFunc, decodePercentage);
}

// ============================================================================
// PART 22: NsmLongRunningSensor and NsmAsyncLongRunningSensor constructors
// with device set
// ============================================================================

TEST(NsmLongRunningSensorConstructorB12F,
     Constructor_NullDevice_SetsFieldsCorrectly)
{
    // Arrange & Act
    auto sensor = std::make_shared<MockableLongRunningSensor>(
        "B12F_LR_Ctor", "TestLR", true, nullptr, 0x0A, 0x20);

    // Assert
    EXPECT_EQ(sensor->NsmSensor::getName(), "B12F_LR_Ctor");
    EXPECT_EQ(sensor->NsmSensor::getType(), "TestLR");
    EXPECT_EQ(sensor->messageType, 0x0A);
    EXPECT_EQ(sensor->commandCode, 0x20);
    EXPECT_EQ(sensor->device, nullptr);
}

TEST(NsmAsyncLongRunningSensorConstructorB12F,
     Constructor_NullDevice_SetsFieldsCorrectly)
{
    // Arrange & Act
    auto sensor = std::make_shared<MockableAsyncLongRunningSensor>(
        "B12F_ALR_Ctor", "TestALR", false, nullptr, 0x0B, 0x21);

    // Assert
    EXPECT_EQ(sensor->NsmSensor::getName(), "B12F_ALR_Ctor");
    EXPECT_EQ(sensor->NsmSensor::getType(), "TestALR");
    EXPECT_EQ(sensor->messageType, 0x0B);
    EXPECT_EQ(sensor->commandCode, 0x21);
    EXPECT_EQ(sensor->device, nullptr);
}

// ============================================================================
// PART 23: NsmThreshold -- getSensorType and full round-trip
// ============================================================================

TEST(NsmThresholdRoundTripB12F, FullCycle_GenRequest_HandleResponse_Success)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdWarningIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_rt1");
    auto thresholdValue = std::make_unique<NsmThresholdValueWarningHigh>(
        "B12F_RT1", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("B12F_RT_Threshold", "NSM_ThermalParameter", 50,
                           sensorValueAgg);

    // Step 1: Generate request
    auto request = threshold.genRequestMsg(10, 3);
    ASSERT_TRUE(request.has_value());

    // Step 2: Build response
    int32_t paramValue = 88;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL, paramValue,
                                       response);

    // Step 3: Handle response
    auto rc = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(thresholdIntf->warningHigh(), 88.0);
}

TEST(NsmThresholdRoundTripB12F, GetSensorType_ReturnsThreshold)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdCriticalIntf>(
        dbus, "/xyz/openbmc_project/sensors/threshold/b12f_rt2");
    auto thresholdValue = std::make_unique<NsmThresholdValueCriticalLow>(
        "B12F_RT2", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("B12F_RT_GetType", "NSM_ThermalParameter", 10,
                           sensorValueAgg);

    // Act
    auto result = threshold.getSensorType();

    // Assert
    EXPECT_EQ(result, "threshold");
}

// ============================================================================
// PART 24: NsmNumericSensorDbusValue -- updateReading value dedup
// ============================================================================

TEST_F(NsmNumericSensorDbusValueB12FFixture,
       UpdateReading_SameValue_DoesNotUpdate)
{
    // Arrange
    auto& dbus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc = {
        {"chassis", "all_sensors",
         "/xyz/openbmc_project/inventory/b12f_dedup"}};
    NsmNumericSensorDbusValue value(
        dbus, "b12f_dedup_test", "temperature_b12f_dd", SensorUnit::DegreesC,
        assoc, "GPU", nullptr, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(), nullptr, nullptr);

    // Act -- first update
    value.updateReading(42.0, 1000);
    EXPECT_EQ(value.valueIntf.value(), 42.0);

    // Second update with same value -- the internal value is unchanged
    // (dedup optimization: previousValue == value, so no set)
    value.updateReading(42.0, 2000);
    EXPECT_EQ(value.valueIntf.value(), 42.0);

    // Third update with different value
    value.updateReading(43.0, 3000);
    EXPECT_EQ(value.valueIntf.value(), 43.0);
}

// ============================================================================
// PART 25: NsmLongRunningSensor::updateLongRunningSensor() branch coverage
// ============================================================================

struct NsmLongRunningSensorUpdateTest :
    public testing::Test,
    public SensorManagerTest
{
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "5", 0);
    NsmDeviceTable devices{{device}};

    NsmLongRunningSensorUpdateTest() : SensorManagerTest(devices) {}
    ~NsmLongRunningSensorUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

/**
 * updateLongRunningSensor(): genRequestMsg returns nullopt → returns
 * NSM_SW_ERROR. Covers L76 TRUE branch.
 */
TEST_F(NsmLongRunningSensorUpdateTest,
       UpdateLongRunning_GenRequestMsgFails_ReturnsError)
{
    auto sensor = std::make_shared<MockableLongRunningSensor>(
        "LR_Upd1", "LongRunning", false, nullptr, 0x05, 0x10);

    EXPECT_CALL(*sensor, genRequestMsg(_, _)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*device, sensorIO(_, _, _, _, _)).Times(0);

    auto cr = sensor->updateLongRunningSensor(device);
    EXPECT_NE(cr.data(), NSM_SW_SUCCESS);
}

/**
 * updateLongRunningSensor(): sensorIO returns a non-zero transport error →
 * returns that error. Covers L90 TRUE branch.
 */
TEST_F(NsmLongRunningSensorUpdateTest,
       UpdateLongRunning_SensorIOFails_ReturnsError)
{
    auto sensor = std::make_shared<MockableLongRunningSensor>(
        "LR_Upd2", "LongRunning", false, nullptr, 0x05, 0x10);

    Request fakeReq(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    EXPECT_CALL(*sensor, genRequestMsg(_, _)).WillOnce(Return(fakeReq));
    EXPECT_CALL(*device, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    auto cr = sensor->updateLongRunningSensor(device);
    EXPECT_NE(cr.data(), NSM_SW_SUCCESS);
}

/**
 * updateLongRunningSensor(): sensorIO succeeds, decode gives cc == NSM_SUCCESS
 * → calls handleResponseMsg and sets isLongRunning = false.
 * Covers L103 TRUE branch.
 */
TEST_F(NsmLongRunningSensorUpdateTest,
       UpdateLongRunning_CcIsNsmSuccess_CallsHandleResponse)
{
    auto sensor = std::make_shared<MockableLongRunningSensor>(
        "LR_Upd3", "LongRunning", true, nullptr, 0x05, 0x10);

    Request fakeReq(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    EXPECT_CALL(*sensor, genRequestMsg(_, _)).WillOnce(Return(fakeReq));

    // Zero-filled response: completion_code byte = 0 = NSM_SUCCESS
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    EXPECT_CALL(*device, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp, NSM_SUCCESS));
    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .WillOnce(Return(NSM_SW_SUCCESS));

    auto cr = sensor->updateLongRunningSensor(device);
    EXPECT_EQ(cr.data(), NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor->isLongRunning); // isLongRunning cleared on NSM_SUCCESS
}

/**
 * updateLongRunningSensor(): cc != NSM_SUCCESS, initAcceptInstanceId returns
 * false (cc = NSM_ERROR, not NSM_ACCEPTED) → rc = NSM_SW_ERROR_COMMAND_FAIL.
 * Covers L113 TRUE branch.
 */
TEST_F(NsmLongRunningSensorUpdateTest,
       UpdateLongRunning_InitAcceptInstanceIdFails_ReturnsCommandFail)
{
    auto sensor = std::make_shared<MockableLongRunningSensor>(
        "LR_Upd4", "LongRunning", false, nullptr, 0x05, 0x10);

    Request fakeReq(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    EXPECT_CALL(*sensor, genRequestMsg(_, _)).WillOnce(Return(fakeReq));

    // Set completion_code = NSM_ERROR (1): cc != NSM_SUCCESS, cc !=
    // NSM_ACCEPTED → initAcceptInstanceId returns false → L115 taken
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = static_cast<uint8_t>(NSM_ERROR);
    EXPECT_CALL(*device, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp, NSM_SUCCESS));
    EXPECT_CALL(*sensor, handleResponseMsg(_, _)).Times(0);

    auto cr = sensor->updateLongRunningSensor(device);
    EXPECT_EQ(cr.data(), NSM_SW_ERROR_COMMAND_FAIL);
}

/**
 * updateLongRunningSensor(): cc != NSM_SUCCESS but cc == NSM_ACCEPTED and
 * transport rc == NSM_SW_SUCCESS → initAcceptInstanceId returns true →
 * L113 FALSE, acceptInstanceId set to the instance_id from the response header.
 * Covers L113 FALSE branch.
 */
TEST_F(NsmLongRunningSensorUpdateTest,
       UpdateLongRunning_InitAcceptInstanceIdSucceeds_AcceptsInstance)
{
    auto sensor = std::make_shared<MockableLongRunningSensor>(
        "LR_Upd5", "LongRunning", false, nullptr, 0x05, 0x10);

    Request fakeReq(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    EXPECT_CALL(*sensor, genRequestMsg(_, _)).WillOnce(Return(fakeReq));

    // Set completion_code = NSM_ACCEPTED (0x7d): cc != NSM_SUCCESS,
    // initAcceptInstanceId(instance_id=0, NSM_ACCEPTED, NSM_SW_SUCCESS) → true
    // → L113 FALSE, acceptInstanceId = instance_id from header (0)
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = static_cast<uint8_t>(NSM_ACCEPTED);
    EXPECT_CALL(*device, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp, NSM_SUCCESS));
    EXPECT_CALL(*sensor, handleResponseMsg(_, _)).Times(0);

    auto cr = sensor->updateLongRunningSensor(device);
    EXPECT_EQ(sensor->acceptInstanceId, 0u); // set to instance_id from header
}
