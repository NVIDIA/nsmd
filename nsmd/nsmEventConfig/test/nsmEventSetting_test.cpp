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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "device-capability-discovery.h"

#define private public
#define protected public

#include "nsmEventSetting.hpp"

namespace nsm
{
requester::Coroutine createNsmEventSetting(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

using namespace nsm;

TEST(NsmEventSetting, Constructor)
{
    std::string name = "EventSetting";
    std::string type = "NSM_EventSetting";
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;

    uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "STATIC",
                                                  uuid, 0);

    NsmEventSetting eventSetting(name, type, eventGenSetting, device);

    EXPECT_EQ(eventSetting.getName(), name);
    EXPECT_EQ(eventSetting.getType(), type);
    EXPECT_EQ(eventSetting.eventGenerationSetting, eventGenSetting);
    EXPECT_EQ(eventSetting.nsmDevice, device);
}

TEST(NsmEventSetting, ConstructorDisabled)
{
    std::string name = "EventSettingDisabled";
    std::string type = "NSM_EventSetting";
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_DISABLE;

    uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:2";
    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "STATIC",
                                                  uuid, 0);

    NsmEventSetting eventSetting(name, type, eventGenSetting, device);

    EXPECT_EQ(eventSetting.eventGenerationSetting,
              GLOBAL_EVENT_GENERATION_DISABLE);
}

TEST(NsmGetEventSetting, Constructor)
{
    std::string name = "GetEventSetting";
    std::string type = "NSM_GetEventSetting";
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_PUSH;

    uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:3";
    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "STATIC",
                                                  uuid, 0);

    auto eventSetting =
        std::make_shared<NsmEventSetting>(name, type, eventGenSetting, device);

    NsmGetEventSetting getEventSetting(name + "_Get", type + "_Get",
                                       eventSetting);

    EXPECT_EQ(getEventSetting.getName(), name + "_Get");
    EXPECT_EQ(getEventSetting.getType(), type + "_Get");
    EXPECT_EQ(getEventSetting.eventSetting, eventSetting);
}

struct NsmEventSettingTestFixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_EventSetting";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/test";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:5";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmEventSettingTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        // Simulate MCTP discovery having provided LocalEID (0) for event tests
        gpu->mctpLocalEid = 0;
    }

    ~NsmEventSettingTestFixture()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmEventSettingTestFixture, goodTestCreateEventSetting)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("EventSetting")},
        {"UUID", gpuUuid},
        {"EventGenerationSetting",
         uint64_t(GLOBAL_EVENT_GENERATION_ENABLE_POLLING)},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    createNsmEventSetting(mockManager, basicIntfName, objPath);

    EXPECT_GE(gpu->deviceSensors.size(),
              2); // NsmEventSetting + NsmGetEventSetting (may have more)
}

TEST_F(NsmEventSettingTestFixture, badTestMissingUUID)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("EventSetting")},
        {"EventGenerationSetting",
         uint64_t(GLOBAL_EVENT_GENERATION_ENABLE_POLLING)},
        // Missing UUID - empty uuid will be used
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    EXPECT_THROW_COROUTINE(
        createNsmEventSetting(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmEventSettingTestFixture, badTestInvalidEventGenerationSetting)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("EventSetting")},
        {"UUID", gpuUuid},
        {"EventGenerationSetting", uint64_t(99)}, // Invalid value
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    auto result = createNsmEventSetting(mockManager, basicIntfName, objPath);
    EXPECT_EQ(result.data(), NSM_ERROR);
}

TEST_F(NsmEventSettingTestFixture, testNsmEventSettingUpdate)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    NsmEventSetting eventSetting("EventSetting", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    const Response setEventResp{0x10,
                                0xDE,
                                0x00,
                                0x89,
                                NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                NSM_SET_EVENT_SUBSCRIPTION,
                                NSM_SUCCESS,
                                0,
                                0};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(setEventResp, Response{}));

    eventSetting.update(gpu);
}

