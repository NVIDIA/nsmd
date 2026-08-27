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

#include "platform-environmental.h"

#include "nsmXIDEvent.hpp"

namespace nsm
{
requester::Coroutine createNsmXIDEvent(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NsmXIDEventTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Event_XID";
    const std::string name = "XIDEvent_Test";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/test";

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    // NSM DEVICE_GUID as published on the GPU chassis Common.UUID
    const uuid_t deviceGuid = "6463b098-ca8d-55c1-a65a-4d03c14fb86d";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmXIDEventTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
    }

    ~NsmXIDEventTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", gpuUuid},
        {"OriginOfCondition",
         std::string("/xyz/openbmc_project/inventory/system/chassis/GPU_1")},
        {"MessageId", std::string("NVIDIAGPUDiagnostics.1.0.XIDError")},
        {"LoggingNamespace", std::string("GPU_XID")},
        {"Resolution", std::string("Check GPU logs for more details")},
        {"MessageArgs",
         std::vector<std::string>{"{SequenceNumber}", "{Flags}",
                                  "{EventMessageReason}", "{MessageTextString}",
                                  "{Timestamp}"}},
        {"Severity", std::string("Critical")},
        {"EventIds", std::vector<std::string>{"XID_1", "XID_2"}},
        {"ImpactedComponent", std::string("GPU_1")},
    };
};

TEST_F(NsmXIDEventTest, goodTestCreateXIDEvent)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = basicProperties;

    createNsmXIDEvent(mockManager, basicIntfName, objPath);

    // Verify event was added to device (may create multiple events)
    EXPECT_GE(gpu->deviceEvents.size(), 1);
}

