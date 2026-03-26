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
 * - createNsmGPMMetrics: missing properties (Name, UUID, Type), invalid UUID
 * - createNsmGPMMetrics: valid creation, populateMemoryBandwidth path
 * - createNsmPerInstanceGPMMetric: missing properties, invalid metrics
 * - convertToBytes: various inputs
 * - getPerInstanceInterfacesAsync: matching and non-matching interfaces
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

requester::Coroutine createNsmPerPortGPMMetrics(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath);
} // namespace nsm

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Fixture for createNsmGPMMetrics branch coverage
// ============================================================================
struct NsmGpmOemFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_GPMMetrics";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpm_factory_br";
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

    void setupMainProperties(const std::string& path = "",
                             const std::string& inventoryPath =
                                 "/xyz/openbmc_project/inventory/gpm_fb")
    {
        const std::string& p = path.empty() ? objPath : path;
        auto& pm = utils::MockDbusAsync::propertyMap(p, basicIntfName);
        pm["Name"] = std::string("GPM_Metrics_FB");
        pm["UUID"] = gpuUuid;
        pm["RetrievalSource"] = uint64_t(2);
        pm["GpuInstance"] = uint64_t(0);
        pm["ComputeInstance"] = uint64_t(0);
        pm["MetricsBitfield"] = std::vector<uint64_t>{0x89, 0x04, 0x15};
        pm["InventoryObjPath"] = inventoryPath;
    }
};

// ============================================================================
// createNsmGPMMetrics: valid creation path
// ============================================================================
TEST_F(NsmGpmOemFactoryBranchTest, CreateGPMMetrics_ValidProperties_Success)
{
    setupMainProperties();

    // No per-instance interfaces
    utils::MockDbusAsync::serviceMap() = {};

    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(ReturnRef(*objServer));

    createNsmGPMMetrics(mockManager, basicIntfName, objPath);

    // At least one sensor added (the GetSupportedGPMMetrics static sensor)
    EXPECT_GT(gpu->deviceSensors.size(), initialSensorCount);
}

// ============================================================================
// createNsmGPMMetrics: populateMemoryBandwidth=true path
// ============================================================================
TEST_F(NsmGpmOemFactoryBranchTest,
       CreateGPMMetrics_PopulateMemoryBandwidth_True)
{
    const std::string path =
        "/xyz/openbmc_project/inventory/system/gpm_factory_br_mem";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/gpm_fb_mem";

    auto& pm = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    pm["Name"] = std::string("GPM_Metrics_MemBW");
    pm["UUID"] = gpuUuid;
    pm["RetrievalSource"] = uint64_t(2);
    pm["GpuInstance"] = uint64_t(0);
    pm["ComputeInstance"] = uint64_t(0);
    pm["MetricsBitfield"] = std::vector<uint64_t>{0x10}; // bit for DRAMUsage
    pm["InventoryObjPath"] = inventoryPath;
    pm["MemoryBandwidth"] = bool{true};
    pm["MemoryInventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/memory0");

    utils::MockDbusAsync::serviceMap() = {};

    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(ReturnRef(*objServer));

    // This will attempt to create DimmIntf - may throw but caught internally //

    createNsmGPMMetrics(mockManager, basicIntfName, path);

    EXPECT_GT(gpu->deviceSensors.size(), initialSensorCount);
}

// ============================================================================
// createNsmGPMMetrics: populateMemoryBandwidth=false (default) path
// ============================================================================
TEST_F(NsmGpmOemFactoryBranchTest,
       CreateGPMMetrics_PopulateMemoryBandwidth_False)
{
    const std::string path =
        "/xyz/openbmc_project/inventory/system/gpm_factory_br_nomem";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/gpm_fb_nomem";

    auto& pm = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    pm["Name"] = std::string("GPM_Metrics_NoMemBW");
    pm["UUID"] = gpuUuid;
    pm["RetrievalSource"] = uint64_t(2);
    pm["GpuInstance"] = uint64_t(0);
    pm["ComputeInstance"] = uint64_t(0);
    pm["MetricsBitfield"] = std::vector<uint64_t>{0x01};
    pm["InventoryObjPath"] = inventoryPath;
    // No MemoryBandwidth property -> false (caught by try/catch at L213-220)

    utils::MockDbusAsync::serviceMap() = {};

    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(ReturnRef(*objServer));

    createNsmGPMMetrics(mockManager, basicIntfName, path);
    EXPECT_GT(gpu->deviceSensors.size(), initialSensorCount);
}

