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
 * Tests for nsmd/nsmEvent/nsmResetRequiredEvent.cpp
 *
 *   - NsmResetRequiredEvent constructor (no messageArgs)
 *   - NsmResetRequiredEvent constructor (one messageArg)
 *   - NsmResetRequiredEvent constructor (multiple messageArgs joined with
 * comma)
 *   - NsmResetRequiredEvent constructor (errorId key found → adds EventId)
 *   - NsmResetRequiredEvent constructor (impactedComponent → adds DEVICE_NAME)
 *   - NsmResetRequiredEvent::handle (decode failure → returns error)
 *   - NsmResetRequiredEvent::handle (success → returns NSM_SW_SUCCESS)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "platform-environmental.h"

#include "nsmEvent/nsmResetRequiredEvent.hpp"
#include "nsmObjectFactory.hpp"

using namespace nsm;

// Build a valid NsmEventInfo for NsmResetRequiredEvent
static NsmEventInfo makeResetRequiredInfo()
{
    NsmEventInfo info{};
    info.messageId = "ResourceEvent.1.0.ResourceChanged";
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.loggingNamespace = "GPU_Reset";
    info.resolution = "Reboot the system";
    info.messageArgs = {"GPU_1"};
    info.severity = Level::Critical;
    return info;
}

// Encode a reset required event into a buffer
static std::vector<uint8_t> buildResetRequiredEvent()
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_reset_required_event(0, false, msg);
    return buf;
}

// =============================================================================
// Constructor tests
// =============================================================================

TEST(NsmResetRequiredEvent, Constructor_NoMessageArgs_EmptyMessageArgs)
{
    auto info = makeResetRequiredInfo();
    info.messageArgs.clear();
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    EXPECT_EQ(event.getName(), "reset_event");
    EXPECT_EQ(event.getType(), "NSM_ResetRequired");
    EXPECT_EQ(event.messageArgs, "");
}

TEST(NsmResetRequiredEvent, Constructor_OneMessageArg_MessageArgsSet)
{
    auto info = makeResetRequiredInfo();
    info.messageArgs = {"GPU_1"};
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    EXPECT_EQ(event.messageArgs, "GPU_1");
}

TEST(NsmResetRequiredEvent, Constructor_MultipleMessageArgs_JoinedWithComma)
{
    auto info = makeResetRequiredInfo();
    info.messageArgs = {"GPU_1", "arg2", "arg3"};
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    EXPECT_EQ(event.messageArgs, "GPU_1,arg2,arg3");
}

#ifdef ENABLE_EVENT_GPU_UUID_LABEL
// NsmEventInfo::uuid is the entity-manager static device-identification key
// (STATIC:d:d:s:s), never a device identity. It must never leak into the event
// label; see NVBug 6659807.
TEST(NsmResetRequiredEvent, Constructor_GpuMessageArg_DoesNotUseStaticUuid)
{
    auto info = makeResetRequiredInfo();
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    info.messageArgs = {"GPU_1", "ForceRestart"};
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    EXPECT_EQ(event.messageArgs, "GPU_1,ForceRestart");
    EXPECT_EQ(event.messageArgs.find("STATIC:"), std::string::npos);
}

TEST(NsmResetRequiredEvent, MessageArgs_WithDeviceUuid_UsesDeviceUuid)
{
    auto info = makeResetRequiredInfo();
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    info.messageArgs = {"GPU_1", "ForceRestart"};

    EXPECT_EQ(getUuidMessageArgs(info, "6463b098-ca8d-55c1-a65a-4d03c14fb86d"),
              "GPU UUID 6463b098-ca8d-55c1-a65a-4d03c14fb86d,ForceRestart");
}
#endif // ENABLE_EVENT_GPU_UUID_LABEL

