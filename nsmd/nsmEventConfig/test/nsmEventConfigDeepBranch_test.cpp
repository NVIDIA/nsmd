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
 * Deep branch coverage tests for nsmEventConfig.cpp and nsmEventSetting.cpp.
 *
 * Targets remaining uncovered lines from the coverage report:
 *
 * nsmEventConfig.cpp:
 *   - Lines 36,40-41,44-46: NsmEventConfig constructor body
 *   - Lines 99,101,103: encode failure in setCurrentEventSources
 *   - Lines 146,148,150: encode failure in configureEventAcknowledgement
 *   - Lines 178,182-183,186-187: NsmGetEventConfig constructor body
 *   - Lines 215,217,219: encode failure in NsmGetEventConfig::update
 *
 * nsmEventSetting.cpp:
 *   - Lines 32,36-38: NsmEventSetting constructor body
 *   - Lines 70,72,74: encode failure in setEventSubscription
 *   - Lines 100,103-104: NsmGetEventSetting constructor body
 *   - Lines 114,116,118: encode failure in NsmGetEventSetting::update
 *
 * Strategy:
 *   - Exercise constructors through diverse parameter paths
 *   - Call coroutines with decode success/failure combinations to toggle
 *     LG2_ERROR_FLT shouldLog branches (first-time log vs. repeated flood)
 *   - Cover cc != 0 paths in decode responses
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
// Fixture
// ============================================================================

struct NsmEventConfigDeepBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmEventConfigDeepBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        gpu->mctpLocalEid = 0;
    }

    ~NsmEventConfigDeepBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NsmEventConfig constructor: varied parameters exercise initializer list
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest, Constructor_EmptySrcAndAckIds)
{
    std::vector<uint64_t> srcIds = {};
    std::vector<uint64_t> ackIds = {};
    auto sensor = std::make_shared<NsmEventConfig>(
        "EvCfgEmpty", "NSM_EventConfig", 0, srcIds, ackIds);

    EXPECT_EQ(sensor->getName(), "EvCfgEmpty");
    EXPECT_EQ(sensor->getType(), "NSM_EventConfig");
    EXPECT_EQ(sensor->messageType, 0);
    EXPECT_EQ(sensor->srcEventMask.size(), 8u);
    EXPECT_EQ(sensor->ackEventMask.size(), 8u);
    // All mask bytes should be zero
    for (size_t i = 0; i < 8; i++)
    {
        EXPECT_EQ(sensor->srcEventMask[i].byte, 0);
        EXPECT_EQ(sensor->ackEventMask[i].byte, 0);
    }
}

TEST_F(NsmEventConfigDeepBranchTest, Constructor_BothSrcAndAckIds)
{
    std::vector<uint64_t> srcIds = {0, 8, 16, 24, 32, 40, 48, 56};
    std::vector<uint64_t> ackIds = {7, 15, 23, 31, 39, 47, 55, 63};
    auto sensor = std::make_shared<NsmEventConfig>(
        "EvCfgBoth", "NSM_EventConfig", NSM_TYPE_FIRMWARE, srcIds, ackIds);

    EXPECT_EQ(sensor->messageType, NSM_TYPE_FIRMWARE);
    // Verify srcEventMask: bit 0 in each byte
    for (size_t i = 0; i < 8; i++)
    {
        EXPECT_TRUE(sensor->srcEventMask[i].byte & 0x01);
    }
    // Verify ackEventMask: bit 7 in each byte
    for (size_t i = 0; i < 8; i++)
    {
        EXPECT_TRUE(sensor->ackEventMask[i].byte & 0x80);
    }
}

