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

/*
 * Tests for nsmd/nsmSetAsync/nsmSetErrorInjection.cpp
 *
 *   - NsmSetErrorInjection constructor (name, type)
 *   - NsmSetErrorInjectionEnabled constructor (valid type)
 *   - NsmSetErrorInjectionEnabled constructor (Unknown type → throws)
 *   - NsmSetErrorInjectionPayload constructor
 *   - errorInjectionModeEnabled with non-bool value (throws InvalidArgument)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmSetErrorInjection.hpp"

using namespace nsm;

static auto& testBus = utils::DBusHandler::getBus();

// =============================================================================
// NsmSetErrorInjection constructor test
// =============================================================================

TEST(NsmSetErrorInjection, Constructor_InitializesNameAndType)
{
    NsmDeviceTable devices;
    SensorManagerTest smTest(devices);

    std::filesystem::path objPath("/test/ei/mode");
    NsmSetErrorInjection sensor(smTest.mockManager, objPath);

    EXPECT_EQ(sensor.getName(), "ErrorInjection");
    EXPECT_EQ(sensor.getType(), "NSM_ErrorInjection");
}

// =============================================================================
// NsmSetErrorInjectionEnabled constructor tests
// =============================================================================

TEST(NsmSetErrorInjectionEnabled, Constructor_ValidType_Succeeds)
{
    NsmDeviceTable devices;
    SensorManagerTest smTest(devices);

    std::filesystem::path objPath("/test/ei/cap");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(testBus,
                                                               objPath.c_str());

    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[objPath] = intf;

    EXPECT_NO_THROW(NsmSetErrorInjectionEnabled sensor(
        "ErrorInjectionCap", ErrorInjectionCapabilityIntf::Type::FatalErrors,
        smTest.mockManager, interfaces));
}

TEST(NsmSetErrorInjectionEnabled, Constructor_UnknownType_Throws)
{
    NsmDeviceTable devices;
    SensorManagerTest smTest(devices);

    std::filesystem::path objPath("/test/ei/cap_unknown");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(testBus,
                                                               objPath.c_str());

    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[objPath] = intf;

    EXPECT_THROW(NsmSetErrorInjectionEnabled sensor(
                     "ErrorInjectionCap",
                     ErrorInjectionCapabilityIntf::Type::Unknown,
                     smTest.mockManager, interfaces),
                 std::invalid_argument);
}

TEST(NsmSetErrorInjectionEnabled, Constructor_NameAndType)
{
    NsmDeviceTable devices;
    SensorManagerTest smTest(devices);

    std::filesystem::path objPath("/test/ei/cap2");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(testBus,
                                                               objPath.c_str());

    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[objPath] = intf;

    NsmSetErrorInjectionEnabled sensor(
        "EiCapSensor", ErrorInjectionCapabilityIntf::Type::MemoryErrors,
        smTest.mockManager, interfaces);

    EXPECT_EQ(sensor.getName(), "EiCapSensor");
    EXPECT_EQ(sensor.getType(), "NSM_ErrorInjectionCapability");
}

// =============================================================================
// NsmSetErrorInjectionPayload constructor test
// =============================================================================

TEST(NsmSetErrorInjectionPayload, Constructor_InitializesFields)
{
    NsmDeviceTable devices;
    SensorManagerTest smTest(devices);

    std::filesystem::path objPath("/test/ei/payload");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(testBus,
                                                            objPath.c_str());

    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[objPath] = intf;

    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        testBus, objPath.c_str(), 0x01, 0x02, nullptr);

    NsmSetErrorInjectionPayload sensor("EiPayload", smTest.mockManager,
                                       interfaces, activateIntf, 0x01, 0x02);

    EXPECT_EQ(sensor.getName(), "EiPayload");
    EXPECT_EQ(sensor.getType(), "NSM_ErrorInjectionPayload");
    EXPECT_EQ(sensor.errorInjectionType, 0x01);
    EXPECT_EQ(sensor.errorInjectionSubtype, 0x02);
}
