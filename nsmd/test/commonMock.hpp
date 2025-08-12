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

// Add this concrete test class before the tests
class MockNsmDeviceBase : public nsm::NsmDevice
{
  public:
    // Use the base class constructor
    using NsmDevice::NsmDevice;

    MockNsmDeviceBase(uint8_t deviceType, uint8_t instanceNumber,
                      std::string remapProp, std::string remapPropValue,
                      uint8_t deviceRole) :
        NsmDevice(nullptr, nullptr, deviceType, instanceNumber, remapProp,
                  remapPropValue, deviceRole)
    {
        isDeviceActive = true;
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
};
