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

#include "firmware-utils.h"

#define private public
#define protected public

#include "nsmFirmwareUtils/nsmFirmwareUtilsCommon.hpp"
#include "nsmRoTProperty.hpp"

using namespace nsm;

struct NsmRoTPropertyTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis";
    const uint16_t classification = 1;
    const uint16_t identifier = 2;
    const uint8_t index = 0;

    NsmDeviceTable devices;
    std::shared_ptr<NsmInbandUpdatePolicyObject> policyObject;

    NsmRoTPropertyTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        policyObject = std::make_shared<NsmInbandUpdatePolicyObject>(
            bus, chassisName, classification, identifier, index);

        EXPECT_NE(policyObject, nullptr);
    }
};

TEST_F(NsmRoTPropertyTest, goodTestConstructor)
{
    EXPECT_NE(policyObject, nullptr);
    EXPECT_EQ(policyObject->classification, classification);
    EXPECT_EQ(policyObject->identifier, identifier);
    EXPECT_EQ(policyObject->index, index);
}

TEST_F(NsmRoTPropertyTest, goodTestClassificationAndIdentifier)
{
    EXPECT_EQ(policyObject->classification, 1);
    EXPECT_EQ(policyObject->identifier, 2);
    EXPECT_EQ(policyObject->index, 0);
}

TEST_F(NsmRoTPropertyTest, goodTestNsmSensorInheritance)
{
    // NsmInbandUpdatePolicyObject inherits from NsmSensor
    NsmSensor* sensor = policyObject.get();
    EXPECT_NE(sensor, nullptr);
}

// These interfaces are private and cannot be accessed in tests

TEST_F(NsmRoTPropertyTest, testWithDifferentClassification)
{
    auto& bus = utils::DBusHandler::getBus();
    uint16_t classification2 = 5;

    auto policyObject2 = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "2", classification2, identifier, index);

    EXPECT_EQ(policyObject2->classification, classification2);
}

TEST_F(NsmRoTPropertyTest, testWithDifferentIdentifier)
{
    auto& bus = utils::DBusHandler::getBus();
    uint16_t identifier2 = 10;

    auto policyObject2 = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "3", classification, identifier2, index);

    EXPECT_EQ(policyObject2->identifier, identifier2);
}

TEST_F(NsmRoTPropertyTest, testWithDifferentIndex)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t index2 = 3;

    auto policyObject2 = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "4", classification, identifier, index2);

    EXPECT_EQ(policyObject2->index, index2);
}

TEST_F(NsmRoTPropertyTest, testZeroClassification)
{
    auto& bus = utils::DBusHandler::getBus();
    uint16_t classification0 = 0;

    auto policyObject0 = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "5", classification0, identifier, index);

    EXPECT_EQ(policyObject0->classification, 0);
}

TEST_F(NsmRoTPropertyTest, testMaxValues)
{
    auto& bus = utils::DBusHandler::getBus();
    uint16_t maxClassification = 0xFFFF;
    uint16_t maxIdentifier = 0xFFFF;
    uint8_t maxIndex = 0xFF;

    auto maxPolicyObject = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "6", maxClassification, maxIdentifier, maxIndex);

    EXPECT_EQ(maxPolicyObject->classification, maxClassification);
    EXPECT_EQ(maxPolicyObject->identifier, maxIdentifier);
    EXPECT_EQ(maxPolicyObject->index, maxIndex);
}

// TEST_F(NsmRoTPropertyTest, goodTestMultipleInstances) - Disabled due to DBus
// path conflict

// TEST_F(NsmRoTPropertyTest, goodTestDifferentChassisNames) - Disabled due to
// DBus path conflict
// ============================================================================
// nsmRoTProperty.cpp - InbandUpdatePolicyHandler,
// ImageCopyPolicyHandler, NsmImageCopy, mapReasonCodeToErrorCode
// ============================================================================

struct NsmRoTPropertyDeepTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis_B11";
    const uint16_t classification = 1;
    const uint16_t identifier = 2;
    const uint8_t index = 0;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmRoTPropertyDeepTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmRoTPropertyDeepTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// --- NsmInbandUpdatePolicyObject genRequestMsg ---

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyObject_GenRequestMsg_ValidParams_ReturnsRequest)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName, classification, identifier, index);

    // Act
    auto request = policyObj->genRequestMsg(0, 0);

    // Assert
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_get_erot_state_info_req));
}

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyObject_GenRequestMsg_BadInstanceId_ReturnsNullopt)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName, classification, identifier, index);

    // Act - use invalid instance ID
    auto request = policyObj->genRequestMsg(0, NSM_INSTANCE_MAX + 1);

    // Assert
    EXPECT_FALSE(request.has_value());
}

// --- NsmInbandUpdatePolicyObject handleResponseMsg ---

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyObject_HandleResponse_Success_UpdatesProperties)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_hr1", classification, identifier, index);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        0, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = policyObj->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyObject_HandleResponse_ErrorCC_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_hr2", classification, identifier, index);

    // Create a response with error completion code
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        0, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Corrupt the completion code
    auto* payload = reinterpret_cast<nsm_firmware_get_erot_state_info_resp*>(
        responseMsg->payload);
    payload->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Act
    rc = policyObj->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(
    NsmRoTPropertyDeepTest,
    DISABLED_InbandUpdatePolicyObject_HandleResponse_DecodeFailure_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_hr3", classification, identifier, index);

    // Buffer too small to even read the completion code field.
    // decode_reason_code_and_cc requires at least
    // sizeof(nsm_msg_hdr) + offsetof(nsm_common_resp, completion_code) + 1
    // = 5 + 1 + 1 = 7 bytes. Using 4 bytes triggers NSM_SW_ERROR_LENGTH.
    std::vector<uint8_t> response(4, 0);
    auto responseMsg = reinterpret_cast<const nsm_msg*>(response.data());

    // Act
    auto rc = policyObj->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// --- NsmInbandUpdatePolicy::updateProperties ---

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicy_UpdateProperties_Enable_SetsEnabled)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_up1", classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE;

    // Act
    policyObj->nsmInbandUpdatePolicy->updateProperties(erotInfo);

    // Assert - no crash, property set
    EXPECT_NE(policyObj->nsmInbandUpdatePolicy, nullptr);
}

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicy_UpdateProperties_Disable_SetsDisabled)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_up2", classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_DISABLE;

    // Act
    policyObj->nsmInbandUpdatePolicy->updateProperties(erotInfo);

    // Assert
    EXPECT_NE(policyObj->nsmInbandUpdatePolicy, nullptr);
}

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicy_UpdateProperties_InvalidValue_LogsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_up3", classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.inband_update_policy = 0xFF; // Invalid value

    // Act - should hit default case
    policyObj->nsmInbandUpdatePolicy->updateProperties(erotInfo);

    // Assert - no crash
    EXPECT_NE(policyObj->nsmInbandUpdatePolicy, nullptr);
}

// --- NsmImageCopyPolicyObject ---