TEST_F(NsmXIDEventTest, badTestMissingUUID)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = basicProperties;
    propertyMap.erase("UUID");

    EXPECT_THROW_COROUTINE(
        createNsmXIDEvent(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmXIDEventTest, badTestInvalidUUID)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = basicProperties;
    propertyMap["UUID"] = "INVALID:UUID:FORMAT";

    EXPECT_THROW_COROUTINE(
        createNsmXIDEvent(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmXIDEventTest, testXIDEventConstructor)
{
    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID";
    info.resolution = "Check GPU logs";
    info.messageArgs = {"{SequenceNumber}", "{Flags}"};
    info.severity = Level::Critical;

    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info);

    EXPECT_EQ(xidEvent.getName(), name);
    EXPECT_EQ(xidEvent.getType(), "NSM_Event_XID");
    EXPECT_EQ(xidEvent.info.uuid, gpuUuid);
    EXPECT_EQ(xidEvent.info.messageId, "NVIDIAGPUDiagnostics.1.0.XIDError");
}

#ifdef ENABLE_EVENT_GPU_UUID_LABEL
TEST_F(NsmXIDEventTest, testGpuMessageArgUsesDeviceUuid)
{
    // The label must carry the NSM DEVICE_GUID published on the GPU chassis
    // Common.UUID interface, not the static EM lookup key (NVBug 6659807).
    EXPECT_EQ(
        replaceGpuMessageArgDeviceName("6463b098-ca8d-55c1-a65a-4d03c14fb86d",
                                       "GPU_1 Driver Event Message"),
        "GPU UUID 6463b098-ca8d-55c1-a65a-4d03c14fb86d Driver Event "
        "Message");
}

TEST_F(NsmXIDEventTest, testGpuMessageArgUnresolvedDeviceUuidUnchanged)
{
    EXPECT_EQ(replaceGpuMessageArgDeviceName("", "GPU_1 Driver Event Message"),
              "GPU_1 Driver Event Message");
}
#endif // ENABLE_EVENT_GPU_UUID_LABEL

// ============================================================================
// getEventDeviceUuid: resolves the NSM DEVICE_GUID from the owning NsmDevice.
// These run in BOTH build configurations (the helper is not gated), so they
// also cover nsmEvent/nsmEventInfo.cpp when the GPU UUID label is disabled.
// ============================================================================

TEST_F(NsmXIDEventTest, testGetEventDeviceUuidResolvesFromDevice)
{
    gpu->deviceUuid = deviceGuid;
    EXPECT_EQ(getEventDeviceUuid(gpu), deviceGuid);
}

TEST_F(NsmXIDEventTest, testGetEventDeviceUuidEmptyWhenIdentityNotRead)
{
    gpu->deviceUuid.clear();
    EXPECT_TRUE(getEventDeviceUuid(gpu).empty());
}

TEST_F(NsmXIDEventTest, testGetEventDeviceUuidEmptyWhenDeviceExpired)
{
    std::weak_ptr<NsmDevice> expired;
    {
        auto tmp = std::make_shared<MockNsmDevice>(
            0, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0", 0);
        tmp->deviceUuid = deviceGuid;
        expired = tmp;
        EXPECT_EQ(getEventDeviceUuid(expired), deviceGuid);
    }
    EXPECT_TRUE(getEventDeviceUuid(expired).empty());
}

// utils::convertUUIDToString() returns a UUID_LEN + 1 byte string whose last
// byte is the NUL terminator, so NsmDevice::deviceUuid carries it too. The
// label concatenates a suffix onto that value, so an untrimmed NUL would
// truncate REDFISH_MESSAGE_ARGS at the UUID on the wire and drop every
// argument after it. A clean 36 character literal would not catch that, so
// reproduce the real shape here.
TEST_F(NsmXIDEventTest, testGetEventDeviceUuidTrimsTrailingNul)
{
    gpu->deviceUuid = deviceGuid;
    gpu->deviceUuid.push_back('\0');
    ASSERT_EQ(gpu->deviceUuid.size(), deviceGuid.size() + 1);

    const auto resolved = getEventDeviceUuid(gpu);

    EXPECT_EQ(resolved, deviceGuid);
    EXPECT_EQ(resolved.size(), deviceGuid.size());
    EXPECT_EQ(resolved.find('\0'), std::string::npos);
}

TEST_F(NsmXIDEventTest, testGetEventDeviceUuidEmptyForDefaultConstructed)
{
    EXPECT_TRUE(getEventDeviceUuid(std::weak_ptr<NsmDevice>{}).empty());
}

#ifdef ENABLE_EVENT_GPU_UUID_LABEL
// Regression: with a NUL padded DEVICE_GUID the arguments after the label must
// still reach the wire. std::string::c_str() is what sdbusplus sends, so
// comparing through it is what proves the payload survived.
TEST_F(NsmXIDEventTest, testGpuLabelKeepsTrailingArgsWhenGuidIsNulPadded)
{
    gpu->deviceUuid = deviceGuid;
    gpu->deviceUuid.push_back('\0');

    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.messageArgs = {"GPU_1 Driver Event Message", "XID 79 detail"};

    const auto args = getUuidMessageArgs(info, getEventDeviceUuid(gpu));

    EXPECT_EQ(std::string(args.c_str()),
              "GPU UUID " + deviceGuid + " Driver Event Message,XID 79 detail");
}

// End-to-end: a real NsmDevice carrying a DEVICE_GUID must produce the label
// that Redfish can correlate with Chassis.UUID (NVBug 6659807).
TEST_F(NsmXIDEventTest, testGpuLabelComposedFromDeviceGuidEndToEnd)
{
    gpu->deviceUuid = deviceGuid;

    NsmEventInfo info;
    info.uuid = gpuUuid; // the static EM lookup key - must NOT leak
    info.messageArgs = {"GPU_1 Driver Event Message", "detail"};

    const auto args = getUuidMessageArgs(info, getEventDeviceUuid(gpu));

    EXPECT_EQ(args, "GPU UUID " + deviceGuid + " Driver Event Message,detail");
    EXPECT_EQ(args.find("STATIC:"), std::string::npos);
}
#endif // ENABLE_EVENT_GPU_UUID_LABEL

// Emitted-payload coverage. The direct helper tests and a handle() test that
// only checks the return code would all stay green if the substitution were
// removed from handle(), so assert on the payload the handler assembled and
// submitted. eventData holds exactly the map passed to logEventAsync().
TEST_F(NsmXIDEventTest, Handle_EmittedPayloadUsesDeviceGuid)
{
    gpu->deviceUuid = deviceGuid;

    NsmEventInfo info;
    info.uuid = gpuUuid; // static EM lookup key - must never be emitted
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID";
    info.resolution = "Check GPU logs";
    info.messageArgs = {"GPU_1 Driver Event Message", "{EventMessageReason}"};
    info.impactedComponent = "GPU_1";
    info.severity = Level::Critical;

    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info, gpu);

    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_xid_event_payload) + 100);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    nsm_xid_event_payload payload;
    payload.sequence_number = 12345;
    payload.flag = 1;
    payload.reason = 79;
    payload.timestamp = 1234567890000000;
    const char* messageText = "XID Error occurred";
    ASSERT_EQ(encode_nsm_xid_event(0, false, payload, messageText,
                                   strlen(messageText), msg),
              NSM_SW_SUCCESS);

    ASSERT_EQ(xidEvent.handle(gpu->getEid(), NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                              NSM_XID_EVENT, msg, eventMsg.size()),
              NSM_SW_SUCCESS);

    const auto& data = xidEvent.eventData;
    ASSERT_NE(data.find("REDFISH_MESSAGE_ARGS"), data.end());
    ASSERT_NE(data.find("DEVICE_NAME"), data.end());

    // Holds in both build configurations.
    EXPECT_EQ(data.at("REDFISH_MESSAGE_ARGS").find("STATIC:"),
              std::string::npos);
    EXPECT_EQ(data.at("DEVICE_NAME").find("STATIC:"), std::string::npos);

