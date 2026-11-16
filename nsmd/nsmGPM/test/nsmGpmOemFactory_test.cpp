/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "platform-environmental.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmGpmOem.hpp"

#undef private
#undef protected

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

// Forward-declare internal functions from nsmGpmOemFactory.cpp
namespace nsm
{
std::vector<uint8_t> convertToBytes(const std::vector<uint64_t>& data);

int addNsmPerInstanceGPMMetric(
    std::shared_ptr<NsmGPMInterfaceCreator> gpmInterfaceCreator,
    std::shared_ptr<NsmDevice> nsmDevice, const std::string& inventoryObjPath,
    const std::string& name, const std::string& type, uint8_t retrievalSource,
    uint8_t gpuInstance, uint8_t computeInstance, const std::string& metric,
    uint8_t metricId, std::optional<uint64_t> v1BitmaskOpt,
    const std::optional<std::vector<uint64_t>>& v2InstanceBitfield,
    const std::string& uuid, const std::string& interface);
} // namespace nsm

using namespace nsm;

// ============================================================================
// convertToBytes Tests
// ============================================================================

TEST(NsmGpmOemFactoryTest, ConvertToBytesEmptyVector)
{
    std::vector<uint64_t> input;
    auto result = convertToBytes(input);

    EXPECT_TRUE(result.empty());
    EXPECT_EQ(result.size(), 0u);
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesZeroValue)
{
    std::vector<uint64_t> input = {0};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0));
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesMaxUint8)
{
    std::vector<uint64_t> input = {255};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(255));
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesValueTruncatedToUint8)
{
    // 0x1FF = 511 truncated to uint8 = 0xFF = 255
    std::vector<uint64_t> input = {0x1FF};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0xFF));
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesLargeValueTruncated)
{
    // 0xABCD takes low byte 0xCD
    std::vector<uint64_t> input = {0xABCD};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0xCD));
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesMaxUint64Truncated)
{
    // UINT64_MAX = 0xFFFFFFFFFFFFFFFF, low byte = 0xFF
    std::vector<uint64_t> input = {UINT64_MAX};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0xFF));
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesMultipleElements)
{
    std::vector<uint64_t> input = {0, 1, 2, 3, 255};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0));
    EXPECT_EQ(result[1], static_cast<uint8_t>(1));
    EXPECT_EQ(result[2], static_cast<uint8_t>(2));
    EXPECT_EQ(result[3], static_cast<uint8_t>(3));
    EXPECT_EQ(result[4], static_cast<uint8_t>(255));
}

TEST(NsmGpmOemFactoryTest, ConvertBytesSizePreserved)
{
    std::vector<uint64_t> input = {10, 20, 30};
    auto result = convertToBytes(input);

    EXPECT_EQ(result.size(), input.size());
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesValuesAreOnlyLowByte)
{
    // Each value has different bits set; result should be just the low byte
    std::vector<uint64_t> input = {0x0102030405060708ULL,
                                   0xFF00FF00FF00FF00ULL};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0x08));
    EXPECT_EQ(result[1], static_cast<uint8_t>(0x00));
}

namespace
{
// Count how many items in `deviceSensors` are convertible to T.
template <typename T>
size_t countSensors(
    const std::vector<std::shared_ptr<nsm::NsmObject>>& deviceSensors)
{
    size_t n = 0;
    for (const auto& s : deviceSensors)
    {
        if (std::dynamic_pointer_cast<T>(s))
        {
            ++n;
        }
    }
    return n;
}

features::DeviceContext gpuContext(const std::string& name, const uuid_t& uuid,
                                   std::shared_ptr<NsmDevice> device)
{
    features::DeviceContext dev;
    dev.name = name;
    dev.kind = "gpu";
    dev.staticUuid = uuid;
    dev.platformPrefix = "HGX_";
    dev.device = std::move(device);
    return dev;
}
} // namespace

// ============================================================================
// Test fixture for createGPMPortMetricsFeature
// ============================================================================