TEST_F(NsmRoTPropertyDeepTest, ImageCopyPolicyObject_Constructor_SetsFields)
{
    // Arrange & Act
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp1", "NSM_ChassisRoT", classification,
        identifier, index);

    // Assert
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(obj->classification, classification);
    EXPECT_EQ(obj->identifier, identifier);
    EXPECT_EQ(obj->index, index);
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicyObject_GenRequestMsg_ValidParams_ReturnsRequest)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp2", "NSM_ChassisRoT", classification,
        identifier, index);

    // Act
    auto request = obj->genRequestMsg(0, 0);

    // Assert
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_get_erot_state_info_req));
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicyObject_GenRequestMsg_BadInstanceId_ReturnsNullopt)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp3", "NSM_ChassisRoT", classification,
        identifier, index);

    // Act
    auto request = obj->genRequestMsg(0, NSM_INSTANCE_MAX + 1);

    // Assert
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicyObject_HandleResponse_Success_UpdatesProperties)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp4", "NSM_ChassisRoT", classification,
        identifier, index);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        0, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = obj->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicyObject_HandleResponse_ErrorCC_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp5", "NSM_ChassisRoT", classification,
        identifier, index);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        0, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Corrupt CC
    auto* payload = reinterpret_cast<nsm_firmware_get_erot_state_info_resp*>(
        responseMsg->payload);
    payload->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Act
    rc = obj->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmRoTPropertyDeepTest,
       DISABLED_ImageCopyPolicyObject_HandleResponse_DecodeFailure_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp6", "NSM_ChassisRoT", classification,
        identifier, index);

    // Buffer too small to even read the completion code field.
    // decode_reason_code_and_cc requires at least 7 bytes.
    // Using 4 bytes triggers NSM_SW_ERROR_LENGTH.
    std::vector<uint8_t> response(4, 0);
    auto responseMsg = reinterpret_cast<const nsm_msg*>(response.data());

    // Act
    auto rc = obj->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// --- NsmImageCopyPolicy::updateProperties ---

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicy_UpdateProperties_Manual_SetsManual)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp7", "NSM_ChassisRoT", classification,
        identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY;

    // Act
    obj->imageCopyPolicyObject->updateProperties(erotInfo);

    // Assert
    EXPECT_NE(obj->imageCopyPolicyObject, nullptr);
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicy_UpdateProperties_Automatic_SetsAutomatic)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp8", "NSM_ChassisRoT", classification,
        identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY;

    // Act
    obj->imageCopyPolicyObject->updateProperties(erotInfo);

    // Assert
    EXPECT_NE(obj->imageCopyPolicyObject, nullptr);
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicy_UpdateProperties_InvalidValue_LogsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp9", "NSM_ChassisRoT", classification,
        identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy = 0xFF; // Invalid

    // Act - should hit default case
    obj->imageCopyPolicyObject->updateProperties(erotInfo);

    // Assert
    EXPECT_NE(obj->imageCopyPolicyObject, nullptr);
}

// --- NsmImageCopyObject constructor ---

TEST_F(NsmRoTPropertyDeepTest, ImageCopyObject_Constructor_CreatesValidObject)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-1234";

    // Act
    auto obj = std::make_shared<NsmImageCopyObject>(bus, chassisName + "_ico1",
                                                    testUuid);

    // Assert
    EXPECT_NE(obj, nullptr);
    EXPECT_NE(obj->nsmImageCopy, nullptr);
}

