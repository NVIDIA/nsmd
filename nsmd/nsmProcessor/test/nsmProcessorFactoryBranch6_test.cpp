/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage batch 6 for nsm::features::createProcessorAttributesFeature:
 *
 * Targets the FALSE side of every "*Supported" param that IS present but
 * false (param<bool>(params, key).value_or(false) == false), e.g.
 * MIGModeSupported, PortDisableFutureSupported, ECCModeSupported, ...,
 * GPUCopyCPUPowerLimitSupported.
 *
 * Also covers:
 *   - all Supported = false (no sub-sensors)
 *   - first group true, second group false and vice versa
 *   - LocationType without LocationCode and vice versa
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

struct NsmProcessorFactory6Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:11";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactory6Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactory6Test()
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
// ProcessorAttributes: ALL bool Supported properties present but FALSE
// Each param is present but false -> body skipped
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_AllSupportedFalse)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F6_AllFalse",
                         {{"MIGModeSupported", false},
                          {"PortDisableFutureSupported", false},
                          {"ECCModeSupported", false},
                          {"EDPpScalingFactorSupported", false},
                          {"PowerSmoothingSupported", false},
                          {"CpuOperatingConfigSupported", false},
                          {"MemCapacityUtilSupported", false},
                          {"TotalNvLinksCountSupported", false},
                          {"EGMModeSupported", false},
                          {"MNNVLTopologySupported", false},
                          {"MctpNsmOperationalStatusSupported", false},
                          {"GPUBasePowerLimitSupported", false},
                          {"GPUCopyCPUPowerLimitSupported", false}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Only asset sensors created, no MIG/ECC/EDPp/PowerSmoothing etc.
    // The Attributes block always creates asset + inventory sensors
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    // None of the bool-guarded sub-functions should have added RR sensors
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// ProcessorAttributes: first group TRUE, second group FALSE
// Covers MIG/ECC/EDPp/PortDisable=true, rest=false
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_FirstGroupTrue_SecondFalse)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F6_Split",
                         {{"MIGModeSupported", true},
                          {"PortDisableFutureSupported", true},
                          {"ECCModeSupported", true},
                          {"EDPpScalingFactorSupported", true},
                          {"PowerSmoothingSupported", false},
                          {"CpuOperatingConfigSupported", false},
                          {"MemCapacityUtilSupported", false},
                          {"TotalNvLinksCountSupported", false},
                          {"EGMModeSupported", false},
                          {"MNNVLTopologySupported", false},
                          {"MctpNsmOperationalStatusSupported", false},
                          {"GPUBasePowerLimitSupported", false},
                          {"GPUCopyCPUPowerLimitSupported", false}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // MIG/ECC/EDPp/PortDisable sensors created; rest skipped
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: second group TRUE, first group FALSE
// Covers CpuOp/MemCap/TotalNvLinks/EGM/MNNVL/GpuOpStatus/GPUBase/GPUCopy=true
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_SecondGroupTrue_FirstFalse)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F6_Split2",
                         {{"MIGModeSupported", false},
                          {"PortDisableFutureSupported", false},
                          {"ECCModeSupported", false},
                          {"EDPpScalingFactorSupported", false},
                          {"PowerSmoothingSupported", false},
                          {"CpuOperatingConfigSupported", true},
                          {"MemCapacityUtilSupported", true},
                          {"TotalNvLinksCountSupported", true},
                          {"EGMModeSupported", true},
                          {"MNNVLTopologySupported", true},
                          {"MctpNsmOperationalStatusSupported", true},
                          {"GPUBasePowerLimitSupported", true},
                          {"GPUCopyCPUPowerLimitSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: PowerSmoothing=true only (all others absent)
// Covers PowerSmoothingSupported TRUE with minimum config
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_PowerSmoothingOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F6_PSOnly", {{"PowerSmoothingSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // PowerSmoothing creates many device sensors
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: LocationType only (no LocationCode)
// Covers: LocationType present, LocationCode absent
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_LocationTypeNoCode)
{
    auto rc = runFeature(
        nsm::features::createProcessorAttributesFeature, "GPU_F6_LocTypeOnly",
        {{"LocationType",
          std::string(
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
              "Embedded")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(countOf<NsmLocationIntfProcessor>(gpu->deviceSensors), 1u);
    EXPECT_EQ(countOf<NsmLocationCodeIntfProcessor>(gpu->deviceSensors), 0u);
}

// ============================================================================
// ProcessorAttributes: LocationCode only (no LocationType)
// Covers: LocationType absent, LocationCode present
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_LocationCodeNoType)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F6_LocCodeOnly",
                         {{"LocationCode", std::string("GPU_Slot_1")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(countOf<NsmLocationIntfProcessor>(gpu->deviceSensors), 0u);
    EXPECT_EQ(countOf<NsmLocationCodeIntfProcessor>(gpu->deviceSensors), 1u);
}

// ============================================================================
// ProcessorAttributes: GPUBasePowerLimit only TRUE (rest absent)
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_GPUBasePowerLimitOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F6_GPUBase",
                         {{"GPUBasePowerLimitSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: GPUCopyCPUPowerLimit only TRUE (rest absent)
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_GPUCopyCPUPowerLimitOnly)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F6_GPUCopy",
                         {{"GPUCopyCPUPowerLimitSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: GPUBasePowerLimit=false, GPUCopyCPU=false
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_GPUPowerLimitsBothFalse)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F6_GPUBothFalse",
                         {{"GPUBasePowerLimitSupported", false},
                          {"GPUCopyCPUPowerLimitSupported", false}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // No power limit sensors added
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}
