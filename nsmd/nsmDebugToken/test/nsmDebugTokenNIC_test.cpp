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

#include "nsmDebugTokenNIC.hpp"

using namespace nsm;

struct NsmDebugTokenNICTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "NIC_DebugToken";
    const uuid_t uuid = "12345678-1234-1234-1234-123456789012";

    NsmDeviceTable devices;

    NsmDebugTokenNICTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmDebugTokenNICTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken = std::make_shared<NsmDebugTokenNICObject>(bus, name, uuid);

    EXPECT_NE(debugToken, nullptr);
    EXPECT_EQ(debugToken->uuid, uuid);
    EXPECT_EQ(debugToken->getName(), name);
    EXPECT_EQ(debugToken->getType(), "NSM_DebugTokenNIC");
}

TEST_F(NsmDebugTokenNICTest, testConstructorWithDifferentNames)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken1 =
        std::make_shared<NsmDebugTokenNICObject>(bus, "NIC_DebugToken_1", uuid);

    auto debugToken2 =
        std::make_shared<NsmDebugTokenNICObject>(bus, "NIC_DebugToken_2", uuid);

    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->getName(), "NIC_DebugToken_1");
    EXPECT_EQ(debugToken2->getName(), "NIC_DebugToken_2");
}

TEST_F(NsmDebugTokenNICTest, testConstructorWithDifferentUUIDs)
{
    auto& bus = utils::DBusHandler::getBus();

    uuid_t uuid1 = "11111111-1111-1111-1111-111111111111";
    uuid_t uuid2 = "22222222-2222-2222-2222-222222222222";

    auto debugToken1 = std::make_shared<NsmDebugTokenNICObject>(
        bus, "NIC_DebugToken_UUID1", uuid1);

    auto debugToken2 = std::make_shared<NsmDebugTokenNICObject>(
        bus, "NIC_DebugToken_UUID2", uuid2);

    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->uuid, uuid1);
    EXPECT_EQ(debugToken2->uuid, uuid2);
    EXPECT_NE(debugToken1->uuid, debugToken2->uuid);
}

TEST_F(NsmDebugTokenNICTest, testNsmObjectInheritance)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken = std::make_shared<NsmDebugTokenNICObject>(bus, name, uuid);

    NsmObject* nsmObj = debugToken.get();
    EXPECT_NE(nsmObj, nullptr);
    EXPECT_EQ(nsmObj->getName(), name);
    EXPECT_EQ(nsmObj->getType(), "NSM_DebugTokenNIC");
}

TEST_F(NsmDebugTokenNICTest, testSuccessReasonCode)
{
    EXPECT_EQ(successReasonCode, 0);
}

TEST_F(NsmDebugTokenNICTest, testTokenAlreadyActiveReasonCode)
{
    EXPECT_EQ(tokenAlreadyActiveReasonCode, 1);
}

TEST_F(NsmDebugTokenNICTest, testMultipleInstancesWithSameUUID)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken1 = std::make_shared<NsmDebugTokenNICObject>(
        bus, "NIC_DebugToken_Same1", uuid);

    auto debugToken2 = std::make_shared<NsmDebugTokenNICObject>(
        bus, "NIC_DebugToken_Same2", uuid);

    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->uuid, debugToken2->uuid);
    EXPECT_NE(debugToken1->getName(), debugToken2->getName());
}

TEST_F(NsmDebugTokenNICTest, testTypeIsConsistent)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken1 =
        std::make_shared<NsmDebugTokenNICObject>(bus, "NIC_DebugToken_A", uuid);

    auto debugToken2 =
        std::make_shared<NsmDebugTokenNICObject>(bus, "NIC_DebugToken_B", uuid);

    EXPECT_EQ(debugToken1->getType(), debugToken2->getType());
    EXPECT_EQ(debugToken1->getType(), "NSM_DebugTokenNIC");
}

TEST_F(NsmDebugTokenNICTest, testUUIDStorage)
{
    auto& bus = utils::DBusHandler::getBus();

    uuid_t testUuid = "ABCDEF12-3456-7890-ABCD-EF1234567890";

    auto debugToken = std::make_shared<NsmDebugTokenNICObject>(bus, name,
                                                               testUuid);

    EXPECT_EQ(debugToken->uuid, testUuid);
}

TEST_F(NsmDebugTokenNICTest, testNameStorage)
{
    auto& bus = utils::DBusHandler::getBus();

    std::string testName = "CustomNICDebugToken";

    auto debugToken = std::make_shared<NsmDebugTokenNICObject>(bus, testName,
                                                               uuid);

    EXPECT_EQ(debugToken->getName(), testName);
}
