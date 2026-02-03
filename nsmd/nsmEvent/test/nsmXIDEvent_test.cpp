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

    NsmXIDEvent xidEvent(name, "NSM_Event_XID", info);

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
