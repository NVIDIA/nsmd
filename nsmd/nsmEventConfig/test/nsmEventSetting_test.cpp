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

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmEventSetting.hpp"

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

    requester::Coroutine createEventSetting(const dbus::PropertyMap& params,
                                            bool withDevice = true)
    {
        features::DeviceContext dev;
        dev.name = "GPU_SXM_5";
        dev.kind = "gpu";
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = gpu;
        }
        return features::createEventSettingFeature(mockManager, dev, params);
    }
};

TEST_F(NsmEventSettingTestFixture, goodTestCreateEventSetting)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("EventSetting")},
        {"EventGenerationSetting",
         uint64_t(GLOBAL_EVENT_GENERATION_ENABLE_POLLING)},
    };

    const size_t before = gpu->deviceSensors.size();
    const size_t refreshBefore = gpu->capabilityRefreshSensors.size();
    auto rc = createEventSetting(properties);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    // NsmEventSetting + NsmGetEventSetting
    EXPECT_EQ(gpu->deviceSensors.size(), before + 2);
    EXPECT_EQ(gpu->capabilityRefreshSensors.size(), refreshBefore + 1);
    EXPECT_TRUE(gpu->hasNsmEventSettingConfig());
}

TEST_F(NsmEventSettingTestFixture, badTestNoDevice)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("EventSetting")},
        {"EventGenerationSetting",
         uint64_t(GLOBAL_EVENT_GENERATION_ENABLE_POLLING)},
    };

    const size_t before = gpu->deviceSensors.size();
    auto rc = createEventSetting(properties, false);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

TEST_F(NsmEventSettingTestFixture, badTestInvalidEventGenerationSetting)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("EventSetting")},
        {"EventGenerationSetting", uint64_t(99)}, // Invalid value
    };

    const size_t before = gpu->deviceSensors.size();
    auto result = createEventSetting(properties);
    EXPECT_EQ(result.data(), NSM_ERROR);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// "Name" is mandatory for the feature creator: params.at("Name") throws
TEST_F(NsmEventSettingTestFixture, Factory_MissingName_Throws)
{
    dbus::PropertyMap properties = {
        {"EventGenerationSetting",
         uint64_t(GLOBAL_EVENT_GENERATION_ENABLE_POLLING)},
    };

    const size_t before = gpu->deviceSensors.size();
    EXPECT_THROW_COROUTINE(createEventSetting(properties), std::out_of_range);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// Missing "EventGenerationSetting" → default 0
// (GLOBAL_EVENT_GENERATION_DISABLE) is valid → sensor IS created
TEST_F(NsmEventSettingTestFixture, Factory_MissingEventGenSetting_SensorCreated)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("EventSetting_NoGen")},
    };

    const size_t before = gpu->deviceSensors.size();
    auto rc = createEventSetting(properties);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), before);
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
        dbus::PropertyMap properties = {
            {"Name", std::string("EventSetting_") + std::to_string(i)},
            {"EventGenerationSetting", uint64_t(settings[i])},
        };

        auto rc = createEventSetting(properties);
        EXPECT_EQ(rc.data(), NSM_SUCCESS);
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

// Cover sensorIO transport error path in setEventSubscription (lines 81-84)
TEST_F(NsmEventSettingTestFixture, testSetEventSubscriptionSensorIOError)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    NsmEventSetting eventSetting("EventSettingIOErr", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    // sensorIO returns non-zero (transport error) → co_return rc at line 84 //

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));

    eventSetting.setEventSubscription(gpu);
}

// Cover sensorIO transport error path in NsmGetEventSetting::update (lines
// 124-130)
TEST_F(NsmEventSettingTestFixture, testGetEventSettingSensorIOError)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EventSetting", "NSM_EventSetting", eventGenSetting, gpu);

    NsmGetEventSetting getEventSetting("GetEventSettingIOErr2",
                                       "NSM_GetEventSetting", eventSetting);

    // sensorIO returns non-zero → co_return rc at line 130
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));

    getEventSetting.update(gpu);
}

