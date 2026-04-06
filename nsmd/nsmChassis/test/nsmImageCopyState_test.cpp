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

/*
 * Unit tests for NsmImageCopyState / NsmImageCopyStateObject - the NSM
 * Image Copy Control "Query Image Copy Progress" flow (request type = 0).
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
using namespace ::testing;

#include "firmware-utils.h"

#define private public
#define protected public

#include "nsmRoTProperty.hpp"

using namespace nsm;
using Status = sdbusplus::common::com::nvidia::ImageCopyState::Status;

struct NsmImageCopyStateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    // Wire-format response types from libnsm.
    // RespCmd is the on-the-wire payload (header + Resp); Resp is just the
    // inner status/progress struct that updateProperties() consumes.
    using RespCmd = nsm_firmware_image_copy_control_query_progress_resp_command;
    using Resp = nsm_firmware_image_copy_control_query_progress_resp;

    NsmDeviceTable devices;
    int objectCount = 0;

    NsmImageCopyStateTest() : SensorManagerTest(devices) {}
    ~NsmImageCopyStateTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmImageCopyStateObject> makeObject()
    {
        return std::make_shared<NsmImageCopyStateObject>(
            utils::DBusHandler::getBus(),
            "HGX_Chassis_ICS_" + std::to_string(++objectCount));
    }

    // Snapshot of (status, progress) currently published on D-Bus.
    static std::pair<Status, uint8_t> state(const NsmImageCopyStateObject& obj)
    {
        return {obj.imageCopyState->status(), obj.imageCopyState->progress()};
    }

    // Drive updateProperties() directly with a wire-format payload and return
    // the resulting D-Bus state.
    static std::pair<Status, uint8_t> applyUpdate(NsmImageCopyStateObject& obj,
                                                  uint8_t wireStatus,
                                                  uint8_t wireProgress)
    {
        Resp resp{wireStatus, wireProgress};
        obj.imageCopyState->updateProperties(resp);
        return state(obj);
    }

    // Build a full NSM query-progress response (success or non-success CC)
    // and dispatch it through handleResponseMsg. Returns the rc.
    static uint8_t feedResponse(NsmImageCopyStateObject& obj, uint8_t cc,
                                uint8_t wireStatus = 0,
                                uint8_t wireProgress = 0)
    {
        const size_t payloadSize = (cc == NSM_SUCCESS)
                                       ? sizeof(RespCmd)
                                       : sizeof(nsm_common_non_success_resp);
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + payloadSize);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_common_resp(0, cc, ERR_NULL, NSM_TYPE_FIRMWARE,
                           NSM_FW_IMAGE_COPY_CONTROL, msg);
        if (cc == NSM_SUCCESS)
        {
            auto* respCmd = reinterpret_cast<RespCmd*>(msg->payload);
            respCmd->hdr.data_size = htole16(sizeof(Resp));
            respCmd->image_copy_control_query.image_copy_status = wireStatus;
            respCmd->image_copy_control_query.image_copy_progress =
                wireProgress;
        }
        return obj.handleResponseMsg(msg, buf.size());
    }
};

// =============================================================================
// updateProperties: status mapping (8 spec values + invalid wire fallback)
// =============================================================================

TEST_F(NsmImageCopyStateTest, UpdateProperties_StatusMapping)
{
    struct Case
    {
        uint8_t wire;
        Status expected;
    };
    const Case cases[] = {
        // 8 spec values: each maps to its named D-Bus status.
        {NSM_IMAGE_COPY_NOT_TRIGGERED, Status::ImageCopyNotTriggered},
        {NSM_IMAGE_COPY_IN_PROGRESS, Status::InProgress},
        {NSM_IMAGE_COPY_COMPLETE, Status::Complete},
        {NSM_IMAGE_COPY_UNDEFINED_FAILURE, Status::UndefinedFailure},
        {NSM_IMAGE_COPY_NO_VALID_IMAGE, Status::NoValidImage},
        {NSM_IMAGE_COPY_DESTINATION_WRITE_PROTECTED,
         Status::DestinationWriteProtected},
        {NSM_IMAGE_COPY_FAIL_FLASH_ACCESS, Status::FailFlashAccess},
        {NSM_IMAGE_COPY_FAILED_VERIFY, Status::FailedVerify},
        // Out-of-enum: falls through to default branch.
        {0xFE, Status::UndefinedFailure},
    };
    auto obj = makeObject();
    for (const auto& c : cases)
    {
        EXPECT_EQ(applyUpdate(*obj, c.wire, 0),
                  (std::pair<Status, uint8_t>{c.expected, 0}));
    }
}

// =============================================================================
// updateProperties: progress forwarding + out-of-spec clamping
//
// In-spec (0..101): forwarded verbatim. The progressNotSupported sentinel
// signals "FD does not support progress reporting" per spec.
// Out-of-spec (>101): clamped to progressNotSupported so we don't leak a
// bogus percentage onto D-Bus. status() still reflects the wire value.
// =============================================================================

TEST_F(NsmImageCopyStateTest, UpdateProperties_ProgressForwardingAndClamping)
{
    struct Case
    {
        uint8_t wire;
        uint8_t expected;
    };
    const Case cases[] = {
        // In-spec percentages: forwarded verbatim.
        {0, 0},
        {50, 50},
        {100, 100},
        // Sentinel: FD does not support progress reporting.
        {NsmImageCopyState::progressNotSupported,
         NsmImageCopyState::progressNotSupported},
        // Out-of-spec (>101): clamped to progressNotSupported.
        {0x66, NsmImageCopyState::progressNotSupported},
        {0xC8, NsmImageCopyState::progressNotSupported},
        {0xFF, NsmImageCopyState::progressNotSupported},
    };
    auto obj = makeObject();
    for (const auto& c : cases)
    {
        EXPECT_EQ(applyUpdate(*obj, NSM_IMAGE_COPY_IN_PROGRESS, c.wire),
                  (std::pair<Status, uint8_t>{Status::InProgress, c.expected}));
    }
}

// =============================================================================
// genRequestMsg
// =============================================================================

TEST_F(NsmImageCopyStateTest, GenRequestMsg_SuccessAndEncodeFail)
{
    auto obj = makeObject();
    EXPECT_EQ(obj->getType(), std::string("NSM_ChassisRoT"));

    auto request = obj->genRequestMsg(0, 0);
    ASSERT_TRUE(request.has_value());

    // The encoded buffer is: [nsm_msg_hdr | nsm_common_req | request body].
    auto* msg = reinterpret_cast<nsm_msg*>(request->data());
    auto* req = reinterpret_cast<nsm_firmware_image_copy_control_req*>(
        msg->payload + sizeof(nsm_common_req));
    EXPECT_EQ(req->request_type, NSM_IMAGE_COPY_QUERY_PROGRESS);
    EXPECT_EQ(req->component_count, 0);

    // Bad instance id (> NSM_INSTANCE_MAX) -> encode failure.
    EXPECT_FALSE(obj->genRequestMsg(0, NSM_INSTANCE_MAX + 1).has_value());
}

// =============================================================================
// handleResponseMsg
// =============================================================================

TEST_F(NsmImageCopyStateTest, HandleResponseMsg_SuccessUpdatesProperties)
{
    auto obj = makeObject();
    EXPECT_EQ(feedResponse(*obj, NSM_SUCCESS, NSM_IMAGE_COPY_IN_PROGRESS, 50),
              NSM_SUCCESS);
    EXPECT_EQ(state(*obj),
              (std::pair<Status, uint8_t>{Status::InProgress, 50}));
}

TEST_F(NsmImageCopyStateTest, HandleResponseMsg_ErrorCcKeepsConstructorDefaults)
{
    auto obj = makeObject();
    EXPECT_EQ(feedResponse(*obj, NSM_ERROR), NSM_ERROR);
    EXPECT_EQ(state(*obj), (std::pair<Status, uint8_t>{
                               Status::ImageCopyNotTriggered,
                               NsmImageCopyState::progressNotSupported}));
}

TEST_F(NsmImageCopyStateTest, HandleResponseMsg_ShortBufferReturnsLengthError)
{
    // hdr + nsm_common_resp: cc=NSM_SUCCESS but message is shorter than the
    // full query_progress_resp_command, so libnsm's decoder returns
    // NSM_SW_ERROR_LENGTH.
    auto obj = makeObject();
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    EXPECT_EQ(obj->handleResponseMsg(msg, buf.size()), NSM_SW_ERROR_LENGTH);
}
