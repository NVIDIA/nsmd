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
 * Branch coverage for nsmd/nsmEvent/nsmRediscoveryEvent.cpp factory function
 *
 * Covers FALSE branches in createNsmRediscoveryEvent (lines 186-222):
 * - "Severity" absent → severityStr="" → convertStringToLevel returns nullopt
 *   → value_or(Critical)
 * - "Severity" present = "Warning" → value_or uses actual value
 * - UUID present but device not found → NSM_ERROR path
 * - All properties present → success path (event added to device)
 * - "OriginOfCondition" absent → stays empty
 * - "MessageId" absent → stays empty
 * - "LoggingNamespace" absent → stays empty
 * - "Resolution" absent → stays empty
 * - "MessageArgs" absent → stays empty
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-capability-discovery.h"

#include "nsmEvent/nsmRediscoveryEvent.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine createNsmRediscoveryEvent(SensorManager& manager,
                                               const std::string& interface,
                                               const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmRediscoveryEventBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string interfaceName =
        "xyz.openbmc_project.Configuration.NSM_Event_Rediscovery";
    const std::string basePath =
        "/xyz/openbmc_project/inventory/system/rediscovery_br";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:2";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmRediscoveryEventBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmRediscoveryEventBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProps() const
    {
        return {
            {"Name", std::string("RediscBranch")},
            {"UUID", gpuUuid},
            {"Logging", bool(true)},
        };
    }
};

// ============================================================================
// "Severity" absent → severityStr="" → convertStringToLevel returns nullopt
// → value_or(Critical) used (lines 189-200)
// ============================================================================

TEST_F(NsmRediscoveryEventBranchTest, Factory_SeverityAbsent_DefaultsCritical)
{
    const std::string path = basePath + "/sev_absent";
    auto& pm = utils::MockDbusAsync::propertyMap(path, interfaceName);
    pm = basicProps();
    // No "Severity" → FALSE branch at line ~190

    const size_t before = gpu->deviceEvents.size();
    EXPECT_NO_THROW(
        createNsmRediscoveryEvent(mockManager, interfaceName, path));
    // Event was added (success path)
    EXPECT_GT(gpu->deviceEvents.size(), before);
}

// ============================================================================
// "Severity" present = "Warning" → convertStringToLevel has_value() TRUE
// → value_or uses actual value (lines 190-200 TRUE branch)
// ============================================================================

TEST_F(NsmRediscoveryEventBranchTest, Factory_SeverityWarning_UsesWarning)
{
    const std::string path = basePath + "/sev_warning";
    auto& pm = utils::MockDbusAsync::propertyMap(path, interfaceName);
    pm = basicProps();
    pm["Severity"] = std::string("Warning");

    const size_t before = gpu->deviceEvents.size();
    EXPECT_NO_THROW(
        createNsmRediscoveryEvent(mockManager, interfaceName, path));
    EXPECT_GT(gpu->deviceEvents.size(), before);
}

// ============================================================================
// "OriginOfCondition" absent → stays "" (FALSE branch at line ~159)
// ============================================================================

TEST_F(NsmRediscoveryEventBranchTest,
       Factory_OriginOfConditionAbsent_StaysEmpty)
{
    const std::string path = basePath + "/no_origin";
    auto& pm = utils::MockDbusAsync::propertyMap(path, interfaceName);
    pm = basicProps();
    // No OriginOfCondition

    EXPECT_NO_THROW(
        createNsmRediscoveryEvent(mockManager, interfaceName, path));
}

// ============================================================================
// "MessageId" absent → stays "" (FALSE branch at line ~164)
// ============================================================================

TEST_F(NsmRediscoveryEventBranchTest, Factory_MessageIdAbsent_StaysEmpty)
{
    const std::string path = basePath + "/no_msgid";
    auto& pm = utils::MockDbusAsync::propertyMap(path, interfaceName);
    pm = basicProps();
    // No MessageId

    EXPECT_NO_THROW(
        createNsmRediscoveryEvent(mockManager, interfaceName, path));
}

// ============================================================================
// "LoggingNamespace" absent → stays "" (FALSE branch at line ~169)
// ============================================================================