#ifdef ENABLE_EVENT_GPU_UUID_LABEL
    EXPECT_NE(data.at("REDFISH_MESSAGE_ARGS").find(deviceGuid),
              std::string::npos);
    EXPECT_NE(data.at("DEVICE_NAME").find(deviceGuid), std::string::npos);
    // The argument after the label must survive the concatenation.
    EXPECT_NE(data.at("REDFISH_MESSAGE_ARGS").find(" Driver Event Message"),
              std::string::npos);
#endif
}

TEST_F(NsmXIDEventTest, testHandleXIDEvent)
{
    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID";
    info.resolution = "Check GPU logs";
    info.messageArgs = {"{SequenceNumber}", "{EventMessageReason}"};
    info.severity = Level::Critical;

    gpu->deviceUuid = deviceGuid;
    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info, gpu);

    // Create mock XID event message
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_xid_event_payload) + 100);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    nsm_xid_event_payload payload;
    payload.sequence_number = 12345;
    payload.flag = 1;
    payload.reason = 79;                  // XID 79
    payload.timestamp = 1234567890000000; // nanoseconds

    const char* messageText = "XID Error occurred";
    size_t messageTextSize = strlen(messageText);

    auto rc = encode_nsm_xid_event(0, false, payload, messageText,
                                   messageTextSize, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Handle the event
    rc = xidEvent.handle(gpu->getEid(), NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                         NSM_XID_EVENT, msg, eventMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmXIDEventTest, testWithDifferentSeverities)
{
    std::vector<std::pair<std::string, Level>> severities = {
        {"Critical", Level::Critical},
        {"Warning", Level::Warning},
        {"Informational", Level::Informational}};

    for (const auto& [severityStr, severityEnum] : severities)
    {
        auto props = basicProperties;
        props["Severity"] = severityStr;

        auto& propertyMap = utils::MockDbusAsync::propertyMap(
            objPath + std::to_string((int)severityEnum), basicIntfName);
        propertyMap = props;

        // This should succeed for all valid severity levels
        createNsmXIDEvent(mockManager, basicIntfName,
                          objPath + std::to_string((int)severityEnum));
    }
}

TEST_F(NsmXIDEventTest, testEventWithEmptyMessageArgs)
{
    auto props = basicProperties;
    props["MessageArgs"] = std::vector<std::string>{};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath + "_empty",
                                                          basicIntfName);
    propertyMap = props;

    createNsmXIDEvent(mockManager, basicIntfName, objPath + "_empty");

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}