// Cover setEventSubscription failure in NsmGetEventSetting::update (lines
// 148-151)
TEST_F(NsmEventSettingTestFixture, testGetEventSettingSetSubscriptionFails)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EventSetting", "NSM_EventSetting", eventGenSetting, gpu);

    NsmGetEventSetting getEventSetting("GetEventSettingSubFail",
                                       "NSM_GetEventSetting", eventSetting);

    // First call: get subscription returns mismatched receiver_eid
    const Response getEventRespMismatch{
        0x10,
        0xDE,
        0x00,
        0x89,
        NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
        NSM_GET_EVENT_SUBSCRIPTION,
        NSM_SUCCESS,
        0,
        0,
        1,
        0,     // data_size = 1
        0x99}; // receiver_eid = 0x99 (not local EID)

    // Second call: setEventSubscription sensorIO returns transport error
    EXPECT_CALL(*gpu, sensorIO)
        .Times(2)
        .WillOnce(mockSensorIO(getEventRespMismatch, Response{}))
        .WillOnce(mockSensorIO(NSM_ERROR));

    getEventSetting.update(gpu);
}

// NsmGetEventSetting::update: decode_reason_code_and_cc returns NSM_SW_SUCCESS
// with cc=NSM_ERROR (non-success response exactly 9 bytes).
// Covers the (cc != NSM_SUCCESS && rc == NSM_SW_SUCCESS) branch at line 141.
TEST_F(NsmEventSettingTestFixture,
       testGetEventSettingUpdateCCErrorTriggersSetSubscription)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_PUSH;
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EventSetting", "NSM_EventSetting", eventGenSetting, gpu);

    NsmGetEventSetting getEventSetting("GetEventSettingCCErr",
                                       "NSM_GetEventSetting", eventSetting);

    // 9-byte non-success response:
    // decode_reason_code_and_cc reads cc=NSM_ERROR, checks msg_len==9 (exact),
    // returns NSM_SW_SUCCESS.  decode_nsm_get_event_subscription_resp then
    // returns NSM_SW_SUCCESS early (cc != NSM_SUCCESS branch).
    // → (cc != NSM_SUCCESS && rc == NSM_SW_SUCCESS) fires.
    const Response getErrCCResp{
        0x10,                                 // pci_vendor_id low
        0xDE,                                 // pci_vendor_id high
        0x00,                                 // instance_id + flags
        0x89,                                 // ocp_ver + ocp_type
        NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // nvidia_msg_type
        NSM_GET_EVENT_SUBSCRIPTION,           // command
        NSM_ERROR,                            // completion_code (cc != SUCCESS)
        0x00,                                 // reason_code low
        0x00};                                // reason_code high

    // Second sensorIO call: setEventSubscription with success response.
    const Response setEventSuccResp{0x10,
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
        .Times(2)
        .WillOnce(mockSensorIO(getErrCCResp, Response{}))
        .WillOnce(mockSensorIO(setEventSuccResp, Response{}));

    getEventSetting.update(gpu);
}

// NsmEventSetting::update: setEventSubscription returns
// NSM_ERR_UNSUPPORTED_COMMAND_CODE via transport error → outer if=TRUE but
// inner if=FALSE (error suppressed).  Covers the FALSE branch of
// (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE) at line 47.
TEST_F(NsmEventSettingTestFixture,
       testEventSettingUpdateUnsupportedCmdSuppressed)
{
    uint8_t eventGenSetting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING;
    NsmEventSetting eventSetting("EventSettingUnsupCtl", "NSM_EventSetting",
                                 eventGenSetting, gpu);

    // sensorIO returns NSM_ERR_UNSUPPORTED_COMMAND_CODE (=5) as transport
    // error. setEventSubscription sees rc=5 at the early-return check and
    // co_returns 5. In update(): outer if=TRUE (5 != 0), inner if=FALSE (5 ==
    // 5) → no log.
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    eventSetting.update(gpu);
}
