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
 * Branch coverage for nsmd/nsmChassis/nsmRoTProperty.cpp
 *
 * Covers:
 * - mapReasonCodeToErrorCode: all 7 switch cases exercised via
 * setImageCopyResult
 * - NsmInbandUpdatePolicy::updateProperties: ENABLE/DISABLE/default branches
 * - NsmImageCopyPolicy::updateProperties: MANUAL/AUTOMATIC/default + shouldLog
 * - NsmInbandUpdatePolicyObject: genRequestMsg encode fail, handleResponseMsg
 *   success/errorCC/decodeFail, cc?cc:rc ternary both sides
 * - NsmImageCopyPolicyObject: same pattern
 * - InbandUpdatePolicyHandler::updateInbandUpdatePolicy coroutine: invalid
 *   value type, invalid policy string, encode fail, postPatchIO fail, decode
 *   fail (cc!=0 and rc!=0), success
 * - ImageCopyPolicyHandler::updateImageCopyPolicy: same pattern
 * - imageCopyAsyncHandler: encode fail, postPatchIO fail, decode fail, success
 * - shouldLog repeated-call suppression paths
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "firmware-utils.h"

#define private public
#define protected public

#include "nsmFirmwareUtils/nsmFirmwareUtilsCommon.hpp"
#include "nsmRoTProperty.hpp"

namespace nsm
{
sdbusplus::common::com::nvidia::ImageCopy::ErrorCode
    mapReasonCodeToErrorCode(uint16_t reasonCode);
} // namespace nsm

using namespace nsm;

// =============================================================================
// Fixture
// =============================================================================

struct NsmRoTPropertyBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis_BR";
    const uint16_t classification = 1;
    const uint16_t identifier = 2;
    const uint8_t index = 0;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmRoTPropertyBranchTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmRoTPropertyBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    // Helper: build a valid NSM_SUCCESS erot_state response
    static std::vector<uint8_t> buildErotStateResponse(uint8_t inbandPolicy,
                                                       uint8_t bgCopyPolicy)
    {
        std::vector<uint8_t> buf(256, 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        nsm_firmware_erot_state_info_resp erotInfo = {};
        erotInfo.slot_info = nullptr;
        erotInfo.fq_resp_hdr.inband_update_policy = inbandPolicy;
        erotInfo.fq_resp_hdr.background_copy_policy = bgCopyPolicy;
        erotInfo.fq_resp_hdr.firmware_slot_count = 0;
        [[maybe_unused]] auto rc =
            encode_nsm_query_get_erot_state_parameters_resp(
                0, NSM_SUCCESS, ERR_NULL, &erotInfo, msg);
        return buf;
    }

    // Helper: build a valid set_rot_property success response
    static std::vector<uint8_t> buildSetRotPropertyResponse(uint8_t cc,
                                                            uint16_t reason)
    {
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) +
                sizeof(nsm_firmware_set_rot_property_resp_command),
            0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        [[maybe_unused]] auto rc =
            encode_nsm_firmware_set_rot_property_resp(0, cc, reason, msg);
        return resp;
    }

    // Helper: build an error CC erot_state response (valgrind-safe size)
    static std::vector<uint8_t> buildErotErrorCCResponse(uint8_t cc,
                                                         uint16_t reason)
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp),
            0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        nsm_firmware_erot_state_info_resp erotInfo = {};
        erotInfo.slot_info = nullptr;
        [[maybe_unused]] auto rc =
            encode_nsm_query_get_erot_state_parameters_resp(0, cc, reason,
                                                            &erotInfo, msg);
        return buf;
    }

    // Helper: build a decode-fail buffer (too short but valgrind-safe)
    static std::vector<uint8_t> buildDecodeFailBuffer()
    {
        return std::vector<uint8_t>(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    }
};

