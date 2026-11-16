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
 * Branch coverage tests for the createNVLinkFeature creator coroutine
 * in nsmd/nsmPort/nsmPort.cpp: absent optional params (Count,
 * ParentObjPath, Priority) and the per-port characteristics sensor choice.
 */

#include "base.h"
#include "network-ports.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmPort.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================
struct NsmPortFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:8";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPortFactoryBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPortFactoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    /** @brief Drive the event loop until the task finishes.
     *
     * createNVLinkFeature yields between ports, so the loop has to run for
     * construction to complete. Every call site must hold the returned task
     * and run this: discarding the temporary destroys a suspended coroutine
     * frame while its timer is still armed on the loop.
     * Bounded so a task that never finishes fails here instead of hanging.
     */
    void runUntilDone(const requester::Coroutine& task) const
    {
        auto* loop =
            static_cast<const sdeventplus::Event&>(gpu->getEvent()).get();
        for (int i = 0; i < 1000 && !task.done(); ++i)
        {
            sd_event_run(loop, 0);
        }
        ASSERT_TRUE(task.done());
    }

    /** @brief Run the NVLink feature of the GPU with the given params */
    requester::Coroutine createNVLink(const dbus::PropertyMap& params)
    {
        nsm::features::DeviceContext dev;
        dev.name = "HGX_GPU_0";
        dev.kind = "gpu";
        dev.deviceType = NSM_DEV_ID_GPU;
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        dev.device = gpu;
        return nsm::features::createNVLinkFeature(mockManager, dev, params);
    }
};

// ============================================================================
// Count=0: no port loop iterations, nothing registered
// ============================================================================
TEST_F(NsmPortFactoryBranchTest, Feature_CountZero)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto result = createNVLink(
        {{"Name", std::string("NVLink_Zero")},
         {"ParentObjPath",
          std::string("/xyz/openbmc_project/inventory/system/GPU_0")},
         {"Priority", false},
         {"Count", uint64_t{0}}});
    runUntilDone(result);

    EXPECT_EQ(result.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->roundRobinSensors.size(), before);
}

// ============================================================================
// ParentObjPath absent but Count > 0: ports still created
// ============================================================================
TEST_F(NsmPortFactoryBranchTest, Feature_NoParentObjPath)
{
    auto result = createNVLink({{"Name", std::string("NVLink_NoParent")},
                                {"Priority", false},
                                {"Count", uint64_t{1}}});
    runUntilDone(result);

    // Port sensors created using empty parent path
    EXPECT_EQ(result.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Priority absent: defaults to false, sensors go round robin
// ============================================================================
TEST_F(NsmPortFactoryBranchTest, Feature_NoPriority)
{
    auto result = createNVLink(
        {{"Name", std::string("NVLink_NoPrio")},
         {"ParentObjPath",
          std::string("/xyz/openbmc_project/inventory/system/GPU_2")},
         {"Count", uint64_t{1}}});
    runUntilDone(result);

    EXPECT_EQ(result.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Every NVLink port gets NsmPortCharacteristicsV2; the device, not the
// feature params, decide between 0x12 and the 0x42 fallback.
// ============================================================================
namespace
{
template <typename SensorType>
size_t countSensors(const std::shared_ptr<MockNsmDevice>& device)
{
    size_t count = 0;
    for (const auto& sensor : device->roundRobinSensors)
    {
        if (std::dynamic_pointer_cast<SensorType>(sensor))
        {
            ++count;
        }
    }
    return count;
}
} // namespace

TEST_F(NsmPortFactoryBranchTest, Factory_NvlinkPort_CreatesV2)
{
    auto result = createNVLink(
        {{"Name", std::string("NVLink_V2")},
         {"ParentObjPath",
          std::string("/xyz/openbmc_project/inventory/system/GPU_V2")},
         {"Priority", false},
         {"Count", uint64_t{2}}});
    runUntilDone(result);

    // One v2 sensor per port; the 0x42 fallback sensor is owned by it, not
    // registered on the device.
    EXPECT_EQ(countSensors<NsmPortCharacteristicsV2>(gpu), 2u);
    EXPECT_EQ(countSensors<NsmPortCharacteristics>(gpu), 0u);
    for (const auto& sensor : gpu->roundRobinSensors)
    {
        if (auto v2 =
                std::dynamic_pointer_cast<NsmPortCharacteristicsV2>(sensor))
        {
            // 0x12 has not answered yet: the clear action is not advertised.
            EXPECT_FALSE(v2->isFallbackActive());
            EXPECT_FALSE(v2->portHealthMetricsIntf
                             ->clearEarlyHealthIndicationSupported());
        }
    }
}

TEST_F(NsmPortFactoryBranchTest, Factory_StaleLinkHealthKey_IsIgnored)
{
    // Params that still carry the withdrawn LinkHealthExtensionSupported
    // key must not change the sensor choice.
    auto result = createNVLink(
        {{"Name", std::string("NVLink_Stale")},
         {"ParentObjPath",
          std::string("/xyz/openbmc_project/inventory/system/GPU_Stale")},
         {"Priority", false},
         {"Count", uint64_t{1}},
         {"LinkHealthExtensionSupported", false}});
    runUntilDone(result);

    EXPECT_EQ(countSensors<NsmPortCharacteristicsV2>(gpu), 1u);
    EXPECT_EQ(countSensors<NsmPortCharacteristics>(gpu), 0u);
}