struct NsmPerPortGPMMetricsTestFixture :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    size_t initialSensorCount = 0;

    NsmPerPortGPMMetricsTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        // NsmDevice constructor calls initMsgTypesSensor() which adds 1 sensor
        initialSensorCount = gpu->deviceSensors.size();
    }

    ~NsmPerPortGPMMetricsTestFixture()
    {
        cleanupDeviceSensors(devices);
    }

    static dbus::PropertyMap
        portParams(const std::vector<std::string>& metrics,
                   uint64_t nvlinkCount = 0,
                   const std::vector<uint64_t>& instanceBitfield = {0x01},
                   const std::string& parentPath =
                       "/xyz/openbmc_project/inventory/gpm_port")
    {
        return {
            {"Name", std::string("TestGPMPort")},
            {"RetrievalSource", uint64_t(1)},
            {"GpuInstance", uint64_t(0)},
            {"ComputeInstance", uint64_t(0)},
            {"Metrics", metrics},
            {"NVLinkCount", nvlinkCount},
            {"InstanceBitfield", instanceBitfield},
            {"ParentObjPath", parentPath},
        };
    }

    requester::Coroutine callFactory(const dbus::PropertyMap& params,
                                     bool withDevice = true)
    {
        auto dev = gpuContext("HGX_GPU_SXM_0", gpuUuid,
                              withDevice ? gpu : nullptr);
        return features::createGPMPortMetricsFeature(mockManager, dev, params);
    }
};

TEST_F(NsmPerPortGPMMetricsTestFixture, AllValidMetrics_SensorsAdded)
{
    auto rc = callFactory(
        portParams({"NVLinkRawTxBandwidthGbps", "NVLinkDataTxBandwidthGbps",
                    "NVLinkRawRxBandwidthGbps", "NVLinkDataRxBandwidthGbps"},
                   0, {0x01}, "/xyz/openbmc_project/inventory/gpm_port_all"));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 4);
    EXPECT_EQ(countSensors<nsm::NsmGetSupportedPerInstanceGPMMetrics>(
                  gpu->deviceSensors),
              4u);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, NoDevice_ReturnsError)
{
    auto rc = callFactory(portParams({"NVLinkRawTxBandwidthGbps"}), false);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, UnknownMetric_Skipped)
{
    auto rc =
        callFactory(portParams({"UnknownMetricXYZ"}, 0, {0x01},
                               "/xyz/openbmc_project/inventory/gpm_port_unk"));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 0);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, MixedMetrics_OnlyValidAdded)
{
    auto rc = callFactory(
        portParams({"NVLinkRawTxBandwidthGbps", "UnknownMetric",
                    "NVLinkDataRxBandwidthGbps"},
                   0, {0x01}, "/xyz/openbmc_project/inventory/gpm_port_mixed"));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 2);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, NoPorts_SensorCreated)
{
    auto rc = callFactory(
        portParams({"NVLinkRawTxBandwidthGbps"}, 0, {0x01},
                   "/xyz/openbmc_project/inventory/gpm_port_empty_ports"));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

// NVLinkCount ports are created per sensor's updator; still one sensor per
// metric
TEST_F(NsmPerPortGPMMetricsTestFixture, NVLinkCount_OneSensorPerMetric)
{
    auto rc =
        callFactory(portParams({"NVLinkDataTxBandwidthGbps"}, 4, {0x01},
                               "/xyz/openbmc_project/inventory/gpm_port_nvl"));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, EmptyMetrics_NoSensorsAdded)
{
    auto rc = callFactory(
        portParams({}, 0, {0x01},
                   "/xyz/openbmc_project/inventory/gpm_port_empty_metrics"));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 0);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, MultipleInstanceBitfieldBytes_Converted)
{
    // Tests instanceBitfield conversion loop with multiple bytes
    auto rc = callFactory(
        portParams({"NVLinkRawRxBandwidthGbps"}, 0, {0x01, 0x02, 0x03},
                   "/xyz/openbmc_project/inventory/gpm_port_multi_bitfield"));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, NVLinkDataRxBandwidthGbps_SensorAdded)
{
    auto rc =
        callFactory(portParams({"NVLinkDataRxBandwidthGbps"}, 0, {0x01},
                               "/xyz/openbmc_project/inventory/gpm_port_rx"));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerPortGPMMetricsTestFixture,
       V1_BitmaskPresent_OnePerInstanceV1PerMetric)
{
    auto params =
        portParams({"NVLinkRawTxBandwidthGbps", "NVLinkDataRxBandwidthGbps"}, 0,
                   {}, "/xyz/openbmc_project/inventory/gpm_port_v1");
    // V1 config: InstanceBitfield set as scalar uint64_t.
    params["InstanceBitfield"] = uint64_t(0x000000FFU);

    auto rc = callFactory(params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 2);
    EXPECT_EQ(countSensors<nsm::NsmGPMPerInstanceV1>(gpu->deviceSensors), 2u);
    EXPECT_EQ(countSensors<nsm::NsmGetSupportedPerInstanceGPMMetrics>(
                  gpu->deviceSensors),
              0u);
}