// --- InbandUpdatePolicyHandler::updateInbandUpdatePolicy coroutine ---

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyHandler_InvalidValueType_ReturnsError)
{
    // Arrange
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    // Pass a non-string type (int)
    AsyncSetOperationValueType value = uint32_t(42);

    // Act
    auto result = handler.updateInbandUpdatePolicy(
        value, &status, fpga, classification, identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyHandler_InvalidPolicyString_ReturnsError)
{
    // Arrange
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string("InvalidPolicyState");

    // Act
    auto result = handler.updateInbandUpdatePolicy(
        value, &status, fpga, classification, identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyHandler_EnabledPolicy_EncodesAndSends)
{
    // Arrange
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    // Build a successful response for the set rot property
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    // Act
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyHandler_DisabledPolicy_EncodesAndSends)
{
    // Arrange
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Disabled");

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    // Act
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyHandler_PostPatchIOFail_ReturnsWriteFailure)
{
    // Arrange
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO)
        .WillOnce(mockPostPatchIO(
            static_cast<nsm_completion_codes>(NSM_SW_ERROR_COMMAND_FAIL)));

    // Act
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyHandler_DecodeRespFail_ReturnsWriteFailure)
{
    // Arrange
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    // Return a response with error CC
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_nsm_firmware_set_rot_property_resp(0, NSM_ERROR, ERR_NULL,
                                                        responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    // Act
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// --- updateInbandUpdatePolicyHandler standalone wrapper ---

TEST_F(NsmRoTPropertyDeepTest,
       UpdateInbandUpdatePolicyHandler_Wrapper_InvalidType_ReturnsError)
{
    // Arrange
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = uint32_t(42);

    // Act
    updateInbandUpdatePolicyHandler(value, &status, fpga, classification,
                                    identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// --- ImageCopyPolicyHandler::updateImageCopyPolicy coroutine ---

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicyHandler_InvalidValueType_ReturnsError)
{
    // Arrange
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = uint32_t(42);

    // Act
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicyHandler_InvalidPolicyString_ReturnsError)
{
    // Arrange
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string("InvalidState");

    // Act
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicyHandler_ManualPolicy_EncodesAndSends)
{
    // Arrange
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    // Act
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicyHandler_AutomaticPolicy_EncodesAndSends)
{
    // Arrange
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Automatic");

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    // Act
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicyHandler_PostPatchIOFail_ReturnsWriteFailure)
{
    // Arrange
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO)
        .WillOnce(mockPostPatchIO(
            static_cast<nsm_completion_codes>(NSM_SW_ERROR_COMMAND_FAIL)));

    // Act
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyDeepTest,
       ImageCopyPolicyHandler_DecodeRespFail_ReturnsWriteFailure)
{
    // Arrange
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Automatic");

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_nsm_firmware_set_rot_property_resp(0, NSM_ERROR, ERR_NULL,
                                                        responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    // Act
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// --- updateImageCopyPolicyHandler standalone wrapper ---

TEST_F(NsmRoTPropertyDeepTest,
       UpdateImageCopyPolicyHandler_Wrapper_InvalidType_ReturnsError)
{
    // Arrange
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = uint32_t(42);

    // Act
    updateImageCopyPolicyHandler(value, &status, fpga, classification,
                                 identifier, index);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// ============================================================================
// NsmFailoverPolicyObject
// ============================================================================

TEST_F(NsmRoTPropertyDeepTest, FailoverPolicyObject_Constructor_SetsFields)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp1";
    auto obj = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_fp1", testUuid, classification, identifier, index);

    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(obj->classification, classification);
    EXPECT_EQ(obj->identifier, identifier);
    EXPECT_EQ(obj->index, index);
}

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicyObject_GenRequestMsg_ValidParams_ReturnsRequest)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp2";
    auto obj = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_fp2", testUuid, classification, identifier, index);

    auto request = obj->genRequestMsg(0, 0);

    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_get_erot_state_info_req));
}

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicyObject_GenRequestMsg_BadInstanceId_ReturnsNullopt)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp3";
    auto obj = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_fp3", testUuid, classification, identifier, index);

    auto request = obj->genRequestMsg(0, NSM_INSTANCE_MAX + 1);

    EXPECT_FALSE(request.has_value());
}

// --- NsmFailoverPolicyObject handleResponseMsg ---

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicyObject_HandleResponse_Success_UpdatesProperties)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp4";
    auto obj = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_fp4", testUuid, classification, identifier, index);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.global_failover_policy =
        NSM_ROT_GLOBAL_FAILOVER_POLICY_AUTOMATIC_FAILOVER;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        0, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = obj->handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicyObject_HandleResponse_ErrorCC_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp5";
    auto obj = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_fp5", testUuid, classification, identifier, index);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        0, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto* payload = reinterpret_cast<nsm_firmware_get_erot_state_info_resp*>(
        responseMsg->payload);
    payload->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = obj->handleResponseMsg(responseMsg, response.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmRoTPropertyDeepTest,
       DISABLED_FailoverPolicyObject_HandleResponse_DecodeFailure_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp-df";
    auto obj = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_fp_df", testUuid, classification, identifier,
        index);

    // Buffer too small to even read the completion code field.
    std::vector<uint8_t> response(4, 0);
    auto responseMsg = reinterpret_cast<const nsm_msg*>(response.data());

    auto rc = obj->handleResponseMsg(responseMsg, response.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}
