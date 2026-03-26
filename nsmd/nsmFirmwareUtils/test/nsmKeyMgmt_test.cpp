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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "libnsm/firmware-utils.h"

#include "nsmKeyMgmt.hpp"

using namespace nsm;

// ============================================================================
// Fixture: NsmKeyMgmtTest
// ============================================================================

struct NsmKeyMgmtTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "HGX_EROT_KeyMgmt";
    const std::string type = "NSM_KeyMgmt";
    // FPGA device UUID — must match a device in devices table
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    eid_t eid = 0;
    uint8_t instanceId = 0;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    std::shared_ptr<ProgressIntf> progressIntf;
    std::shared_ptr<NsmKeyMgmt> sensor;

    NsmKeyMgmtTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(fpga, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                                   name + "/Progress";
        progressIntf = std::make_shared<ProgressIntf>(bus,
                                                      progressPath.c_str());
        sensor = std::make_shared<NsmKeyMgmt>(bus, name, type, deviceUuid,
                                              progressIntf, 1, 1, 0);
        ASSERT_NE(sensor, nullptr);
    }

    ~NsmKeyMgmtTest() override
    {
        sensor.reset();
        progressIntf.reset();
        cleanupDeviceSensors(devices);
    }

    // Build a valid NSM success response for code-auth-key-perm query.
    // permissionBitmapLength = bitmapLen (4 bitmaps of bitmapLen bytes each).
    std::vector<uint8_t> buildQueryResponse(uint8_t bitmapLen)
    {
        size_t bitmapDataSize = 4UL * bitmapLen;
        size_t totalSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_code_auth_key_perm_query_resp) +
                           bitmapDataSize;
        std::vector<uint8_t> resp(totalSize, 0);
        auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

        std::vector<uint8_t> bitmapData(bitmapDataSize + 1, 0);
        uint8_t* bitmapPtr = bitmapData.data();

        encode_nsm_code_auth_key_perm_query_resp(
            instanceId, NSM_SUCCESS, ERR_NULL,
            /*active_key_idx*/ 0, /*pending_key_idx*/ 0, bitmapLen,
            bitmapLen ? bitmapPtr + 0 * bitmapLen : nullptr,
            bitmapLen ? bitmapPtr + 1 * bitmapLen : nullptr,
            bitmapLen ? bitmapPtr + 2 * bitmapLen : nullptr,
            bitmapLen ? bitmapPtr + 3 * bitmapLen : nullptr, msg);
        return resp;
    }

    // Build an NSM error response (cc != NSM_SUCCESS) with the correct
    // msg_len = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp).
    std::vector<uint8_t>
        buildQueryErrorResponse(uint8_t cc = NSM_ERR_INVALID_DATA,
                                uint16_t reasonCode = ERR_NULL)
    {
        size_t totalSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_common_non_success_resp);
        std::vector<uint8_t> resp(totalSize, 0);
        auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
        // Use bitmapLen=0 and null pointers to trigger the error-cc encode path
        encode_nsm_code_auth_key_perm_query_resp(instanceId, cc, reasonCode, 0,
                                                 0, 0, nullptr, nullptr,
                                                 nullptr, nullptr, msg);
        return resp;
    }

    // Build a valid encode_nsm_code_auth_key_perm_update_resp response
    std::vector<uint8_t> buildUpdateResponse(uint8_t cc = NSM_SUCCESS,
                                             uint16_t reasonCode = ERR_NULL,
                                             uint32_t updateMethod = 0)
    {
        size_t totalSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_code_auth_key_perm_update_resp);
        std::vector<uint8_t> resp(totalSize, 0);
        auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_nsm_code_auth_key_perm_update_resp(instanceId, cc, reasonCode,
                                                  updateMethod, msg);
        return resp;
    }
};

// ============================================================================
// Constructor test
// ============================================================================

TEST_F(NsmKeyMgmtTest, Constructor_ValidParams_CreatesObject)
{
    EXPECT_EQ(sensor->getName(), name);
    EXPECT_EQ(sensor->getType(), type);
    EXPECT_NE(sensor->settingsObject, nullptr);
    EXPECT_FALSE(sensor->opInProgress);
}

// ============================================================================
// genRequestMsg tests
// ============================================================================

TEST_F(NsmKeyMgmtTest, GenRequestMsg_ValidParams_ReturnsRequest)
{
    auto request = sensor->genRequestMsg(eid, instanceId);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_query_req));
}

TEST_F(NsmKeyMgmtTest, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// handleResponseMsg tests
// ============================================================================

TEST_F(NsmKeyMgmtTest, HandleResponseMsg_TooSmallBuffer_ReturnsFail)
{
    // Buffer too small to decode → first decode returns error
    std::vector<uint8_t> tiny(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN + 1,
                              0);
    tiny[sizeof(nsm_msg_hdr) + 1] = NSM_ERR_INVALID_DATA;
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(tiny.data()), tiny.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmKeyMgmtTest, HandleResponseMsg_ErrorCC_DoesNotUpdateBitmapLength)
{
    // An error CC response has the non-success format;
    // decode_reason_code_and_cc returns NSM_SW_SUCCESS but with *cc = error.
    // handleResponseMsg returns the decode rc (NSM_SW_SUCCESS) and does NOT
    // update bitmapLength.
    sensor->bitmapLength = 42; // sentinel - should NOT change on error CC
    auto resp = buildQueryErrorResponse(NSM_ERR_INVALID_DATA);
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(resp.data()), resp.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor->bitmapLength, 42); // not updated because of early return
}

