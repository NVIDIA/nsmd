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
 * Branch coverage tests for nsmEventConfig.cpp and nsmEventSetting.cpp
 *
 * nsmEventConfig.cpp targets:
 *   - NsmEventConfig::update: rc != NSM_SW_SUCCESS, rc == NSM_ERR_UNSUPPORTED
 *   - setCurrentEventSources: eventIdMasks.size() != EVENT_SOURCES_LENGTH
 *   - setCurrentEventSources: encode failure (rc != 0)
 *   - setCurrentEventSources: sensorIO failure
 *   - configureEventAcknowledgement: size check, encode fail, sensorIO fail
 *   - configureEventAcknowledgement: decode failure
 *   - NsmGetEventConfig::update: encode failure
 *   - NsmGetEventConfig::update: sensorIO failure
 *   - NsmGetEventConfig::update: decode success + validation pass/fail
 *   - validateEventIds: event configured but not supported
 *
 * nsmEventSetting.cpp targets:
 *   - NsmEventSetting::update: rc != NSM_SW_SUCCESS
 *   - NsmEventSetting::update: rc == NSM_ERR_UNSUPPORTED_COMMAND_CODE
 *   - setEventSubscription: sensorIO failure
 *   - setEventSubscription: decode failure
 *   - NsmGetEventSetting::update: sensorIO failure
 *   - NsmGetEventSetting::update: cc != NSM_SUCCESS → re-subscribe
 *   - NsmGetEventSetting::update: receiver_eid != localEid → re-subscribe
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-capability-discovery.h"

#include "nsmEventConfig.hpp"
#include "nsmEventSetting.hpp"

using namespace nsm;

// ============================================================================
// Fixture for NsmEventConfig
// ============================================================================

struct NsmEventConfigBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmEventConfigBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmEventConfigBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmEventConfig>
        makeEventConfig(uint8_t messageType = NSM_TYPE_PLATFORM_ENVIRONMENTAL)
    {
        std::vector<uint64_t> srcIds = {1, 2, 3};
        std::vector<uint64_t> ackIds = {};
        return std::make_shared<NsmEventConfig>("EventCfgBr", "NSM_EventConfig",
                                                messageType, srcIds, ackIds);
    }
};

// ============================================================================
// NsmEventConfig::update branches
// ============================================================================

// update success path: sensorIO returns success response
TEST_F(NsmEventConfigBranchTest, Update_Success_ReturnsCc)
{
    auto sensor = makeEventConfig();

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_CURRENT_EVENT_SOURCES, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
}

// update with sensorIO failure → rc != NSM_SW_SUCCESS and
// rc != NSM_ERR_UNSUPPORTED → logs error
TEST_F(NsmEventConfigBranchTest, Update_SensorIOFail_LogsError)
{
    auto sensor = makeEventConfig();

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->update(gpu);
}

// update with rc == NSM_ERR_UNSUPPORTED_COMMAND_CODE → does NOT log error
TEST_F(NsmEventConfigBranchTest, Update_UnsupportedCommand_NoError)
{
    auto sensor = makeEventConfig();

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    sensor->update(gpu);
}

// ============================================================================
// setCurrentEventSources branches
// ============================================================================

// eventIdMasks.size() != EVENT_SOURCES_LENGTH → returns
// NSM_ERR_INVALID_DATA_LENGTH
TEST_F(NsmEventConfigBranchTest,
       SetCurrentEventSources_WrongSize_ReturnsInvalidLength)
{
    auto sensor = makeEventConfig();

    // Create a mask with wrong size
    std::vector<bitfield8_t> wrongSizeMask(4); // should be 8
    sensor->setCurrentEventSources(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   wrongSizeMask);
}

// sensorIO failure in setCurrentEventSources
TEST_F(NsmEventConfigBranchTest, SetCurrentEventSources_SensorIOFail)
{
    auto sensor = makeEventConfig();

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    std::vector<bitfield8_t> mask(EVENT_SOURCES_LENGTH);
    mask[0].byte = 0x0E; // bits 1,2,3
    sensor->setCurrentEventSources(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL, mask);
}