// ============================================================================
// NsmGetEventConfig constructor: varied parameters
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest, GetEventConfig_Constructor_EmptyIds)
{
    std::vector<uint64_t> srcIds = {};
    std::vector<uint64_t> ackIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        "EvCfg", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcIds,
        ackIds);

    std::vector<uint64_t> getSrcIds = {};
    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgE", "NSM_GetEventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        getSrcIds, eventConfig);

    EXPECT_EQ(getSensor->getName(), "GetEvCfgE");
    EXPECT_EQ(getSensor->getType(), "NSM_GetEventConfig");
    EXPECT_EQ(getSensor->messageType, NSM_TYPE_PLATFORM_ENVIRONMENTAL);
    EXPECT_EQ(getSensor->srcEventMask.size(), 8u);
}

TEST_F(NsmEventConfigDeepBranchTest, GetEventConfig_Constructor_AllBitsSet)
{
    std::vector<uint64_t> srcIds;
    for (uint64_t i = 0; i < 64; i++)
    {
        srcIds.push_back(i);
    }
    std::vector<uint64_t> ackIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        "EvCfgAll", "NSM_EventConfig", NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
        srcIds, ackIds);

    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgAll", "NSM_GetEventConfig",
        NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, srcIds, eventConfig);

    EXPECT_EQ(getSensor->messageType, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY);
    // All bits should be set
    for (size_t i = 0; i < 8; i++)
    {
        EXPECT_EQ(getSensor->srcEventMask[i].byte, 0xFF);
    }
}

// ============================================================================
// NsmEventSetting constructor: varied parameters
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest, EventSetting_Constructor_DisableSetting)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettDis", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_DISABLE, gpu);

    EXPECT_EQ(sensor->getName(), "EvSettDis");
    EXPECT_EQ(sensor->getType(), "NSM_EventSetting");
    EXPECT_EQ(sensor->eventGenerationSetting, GLOBAL_EVENT_GENERATION_DISABLE);
    EXPECT_EQ(sensor->nsmDevice, gpu);
}

TEST_F(NsmEventConfigDeepBranchTest, EventSetting_Constructor_PollingSetting)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettPoll", "NSM_EventSetting",
        GLOBAL_EVENT_GENERATION_ENABLE_POLLING, gpu);

    EXPECT_EQ(sensor->eventGenerationSetting,
              GLOBAL_EVENT_GENERATION_ENABLE_POLLING);
}

TEST_F(NsmEventConfigDeepBranchTest, EventSetting_Constructor_PushSetting)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettPush", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);

    EXPECT_EQ(sensor->eventGenerationSetting,
              GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
}

// ============================================================================
// NsmGetEventSetting constructor
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest, GetEventSetting_Constructor)
{
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EvSett", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH, gpu);
    auto getSensor = std::make_shared<NsmGetEventSetting>(
        "GetEvSett", "NSM_GetEventSetting", eventSetting);

    EXPECT_EQ(getSensor->getName(), "GetEvSett");
    EXPECT_EQ(getSensor->getType(), "NSM_GetEventSetting");
    EXPECT_EQ(getSensor->eventSetting, eventSetting);
}

// ============================================================================
// setCurrentEventSources: decode success with cc != 0
// Exercises the LG2_ERROR_FLT shouldLog branch after decode
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest,
       SetCurrentEventSources_DecodeSuccess_NonZeroCC)
{
    std::vector<uint64_t> srcIds = {1};
    std::vector<uint64_t> ackIds = {};
    auto sensor = std::make_shared<NsmEventConfig>(
        "EvCfgDecCC", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, ackIds);

    // Response with non-zero completion code (NSM_ERROR)
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_common_resp(0, NSM_ERROR, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_CURRENT_EVENT_SOURCES, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->setCurrentEventSources(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   sensor->srcEventMask);
}

// Call twice to exercise the shouldLog flood-prevention FALSE branch
TEST_F(NsmEventConfigDeepBranchTest,
       SetCurrentEventSources_RepeatedCall_ShouldLogFloodPrevention)
{
    std::vector<uint64_t> srcIds = {2};
    std::vector<uint64_t> ackIds = {};
    auto sensor = std::make_shared<NsmEventConfig>(
        "EvCfgFlood", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, ackIds);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_CURRENT_EVENT_SOURCES, msg);

    // First call: shouldLog returns true (first occurrence)
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));
    sensor->setCurrentEventSources(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   sensor->srcEventMask);

    // Second call with same result: shouldLog flood prevention may suppress
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));
    sensor->setCurrentEventSources(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   sensor->srcEventMask);
}

