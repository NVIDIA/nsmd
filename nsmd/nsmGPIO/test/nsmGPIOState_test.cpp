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

// Test handleResponseMsg with offset
TEST_F(NsmGPIOStateTest, HandleResponseMsg_WithOffset)
{
    std::string name = "TestGPIO";
    std::string type = "GPIO";
    std::string inventoryObjPath = "/xyz/openbmc_project/gpio/test";
    std::vector<std::tuple<std::string, std::string, std::string>> associations;

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), inventoryObjPath.c_str());

    auto gpioStateSensor = std::make_shared<NsmGPIOState>(
        utils::DBusHandler::getBus(), type, name, inventoryObjPath,
        associations, gpioStateIntf);

    uint8_t instanceId = 1;
    uint16_t offset = 16;                   // Start at GPIO 16
    uint16_t length = 1;                    // 1 byte = 8 GPIO pins
    std::vector<uint8_t> gpioData = {0xFF}; // All high

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_gpio_state_resp) - 1 +
                                     gpioData.size());
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_gpio_state_resp(instanceId, NSM_SUCCESS, ERR_NULL,
                                         offset, length, gpioData.data(),
                                         gpioData.size(), responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = gpioStateSensor->handleResponseMsg(responseMsgPtr,
                                                     responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto states = gpioStateIntf->lineStates();

    // Check that GPIOs 16-23 are all true (offset + 0-7)
    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(states[16 + i], true)
            << "GPIO " << (16 + i) << " should be true";
    }
}

// Test GPIO bit extraction logic - all zeros
TEST_F(NsmGPIOStateTest, HandleResponseMsg_AllZeros)
{
    std::string name = "TestGPIO";
    std::string type = "GPIO";
    std::string inventoryObjPath = "/xyz/openbmc_project/gpio/test";
    std::vector<std::tuple<std::string, std::string, std::string>> associations;

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), inventoryObjPath.c_str());

    auto gpioStateSensor = std::make_shared<NsmGPIOState>(
        utils::DBusHandler::getBus(), type, name, inventoryObjPath,
        associations, gpioStateIntf);

    uint8_t instanceId = 1;
    uint16_t offset = 0;
    uint16_t length = 1;
    std::vector<uint8_t> gpioData = {0x00};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_gpio_state_resp) - 1 +
                                     gpioData.size());
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_gpio_state_resp(instanceId, NSM_SUCCESS, ERR_NULL,
                                         offset, length, gpioData.data(),
                                         gpioData.size(), responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = gpioStateSensor->handleResponseMsg(responseMsgPtr,
                                                     responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto states = gpioStateIntf->lineStates();

    // All 8 bits should be false
    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(states[i], false) << "GPIO " << i << " should be false";
    }
}

// Test GPIO bit extraction logic - all ones
TEST_F(NsmGPIOStateTest, HandleResponseMsg_AllOnes)
{
    std::string name = "TestGPIO";
    std::string type = "GPIO";
    std::string inventoryObjPath = "/xyz/openbmc_project/gpio/test";
    std::vector<std::tuple<std::string, std::string, std::string>> associations;

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), inventoryObjPath.c_str());

    auto gpioStateSensor = std::make_shared<NsmGPIOState>(
        utils::DBusHandler::getBus(), type, name, inventoryObjPath,
        associations, gpioStateIntf);

    uint8_t instanceId = 1;
    uint16_t offset = 0;
    uint16_t length = 1;
    std::vector<uint8_t> gpioData = {0xFF};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_gpio_state_resp) - 1 +
                                     gpioData.size());
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_gpio_state_resp(instanceId, NSM_SUCCESS, ERR_NULL,
                                         offset, length, gpioData.data(),
                                         gpioData.size(), responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = gpioStateSensor->handleResponseMsg(responseMsgPtr,
                                                     responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto states = gpioStateIntf->lineStates();

    // All 8 bits should be true
    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(states[i], true) << "GPIO " << i << " should be true";
    }
}

// Test success with zero-length GPIO data: gpioStateMap is empty →
// the `if (gpioStateIntf != nullptr && !gpioStateMap.empty())` false branch
TEST_F(NsmGPIOStateTest, HandleResponseMsg_EmptyGPIOData_NoLineStateUpdate)
{
    std::string name = "TestGPIO_Empty";
    std::string type = "GPIO";
    std::string inventoryObjPath = "/xyz/openbmc_project/gpio/empty";
    std::vector<std::tuple<std::string, std::string, std::string>> associations;

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), inventoryObjPath.c_str());

    auto gpioStateSensor = std::make_shared<NsmGPIOState>(
        utils::DBusHandler::getBus(), type, name, inventoryObjPath,
        associations, gpioStateIntf);

    // Pre-set lineStates to verify they are NOT updated
    std::map<uint16_t, bool> preStates{{0, true}, {1, false}};
    gpioStateIntf->lineStates(preStates);

    // Send a valid response with 0 GPIO bytes: gpioValuesSize = 0 → map stays
    // empty encode_get_gpio_state_resp requires non-null gpio_values even for 0
    // bytes
    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_gpio_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());
    uint8_t dummyBuf = 0;

    auto rc = encode_get_gpio_state_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0,
                                         &dummyBuf, 0, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = gpioStateSensor->handleResponseMsg(responseMsgPtr,
                                                     responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);
    // lineStates should NOT have been updated (map was empty)
    auto states = gpioStateIntf->lineStates();
    EXPECT_EQ(states, preStates);
}