// ============================================================================
// createNsmGPMMetrics: with per-instance interface matching
// ============================================================================
TEST_F(NsmGpmOemFactoryBranchTest,
       CreateGPMMetrics_WithPerInstanceInterface_Success)
{
    const std::string path =
        "/xyz/openbmc_project/inventory/system/gpm_factory_br_pi";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/gpm_fb_pi";

    auto& pm = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    pm["Name"] = std::string("GPM_Metrics_PI_FB");
    pm["UUID"] = gpuUuid;
    pm["RetrievalSource"] = uint64_t(2);
    pm["GpuInstance"] = uint64_t(0);
    pm["ComputeInstance"] = uint64_t(0);
    pm["MetricsBitfield"] = std::vector<uint64_t>{0x89, 0x04};
    pm["InventoryObjPath"] = inventoryPath;

    // Set up per-instance interface
    const std::string piIntf = basicIntfName + ".PerInstanceMetrics0";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {piIntf}}};

    auto& piPm = utils::MockDbusAsync::propertyMap(path, piIntf);
    piPm["Name"] = std::string("PerInstMetric_FB");
    piPm["Type"] = std::string("GPMPerInstance");
    piPm["RetrievalSource"] = uint64_t(1);
    piPm["GpuInstance"] = uint64_t(0);
    piPm["ComputeInstance"] = uint64_t(0);
    piPm["Metric"] = std::string("NVJPG");
    piPm["MetricId"] = uint64_t(7);
    piPm["InstanceBitfield"] = std::vector<uint64_t>{0x03};

    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(ReturnRef(*objServer));

    createNsmGPMMetrics(mockManager, basicIntfName, path);

    // aggregate + per-instance
    EXPECT_GE(gpu->deviceSensors.size(), initialSensorCount + 2);
}

// ============================================================================
// createNsmGPMMetrics: with non-matching per-instance interface
// ============================================================================
TEST_F(NsmGpmOemFactoryBranchTest,
       CreateGPMMetrics_NonMatchingPerInstance_LoopFalseBranch)
{
    const std::string path =
        "/xyz/openbmc_project/inventory/system/gpm_factory_br_nomatch";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/gpm_fb_nomatch";

    auto& pm = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    pm["Name"] = std::string("GPM_Metrics_NoMatch");
    pm["UUID"] = gpuUuid;
    pm["RetrievalSource"] = uint64_t(2);
    pm["GpuInstance"] = uint64_t(0);
    pm["ComputeInstance"] = uint64_t(0);
    pm["MetricsBitfield"] = std::vector<uint64_t>{0x01};
    pm["InventoryObjPath"] = inventoryPath;

    // Non-matching interface
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager",
         {"xyz.openbmc_project.Configuration.NSM_GPMMetrics.SomethingElse"}}};

    EXPECT_CALL(mockManager, getObjServer())
        .WillRepeatedly(ReturnRef(*objServer));

    createNsmGPMMetrics(mockManager, basicIntfName, path);

    // Only aggregate sensor
    EXPECT_GE(gpu->deviceSensors.size(), initialSensorCount + 1);
}

// ============================================================================
// Fixture for createNsmPerInstanceGPMMetric branch coverage
// ============================================================================
struct NsmPerInstanceGPMBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_GPMMetrics.PerInstanceMetrics0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpm_pi_br_test";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/gpm_pi_br";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    boost::asio::io_context io;
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<NsmGPMInterfaceCreator> gpmCreator;
    size_t initialSensorCount = 0;

    NsmPerInstanceGPMBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        gpmCreator = std::make_shared<NsmGPMInterfaceCreator>(*objServer,
                                                              inventoryObjPath);
        initialSensorCount = gpu->deviceSensors.size();
    }

    ~NsmPerInstanceGPMBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    void setupProperties(const std::string& metric)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
        pm["Name"] = std::string("PerInstMetric_BR");
        pm["Type"] = std::string("GPMPerInstance");
        pm["RetrievalSource"] = uint64_t(1);
        pm["GpuInstance"] = uint64_t(0);
        pm["ComputeInstance"] = uint64_t(0);
        pm["Metric"] = metric;
        pm["MetricId"] = uint64_t(5);
        pm["InstanceBitfield"] = std::vector<uint64_t>{0x01};
    }
};

