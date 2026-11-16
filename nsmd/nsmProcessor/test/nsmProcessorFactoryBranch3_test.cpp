/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Additional coverage for the processor feature creators
 * (nsm::features::create*Feature in nsmd/nsmProcessor/nsmProcessor.cpp):
 * - ProcessorAttributes with optional params TRUE
 * - PowerCap
 * - ReconfigPermissions without knobs
 * - WorkloadPowerProfile
 * - every processor creator without an NsmDevice
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

struct NsmProcessorFactory3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:8";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactory3Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactory3Test()
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
// ProcessorAttributes with LocationType and LocationCode
// ============================================================================
TEST_F(NsmProcessorFactory3Test, Attributes_WithLocation)
{
    auto rc = runFeature(
        nsm::features::createProcessorAttributesFeature, "GPU_F3_Loc",
        {{"LocationType",
          std::string(
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
              "Embedded")},
         {"LocationCode", std::string("GPU_Slot_0")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(countOf<NsmLocationIntfProcessor>(gpu->deviceSensors), 1u);
    EXPECT_EQ(countOf<NsmLocationCodeIntfProcessor>(gpu->deviceSensors), 1u);
}

// ============================================================================
// ProcessorAttributes with MIGMode, ECC, EDPp, PortDisableFuture enabled
// ============================================================================
TEST_F(NsmProcessorFactory3Test, Attributes_WithMIGAndECC)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F3_MIG",
                         {{"MIGModeSupported", true},
                          {"ECCModeSupported", true},
                          {"EDPpScalingFactorSupported", true},
                          {"PortDisableFutureSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(countOf<NsmMigMode>(gpu->longRunningSensors), 1u);
    EXPECT_EQ(countOf<NsmEccMode>(gpu->longRunningSensors), 1u);
}

// ============================================================================
// ProcessorAttributes with remaining supported flags
// ============================================================================
TEST_F(NsmProcessorFactory3Test, Attributes_WithRemainingFlags)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F3_Flags",
                         {{"CpuOperatingConfigSupported", true},
                          {"MemCapacityUtilSupported", true},
                          {"TotalNvLinksCountSupported", true},
                          {"EGMModeSupported", true},
                          {"MNNVLTopologySupported", true},
                          {"MctpNsmOperationalStatusSupported", true},
                          {"GPUBasePowerLimitSupported", true},
                          {"GPUCopyCPUPowerLimitSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// PowerCap feature
// ============================================================================
TEST_F(NsmProcessorFactory3Test, PowerCap_Type)
{
    const size_t powerCapsBefore = mockManager.powerCapList.size();
    auto rc = runFeature(nsm::features::createPowerCapFeature, "GPU_F3_PCap");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(countOf<NsmPowerCap>(gpu->roundRobinSensors), 1u);
    EXPECT_EQ(mockManager.powerCapList.size(), powerCapsBefore + 1);
}

// ============================================================================
// ReconfigPermissions feature without knobs
// ============================================================================
TEST_F(NsmProcessorFactory3Test, ReconfigPermissions_Type)
{
    auto rc = runFeature(nsm::features::createReconfigPermissionsFeature,
                         "GPU_F3_Reconfig");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(countOf<NsmReconfigPermissions>(gpu->roundRobinSensors), 0u);
}

// ============================================================================
// WorkloadPowerProfile feature
// ============================================================================
TEST_F(NsmProcessorFactory3Test, WorkloadPowerProfile_Type)
{
    auto rc = runFeature(nsm::features::createWorkloadPowerProfileFeature,
                         "GPU_F3_WPP");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(countOf<NsmWorkLoadProfileEnum>(gpu->staticSensors), 1u);
}

// ============================================================================
// No NsmDevice: every processor feature fails before creating anything
// ============================================================================
TEST_F(NsmProcessorFactory3Test, NoNsmDevice_ReturnsError)
{
    const nsm::features::CreatorFn creators[] = {
        nsm::features::createProcessorFeature,
        nsm::features::createProcessorAttributesFeature,
        nsm::features::createPowerCapFeature,
        nsm::features::createWorkloadPowerProfileFeature,
        nsm::features::createProcessorPerformanceFeature,
        nsm::features::createReconfigPermissionsFeature,
    };

    nsm::features::DeviceContext dev;
    dev.name = "GPU_F3_NoDevice";
    dev.kind = "gpu";
    dev.staticUuid = gpuUuid;
    const dbus::PropertyMap params{{"Name", std::string("GPU_F3_NoDevice")}};

    const size_t before = gpu->deviceSensors.size();
    for (auto create : creators)
    {
        auto rc = create(mockManager, dev, params);
        EXPECT_FALSE(rc.exception());
        EXPECT_EQ(rc.data(), NSM_ERROR);
    }
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}