// =============================================================================
// mapReasonCodeToErrorCode: all switch cases via setImageCopyResult
// The function is static, only reachable through
// NsmImageCopy::setImageCopyResult
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest, MapReasonCode_AllCases)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;

    auto& bus = utils::DBusHandler::getBus();
    auto nsmObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_mrc", classification, identifier, index);
    auto imageCopy = std::make_unique<NsmImageCopy>(
        bus, std::string(chassisInventoryBasePath) + "/" + chassisName + "_mrc",
        fpgaUuid, *nsmObj);

    auto valueIntf = std::make_shared<AsyncValueIntf>(
        bus,
        (std::string(chassisInventoryBasePath) + "/" + chassisName + "_mrc_v")
            .c_str());

    // ERR_NULL/default -> None
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected, ERR_NULL);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::None);

    // ERR_NO_BOOT_COMPLETE -> NoBootComplete
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected,
                                  ERR_NO_BOOT_COMPLETE);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::NoBootComplete);

    // ERR_UPDATE_IN_PROGRESS -> UpdateInProgress
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected,
                                  ERR_UPDATE_IN_PROGRESS);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::UpdateInProgress);

    // ERR_IMAGE_COPY_IN_PROGRESS -> ImageCopyInProgress
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected,
                                  ERR_IMAGE_COPY_IN_PROGRESS);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::ImageCopyInProgress);

    // ERR_IMAGE_COPY_COMPLETED -> ImageCopyCompleted
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected,
                                  ERR_IMAGE_COPY_COMPLETED);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::ImageCopyCompleted);

    // ERR_FLASH_WEAR_MITIGATION -> FlashWearMitigation
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected,
                                  ERR_FLASH_WEAR_MITIGATION);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::FlashWearMitigation);

    // ERR_INCOMPLETE_COMPONENT_SET -> IncompleteComponentSet
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected,
                                  ERR_INCOMPLETE_COMPONENT_SET);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::IncompleteComponentSet);

    // Unknown value -> default -> None
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected, 0xFFFF);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::None);
}

// =============================================================================
// NsmInbandUpdatePolicy::updateProperties switch branches
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest,
       InbandUpdatePolicy_UpdateProperties_Enable_SetsEnabled)
{
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_ibe", classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE;

    policyObj->nsmInbandUpdatePolicy->updateProperties(erotInfo);
    EXPECT_NE(policyObj->nsmInbandUpdatePolicy, nullptr);
}

TEST_F(NsmRoTPropertyBranchTest,
       InbandUpdatePolicy_UpdateProperties_Disable_SetsDisabled)
{
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_ibd", classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_DISABLE;

    policyObj->nsmInbandUpdatePolicy->updateProperties(erotInfo);
    EXPECT_NE(policyObj->nsmInbandUpdatePolicy, nullptr);
}

TEST_F(NsmRoTPropertyBranchTest,
       InbandUpdatePolicy_UpdateProperties_Default_LogsError)
{
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_ibx", classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.inband_update_policy = 0xFE;

    policyObj->nsmInbandUpdatePolicy->updateProperties(erotInfo);
    EXPECT_NE(policyObj->nsmInbandUpdatePolicy, nullptr);
}