TEST_F(NsmRediscoveryEventBranchTest, Factory_LoggingNamespaceAbsent_StaysEmpty)
{
    const std::string path = basePath + "/no_logns";
    auto& pm = utils::MockDbusAsync::propertyMap(path, interfaceName);
    pm = basicProps();
    // No LoggingNamespace

    EXPECT_NO_THROW(
        createNsmRediscoveryEvent(mockManager, interfaceName, path));
}

// ============================================================================
// "Resolution" absent → stays "" (FALSE branch at line ~176)
// ============================================================================

TEST_F(NsmRediscoveryEventBranchTest, Factory_ResolutionAbsent_StaysEmpty)
{
    const std::string path = basePath + "/no_res";
    auto& pm = utils::MockDbusAsync::propertyMap(path, interfaceName);
    pm = basicProps();
    // No Resolution

    EXPECT_NO_THROW(
        createNsmRediscoveryEvent(mockManager, interfaceName, path));
}

// ============================================================================
// "MessageArgs" absent → stays empty vector (FALSE branch at line ~181)
// ============================================================================

TEST_F(NsmRediscoveryEventBranchTest, Factory_MessageArgsAbsent_StaysEmpty)
{
    const std::string path = basePath + "/no_msgargs";
    auto& pm = utils::MockDbusAsync::propertyMap(path, interfaceName);
    pm = basicProps();
    // No MessageArgs

    EXPECT_NO_THROW(
        createNsmRediscoveryEvent(mockManager, interfaceName, path));
}

// ============================================================================
// "MessageArgs" present → used in construction (TRUE branch at ~181)
// ============================================================================

TEST_F(NsmRediscoveryEventBranchTest, Factory_MessageArgsPresent_Used)
{
    const std::string path = basePath + "/with_msgargs";
    auto& pm = utils::MockDbusAsync::propertyMap(path, interfaceName);
    pm = basicProps();
    pm["MessageArgs"] = std::vector<std::string>{"arg1", "arg2"};

    EXPECT_NO_THROW(
        createNsmRediscoveryEvent(mockManager, interfaceName, path));
}

// ============================================================================
// All optional properties present → success path, event created
// ============================================================================

TEST_F(NsmRediscoveryEventBranchTest, Factory_AllProperties_EventCreated)
{
    const std::string path = basePath + "/all_props";
    auto& pm = utils::MockDbusAsync::propertyMap(path, interfaceName);
    pm = {
        {"Name", std::string("RediscAll")},
        {"UUID", gpuUuid},
        {"Severity", std::string("Critical")},
        {"OriginOfCondition", std::string("/xyz/origin")},
        {"MessageId", std::string("com.nvidia.Msg.Id")},
        {"LoggingNamespace", std::string("TestNamespace")},
        {"Resolution", std::string("Reboot the device")},
        {"MessageArgs", std::vector<std::string>{"x", "y"}},
        {"Logging", bool(true)},
    };

    const size_t before = gpu->deviceEvents.size();
    EXPECT_NO_THROW(
        createNsmRediscoveryEvent(mockManager, interfaceName, path));
    EXPECT_GT(gpu->deviceEvents.size(), before);
}

// ============================================================================
// UUID present but no matching device → nsmDevice=nullptr → NSM_ERROR
// (lines 202-210)
// ============================================================================

TEST_F(NsmRediscoveryEventBranchTest, Factory_DeviceNotFound_ReturnsError)
{
    const std::string path = basePath + "/no_device";
    auto& pm = utils::MockDbusAsync::propertyMap(path, interfaceName);
    pm = basicProps();
    // Use a UUID that doesn't exist in the device table
    pm["UUID"] = uuid_t("STATIC:99:99:NSM_DEVICE_INSTANCE_NUMBER:99");

    // factory returns NSM_ERROR (co_return), does not throw
    const size_t before = gpu->deviceEvents.size();
    EXPECT_NO_THROW(
        createNsmRediscoveryEvent(mockManager, interfaceName, path));
    // No event should have been added (device not found)
    EXPECT_EQ(gpu->deviceEvents.size(), before);
}