// ============================================================================
// Per-instance metrics of createGPMMetricsFeature (<Metric>PerInstance*
// params). Each enabled metric adds one per-instance sensor next to the
// aggregate NsmGetSupportedGPMMetrics sensor.
// ============================================================================

struct NsmPerInstanceGPMMetricTestFixture :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/gpm_per_inst";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";

    boost::asio::io_context io;
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<NsmGPMInterfaceCreator> gpmCreator;
    size_t initialSensorCount = 0;

    NsmPerInstanceGPMMetricTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        gpmCreator = std::make_shared<NsmGPMInterfaceCreator>(*objServer,
                                                              inventoryObjPath);
        // NsmDevice constructor adds msgTypesSensor to deviceSensors
        initialSensorCount = gpu->deviceSensors.size();
    }

    ~NsmPerInstanceGPMMetricTestFixture()
    {
        cleanupDeviceSensors(devices);
    }

    // Aggregated params plus one enabled per-instance metric
    static dbus::PropertyMap perInstanceParams(const std::string& metric,
                                               const dbus::Value& bitfield)
    {
        const std::string prefix = metric + "PerInstance";
        return {
            {"Name", std::string("GPM_Metrics_PI")},
            {"AggregatedRetrievalSource", uint64_t(2)},
            {"AggregatedMetricsBitfield",
             std::vector<uint64_t>{0x89, 0x04, 0x15}},
            {prefix + "Enabled", true},
            {prefix + "Name", std::string("TestPerInstanceMetric")},
            {prefix + "RetrievalSource", uint64_t(1)},
            {prefix + "GpuInstance", uint64_t(0)},
            {prefix + "ComputeInstance", uint64_t(0)},
            {prefix + "Bitfield", bitfield},
        };
    }

    requester::Coroutine createGPMMetrics(const dbus::PropertyMap& params)
    {
        auto dev = gpuContext("HGX_GPU_SXM_1", gpuUuid, gpu);
        return features::createGPMMetricsFeature(mockManager, dev, params);
    }

    size_t perInstanceSensorCount() const
    {
        return countSensors<nsm::NsmGPMPerInstanceV1>(gpu->deviceSensors) +
               countSensors<nsm::NsmGetSupportedPerInstanceGPMMetrics>(
                   gpu->deviceSensors);
    }
};

TEST_F(NsmPerInstanceGPMMetricTestFixture, MetricNVDEC_SensorAdded)
{
    auto rc = createGPMMetrics(
        perInstanceParams("NVDEC", std::vector<uint64_t>{0x01}));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // aggregate + per-instance
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 2);
    EXPECT_EQ(perInstanceSensorCount(), 1u);
}

TEST_F(NsmPerInstanceGPMMetricTestFixture, MetricNVJPG_SensorAdded)
{
    auto rc = createGPMMetrics(
        perInstanceParams("NVJPG", std::vector<uint64_t>{0x01}));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 2);
    EXPECT_EQ(perInstanceSensorCount(), 1u);
}

TEST_F(NsmPerInstanceGPMMetricTestFixture, MetricNVENC_SensorAdded)
{
    auto rc = createGPMMetrics(
        perInstanceParams("NVENC", std::vector<uint64_t>{0x01}));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 2);
    EXPECT_EQ(perInstanceSensorCount(), 1u);
}

