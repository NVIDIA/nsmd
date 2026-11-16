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
 * Branch coverage tests for the memory feature creators in nsmMemory.cpp
 * (createMemoryFeature, createMemoryAttributesFeature,
 * createMemoryLocationFeature).
 *
 * Targets:
 *   - Priority param true / absent: polled clock sensor goes to the priority
 *     or the round-robin queue
 *   - MemoryAttributes: RowRemapping + ECCMode + MemCapUtil sensors
 *   - MemoryLocation: LocationCode / LocationContext present or absent
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "interfaceWrapper.hpp"
#include "nsmMemory.hpp"

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

class NsmMemoryFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmMemoryFactoryBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmMemoryFactoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    /** @brief Run a memory feature creator of the device "HGX_<baseName>"
     *  (DIMM dimm/<baseName>_DRAM_0) on the mock GPU
     */
    requester::Coroutine runFeature(nsm::features::CreatorFn create,
                                    const std::string& baseName,
                                    const dbus::PropertyMap& params)
    {
        nsm::features::DeviceContext dev;
        dev.name = "HGX_" + baseName;
        dev.kind = "gpu";
        dev.deviceType = NSM_DEV_ID_GPU;
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        dev.device = gpu;
        return create(mockManager, dev, params);
    }

    /** @brief Memory feature params with valid ECC and DIMM device type */
    static dbus::PropertyMap memoryParams(const std::string& name)
    {
        return {
            {"Name", name},
            {"ErrorCorrection",
             std::string(
                 "xyz.openbmc_project.Inventory.Item.Dimm.Ecc.MultiBitECC")},
            {"DeviceType",
             std::string("xyz.openbmc_project.Inventory.Item.Dimm."
                         "DeviceType.HBM3")}};
    }
};

// ============================================================================
// Priority absent -> priority=false -> clock sensor polled round robin
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, Feature_Memory_MissingPriority)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    const size_t prBefore = gpu->prioritySensors.size();
    auto rc = runFeature(nsm::features::createMemoryFeature, "GPU_FbrNoPrio",
                         memoryParams("Memory_FBr_NoPrio"));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // currClockFreqSensor added with priority=false -> roundRobin
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
    EXPECT_EQ(gpu->prioritySensors.size(), prBefore);
}

// ============================================================================
// Priority=true -> clock sensor polled with priority
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, Feature_Memory_PriorityTrue)
{
    auto params = memoryParams("Memory_FBr_Prio");
    params["Priority"] = true;

    const size_t prBefore = gpu->prioritySensors.size();
    auto rc = runFeature(nsm::features::createMemoryFeature, "GPU_FbrPrio",
                         params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->prioritySensors.size(), prBefore);
}

// ============================================================================
// MemoryAttributes -> createMemoryRowRemapping + ECCMode + MemCapUtil
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, Feature_MemoryAttributes)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createMemoryAttributesFeature,
                         "GPU_FbrAttrs",
                         {{"Name", std::string("Memory_FBr_Attrs")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // 3 roundRobin sensors: RowRemapState, RowRemappingCounts,
    // RemappingAvailability + EccErrorCountsDram + MemoryCapacityUtil
    EXPECT_GT(gpu->roundRobinSensors.size(), before);
}

// ============================================================================
// MemoryLocation with LocationContext only -> one decorator
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, Feature_MemoryLocation_ContextOnly)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = runFeature(nsm::features::createMemoryLocationFeature,
                         "GPU_FbrLocCtx",
                         {{"Name", std::string("Memory_FBr_LocCtx")},
                          {"LocationContext", std::string("GPU_0")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before + 1);
}

// ============================================================================
// MemoryLocation without location params -> nothing published
// ============================================================================
TEST_F(NsmMemoryFactoryBranchTest, Feature_MemoryLocation_NoParams)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = runFeature(nsm::features::createMemoryLocationFeature,
                         "GPU_FbrLocNone",
                         {{"Name", std::string("Memory_FBr_LocNone")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}