// ============================================================================
// configureEventAcknowledgement: decode failure (rc != 0) path
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest,
       ConfigureEventAck_DecodeSuccess_CcZero_RcZero)
{
    std::vector<uint64_t> srcIds = {};
    std::vector<uint64_t> ackIds = {1, 2, 3};
    auto sensor = std::make_shared<NsmEventConfig>(
        "EvCfgAckOk", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, ackIds);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_configure_event_acknowledgement_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    bitfield8_t newMasks[EVENT_SOURCES_LENGTH] = {};
    newMasks[0].byte = 0x0E;
    encode_nsm_configure_event_acknowledgement_resp(0, NSM_SUCCESS, newMasks,
                                                    msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->configureEventAcknowledgement(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                          sensor->ackEventMask);
}

// configureEventAcknowledgement: decode failure with short response
TEST_F(NsmEventConfigDeepBranchTest,
       ConfigureEventAck_DecodeFailure_ShortResponse)
{
    std::vector<uint64_t> srcIds = {};
    std::vector<uint64_t> ackIds = {4, 5};
    auto sensor = std::make_shared<NsmEventConfig>(
        "EvCfgAckDec", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, ackIds);

    // Build a response that is too short for
    // decode_nsm_configure_event_acknowledgement_resp
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    sensor->configureEventAcknowledgement(gpu, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                          sensor->ackEventMask);
}

// ============================================================================
// NsmEventConfig::update: success path returning cc=0
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest, Update_Success_CcZero)
{
    std::vector<uint64_t> srcIds = {5, 10};
    std::vector<uint64_t> ackIds = {};
    auto sensor = std::make_shared<NsmEventConfig>(
        "EvCfgUpdOk", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, ackIds);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_CURRENT_EVENT_SOURCES, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
}

// ============================================================================
// NsmGetEventConfig::update: encode success, decode with non-zero cc
// covers the else branch (cc != NSM_SUCCESS) at line 244
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest,
       GetEventConfig_Update_DecodeNonZeroCC_NoValidation)
{
    std::vector<uint64_t> srcIds = {1, 2};
    std::vector<uint64_t> ackIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        "EvCfg", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcIds,
        ackIds);

    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgCC", "NSM_GetEventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, eventConfig);

    // Non-success response: minimal buffer with cc=NSM_ERROR
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto resp = reinterpret_cast<nsm_common_non_success_resp*>(msg->payload);
    resp->command = NSM_GET_CURRENT_EVENT_SOURCES;
    resp->completion_code = NSM_ERROR;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    getSensor->update(gpu);
}

// ============================================================================
// NsmGetEventConfig::update: success + validation pass (all events supported)
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest,
       GetEventConfig_Update_AllEventsSupported_NoResubscribe)
{
    std::vector<uint64_t> srcIds = {0, 1, 2};
    std::vector<uint64_t> ackIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        "EvCfgVP", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcIds,
        ackIds);

    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgVP", "NSM_GetEventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, eventConfig);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_event_source_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    bitfield8_t eventSources[EVENT_SOURCES_LENGTH] = {};
    eventSources[0].byte = 0x07; // bits 0,1,2 set
    encode_nsm_get_current_event_source_resp(0, NSM_SUCCESS, ERR_NULL,
                                             eventSources, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    getSensor->update(gpu);
}

// ============================================================================
// NsmEventSetting::setEventSubscription: decode success with non-zero cc
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest,
       SetEventSubscription_DecodeSuccess_NonZeroCC)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettCC", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);

    // Build response with non-zero cc
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_common_resp(0, NSM_ERROR, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_EVENT_SUBSCRIPTION, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->setEventSubscription(gpu);
}

