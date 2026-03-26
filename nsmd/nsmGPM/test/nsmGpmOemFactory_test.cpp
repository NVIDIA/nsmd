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

#include "nsmGpmOem.hpp"
#include "nsmObjectFactory.hpp"

#undef private
#undef protected

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

// Forward-declare internal functions from nsmGpmOemFactory.cpp
namespace nsm
{
std::vector<uint8_t> convertToBytes(const std::vector<uint64_t>& data);

requester::Coroutine createNsmGPMMetrics(SensorManager& manager,
                                         const std::string& interface,
                                         const std::string& objPath);

requester::Coroutine createNsmPerInstanceGPMMetric(
    std::shared_ptr<NsmGPMInterfaceCreator> gpmInterfaceCreator,
    std::shared_ptr<NsmDevice> nsmDevice, const std::string& inventoryObjPath,
    const std::string& interface, const std::string& objPath,
    const std::string& uuid);
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

// ============================================================================
// Test fixture for createNsmPerPortGPMMetrics
// (static function, accessed via NsmObjectFactory)
// ============================================================================

struct NsmPerPortGPMMetricsTestFixture :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string portInterface =
        "xyz.openbmc_project.Configuration.NSM_GPMPortMetrics";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpm_port_test";
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

    void setupProperties(const std::vector<std::string>& metrics,
                         const std::vector<uint64_t>& ports = {},
                         const std::vector<uint64_t>& instanceBitfield = {0x01},
                         const std::string& inventoryPath =
                             "/xyz/openbmc_project/inventory/gpm_port")
    {
        auto& pm = utils::MockDbusAsync::propertyMap(objPath, portInterface);
        pm["Name"] = std::string("TestGPMPort");
        pm["UUID"] = gpuUuid;
        pm["RetrievalSource"] = uint64_t(1);
        pm["GpuInstance"] = uint64_t(0);
        pm["ComputeInstance"] = uint64_t(0);
        pm["Metrics"] = metrics;
        pm["Ports"] = ports;
        pm["InstanceBitfield"] = instanceBitfield;
        pm["InventoryObjPath"] = inventoryPath;
    }

    void callFactory()
    {
        auto& factory = NsmObjectFactory::instance();
        auto it = factory.creationFunctions.find(portInterface);
        ASSERT_NE(it, factory.creationFunctions.end());
        it->second(mockManager, portInterface, objPath);
    }
};