TEST_F(NsmXIDEventTest, testHandleXIDEventDecodeFailure)
{
    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID";
    info.resolution = "Check GPU logs";
    info.messageArgs = {"{SequenceNumber}"};
    info.severity = Level::Critical;

    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info);

    // Create invalid message (too small)
    std::vector<uint8_t> eventMsg(10);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    // Handle should fail due to invalid message
    auto rc = xidEvent.handle(gpu->getEid(), NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                              NSM_XID_EVENT, msg, eventMsg.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmXIDEventTest, testHandleXIDEventVariousXIDCodes)
{
    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID";
    info.resolution = "Check GPU logs";
    info.messageArgs = {"{EventMessageReason}"};
    info.severity = Level::Warning;
    info.errorId = {"XID_79", "XID_80", "XID_81"};

    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info);

    // Test various XID codes
    std::vector<uint16_t> xidCodes = {79, 80, 81, 94, 95};

    for (auto xidCode : xidCodes)
    {
        std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_xid_event_payload) + 100);
        auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

        nsm_xid_event_payload payload;
        payload.sequence_number = 1;
        payload.flag = 0;
        payload.reason = xidCode;
        payload.timestamp = 1234567890000000;

        const char* messageText = "XID Error";
        size_t messageTextSize = strlen(messageText);

        auto rc = encode_nsm_xid_event(0, false, payload, messageText,
                                       messageTextSize, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        rc = xidEvent.handle(gpu->getEid(), NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                             NSM_XID_EVENT, msg, eventMsg.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
}

TEST_F(NsmXIDEventTest, testEventWithSpecialCharactersInMessageText)
{
    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID";
    info.resolution = "Check GPU logs";
    info.messageArgs = {"{MessageTextString}"};
    info.severity = Level::Critical;

    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info);

    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_xid_event_payload) + 200);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    nsm_xid_event_payload payload;
    payload.sequence_number = 1;
    payload.flag = 1;
    payload.reason = 79;
    payload.timestamp = 1234567890000000;

    // Message with commas (should be replaced with semicolons)
    const char* messageText = "Error, occurred, on, GPU";
    size_t messageTextSize = strlen(messageText);

    auto rc = encode_nsm_xid_event(0, false, payload, messageText,
                                   messageTextSize, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = xidEvent.handle(gpu->getEid(), NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                         NSM_XID_EVENT, msg, eventMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmXIDEventTest, testEventInfoWithAllFields)
{
    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID_Detailed";
    info.resolution = "Replace GPU if error persists";
    info.messageArgs = {"{SequenceNumber}", "{Flags}", "{EventMessageReason}",
                        "{MessageTextString}", "{Timestamp}"};
    info.severity = Level::Critical;
    info.errorId = {"XID_79", "XID_80"};
    info.impactedComponent = "GPU_SXM_1";

    NsmXIDEvent xidEvent(name, "NSM_Event_XID_Full", info);

    EXPECT_EQ(xidEvent.info.messageArgs.size(), 5);
    EXPECT_EQ(xidEvent.info.errorId.size(), 2);
    EXPECT_EQ(xidEvent.info.impactedComponent, "GPU_SXM_1");
}

TEST_F(NsmXIDEventTest, testHandleXIDEventWithLongMessageText)
{
    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID";
    info.resolution = "Check GPU logs";
    info.messageArgs = {"{MessageTextString}"};
    info.severity = Level::Warning;

    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info);

    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_xid_event_payload) +
                                  NSM_EVENT_DATA_MAX_LEN);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    nsm_xid_event_payload payload;
    payload.sequence_number = 999;
    payload.flag = 1;
    payload.reason = 95;
    payload.timestamp =
        std::chrono::system_clock::now().time_since_epoch().count();

    // Very long message text (near max)
    std::string longText(NSM_EVENT_DATA_MAX_LEN - 100,
                         'A'); // Fill with 'A's
    const char* messageText = longText.c_str();
    size_t messageTextSize = longText.size();

    auto rc = encode_nsm_xid_event(0, false, payload, messageText,
                                   messageTextSize, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = xidEvent.handle(gpu->getEid(), NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                         NSM_XID_EVENT, msg, eventMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmXIDEventTest, testHandleXIDEventWithInvalidFormatSpecifier)
{
    // Test exception handling in fmt::vformat (lines 89-95)
    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID";
    info.resolution = "Check GPU logs";
    // Invalid format specifier that will cause fmt::vformat to throw
    info.messageArgs = {"{InvalidSpecifier}", "{AnotherInvalid}"};
    info.severity = Level::Critical;

    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info);

    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_xid_event_payload) + 100);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    nsm_xid_event_payload payload;
    payload.sequence_number = 1;
    payload.flag = 1;
    payload.reason = 79;
    payload.timestamp = 1234567890000000;

    const char* messageText = "Test message";
    size_t messageTextSize = strlen(messageText);

    auto rc = encode_nsm_xid_event(0, false, payload, messageText,
                                   messageTextSize, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Should handle exception gracefully and still return success
    rc = xidEvent.handle(gpu->getEid(), NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                         NSM_XID_EVENT, msg, eventMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmXIDEventTest, testHandleXIDEventWithErrorIdAndImpactedComponent)
{
    // Test lines 122 and 126 - setting EventId and DEVICE_NAME
    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID";
    info.resolution = "Check GPU logs";
    info.messageArgs = {"{EventMessageReason}"};
    info.severity = Level::Warning;
    // Set errorId list with XID 79 to ensure getEventErrorId returns non-empty
    // Format: key-value pairs where key="XID 79" (with space), value=error ID
    info.errorId = {"XID 79", "GPU_ERROR_79"};
    // Set impacted component to trigger line 126
    info.impactedComponent = "GPU_Module_1";

    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info);

    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_xid_event_payload) + 100);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    nsm_xid_event_payload payload;
    payload.sequence_number = 42;
    payload.flag = 1;
    payload.reason = 79; // Matches XID_79 in errorId
    payload.timestamp = 1234567890000000;

    const char* messageText = "XID 79 error";
    size_t messageTextSize = strlen(messageText);

    auto rc = encode_nsm_xid_event(0, false, payload, messageText,
                                   messageTextSize, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // This should cover lines 122 and 126
    rc = xidEvent.handle(gpu->getEid(), NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                         NSM_XID_EVENT, msg, eventMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// FALSE-branch coverage: count() checks in createNsmXIDEvent
// =============================================================================

// Only UUID present → all 9 optional property count() checks return FALSE:
//   Name, OriginOfCondition, MessageId, LoggingNamespace, Resolution,
//   MessageArgs, Severity, EventIds, ImpactedComponent all absent →
//   event created with empty/default values; severity=Level::Critical
//   (via value_or since severityStr="" → convertStringToLevel returns nullopt)
TEST_F(NsmXIDEventTest, Factory_MinimalProperties_EventCreatedWithDefaults)
{
    const std::string uniquePath = "/xyz/test/xid/minimal_props";
    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    // Only UUID → all other count() checks return 0 (FALSE branches)
    pm["UUID"] = gpuUuid;

    const size_t before = gpu->deviceEvents.size();
    createNsmXIDEvent(mockManager, basicIntfName, uniquePath);
    // Event created with all-default optional fields
    EXPECT_GT(gpu->deviceEvents.size(), before);
}

// Covers the FALSE branch of `if (!formattedMessageArgs.empty())` (line 100).
// When info.messageArgs is empty the for-loop never runs, formattedMessageArgs
// stays empty, and the if-body is skipped → messageArgs stays "".
TEST_F(NsmXIDEventTest, testHandleXIDEvent_EmptyMessageArgs_SkipsIfBody)
{
    NsmEventInfo info;
    info.uuid = gpuUuid;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.messageId = "NVIDIAGPUDiagnostics.1.0.XIDError";
    info.loggingNamespace = "GPU_XID";
    info.resolution = "Check GPU logs";
    info.messageArgs = {}; // empty → formattedMessageArgs stays empty
    info.severity = Level::Critical;

    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info);

    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_xid_event_payload) + 50);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    nsm_xid_event_payload payload{};
    payload.sequence_number = 1;
    payload.flag = 0;
    payload.reason = 79;
    payload.timestamp = 1234567890000000;

    const char* messageText = "Empty args test";
    auto rc = encode_nsm_xid_event(0, false, payload, messageText,
                                   strlen(messageText), msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = xidEvent.handle(gpu->getEid(), NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                         NSM_XID_EVENT, msg, eventMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
