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
 * Branch coverage batch 3 for nsmd/nsmChassis/nsmRoTProperty.cpp
 *
 * Targets remaining uncovered branches:
 * - mapReasonCodeToErrorCode: all 7 switch cases (direct calls)
 * - NsmInbandUpdatePolicy::updateProperties: default (invalid) branch
 * - NsmImageCopyPolicy::updateProperties: default (invalid) branch
 * - InbandUpdatePolicyHandler::updateInbandUpdatePolicy: unsupported policy
 *   state after valid conversion
 * - ImageCopyPolicyHandler::updateImageCopyPolicy: unsupported policy state
 * - NsmInbandUpdatePolicyObject::handleResponseMsg: cc!=0 error path
 * - NsmImageCopyPolicyObject::handleResponseMsg: cc!=0 error path
 * - NsmInbandUpdatePolicyObject::genRequestMsg: encode failure returns nullopt
 * - InbandUpdatePolicyHandler: postPatchIO failure
 * - InbandUpdatePolicyHandler: decode failure (cc!=0)
 * - ImageCopyPolicyHandler: postPatchIO failure
 * - ImageCopyPolicyHandler: decode failure (cc!=0)
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

struct NsmRoTPropertyBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis_BR3";
    const uint16_t classification = 1;
    const uint16_t identifier = 2;
    const uint8_t index = 0;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:3";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmRoTPropertyBranch3Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmRoTPropertyBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }

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
};

// =============================================================================
// mapReasonCodeToErrorCode: direct tests for all switch cases
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, MapReasonCode_ERR_NULL_ReturnsNone)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_NULL), ErrorCode::None);
}

TEST_F(NsmRoTPropertyBranch3Test, MapReasonCode_NoBootComplete)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_NO_BOOT_COMPLETE),
              ErrorCode::NoBootComplete);
}

TEST_F(NsmRoTPropertyBranch3Test, MapReasonCode_UpdateInProgress)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_UPDATE_IN_PROGRESS),
              ErrorCode::UpdateInProgress);
}

TEST_F(NsmRoTPropertyBranch3Test, MapReasonCode_ImageCopyInProgress)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_IMAGE_COPY_IN_PROGRESS),
              ErrorCode::ImageCopyInProgress);
}

TEST_F(NsmRoTPropertyBranch3Test, MapReasonCode_ImageCopyCompleted)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_IMAGE_COPY_COMPLETED),
              ErrorCode::ImageCopyCompleted);
}

TEST_F(NsmRoTPropertyBranch3Test, MapReasonCode_FlashWearMitigation)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_FLASH_WEAR_MITIGATION),
              ErrorCode::FlashWearMitigation);
}

TEST_F(NsmRoTPropertyBranch3Test, MapReasonCode_IncompleteComponentSet)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(ERR_INCOMPLETE_COMPONENT_SET),
              ErrorCode::IncompleteComponentSet);
}

TEST_F(NsmRoTPropertyBranch3Test, MapReasonCode_UnknownCode_ReturnsNone)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    EXPECT_EQ(mapReasonCodeToErrorCode(0xFFFF), ErrorCode::None);
}