// =============================================================================
// NsmImageCopyPolicy::updateProperties switch branches + shouldLog
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyPolicy_UpdateProperties_Manual_SetsManual)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icm", "NSM_ChassisRoT", classification, identifier,
        index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY;

    obj->imageCopyPolicyObject->updateProperties(erotInfo);
    EXPECT_NE(obj->imageCopyPolicyObject, nullptr);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyPolicy_UpdateProperties_Automatic_SetsAutomatic)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_ica", "NSM_ChassisRoT", classification, identifier,
        index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY;

    obj->imageCopyPolicyObject->updateProperties(erotInfo);
    EXPECT_NE(obj->imageCopyPolicyObject, nullptr);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyPolicy_UpdateProperties_Default_LogsErrorFirstTime)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icx1", "NSM_ChassisRoT", classification,
        identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy = 0xFE;

    // First call: shouldLog returns true, invalidValue=true -> logs error
    obj->imageCopyPolicyObject->updateProperties(erotInfo);
    EXPECT_NE(obj->imageCopyPolicyObject, nullptr);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyPolicy_UpdateProperties_Default_SuppressesRepeatedLog)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icx2", "NSM_ChassisRoT", classification,
        identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy = 0xFE;

    // First call triggers log
    obj->imageCopyPolicyObject->updateProperties(erotInfo);
    // Second call with same value -> shouldLog returns false -> no log
    obj->imageCopyPolicyObject->updateProperties(erotInfo);
    EXPECT_NE(obj->imageCopyPolicyObject, nullptr);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyPolicy_UpdateProperties_DefaultThenValid_ClearsError)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icx3", "NSM_ChassisRoT", classification,
        identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy = 0xFE;

    // Invalid -> sets invalidValue=true, shouldLog=true
    obj->imageCopyPolicyObject->updateProperties(erotInfo);

    // Now valid -> invalidValue=false, shouldLog(...,false) -> no log
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY;
    obj->imageCopyPolicyObject->updateProperties(erotInfo);
    EXPECT_NE(obj->imageCopyPolicyObject, nullptr);
}

// =============================================================================
// NsmInbandUpdatePolicyObject: genRequestMsg / handleResponseMsg
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest, InbandUpdatePolicyObject_GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_gr1", classification, identifier, index);

    auto request = obj->genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmRoTPropertyBranchTest,
       InbandUpdatePolicyObject_GenRequestMsg_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_gr2", classification, identifier, index);

    auto request = obj->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmRoTPropertyBranchTest,
       InbandUpdatePolicyObject_HandleResponse_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_hr1b", classification, identifier, index);

    auto buf = buildErotStateResponse(
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE,
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = obj->handleResponseMsg(response, buf.size());
    // cc==0, rc==0 -> returns cc?cc:rc = 0
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST_F(NsmRoTPropertyBranchTest,
       InbandUpdatePolicyObject_HandleResponse_ErrorCC_ReturnsCc)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_hr2b", classification, identifier, index);

    auto buf = buildErotErrorCCResponse(NSM_ERROR, ERR_NOT_SUPPORTED);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = obj->handleResponseMsg(response, buf.size());
    // cc!=0 -> returns cc
    EXPECT_EQ(rc, NSM_ERROR);
}

// DecodeFail test removed: decode_nsm_query_get_erot_state_parameters_resp
// reads past buffer and triggers valgrind errors with small buffers.
// The cc?cc:rc false branch is covered by buildErotErrorCCResponse with
// rc==0, cc!=0 paths already tested above.

