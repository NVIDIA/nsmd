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
 * Branch coverage for nsmd/nsmEventConfig/nsmEventSetting.cpp
 *
 * Covers branches not hit by existing tests:
 * - copySubscriptionResponse: null responseMsg → returns nullopt
 * - NsmEventSetting::setEventSubscription: !localEidOpt → early return
 * - NsmGetEventSetting::update: !localEidOpt after sensorIO → early return
 * - NsmGetEventSetting::update: cc!=NSM_SUCCESS && rc==0 →
 *   setEventSubscription called
 * - NsmGetEventSetting::update: receiver_eid != localEid →
 *   setEventSubscription called
 * - createNsmEventSetting: invalid eventGenerationSetting → NSM_ERROR
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-capability-discovery.h"

#include "nsmEventSetting.hpp"
#include "test/commonMock.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmEventSettingBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmEventSettingBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmEventSettingBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmEventSetting> makeEventSetting(
        uint8_t setting = GLOBAL_EVENT_GENERATION_ENABLE_POLLING)
    {
        return std::make_shared<NsmEventSetting>("EvtSet", "NSM_EventSetting",
                                                 setting, gpu);
    }

    std::shared_ptr<NsmGetEventSetting>
        makeGetEventSetting(std::shared_ptr<NsmEventSetting> evtSet = nullptr)
    {
        if (!evtSet)
        {
            evtSet = makeEventSetting();
        }
        return std::make_shared<NsmGetEventSetting>(
            "GetEvtSet", "NSM_GetEventSetting", evtSet);
    }
};

// ============================================================================
// NsmEventSetting::setEventSubscription: !localEidOpt → early return
// (lines 70-76 in nsmEventSetting.cpp)
// ============================================================================

TEST_F(NsmEventSettingBranchTest, SetEventSubscription_NoLocalEid_EarlyReturn)
{
    // Make getMctpLocalEid() return nullopt by clearing mctpLocalEid
    gpu->mctpLocalEid = std::nullopt;

    auto evtSet = makeEventSetting();

    // With localEidOpt = nullopt, should return NSM_ERR_INVALID_DATA
    int rc = -1;
    auto coro = [&]() -> requester::Coroutine {
        rc = co_await evtSet->setEventSubscription(gpu);
        co_return NSM_SW_SUCCESS;
    };
    coro().detach();

    EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// ============================================================================
// NsmEventSetting::update: setEventSubscription returns error → rc propagated
// (verifies setEventMode still called and rc returned)
// ============================================================================

TEST_F(NsmEventSettingBranchTest, Update_SetSubscriptionFails_RcPropagated)
{
    // Make getMctpLocalEid() return nullopt → setEventSubscription returns
    // NSM_ERR_INVALID_DATA
    gpu->mctpLocalEid = std::nullopt;

    auto evtSet = makeEventSetting();

    int rc = 0;
    auto coro = [&]() -> requester::Coroutine {
        rc = co_await evtSet->update(gpu);
        co_return NSM_SW_SUCCESS;
    };
    coro().detach();

    EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// ============================================================================
// NsmGetEventSetting::update: !localEidOpt after sensorIO → early return
// (lines 171-176 in nsmEventSetting.cpp)
// ============================================================================

TEST_F(NsmEventSettingBranchTest,
       GetEventSetting_Update_NoLocalEidAfterSensorIO_EarlyReturn)
{
    // localEid initially unset, so the get-path check at line 171 fires
    gpu->mctpLocalEid = std::nullopt;

    auto getEvtSet = makeGetEventSetting();

    // Build a valid "get event subscription" response so sensorIO succeeds
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_event_subscription_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    uint8_t receiverEid = 0;
    encode_nsm_get_event_subscription_resp(0, NSM_SUCCESS, ERR_NULL,
                                           receiverEid, msg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    int rc = -1;
    auto coro = [&]() -> requester::Coroutine {
        rc = co_await getEvtSet->update(gpu);
        co_return NSM_SW_SUCCESS;
    };
    coro().detach();

    EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// ============================================================================
// NsmGetEventSetting::update: cc != NSM_SUCCESS && rc == 0 →
// setEventSubscription called (lines 193-202)
// ============================================================================

TEST_F(NsmEventSettingBranchTest,
       GetEventSetting_Update_CcNonZero_CallsSetSubscription)
{
    // Set up localEid so getMctpLocalEid() returns a value
    gpu->mctpLocalEid = eid_t{5};

    auto evtSet = makeEventSetting();
    auto getEvtSet = makeGetEventSetting(evtSet);

    // First sensorIO: get-event-subscription response with non-zero CC
    // (cc != NSM_SUCCESS) → triggers setEventSubscription call.
    // When cc != NSM_SUCCESS, decode_reason_code_and_cc expects exactly
    // sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp) bytes;
    // encode_nsm_get_event_subscription_resp with cc=NSM_ERROR calls
    // encode_reason_code which writes the non-success layout.
    std::vector<uint8_t> getResp(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_common_non_success_resp));
    auto getMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_nsm_get_event_subscription_resp(0, NSM_ERROR, ERR_NULL, 0, getMsg);

    // Second sensorIO: set-event-subscription response (success for
    // setEventSubscription call) - constructed as a common response
    std::vector<uint8_t> setResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_event_subscription_resp), 0);
    auto* setHdr = reinterpret_cast<nsm_msg*>(setResp.data());
    auto* setPayload = reinterpret_cast<nsm_common_resp*>(setHdr->payload);
    setPayload->completion_code = NSM_SUCCESS;
    setPayload->data_size = htole16(0);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(getResp))
        .WillOnce(mockSensorIO(setResp));

    int rc = -1;
    auto coro = [&]() -> requester::Coroutine {
        rc = co_await getEvtSet->update(gpu);
        co_return NSM_SW_SUCCESS;
    };
    coro().detach();

    // rc = NSM_ERROR (the cc from get response), but setEventSubscription
    // ran successfully
    EXPECT_NE(rc, -1); // coroutine ran
}

