/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmGPIO/nsmGPIOState.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace nsm;

struct NsmGPIOStateTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string type = "NSM_GPIO_State";
    const std::string name = "GPIO_Test";
    std::string objPath = "/xyz/openbmc_project/gpio/GPIO_Test";

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmGPIOStateTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
    }

    ~NsmGPIOStateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmGPIOStateTest, testConstructor)
{
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    associationsList.emplace_back("parent_chassis", "gpio_state",
                                  "/xyz/openbmc_project/inventory/system");

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    auto gpioStateSensor = std::make_shared<NsmGPIOState>(
        utils::DBusHandler::getBus(), type, name, objPath, associationsList,
        gpioStateIntf);

    EXPECT_NE(gpioStateSensor, nullptr);
    EXPECT_NE(gpioStateSensor->gpioStateIntf, nullptr);
    EXPECT_NE(gpioStateSensor->associationDefIntf, nullptr);
    EXPECT_EQ(gpioStateSensor->objPath, objPath);
}

TEST_F(NsmGPIOStateTest, testGenRequestMsg)
{
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    associationsList.emplace_back("parent_chassis", "gpio_state",
                                  "/xyz/openbmc_project/inventory/system");

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    auto gpioStateSensor = std::make_shared<NsmGPIOState>(
        utils::DBusHandler::getBus(), type, name, objPath, associationsList,
        gpioStateIntf);

    auto request = gpioStateSensor->genRequestMsg(eid, instanceId);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpio_state_req));
}

TEST_F(NsmGPIOStateTest, testHandleResponseMsgSuccess)
{
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    associationsList.emplace_back("parent_chassis", "gpio_state",
                                  "/xyz/openbmc_project/inventory/system");

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    auto gpioStateSensor = std::make_shared<NsmGPIOState>(
        utils::DBusHandler::getBus(), type, name, objPath, associationsList,
        gpioStateIntf);

    // Create a mock response message
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpio_state_resp) + 16);
    auto responsePtr = reinterpret_cast<struct nsm_msg*>(response.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t offset = 0;
    uint16_t length = 16;
    std::vector<uint8_t> gpioValues = {0xFF, 0x00, 0xAA, 0x55};

    auto rc = encode_get_gpio_state_resp(instanceId, cc, reasonCode, offset,
                                         length, gpioValues.data(),
                                         gpioValues.size(), responsePtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = gpioStateSensor->handleResponseMsg(responsePtr,
                                                     response.size());

    EXPECT_EQ(result, NSM_SUCCESS);
}

TEST_F(NsmGPIOStateTest, testHandleResponseMsgFailure)
{
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    associationsList.emplace_back("parent_chassis", "gpio_state",
                                  "/xyz/openbmc_project/inventory/system");

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    auto gpioStateSensor = std::make_shared<NsmGPIOState>(
        utils::DBusHandler::getBus(), type, name, objPath, associationsList,
        gpioStateIntf);

    // Create a mock response message with error
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpio_state_resp));
    auto responsePtr = reinterpret_cast<struct nsm_msg*>(response.data());

    uint8_t cc = NSM_ERR_INVALID_DATA;
    uint16_t reasonCode = ERR_INVALID_RQD;
    uint16_t offset = 0;
    uint16_t length = 0;
    std::vector<uint8_t> gpioValues;

    auto rc = encode_get_gpio_state_resp(instanceId, cc, reasonCode, offset,
                                         length, gpioValues.data(), 0,
                                         responsePtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = gpioStateSensor->handleResponseMsg(responsePtr,
                                                     response.size());

    EXPECT_EQ(result, NSM_ERR_INVALID_DATA);
}

TEST_F(NsmGPIOStateTest, testHandleResponseMsgMultipleGPIOs)
{
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    associationsList.emplace_back("parent_chassis", "gpio_state",
                                  "/xyz/openbmc_project/inventory/system");

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    auto gpioStateSensor = std::make_shared<NsmGPIOState>(
        utils::DBusHandler::getBus(), type, name, objPath, associationsList,
        gpioStateIntf);

    // Create a mock response message with multiple GPIO values
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpio_state_resp) + 32);
    auto responsePtr = reinterpret_cast<struct nsm_msg*>(response.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t offset = 100;                     // Start from GPIO index 100
    uint16_t length = 32;
    std::vector<uint8_t> gpioValues(32, 0xAA); // Pattern 10101010

    auto rc = encode_get_gpio_state_resp(instanceId, cc, reasonCode, offset,
                                         length, gpioValues.data(),
                                         gpioValues.size(), responsePtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = gpioStateSensor->handleResponseMsg(responsePtr,
                                                     response.size());

    EXPECT_EQ(result, NSM_SUCCESS);
}