// ============================================================================
// NsmEventSetting::update: setEventSubscription returns non-zero cc
// (not transport failure but device-reported error)
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest, EventSetting_Update_NonZeroCC_LogsError)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettUpCC", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);

    // Build response with cc=NSM_ERROR (not transport failure)
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_common_resp(0, NSM_ERROR, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_EVENT_SUBSCRIPTION, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
}

// ============================================================================
// NsmGetEventSetting::update: success path with matching localEid
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest, GetEventSetting_Update_Success_MatchingEid)
{
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EvSettOk", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);
    auto getSensor = std::make_shared<NsmGetEventSetting>(
        "GetEvSettOk", "NSM_GetEventSetting", eventSetting);

    eid_t localEid = gpu->getMctpLocalEid().value_or(0);

    std::vector<uint8_t> getResp(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_event_subscription_resp));
    auto getMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_nsm_get_event_subscription_resp(0, NSM_SUCCESS, ERR_NULL, localEid,
                                           getMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(getResp));

    getSensor->update(gpu);
}

// ============================================================================
// NsmGetEventSetting::update: cc != NSM_SUCCESS triggers re-subscribe,
// and setEventSubscription also fails
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest,
       GetEventSetting_Update_CCError_ResubscribeFails)
{
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EvSettRF", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);
    auto getSensor = std::make_shared<NsmGetEventSetting>(
        "GetEvSettRF", "NSM_GetEventSetting", eventSetting);

    // Non-success cc response
    const Response getErrCCResp{0x10,
                                0xDE,
                                0x00,
                                0x89,
                                NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                NSM_GET_EVENT_SUBSCRIPTION,
                                NSM_ERROR,
                                0x00,
                                0x00};

    // setEventSubscription call also fails
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(getErrCCResp))
        .WillOnce(mockSensorIO(NSM_ERROR));

    getSensor->update(gpu);
}

// ============================================================================
// NsmGetEventConfig::validateEventIds: all events supported (return true)
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest, ValidateEventIds_AllSupported_ReturnsTrue)
{
    std::vector<uint64_t> srcIds = {0, 1, 2, 3, 4, 5, 6, 7};
    std::vector<uint64_t> ackIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        "EvCfgV", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcIds,
        ackIds);

    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgV", "NSM_GetEventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, eventConfig);

    bitfield8_t eventSources[EVENT_SOURCES_LENGTH] = {};
    eventSources[0].byte = 0xFF; // all bits 0-7 set

    EXPECT_TRUE(getSensor->validateEventIds(gpu->getEid(), eventSources));
}

// ============================================================================
// NsmGetEventConfig::validateEventIds: some events NOT supported (return false)
// Called twice to exercise shouldLog flood prevention
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest,
       ValidateEventIds_SomeUnsupported_Repeated_FloodPrevention)
{
    std::vector<uint64_t> srcIds = {1, 2, 3};
    std::vector<uint64_t> ackIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        "EvCfgVF", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcIds,
        ackIds);

    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgVF", "NSM_GetEventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, eventConfig);

    bitfield8_t eventSources[EVENT_SOURCES_LENGTH] = {};
    eventSources[0].byte = 0x02; // only bit 1 set; bits 2,3 NOT set

    // First call: shouldLog TRUE → logs unsupported events
    EXPECT_FALSE(getSensor->validateEventIds(gpu->getEid(), eventSources));

    // Second call with same state: shouldLog flood prevention may suppress
    EXPECT_FALSE(getSensor->validateEventIds(gpu->getEid(), eventSources));

    // Third call with all supported: shouldLog with state change
    eventSources[0].byte = 0x0E; // bits 1,2,3 all set
    EXPECT_TRUE(getSensor->validateEventIds(gpu->getEid(), eventSources));

    // Fourth call back to unsupported: shouldLog TRUE again (state changed)
    eventSources[0].byte = 0x02;
    EXPECT_FALSE(getSensor->validateEventIds(gpu->getEid(), eventSources));
}