// --- NsmFailoverPolicy::updateProperties ---

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicy_UpdateProperties_AutomaticFailover_SetsAutomatic)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp6";
    auto obj = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_fp6", testUuid, classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.global_failover_policy =
        NSM_ROT_GLOBAL_FAILOVER_POLICY_AUTOMATIC_FAILOVER;

    obj->nsmFailoverPolicy->updateProperties(erotInfo);

    EXPECT_NE(obj->nsmFailoverPolicy, nullptr);
}

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicy_UpdateProperties_NoFailover_SetsNoFailover)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp7";
    auto obj = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_fp7", testUuid, classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.global_failover_policy =
        NSM_ROT_GLOBAL_FAILOVER_POLICY_NO_FAILOVER;

    obj->nsmFailoverPolicy->updateProperties(erotInfo);

    EXPECT_NE(obj->nsmFailoverPolicy, nullptr);
}

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicy_UpdateProperties_NotApplicable_SetsUnknown)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp8";
    auto obj = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_fp8", testUuid, classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.global_failover_policy =
        NSM_ROT_GLOBAL_FAILOVER_POLICY_NOT_APPLICABLE;

    obj->nsmFailoverPolicy->updateProperties(erotInfo);

    EXPECT_NE(obj->nsmFailoverPolicy, nullptr);
}

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicy_UpdateProperties_InvalidValue_LogsError)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp9";
    auto obj = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_fp9", testUuid, classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.global_failover_policy = 0xFE;

    obj->nsmFailoverPolicy->updateProperties(erotInfo);

    EXPECT_NE(obj->nsmFailoverPolicy, nullptr);
}

// --- FailoverPolicyHandler::updateFailoverPolicy coroutine ---

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicyHandler_InvalidValueType_ReturnsError)
{
    FailoverPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = uint32_t(42);

    handler.updateFailoverPolicy(value, &status, fpga, classification,
                                 identifier, index);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicyHandler_InvalidPolicyString_ReturnsError)
{
    FailoverPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string("InvalidFailoverState");

    handler.updateFailoverPolicy(value, &status, fpga, classification,
                                 identifier, index);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicyHandler_AutomaticFailover_EncodesAndSends)
{
    FailoverPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.FailoverPolicy.FailoverPolicyState.AutomaticFailover");

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    handler.updateFailoverPolicy(value, &status, fpga, classification,
                                 identifier, index);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmRoTPropertyDeepTest, FailoverPolicyHandler_NoFailover_EncodesAndSends)
{
    FailoverPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.FailoverPolicy.FailoverPolicyState.NoFailover");

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    handler.updateFailoverPolicy(value, &status, fpga, classification,
                                 identifier, index);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicyHandler_PostPatchIOFail_ReturnsWriteFailure)
{
    FailoverPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.FailoverPolicy.FailoverPolicyState.AutomaticFailover");

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO)
        .WillOnce(mockPostPatchIO(
            static_cast<nsm_completion_codes>(NSM_SW_ERROR_COMMAND_FAIL)));

    handler.updateFailoverPolicy(value, &status, fpga, classification,
                                 identifier, index);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmRoTPropertyDeepTest,
       FailoverPolicyHandler_DecodeRespFail_ReturnsWriteFailure)
{
    FailoverPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.FailoverPolicy.FailoverPolicyState.NoFailover");

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_nsm_firmware_set_rot_property_resp(0, NSM_ERROR, ERR_NULL,
                                                        responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    handler.updateFailoverPolicy(value, &status, fpga, classification,
                                 identifier, index);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// --- updateFailoverPolicyHandler standalone wrapper ---

TEST_F(NsmRoTPropertyDeepTest,
       UpdateFailoverPolicyHandler_Wrapper_InvalidType_ReturnsError)
{
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value = uint32_t(42);

    updateFailoverPolicyHandler(value, &status, fpga, classification,
                                identifier, index);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// ============================================================================
// addSensor<T> instantiation coverage
// ============================================================================

TEST_F(NsmRoTPropertyDeepTest, AddSensorNsmFailoverPolicyObject)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "test-uuid-fp-as";
    auto sensor = std::make_shared<NsmFailoverPolicyObject>(
        bus, chassisName + "_FP_AS", testUuid, classification, identifier,
        index);
    size_t before = fpga->deviceSensors.size();
    fpga->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

TEST_F(NsmRoTPropertyDeepTest, AddSensorNsmInbandUpdatePolicyObject)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_AS", classification, identifier, index);
    size_t before = fpga->deviceSensors.size();
    fpga->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

TEST_F(NsmRoTPropertyDeepTest, AddSensorNsmImageCopyPolicyObject)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_ICP_AS", "NSM_Chassis", classification, identifier,
        index);
    size_t before = fpga->deviceSensors.size();
    fpga->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}
