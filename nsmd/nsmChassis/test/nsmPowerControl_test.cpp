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

#include "nsmPowerControl.hpp"

namespace nsm
{
requester::Coroutine createControlGpuPower(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NsmPowerControlTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "PowerControl";
    const std::string type = "NSM_PowerControl";
    const std::string path = "/xyz/openbmc_project/control/power_cap/HGX";
    const std::string physicalContext = "SystemBoard";

    NsmDeviceTable devices;
    std::shared_ptr<NsmPowerControl> powerControl;

    NsmPowerControlTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::vector<utils::Association> associations;
        associations.push_back(
            {"chassis", "power_control",
             "/xyz/openbmc_project/inventory/system/chassis/HGX"});
        std::string typeCopy = type;
        powerControl = std::make_shared<NsmPowerControl>(
            bus, name, associations, typeCopy, path, physicalContext);
        EXPECT_NE(powerControl, nullptr);
        EXPECT_EQ(powerControl->getName(), name);
        EXPECT_EQ(powerControl->getType(), type);
    }
};

TEST_F(NsmPowerControlTest, goodTestConstructor)
{
    EXPECT_NE(powerControl->decoratorAreaIntf, nullptr);
    EXPECT_NE(powerControl->associationDefinitionsInft, nullptr);
    EXPECT_NE(powerControl->powerModeIntf, nullptr);
    EXPECT_NE(powerControl->clearPowerCapAsyncIntf, nullptr);
}

TEST_F(NsmPowerControlTest, goodTestPowerCapEnable)
{
    EXPECT_TRUE(powerControl->powerCapEnable());
}

TEST_F(NsmPowerControlTest, goodTestGetMaxPowerCapValue)
{
    // Verify getter is callable (no throw)
    (void)powerControl->maxPowerCapValue();
}

TEST_F(NsmPowerControlTest, goodTestGetMinPowerCapValue)
{
    // Verify getter is callable (no throw)
    (void)powerControl->minPowerCapValue();
}

TEST_F(NsmPowerControlTest, goodTestSetPowerCap)
{
    uint32_t powerCap = 5000;
    powerControl->powerCap(powerCap);
    EXPECT_EQ(powerControl->powerCap(), powerCap);
}

TEST_F(NsmPowerControlTest, goodTestPowerMode)
{
    using Mode = sdbusplus::common::xyz::openbmc_project::control::power::Mode;
    EXPECT_EQ(powerControl->powerModeIntf->powerMode(), Mode::PowerMode::OEM);
}

TEST_F(NsmPowerControlTest, goodTestPhysicalContext)
{
    using AreaIntf =
        sdbusplus::common::xyz::openbmc_project::inventory::decorator::Area;
    EXPECT_EQ(powerControl->decoratorAreaIntf->physicalContext(),
              AreaIntf::PhysicalContextType::SystemBoard);
}

// Test for createControlGpuPower function
TEST_F(NsmPowerControlTest, goodTestCreateControlGpuPower)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ControlTotalGPUPower";
    const std::string controlName = "ControlTotalGPUPower";
    const std::string objPath = "/xyz/openbmc_project/control/power_cap/" +
                                controlName;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap["Name"] = controlName;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["PhysicalContext"] = std::string("SystemBoard");

    auto fpga = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
    EXPECT_NE(fpga, nullptr);

    size_t initialSensorCount = fpga->deviceSensors.size();
    createControlGpuPower(mockManager, basicIntfName, objPath);

    EXPECT_GT(fpga->deviceSensors.size(), initialSensorCount);
}