TEST(NsmResetRequiredEvent, Constructor_ErrorIdFound_AddsEventId)
{
    auto info = makeResetRequiredInfo();
    // getEventErrorId looks for "ResetRequired" key in pairs
    info.errorId = {"ResetRequired", "ERR-1234", "OtherKey", "OTHER-999"};
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    auto it = event.eventData.find("xyz.openbmc_project.Logging.Entry.EventId");
    ASSERT_NE(it, event.eventData.end());
    EXPECT_EQ(it->second, "ERR-1234");
}

TEST(NsmResetRequiredEvent, Constructor_ImpactedComponent_AddsDeviceName)
{
    auto info = makeResetRequiredInfo();
    info.impactedComponent = "GPU_1_Component";
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    auto it = event.eventData.find("DEVICE_NAME");
    ASSERT_NE(it, event.eventData.end());
    EXPECT_EQ(it->second, "GPU_1_Component");
}

#ifdef ENABLE_EVENT_GPU_UUID_LABEL
TEST(NsmResetRequiredEvent, Constructor_ImpactedComponent_DoesNotUseStaticUuid)
{
    auto info = makeResetRequiredInfo();
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    info.impactedComponent = "GPU_1_Component";
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    auto it = event.eventData.find("DEVICE_NAME");
    ASSERT_NE(it, event.eventData.end());
    EXPECT_EQ(it->second, "GPU_1_Component");
}

TEST(NsmResetRequiredEvent, DeviceName_WithDeviceUuid_UsesDeviceUuid)
{
    EXPECT_EQ(replaceGpuMessageArgDeviceName(
                  "6463b098-ca8d-55c1-a65a-4d03c14fb86d", "GPU_1_Component"),
              "GPU UUID 6463b098-ca8d-55c1-a65a-4d03c14fb86d");
}

TEST(NsmResetRequiredEvent, DeviceName_NonGpuComponentWithDeviceUuid_Unchanged)
{
    EXPECT_EQ(replaceGpuMessageArgDeviceName(
                  "6463b098-ca8d-55c1-a65a-4d03c14fb86d", "NVSwitch_0"),
              "NVSwitch_0");
}
#endif // ENABLE_EVENT_GPU_UUID_LABEL

TEST(NsmResetRequiredEvent, Constructor_NoImpactedComponent_NoDeviceName)
{
    auto info = makeResetRequiredInfo();
    info.impactedComponent = "";
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    EXPECT_EQ(event.eventData.count("DEVICE_NAME"), 0u);
}

// =============================================================================
// handle – decode failure (buffer too small)
// =============================================================================

TEST(NsmResetRequiredEvent, Handle_ShortBuffer_ReturnsError)
{
    auto info = makeResetRequiredInfo();
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handle – success: decoded event, async logging detached
// =============================================================================

TEST(NsmResetRequiredEvent, Handle_ValidEvent_ReturnsSuccess)
{
    auto info = makeResetRequiredInfo();
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    auto buf = buildResetRequiredEvent();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// createNsmResetRequiredEvent factory tests
// =============================================================================

struct NsmResetRequiredEventFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string interfaceName =
        "xyz.openbmc_project.Configuration.NSM_Event_Reset_Required";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/reset_required0";
    const std::string name = "ResetRequired_Test";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmResetRequiredEventFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmResetRequiredEventFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", gpuUuid},
        {"OriginOfCondition",
         std::string("/xyz/openbmc_project/inventory/system/chassis/GPU_1")},
        {"MessageId", std::string("ResourceEvent.1.0.ResourceChanged")},
        {"LoggingNamespace", std::string("GPU_Reset")},
        {"Resolution", std::string("Reboot the system")},
        {"MessageArgs", std::vector<std::string>{"GPU_1"}},
        {"Severity", std::string("Critical")},
    };
};

