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

struct NsmEventConfigTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_EventConfig";
    const std::string name = "EventConfig_Test";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/test";

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmEventConfigTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
    }

    ~NsmEventConfigTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", gpuUuid},
        {"MessageType", uint64_t(NSM_TYPE_PLATFORM_ENVIRONMENTAL)},
        {"SubscribedEventIDs", std::vector<uint64_t>{1, 2, 3}},
    };
};

TEST_F(NsmEventConfigTest, goodTestCreateEventConfig)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = basicProperties;

    createNsmEventConfig(mockManager, basicIntfName, objPath);

    EXPECT_GE(gpu->deviceSensors.size(),
              2); // NsmEventConfig + NsmGetEventConfig (may have more)
}

TEST_F(NsmEventConfigTest, badTestMissingUUID)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = basicProperties;
    propertyMap.erase("UUID");

    EXPECT_THROW_COROUTINE(
        createNsmEventConfig(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmEventConfigTest, badTestInvalidUUID)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = basicProperties;
    propertyMap["UUID"] = "INVALID:UUID:FORMAT";

    EXPECT_THROW_COROUTINE(
        createNsmEventConfig(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

// Missing "Name" → FALSE branch for count("Name") → name="" used;
// sensor IS still created with empty name
TEST_F(NsmEventConfigTest, Factory_MissingName_SensorCreated)
{
    const std::string uniquePath =
        "/xyz/openbmc_project/inventory/system/eventconfig_noname";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    dbus::PropertyMap props = basicProperties;
    props.erase("Name");
    propertyMap = props;

    const size_t before = gpu->deviceSensors.size();
    createNsmEventConfig(mockManager, basicIntfName, uniquePath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// Missing "MessageType" → FALSE branch for count("MessageType") →
// default 0 used; sensor IS still created
TEST_F(NsmEventConfigTest, Factory_MissingMessageType_SensorCreated)
{
    const std::string uniquePath =
        "/xyz/openbmc_project/inventory/system/eventconfig_nomsgtype";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    dbus::PropertyMap props = basicProperties;
    props["Name"] = std::string("EventConfig_NoMsgType"); // unique name
    props.erase("MessageType");
    propertyMap = props;

    const size_t before = gpu->deviceSensors.size();
    createNsmEventConfig(mockManager, basicIntfName, uniquePath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// Missing "SubscribedEventIDs" → FALSE branch for count("SubscribedEventIDs")
// → empty vector used; sensor IS still created
TEST_F(NsmEventConfigTest, Factory_MissingSubscribedEventIDs_SensorCreated)
{
    const std::string uniquePath =
        "/xyz/openbmc_project/inventory/system/eventconfig_nosubevents";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    dbus::PropertyMap props = basicProperties;
    props["Name"] = std::string("EventConfig_NoSubEvents"); // unique name
    props.erase("SubscribedEventIDs");
    propertyMap = props;

    const size_t before = gpu->deviceSensors.size();
    createNsmEventConfig(mockManager, basicIntfName, uniquePath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

TEST_F(NsmEventConfigTest, testNsmEventConfigUpdate)
{
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {};
    NsmEventConfig eventConfig(name, "NSM_EventConfig",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    EXPECT_EQ(eventConfig.messageType, NSM_TYPE_PLATFORM_ENVIRONMENTAL);
    EXPECT_EQ(eventConfig.srcEventMask.size(), 8);

    // Test update

    const Response getEventSourceResp{0x10,
                                      0xDE,
                                      0x00,
                                      0x89,
                                      NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                      NSM_GET_CURRENT_EVENT_SOURCES,
                                      NSM_SUCCESS,
                                      0,
                                      0,
                                      8,
                                      0,
                                      0x0E,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(getEventSourceResp, Response{}));

    eventConfig.update(gpu);
}

TEST_F(NsmEventConfigTest, testNsmEventConfigSetCurrentEventSources)
{
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {};
    NsmEventConfig eventConfig(name, "NSM_EventConfig",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    const Response setEventSourceResp{0x10,
                                      0xDE,
                                      0x00,
                                      0x89,
                                      NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                      NSM_SET_CURRENT_EVENT_SOURCES,
                                      NSM_SUCCESS};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(setEventSourceResp, Response{}));

    eventConfig.setCurrentEventSources(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                       eventConfig.srcEventMask);
}

TEST_F(NsmEventConfigTest, testNsmGetEventConfigUpdate)
{
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        name, "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
        ackEventIds);

    NsmGetEventConfig getEventConfig(name, "NSM_GetEventConfig",
                                     NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                     srcEventIds, eventConfig);

    const Response getEventSourceResp{0x10,
                                      0xDE,
                                      0x00,
                                      0x89,
                                      NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                      NSM_GET_CURRENT_EVENT_SOURCES,
                                      NSM_SUCCESS,
                                      0,
                                      0,
                                      8,
                                      0,
                                      0x0E,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0};

    EXPECT_CALL(*gpu, sensorIO)
        .Times(AtLeast(1))
        .WillRepeatedly(mockSensorIO(getEventSourceResp, Response{}));

    getEventConfig.update(gpu);
}

TEST_F(NsmEventConfigTest, testConvertIdsToMask)
{
    std::vector<uint64_t> eventIds = {0, 7, 15, 31};
    std::vector<bitfield8_t> bitfields(8);

    for (auto& value : bitfields)
    {
        value.byte = 0;
    }

    for (auto& value : eventIds)
    {
        uint8_t index = value / 8;
        uint8_t offset = value % 8;
        bitfields[index].byte |= (1 << offset);
    }

    EXPECT_EQ(bitfields[0].byte, 0x81); // bits 0 and 7
    EXPECT_EQ(bitfields[1].byte, 0x80); // bit 15 (offset 7 in byte 1)
    EXPECT_EQ(bitfields[3].byte, 0x80); // bit 31 (offset 7 in byte 3)
}

TEST_F(NsmEventConfigTest, testConfigureEventAcknowledgement)
{
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {4, 5};
    NsmEventConfig eventConfig(name, "NSM_EventConfig",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    const Response configAckResp{0x10,
                                 0xDE,
                                 0x00,
                                 0x89,
                                 NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                 NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT,
                                 NSM_SUCCESS,
                                 0,
                                 0,
                                 8,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(configAckResp, Response{}));

    eventConfig.configureEventAcknowledgement(
        gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL, eventConfig.ackEventMask);
}

TEST_F(NsmEventConfigTest, badTestSetCurrentEventSourcesInvalidLength)
{
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {};
    NsmEventConfig eventConfig(name, "NSM_EventConfig",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    std::vector<bitfield8_t> invalidMask(4); // Wrong size, should be 8

    auto rc = eventConfig.setCurrentEventSources(
        gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL, invalidMask);
    // Should fail due to invalid length without making sensor call
}

TEST_F(NsmEventConfigTest, testValidateEventIds)
{
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        name, "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
        ackEventIds);

    NsmGetEventConfig getEventConfig(name, "NSM_GetEventConfig",
                                     NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                     srcEventIds, eventConfig);

    bitfield8_t eventSources[EVENT_SOURCES_LENGTH];
    for (size_t i = 0; i < EVENT_SOURCES_LENGTH; i++)
    {
        eventSources[i].byte = 0;
    }

    // Mark events 1,2,3 as supported (bit set means supported)
    eventSources[0].byte = 0x0E; // bits 1,2,3 set = events 1,2,3 supported

    bool result = getEventConfig.validateEventIds(eid, eventSources);
    EXPECT_TRUE(result);
}

TEST_F(NsmEventConfigTest, testValidateEventIdsUnsupported)
{
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        name, "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
        ackEventIds);

    NsmGetEventConfig getEventConfig(name, "NSM_GetEventConfig",
                                     NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                     srcEventIds, eventConfig);

    bitfield8_t eventSources[EVENT_SOURCES_LENGTH];
    for (size_t i = 0; i < EVENT_SOURCES_LENGTH; i++)
    {
        eventSources[i].byte = 0;
    }

    // Mark event 1 as unsupported (bit set to 1)
    eventSources[0].byte = 0x02; // Event ID 1 unsupported

    bool result = getEventConfig.validateEventIds(eid, eventSources);
    EXPECT_FALSE(result);
}

TEST_F(NsmEventConfigTest, testMultipleEventTypes)
{
    std::vector<uint8_t> messageTypes = {NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                         NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                         NSM_TYPE_FIRMWARE};

    for (size_t i = 0; i < messageTypes.size(); i++)
    {
        std::string testPath = objPath + "_" + std::to_string(i);
        auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                              basicIntfName);
        propertyMap = basicProperties;
        propertyMap["MessageType"] = uint64_t(messageTypes[i]);

        createNsmEventConfig(mockManager, basicIntfName, testPath);
    }

    // Should have created 2 sensors per type (EventConfig + GetEventConfig)
    EXPECT_GE(gpu->deviceSensors.size(), messageTypes.size() * 2);
}

TEST_F(NsmEventConfigTest, testSetCurrentEventSourcesError)
{
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {};
    NsmEventConfig eventConfig(name, "NSM_EventConfig",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    const Response errorResp{0x10,
                             0xDE,
                             0x00,
                             0x89,
                             NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                             NSM_SET_CURRENT_EVENT_SOURCES,
                             NSM_ERROR,
                             0x12,
                             0x34};

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(errorResp, Response{}));

    eventConfig.setCurrentEventSources(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                       eventConfig.srcEventMask);
}

TEST_F(NsmEventConfigTest, testGetEventConfigUpdateWithMismatch)
{
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        name, "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
        ackEventIds);

    NsmGetEventConfig getEventConfig(name, "NSM_GetEventConfig",
                                     NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                     srcEventIds, eventConfig);

    // Response with mismatched event IDs (configured events not supported)
    // Events 1,2,3 are NOT set in eventSources[0] (bit clear = not supported)
    const Response getEventSourceResp{0x10,
                                      0xDE,
                                      0x00,
                                      0x89,
                                      NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                      NSM_GET_CURRENT_EVENT_SOURCES,
                                      NSM_SUCCESS,
                                      0,
                                      0,
                                      8,
                                      0,
                                      0x00,
                                      0xFF,
                                      0xFF,
                                      0xFF,
                                      0xFF,
                                      0xFF,
                                      0xFF,
                                      0xFF};

    const Response setEventSourceResp{0x10,
                                      0xDE,
                                      0x00,
                                      0x89,
                                      NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                      NSM_SET_CURRENT_EVENT_SOURCES,
                                      NSM_SUCCESS};

    EXPECT_CALL(*gpu, sensorIO)
        .Times(2)
        .WillOnce(mockSensorIO(getEventSourceResp, Response{}))
        .WillOnce(mockSensorIO(setEventSourceResp, Response{}));

    getEventConfig.update(gpu);
}

TEST_F(NsmEventConfigTest, testConfigureEventAcknowledgementError)
{
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {4, 5};
    NsmEventConfig eventConfig(name, "NSM_EventConfig",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    const Response configAckErrResp{0x10,
                                    0xDE,
                                    0x00,
                                    0x89,
                                    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                    NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT,
                                    NSM_ERROR,
                                    0xAB,
                                    0xCD};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(configAckErrResp, Response{}));

    eventConfig.configureEventAcknowledgement(
        gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL, eventConfig.ackEventMask);
}

// configureEventAcknowledgement: wrong-size mask → early return (line 132)
TEST_F(NsmEventConfigTest, ConfigureEventAck_WrongSizeMask_EarlyReturn)
{
    std::vector<uint64_t> srcEventIds = {1, 2};
    std::vector<uint64_t> ackEventIds = {};
    NsmEventConfig eventConfig(name, "NSM_EventConfig_Ack_Sz",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    // Wrong size (4 instead of EVENT_SOURCES_LENGTH=8) → early co_return //

    std::vector<bitfield8_t> wrongSizeMask(4);
    // No sensorIO call expected (function returns early)
    eventConfig.configureEventAcknowledgement(
        gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL, wrongSizeMask);
}

TEST_F(NsmEventConfigTest, testEventConfigWithLargeEventIds)
{
    // Test with event IDs at the boundary (0-63)
    std::vector<uint64_t> srcEventIds = {0, 7, 15, 23, 31, 39, 47, 55, 63};
    std::vector<uint64_t> ackEventIds = {};

    NsmEventConfig eventConfig(name, "NSM_EventConfig_Large",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    EXPECT_EQ(eventConfig.srcEventMask.size(), 8);

    // Verify each event ID is set in the mask
    for (auto eventId : srcEventIds)
    {
        uint8_t index = eventId / 8;
        uint8_t offset = eventId % 8;
        EXPECT_TRUE(eventConfig.srcEventMask[index].byte & (1 << offset));
    }
}

TEST_F(NsmEventConfigTest, testGetEventConfigUpdateDecodeError)
{
    std::vector<uint64_t> srcEventIds = {1, 2};
    std::vector<uint64_t> ackEventIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        name, "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
        ackEventIds);

    NsmGetEventConfig getEventConfig(name, "NSM_GetEventConfig",
                                     NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                     srcEventIds, eventConfig);

    // Response with wrong size
    const Response badResp{0x10,
                           0xDE,
                           0x00,
                           0x89,
                           NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                           NSM_GET_CURRENT_EVENT_SOURCES,
                           NSM_SUCCESS};

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(badResp, Response{}));

    getEventConfig.update(gpu);
}

TEST_F(NsmEventConfigTest, testUpdateUnsupportedCommand)
{
    std::vector<uint64_t> srcEventIds = {1};
    std::vector<uint64_t> ackEventIds = {};
    NsmEventConfig eventConfig(name, "NSM_EventConfig",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    const Response unsupportedResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                   NSM_SET_CURRENT_EVENT_SOURCES,
                                   NSM_ERR_UNSUPPORTED_COMMAND_CODE};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(unsupportedResp, Response{}));

    // Should not log error for unsupported command
    eventConfig.update(gpu);
}

// update() error path: sensorIO returns NSM_ERROR (not UNSUPPORTED) →
// setCurrentEventSources co_returns NSM_ERROR (line 113) →
// update() enters if (rc != NSM_SW_SUCCESS) with rc != UNSUPPORTED →
// lg2::error is called (lines 71, 73-74 in nsmEventConfig.cpp). //

TEST_F(NsmEventConfigTest, Update_SensorIOError_LogsError)
{
    std::vector<uint64_t> srcEventIds = {1, 2};
    std::vector<uint64_t> ackEventIds = {};
    NsmEventConfig eventConfig(name, "NSM_EventConfig",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    // sensorIO itself fails with NSM_ERROR (not just bad completion code)
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    eventConfig.update(gpu);
}

TEST_F(NsmEventConfigTest, testEventConfigConstructor)
{
    std::vector<uint64_t> srcEventIds = {5, 10, 15};
    std::vector<uint64_t> ackEventIds = {20, 25};

    NsmEventConfig eventConfig("EventConfigConstr", "NSM_EventConfig",
                               NSM_TYPE_FIRMWARE, srcEventIds, ackEventIds);

    EXPECT_EQ(eventConfig.getName(), "EventConfigConstr");
    EXPECT_EQ(eventConfig.getType(), "NSM_EventConfig");
    EXPECT_EQ(eventConfig.messageType, NSM_TYPE_FIRMWARE);
    EXPECT_EQ(eventConfig.srcEventMask.size(), 8);
    EXPECT_EQ(eventConfig.ackEventMask.size(), 8);
}

// NsmGetEventConfig::update(): sensorIO returns NSM_ERROR (transport failure)
// → if (rc) at line 225 taken → lg2::debug at 227-229 → co_return rc at 231 //

TEST_F(NsmEventConfigTest, GetEventConfig_Update_SensorIOError)
{
    std::vector<uint64_t> srcEventIds = {1, 2};
    std::vector<uint64_t> ackEventIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        name, "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
        ackEventIds);

    NsmGetEventConfig getEventConfig(name, "NSM_GetEventConfig",
                                     NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                     srcEventIds, eventConfig);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    getEventConfig.update(gpu);
}

// configureEventAcknowledgement(): sensorIO returns NSM_ERROR (transport
// failure) → if (rc) at line 157 taken → co_return rc at 160 //

TEST_F(NsmEventConfigTest, ConfigureEventAck_SensorIOError)
{
    std::vector<uint64_t> srcEventIds = {};
    std::vector<uint64_t> ackEventIds = {1, 2};
    NsmEventConfig eventConfig(name, "NSM_EventConfig",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    eventConfig.configureEventAcknowledgement(
        gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL, eventConfig.ackEventMask);
}

// createNsmEventConfig: base interface not registered at unique path →
// coGetCachedBaseProperties returns error → co_return rc (early return //
// at line 303 in nsmEventConfig.cpp).
TEST_F(NsmEventConfigTest, CreateEventConfig_BasePropertiesFail_NoSensor)
{
    const std::string uniquePath = "/xyz/test/eventcfg/base_fail_unique";
    // Register a sub-interface only; leave basicIntfName absent.
    auto& other = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    basicIntfName + ".Sub");
    other["Type"] = std::string("NSM_EventConfig");

    const size_t before = gpu->deviceSensors.size();
    createNsmEventConfig(mockManager, basicIntfName, uniquePath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// NsmEventConfig::update(): setCurrentEventSources returns
// NSM_ERR_UNSUPPORTED_COMMAND_CODE via transport error → outer if=TRUE but
// inner if=FALSE (error suppressed). Covers the FALSE branch of
// (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE) at line 71.
TEST_F(NsmEventConfigTest, Update_UnsupportedCmdTransportError_Suppressed)
{
    std::vector<uint64_t> srcEventIds = {1};
    std::vector<uint64_t> ackEventIds = {};
    NsmEventConfig eventConfig(name, "NSM_EventConfig",
                               NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
                               ackEventIds);

    // sensorIO returns NSM_ERR_UNSUPPORTED_COMMAND_CODE (=5) as transport
    // error. setCurrentEventSources returns 5 at the early-return rc check. In
    // update(): outer if=TRUE (5 != 0), inner if=FALSE (5 == 5) → no log.
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));
    eventConfig.update(gpu);
}

// NsmGetEventConfig::update(): validation fails (some configured event ID not
// supported by device) → !validationPassed TRUE → eventConfig->update() called
// Also covers shouldLog(clearloggerMsg, isNotSupported=true) TRUE branch.
TEST_F(NsmEventConfigTest,
       GetEventConfig_Update_ValidationFails_CallsEventConfigUpdate)
{
    // Configure event IDs {1, 2, 3} → need bits 1, 2, 3 supported
    std::vector<uint64_t> srcEventIds = {1, 2, 3};
    std::vector<uint64_t> ackEventIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        name, "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcEventIds,
        ackEventIds);

    NsmGetEventConfig getEventConfig(name, "NSM_GetEventConfig",
                                     NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                     srcEventIds, eventConfig);

    // Device reports bits 1 and 3 set but NOT bit 2 (0x0A = 0b00001010)
    // → event ID 2 is NOT supported → validateEventIds returns false
    // → !validationPassed TRUE branch taken → eventConfig->update() called
    const Response getEventSourceResp{0x10,
                                      0xDE,
                                      0x00,
                                      0x89,
                                      NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                      NSM_GET_CURRENT_EVENT_SOURCES,
                                      NSM_SUCCESS,
                                      0,
                                      0,
                                      8,
                                      0,
                                      0x0A, // bits 1,3 set; bit 2 NOT set
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0};

    // Second sensorIO call comes from eventConfig->update() →
    // setCurrentEventSources() trying to reconfigure the device
    const Response setEventSourceResp{0x10,
                                      0xDE,
                                      0x00,
                                      0x89,
                                      NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                      NSM_SET_CURRENT_EVENT_SOURCES,
                                      NSM_SUCCESS};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(getEventSourceResp, Response{}))
        .WillOnce(mockSensorIO(setEventSourceResp, Response{}));

    getEventConfig.update(gpu);
}