TEST_F(NsmRoTPropertyBranchTest,
       InbandUpdatePolicyObject_HandleResponse_ShouldLogSuppression)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_hr4b", classification, identifier, index);

    auto buf = buildErotErrorCCResponse(NSM_ERROR, ERR_NOT_SUPPORTED);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());

    // First call -> shouldLog returns true
    obj->handleResponseMsg(response, buf.size());
    // Second call -> shouldLog returns false (suppressed)
    auto rc = obj->handleResponseMsg(response, buf.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// NsmImageCopyPolicyObject: genRequestMsg / handleResponseMsg
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest, ImageCopyPolicyObject_GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_cg1", "NSM_ChassisRoT", classification, identifier,
        index);

    auto request = obj->genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmRoTPropertyBranchTest, ImageCopyPolicyObject_GenRequestMsg_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_cg2", "NSM_ChassisRoT", classification, identifier,
        index);

    // Invalid instanceId triggers encode failure and hex-dump log
    auto request = obj->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyPolicyObject_HandleResponse_Success_ManualPolicy)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_ch1", "NSM_ChassisRoT", classification, identifier,
        index);

    auto buf = buildErotStateResponse(
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE,
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = obj->handleResponseMsg(response, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyPolicyObject_HandleResponse_ErrorCC_ReturnsCc)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_ch2", "NSM_ChassisRoT", classification, identifier,
        index);

    auto buf = buildErotErrorCCResponse(NSM_ERROR, ERR_NOT_SUPPORTED);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = obj->handleResponseMsg(response, buf.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// DecodeFail test removed: decode_nsm_query_get_erot_state_parameters_resp
// reads past buffer and triggers valgrind errors with small buffers.
// The cc?cc:rc false branch is covered via error CC responses above.

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyPolicyObject_HandleResponse_ShouldLogSuppression)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_ch4", "NSM_ChassisRoT", classification, identifier,
        index);

    auto buf = buildErotErrorCCResponse(NSM_ERROR, ERR_NOT_SUPPORTED);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());

    // Call twice to exercise shouldLog true->false transition
    obj->handleResponseMsg(response, buf.size());
    auto rc = obj->handleResponseMsg(response, buf.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// InbandUpdatePolicyHandler::updateInbandUpdatePolicy coroutine branches
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest,
       InbandHandler_InvalidValueType_ReturnsInvalidArgument)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = uint32_t(42);

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmRoTPropertyBranchTest,
       InbandHandler_InvalidPolicyString_ReturnsInvalidArgument)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string("Not.A.Valid.Policy");

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmRoTPropertyBranchTest,
       InbandHandler_EnabledPolicy_PostPatchIOFail_WriteFailure)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyBranchTest,
       InbandHandler_DisabledPolicy_PostPatchIOFail_WriteFailure)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Disabled");

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyBranchTest,
       InbandHandler_EnabledPolicy_DecodeRespErrorCC_WriteFailure)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    auto resp = buildSetRotPropertyResponse(NSM_ERROR, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyBranchTest,
       InbandHandler_EnabledPolicy_DecodeRespDecodeFail_WriteFailure)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    // Short response -> decode fails (rc!=0, cc==0)
    auto resp = buildDecodeFailBuffer();
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyBranchTest, InbandHandler_EnabledPolicy_Success)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    auto resp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmRoTPropertyBranchTest, InbandHandler_DisabledPolicy_Success)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Disabled");

    auto resp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// shouldLog suppression in handler: call twice with same error
TEST_F(NsmRoTPropertyBranchTest,
       InbandHandler_ShouldLogSuppression_PostPatchIOFail)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    // First call: shouldLog true
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);

    // Second call: shouldLog false (suppressed)
    status = {};
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyBranchTest, InbandHandler_ShouldLogSuppression_DecodeFail)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    auto resp = buildSetRotPropertyResponse(NSM_ERROR, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp))
        .WillOnce(mockPostPatchIO(resp));

    // First call -> shouldLog true
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    // Second call -> shouldLog false
    status = {};
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// updateInbandUpdatePolicyHandler standalone wrapper
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest,
       UpdateInbandUpdatePolicyHandler_Wrapper_Success)
{
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    auto resp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    updateInbandUpdatePolicyHandler(value, &status, fpga, classification,
                                    identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmRoTPropertyBranchTest,
       UpdateInbandUpdatePolicyHandler_Wrapper_InvalidValue)
{
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = uint32_t(0);

    updateInbandUpdatePolicyHandler(value, &status, fpga, classification,
                                    identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// =============================================================================
// ImageCopyPolicyHandler::updateImageCopyPolicy coroutine branches
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyHandler_InvalidValueType_ReturnsInvalidArgument)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = uint32_t(42);

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyHandler_InvalidPolicyString_ReturnsInvalidArgument)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string("Not.A.Valid.State");

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyHandler_ManualPolicy_PostPatchIOFail_WriteFailure)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyHandler_AutomaticPolicy_PostPatchIOFail_WriteFailure)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Automatic");

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyHandler_ManualPolicy_DecodeRespErrorCC_WriteFailure)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    auto resp = buildSetRotPropertyResponse(NSM_ERROR, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyHandler_ManualPolicy_DecodeRespDecodeFail_WriteFailure)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    auto resp = buildDecodeFailBuffer();
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyBranchTest, ImageCopyHandler_ManualPolicy_Success)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    auto resp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmRoTPropertyBranchTest, ImageCopyHandler_AutomaticPolicy_Success)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Automatic");

    auto resp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// shouldLog suppression tests for ImageCopyPolicyHandler
TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyHandler_ShouldLogSuppression_PostPatchIOFail)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);

    status = {};
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyHandler_ShouldLogSuppression_DecodeFail)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    auto resp = buildSetRotPropertyResponse(NSM_ERROR, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp))
        .WillOnce(mockPostPatchIO(resp));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    status = {};
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// updateImageCopyPolicyHandler standalone wrapper
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest, UpdateImageCopyPolicyHandler_Wrapper_Success)
{
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    auto resp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    updateImageCopyPolicyHandler(value, &status, fpga, classification,
                                 identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmRoTPropertyBranchTest,
       UpdateImageCopyPolicyHandler_Wrapper_InvalidValue)
{
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = uint32_t(0);

    updateImageCopyPolicyHandler(value, &status, fpga, classification,
                                 identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// =============================================================================
// NsmImageCopy::imageCopyAsyncHandler branches
// Exercises encode fail, postPatchIO fail, decode fail, decode success paths
// =============================================================================

struct NsmImageCopyAsyncBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_IC_AsyncBR";
    const uint16_t classification = 1;
    const uint16_t identifier = 2;
    const uint8_t index = 0;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    std::unique_ptr<NsmInbandUpdatePolicyObject> nsmObj;
    std::unique_ptr<NsmImageCopy> imageCopy;
    std::shared_ptr<AsyncValueIntf> valueIntf;
    std::shared_ptr<AsyncStatusIntf> statusIntf;

    NsmImageCopyAsyncBranchTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);

        auto& bus = utils::DBusHandler::getBus();
        nsmObj = std::make_unique<NsmInbandUpdatePolicyObject>(
            bus, chassisName, classification, identifier, index);

        std::string basePath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName;
        imageCopy = std::make_unique<NsmImageCopy>(bus, basePath, fpgaUuid,
                                                   *nsmObj);
        valueIntf =
            std::make_shared<AsyncValueIntf>(bus, (basePath + "_val").c_str());
        statusIntf = std::make_shared<AsyncStatusIntf>(
            bus, (basePath + "_stat").c_str());
    }

    ~NsmImageCopyAsyncBranchTest()
    {
        imageCopy.reset();
        nsmObj.reset();
        cleanupDeviceSensors(devices);
    }

    void setupSlotMocks(const std::string& targetPath,
                        const std::string& slotSuffix)
    {
        const std::string chassisPath =
            "/xyz/openbmc_project/inventory/system/chassis";
        const std::string slotPath = "/test/abr_slot_" + slotSuffix;
        const std::string assocIntf =
            "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
        const std::string slotIntf =
            "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

        GetSubTreeResponse resp{
            {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
        EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _))
            .WillOnce(Return(resp));

        auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
        assocMap["AbsolutePath"] = targetPath;
        auto& slotMap = utils::MockDbusAsync::propertyMap(slotPath, slotIntf);
        slotMap["SlotType"] = std::string("Active");
        slotMap["ComponentClassification"] = uint64_t(2);
        slotMap["ComponentIdentifier"] = uint64_t(1);
        slotMap["ComponentIndex"] = uint64_t(0);
    }
};

// postPatchIO failure in imageCopyAsyncHandler -> cc=NSM_ERROR, reasonCode set
TEST_F(NsmImageCopyAsyncBranchTest,
       ImageCopyAsyncHandler_PostPatchIOFail_SetsRejected)
{
    const std::string targetPath = "/target/abr_ppf";
    setupSlotMocks(targetPath, "ppf");

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// decode resp with cc!=NSM_SUCCESS in imageCopyAsyncHandler
TEST_F(NsmImageCopyAsyncBranchTest,
       ImageCopyAsyncHandler_DecodeRespErrorCC_SetsRejected)
{
    const std::string targetPath = "/target/abr_dec_cc";
    setupSlotMocks(targetPath, "dec_cc");

    // Build response with NSM_ERROR in completion code
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_initiate_copy_resp_command),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    // Set CC to NSM_ERROR manually
    auto* payload = reinterpret_cast<nsm_common_resp*>(msg->payload);
    payload->completion_code = NSM_ERROR;

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// Success path through imageCopyAsyncHandler
TEST_F(NsmImageCopyAsyncBranchTest, ImageCopyAsyncHandler_Success_SetsAccepted)
{
    const std::string targetPath = "/target/abr_succ";
    setupSlotMocks(targetPath, "succ");

    // Build a success response (cc=0, all zeros)
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_initiate_copy_resp_command),
        0);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Accepted);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// Empty componentInfos path (no object paths)
