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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "diagnostics.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmProcessor.hpp"
#include "nsmWorkloadPowerProfile.hpp"

#undef private
#undef protected

using namespace nsm;

static auto busPFB = sdbusplus::bus::new_default();

// =============================================================================
// Processor feature creators: createWorkloadPowerProfileFeature,
// createPowerCapFeature, createReconfigPermissionsFeature
// =============================================================================

struct NsmProcessorBatch12CTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 28;
    uint8_t instanceId = 0;

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorBatch12CTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorBatch12CTest()
    {
        cleanupDeviceSensors(devices);
    }

    /** @brief Run a processor feature of the device `name`; each test uses
     *  its own name so the inventory objects do not collide on D-Bus
     */
    requester::Coroutine run(nsm::features::CreatorFn create,
                             const std::string& name, dbus::PropertyMap params,
                             bool withDevice = true)
    {
        nsm::features::DeviceContext dev;
        dev.name = name;
        dev.kind = "gpu";
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = gpu;
        }
        params["Name"] = name;
        return create(mockManager, dev, params);
    }
};

TEST_F(NsmProcessorBatch12CTest, goodTestCreateWorkloadPowerProfileFeature)
{
    auto rc = run(nsm::features::createWorkloadPowerProfileFeature,
                  "HGX_GPU_SXM_1", {});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    // The creator should create:
    // - NsmWorkLoadProfileEnum (static)
    // - OemProfileInfoIntf (non-sensor)
    // - NsmWorkloadProfileInfoAsyncIntf (non-sensor)
    // - NsmWorkLoadProfileStatus (RR)
    // - NsmWorkloadPowerProfileCollection (static)
    // - NsmWorkloadPowerProfilePageCollection (static)
    // - NsmWorkloadPowerProfilePage (RR, first page)
    // So we expect at least 2 RR sensors and 3+ static sensors
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
    EXPECT_GE(gpu->staticSensors.size(), 3u);
}

TEST_F(NsmProcessorBatch12CTest, badTestWorkloadPowerProfileFeatureNoDevice)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createWorkloadPowerProfileFeature,
                  "HGX_GPU_SXM_1_NoDevice", {}, false);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// =============================================================================
// createPowerCapFeature: no CompositeNumericSensors property is passed
// =============================================================================

TEST_F(NsmProcessorBatch12CTest,
       createPowerCapFeature_NoCompositeNumericSensors)
{
    auto rc = run(nsm::features::createPowerCapFeature,
                  "HGX_GPU_SXM_PwrCap_NoCNS", {});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // createPowerCap adds NsmPowerCap (RR) + 3 static sensors
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
    EXPECT_GE(gpu->staticSensors.size(), 3u);
}

// =============================================================================
// createReconfigPermissionsFeature: no enabled knob → no sensors
// =============================================================================

TEST_F(NsmProcessorBatch12CTest, createReconfigPermissionsFeature_NoKnobs)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createReconfigPermissionsFeature,
                  "HGX_GPU_SXM_Reconfig_NoFeat", {});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(1u, devices.size());
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

TEST_F(NsmProcessorBatch12CTest,
       createReconfigPermissionsFeature_AllKnobsDisabled)
{
    // Disabled knobs are dropped before their names are converted, so an
    // unknown knob that is also disabled creates nothing
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createReconfigPermissionsFeature,
                  "HGX_GPU_SXM_Reconfig_Disabled",
                  {{"AvailableKnobs", std::vector<std::string>{"NotAKnob"}},
                   {"DisabledKnobs", std::vector<std::string>{"NotAKnob"}}});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

TEST(NsmAcceleratorIntf, GoodConstruction)
{
    std::string accName = "GPU_SXM_1";
    std::string accType = "NSM_Processor";
    std::string accPath = "/xyz/openbmc_project/inventory/gpu_acc";

    NsmAcceleratorIntf accSensor(busPFB, accName, accType, accPath);
    EXPECT_EQ(accSensor.getName(), accName);
    EXPECT_EQ(accSensor.getType(), accType);
}

TEST(NsmProcessorAssociation, GoodConstruction)
{
    std::string assocName = "GPU_SXM_1";
    std::string assocType = "NSM_Processor";
    std::string assocPath = "/xyz/openbmc_project/inventory/gpu_assoc";
    std::vector<utils::Association> associations = {
        {"parent", "child", "/xyz/openbmc_project/inventory/system"}};

    NsmProcessorAssociation assocSensor(busPFB, assocName, assocType, assocPath,
                                        associations);
    EXPECT_EQ(assocSensor.getName(), assocName);
    EXPECT_EQ(assocSensor.getType(), assocType);
}

TEST(NsmLocationIntfProcessor, GoodConstruction)
{
    std::string locName = "GPU_SXM_1";
    std::string locType = "NSM_Processor";
    std::string locPath = "/xyz/openbmc_project/inventory/gpu_loc";
    std::string locationType = "xyz.openbmc_project.Inventory.Decorator."
                               "Location.LocationTypes.Embedded";

    NsmLocationIntfProcessor locSensor(busPFB, locName, locType, locPath,
                                       locationType);
    EXPECT_EQ(locSensor.getName(), locName);
    EXPECT_EQ(locSensor.getType(), locType);
}

TEST(NsmLocationCodeIntfProcessor, GoodConstruction)
{
    std::string locCodeName = "GPU_SXM_1";
    std::string locCodeType = "NSM_Processor";
    std::string locCodePath = "/xyz/openbmc_project/inventory/gpu_loccode";
    std::string locationCode = "GPU_SXM_1";

    NsmLocationCodeIntfProcessor locCodeSensor(busPFB, locCodeName, locCodeType,
                                               locCodePath, locationCode);
    EXPECT_EQ(locCodeSensor.getName(), locCodeName);
    EXPECT_EQ(locCodeSensor.getType(), locCodeType);
}
