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

#include "nsmDebugTokenUnified.hpp"

using namespace nsm;

struct NsmDebugTokenUnifiedTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "Unified_DebugToken";
    const uuid_t uuid = "ABCDEF12-3456-7890-ABCD-EF1234567890";
    const std::string debugTokenDeviceType = "TestDeviceType";

    NsmDeviceTable devices;

    NsmDebugTokenUnifiedTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmDebugTokenUnifiedTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);

    EXPECT_NE(debugToken, nullptr);
    EXPECT_EQ(debugToken->uuid, uuid);
    EXPECT_EQ(debugToken->getName(), name);
    EXPECT_EQ(debugToken->getType(), "NSM_DebugTokenUnified");
}

TEST_F(NsmDebugTokenUnifiedTest, testConstructorWithDifferentNames)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken1 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_1", uuid, debugTokenDeviceType);

    auto debugToken2 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_2", uuid, debugTokenDeviceType);

    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->getName(), "Unified_DebugToken_1");
    EXPECT_EQ(debugToken2->getName(), "Unified_DebugToken_2");
}

TEST_F(NsmDebugTokenUnifiedTest, testConstructorWithDifferentUUIDs)
{
    auto& bus = utils::DBusHandler::getBus();

    uuid_t uuid1 = "11111111-1111-1111-1111-111111111111";
    uuid_t uuid2 = "22222222-2222-2222-2222-222222222222";

    auto debugToken1 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_UUID1", uuid1, debugTokenDeviceType);

    auto debugToken2 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_UUID2", uuid2, debugTokenDeviceType);

    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->uuid, uuid1);
    EXPECT_EQ(debugToken2->uuid, uuid2);
    EXPECT_NE(debugToken1->uuid, debugToken2->uuid);
}

TEST_F(NsmDebugTokenUnifiedTest, testNsmObjectInheritance)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);

    NsmObject* nsmObj = debugToken.get();
    EXPECT_NE(nsmObj, nullptr);
    EXPECT_EQ(nsmObj->getName(), name);
    EXPECT_EQ(nsmObj->getType(), "NSM_DebugTokenUnified");
}

TEST_F(NsmDebugTokenUnifiedTest, testInstallationChunkSizeInitial)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);

    EXPECT_EQ(debugToken->installationChunkSize, 0);
}

TEST_F(NsmDebugTokenUnifiedTest, testMultipleInstancesWithSameUUID)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken1 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_Same1", uuid, debugTokenDeviceType);

    auto debugToken2 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_Same2", uuid, debugTokenDeviceType);

    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->uuid, debugToken2->uuid);
    EXPECT_NE(debugToken1->getName(), debugToken2->getName());
}

TEST_F(NsmDebugTokenUnifiedTest, testTypeIsConsistent)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken1 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_A", uuid, debugTokenDeviceType);

    auto debugToken2 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_B", uuid, debugTokenDeviceType);

    EXPECT_EQ(debugToken1->getType(), debugToken2->getType());
    EXPECT_EQ(debugToken1->getType(), "NSM_DebugTokenUnified");
}

TEST_F(NsmDebugTokenUnifiedTest, testUUIDStorage)
{
    auto& bus = utils::DBusHandler::getBus();

    uuid_t testUuid = "FEDCBA09-8765-4321-FEDC-BA0987654321";

    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, testUuid, debugTokenDeviceType);

    EXPECT_EQ(debugToken->uuid, testUuid);
}

TEST_F(NsmDebugTokenUnifiedTest, testNameStorage)
{
    auto& bus = utils::DBusHandler::getBus();

    std::string testName = "CustomUnifiedDebugToken";

    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, testName, uuid, debugTokenDeviceType);

    EXPECT_EQ(debugToken->getName(), testName);
}

TEST_F(NsmDebugTokenUnifiedTest, testDebugTokenActionIntfInheritance)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);

    DebugTokenActionIntf* actionIntf = debugToken.get();
    EXPECT_NE(actionIntf, nullptr);
}

TEST_F(NsmDebugTokenUnifiedTest, testDebugTokenStatusIntfInheritance)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);

    DebugTokenStatusIntf* statusIntf = debugToken.get();
    EXPECT_NE(statusIntf, nullptr);
}

TEST_F(NsmDebugTokenUnifiedTest, testInstallationChunkSizeModification)
{
    auto& bus = utils::DBusHandler::getBus();

    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);

    debugToken->installationChunkSize = 4096;
    EXPECT_EQ(debugToken->installationChunkSize, 4096);

    debugToken->installationChunkSize = 8192;
    EXPECT_EQ(debugToken->installationChunkSize, 8192);
}