// Full property map with valid UUID → event registered on device
// Emitted-payload coverage for REDFISH_MESSAGE_ARGS and DEVICE_NAME - see the
// equivalent test in nsmXIDEvent_test.cpp for why the return code alone is not
// enough.
TEST_F(NsmResetRequiredEventFactoryTest, Handle_EmittedPayloadUsesDeviceGuid)
{
    const uuid_t deviceGuid = "6463b098-ca8d-55c1-a65a-4d03c14fb86d";

    gpu->deviceUuid = deviceGuid;

    auto info = makeResetRequiredInfo();
    info.uuid = gpuUuid; // static EM lookup key - must never be emitted
    info.messageArgs = {"GPU_1", "ForceRestart"};
    info.impactedComponent = "GPU_1";

    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info, gpu);

    auto buf = buildResetRequiredEvent();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    ASSERT_EQ(event.handle(gpu->getEid(), NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                           NSM_RESET_REQUIRED_EVENT, msg, buf.size()),
              NSM_SW_SUCCESS);

    const auto& data = event.eventData;
    ASSERT_NE(data.find("REDFISH_MESSAGE_ARGS"), data.end());
    ASSERT_NE(data.find("DEVICE_NAME"), data.end());

    EXPECT_EQ(data.at("REDFISH_MESSAGE_ARGS").find("STATIC:"),
              std::string::npos);
    EXPECT_EQ(data.at("DEVICE_NAME").find("STATIC:"), std::string::npos);

#ifdef ENABLE_EVENT_GPU_UUID_LABEL
    EXPECT_NE(data.at("REDFISH_MESSAGE_ARGS").find(deviceGuid),
              std::string::npos);
    EXPECT_NE(data.at("DEVICE_NAME").find(deviceGuid), std::string::npos);
    // The second configured argument must survive.
    EXPECT_NE(data.at("REDFISH_MESSAGE_ARGS").find("ForceRestart"),
              std::string::npos);
#endif
}

TEST_F(NsmResetRequiredEventFactoryTest, Factory_CreateEvent_Success)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    propertyMap = basicProperties;

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               objPath);

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}

// Missing UUID → empty string → parseStaticUuid fails → exception caught inside
// createObjects → no event added (covers exception path of inner factory)
TEST_F(NsmResetRequiredEventFactoryTest, Factory_MissingUUID_NoEvent)
{
    const std::string uniquePath =
        "/xyz/openbmc_project/inventory/system/reset_no_uuid";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props.erase("UUID");
    propertyMap = props;

    const size_t before = gpu->deviceEvents.size();

    // createObjects catches the std::runtime_error from parseStaticUuid
    // internally
    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               uniquePath);

    EXPECT_EQ(before, gpu->deviceEvents.size());
}

// EventIds present → covers the "EventIds" branch (line 151-154)
TEST_F(NsmResetRequiredEventFactoryTest, Factory_CreateEvent_WithEventIds)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/reset_with_eventids";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props["EventIds"] = std::vector<std::string>{"ResetRequired", "RESET-001"};
    propertyMap = props;

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}

// ImpactedComponent present → covers the "ImpactedComponent" branch (line
// 156-160)
TEST_F(NsmResetRequiredEventFactoryTest,
       Factory_CreateEvent_WithImpactedComponent)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/reset_with_impacted";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props["ImpactedComponent"] = std::string("GPU_Module_1");
    propertyMap = props;

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}

// Severity=Warning → severityEnum.has_value() true → value_or returns actual
// value
TEST_F(NsmResetRequiredEventFactoryTest,
       Factory_CreateEvent_WithSeverityWarning)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/reset_warning";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props["Severity"] = std::string("Warning");
    propertyMap = props;

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}

// Only UUID and Name → all other optional property count() checks return 0
// (FALSE branches) Missing Severity → severityEnum empty →
// value_or(Level::Critical) used
TEST_F(NsmResetRequiredEventFactoryTest, Factory_CreateEvent_MinimalProperties)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/reset_minimal";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    propertyMap = {
        {"Name", name},
        {"UUID", gpuUuid},
    };

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}
