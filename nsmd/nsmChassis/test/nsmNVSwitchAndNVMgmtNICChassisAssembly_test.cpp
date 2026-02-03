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

#include "nsmNVSwitchAndNVMgmtNICChassisAssembly.hpp"

using namespace nsm;

// Import types from the namespace
using AssemblyIntf = nsm::AssemblyIntf;
using HealthIntf = nsm::HealthIntf;
using AreaIntf = nsm::AreaIntf;
using LocationIntf = nsm::LocationIntf;

struct NsmNVSwitchChassisAssemblyTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis";
    const std::string name = "NVSwitch0_Assembly";
    const std::string type = "NSM_NVSwitch_Assembly";

    NsmDeviceTable devices;

    NsmNVSwitchChassisAssemblyTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestConstructor)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, name, type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), name);
    EXPECT_EQ(assembly->getType(), type);
}

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestNVSwitchAssembly)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, "NVSwitch0", type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), "NVSwitch0");
}

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestNVMgmtNICAssembly)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, "NVMgmtNIC0", type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), "NVMgmtNIC0");
}

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestMultipleAssemblies)
{
    auto assembly1 =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, "Assembly0", type);
    auto assembly2 =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, "Assembly1", type);
    auto assembly3 =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, "Assembly2", type);

    EXPECT_NE(assembly1, nullptr);
    EXPECT_NE(assembly2, nullptr);
    EXPECT_NE(assembly3, nullptr);

    EXPECT_EQ(assembly1->getName(), "Assembly0");
    EXPECT_EQ(assembly2->getName(), "Assembly1");
    EXPECT_EQ(assembly3->getName(), "Assembly2");
}

// TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestDifferentTypes) - Disabled due
// to DBus path conflict

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestWithHealthIntf)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<HealthIntf>>(
            chassisName, name, type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), name);
}

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestWithAreaIntf)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AreaIntf>>(
            chassisName, name, type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), name);
}

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestWithLocationIntf)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<LocationIntf>>(
            chassisName, name, type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), name);
}