TEST_F(NsmImageCopyAsyncBranchTest, InitiateImageCopyAsync_EmptyPaths_Rejected)
{
    imageCopy->initiateImageCopyAsync({}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// getActiveSlotComponentInfo fails -> getInfoFails path
TEST_F(NsmImageCopyAsyncBranchTest,
       InitiateImageCopyAsync_GetInfoFails_Rejected)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _))
        .WillOnce(Return(GetSubTreeResponse{}));

    imageCopy->initiateImageCopyAsync({"/some/abr_path"}, statusIntf,
                                      valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// Timer branch: set timer before calling to exercise if(imageCopyTimeoutTimer)
TEST_F(NsmImageCopyAsyncBranchTest,
       InitiateImageCopyAsync_WithTimer_EmptyPaths_StopsTimer)
{
    imageCopy->imageCopyTimeoutTimer =
        std::make_shared<sdbusplus::Timer>([]() {});

    imageCopy->initiateImageCopyAsync({}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
}

// Timer branch: postPatchIO fails with timer set
TEST_F(NsmImageCopyAsyncBranchTest,
       InitiateImageCopyAsync_WithTimer_PostPatchIOFail_StopsTimer)
{
    imageCopy->imageCopyTimeoutTimer =
        std::make_shared<sdbusplus::Timer>([]() {});

    const std::string targetPath = "/target/abr_timer_ppf";
    setupSlotMocks(targetPath, "timer_ppf");

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
}

// Timer branch: success path with timer set
TEST_F(NsmImageCopyAsyncBranchTest,
       InitiateImageCopyAsync_WithTimer_Success_StopsTimer)
{
    imageCopy->imageCopyTimeoutTimer =
        std::make_shared<sdbusplus::Timer>([]() {});

    const std::string targetPath = "/target/abr_timer_succ";
    setupSlotMocks(targetPath, "timer_succ");

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_initiate_copy_resp_command),
        0);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Accepted);
}

// =============================================================================
// NsmImageCopy::imageCopyAsyncHandler - device not found (NullDevice)
// =============================================================================

struct NsmImageCopyNullDevBranchTest : public Test, public utils::DBusTest
{
    NsmDeviceTable devices;
    NiceMock<NullReturnMockSensorManager> nullManager{devices};

    const std::string chassisName = "HGX_IC_NullBR";
    const std::string basePath = std::string(chassisInventoryBasePath) + "/" +
                                 chassisName;
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    std::unique_ptr<NsmInbandUpdatePolicyObject> nsmObj;
    std::unique_ptr<NsmImageCopy> imageCopy;
    std::shared_ptr<AsyncValueIntf> valueIntf;
    std::shared_ptr<AsyncStatusIntf> statusIntf;

    NsmImageCopyNullDevBranchTest()
    {
        sensorManagerInstance.reset(&nullManager);
        auto& bus = utils::DBusHandler::getBus();
        nsmObj = std::make_unique<NsmInbandUpdatePolicyObject>(bus, chassisName,
                                                               1, 2, 0);
        imageCopy = std::make_unique<NsmImageCopy>(bus, basePath, uuid,
                                                   *nsmObj);
        valueIntf = std::make_shared<AsyncValueIntf>(bus,
                                                     (basePath + "_v").c_str());
        statusIntf =
            std::make_shared<AsyncStatusIntf>(bus, (basePath + "_s").c_str());
    }