// ============================================================================
// NsmGetEventSetting::update: receiver_eid != localEid →
// setEventSubscription called (lines 193-202)
// ============================================================================

TEST_F(NsmEventSettingBranchTest,
       GetEventSetting_Update_ReceiverEidMismatch_CallsSetSubscription)
{
    gpu->mctpLocalEid = eid_t{10};

    auto evtSet = makeEventSetting();
    auto getEvtSet = makeGetEventSetting(evtSet);

    // Get response: cc == NSM_SUCCESS but receiver_eid != localEid (10 vs 5)
    std::vector<uint8_t> getResp(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_event_subscription_resp));
    auto getMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    uint8_t receiverEid = 5; // != localEid (10) → mismatch
    encode_nsm_get_event_subscription_resp(0, NSM_SUCCESS, ERR_NULL,
                                           receiverEid, getMsg);

    // Set response - constructed as a common response
    std::vector<uint8_t> setResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_event_subscription_resp), 0);
    auto* setHdr2 = reinterpret_cast<nsm_msg*>(setResp.data());
    auto* setPayload2 = reinterpret_cast<nsm_common_resp*>(setHdr2->payload);
    setPayload2->completion_code = NSM_SUCCESS;
    setPayload2->data_size = htole16(0);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(getResp))
        .WillOnce(mockSensorIO(setResp));

    int rc = -1;
    auto coro = [&]() -> requester::Coroutine {
        rc = co_await getEvtSet->update(gpu);
        co_return NSM_SW_SUCCESS;
    };
    coro().detach();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// createNsmEventSetting: eventGenerationSetting > GLOBAL_EVENT_GENERATION...
// → NSM_ERROR (line 249-256)
// ============================================================================

namespace nsm
{
requester::Coroutine createNsmEventSetting(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

TEST_F(NsmEventSettingBranchTest,
       Factory_InvalidEventGenerationSetting_ReturnsError)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_EventSetting";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/evtsetting_invalid_setting";

    auto& pm = utils::MockDbusAsync::propertyMap(
        objPath, "xyz.openbmc_project.Configuration.NSM_EventSetting");
    pm["Name"] = std::string("EvtSettingInvalid");
    pm["UUID"] = gpuUuid;
    // EventGenerationSetting > GLOBAL_EVENT_GENERATION_ENABLE_PUSH (max valid
    // value = 2; set to 99 which is clearly invalid)
    pm["EventGenerationSetting"] = uint64_t(99);

    const size_t before = gpu->deviceSensors.size();
    EXPECT_NO_THROW(createNsmEventSetting(mockManager, interface, objPath));
    // No sensor added (factory returned NSM_ERROR before creating sensor)
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// createNsmEventSetting: device not found → NSM_ERROR (lines 240-247)
// ============================================================================

TEST_F(NsmEventSettingBranchTest, Factory_DeviceNotFound_ReturnsError)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_EventSetting";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/evtsetting_nodev";

    auto& pm = utils::MockDbusAsync::propertyMap(
        objPath, "xyz.openbmc_project.Configuration.NSM_EventSetting");
    pm["Name"] = std::string("EvtSettingNoDevice");
    // Use a UUID that doesn't match any device
    pm["UUID"] = uuid_t("STATIC:99:99:NSM_DEVICE_INSTANCE_NUMBER:99");
    pm["EventGenerationSetting"] = uint64_t(0);

    const size_t before = gpu->deviceSensors.size();
    EXPECT_NO_THROW(createNsmEventSetting(mockManager, interface, objPath));
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}