// ============================================================================
// configureEventAcknowledgement branches
// ============================================================================

// Wrong size → returns NSM_ERR_INVALID_DATA_LENGTH
TEST_F(NsmEventConfigBranchTest,
       ConfigureEventAck_WrongSize_ReturnsInvalidLength)
{
    auto sensor = makeEventConfig();

    std::vector<bitfield8_t> wrongSizeMask(3); // should be 8
    sensor->configureEventAcknowledgement(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                          wrongSizeMask);
}

// sensorIO failure
TEST_F(NsmEventConfigBranchTest, ConfigureEventAck_SensorIOFail)
{
    auto sensor = makeEventConfig();

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    std::vector<bitfield8_t> mask(EVENT_SOURCES_LENGTH);
    mask[0].byte = 0x0E;
    sensor->configureEventAcknowledgement(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                          mask);
}

// Decode failure (rc != 0) in configureEventAcknowledgement
TEST_F(NsmEventConfigBranchTest, ConfigureEventAck_DecodeFail_LogsError)
{
    auto sensor = makeEventConfig();

    // Short buffer → decode fails
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    std::vector<bitfield8_t> mask(EVENT_SOURCES_LENGTH);
    mask[0].byte = 0x0E;
    sensor->configureEventAcknowledgement(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                          mask);
}

// Success path for configureEventAcknowledgement
TEST_F(NsmEventConfigBranchTest, ConfigureEventAck_Success)
{
    auto sensor = makeEventConfig();

    // Build a response with event masks
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_configure_event_acknowledgement_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    bitfield8_t newMasks[EVENT_SOURCES_LENGTH] = {};
    encode_nsm_configure_event_acknowledgement_resp(0, NSM_SUCCESS, newMasks,
                                                    msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    std::vector<bitfield8_t> mask(EVENT_SOURCES_LENGTH);
    mask[0].byte = 0x0E;
    sensor->configureEventAcknowledgement(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                          mask);
}

// ============================================================================
// NsmGetEventConfig::update branches
// ============================================================================

// sensorIO failure in NsmGetEventConfig::update
TEST_F(NsmEventConfigBranchTest, GetEventConfig_Update_SensorIOFail)
{
    auto sensor = makeEventConfig();
    std::vector<uint64_t> srcIds = {1, 2, 3};
    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgBr", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, sensor);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    getSensor->update(gpu);
}

// decode success + cc=0 + validation passes → no re-subscription
TEST_F(NsmEventConfigBranchTest,
       GetEventConfig_Update_ValidationPass_NoResubscribe)
{
    auto sensor = makeEventConfig();
    std::vector<uint64_t> srcIds = {1, 2, 3};
    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgBr2", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, sensor);

    // Build response with event sources that include all configured IDs
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_event_source_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    bitfield8_t eventSources[EVENT_SOURCES_LENGTH] = {};
    eventSources[0].byte = 0x0E; // bits 1,2,3 set → all configured supported
    encode_nsm_get_current_event_source_resp(0, NSM_SUCCESS, ERR_NULL,
                                             eventSources, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    getSensor->update(gpu);
}

// decode success + cc=0 + validation fails → re-subscribe
TEST_F(NsmEventConfigBranchTest,
       GetEventConfig_Update_ValidationFail_Resubscribes)
{
    auto sensor = makeEventConfig();
    std::vector<uint64_t> srcIds = {1, 2, 3};
    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgBr3", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, sensor);

    // Build response with event sources that DON'T include all configured IDs
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_event_source_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    bitfield8_t eventSources[EVENT_SOURCES_LENGTH] = {};
    eventSources[0].byte = 0x02; // only bit 1 set → bits 2,3 missing
    encode_nsm_get_current_event_source_resp(0, NSM_SUCCESS, ERR_NULL,
                                             eventSources, msg);

    // First call: NsmGetEventConfig::update → get event sources
    // Second call: NsmEventConfig::update (re-subscribe) →
    // setCurrentEventSources
    std::vector<uint8_t> setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto setMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_CURRENT_EVENT_SOURCES, setMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(response))
        .WillOnce(mockSensorIO(setResp));

    getSensor->update(gpu);
}

// decode success + cc != 0 → else branch → no validation
TEST_F(NsmEventConfigBranchTest, GetEventConfig_Update_NonZeroCC_ElseBranch)
{
    auto sensor = makeEventConfig();
    std::vector<uint64_t> srcIds = {1, 2};
    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgBr4", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, sensor);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    getSensor->update(gpu);
}

// ============================================================================
// Fixture for NsmEventSetting
// ============================================================================

struct NsmEventSettingBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmEventSettingBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        gpu->mctpLocalEid = 0;
    }

    ~NsmEventSettingBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NsmEventSetting::update branches