TEST_F(NsmPerPortGPMMetricsTestFixture, AllValidMetrics_SensorsAdded)
{
    setupProperties({"NVLinkRawTxBandwidthGbps", "NVLinkDataTxBandwidthGbps",
                     "NVLinkRawRxBandwidthGbps", "NVLinkDataRxBandwidthGbps"},
                    {}, {0x01}, "/xyz/openbmc_project/inventory/gpm_port_all");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 4);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, UnknownMetric_Skipped)
{
    setupProperties({"UnknownMetricXYZ"}, {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_unk");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 0);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, MixedMetrics_OnlyValidAdded)
{
    setupProperties({"NVLinkRawTxBandwidthGbps", "UnknownMetric",
                     "NVLinkDataRxBandwidthGbps"},
                    {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_mixed");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 2);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, EmptyPorts_SensorCreated)
{
    setupProperties({"NVLinkRawTxBandwidthGbps"}, {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_empty_ports");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, DuplicatePorts_Deduped)
{
    setupProperties({"NVLinkDataTxBandwidthGbps"}, {2, 2, 3, 3}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_dup");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, EmptyMetrics_NoSensorsAdded)
{
    setupProperties({}, {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_empty_metrics");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 0);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, MultipleInstanceBitfieldBytes_Converted)
{
    // Tests instanceBitfield conversion loop with multiple bytes
    setupProperties({"NVLinkRawRxBandwidthGbps"}, {}, {0x01, 0x02, 0x03},
                    "/xyz/openbmc_project/inventory/gpm_port_multi_bitfield");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerPortGPMMetricsTestFixture, NVLinkDataRxBandwidthGbps_SensorAdded)
{
    setupProperties({"NVLinkDataRxBandwidthGbps"}, {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_rx");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

// ============================================================================
// Test fixture for createNsmPerInstanceGPMMetric
// ============================================================================

struct NsmPerInstanceGPMMetricTestFixture :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_GPMMetrics.PerInstanceMetrics0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpm_per_inst_test";
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

    void setupProperties(const std::string& metric)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
        pm["Name"] = std::string("TestPerInstanceMetric");
        pm["Type"] = std::string("GPMPerInstance");
        pm["RetrievalSource"] = uint64_t(1);
        pm["GpuInstance"] = uint64_t(0);
        pm["ComputeInstance"] = uint64_t(0);
        pm["Metric"] = metric;
        pm["MetricId"] = uint64_t(5);
        pm["InstanceBitfield"] = std::vector<uint64_t>{0x01};
    }
};

TEST_F(NsmPerInstanceGPMMetricTestFixture, MetricNVDEC_SensorAdded)
{
    setupProperties("NVDEC");
    createNsmPerInstanceGPMMetric(gpmCreator, gpu, inventoryObjPath, interface,
                                  objPath, gpuUuid);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerInstanceGPMMetricTestFixture, MetricNVJPG_SensorAdded)
{
    setupProperties("NVJPG");
    createNsmPerInstanceGPMMetric(gpmCreator, gpu, inventoryObjPath, interface,
                                  objPath, gpuUuid);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerInstanceGPMMetricTestFixture, MetricNVENC_SensorAdded)
{
    setupProperties("NVENC");
    createNsmPerInstanceGPMMetric(gpmCreator, gpu, inventoryObjPath, interface,
                                  objPath, gpuUuid);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerInstanceGPMMetricTestFixture, MetricUnknown_NoSensorAdded)
{
    setupProperties("UnknownMetric");
    createNsmPerInstanceGPMMetric(gpmCreator, gpu, inventoryObjPath, interface,
                                  objPath, gpuUuid);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 0);
}

TEST_F(NsmPerInstanceGPMMetricTestFixture, MultipleInstanceBitfieldBytes)
{
    // Tests instanceBitfield conversion loop with multiple bytes
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Name"] = std::string("TestPerInstanceMetric");
    pm["Type"] = std::string("GPMPerInstance");
    pm["RetrievalSource"] = uint64_t(1);
    pm["GpuInstance"] = uint64_t(0);
    pm["ComputeInstance"] = uint64_t(0);
    pm["Metric"] = std::string("NVENC");
    pm["MetricId"] = uint64_t(5);
    pm["InstanceBitfield"] = std::vector<uint64_t>{0x01, 0x02, 0x03};

    createNsmPerInstanceGPMMetric(gpmCreator, gpu, inventoryObjPath, interface,
                                  objPath, gpuUuid);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

// ============================================================================
// Test fixture for createNsmGPMMetrics with per-instance interfaces
// (covers getPerInstanceInterfacesAsync loop branches)
// ============================================================================

struct NsmGPMMetricsWithPerInstanceTestFixture :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_GPMMetrics";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpm_with_pi_test";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:2";

    boost::asio::io_context io;
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    size_t initialSensorCount = 0;

    NsmGPMMetricsWithPerInstanceTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        // NsmDevice constructor adds msgTypesSensor to deviceSensors
        initialSensorCount = gpu->deviceSensors.size();
    }

    ~NsmGPMMetricsWithPerInstanceTestFixture()
    {
        cleanupDeviceSensors(devices);
    }

    void setupMainProperties()
    {
        auto& pm = utils::MockDbusAsync::propertyMap(objPath, basicIntfName);
        pm["Name"] = std::string("GPM_Metrics_PI");
        pm["UUID"] = gpuUuid;
        pm["RetrievalSource"] = uint64_t(2);
        pm["GpuInstance"] = uint64_t(0);
        pm["ComputeInstance"] = uint64_t(0);
        pm["MetricsBitfield"] = std::vector<uint64_t>{0x89, 0x04, 0x15};
        pm["InventoryObjPath"] =
            std::string("/xyz/openbmc_project/inventory/gpm_pi");
    }
};

TEST_F(NsmGPMMetricsWithPerInstanceTestFixture,
       WithPerInstanceInterface_LoopExecutes)
{
    setupMainProperties();

    // Set up serviceMap so getPerInstanceInterfacesAsync finds a per-instance
    // interface
    const std::string piIntf = basicIntfName + ".PerInstanceMetrics0";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {piIntf}}};

    // Set up per-instance metric properties (using NVDEC metric)
    auto& piPm = utils::MockDbusAsync::propertyMap(objPath, piIntf);
    piPm["Name"] = std::string("PerInstMetric");
    piPm["Type"] = std::string("GPMPerInstance");
    piPm["RetrievalSource"] = uint64_t(1);
    piPm["GpuInstance"] = uint64_t(0);
    piPm["ComputeInstance"] = uint64_t(0);
    piPm["Metric"] = std::string("NVDEC");
    piPm["MetricId"] = uint64_t(5);
    piPm["InstanceBitfield"] = std::vector<uint64_t>{0x01};

    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(testing::ReturnRef(*objServer));

    createNsmGPMMetrics(mockManager, basicIntfName, objPath);

    // Expect: initial + 1 aggregate sensor + 1 per-instance sensor
    EXPECT_GE(gpu->deviceSensors.size(), initialSensorCount + 2);
}

TEST_F(NsmGPMMetricsWithPerInstanceTestFixture,
       WithNonMatchingServiceMap_NoPerInstance)
{
    setupMainProperties();

    // serviceMap has an interface that does NOT match the per-instance pattern
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager",
         {"xyz.openbmc_project.Configuration.NSM_GPMMetrics.OtherInterface0"}}};

    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(testing::ReturnRef(*objServer));

    createNsmGPMMetrics(mockManager, basicIntfName, objPath);

    // Only the aggregate sensor should be added (no per-instance)
    EXPECT_GE(gpu->deviceSensors.size(), initialSensorCount + 1);
}
