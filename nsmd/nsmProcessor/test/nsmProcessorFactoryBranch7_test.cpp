/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage batch 7 for the processor feature creators
 * (nsm::features::create*Feature in nsmd/nsmProcessor/nsmProcessor.cpp):
 *
 * Targets individual *Supported flags TRUE while all others absent,
 * Processor with PCIeEnabled and PCIePortCount=0/1/2, the Processor objects,
 * and the ProcessorPerformance / PowerCap / ReconfigPermissions /
 * WorkloadPowerProfile creators.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmProcessor.hpp"

#undef private
#undef protected

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmReconfigPermissions.hpp"

#include <algorithm>

using namespace nsm;

struct NsmProcessorFactory7Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:77";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactory7Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactory7Test()
    {
        cleanupDeviceSensors(devices);
    }

    /** @brief Run a processor feature creator for the GPU `name` on the mock
     *  NsmDevice; the inventory object is accelerator/<name>
     */
    requester::Coroutine runFeature(nsm::features::CreatorFn create,
                                    const std::string& name,
                                    dbus::PropertyMap params = {})
    {
        nsm::features::DeviceContext dev;
        dev.name = name;
        dev.kind = "gpu";
        dev.staticUuid = gpuUuid;
        dev.device = gpu;
        params["Name"] = name;
        return create(mockManager, dev, params);
    }

    /** @brief Number of sensors of type T in the queue */
    template <typename T, typename Queue>
    static size_t countOf(const Queue& queue)
    {
        return std::count_if(queue.begin(), queue.end(), [](const auto& s) {
            return std::dynamic_pointer_cast<T>(s) != nullptr;
        });
    }
};

// ============================================================================
// ProcessorAttributes: each *Supported individually TRUE
// ============================================================================

TEST_F(NsmProcessorFactory7Test, Attributes_MIGModeOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_MIG", {{"MIGModeSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_PortDisableFutureOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_PDF", {{"PortDisableFutureSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_ECCModeOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_ECC", {{"ECCModeSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_EDPpScalingFactorOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_EDPp", {{"EDPpScalingFactorSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_CpuOperatingConfigOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_CpuOp",
                         {{"CpuOperatingConfigSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_MemCapacityUtilOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_MemCap", {{"MemCapacityUtilSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_TotalNvLinksCountOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_NvLink",
                         {{"TotalNvLinksCountSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_EGMModeOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_EGM", {{"EGMModeSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_MNNVLTopologyOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_MNNVL", {{"MNNVLTopologySupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_MctpNsmOperationalStatusOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_OpStat",
                         {{"MctpNsmOperationalStatusSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// Processor PCIe: Count = 0, 1, 2
// ============================================================================

TEST_F(NsmProcessorFactory7Test, PCIe_Count0)
{
    auto rc = runFeature(nsm::features::createProcessorFeature, "GPU_F7_PCIe0",
                         {{"PCIeEnabled", true},
                          {"PCIeDeviceId", uint64_t{0}},
                          {"PCIePortCount", uint64_t{0}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Only the PCIeLinkSpeed sensor, no port sensors
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_EQ(countOf<NsmPciePortIntf>(gpu->deviceSensors), 0u);
}

TEST_F(NsmProcessorFactory7Test, PCIe_Count1)
{
    auto rc = runFeature(nsm::features::createProcessorFeature, "GPU_F7_PCIe1",
                         {{"PCIeEnabled", true},
                          {"PCIeDeviceId", uint64_t{1}},
                          {"PCIePortCount", uint64_t{1}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(countOf<NsmPciePortIntf>(gpu->deviceSensors), 1u);
    EXPECT_EQ(countOf<NsmPciGroup2>(gpu->roundRobinSensors), 1u);
}

TEST_F(NsmProcessorFactory7Test, PCIe_Count2)
{
    auto rc = runFeature(nsm::features::createProcessorFeature, "GPU_F7_PCIe2",
                         {{"PCIeEnabled", true},
                          {"PCIeDeviceId", uint64_t{2}},
                          {"PCIePortCount", uint64_t{2}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 2u);
    EXPECT_EQ(countOf<NsmPciePortIntf>(gpu->deviceSensors), 2u);
    EXPECT_EQ(countOf<NsmPciGroup2>(gpu->roundRobinSensors), 2u);
}

TEST_F(NsmProcessorFactory7Test, PCIe_NoDeviceIdNoCount)
{
    // No PCIeDeviceId, no PCIePortCount -> defaults to 0
    auto rc = runFeature(nsm::features::createProcessorFeature,
                         "GPU_F7_PCIeNone", {{"PCIeEnabled", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// Processor: main processor objects
// ============================================================================

TEST_F(NsmProcessorFactory7Test, Processor_Created)
{
    auto rc = runFeature(nsm::features::createProcessorFeature, "GPU_F7_UUID");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
    // NsmUuidIntf starts empty; update() reads the device UUID over MCTP
    EXPECT_EQ(countOf<NsmUuidIntf>(gpu->staticSensors), 1u);
}

// ============================================================================
// ProcessorPerformance
// ============================================================================

TEST_F(NsmProcessorFactory7Test, ProcessorPerformance_WithDeviceId)
{
    auto rc = runFeature(nsm::features::createProcessorPerformanceFeature,
                         "GPU_F7_Perf", {{"DeviceId", uint64_t{3}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, ProcessorPerformance_NoDeviceId)
{
    // No DeviceId -> default 0
    auto rc = runFeature(nsm::features::createProcessorPerformanceFeature,
                         "GPU_F7_PerfND");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, PowerCap_Created)
{
    auto rc = runFeature(nsm::features::createPowerCapFeature,
                         "GPU_F7_PwrCapNC");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ReconfigPermissions
// ============================================================================

TEST_F(NsmProcessorFactory7Test, ReconfigPermissions_WithFeatures)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F7_Reconf",
        {{"AvailableKnobs", std::vector<std::string>{"CCMode", "ECCEnable"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, ReconfigPermissions_NoFeatures)
{
    // No AvailableKnobs -> no knob sensors
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createReconfigPermissionsFeature,
                         "GPU_F7_ReconfNF");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// WorkloadPowerProfile
// ============================================================================

TEST_F(NsmProcessorFactory7Test, WorkloadPowerProfile_Created)
{
    auto rc = runFeature(nsm::features::createWorkloadPowerProfileFeature,
                         "GPU_F7_WPP");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// Attributes: LocationType + LocationCode both present
// ============================================================================

TEST_F(NsmProcessorFactory7Test, Attributes_LocationTypeAndCode)
{
    auto rc = runFeature(
        nsm::features::createProcessorAttributesFeature, "GPU_F7_LocBoth",
        {{"LocationType",
          std::string(
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
              "Embedded")},
         {"LocationCode", std::string("GPU_Bay_7")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// Attributes: neither LocationType nor LocationCode
// ============================================================================

TEST_F(NsmProcessorFactory7Test, Attributes_NoLocation)
{
    // No LocationType, no LocationCode, no Supported flags
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F7_NoLoc");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Only asset sensors created
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}