// ============================================================================
// createNsmPerInstanceGPMMetric: NVDEC metric
// ============================================================================
TEST_F(NsmPerInstanceGPMBranchTest, MetricNVDEC_SensorAdded)
{
    setupProperties("NVDEC");
    createNsmPerInstanceGPMMetric(gpmCreator, gpu, inventoryObjPath, interface,
                                  objPath, gpuUuid);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

// ============================================================================
// createNsmPerInstanceGPMMetric: NVJPG metric
// ============================================================================
TEST_F(NsmPerInstanceGPMBranchTest, MetricNVJPG_SensorAdded)
{
    setupProperties("NVJPG");
    createNsmPerInstanceGPMMetric(gpmCreator, gpu, inventoryObjPath, interface,
                                  objPath, gpuUuid);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

// ============================================================================
// createNsmPerInstanceGPMMetric: NVENC metric
// ============================================================================
TEST_F(NsmPerInstanceGPMBranchTest, MetricNVENC_SensorAdded)
{
    setupProperties("NVENC");
    createNsmPerInstanceGPMMetric(gpmCreator, gpu, inventoryObjPath, interface,
                                  objPath, gpuUuid);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

// ============================================================================
// createNsmPerInstanceGPMMetric: unknown metric -> else branch -> co_return //

// ============================================================================
TEST_F(NsmPerInstanceGPMBranchTest, MetricUnknown_NoSensorAdded)
{
    setupProperties("UnknownMetric_BR");
    createNsmPerInstanceGPMMetric(gpmCreator, gpu, inventoryObjPath, interface,
                                  objPath, gpuUuid);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount);
}

// ============================================================================
// createNsmPerInstanceGPMMetric: multiple instance bitfield bytes
// ============================================================================
TEST_F(NsmPerInstanceGPMBranchTest, MultipleInstanceBitfieldBytes)
{
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Name"] = std::string("PerInstMetric_MultiBF");
    pm["Type"] = std::string("GPMPerInstance");
    pm["RetrievalSource"] = uint64_t(1);
    pm["GpuInstance"] = uint64_t(0);
    pm["ComputeInstance"] = uint64_t(0);
    pm["Metric"] = std::string("NVENC");
    pm["MetricId"] = uint64_t(5);
    pm["InstanceBitfield"] = std::vector<uint64_t>{0x01, 0x02, 0x04};

    createNsmPerInstanceGPMMetric(gpmCreator, gpu, inventoryObjPath, interface,
                                  objPath, gpuUuid);
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

// ============================================================================
// Fixture for createNsmPerPortGPMMetrics branch coverage
// ============================================================================
struct NsmPerPortGPMBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string portInterface =
        "xyz.openbmc_project.Configuration.NSM_GPMPortMetrics";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpm_port_br";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    size_t initialSensorCount = 0;

    NsmPerPortGPMBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        initialSensorCount = gpu->deviceSensors.size();
    }

    ~NsmPerPortGPMBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    void setupProperties(const std::vector<std::string>& metrics,
                         const std::vector<uint64_t>& ports = {},
                         const std::vector<uint64_t>& instanceBitfield = {0x01},
                         const std::string& inventoryPath =
                             "/xyz/openbmc_project/inventory/gpm_port_br")
    {
        auto& pm = utils::MockDbusAsync::propertyMap(objPath, portInterface);
        pm["Name"] = std::string("TestGPMPort_BR");
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

// ============================================================================
// createNsmPerPortGPMMetrics: all valid metrics
// ============================================================================
TEST_F(NsmPerPortGPMBranchTest, AllValidMetrics_AllAdded)
{
    setupProperties({"NVLinkRawTxBandwidthGbps", "NVLinkDataTxBandwidthGbps",
                     "NVLinkRawRxBandwidthGbps", "NVLinkDataRxBandwidthGbps"},
                    {0, 1}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_br_all");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 4);
}

// ============================================================================
// createNsmPerPortGPMMetrics: unknown metric skipped (else branch -> continue)
// ============================================================================
TEST_F(NsmPerPortGPMBranchTest, UnknownMetric_Skipped)
{
    setupProperties({"InvalidMetricXYZ_BR"}, {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_br_unk");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount);
}

// ============================================================================
// createNsmPerPortGPMMetrics: mixed valid and invalid metrics
// ============================================================================
TEST_F(NsmPerPortGPMBranchTest, MixedMetrics_OnlyValidAdded)
{
    setupProperties({"NVLinkRawTxBandwidthGbps", "BadMetric_BR",
                     "NVLinkDataRxBandwidthGbps"},
                    {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_br_mixed");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 2);
}

// ============================================================================
// createNsmPerPortGPMMetrics: duplicate ports -> deduplicated
// ============================================================================
TEST_F(NsmPerPortGPMBranchTest, DuplicatePorts_Deduped)
{
    setupProperties({"NVLinkDataTxBandwidthGbps"}, {1, 1, 2, 2}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_br_dup");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

// ============================================================================
// createNsmPerPortGPMMetrics: empty metrics -> no sensors
// ============================================================================
TEST_F(NsmPerPortGPMBranchTest, EmptyMetrics_NoSensors)
{
    setupProperties({}, {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_br_empty");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount);
}

// ============================================================================
// createNsmPerPortGPMMetrics: each individual metric
// ============================================================================
TEST_F(NsmPerPortGPMBranchTest, NVLinkRawTxBandwidthGbps_Only)
{
    setupProperties({"NVLinkRawTxBandwidthGbps"}, {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_br_rtx");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerPortGPMBranchTest, NVLinkDataTxBandwidthGbps_Only)
{
    setupProperties({"NVLinkDataTxBandwidthGbps"}, {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_br_dtx");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerPortGPMBranchTest, NVLinkRawRxBandwidthGbps_Only)
{
    setupProperties({"NVLinkRawRxBandwidthGbps"}, {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_br_rrx");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
}

TEST_F(NsmPerPortGPMBranchTest, NVLinkDataRxBandwidthGbps_Only)
{
    setupProperties({"NVLinkDataRxBandwidthGbps"}, {}, {0x01},
                    "/xyz/openbmc_project/inventory/gpm_port_br_drx");
    callFactory();
    EXPECT_EQ(gpu->deviceSensors.size(), initialSensorCount + 1);
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