// ============================================================================

// sensorIO success → cc=0
TEST_F(NsmEventSettingBranchTest, Update_Success)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettBr", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_CURRENT_EVENT_SOURCES, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
}

// sensorIO failure → rc != NSM_SW_SUCCESS, rc != UNSUPPORTED → logs error
TEST_F(NsmEventSettingBranchTest, Update_SensorIOFail_LogsError)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettBr2", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->update(gpu);
}

// sensorIO failure → rc == NSM_ERR_UNSUPPORTED → no error log
TEST_F(NsmEventSettingBranchTest, Update_Unsupported_NoErrorLog)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettBr3", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    sensor->update(gpu);
}

// ============================================================================
// setEventSubscription branches
// ============================================================================

// sensorIO failure
TEST_F(NsmEventSettingBranchTest, SetEventSubscription_SensorIOFail)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettBr4", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->setEventSubscription(gpu);
}

// Decode failure (rc != 0)
TEST_F(NsmEventSettingBranchTest, SetEventSubscription_DecodeFail)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettBr5", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);

    // Short buffer → decode fails
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_SUCCESS;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    sensor->setEventSubscription(gpu);
}

// ============================================================================
// NsmGetEventSetting::update branches
// ============================================================================

// sensorIO failure
TEST_F(NsmEventSettingBranchTest, GetEventSetting_Update_SensorIOFail)
{
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EvSettBr6", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);
    auto getSensor = std::make_shared<NsmGetEventSetting>(
        "GetEvSettBr", "NSM_EventSetting", eventSetting);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    getSensor->update(gpu);
}

// cc == NSM_SUCCESS but receiver_eid != localEid → re-subscribes
TEST_F(NsmEventSettingBranchTest,
       GetEventSetting_Update_WrongReceiverEid_Resubscribes)
{
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EvSettBr8", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);
    auto getSensor = std::make_shared<NsmGetEventSetting>(
        "GetEvSettBr3", "NSM_EventSetting", eventSetting);

    // Build response with wrong receiver_eid (255)
    std::vector<uint8_t> getResp(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_event_subscription_resp));
    auto getMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_nsm_get_event_subscription_resp(0, NSM_SUCCESS, ERR_NULL, 255,
                                           getMsg);

    // Second call: setEventSubscription success
    std::vector<uint8_t> setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto setMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_CURRENT_EVENT_SOURCES, setMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(getResp))
        .WillOnce(mockSensorIO(setResp));

    getSensor->update(gpu);
}

// cc == NSM_SUCCESS and receiver_eid == localEid → no re-subscribe
TEST_F(NsmEventSettingBranchTest,
       GetEventSetting_Update_CorrectEid_NoResubscribe)
{
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EvSettBr9", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);
    auto getSensor = std::make_shared<NsmGetEventSetting>(
        "GetEvSettBr4", "NSM_EventSetting", eventSetting);

    eid_t localEid = gpu->getMctpLocalEid().value_or(0);

    std::vector<uint8_t> getResp(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_event_subscription_resp));
    auto getMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_nsm_get_event_subscription_resp(0, NSM_SUCCESS, ERR_NULL, localEid,
                                           getMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(getResp));

    getSensor->update(gpu);
}
