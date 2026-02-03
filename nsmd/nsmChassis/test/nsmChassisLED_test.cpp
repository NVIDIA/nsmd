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

#include "network-ports.h"

#include "nsmChassisLED.hpp"

namespace nsm
{
requester::Coroutine createNsmChassisLEDSensor(SensorManager& manager,
                                               const std::string& interface,
                                               const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NsmChassisLEDTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisLED";
    const std::string name = "HGX_GPU_SXM_1";
    const std::string type = "NSM_ChassisLED";
    const std::string objPath = chassisInventoryBasePath / name;
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/" + name;

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmChassisLEDTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
    }

    dbus::PropertyMap chassisLED = {
        {"Type", "NSM_ChassisLED"},
        {"Name", name},
        {"UUID", gpuUuid},
        {"InventoryObjPath", inventoryObjPath},
    };
};

TEST_F(NsmChassisLEDTest, goodTestCreateSensor)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap.clear();

    propertyMap["Type"] = chassisLED["Type"];
    propertyMap["Name"] = chassisLED["Name"];
    propertyMap["UUID"] = chassisLED["UUID"];
    propertyMap["InventoryObjPath"] = chassisLED["InventoryObjPath"];

    size_t initialSensorCount = gpu->deviceSensors.size();
    createNsmChassisLEDSensor(mockManager, basicIntfName, objPath);

    EXPECT_EQ(1, devices.size());
    EXPECT_GT(gpu->deviceSensors.size(), initialSensorCount);

    auto nvlinkLedSensor =
        dynamic_pointer_cast<NsmNvlinkLedIntf>(gpu->deviceSensors.back());
    EXPECT_NE(nullptr, nvlinkLedSensor);
    EXPECT_EQ(name, nvlinkLedSensor->getName());
    EXPECT_EQ(type, nvlinkLedSensor->getType());
}

struct NsmNvlinkLedIntfTest : public NsmChassisLEDTest
{
    std::shared_ptr<NsmNvlinkLedIntf> sensor;

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string nameCopy = name;
        std::string typeCopy = type;
        std::string inventoryObjPathCopy = inventoryObjPath;
        sensor = std::make_shared<NsmNvlinkLedIntf>(bus, nameCopy, typeCopy,
                                                    inventoryObjPathCopy);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), type);
    }
};

TEST_F(NsmNvlinkLedIntfTest, goodTestConstructor)
{
    EXPECT_NE(sensor->nvlinkledIntf, nullptr);
}

TEST_F(NsmNvlinkLedIntfTest, goodTestRequest)
{
    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO());
    sensor->update(gpu).detach();
}

// Note: encode_get_nvlink_agg_led_status_resp is not available in libnsm,
// so response tests are not implemented
