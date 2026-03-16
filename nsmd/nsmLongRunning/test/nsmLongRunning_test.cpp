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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmAsyncLongRunningSensor.hpp"
#include "nsmLongRunningSensor.hpp"

// Concrete test class for NsmLongRunningSensor
class TestNsmLongRunningSensor : public nsm::NsmLongRunningSensor
{
  public:
    TestNsmLongRunningSensor(const std::string& name, const std::string& type,
                             bool isLongRunning,
                             std::shared_ptr<nsm::NsmDevice> device,
                             uint8_t messageType, uint8_t commandCode) :
        NsmLongRunningSensor(name, type, isLongRunning, device, messageType,
                             commandCode)
    {}

    // Implement pure virtual methods
    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::vector<uint8_t>{};
    }

    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return NSM_SW_SUCCESS;
    }
};

// Concrete test class for NsmAsyncLongRunningSensor
class TestNsmAsyncLongRunningSensor : public nsm::NsmAsyncLongRunningSensor
{
  public:
    TestNsmAsyncLongRunningSensor(const std::string& name,
                                  const std::string& type, bool isLongRunning,
                                  std::shared_ptr<nsm::NsmDevice> device,
                                  uint8_t messageType, uint8_t commandCode) :
        NsmAsyncLongRunningSensor(name, type, isLongRunning, device,
                                  messageType, commandCode)
    {}

    // Implement pure virtual methods
    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::vector<uint8_t>{};
    }

    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return NSM_SW_SUCCESS;
    }
};

class NsmLongRunningTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
};

// Test NsmLongRunningSensor constructor with valid parameters
TEST_F(NsmLongRunningTest, NsmLongRunningSensor_Constructor_ValidParams)
{
    std::string name = "TestLongRunningSensor";
    std::string type = "LongRunningSensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x05;
    uint8_t commandCode = 0x10;

    TestNsmLongRunningSensor sensor(name, type, isLongRunning, device,
                                    messageType, commandCode);

    EXPECT_EQ(sensor.NsmSensor::getName(), name);
    EXPECT_EQ(sensor.NsmSensor::getType(), type);
    EXPECT_EQ(sensor.messageType, messageType);
    EXPECT_EQ(sensor.commandCode, commandCode);
}

// Test NsmLongRunningSensor constructor with different message types
TEST_F(NsmLongRunningTest,
       NsmLongRunningSensor_Constructor_DifferentMessageTypes)
{
    std::string name = "TestSensor";
    std::string type = "Sensor";
    bool isLongRunning = false;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;

    // Test with message type 0x01
    TestNsmLongRunningSensor sensor1(name, type, isLongRunning, device, 0x01,
                                     0x20);
    EXPECT_EQ(sensor1.messageType, 0x01);
    EXPECT_EQ(sensor1.commandCode, 0x20);

    // Test with message type 0x0F
    TestNsmLongRunningSensor sensor2(name, type, isLongRunning, device, 0x0F,
                                     0x30);
    EXPECT_EQ(sensor2.messageType, 0x0F);
    EXPECT_EQ(sensor2.commandCode, 0x30);
}

// Test NsmLongRunningSensor constructor with isLongRunning true
TEST_F(NsmLongRunningTest, NsmLongRunningSensor_Constructor_IsLongRunningTrue)
{
    std::string name = "LongRunningSensor";
    std::string type = "Sensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x05;
    uint8_t commandCode = 0x10;

    TestNsmLongRunningSensor sensor(name, type, isLongRunning, device,
                                    messageType, commandCode);

    // Verify the sensor is created successfully
    EXPECT_EQ(sensor.NsmSensor::getName(), name);
}

// Test NsmLongRunningSensor constructor with isLongRunning false
TEST_F(NsmLongRunningTest, NsmLongRunningSensor_Constructor_IsLongRunningFalse)
{
    std::string name = "QuickSensor";
    std::string type = "Sensor";
    bool isLongRunning = false;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x05;
    uint8_t commandCode = 0x10;

    TestNsmLongRunningSensor sensor(name, type, isLongRunning, device,
                                    messageType, commandCode);

    // Verify the sensor is created successfully
    EXPECT_EQ(sensor.NsmSensor::getName(), name);
}

