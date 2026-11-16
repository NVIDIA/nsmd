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
 * Branch coverage tests for the processor feature creators in
 * nsmd/nsmProcessor/nsmProcessor.cpp (nsm::features::create*Feature).
 *
 * Targets the FALSE branches of the optional feature params:
 *   ProcessorAttributes: LocationType, LocationCode and every *Supported flag
 *                        absent or false
 *   Processor:           PCIeEnabled with PCIeDeviceId / PCIePortCount absent
 *   ProcessorPerformance: DeviceId absent
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

// ============================================================================
// Fixture
// ============================================================================
struct NsmProcessorFactoryBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:7";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactoryBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactoryBranch2Test()
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
// ProcessorAttributes with NO optional params at all
// This exercises all the FALSE branches for every "Supported" param and
// LocationType/LocationCode
// ============================================================================
TEST_F(NsmProcessorFactoryBranch2Test,
       ProcessorAttributes_AllOptionalPropsAbsent)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    const size_t staticBefore = gpu->staticSensors.size();

    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_FB2_Attr_NoOpt");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Only the asset sensors (part number, serial number, model); no feature
    // sensors
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore + 3);
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// Processor with PCIeEnabled: PCIeDeviceId and PCIePortCount absent
// ============================================================================
TEST_F(NsmProcessorFactoryBranch2Test, PCIe_NoDeviceIdNoCount)
{
    auto rc = runFeature(nsm::features::createProcessorFeature,
                         "GPU_FB2_PCIe_NoDevIdCount", {{"PCIeEnabled", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // createPCIe still adds the NsmPCIeLinkSpeed sensor, but no port sensors
    // since the port count defaults to 0
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
    EXPECT_EQ(countOf<NsmPciGroup2>(gpu->roundRobinSensors), 0u);
    EXPECT_EQ(countOf<NsmPciePortIntf>(gpu->deviceSensors), 0u);
}

// ============================================================================
// ProcessorPerformance: DeviceId absent (FALSE branch)
// ============================================================================
TEST_F(NsmProcessorFactoryBranch2Test, ProcessorPerf_NoDeviceId)
{
    auto rc = runFeature(nsm::features::createProcessorPerformanceFeature,
                         "GPU_FB2_Perf_NoDevId");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Still creates throttle/gpu-util/pci sensors
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
}

// ============================================================================
// ProcessorAttributes: each "Supported" flag present but set to false
// ============================================================================
TEST_F(NsmProcessorFactoryBranch2Test,
       ProcessorAttributes_AllSupportedFlagsFalse)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();

    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_FB2_Attr_AllFalse",
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

    // No feature sensors created, only asset sensors
    EXPECT_GE(gpu->staticSensors.size(), 1u);
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}