TEST_F(NsmEventSettingTestFixture, testNsmGetEventSettingUpdate)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_PUSH;
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EventSetting", "NSM_EventSetting", eventGenSetting, gpu);

    NsmGetEventSetting getEventSetting("GetEventSetting", "NSM_GetEventSetting",
                                       eventSetting);

    // Structure: nsm_msg_hdr (5 bytes) + nsm_common_resp (6 bytes) +
    // receiver_eid (1 byte) = 12 bytes
    // Use receiver_eid = 0 (localEid) to avoid triggering setEventSubscription
    const Response getEventResp{0x10,
                                0xDE,
                                0x00,
                                0x89,
                                NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                NSM_GET_EVENT_SUBSCRIPTION,
                                NSM_SUCCESS,
                                0,
                                0,
                                1,     // data_size low (must be 1)
                                0,
                                0x00}; // receiver_eid = 0 (matches localEid)

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(getEventResp, Response{}));

    getEventSetting.update(gpu);
}

TEST_F(NsmEventSettingTestFixture, testNsmEventSettingUpdateError)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    NsmEventSetting eventSetting("EventSettingErr", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    const Response setEventErrResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                   NSM_SET_EVENT_SUBSCRIPTION,
                                   NSM_ERROR,
                                   0x11,
                                   0x22,
                                   0,
                                   0};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(setEventErrResp, Response{}));

    eventSetting.update(gpu);
}

TEST_F(NsmEventSettingTestFixture, testNsmEventSettingUnsupportedCommand)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_PUSH;
    NsmEventSetting eventSetting("EventSettingUnsup", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    const Response unsupportedResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                   NSM_SET_EVENT_SUBSCRIPTION,
                                   NSM_ERR_UNSUPPORTED_COMMAND_CODE,
                                   0,
                                   0,
                                   0,
                                   0};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(unsupportedResp, Response{}));

    // Should not log error for unsupported command
    eventSetting.update(gpu);
}

TEST_F(NsmEventSettingTestFixture, testNsmGetEventSettingUpdateMismatch)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EventSetting", "NSM_EventSetting", eventGenSetting, gpu);

    NsmGetEventSetting getEventSetting("GetEventSettingMismatch",
                                       "NSM_GetEventSetting", eventSetting);

    // Mock response with different receiver_eid (mismatch)
    // Structure: nsm_msg_hdr (5 bytes) + nsm_common_resp (6 bytes) +
    // receiver_eid (1 byte) = 12 bytes
    const Response getEventRespMismatch{
        0x10,                                 // pci_vendor_id low
        0xDE,                                 // pci_vendor_id high
        0x00,                                 // instance_id + flags
        0x89,                                 // ocp_ver=9, ocp_type=8
        NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // nvidia_msg_type
        NSM_GET_EVENT_SUBSCRIPTION,           // command
        NSM_SUCCESS,                          // completion_code
        0,                                    // reserved low
        0,                                    // reserved high
        1,                                    // data_size low (must be 1)
        0,                                    // data_size high
        0x99};                                // receiver_eid = 0x99 (not local)

    const Response setEventResp{0x10,
                                0xDE,
                                0x00,
                                0x89,
                                NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                NSM_SET_EVENT_SUBSCRIPTION,
                                NSM_SUCCESS,
                                0,  // reserved byte 1
                                0,  // reserved byte 2
                                0,  // data_size low
                                0}; // data_size high

    EXPECT_CALL(*gpu, sensorIO)
        .Times(2)
        .WillOnce(mockSensorIO(getEventRespMismatch, Response{}))
        .WillOnce(mockSensorIO(setEventResp, Response{}));

    getEventSetting.update(gpu);
}

TEST_F(NsmEventSettingTestFixture, testSetEventSubscriptionDecodeError)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_DISABLE;
    NsmEventSetting eventSetting("EventSettingDecErr", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    // Mock response with wrong size
    const Response badResp{0x10};

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(badResp, Response{}));

    eventSetting.setEventSubscription(gpu);
}

