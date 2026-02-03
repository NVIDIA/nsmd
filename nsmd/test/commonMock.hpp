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

#pragma once
#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::ElementsAre;

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmDevice.hpp"
#include "nsmMsghandler.hpp"
#include "nsmSensor.hpp"

#define EXPECT_THROW_COROUTINE(expression, exceptionType)                      \
    if ((expression).exception())                                              \
    {                                                                          \
        EXPECT_THROW(std::rethrow_exception((expression).exception()),         \
                     exceptionType);                                           \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        FAIL() << "Coroutine " << #expression << " did not throw "             \
               << #exceptionType;                                              \
    }

class MockSensor : public nsm::NsmSensor
{
  public:
    MockSensor(const std::string& name, const std::string& type,
               uint8_t returnCode = 0, uint64_t delayUs = 0) :
        NsmSensor(name, type), mockReturnCode(returnCode), mockDelayUs(delayUs)
    {
        // Set refresh limit to 0 so needsUpdate always returns true
        refreshLimitInUsec = 0;
        // Set last updated to old timestamp
        setLastUpdatedTimeStamp(0);
    }

    // Implement pure virtual functions from NsmSensor
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        return std::vector<uint8_t>{};
    }

    uint8_t handleResponseMsg(const nsm_msg* /*responseMsg*/,
                              size_t /*responseLen*/) override
    {
        return mockReturnCode;
    }

    requester::Coroutine update(std::shared_ptr<nsm::NsmDevice>) override
    {
        co_return mockReturnCode;
    }

    uint8_t mockReturnCode;
    uint64_t mockDelayUs; // Simulated operation time in microseconds
};

// Add this concrete test class before the tests
class MockNsmDevice : public nsm::NsmDevice
{
  public:
    // Use the base class constructor
    using NsmDevice::NsmDevice;

    MockNsmDevice(uint8_t deviceType, uint8_t instanceNumber,
                  std::string remapProp, std::string remapPropValue,
                  uint8_t deviceRole) :
        NsmDevice(nullptr, nullptr, deviceType, instanceNumber, remapProp,
                  std::vector<std::string>{remapPropValue}, deviceRole)
    {
        isDeviceActive = true;
        // Set device as ready by default for tests
        isDeviceReady = true;
        // Mark command codes as retrieved to skip refreshCommandMatrix
        areMessageTypesRetrieved = true;
        // Fill in retrieved message types to make allCommandCodesAreRetrieved()
        // return true
        for (int i = 0; i < 256; i++)
        {
            commandCodesRetrieved[i] = true;
        }

        if (remapProp == "NSM_DEVICE_INSTANCE_NUMBER")
        {
            nsmDeviceInstanceNumber = std::stoi(remapPropValue);
        }
        else if (remapProp == "MCTP_EID")
        {
            eid = std::stoi(remapPropValue);
        }
        else if (remapProp == "MCTP_UUID")
        {
            uuid = remapPropValue;
        }
    }

    // Mock the pure virtual functions
    MOCK_METHOD(requester::Coroutine, sensorIO,
                (eid_t eid, Request& request,
                 std::shared_ptr<const nsm_msg>& responseMsg,
                 size_t& responseLen, bool bypassCommandCheck),
                (override));

    MOCK_METHOD(requester::Coroutine, postPatchIO,
                (eid_t eid, Request& request,
                 std::shared_ptr<const nsm_msg>& responseMsg,
                 size_t& responseLen),
                (override));

    MOCK_METHOD(requester::Coroutine, updateNsmDevice, (), (override));

    MOCK_METHOD(requester::Coroutine, refreshCapabilitySensor, (), (override));
};

class MockNsmMessageHandler : public nsm::NsmMessageHandler
{
  public:
    MockNsmMessageHandler(nsm::RequesterHandler& handler) :
        NsmMessageHandler(handler)
    {}

    MOCK_METHOD(requester::Coroutine, SendRecvNsmMsg,
                (eid_t eid, Request& request,
                 std::shared_ptr<const nsm_msg>& responseMsg,
                 size_t* responseLen),
                (override));
};