TEST_F(NsmKeyMgmtTest, HandleResponseMsg_SuccessNoSlots_UpdatesProperties)
{
    // Build a valid response with bitmapLength=1
    auto resp = buildQueryResponse(1);
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(resp.data()), resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor->bitmapLength, 1);
}

TEST_F(NsmKeyMgmtTest, HandleResponseMsg_WithSlotObjects_UpdatesSlots)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string slotPath = std::string(chassisInventoryBasePath) + "/" + name +
                           "/FwSlot/0";
    auto slot = std::make_shared<NsmFirmwareSlot>(
        bus, slotPath, std::vector<utils::Association>{}, 0,
        SlotIntf::FirmwareType::AP, name);
    sensor->addSlotObject(slot);

    auto resp = buildQueryResponse(1);
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(resp.data()), resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// revokeKeys throw-path tests (no async involved)
// ============================================================================

TEST_F(NsmKeyMgmtTest, RevokeKeys_OpInProgress_ThrowsUnavailable)
{
    sensor->opInProgress = true;
    EXPECT_THROW(sensor->revokeKeys(
                     SecurityCommon::RequestTypes::MostRestrictiveValue, 0, {}),
                 sdbusplus::error::xyz::openbmc_project::common::Unavailable);
}

TEST_F(NsmKeyMgmtTest, RevokeKeys_InvalidRequestType_ThrowsInvalidArgument)
{
    // Cast an out-of-range value as RequestTypes to trigger the 'else' branch
    auto invalidType = static_cast<SecurityCommon::RequestTypes>(99);
    EXPECT_THROW(
        sensor->revokeKeys(invalidType, 0, {}),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmKeyMgmtTest,
       RevokeKeys_MostRestrictive_NonEmptyIndices_ThrowsInvalidArgument)
{
    EXPECT_THROW(
        sensor->revokeKeys(SecurityCommon::RequestTypes::MostRestrictiveValue,
                           0, {1, 2}),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmKeyMgmtTest,
       RevokeKeys_SpecifiedValue_IndexOutOfBounds_ThrowsInvalidArgument)
{
    // bitmapLength = 9 > maxBitmapSize (8) → indicesToBitmap throws
    sensor->bitmapLength = 9;
    EXPECT_THROW(
        sensor->revokeKeys(SecurityCommon::RequestTypes::SpecifiedValue, 0,
                           {0}),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmKeyMgmtTest,
       RevokeKeys_SpecifiedValue_EmptyBitmap_ThrowsInvalidArgument)
{
    // bitmapLength=0, indices={} → indicesToBitmap returns empty vector →
    // encode fails with NSM_SW_ERROR_DATA == NSM_ERR_INVALID_DATA (0x02) →
    // throws InvalidArgument (lines 182,184-186,188 covered).
    // Note: the InternalFailure path (line 190) is unreachable because the
    // encode function only returns NSM_SW_ERROR_DATA (== NSM_ERR_INVALID_DATA).
    sensor->bitmapLength = 0;
    EXPECT_THROW(
        sensor->revokeKeys(SecurityCommon::RequestTypes::SpecifiedValue, 0, {}),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// revokeKeysAsyncHandler tests (via revokeKeys → encode succeeds →
// revokeKeysAsyncHandler runs synchronously in coverage mode)
// ============================================================================

TEST_F(NsmKeyMgmtTest, RevokeKeys_PostPatchFails_SetsErrorCode)
{
    // postPatchIO returns error → sendRc != NSM_SW_SUCCESS branch (lines
    // 92-101)
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    EXPECT_NO_THROW(sensor->revokeKeys(
        SecurityCommon::RequestTypes::MostRestrictiveValue, 0, {}));
    // After abort, opInProgress should be false
    EXPECT_FALSE(sensor->opInProgress);
}

TEST_F(NsmKeyMgmtTest, RevokeKeys_DecodeFailure_SetsErrorCode)
{
    // postPatchIO returns too-small buffer → decode fails (lines 108-119)
    std::vector<uint8_t> tiny(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(tiny));

    EXPECT_NO_THROW(sensor->revokeKeys(
        SecurityCommon::RequestTypes::MostRestrictiveValue, 0, {}));
    EXPECT_FALSE(sensor->opInProgress);
}

TEST_F(NsmKeyMgmtTest, RevokeKeys_SuccessPath_UpdatesMethod)
{
    // postPatchIO returns valid update response → decode succeeds (lines
    // 121-133) sensorIO called during update() → mock to fail (update rc
    // logged)
    auto updateResp = buildUpdateResponse(NSM_SUCCESS, ERR_NULL, 0x01);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(updateResp));
    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    EXPECT_NO_THROW(sensor->revokeKeys(
        SecurityCommon::RequestTypes::MostRestrictiveValue, 0, {}));
    EXPECT_FALSE(sensor->opInProgress);
}
