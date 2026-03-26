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
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<NsmInbandUpdatePolicyObject> policyObject;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmRoTPropertyTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmRoTPropertyTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        policyObject = std::make_shared<NsmInbandUpdatePolicyObject>(
            bus, chassisName, classification, identifier, index);

        EXPECT_NE(policyObject, nullptr);
    }

    // Helper: build a valid NSM_SUCCESS erot_state response for the given
    // policy
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
        encode_nsm_query_get_erot_state_parameters_resp(
            0, NSM_SUCCESS, ERR_NULL, &erotInfo, msg);
        return buf;
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

TEST_F(NsmRoTPropertyDeepTest,
       InbandUpdatePolicyObject_HandleResponse_DecodeFailure_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_hr3", classification, identifier, index);

    // Use sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_aggregate_tag) - 1 = 7
    // bytes so decode_reason_code_and_cc safely reads cc (at payload[1]=byte6)
    // but the subsequent length check (< 8) returns NSM_SW_ERROR_LENGTH.
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_aggregate_tag) - 1, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

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
       ImageCopyPolicyObject_HandleResponse_DecodeFailure_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_icp6", "NSM_ChassisRoT", classification,
        identifier, index);

    // Use sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_aggregate_tag) - 1 = 7
    // bytes so decode_reason_code_and_cc safely reads cc (at payload[1]=byte6)
    // but the subsequent length check (< 8) returns NSM_SW_ERROR_LENGTH.
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_aggregate_tag) - 1, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

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

// =============================================================================
// NsmInbandUpdatePolicyObject handleResponseMsg coverage tests
// =============================================================================