// Per-instance metric not enabled → only the aggregate sensor
TEST_F(NsmPerInstanceGPMMetricTestFixture, MetricDisabled_NoPerInstanceSensor)
{
    auto params = perInstanceParams("NVDEC", std::vector<uint64_t>{0x01});
    params["NVDECPerInstanceEnabled"] = false;

    auto rc = createGPMMetrics(params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
    EXPECT_EQ(perInstanceSensorCount(), 0u);
}

// Unsupported metric name → addNsmPerInstanceGPMMetric rejects it
TEST_F(NsmPerInstanceGPMMetricTestFixture, MetricUnknown_NoSensorAdded)
{
    auto rc = addNsmPerInstanceGPMMetric(
        gpmCreator, gpu, inventoryObjPath, "TestPerInstanceMetric",
        "NSM_GPMPerInstanceMetrics", 1, 0, 0, "UnknownMetric", 5, std::nullopt,
        std::vector<uint64_t>{0x01}, gpuUuid, "UnknownMetricPerInstance");
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 0);
}

TEST_F(NsmPerInstanceGPMMetricTestFixture, MultipleInstanceBitfieldBytes)
{
    // Tests instanceBitfield conversion loop with multiple bytes
    auto rc = createGPMMetrics(
        perInstanceParams("NVENC", std::vector<uint64_t>{0x01, 0x02, 0x03}));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(countSensors<nsm::NsmGetSupportedPerInstanceGPMMetrics>(
                  gpu->deviceSensors),
              1u);
}

TEST_F(NsmPerInstanceGPMMetricTestFixture, V1_BitmaskPresent_RoutesToV1Sensor)
{
    // V1 config: Bitfield set as scalar uint64_t.
    auto rc =
        createGPMMetrics(perInstanceParams("NVDEC", uint64_t(0x0000000FU)));

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(countSensors<nsm::NsmGPMPerInstanceV1>(gpu->deviceSensors), 1u);
    EXPECT_EQ(countSensors<nsm::NsmGetSupportedPerInstanceGPMMetrics>(
                  gpu->deviceSensors),
              0u);
}

TEST_F(NsmPerInstanceGPMMetricTestFixture,
       V1_BitmaskPresent_AbovedUint32_Truncated)
{
    // high bits > 31 must be silently truncated with a warning, not throw
    auto rc = createGPMMetrics(
        perInstanceParams("NVENC", uint64_t(0x0000000F'FFFFFFFFULL)));

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(countSensors<nsm::NsmGPMPerInstanceV1>(gpu->deviceSensors), 1u);
}

TEST_F(NsmPerInstanceGPMMetricTestFixture, V2_NoBitmask_RoutesToStaticSensor)
{
    // Baseline: confirm V2 path still creates the discovery static sensor and
    // NOT a V1 sensor when the Bitfield is a vector.
    auto rc = createGPMMetrics(
        perInstanceParams("NVDEC", std::vector<uint64_t>{0x01}));

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(countSensors<nsm::NsmGPMPerInstanceV1>(gpu->deviceSensors), 0u);
    EXPECT_EQ(countSensors<nsm::NsmGetSupportedPerInstanceGPMMetrics>(
                  gpu->deviceSensors),
              1u);
}

// All three per-instance metrics enabled at once → three per-instance sensors
TEST_F(NsmPerInstanceGPMMetricTestFixture, AllPerInstanceMetrics_Enabled)
{
    auto params = perInstanceParams("NVDEC", std::vector<uint64_t>{0x01});
    for (const std::string metric : {"NVJPG", "NVENC"})
    {
        const std::string prefix = metric + "PerInstance";
        params[prefix + "Enabled"] = true;
        params[prefix + "Name"] = std::string("PI_") + metric;
        params[prefix + "Bitfield"] = std::vector<uint64_t>{0x01};
    }

    auto rc = createGPMMetrics(params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 4);
    EXPECT_EQ(perInstanceSensorCount(), 3u);
}
