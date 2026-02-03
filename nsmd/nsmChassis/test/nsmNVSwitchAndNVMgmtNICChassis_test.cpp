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

#include "nsmNVSwitchAndNVMgmtNICChassis.hpp"

using namespace nsm;

struct NsmNVSwitchChassisTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "NVSwitch0";
    const std::string type = "NSM_NVSwitch";

    NsmDeviceTable devices;

    NsmNVSwitchChassisTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmNVSwitchChassisTest, goodTestAssetChassis)
{
    auto chassis =
        std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(name, type);
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), name);
    EXPECT_EQ(chassis->getType(), type);
}

TEST_F(NsmNVSwitchChassisTest, goodTestChassisIntf)
{
    auto chassis =
        std::make_shared<NsmNVSwitchAndNicChassis<ChassisIntf>>(name, type);
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), name);
    EXPECT_EQ(chassis->getType(), type);
}

TEST_F(NsmNVSwitchChassisTest, goodTestHealthIntf)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<HealthIntf>>(name,
                                                                          type);
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), name);
    EXPECT_EQ(chassis->getType(), type);
}

TEST_F(NsmNVSwitchChassisTest, goodTestLocationIntf)
{
    auto chassis =
        std::make_shared<NsmNVSwitchAndNicChassis<LocationIntf>>(name, type);
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), name);
    EXPECT_EQ(chassis->getType(), type);
}

TEST_F(NsmNVSwitchChassisTest, goodTestUuidIntf)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<UuidIntf>>(name,
                                                                        type);
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), name);
    EXPECT_EQ(chassis->getType(), type);
}

TEST_F(NsmNVSwitchChassisTest, goodTestMultipleInstances)
{
    // Test creating multiple chassis instances
    auto chassis1 = std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(
        "NVSwitch0", type);
    auto chassis2 = std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(
        "NVSwitch1", type);
    auto chassis3 = std::make_shared<NsmNVSwitchAndNicChassis<ChassisIntf>>(
        "NVSwitch2", type);

    EXPECT_NE(chassis1, nullptr);
    EXPECT_NE(chassis2, nullptr);
    EXPECT_NE(chassis3, nullptr);

    EXPECT_EQ(chassis1->getName(), "NVSwitch0");
    EXPECT_EQ(chassis2->getName(), "NVSwitch1");
    EXPECT_EQ(chassis3->getName(), "NVSwitch2");
}

TEST_F(NsmNVSwitchChassisTest, goodTestNVMgmtNIC)
{
    const std::string nicName = "NVMgmtNIC0";
    auto nic = std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(nicName,
                                                                        type);
    EXPECT_NE(nic, nullptr);
    EXPECT_EQ(nic->getName(), nicName);
}
