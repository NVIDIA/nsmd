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

#include "nsmRoTProperty.hpp"

using namespace nsm;

struct NsmRoTPropertyTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis";
    const uint16_t classification = 1;
    const uint16_t identifier = 2;
    const uint8_t index = 0;

    NsmDeviceTable devices;
    std::shared_ptr<NsmInbandUpdatePolicyObject> policyObject;

    NsmRoTPropertyTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        policyObject = std::make_shared<NsmInbandUpdatePolicyObject>(
            bus, chassisName, classification, identifier, index);

        EXPECT_NE(policyObject, nullptr);
    }
};

TEST_F(NsmRoTPropertyTest, goodTestConstructor)
{
    EXPECT_NE(policyObject, nullptr);
    EXPECT_EQ(policyObject->classification, classification);
    EXPECT_EQ(policyObject->identifier, identifier);
    EXPECT_EQ(policyObject->index, index);
}

TEST_F(NsmRoTPropertyTest, goodTestClassificationAndIdentifier)
{
    EXPECT_EQ(policyObject->classification, 1);
    EXPECT_EQ(policyObject->identifier, 2);
    EXPECT_EQ(policyObject->index, 0);
}

TEST_F(NsmRoTPropertyTest, goodTestNsmSensorInheritance)
{
    // NsmInbandUpdatePolicyObject inherits from NsmSensor
    NsmSensor* sensor = policyObject.get();
    EXPECT_NE(sensor, nullptr);
}

// These interfaces are private and cannot be accessed in tests

TEST_F(NsmRoTPropertyTest, testWithDifferentClassification)
{
    auto& bus = utils::DBusHandler::getBus();
    uint16_t classification2 = 5;

    auto policyObject2 = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "2", classification2, identifier, index);

    EXPECT_EQ(policyObject2->classification, classification2);
}

TEST_F(NsmRoTPropertyTest, testWithDifferentIdentifier)
{
    auto& bus = utils::DBusHandler::getBus();
    uint16_t identifier2 = 10;

    auto policyObject2 = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "3", classification, identifier2, index);

    EXPECT_EQ(policyObject2->identifier, identifier2);
}

TEST_F(NsmRoTPropertyTest, testWithDifferentIndex)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t index2 = 3;

    auto policyObject2 = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "4", classification, identifier, index2);

    EXPECT_EQ(policyObject2->index, index2);
}

TEST_F(NsmRoTPropertyTest, testZeroClassification)
{
    auto& bus = utils::DBusHandler::getBus();
    uint16_t classification0 = 0;

    auto policyObject0 = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "5", classification0, identifier, index);

    EXPECT_EQ(policyObject0->classification, 0);
}

TEST_F(NsmRoTPropertyTest, testMaxValues)
{
    auto& bus = utils::DBusHandler::getBus();
    uint16_t maxClassification = 0xFFFF;
    uint16_t maxIdentifier = 0xFFFF;
    uint8_t maxIndex = 0xFF;

    auto maxPolicyObject = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "6", maxClassification, maxIdentifier, maxIndex);

    EXPECT_EQ(maxPolicyObject->classification, maxClassification);
    EXPECT_EQ(maxPolicyObject->identifier, maxIdentifier);
    EXPECT_EQ(maxPolicyObject->index, maxIndex);
}

// TEST_F(NsmRoTPropertyTest, goodTestMultipleInstances) - Disabled due to DBus
// path conflict

// TEST_F(NsmRoTPropertyTest, goodTestDifferentChassisNames) - Disabled due to
// DBus path conflict