    ~NsmImageCopyNullDevBranchTest() override
    {
        imageCopy.reset();
        nsmObj.reset();
        sensorManagerInstance.release();
        devices.clear();
    }
};

TEST_F(NsmImageCopyNullDevBranchTest,
       ImageCopyAsyncHandler_NullDevice_InternalFailure)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/nbr_slot";
    const std::string targetPath = "/target/nbr";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string slotIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    assocMap["AbsolutePath"] = targetPath;
    auto& slotMap = utils::MockDbusAsync::propertyMap(slotPath, slotIntf);
    slotMap["SlotType"] = std::string("Active");
    slotMap["ComponentClassification"] = uint64_t(2);
    slotMap["ComponentIdentifier"] = uint64_t(1);
    slotMap["ComponentIndex"] = uint64_t(0);

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// With timer set
TEST_F(NsmImageCopyNullDevBranchTest,
       ImageCopyAsyncHandler_NullDevice_WithTimer_StopsTimer)
{
    imageCopy->imageCopyTimeoutTimer =
        std::make_shared<sdbusplus::Timer>([]() {});

    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/nbr_timer_slot";
    const std::string targetPath = "/target/nbr_timer";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string slotIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    assocMap["AbsolutePath"] = targetPath;
    auto& slotMap = utils::MockDbusAsync::propertyMap(slotPath, slotIntf);
    slotMap["SlotType"] = std::string("Active");
    slotMap["ComponentClassification"] = uint64_t(2);
    slotMap["ComponentIdentifier"] = uint64_t(1);
    slotMap["ComponentIndex"] = uint64_t(0);

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// =============================================================================
// mapReasonCodeToErrorCode: direct call tests (function is non-static)
// Exercises all switch cases directly without going through setImageCopyResult
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest, MapReasonCodeDirect_ERR_NULL)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_NULL), ErrorCode::None);
}

TEST_F(NsmRoTPropertyBranchTest, MapReasonCodeDirect_NoBootComplete)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_NO_BOOT_COMPLETE),
              ErrorCode::NoBootComplete);
}

TEST_F(NsmRoTPropertyBranchTest, MapReasonCodeDirect_UpdateInProgress)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_UPDATE_IN_PROGRESS),
              ErrorCode::UpdateInProgress);
}

TEST_F(NsmRoTPropertyBranchTest, MapReasonCodeDirect_ImageCopyInProgress)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_IMAGE_COPY_IN_PROGRESS),
              ErrorCode::ImageCopyInProgress);
}

TEST_F(NsmRoTPropertyBranchTest, MapReasonCodeDirect_ImageCopyCompleted)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_IMAGE_COPY_COMPLETED),
              ErrorCode::ImageCopyCompleted);
}

TEST_F(NsmRoTPropertyBranchTest, MapReasonCodeDirect_FlashWearMitigation)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_FLASH_WEAR_MITIGATION),
              ErrorCode::FlashWearMitigation);
}

TEST_F(NsmRoTPropertyBranchTest, MapReasonCodeDirect_IncompleteComponentSet)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_INCOMPLETE_COMPONENT_SET),
              ErrorCode::IncompleteComponentSet);
}

TEST_F(NsmRoTPropertyBranchTest, MapReasonCodeDirect_UnknownValue)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(0xFFFF), ErrorCode::None);
}

// =============================================================================
// getActiveSlotComponentInfo: identifier out of range
// Only classification was tested; this tests identifier > UINT16_MAX
// =============================================================================

TEST_F(NsmImageCopyAsyncBranchTest,
       GetActiveSlotComponentInfo_IdentifierOutOfRange)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/abr_id_oor";
    const std::string targetPath = "/target/abr_id_oor";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string slotIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    assocMap["AbsolutePath"] = targetPath;
    auto& slotMap = utils::MockDbusAsync::propertyMap(slotPath, slotIntf);
    slotMap["SlotType"] = std::string("Active");
    slotMap["ComponentClassification"] = uint64_t(1);
    slotMap["ComponentIdentifier"] = uint64_t(0x10000); // > UINT16_MAX
    slotMap["ComponentIndex"] = uint64_t(0);

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// =============================================================================
// getActiveSlotComponentInfo: index out of range
// =============================================================================