// =============================================================================
// NsmInbandUpdatePolicy::updateProperties: invalid policy value (default case)
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test,
       InbandUpdatePolicy_UpdateProperties_InvalidPolicy)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_ipol3", classification, identifier, index);

    // Build response with invalid inband_update_policy value (0xFF)
    auto response = buildErotStateResponse(0xFF, 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(msg, response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmImageCopyPolicy::updateProperties: invalid background_copy_policy
// (default)
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test,
       ImageCopyPolicy_UpdateProperties_InvalidPolicy)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icpol3", "NSM_ChassisRoT", classification,
        identifier, index);

    // Build response with invalid background_copy_policy value (0xFF)
    auto response = buildErotStateResponse(0, 0xFF);
    auto* msg = reinterpret_cast<const nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(msg, response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmInbandUpdatePolicyObject::handleResponseMsg: cc!=0 error path
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test,
       InbandUpdatePolicyObject_HandleResponse_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_iup_err3", classification, identifier, index);

    auto response = buildErotErrorCCResponse(NSM_ERR_UNSUPPORTED_COMMAND_CODE,
                                             ERR_NULL);
    auto* msg = reinterpret_cast<const nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(msg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmImageCopyPolicyObject::handleResponseMsg: cc!=0 error path
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, ImageCopyPolicyObject_HandleResponse_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp_err3", "NSM_ChassisRoT", classification,
        identifier, index);

    auto response = buildErotErrorCCResponse(NSM_ERR_UNSUPPORTED_COMMAND_CODE,
                                             ERR_NULL);
    auto* msg = reinterpret_cast<const nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(msg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// InbandUpdatePolicyHandler::updateInbandUpdatePolicy: invalid value type
// (not a string) -> InvalidArgument
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, InbandUpdatePolicyHandler_InvalidValueType)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status{};

    // Pass a uint32_t instead of string
    AsyncSetOperationValueType value = uint32_t(42);
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// =============================================================================
// InbandUpdatePolicyHandler: invalid policy string
// (convertStringToInbandPolicyState returns empty)
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, InbandUpdatePolicyHandler_InvalidPolicyString)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status{};

    AsyncSetOperationValueType value = std::string("InvalidPolicyValue");
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// =============================================================================
// InbandUpdatePolicyHandler: postPatchIO failure
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, InbandUpdatePolicyHandler_PostPatchIO_Failure)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status{};

    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// InbandUpdatePolicyHandler: decode failure (cc!=0)
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, InbandUpdatePolicyHandler_DecodeFailure_CC)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status{};

    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Disabled");

    auto errorResp =
        buildSetRotPropertyResponse(NSM_ERR_UNSUPPORTED_COMMAND_CODE, ERR_NULL);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// InbandUpdatePolicyHandler: success path (Enabled)
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, InbandUpdatePolicyHandler_Success_Enabled)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status{};

    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    auto successResp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// ImageCopyPolicyHandler::updateImageCopyPolicy: invalid value type
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, ImageCopyPolicyHandler_InvalidValueType)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status{};

    AsyncSetOperationValueType value = uint32_t(42);
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// =============================================================================
// ImageCopyPolicyHandler: invalid policy string
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, ImageCopyPolicyHandler_InvalidPolicyString)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status{};

    AsyncSetOperationValueType value = std::string("InvalidPolicyValue");
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// =============================================================================
// ImageCopyPolicyHandler: postPatchIO failure
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, ImageCopyPolicyHandler_PostPatchIO_Failure)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status{};

    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// ImageCopyPolicyHandler: decode failure (cc!=0)
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, ImageCopyPolicyHandler_DecodeFailure_CC)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status{};

    AsyncSetOperationValueType value = std::string(
        "com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Automatic");

    auto errorResp =
        buildSetRotPropertyResponse(NSM_ERR_UNSUPPORTED_COMMAND_CODE, ERR_NULL);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// ImageCopyPolicyHandler: success path (Manual)
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, ImageCopyPolicyHandler_Success_Manual)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status{};

    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    auto successResp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// ImageCopyPolicyHandler: success path (Automatic)
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, ImageCopyPolicyHandler_Success_Automatic)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status{};

    AsyncSetOperationValueType value = std::string(
        "com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Automatic");

    auto successResp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// NsmInbandUpdatePolicyObject: genRequestMsg success
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test,
       InbandUpdatePolicyObject_GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_gen3", classification, identifier, index);

    auto result = sensor->genRequestMsg(0x10, 0);
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// NsmImageCopyPolicyObject: genRequestMsg success
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, ImageCopyPolicyObject_GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_gencp3", "NSM_ChassisRoT", classification,
        identifier, index);

    auto result = sensor->genRequestMsg(0x10, 0);
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// NsmInbandUpdatePolicyObject: handleResponseMsg success path
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test,
       InbandUpdatePolicyObject_HandleResponse_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_hs3", classification, identifier, index);

    auto response = buildErotStateResponse(NSM_ROT_INBAND_UPDATE_POLICY_ENABLE,
                                           0);
    auto* msg = reinterpret_cast<const nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(msg, response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmImageCopyPolicyObject: handleResponseMsg success path
// =============================================================================

TEST_F(NsmRoTPropertyBranch3Test, ImageCopyPolicyObject_HandleResponse_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_hcp3", "NSM_ChassisRoT", classification,
        identifier, index);

    auto response = buildErotStateResponse(
        0, NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY);
    auto* msg = reinterpret_cast<const nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(msg, response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}