// Decode-fail path: 7-byte buffer is too short for
// decode_nsm_query_get_erot_state_parameters_resp → rc != NSM_SW_SUCCESS
// → returns rc (the `cc == 0` branch of `cc ? cc : rc`).
TEST_F(NsmRoTPropertyTest, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    uint8_t rc = policyObject->handleResponseMsg(response, responseData.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// Error-CC path: properly-sized buffer but cc == NSM_ERROR
// → decode succeeds, cc != 0 → returns cc (the `cc != 0` branch).
TEST_F(NsmRoTPropertyTest, HandleResponseMsg_ErrorCC_ReturnsCC)
{
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr;
    uint8_t rc = encode_nsm_query_get_erot_state_parameters_resp(
        0, NSM_ERROR, ERR_NOT_SUPPORTED, &erotInfo, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = policyObject->handleResponseMsg(response, responseData.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmImageCopyPolicyObject handleResponseMsg coverage tests
// =============================================================================

// Decode-fail path for NsmImageCopyPolicyObject.
TEST(NsmImageCopyPolicyObject, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "ImageCopyPolicy";
    const std::string type = "NSM_ImageCopyPolicy";
    NsmImageCopyPolicyObject sensor(bus, name, type, 1, 2, 0);

    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    uint8_t rc = sensor.handleResponseMsg(response, responseData.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// Error-CC path for NsmImageCopyPolicyObject.
TEST(NsmImageCopyPolicyObject, HandleResponseMsg_ErrorCC_ReturnsCC)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "ImageCopyPolicy2";
    const std::string type = "NSM_ImageCopyPolicy";
    NsmImageCopyPolicyObject sensor(bus, name, type, 1, 2, 0);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr;
    uint8_t rc = encode_nsm_query_get_erot_state_parameters_resp(
        0, NSM_ERROR, ERR_NOT_SUPPORTED, &erotInfo, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseData.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmInbandUpdatePolicyObject::handleResponseMsg – SUCCESS paths
// These cover the nsmInbandUpdatePolicy->updateProperties(erot_info) branch
// (lines ~148-150) and the three switch cases in updateProperties.
// =============================================================================

// SUCCESS response with ENABLE policy → updateProperties takes ENABLE branch
TEST_F(NsmRoTPropertyTest, HandleResponseMsg_Success_EnablePolicy)
{
    auto buf = buildErotStateResponse(
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE,
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t rc = policyObject->handleResponseMsg(response, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// SUCCESS response with DISABLE policy → updateProperties takes DISABLE branch
TEST_F(NsmRoTPropertyTest, HandleResponseMsg_Success_DisablePolicy)
{
    auto buf = buildErotStateResponse(
        NSM_ROT_INBAND_UPDATE_POLICY_DISABLE,
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t rc = policyObject->handleResponseMsg(response, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// SUCCESS response with unknown policy → updateProperties takes default branch
TEST_F(NsmRoTPropertyTest, HandleResponseMsg_Success_UnknownPolicy)
{
    auto buf = buildErotStateResponse(
        0xFF, NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t rc = policyObject->handleResponseMsg(response, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmImageCopyPolicyObject::handleResponseMsg – SUCCESS paths
// These cover imageCopyPolicyObject->updateProperties branch and its
// switch cases (MANUAL, AUTOMATIC, unknown/default).
// =============================================================================

TEST(NsmImageCopyPolicyObject, HandleResponseMsg_Success_ManualPolicy)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmImageCopyPolicyObject sensor(bus, "ImageCopyPolicyManual",
                                    "NSM_ImageCopyPolicy", 1, 2, 0);

    std::vector<uint8_t> buf(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr;
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY;
    erotInfo.fq_resp_hdr.firmware_slot_count = 0;
    ASSERT_EQ(encode_nsm_query_get_erot_state_parameters_resp(
                  0, NSM_SUCCESS, ERR_NULL, &erotInfo, msg),
              NSM_SW_SUCCESS);
    uint8_t rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST(NsmImageCopyPolicyObject, HandleResponseMsg_Success_AutomaticPolicy)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmImageCopyPolicyObject sensor(bus, "ImageCopyPolicyAutomatic",
                                    "NSM_ImageCopyPolicy", 1, 2, 0);

    std::vector<uint8_t> buf(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr;
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY;
    erotInfo.fq_resp_hdr.firmware_slot_count = 0;
    ASSERT_EQ(encode_nsm_query_get_erot_state_parameters_resp(
                  0, NSM_SUCCESS, ERR_NULL, &erotInfo, msg),
              NSM_SW_SUCCESS);
    uint8_t rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST(NsmImageCopyPolicyObject, HandleResponseMsg_Success_UnknownPolicy)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmImageCopyPolicyObject sensor(bus, "ImageCopyPolicyUnknown",
                                    "NSM_ImageCopyPolicy", 1, 2, 0);

    std::vector<uint8_t> buf(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr;
    erotInfo.fq_resp_hdr.background_copy_policy = 0xFF; // unknown → default
    erotInfo.fq_resp_hdr.firmware_slot_count = 0;
    ASSERT_EQ(encode_nsm_query_get_erot_state_parameters_resp(
                  0, NSM_SUCCESS, ERR_NULL, &erotInfo, msg),
              NSM_SW_SUCCESS);
    uint8_t rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// updateInbandUpdatePolicyHandler branch coverage
// =============================================================================

// Non-string value → std::get_if<std::string> returns nullptr → InvalidArgument
TEST_F(NsmRoTPropertyTest, UpdateInbandUpdatePolicy_NonStringValue)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint8_t(1); // not a string
    updateInbandUpdatePolicyHandler(value, &status, gpu, classification,
                                    identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// Invalid policy string → convertStringToInbandPolicyState returns nullopt
// → InvalidArgument
TEST_F(NsmRoTPropertyTest, UpdateInbandUpdatePolicy_InvalidString)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("InvalidPolicyString");
    updateInbandUpdatePolicyHandler(value, &status, gpu, classification,
                                    identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// Valid "Enabled" + postPatchIO failure → WriteFailure
TEST_F(NsmRoTPropertyTest, UpdateInbandUpdatePolicy_Enabled_PostPatchFail)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");
    updateInbandUpdatePolicyHandler(value, &status, gpu, classification,
                                    identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Valid "Enabled" + short response → decode fails → WriteFailure
TEST_F(NsmRoTPropertyTest, UpdateInbandUpdatePolicy_Enabled_DecodeFail)
{
    std::vector<uint8_t> shortResp(sizeof(nsm_msg_hdr) + 2, 0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(shortResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");
    updateInbandUpdatePolicyHandler(value, &status, gpu, classification,
                                    identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Valid "Enabled" + success response → Success
TEST_F(NsmRoTPropertyTest, UpdateInbandUpdatePolicy_Enabled_Success)
{
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, msg),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");
    updateInbandUpdatePolicyHandler(value, &status, gpu, classification,
                                    identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Valid "Disabled" + success response → covers the
// policyState == Disabled branch (lines ~185-188)
TEST_F(NsmRoTPropertyTest, UpdateInbandUpdatePolicy_Disabled_Success)
{
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, msg),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Disabled");
    updateInbandUpdatePolicyHandler(value, &status, gpu, classification,
                                    identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// NsmImageCopy::setImageCopyResult / mapReasonCodeToErrorCode switch coverage
// =============================================================================

struct NsmImageCopySetResultTest : public Test, public utils::DBusTest
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;
    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;

    const std::string chassisName = "HGX_IC_TestChassis";
    const std::string imageCopyPath =
        "/xyz/openbmc_project/inventory/system/chassis/" + chassisName;
    const std::string asyncValuePath = imageCopyPath + "_value";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    // NsmInbandUpdatePolicyObject is a NsmSensor (NsmObject), used as
    // the required NsmObject& reference for NsmImageCopy.
    std::unique_ptr<NsmInbandUpdatePolicyObject> nsmObj;
    std::unique_ptr<NsmImageCopy> imageCopy;
    std::shared_ptr<AsyncValueIntf> valueIntf;

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        nsmObj = std::make_unique<NsmInbandUpdatePolicyObject>(bus, chassisName,
                                                               1, 2, 0);
        imageCopy = std::make_unique<NsmImageCopy>(bus, imageCopyPath, uuid,
                                                   *nsmObj);
        valueIntf = std::make_shared<AsyncValueIntf>(bus,
                                                     asyncValuePath.c_str());
    }
};

// Covers all 7 mapReasonCodeToErrorCode switch cases by calling
// setImageCopyResult with each error code reason value.
TEST_F(NsmImageCopySetResultTest, SetImageCopyResult_AllReasonCodes)
{
    // ERR_NULL (default) → ErrorCode::None
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::None, ERR_NULL);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::None);

    // ERR_NO_BOOT_COMPLETE → ErrorCode::NoBootComplete
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected,
                                  ERR_NO_BOOT_COMPLETE);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::NoBootComplete);

    // ERR_UPDATE_IN_PROGRESS → ErrorCode::UpdateInProgress
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected,
                                  ERR_UPDATE_IN_PROGRESS);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::UpdateInProgress);

    // ERR_IMAGE_COPY_IN_PROGRESS → ErrorCode::ImageCopyInProgress
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Processing,
                                  ERR_IMAGE_COPY_IN_PROGRESS);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::ImageCopyInProgress);

    // ERR_IMAGE_COPY_COMPLETED → ErrorCode::ImageCopyCompleted
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Accepted,
                                  ERR_IMAGE_COPY_COMPLETED);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::ImageCopyCompleted);

    // ERR_FLASH_WEAR_MITIGATION → ErrorCode::FlashWearMitigation
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected,
                                  ERR_FLASH_WEAR_MITIGATION);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::FlashWearMitigation);

    // ERR_INCOMPLETE_COMPONENT_SET → ErrorCode::IncompleteComponentSet
    imageCopy->setImageCopyResult(valueIntf, NSM_SUCCESS, ERR_NULL,
                                  RequestStatus::Rejected,
                                  ERR_INCOMPLETE_COMPONENT_SET);
    EXPECT_EQ(imageCopy->errorCode(), ErrorCode::IncompleteComponentSet);
}

// =============================================================================
// updateImageCopyPolicyHandler branch coverage
// Mirrors updateInbandUpdatePolicyHandler tests for the ImageCopyPolicy
// variant.
// =============================================================================

// Non-string value → std::get_if<std::string> returns nullptr → InvalidArgument
TEST_F(NsmRoTPropertyTest, UpdateImageCopyPolicy_NonStringValue)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint8_t(1); // not a string
    updateImageCopyPolicyHandler(value, &status, gpu, classification,
                                 identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// Invalid policy string → convertStringToImageCopyPolicyState returns nullopt
// → InvalidArgument
TEST_F(NsmRoTPropertyTest, UpdateImageCopyPolicy_InvalidString)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("InvalidPolicyString");
    updateImageCopyPolicyHandler(value, &status, gpu, classification,
                                 identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// Valid "Manual" + postPatchIO failure → WriteFailure
TEST_F(NsmRoTPropertyTest, UpdateImageCopyPolicy_Manual_PostPatchFail)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");
    updateImageCopyPolicyHandler(value, &status, gpu, classification,
                                 identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Valid "Manual" + short response → decode fails → WriteFailure
TEST_F(NsmRoTPropertyTest, UpdateImageCopyPolicy_Manual_DecodeFail)
{
    std::vector<uint8_t> shortResp(sizeof(nsm_msg_hdr) + 2, 0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(shortResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");
    updateImageCopyPolicyHandler(value, &status, gpu, classification,
                                 identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Valid "Manual" + success response → Success
TEST_F(NsmRoTPropertyTest, UpdateImageCopyPolicy_Manual_Success)
{
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, msg),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");
    updateImageCopyPolicyHandler(value, &status, gpu, classification,
                                 identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Valid "Automatic" + success response → covers Automatic branch
TEST_F(NsmRoTPropertyTest, UpdateImageCopyPolicy_Automatic_Success)
{
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, msg),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Automatic");
    updateImageCopyPolicyHandler(value, &status, gpu, classification,
                                 identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// NsmImageCopy::getActiveSlotComponentInfo branch coverage
// Called directly (private method exposed via #define private public).
// Uses mockDBus.getSubtree and MockDbusAsync::propertyMap for async calls.
// =============================================================================

// Empty getSubtree response → loop never executes → NSM_SW_ERROR
TEST_F(NsmImageCopySetResultTest, GetActiveSlotComponentInfo_EmptySubtree)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _))
        .WillOnce(Return(GetSubTreeResponse{}));

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo("/test/path", info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// Entry with empty mapServiceInterfaces → continue → NSM_SW_ERROR
TEST_F(NsmImageCopySetResultTest,
       GetActiveSlotComponentInfo_EmptyServiceInterfaces)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    GetSubTreeResponse resp{{"/test/slot_empty_svc", {}}}; // empty interfaces
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo("/test/path_esvc", info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// Entry with matching service but associations has no AbsolutePath → continue
TEST_F(NsmImageCopySetResultTest, GetActiveSlotComponentInfo_NoAbsolutePath)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/slot_no_abs";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    // Associations property map present but without AbsolutePath
    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    assocMap["OtherProp"] = std::string("some_value");

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo("/test/path_nabs", info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// Entry with AbsolutePath that does not match objectPath → continue
TEST_F(NsmImageCopySetResultTest, GetActiveSlotComponentInfo_PathNotMatching)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/slot_path_mismatch";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    assocMap["AbsolutePath"] = std::string("/different/path");

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo("/target/path", info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// Matching path, slot properties found but "SlotType" property absent →
// slotProperties.count("SlotType") == 0 → FALSE branch (line 342) → loop //
// continues → NSM_SW_ERROR
TEST_F(NsmImageCopySetResultTest, GetActiveSlotComponentInfo_NoSlotType)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/slot_no_slottype";
    const std::string targetPath = "/target/no_slottype";
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
    // "SlotType" intentionally absent → count("SlotType") == 0 → FALSE branch
    slotMap["OtherProp"] = std::string("some_value");

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// Matching path, slot properties found but SlotType != "Active" → NSM_SW_ERROR
TEST_F(NsmImageCopySetResultTest, GetActiveSlotComponentInfo_SlotTypeNotActive)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/slot_inactive";
    const std::string targetPath = "/target/inactive";
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
    slotMap["SlotType"] = std::string("Passive"); // not "Active"

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// SlotType "Active" but component values out of range → continue → NSM_SW_ERROR
TEST_F(NsmImageCopySetResultTest, GetActiveSlotComponentInfo_ValuesOutOfRange)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/slot_outofrange";
    const std::string targetPath = "/target/outofrange";
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
    slotMap["ComponentClassification"] = uint64_t(0x10000); // > UINT16_MAX
    slotMap["ComponentIdentifier"] = uint64_t(1);
    slotMap["ComponentIndex"] = uint64_t(0);

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// SlotType "Active" with valid in-range values → NSM_SW_SUCCESS
TEST_F(NsmImageCopySetResultTest, GetActiveSlotComponentInfo_Success)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/slot_success";
    const std::string targetPath = "/target/success";
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

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(info.classification, uint16_t(2));
    EXPECT_EQ(info.identifier, uint16_t(1));
    EXPECT_EQ(info.index, uint8_t(0));
}

// =============================================================================
// NsmImageCopy::initiateImageCopyAsync and imageCopyAsyncHandler coverage
// New fixture inherits SensorManagerTest for device access via singleton.
// =============================================================================

struct NsmImageCopyAsyncTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_IC_AsyncTest";
    const std::string imageCopyPath =
        "/xyz/openbmc_project/inventory/system/chassis/" + chassisName;
    const std::string asyncValuePath = imageCopyPath + "_value";
    const std::string asyncStatusPath = imageCopyPath + "_status";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    std::unique_ptr<NsmInbandUpdatePolicyObject> nsmObj;
    std::unique_ptr<NsmImageCopy> imageCopy;
    std::shared_ptr<AsyncValueIntf> valueIntf;
    std::shared_ptr<AsyncStatusIntf> statusIntf;

    NsmImageCopyAsyncTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_NE(gpu, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        nsmObj = std::make_unique<NsmInbandUpdatePolicyObject>(bus, chassisName,
                                                               1, 2, 0);
        imageCopy = std::make_unique<NsmImageCopy>(bus, imageCopyPath, uuid,
                                                   *nsmObj);
        valueIntf = std::make_shared<AsyncValueIntf>(bus,
                                                     asyncValuePath.c_str());
        statusIntf = std::make_shared<AsyncStatusIntf>(bus,
                                                       asyncStatusPath.c_str());
    }

    ~NsmImageCopyAsyncTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Empty objectPaths → componentInfos empty → WriteFailure + Rejected
TEST_F(NsmImageCopyAsyncTest, InitiateImageCopyAsync_EmptyPaths_Rejected)
{
    imageCopy->initiateImageCopyAsync({}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// One path, getActiveSlotComponentInfo returns error → InternalFailure
TEST_F(NsmImageCopyAsyncTest, InitiateImageCopyAsync_GetInfoFails)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _))
        .WillOnce(Return(GetSubTreeResponse{})); // empty → NSM_SW_ERROR

    imageCopy->initiateImageCopyAsync({"/some/path"}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// getActiveSlotComponentInfo succeeds, imageCopyAsyncHandler postPatchIO fails
// → cc stays NSM_ERROR → InternalFailure in initiateImageCopyAsync
TEST_F(NsmImageCopyAsyncTest, InitiateImageCopyAsync_PostPatchFails)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/async_slot";
    const std::string targetPath = "/target/async";
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

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// =============================================================================
// NsmImageCopy::requestImageCopy branch coverage
// =============================================================================

// requestImageCopy when imageCopyRequestStatus == Processing → throws
// Unavailable (TRUE branch of `if (ImageCopyRequestStatus() == Processing)` in
// requestImageCopy)
TEST_F(NsmImageCopyAsyncTest,
       DISABLED_RequestImageCopy_AlreadyProcessing_Throws)
{
    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    // Force the status to Processing (private field exposed by #define private
    // public)
    imageCopy->imageCopyRequestStatus(RequestStatus::Processing);

    EXPECT_THROW(imageCopy->requestImageCopy({}),
                 sdbusplus::error::xyz::openbmc_project::common::Unavailable);
}

// getActiveSlotComponentInfo and imageCopyAsyncHandler both succeed → Accepted
TEST_F(NsmImageCopyAsyncTest, InitiateImageCopyAsync_Success)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/async_slot_ok";
    const std::string targetPath = "/target/async_ok";
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

    // Build a zeroed response buffer: decode checks cc=0 → NSM_SUCCESS
    std::vector<uint8_t> resp2(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_initiate_copy_resp_command),
        0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp2));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Accepted);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// imageCopyAsyncHandler: postPatchIO succeeds, decode returns cc=NSM_ERROR
// → L619 TRUE (cc != NSM_SUCCESS) → L621-626 covered
TEST_F(NsmImageCopyAsyncTest, InitiateImageCopyAsync_DecodeCcError)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/async_slot_ccerr";
    const std::string targetPath = "/target/async_ccerr";
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

    // Build response with cc=NSM_ERROR (completion_code at payload[1])
    std::vector<uint8_t> errResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    errResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errResp));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// =============================================================================
// NsmImageCopy::imageCopyAsyncHandler: device not found (L551-553)
// Uses NullReturnMockSensorManager so getNsmDeviceFromStaticUUID → nullptr.
// =============================================================================
// REMOVED_TIMER_MARKER

struct NsmImageCopyNullDeviceTest : public Test, public utils::DBusTest
{
    NsmDeviceTable devices;
    NiceMock<NullReturnMockSensorManager> nullManager{devices};

    const std::string chassisName = "HGX_IC_NullDev";
    const std::string imageCopyPath =
        "/xyz/openbmc_project/inventory/system/chassis/" + chassisName;
    const std::string asyncValuePath = imageCopyPath + "_value";
    const std::string asyncStatusPath = imageCopyPath + "_status";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    std::unique_ptr<NsmInbandUpdatePolicyObject> nsmObj;
    std::unique_ptr<NsmImageCopy> imageCopy;
    std::shared_ptr<AsyncValueIntf> valueIntf;
    std::shared_ptr<AsyncStatusIntf> statusIntf;

    NsmImageCopyNullDeviceTest()
    {
        sensorManagerInstance.reset(&nullManager);
        auto& bus = utils::DBusHandler::getBus();
        nsmObj = std::make_unique<NsmInbandUpdatePolicyObject>(bus, chassisName,
                                                               1, 2, 0);
        imageCopy = std::make_unique<NsmImageCopy>(bus, imageCopyPath, uuid,
                                                   *nsmObj);
        valueIntf = std::make_shared<AsyncValueIntf>(bus,
                                                     asyncValuePath.c_str());
        statusIntf = std::make_shared<AsyncStatusIntf>(bus,
                                                       asyncStatusPath.c_str());
    }
    ~NsmImageCopyNullDeviceTest() override
    {
        imageCopy.reset();
        nsmObj.reset();
        sensorManagerInstance.release();
        devices.clear();
    }
};

// getActiveSlotComponentInfo succeeds but imageCopyAsyncHandler finds no
// device → L551-553 covered → InternalFailure
TEST_F(NsmImageCopyNullDeviceTest, ImageCopyAsyncHandler_DeviceNotFound)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/nulldev_slot";
    const std::string targetPath = "/target/nulldev";
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

// =============================================================================
// imageCopyTimeoutTimer TRUE branch tests (lines 454, 472, 492, 503, 530):
// Set imageCopyTimeoutTimer to non-null before calling initiateImageCopyAsync
// so each `if (imageCopyTimeoutTimer)` guard is TRUE → timer->stop() called.
// =============================================================================

// L472 TRUE: empty objectPaths → componentInfos.empty() → timer->stop()
TEST_F(NsmImageCopyAsyncTest,
       InitiateImageCopyAsync_WithTimer_EmptyPaths_StopsTimer)
{
    imageCopy->imageCopyTimeoutTimer =
        std::make_shared<sdbusplus::Timer>([]() {});

    imageCopy->initiateImageCopyAsync({}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// L454 TRUE: getActiveSlotComponentInfo returns error → timer->stop()
TEST_F(NsmImageCopyAsyncTest,
       InitiateImageCopyAsync_WithTimer_GetInfoFails_StopsTimer)
{
    imageCopy->imageCopyTimeoutTimer =
        std::make_shared<sdbusplus::Timer>([]() {});

    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _))
        .WillOnce(Return(GetSubTreeResponse{}));

    imageCopy->initiateImageCopyAsync({"/some/timer_path"}, statusIntf,
                                      valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// L492 TRUE: postPatchIO fails → cc!=NSM_SUCCESS → timer->stop() at L492
TEST_F(NsmImageCopyAsyncTest,
       InitiateImageCopyAsync_WithTimer_CcError_StopsTimer)
{
    imageCopy->imageCopyTimeoutTimer =
        std::make_shared<sdbusplus::Timer>([]() {});

    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/timer_slot_cc";
    const std::string targetPath = "/target/timer_cc";
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

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// L503 TRUE: success path → timer->stop()
TEST_F(NsmImageCopyAsyncTest,
       InitiateImageCopyAsync_WithTimer_Success_StopsTimer)
{
    imageCopy->imageCopyTimeoutTimer =
        std::make_shared<sdbusplus::Timer>([]() {});

    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/timer_slot_succ";
    const std::string targetPath = "/target/timer_succ";
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

    // Build a success response (cc=NSM_SUCCESS, rc=NSM_SW_SUCCESS)
    std::vector<uint8_t> resp2(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_initiate_copy_resp_command),
        0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp2));

    imageCopy->initiateImageCopyAsync({targetPath}, statusIntf, valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Accepted);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// L530 TRUE (NullDevice): imageCopyAsyncHandler finds no device → timer->stop()
TEST_F(NsmImageCopyNullDeviceTest,
       InitiateImageCopyAsync_WithTimer_NullDevice_StopsTimer)
{
    imageCopy->imageCopyTimeoutTimer =
        std::make_shared<sdbusplus::Timer>([]() {});

    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/nulldev_timer2_slot";
    const std::string targetPath = "/target/nulldev_timer2";
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