TEST_F(NsmEventSettingTestFixture, testGetEventSettingDecodeError)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_PUSH;
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EventSetting", "NSM_EventSetting", eventGenSetting, gpu);

    NsmGetEventSetting getEventSetting("GetEventSettingDecErr",
                                       "NSM_GetEventSetting", eventSetting);

    // Mock response with incomplete data (header only, no payload)
    const Response badResp(sizeof(nsm_msg_hdr), 0x10);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(badResp, Response{}));

    getEventSetting.update(gpu);
}

TEST_F(NsmEventSettingTestFixture, testAllEventGenerationSettings)
{
    std::vector<uint8_t> settings = {GLOBAL_EVENT_GENERATION_DISABLE,
                                     GLOBAL_EVENT_GENERATION_ENABLE_POLLING,
                                     GLOBAL_EVENT_GENERATION_ENABLE_PUSH};

    for (size_t i = 0; i < settings.size(); i++)
    {
        std::string testPath = objPath + "_setting_" + std::to_string(i);

        dbus::PropertyMap properties = {
            {"Name", std::string("EventSetting_") + std::to_string(i)},
            {"UUID", gpuUuid},
            {"EventGenerationSetting", uint64_t(settings[i])},
        };

        auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                              basicIntfName);
        propertyMap = properties;

        createNsmEventSetting(mockManager, basicIntfName, testPath);
    }

    // Should have created 2 sensors per setting (EventSetting +
    // GetEventSetting)
    EXPECT_GE(gpu->deviceSensors.size(), settings.size() * 2);
}

TEST_F(NsmEventSettingTestFixture, testEventSettingSetsDeviceEventMode)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_PUSH;
    NsmEventSetting eventSetting("EventSettingMode", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    const Response setEventResp{0x10,
                                0xDE,
                                0x00,
                                0x89,
                                NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                NSM_SET_EVENT_SUBSCRIPTION,
                                NSM_SUCCESS,
                                0,
                                0,
                                0,
                                0};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(setEventResp, Response{}));

    eventSetting.update(gpu);
}

// ---- Event subscription status cache for logDump ----

TEST_F(NsmEventSettingTestFixture, testEventSubscriptionStatusLogOkWithLocalEid)
{
    gpu->mctpLocalEid = 30; // Use specific localEid for OK message
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    NsmEventSetting eventSetting("EventSetting", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    const Response setEventResp{0x10,
                                0xDE,
                                0x00,
                                0x89,
                                NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                NSM_SET_EVENT_SUBSCRIPTION,
                                NSM_SUCCESS,
                                0,
                                0,
                                0,
                                0};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(setEventResp, Response{}));

    eventSetting.update(gpu);

    auto status = gpu->getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "OK (localEid=30)");
}

TEST_F(NsmEventSettingTestFixture,
       testEventSubscriptionStatusLogSkippedLocalEidNotSet)
{
    gpu->mctpLocalEid = std::nullopt; // Simulate LocalEID not from MCTP
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    NsmEventSetting eventSetting("EventSetting", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    eventSetting.setEventSubscription(gpu);

    auto status = gpu->getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "skipped: localEid not set (LocalEID not from MCTP)");
}

TEST_F(NsmEventSettingTestFixture,
       testEventSubscriptionStatusLogSkippedLocalEidNotSetGetPath)
{
    gpu->mctpLocalEid = std::nullopt;
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EventSetting", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);
    NsmGetEventSetting getEventSetting("GetEventSetting", "NSM_GetEventSetting",
                                       eventSetting);

    // sensorIO must succeed to reach localEid check (which then fails)
    const Response getEventResp{0x10,
                                0xDE,
                                0x00,
                                0x89,
                                NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                NSM_GET_EVENT_SUBSCRIPTION,
                                NSM_SUCCESS,
                                0,
                                0,
                                1,
                                0,
                                0};
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(getEventResp, Response{}));

    getEventSetting.update(gpu);

    auto status = gpu->getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "skipped: localEid not set (get path)");
}