// Test NsmAsyncLongRunningSensor constructor with valid parameters
TEST_F(NsmLongRunningTest, NsmAsyncLongRunningSensor_Constructor_ValidParams)
{
    std::string name = "TestAsyncLongRunningSensor";
    std::string type = "AsyncLongRunningSensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x06;
    uint8_t commandCode = 0x11;

    TestNsmAsyncLongRunningSensor sensor(name, type, isLongRunning, device,
                                         messageType, commandCode);

    EXPECT_EQ(sensor.NsmSensor::getName(), name);
    EXPECT_EQ(sensor.NsmSensor::getType(), type);
    EXPECT_EQ(sensor.messageType, messageType);
    EXPECT_EQ(sensor.commandCode, commandCode);
}

// Test NsmAsyncLongRunningSensor constructor with different message types
TEST_F(NsmLongRunningTest,
       NsmAsyncLongRunningSensor_Constructor_DifferentMessageTypes)
{
    std::string name = "TestAsyncSensor";
    std::string type = "AsyncSensor";
    bool isLongRunning = false;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;

    // Test with message type 0x02
    TestNsmAsyncLongRunningSensor sensor1(name, type, isLongRunning, device,
                                          0x02, 0x21);
    EXPECT_EQ(sensor1.messageType, 0x02);
    EXPECT_EQ(sensor1.commandCode, 0x21);

    // Test with message type 0x0E
    TestNsmAsyncLongRunningSensor sensor2(name, type, isLongRunning, device,
                                          0x0E, 0x31);
    EXPECT_EQ(sensor2.messageType, 0x0E);
    EXPECT_EQ(sensor2.commandCode, 0x31);
}

// Test NsmAsyncLongRunningSensor constructor with isLongRunning true
TEST_F(NsmLongRunningTest,
       NsmAsyncLongRunningSensor_Constructor_IsLongRunningTrue)
{
    std::string name = "AsyncLongRunningSensor";
    std::string type = "AsyncSensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x06;
    uint8_t commandCode = 0x11;

    TestNsmAsyncLongRunningSensor sensor(name, type, isLongRunning, device,
                                         messageType, commandCode);

    // Verify the sensor is created successfully
    EXPECT_EQ(sensor.NsmSensor::getName(), name);
}

// Test NsmAsyncLongRunningSensor constructor with isLongRunning false
TEST_F(NsmLongRunningTest,
       NsmAsyncLongRunningSensor_Constructor_IsLongRunningFalse)
{
    std::string name = "AsyncQuickSensor";
    std::string type = "AsyncSensor";
    bool isLongRunning = false;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x06;
    uint8_t commandCode = 0x11;

    TestNsmAsyncLongRunningSensor sensor(name, type, isLongRunning, device,
                                         messageType, commandCode);

    // Verify the sensor is created successfully
    EXPECT_EQ(sensor.NsmSensor::getName(), name);
}

// Test NsmLongRunningSensor with empty name
TEST_F(NsmLongRunningTest, NsmLongRunningSensor_Constructor_EmptyName)
{
    std::string name = "";
    std::string type = "Sensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x05;
    uint8_t commandCode = 0x10;

    TestNsmLongRunningSensor sensor(name, type, isLongRunning, device,
                                    messageType, commandCode);

    EXPECT_EQ(sensor.NsmSensor::getName(), name);
    EXPECT_TRUE(sensor.NsmSensor::getName().empty());
}

// Test NsmAsyncLongRunningSensor with empty name
TEST_F(NsmLongRunningTest, NsmAsyncLongRunningSensor_Constructor_EmptyName)
{
    std::string name = "";
    std::string type = "AsyncSensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x06;
    uint8_t commandCode = 0x11;

    TestNsmAsyncLongRunningSensor sensor(name, type, isLongRunning, device,
                                         messageType, commandCode);

    EXPECT_EQ(sensor.NsmSensor::getName(), name);
    EXPECT_TRUE(sensor.NsmSensor::getName().empty());
}