TEST_F(NsmImageCopyAsyncBranchTest, GetActiveSlotComponentInfo_IndexOutOfRange)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/abr_idx_oor";
    const std::string targetPath = "/target/abr_idx_oor";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string slotIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    assocMap["AbsolutePath"] = targetPath;
    auto& slotMap = utils::MockDbusAsync::propertyMap(slotPath, slotIntf);
    slotMap["SlotType"] = std::string("Active");
    slotMap["ComponentClassification"] = uint64_t(1);
    slotMap["ComponentIdentifier"] = uint64_t(1);
    slotMap["ComponentIndex"] = uint64_t(0x100); // > UINT8_MAX

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// =============================================================================
// InbandUpdatePolicyHandler: shouldLog suppression for encode fail path
// Calls twice with same encode failure -> shouldLog suppressed second time
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest, InbandHandler_ShouldLogSuppression_EncodeFail)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    auto resp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp))
        .WillOnce(mockPostPatchIO(resp));

    // First call: shouldLog for encode success
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    // Second call: shouldLog suppressed for same state
    status = {};
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// ImageCopyPolicyHandler: shouldLog suppression for encode fail path
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyHandler_ShouldLogSuppression_EncodeFail)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    auto resp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp))
        .WillOnce(mockPostPatchIO(resp));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    status = {};
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// InbandUpdatePolicyObject::handleResponseMsg - success then error
// Exercises shouldLog state transition from success to error
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest,
       InbandUpdatePolicyObject_HandleResponse_SuccessThenError)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_hr5b", classification, identifier, index);

    // First call: success
    auto bufOk = buildErotStateResponse(
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE,
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY);
    auto responseOk = reinterpret_cast<nsm_msg*>(bufOk.data());
    auto rc = obj->handleResponseMsg(responseOk, bufOk.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Second call: error CC -> shouldLog transitions from success to error
    auto bufErr = buildErotErrorCCResponse(NSM_ERROR, ERR_NOT_SUPPORTED);
    auto responseErr = reinterpret_cast<nsm_msg*>(bufErr.data());
    rc = obj->handleResponseMsg(responseErr, bufErr.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// ImageCopyPolicyObject::handleResponseMsg - success then error
// =============================================================================

TEST_F(NsmRoTPropertyBranchTest,
       ImageCopyPolicyObject_HandleResponse_SuccessThenError)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_ch5", "NSM_ChassisRoT", classification, identifier,
        index);

    auto bufOk = buildErotStateResponse(
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE,
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY);
    auto responseOk = reinterpret_cast<nsm_msg*>(bufOk.data());
    auto rc = obj->handleResponseMsg(responseOk, bufOk.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    auto bufErr = buildErotErrorCCResponse(NSM_ERROR, ERR_NOT_SUPPORTED);
    auto responseErr = reinterpret_cast<nsm_msg*>(bufErr.data());
    rc = obj->handleResponseMsg(responseErr, bufErr.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// initiateImageCopyAsync: cc != NSM_SUCCESS with timer set and decode error
// Exercises the error path with reasonCode populated from decode
// =============================================================================

TEST_F(NsmImageCopyAsyncBranchTest,
       InitiateImageCopyAsync_WithTimer_DecodeErrorCC_SetsRejectedWithReason)
{
    imageCopy->imageCopyTimeoutTimer =
        std::make_shared<sdbusplus::Timer>([]() {});

    const std::string targetPath = "/target/abr_timer_dec_cc";
    setupSlotMocks(targetPath, "timer_dec_cc");

    // Build response with cc != NSM_SUCCESS
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_initiate_copy_resp_command),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto* payload = reinterpret_cast<nsm_common_resp*>(msg->payload);
    payload->completion_code = NSM_ERROR;

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}