// genRequestMsg failure: instanceId > NSM_INSTANCE_MAX → encode fails →
// if (rc != NSM_SW_SUCCESS) TRUE → return nullopt.
// Covers lines 48-50 in nsmGPIOState.cpp.
TEST_F(NsmGPIOStateTest, GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    std::vector<std::tuple<std::string, std::string, std::string>> assoc;
    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());
    auto sensor = std::make_shared<NsmGPIOState>(utils::DBusHandler::getBus(),
                                                 type, name, objPath, assoc,
                                                 gpioStateIntf);

    // NSM_INSTANCE_MAX + 1 makes encode_get_gpio_state_req fail
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// handleResponseMsg – decode failure: too-short buffer
TEST_F(NsmGPIOStateTest, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::vector<std::tuple<std::string, std::string, std::string>> associations;

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    auto gpioStateSensor =
        std::make_shared<NsmGPIOState>(utils::DBusHandler::getBus(), type, name,
                                       objPath, associations, gpioStateIntf);

    // 7-byte buffer: safely reads cc=0 at payload[1] but too short for
    // nsm_get_gpio_state_resp which embeds nsm_common_resp. decode_common_resp
    // requires at least sizeof(nsm_msg_hdr)+sizeof(nsm_common_resp)=11 bytes.
    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto responsePtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto result = gpioStateSensor->handleResponseMsg(responsePtr,
                                                     responseMsg.size());

    EXPECT_NE(result, NSM_SUCCESS);
}

// handleResponseMsg: decode returns NSM_SW_SUCCESS but cc != NSM_SUCCESS.
// Uses 9-byte buffer so decode_reason_code_and_cc returns NSM_SW_SUCCESS
// with cc=NSM_ERR_INVALID_DATA → L73 "rc==success && cc!=success" branch.
TEST_F(NsmGPIOStateTest, HandleResponseMsg_DecodeSuccessNonZeroCC_ReturnsCC)
{
    std::vector<std::tuple<std::string, std::string, std::string>> associations;

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    auto gpioStateSensor =
        std::make_shared<NsmGPIOState>(utils::DBusHandler::getBus(), type, name,
                                       objPath, associations, gpioStateIntf);

    // sizeof(nsm_msg_hdr)+sizeof(nsm_common_non_success_resp)=9 bytes:
    // decode_reason_code_and_cc returns NSM_SW_SUCCESS with cc=error
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    responseMsg[sizeof(nsm_msg_hdr) + 1] = NSM_ERR_INVALID_DATA;

    auto responsePtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto result = gpioStateSensor->handleResponseMsg(responsePtr,
                                                     responseMsg.size());

    EXPECT_EQ(result, NSM_ERR_INVALID_DATA);
}

// handleResponseMsg with gpioStateIntf = nullptr: decode succeeds and
// gpioStateMap is non-empty, but the `gpioStateIntf != nullptr` guard on
// line 90 of nsmGPIOState.cpp short-circuits to false → lineStates update
// is skipped. Covers the FALSE branch of the first condition.
TEST_F(NsmGPIOStateTest, HandleResponseMsg_NullGpioStateIntf_SkipsLineStates)
{
    std::vector<std::tuple<std::string, std::string, std::string>> associations;

    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    auto sensor = std::make_shared<NsmGPIOState>(utils::DBusHandler::getBus(),
                                                 type, name, objPath,
                                                 associations, gpioStateIntf);

    // Null out the interface (exposed by #define private public)
    sensor->gpioStateIntf = nullptr;

    // Build a valid response with 1 byte of GPIO data (8 pins)
    std::vector<uint8_t> gpioData = {0xFF};
    uint16_t respOffset = 0;
    uint16_t length = static_cast<uint16_t>(gpioData.size());

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_gpio_state_resp) - 1 +
                                     gpioData.size());
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    ASSERT_EQ(encode_get_gpio_state_resp(0, NSM_SUCCESS, ERR_NULL, respOffset,
                                         length, gpioData.data(),
                                         gpioData.size(), responseMsgPtr),
              NSM_SW_SUCCESS);

    // gpioStateIntf is nullptr → the `gpioStateIntf != nullptr` check is
    // false → lineStates() is never called → returns success
    auto result = sensor->handleResponseMsg(responseMsgPtr, responseMsg.size());
    EXPECT_EQ(result, NSM_SUCCESS);
}
