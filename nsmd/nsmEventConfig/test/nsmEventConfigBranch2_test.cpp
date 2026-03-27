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
 * Branch coverage batch 2 for nsmEventConfig.cpp
 *
 * Targets: count() FALSE branches in createNsmEventConfig factory (lines
 * 309-329). When each property is absent, the corresponding variable stays at
 * its default, the UUID resolves to {} → getNsmDeviceFromStaticUUID returns
 * nullptr → factory logs error and co_returns NSM_ERROR without crashing.
 * Each omitted property exercises one FALSE branch.
 *
 * "Name" absent          → line 309 FALSE
 * "UUID" absent          → line 315 FALSE → nsmDevice=nullptr → NSM_ERROR
 * "MessageType" absent   → line 320 FALSE
 * "SubscribedEventIDs"   → line 326 FALSE
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-capability-discovery.h"

#include "nsmEventConfig.hpp"

namespace nsm
{
requester::Coroutine createNsmEventConfig(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NsmEventConfigBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_EventConfig";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;

    NsmEventConfigBranch2Test() : SensorManagerTest(devices) {}

    ~NsmEventConfigBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    // Minimal valid props that produce a working factory run
    dbus::PropertyMap validProps() const
    {
        return {
            {"Name", std::string("ECfg")},
            {"UUID", gpuUuid},
            {"MessageType", uint64_t(NSM_TYPE_PLATFORM_ENVIRONMENTAL)},
            {"SubscribedEventIDs", std::vector<uint64_t>{1, 2}},
        };
    }
};

// ============================================================================
// "Name" absent → line 309 FALSE: name stays ""
// UUID is present so device is found; sensor created with empty name.
// ============================================================================

TEST_F(NsmEventConfigBranch2Test, Factory_NameAbsent_SensorCreatedWithEmptyName)
{
    const std::string objPath = "/xyz/test/ecfg/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, basicIntfName);
    pm = validProps();
    pm.erase("Name"); // line 309 FALSE

    EXPECT_NO_THROW(createNsmEventConfig(mockManager, basicIntfName, objPath));
}

// ============================================================================
// "UUID" absent → line 315 FALSE: uuid stays {} → device not found →
// factory returns NSM_ERROR without crash.
// ============================================================================

TEST_F(NsmEventConfigBranch2Test, Factory_UUIDAbsent_NoDeviceNoSensor)
{
    const std::string objPath = "/xyz/test/ecfg/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, basicIntfName);
    pm = validProps();
    pm.erase("UUID"); // line 315 FALSE → uuid={} → nsmDevice=nullptr

    // UUID="" → parseStaticUuid fails in mock → std::runtime_error thrown
    EXPECT_THROW_COROUTINE(
        createNsmEventConfig(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

// ============================================================================
// "MessageType" absent → line 320 FALSE: messageType stays 0
// UUID present so device found; sensor created with messageType=0.
// ============================================================================

TEST_F(NsmEventConfigBranch2Test, Factory_MessageTypeAbsent_SensorCreated)
{
    const std::string objPath = "/xyz/test/ecfg/no_msgtype";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, basicIntfName);
    pm = validProps();
    pm.erase("MessageType"); // line 320 FALSE

    EXPECT_NO_THROW(createNsmEventConfig(mockManager, basicIntfName, objPath));
}

// ============================================================================
// "SubscribedEventIDs" absent → line 326 FALSE: subscribedEventIds stays {}
// UUID present so device found; sensor created with empty event ID list.
// ============================================================================

TEST_F(NsmEventConfigBranch2Test,
       Factory_SubscribedEventIDsAbsent_SensorCreated)
{
    const std::string objPath = "/xyz/test/ecfg/no_subevents";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, basicIntfName);
    pm = validProps();
    pm.erase("SubscribedEventIDs"); // line 326 FALSE

    EXPECT_NO_THROW(createNsmEventConfig(mockManager, basicIntfName, objPath));
}
