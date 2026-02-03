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

#include "nsmLeakDetection.hpp"

using namespace nsm;

struct NsmLeakDetectionTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "LeakDetector";
    const std::string type = "NSM_LeakDetection";
    const std::string objPath =
        "/xyz/openbmc_project/sensors/voltage/LeakDetector";
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmLeakDetectionTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmLeakDetectionTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmLeakDetectionTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "LeakDetector";
    std::vector<uint64_t> sensorIdMap = {1, 2};
    std::vector<std::string> sensorNameMap = {"Sensor1", "Sensor2"};

    auto leakDetection = std::make_shared<NsmLeakDetection>(
        testName, type, bus, sensorIdMap, sensorNameMap, chassisPath, 100.0,
        0.0);

    EXPECT_NE(leakDetection, nullptr);
    EXPECT_EQ(leakDetection->getName(), testName);
    EXPECT_EQ(leakDetection->getType(), type);
}

TEST_F(NsmLeakDetectionTest, testGetSensorInterfaces)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "LeakDetector";
    std::vector<uint64_t> sensorIdMap = {1};
    std::vector<std::string> sensorNameMap = {"Sensor1"};

    auto leakDetection = std::make_shared<NsmLeakDetection>(
        testName, type, bus, sensorIdMap, sensorNameMap, chassisPath, 100.0,
        0.0);

    uint8_t sensorId = 1;
    auto result = leakDetection->getSensorInterfaces(sensorId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmLeakDetectionTest, testGetSensorInterfacesInvalidSensor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "LeakDetector";
    std::vector<uint64_t> sensorIdMap = {1};
    std::vector<std::string> sensorNameMap = {"Sensor1"};

    auto leakDetection = std::make_shared<NsmLeakDetection>(
        testName, type, bus, sensorIdMap, sensorNameMap, chassisPath, 100.0,
        0.0);

    // Try to get interfaces for a different sensor ID
    auto result = leakDetection->getSensorInterfaces(99);

    EXPECT_FALSE(result.has_value());
}

TEST_F(NsmLeakDetectionTest, testGenRequestMsg)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "LeakDetector";
    std::vector<uint64_t> sensorIdMap = {1};
    std::vector<std::string> sensorNameMap = {"Sensor1"};

    auto leakDetection = std::make_shared<NsmLeakDetection>(
        testName, type, bus, sensorIdMap, sensorNameMap, chassisPath, 100.0,
        0.0);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto result = leakDetection->genRequestMsg(eid, instanceId);

    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0);
}

struct NsmLeakDetectionThresholdsPatchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmLeakDetectionThresholdsPatchTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmLeakDetectionThresholdsPatchTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sensorPath = "/xyz/openbmc_project/sensors/voltage/TestSensor";

    auto sensorValueIntf =
        std::make_shared<SensorValueIntf>(bus, sensorPath.c_str());
    auto thresholdIntf =
        std::make_shared<SensorThresholdCriticalIntf>(bus, sensorPath.c_str());

    uint8_t sensorId = 1;
    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        sensorId, sensorValueIntf, thresholdIntf);

    EXPECT_NE(patchObj, nullptr);
    EXPECT_EQ(patchObj->sensorValueIntf, sensorValueIntf);
    EXPECT_EQ(patchObj->thresholdIntf, thresholdIntf);
}

struct NsmSetLeakDetectionThresholdsTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmSetLeakDetectionThresholdsTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmSetLeakDetectionThresholdsTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmSetLeakDetectionThresholdsTest, testConstructor)
{
    std::string name = "LeakDetectionThresholds";
    std::string type = "NSM_LeakDetectionThresholds";
    std::vector<uint64_t> sensorIdMap = {1, 2, 3};
    std::vector<uint64_t> minThresholds = {100, 150, 200};
    std::vector<uint64_t> criticalThresholds = {300, 350, 400};
    std::vector<uint64_t> maxThresholds = {500, 550, 600};

    auto setThresholds = std::make_shared<NsmSetLeakDetectionThresholds>(
        name, type, sensorIdMap, minThresholds, criticalThresholds,
        maxThresholds);

    EXPECT_NE(setThresholds, nullptr);
    EXPECT_EQ(setThresholds->getName(), name);
    EXPECT_EQ(setThresholds->getType(), type);
}