// ============================================================================
// NsmEventConfig::update: repeated calls to exercise shouldLog branches in
// setCurrentEventSources LG2_ERROR_FLT
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest,
       Update_RepeatedCalls_ShouldLogFloodPrevention)
{
    std::vector<uint64_t> srcIds = {3};
    std::vector<uint64_t> ackIds = {};
    auto sensor = std::make_shared<NsmEventConfig>(
        "EvCfgRepeat", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, ackIds);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_CURRENT_EVENT_SOURCES, msg);

    // Call update twice with same response to exercise shouldLog branches
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(response))
        .WillOnce(mockSensorIO(response));

    sensor->update(gpu);
    sensor->update(gpu);
}

// ============================================================================
// NsmGetEventConfig::update: repeated calls with validation fail then pass
// to exercise shouldLog state transitions
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest,
       GetEventConfig_Update_Repeated_ValidationStateChange)
{
    std::vector<uint64_t> srcIds = {1};
    std::vector<uint64_t> ackIds = {};
    auto eventConfig = std::make_shared<NsmEventConfig>(
        "EvCfgSC", "NSM_EventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL, srcIds,
        ackIds);

    auto getSensor = std::make_shared<NsmGetEventConfig>(
        "GetEvCfgSC", "NSM_GetEventConfig", NSM_TYPE_PLATFORM_ENVIRONMENTAL,
        srcIds, eventConfig);

    // First: all supported → validation passes
    std::vector<uint8_t> respOk(sizeof(nsm_msg_hdr) +
                                sizeof(nsm_get_event_source_resp));
    auto msgOk = reinterpret_cast<nsm_msg*>(respOk.data());
    bitfield8_t srcOk[EVENT_SOURCES_LENGTH] = {};
    srcOk[0].byte = 0x02; // bit 1 set
    encode_nsm_get_current_event_source_resp(0, NSM_SUCCESS, ERR_NULL, srcOk,
                                             msgOk);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(respOk));
    getSensor->update(gpu);

    // Second: event not supported → validation fails → triggers resubscribe
    std::vector<uint8_t> respFail(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_event_source_resp));
    auto msgFail = reinterpret_cast<nsm_msg*>(respFail.data());
    bitfield8_t srcFail[EVENT_SOURCES_LENGTH] = {};
    srcFail[0].byte = 0x00; // bit 1 NOT set
    encode_nsm_get_current_event_source_resp(0, NSM_SUCCESS, ERR_NULL, srcFail,
                                             msgFail);

    std::vector<uint8_t> setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto setMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_CURRENT_EVENT_SOURCES, setMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(respFail))
        .WillOnce(mockSensorIO(setResp));

    getSensor->update(gpu);
}

// ============================================================================
// NsmEventSetting::setEventSubscription: success with cc=0 and rc=0
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest, SetEventSubscription_Success_CcZero_RcZero)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettSuc", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_SET_EVENT_SUBSCRIPTION, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->setEventSubscription(gpu);
}

// ============================================================================
// NsmEventSetting::setEventSubscription: decode returns non-zero rc
// (response too short for success decode)
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest, SetEventSubscription_DecodeError_LogsError)
{
    auto sensor = std::make_shared<NsmEventSetting>(
        "EvSettDE", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_POLLING,
        gpu);

    // Very short response that will fail decode
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    sensor->setEventSubscription(gpu);
}

// ============================================================================
// NsmGetEventSetting::update: decode error (short response)
// ============================================================================

TEST_F(NsmEventConfigDeepBranchTest,
       GetEventSetting_Update_DecodeError_ReturnsError)
{
    auto eventSetting = std::make_shared<NsmEventSetting>(
        "EvSettDE2", "NSM_EventSetting", GLOBAL_EVENT_GENERATION_ENABLE_PUSH,
        gpu);
    auto getSensor = std::make_shared<NsmGetEventSetting>(
        "GetEvSettDE", "NSM_GetEventSetting", eventSetting);

    // Short response
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    getSensor->update(gpu);
}
