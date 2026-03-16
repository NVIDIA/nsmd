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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmChassis/nsmNVSwitchAndNVMgmtNICChassisAssembly.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// SECTION 4: nsmNVSwitchAndNVMgmtNICChassisAssembly.cpp
// ============================================================================

struct NsmChassisAssemblyBatch11F :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "NVSwitch_Batch11F";
    const std::string name = "NVSwitch0_Asm_B11F";
    const std::string baseType = "NSM_NVSwitch_ChassisAssembly";
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:55";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmChassisAssemblyBatch11F() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmChassisAssemblyBatch11F()
    {
        cleanupDeviceSensors(devices);
    }
};

// -- NsmNVSwitchAndNicChassisAssembly<NsmAssetIntf>: asset object construction
TEST_F(NsmChassisAssemblyBatch11F,
       AssetAssembly_Constructor_CreatesValidAssetObject)
{
    // Arrange & Act - construct asset assembly object directly via public API
    auto assetObj =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<NsmAssetIntf>>(
            chassisName, name, baseType);

    // Assert
    EXPECT_NE(assetObj, nullptr);
    EXPECT_EQ(assetObj->getName(), name);
    EXPECT_EQ(assetObj->getType(), baseType);
}

// -- NsmNVSwitchAndNicChassisAssembly<HealthIntf>: health object construction
TEST_F(NsmChassisAssemblyBatch11F,
       HealthAssembly_Constructor_CreatesValidHealthObject)
{
    // Arrange & Act
    auto healthObj =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<HealthIntf>>(
            chassisName, name, baseType);

    // Assert
    EXPECT_NE(healthObj, nullptr);
    EXPECT_EQ(healthObj->getName(), name);
    EXPECT_EQ(healthObj->getType(), baseType);

    // Verify the object can be added as a static sensor
    size_t initialStatic = gpu->staticSensors.size();
    gpu->addStaticSensor(healthObj);
    EXPECT_EQ(gpu->staticSensors.size(), initialStatic + 1);
}

// -- NsmNVSwitchAndNicChassisAssembly<AreaIntf>: physical context construction
TEST_F(NsmChassisAssemblyBatch11F,
       AreaAssembly_Constructor_CreatesValidAreaObject)
{
    // Arrange & Act
    auto areaObj = std::make_shared<NsmNVSwitchAndNicChassisAssembly<AreaIntf>>(
        chassisName, name, baseType);

    // Assert
    EXPECT_NE(areaObj, nullptr);
    EXPECT_EQ(areaObj->getName(), name);
    EXPECT_EQ(areaObj->getType(), baseType);

    // Verify the object can be added as a static sensor
    size_t initialStatic = gpu->staticSensors.size();
    gpu->addStaticSensor(areaObj);
    EXPECT_EQ(gpu->staticSensors.size(), initialStatic + 1);
}

// -- NsmNVSwitchAndNicChassisAssembly<LocationIntf>: location type
TEST_F(NsmChassisAssemblyBatch11F,
       LocationAssembly_Constructor_CreatesValidLocationObject)
{
    // Arrange & Act
    auto locObj =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<LocationIntf>>(
            chassisName, name, baseType);

    // Assert
    EXPECT_NE(locObj, nullptr);
    EXPECT_EQ(locObj->getName(), name);
    EXPECT_EQ(locObj->getType(), baseType);

    // Verify the object can be added as a static sensor
    size_t initialStatic = gpu->staticSensors.size();
    gpu->addStaticSensor(locObj);
    EXPECT_EQ(gpu->staticSensors.size(), initialStatic + 1);
}

// -- NsmNVSwitchAndNicChassisAssembly with RevisionIntf
TEST_F(NsmChassisAssemblyBatch11F, Constructor_RevisionIntf_CreatesValidObject)
{
    // Arrange & Act
    auto revisionObj =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<RevisionIntf>>(
            chassisName, name, baseType);

    // Assert
    EXPECT_NE(revisionObj, nullptr);
    EXPECT_EQ(revisionObj->getName(), name);
    EXPECT_EQ(revisionObj->getType(), baseType);
}
