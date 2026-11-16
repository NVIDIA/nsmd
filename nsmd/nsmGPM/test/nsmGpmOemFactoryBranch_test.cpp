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
 * Branch coverage tests for nsmd/nsmGPM/nsmGpmOemFactory.cpp
 *
 * Covers:
 * - createGPMMetricsFeature: no device, defaults, aggregated params,
 *   AggregatedMemoryBandwidth true/false
 * - convertToBytes: various inputs
 *
 * Per-instance and per-port GPM creators are covered in
 * nsmGpmOemFactory_test.cpp.
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
} // namespace nsm

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Fixture for createGPMMetricsFeature branch coverage
// ============================================================================
struct NsmGpmOemFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    boost::asio::io_context io;
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    size_t initialSensorCount = 0;

    NsmGpmOemFactoryBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        initialSensorCount = gpu->deviceSensors.size();
    }

    ~NsmGpmOemFactoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    static dbus::PropertyMap mainParams(const std::string& name)
    {
        return {
            {"Name", name},
            {"AggregatedRetrievalSource", uint64_t(2)},
            {"AggregatedGpuInstance", uint64_t(0)},
            {"AggregatedComputeInstance", uint64_t(0)},
            {"AggregatedMetricsBitfield",
             std::vector<uint64_t>{0x89, 0x04, 0x15}},
        };
    }

    requester::Coroutine createGPMMetrics(const dbus::PropertyMap& params,
                                          bool withDevice = true)
    {
        features::DeviceContext dev;
        dev.name = "HGX_GPU_SXM_0";
        dev.kind = "gpu";
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = gpu;
        }
        return features::createGPMMetricsFeature(mockManager, dev, params);
    }

    std::shared_ptr<NsmGetSupportedGPMMetrics> aggregateSensor()
    {
        for (const auto& sensor : gpu->deviceSensors)
        {
            if (auto s = std::dynamic_pointer_cast<NsmGetSupportedGPMMetrics>(
                    sensor))
            {
                return s;
            }
        }
        return nullptr;
    }
};

// ============================================================================
// createGPMMetricsFeature: valid creation path
// ============================================================================
TEST_F(NsmGpmOemFactoryBranchTest, CreateGPMMetrics_ValidProperties_Success)
{
    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(ReturnRef(*objServer));

    auto rc = createGPMMetrics(mainParams("GPM_Metrics_FB"));

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    // The GetSupportedGPMMetrics static sensor
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
    auto sensor = aggregateSensor();
    ASSERT_NE(sensor, nullptr);
    EXPECT_EQ(sensor->getName(), "GPM_Metrics_FB");
    EXPECT_EQ(sensor->getType(), "NSM_GPMMetrics");
    EXPECT_EQ(sensor->retrievalSource, 2);
    // Inventory object is the device's accelerator path (prefix stripped)
    EXPECT_EQ(sensor->objPath,
              "/xyz/openbmc_project/inventory/system/accelerator/GPU_SXM_0");
    EXPECT_EQ(sensor->configuredMetricsBitfield,
              (std::vector<uint8_t>{0x89, 0x04, 0x15}));
    EXPECT_EQ(sensor->dimmIntf, nullptr);
}

// ============================================================================
// createGPMMetricsFeature: no NsmDevice → NSM_ERROR, no sensor
// ============================================================================
TEST_F(NsmGpmOemFactoryBranchTest, CreateGPMMetrics_NoDevice_ReturnsError)
{
    auto rc = createGPMMetrics(mainParams("GPM_Metrics_NoDev"), false);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount);
}

// ============================================================================
// createGPMMetricsFeature: every aggregated param defaults
// ============================================================================
TEST_F(NsmGpmOemFactoryBranchTest, CreateGPMMetrics_OnlyName_Defaults)
{
    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(ReturnRef(*objServer));

    auto rc = createGPMMetrics({{"Name", std::string("GPM_Metrics_Def")}});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    auto sensor = aggregateSensor();
    ASSERT_NE(sensor, nullptr);
    EXPECT_EQ(sensor->retrievalSource, 0);
    EXPECT_TRUE(sensor->configuredMetricsBitfield.empty());
    EXPECT_TRUE(sensor->memoryInventoryObjPath.empty());
}

// ============================================================================
// createGPMMetricsFeature: AggregatedMemoryBandwidth=true path
// ============================================================================
TEST_F(NsmGpmOemFactoryBranchTest,
       CreateGPMMetrics_PopulateMemoryBandwidth_True)
{
    auto params = mainParams("GPM_Metrics_MemBW");
    params["AggregatedMetricsBitfield"] =
        std::vector<uint64_t>{0x10}; // bit for DRAMUsage
    params["AggregatedMemoryBandwidth"] = true;

    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(ReturnRef(*objServer));

    // This will attempt to create DimmIntf - may throw but caught internally
    auto rc = createGPMMetrics(params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), initialSensorCount);
    auto sensor = aggregateSensor();
    ASSERT_NE(sensor, nullptr);
    // Memory inventory object is derived from the device name
    EXPECT_EQ(sensor->memoryInventoryObjPath,
              "/xyz/openbmc_project/inventory/system/dimm/GPU_SXM_0_DRAM_0");
}

// ============================================================================
// createGPMMetricsFeature: AggregatedMemoryBandwidth=false path
// ============================================================================
TEST_F(NsmGpmOemFactoryBranchTest,
       CreateGPMMetrics_PopulateMemoryBandwidth_False)
{
    auto params = mainParams("GPM_Metrics_NoMemBW");
    params["AggregatedMetricsBitfield"] = std::vector<uint64_t>{0x01};
    params["AggregatedMemoryBandwidth"] = false;

    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(ReturnRef(*objServer));

    auto rc = createGPMMetrics(params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), initialSensorCount);
    auto sensor = aggregateSensor();
    ASSERT_NE(sensor, nullptr);
    EXPECT_EQ(sensor->dimmIntf, nullptr);
    EXPECT_TRUE(sensor->memoryInventoryObjPath.empty());
}

// ============================================================================
// convertToBytes: additional branch coverage
// ============================================================================
TEST(NsmGpmOemFactoryBranchConvertTest, SingleElement)
{
    auto result = convertToBytes({42});
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], 42u);
}

TEST(NsmGpmOemFactoryBranchConvertTest, MultipleElements)
{
    auto result = convertToBytes({0, 128, 255});
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 0u);
    EXPECT_EQ(result[1], 128u);
    EXPECT_EQ(result[2], 255u);
}

TEST(NsmGpmOemFactoryBranchConvertTest, Empty)
{
    auto result = convertToBytes({});
    EXPECT_TRUE(result.empty());
}

TEST(NsmGpmOemFactoryBranchConvertTest, TruncateLargeValues)
{
    auto result = convertToBytes({0x1234});
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], 0x34u);
}
