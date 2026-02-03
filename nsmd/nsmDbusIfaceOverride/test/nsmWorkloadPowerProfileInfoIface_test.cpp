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

#define private public
#define protected public

#include "base.h"

#include "nsmWorkloadPowerProfileInfoIface.hpp"

using namespace nsm;

struct NsmWorkloadPowerProfileInfoIfaceTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string profilePath =
        "/xyz/openbmc_project/control/power_profile";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmWorkloadPowerProfileInfoIfaceTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmWorkloadPowerProfileInfoIfaceTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmWorkloadPowerProfileInfoIfaceTest, testOemProfileInfoIntfConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testPath = "/test/profile/info";

    auto profileInfo = std::make_shared<OemProfileInfoIntf>(bus, testPath, gpu);

    EXPECT_NE(profileInfo, nullptr);
    EXPECT_EQ(profileInfo->device, gpu);
    EXPECT_EQ(profileInfo->inventoryObjPath, testPath);
}

TEST_F(NsmWorkloadPowerProfileInfoIfaceTest,
       testOemWorkLoadPowerProfileIntfConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string parentPath = "/test/parent";
    uint16_t profileId = 1;
    std::string profileName = "Performance";

    auto workloadProfile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, parentPath, profileId, profileName, gpu);

    EXPECT_NE(workloadProfile, nullptr);
    EXPECT_EQ(workloadProfile->device, gpu);
    EXPECT_EQ(workloadProfile->parentPath, parentPath);
    EXPECT_EQ(workloadProfile->profileId, profileId);
    EXPECT_EQ(workloadProfile->profileName, profileName);
}

TEST_F(NsmWorkloadPowerProfileInfoIfaceTest, testGetProfileName)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string parentPath = "/test/parent";
    uint16_t profileId = 2;
    std::string profileName = "Balanced";

    auto workloadProfile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, parentPath, profileId, profileName, gpu);

    EXPECT_EQ(workloadProfile->getProfileName(), profileName);
}

TEST_F(NsmWorkloadPowerProfileInfoIfaceTest, testGetProfileId)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string parentPath = "/test/parent";
    uint16_t profileId = 3;
    std::string profileName = "PowerSaver";

    auto workloadProfile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, parentPath, profileId, profileName, gpu);

    EXPECT_EQ(workloadProfile->getProfileId(), profileId);
}

TEST_F(NsmWorkloadPowerProfileInfoIfaceTest, testInventoryObjPath)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string parentPath = "/test/parent";
    uint16_t profileId = 4;
    std::string profileName = "Custom";

    auto workloadProfile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, parentPath, profileId, profileName, gpu);

    std::string expectedPath = parentPath + "/workload/profile/" +
                               std::to_string(profileId);
    EXPECT_EQ(workloadProfile->inventoryObjPath, expectedPath);
}

TEST_F(NsmWorkloadPowerProfileInfoIfaceTest, testMultipleProfiles)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string parentPath = "/test/parent";

    std::string profileName1 = "Profile1";
    std::string profileName2 = "Profile2";

    auto profile1 = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, parentPath, 1, profileName1, gpu);
    auto profile2 = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, parentPath, 2, profileName2, gpu);

    EXPECT_NE(profile1, nullptr);
    EXPECT_NE(profile2, nullptr);
    EXPECT_NE(profile1, profile2);
    EXPECT_EQ(profile1->getProfileId(), 1);
    EXPECT_EQ(profile2->getProfileId(), 2);
}

TEST_F(NsmWorkloadPowerProfileInfoIfaceTest, testProfileNameProperty)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string parentPath = "/test/parent";
    uint16_t profileId = 5;
    std::string profileName = "TestProfile";

    auto workloadProfile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, parentPath, profileId, profileName, gpu);

    // Verify that the profile name was set correctly via the interface
    EXPECT_EQ(workloadProfile->WorkLoadPowerProfileIntf::profileName(),
              profileName);
}