TEST_F(NsmEventSettingTestFixture, testEventSubscriptionStatusLogFailedSensorIO)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    NsmEventSetting eventSetting("EventSetting", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(Response{}, static_cast<nsm_completion_codes>(
                                               NSM_SW_ERROR_TIMEOUT)));

    eventSetting.setEventSubscription(gpu);

    auto status = gpu->getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status,
              "failed: sensorIO rc=" + std::to_string(NSM_SW_ERROR_TIMEOUT));
}

TEST_F(NsmEventSettingTestFixture, testEventSubscriptionStatusLogFailedDecode)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_DISABLE;
    NsmEventSetting eventSetting("EventSetting", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    const Response badResp{0x10}; // Too short for valid decode
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(badResp, Response{}));

    eventSetting.setEventSubscription(gpu);

    auto status = gpu->getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_TRUE(status->find("failed: decode rc=") == 0);
}

TEST_F(NsmEventSettingTestFixture, testEventSubscriptionStatusLogFailedDeviceCc)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    NsmEventSetting eventSetting("EventSetting", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    const Response setEventErrResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                   NSM_SET_EVENT_SUBSCRIPTION,
                                   NSM_ERROR,
                                   0x11,
                                   0x22,
                                   0,
                                   0};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(setEventErrResp, Response{}));

    eventSetting.setEventSubscription(gpu);

    auto status = gpu->getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "failed: device cc=" + std::to_string(NSM_ERROR));
}

TEST_F(NsmEventSettingTestFixture,
       testEventSubscriptionStatusLogFailedUnsupportedCommand)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_PUSH;
    NsmEventSetting eventSetting("EventSetting", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    const Response unsupportedResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                   NSM_SET_EVENT_SUBSCRIPTION,
                                   NSM_ERR_UNSUPPORTED_COMMAND_CODE,
                                   0,
                                   0,
                                   0,
                                   0};

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(unsupportedResp, Response{}));

    eventSetting.setEventSubscription(gpu);

    auto status = gpu->getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "failed: unsupported command");
}

TEST_F(NsmEventSettingTestFixture,
       testEventSubscriptionStatusLogGetSuccessWithLocalEid)
{
    gpu->mctpLocalEid = 25;
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EventSetting", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);
    NsmGetEventSetting getEventSetting("GetEventSetting", "NSM_GetEventSetting",
                                       eventSetting);

    const Response getEventResp{0x10,
                                0xDE,
                                0x00,
                                0x89,
                                NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                NSM_GET_EVENT_SUBSCRIPTION,
                                NSM_SUCCESS,
                                0,
                                0,
                                1,
                                0,
                                0x19}; // receiver_eid = 25 (matches localEid)

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(getEventResp, Response{}));

    getEventSetting.update(gpu);

    auto status = gpu->getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "OK (localEid=25)");
}

TEST_F(NsmEventSettingTestFixture,
       testEventSubscriptionStatusLogFailedGetDecode)
{
    gpu->mctpLocalEid = 0;
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EventSetting", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);
    NsmGetEventSetting getEventSetting("GetEventSetting", "NSM_GetEventSetting",
                                       eventSetting);

    const Response badResp(sizeof(nsm_msg_hdr), 0x10); // Incomplete payload
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(badResp, Response{}));

    getEventSetting.update(gpu);

    auto status = gpu->getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_TRUE(status->find("failed: get decode rc=") == 0);
}

TEST_F(NsmEventSettingTestFixture,
       testEventSubscriptionStatusLogFailedGetSensorIO)
{
    gpu->mctpLocalEid = 0;
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EventSetting", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);
    NsmGetEventSetting getEventSetting("GetEventSetting", "NSM_GetEventSetting",
                                       eventSetting);

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(Response{}, static_cast<nsm_completion_codes>(
                                               NSM_SW_ERROR_TIMEOUT)));

    getEventSetting.update(gpu);

    auto status = gpu->getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "failed: get sensorIO rc=" +
                           std::to_string(NSM_SW_ERROR_TIMEOUT));
}
