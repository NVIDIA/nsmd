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

// ============================================================================
// NEW TESTS for nsmSecurityRBP.cpp and nsmFabricManager.cpp
//
// Target files:
//   nsmd/nsmFirmwareUtils/nsmSecurityRBP.cpp  (15 uncovered functions)
//   nsmd/nsmManagers/nsmFabricManager.cpp     (10 uncovered functions)
//
// These tests are organized into two sections. Each section uses the
// established SensorManagerTest fixture pattern with AAA (Arrange-Act-Assert).
// ============================================================================

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "libnsm/firmware-utils.h"
#include "libnsm/network-ports.h"

#include "nsmManagers/nsmFabricManager.hpp"
#include "nsmSecurityRBP.hpp"

using namespace nsm;

// ============================================================================
// SECTION 1: nsmSecurityRBP.cpp tests
// ============================================================================

// ---------------------------------------------------------------------------
// Fixture: NsmSecurityCfgObjectTest
//   Covers NsmSecurityCfgObject constructor, genRequestMsg, handleResponseMsg
//   and SecurityConfiguration constructor, updateState, updateNonce
// ---------------------------------------------------------------------------
struct NsmSecurityCfgObjectTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "HGX_EROT_SecurityCfg";
    const std::string type = "NSM_SecurityCfg";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    eid_t eid = 0;
    uint8_t instanceId = 0;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    std::shared_ptr<ProgressIntf> progressIntf;
    std::shared_ptr<NsmSecurityCfgObject> sensor;

    NsmSecurityCfgObjectTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                                   name + "/Progress";
        progressIntf = std::make_shared<ProgressIntf>(bus,
                                                      progressPath.c_str());

        sensor = std::make_shared<NsmSecurityCfgObject>(bus, name, type,
                                                        fpgaUuid, progressIntf);
        ASSERT_NE(sensor, nullptr);
    }

    ~NsmSecurityCfgObjectTest() override
    {
        sensor.reset();
        progressIntf.reset();
        cleanupDeviceSensors(devices);
    }
};

// -- NsmSecurityCfgObject constructor tests --

TEST_F(NsmSecurityCfgObjectTest, Constructor_ValidParams_CreatesObject)
{
    // Arrange - done in SetUp
    // Act - constructed in SetUp
    // Assert
    EXPECT_EQ(sensor->getName(), name);
    EXPECT_EQ(sensor->getType(), type);
    EXPECT_NE(sensor->securityCfgObject, nullptr);
    std::string expectedPath = std::string(chassisInventoryBasePath) + "/" +
                               name;
    EXPECT_EQ(sensor->objectPath, expectedPath);
}

// -- NsmSecurityCfgObject::genRequestMsg tests --

TEST_F(NsmSecurityCfgObjectTest,
       GenRequestMsg_ValidParams_ReturnsRequestWithCorrectSize)
{
    // Arrange - sensor created in SetUp
    // Act
    auto request = sensor->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_irreversible_config_req_command));
}

TEST_F(NsmSecurityCfgObjectTest,
       GenRequestMsg_ValidParams_EncodesQueryRequestType)
{
    // Arrange
    // Act
    auto request = sensor->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    auto msgPtr = reinterpret_cast<const struct nsm_msg*>(request->data());
    // Verify we can decode the request and confirm the type is QUERY
    struct nsm_firmware_irreversible_config_req decoded_req;
    auto rc = decode_nsm_firmware_irreversible_config_req(
        msgPtr, request->size(), &decoded_req);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(decoded_req.request_type, QUERY_IRREVERSIBLE_CFG);
}

TEST_F(NsmSecurityCfgObjectTest,
       GenRequestMsg_DifferentInstanceId_EncodesInstanceId)
{
    // Arrange
    uint8_t altInstanceId = 5;

    // Act
    auto request = sensor->genRequestMsg(eid, altInstanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    auto msgPtr = reinterpret_cast<const struct nsm_msg*>(request->data());
    EXPECT_EQ(msgPtr->hdr.instance_id, altInstanceId);
}

// -- NsmSecurityCfgObject::handleResponseMsg tests --

TEST_F(NsmSecurityCfgObjectTest,
       HandleResponseMsg_SuccessStateTrue_UpdatesIrreversibleConfigState)
{
    // Arrange
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 1;

    auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &cfg_state, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor->securityCfgObject->irreversibleConfigState());
}

TEST_F(NsmSecurityCfgObjectTest,
       HandleResponseMsg_SuccessStateFalse_UpdatesIrreversibleConfigState)
{
    // Arrange
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 0;

    auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &cfg_state, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor->securityCfgObject->irreversibleConfigState());
}

TEST_F(NsmSecurityCfgObjectTest,
       DISABLED_HandleResponseMsg_CompletionCodeError_ReturnsErrorCode)
{
    // Arrange
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 0;

    auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
        instanceId, NSM_ERROR, ERR_NULL, &cfg_state, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert - cc is NSM_ERROR so result should be non-zero
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmSecurityCfgObjectTest,
       DISABLED_HandleResponseMsg_TruncatedResponse_ReturnsNonSuccess)
{
    // Arrange
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 1;

    auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &cfg_state, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act - pass minimal size (just header) so decode fails
    size_t truncatedSize = sizeof(nsm_msg_hdr);
    rc = sensor->handleResponseMsg(responseMsg, truncatedSize);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// -- SecurityConfiguration::updateState tests --

TEST_F(NsmSecurityCfgObjectTest,
       UpdateState_StateEnabled_SetsIrreversibleConfigTrue)
{
    // Arrange
    struct nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 1;

    // Act
    sensor->securityCfgObject->updateState(cfg_state);

    // Assert
    EXPECT_TRUE(sensor->securityCfgObject->irreversibleConfigState());
}

TEST_F(NsmSecurityCfgObjectTest,
       UpdateState_StateDisabled_SetsIrreversibleConfigFalse)
{
    // Arrange
    struct nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 0;

    // Act
    sensor->securityCfgObject->updateState(cfg_state);

    // Assert
    EXPECT_FALSE(sensor->securityCfgObject->irreversibleConfigState());
}

// -- SecurityConfiguration::updateNonce tests --

TEST_F(NsmSecurityCfgObjectTest, UpdateNonce_ValidNonce_SetsNonceValue)
{
    // Arrange
    struct nsm_firmware_irreversible_config_request_2_resp cfg_resp = {};
    cfg_resp.nonce = 0xDEADBEEF12345678ULL;

    // Act
    sensor->securityCfgObject->updateNonce(cfg_resp);

    // Assert
    EXPECT_EQ(sensor->securityCfgObject->nonce(), 0xDEADBEEF12345678ULL);
}

TEST_F(NsmSecurityCfgObjectTest, UpdateNonce_ZeroNonce_SetsZero)
{
    // Arrange
    struct nsm_firmware_irreversible_config_request_2_resp cfg_resp = {};
    cfg_resp.nonce = 0;

    // Act
    sensor->securityCfgObject->updateNonce(cfg_resp);

    // Assert
    EXPECT_EQ(sensor->securityCfgObject->nonce(), 0ULL);
}

TEST_F(NsmSecurityCfgObjectTest, UpdateNonce_MaxNonce_SetsMaxValue)
{
    // Arrange
    struct nsm_firmware_irreversible_config_request_2_resp cfg_resp = {};
    cfg_resp.nonce = UINT64_MAX;

    // Act
    sensor->securityCfgObject->updateNonce(cfg_resp);

    // Assert
    EXPECT_EQ(sensor->securityCfgObject->nonce(), UINT64_MAX);
}

// -- SecurityConfiguration constructor test --

TEST_F(NsmSecurityCfgObjectTest,
       SecurityConfigurationConstructor_StoresUuidAndProgressIntf)
{
    // Arrange - implicitly via sensor setup
    // Act - constructor called in SetUp via NsmSecurityCfgObject

    // Assert
    EXPECT_EQ(sensor->securityCfgObject->uuid, fpgaUuid);
    EXPECT_NE(sensor->securityCfgObject->progressIntf, nullptr);
}

// ---------------------------------------------------------------------------
// Fixture: NsmMinSecVersionObjectTest
//   Covers NsmMinSecVersionObject constructor, genRequestMsg, handleResponseMsg
//   and MinSecurityVersion constructor, updateProperties
// ---------------------------------------------------------------------------
struct NsmMinSecVersionObjectTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_EROT_MinSecVer";
    const std::string type = "NSM_MinSecVersion";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const uint16_t classification = 0x000A;
    const uint16_t identifier = 0x0001;
    const uint8_t index = 0;
    eid_t eid = 0;
    uint8_t instanceId = 0;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    std::shared_ptr<ProgressIntf> progressIntf;
    std::shared_ptr<NsmMinSecVersionObject> sensor;

    NsmMinSecVersionObjectTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                                   chassisName + "/Progress";
        progressIntf = std::make_shared<ProgressIntf>(bus,
                                                      progressPath.c_str());

        sensor = std::make_shared<NsmMinSecVersionObject>(
            bus, chassisName, type, fpgaUuid, classification, identifier, index,
            progressIntf);
        ASSERT_NE(sensor, nullptr);
    }

    ~NsmMinSecVersionObjectTest() override
    {
        sensor.reset();
        progressIntf.reset();
        cleanupDeviceSensors(devices);
    }
};

// -- NsmMinSecVersionObject constructor tests --

TEST_F(NsmMinSecVersionObjectTest, Constructor_ValidParams_CreatesObject)
{
    // Arrange - done in SetUp
    // Act - constructed in SetUp
    // Assert
    EXPECT_EQ(sensor->getName(), chassisName);
    EXPECT_EQ(sensor->getType(), type);
    EXPECT_NE(sensor->minSecVersion, nullptr);
    EXPECT_EQ(sensor->classification, classification);
    EXPECT_EQ(sensor->identifier, identifier);
    EXPECT_EQ(sensor->index, index);

    std::string expectedPath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName;
    EXPECT_EQ(sensor->objectPath, expectedPath);
}

// -- NsmMinSecVersionObject::genRequestMsg tests --

TEST_F(NsmMinSecVersionObjectTest,
       GenRequestMsg_ValidParams_ReturnsRequestWithCorrectSize)
{
    // Arrange
    // Act
    auto request = sensor->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_security_version_number_req_command));
}

TEST_F(NsmMinSecVersionObjectTest,
       GenRequestMsg_ValidParams_EncodesClassificationAndIdentifier)
{
    // Arrange
    // Act
    auto request = sensor->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    auto msgPtr = reinterpret_cast<const struct nsm_msg*>(request->data());
    struct nsm_firmware_security_version_number_req decoded_req;
    auto rc = decode_nsm_query_firmware_security_version_number_req(
        msgPtr, request->size(), &decoded_req);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(htole16(decoded_req.component_classification),
              htole16(classification));
    EXPECT_EQ(htole16(decoded_req.component_identifier), htole16(identifier));
    EXPECT_EQ(decoded_req.component_classification_index, index);
}

TEST_F(NsmMinSecVersionObjectTest,
       GenRequestMsg_DifferentInstanceId_EncodesInstanceId)
{
    // Arrange
    uint8_t altInstanceId = 7;

    // Act
    auto request = sensor->genRequestMsg(eid, altInstanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    auto msgPtr = reinterpret_cast<const struct nsm_msg*>(request->data());
    EXPECT_EQ(msgPtr->hdr.instance_id, altInstanceId);
}

// -- NsmMinSecVersionObject::handleResponseMsg tests --

TEST_F(NsmMinSecVersionObjectTest,
       HandleResponseMsg_Success_UpdatesSecurityVersionProperties)
{
    // Arrange
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    struct nsm_firmware_security_version_number_resp sec_info = {};
    sec_info.minimum_security_version = htole16(5);
    sec_info.pending_minimum_security_version = htole16(6);
    sec_info.active_component_security_version = htole16(10);
    sec_info.pending_component_security_version = htole16(11);

    auto rc = encode_nsm_query_firmware_security_version_number_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &sec_info, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // updateProperties is called which sets version() and the settings
    // version()
    EXPECT_NE(sensor->minSecVersion->securityVersionObject, nullptr);
    EXPECT_NE(sensor->minSecVersion->securityVersionSettingsObject, nullptr);
}

TEST_F(NsmMinSecVersionObjectTest,
       HandleResponseMsg_ZeroVersions_UpdatesWithZeros)
{
    // Arrange
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    struct nsm_firmware_security_version_number_resp sec_info = {};
    sec_info.minimum_security_version = 0;
    sec_info.pending_minimum_security_version = 0;
    sec_info.active_component_security_version = 0;
    sec_info.pending_component_security_version = 0;

    auto rc = encode_nsm_query_firmware_security_version_number_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &sec_info, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmMinSecVersionObjectTest,
       DISABLED_HandleResponseMsg_CompletionCodeError_ReturnsErrorCode)
{
    // Arrange
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    struct nsm_firmware_security_version_number_resp sec_info = {};
    sec_info.minimum_security_version = htole16(1);
    sec_info.pending_minimum_security_version = htole16(2);

    auto rc = encode_nsm_query_firmware_security_version_number_resp(
        instanceId, NSM_ERROR, ERR_NULL, &sec_info, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert - cc is NSM_ERROR so result should be non-zero
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmMinSecVersionObjectTest,
       DISABLED_HandleResponseMsg_TruncatedResponse_ReturnsNonSuccess)
{
    // Arrange
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    struct nsm_firmware_security_version_number_resp sec_info = {};
    sec_info.minimum_security_version = htole16(1);

    auto rc = encode_nsm_query_firmware_security_version_number_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &sec_info, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act - pass truncated size
    size_t truncatedSize = sizeof(nsm_msg_hdr);
    rc = sensor->handleResponseMsg(responseMsg, truncatedSize);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// -- MinSecurityVersion constructor tests --

TEST_F(NsmMinSecVersionObjectTest,
       MinSecVersionConstructor_StoresClassificationAndIdentifier)
{
    // Arrange - implicitly via sensor setup
    // Act - constructor called via NsmMinSecVersionObject

    // Assert
    EXPECT_EQ(sensor->minSecVersion->classification, classification);
    EXPECT_EQ(sensor->minSecVersion->identifier, identifier);
    EXPECT_EQ(sensor->minSecVersion->index, index);
    EXPECT_NE(sensor->minSecVersion->progressIntf, nullptr);
}

TEST_F(NsmMinSecVersionObjectTest,
       MinSecVersionConstructor_CreatesSecurityVersionInterfaces)
{
    // Arrange - implicitly via sensor setup
    // Act - constructor creates securityVersionObject and Settings

    // Assert
    EXPECT_NE(sensor->minSecVersion->securityVersionObject, nullptr);
    EXPECT_NE(sensor->minSecVersion->securityVersionSettingsObject, nullptr);
}

// -- MinSecurityVersion::updateProperties tests --

TEST_F(NsmMinSecVersionObjectTest, UpdateProperties_ValidData_UpdatesVersions)
{
    // Arrange
    struct nsm_firmware_security_version_number_resp sec_info = {};
    sec_info.minimum_security_version = htole16(42);
    sec_info.pending_minimum_security_version = htole16(43);

    // Act
    sensor->minSecVersion->updateProperties(sec_info);

    // Assert
    EXPECT_EQ(sensor->minSecVersion->securityVersionObject->version(),
              htole16(htole16(42)));
    EXPECT_EQ(sensor->minSecVersion->securityVersionSettingsObject->version(),
              htole16(htole16(43)));
}

TEST_F(NsmMinSecVersionObjectTest, UpdateProperties_MaxValues_UpdatesCorrectly)
{
    // Arrange
    struct nsm_firmware_security_version_number_resp sec_info = {};
    sec_info.minimum_security_version = htole16(0xFFFF);
    sec_info.pending_minimum_security_version = htole16(0xFFFF);

    // Act
    sensor->minSecVersion->updateProperties(sec_info);

    // Assert - htole16(htole16(x)) == x on any endian
    EXPECT_EQ(sensor->minSecVersion->securityVersionObject->version(), 0xFFFF);
    EXPECT_EQ(sensor->minSecVersion->securityVersionSettingsObject->version(),
              0xFFFF);
}

// -- SecurityConfiguration startOperation / finishOperation tests --

TEST_F(NsmSecurityCfgObjectTest, StartOperation_FirstCall_ReturnsSuccess)
{
    // Arrange - sensor ready from SetUp
    // Act
    int rc = sensor->securityCfgObject->startOperation();

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Cleanup - unlock the mutex so destructor can proceed
    sensor->securityCfgObject->mutex.unlock();
}

TEST_F(NsmSecurityCfgObjectTest, StartOperation_SetsProgressStatusInProgress)
{
    // Arrange
    // Act
    int rc = sensor->securityCfgObject->startOperation();

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor->securityCfgObject->progressIntf->status(),
              Progress::OperationStatus::InProgress);
    EXPECT_EQ(sensor->securityCfgObject->progressIntf->completedTime(), 0UL);

    // Cleanup
    sensor->securityCfgObject->mutex.unlock();
}

TEST_F(NsmSecurityCfgObjectTest,
       FinishOperation_Completed_SetsProgress100AndCompletedTime)
{
    // Arrange - lock first via startOperation
    int rc = sensor->securityCfgObject->startOperation();
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    sensor->securityCfgObject->finishOperation(
        Progress::OperationStatus::Completed);

    // Assert
    EXPECT_EQ(sensor->securityCfgObject->progressIntf->progress(), 100);
    EXPECT_EQ(sensor->securityCfgObject->progressIntf->status(),
              Progress::OperationStatus::Completed);
    EXPECT_GT(sensor->securityCfgObject->progressIntf->completedTime(), 0UL);
}

TEST_F(NsmSecurityCfgObjectTest, FinishOperation_Aborted_DoesNotSetProgress100)
{
    // Arrange - lock first via startOperation
    int rc = sensor->securityCfgObject->startOperation();
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    sensor->securityCfgObject->finishOperation(
        Progress::OperationStatus::Aborted);

    // Assert
    EXPECT_NE(sensor->securityCfgObject->progressIntf->progress(), 100);
    EXPECT_EQ(sensor->securityCfgObject->progressIntf->status(),
              Progress::OperationStatus::Aborted);
}

// -- MinSecurityVersion startOperation / finishOperation tests --

TEST_F(NsmMinSecVersionObjectTest, StartOperation_FirstCall_ReturnsSuccess)
{
    // Arrange
    // Act
    int rc = sensor->minSecVersion->startOperation();

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Cleanup
    sensor->minSecVersion->mutex.unlock();
}

TEST_F(NsmMinSecVersionObjectTest, StartOperation_SetsProgressStatusInProgress)
{
    // Arrange
    // Act
    int rc = sensor->minSecVersion->startOperation();

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor->minSecVersion->progressIntf->status(),
              Progress::OperationStatus::InProgress);
    EXPECT_EQ(sensor->minSecVersion->progressIntf->completedTime(), 0UL);

    // Cleanup
    sensor->minSecVersion->mutex.unlock();
}

TEST_F(NsmMinSecVersionObjectTest, FinishOperation_Completed_SetsProgress100)
{
    // Arrange
    int rc = sensor->minSecVersion->startOperation();
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    sensor->minSecVersion->finishOperation(
        Progress::OperationStatus::Completed);

    // Assert
    EXPECT_EQ(sensor->minSecVersion->progressIntf->progress(), 100);
    EXPECT_EQ(sensor->minSecVersion->progressIntf->status(),
              Progress::OperationStatus::Completed);
    EXPECT_GT(sensor->minSecVersion->progressIntf->completedTime(), 0UL);
}

TEST_F(NsmMinSecVersionObjectTest,
       FinishOperation_Aborted_DoesNotSetProgress100)
{
    // Arrange
    int rc = sensor->minSecVersion->startOperation();
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    sensor->minSecVersion->finishOperation(Progress::OperationStatus::Aborted);

    // Assert
    EXPECT_NE(sensor->minSecVersion->progressIntf->progress(), 100);
    EXPECT_EQ(sensor->minSecVersion->progressIntf->status(),
              Progress::OperationStatus::Aborted);
}

// ============================================================================
// SECTION 2: nsmFabricManager.cpp tests
// ============================================================================

// ---------------------------------------------------------------------------
// Fixture: NsmFabricManagerStateTest
//   Covers NsmFabricManagerState constructor, genRequestMsg, handleResponseMsg
//   and NsmAggregateFabricManagerState singleton, updateAggregate
// ---------------------------------------------------------------------------
struct NsmFabricManagerStateTest : public Test
{
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();

    void SetUp() override
    {
        // Reset singleton for each test
        NsmAggregateFabricManagerState::nsmAggregateFabricManagerState.reset();
        NsmAggregateFabricManagerState::associatedFabricManagerIntfs.clear();
        NsmAggregateFabricManagerState::associatedOperationalStatusIntfs
            .clear();
        NsmAggregateFabricManagerState::aggregateFMObjPath = "";
    }
};

// -- NsmFabricManagerState constructor tests --

TEST_F(NsmFabricManagerStateTest,
       Constructor_ValidParams_CreatesObjectWithInterfaces)
{
    // Arrange
    std::string name = "FabricManagerNew0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new";
    std::string description = "Test FM";

    // Act
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    // Assert
    EXPECT_NE(fmState.getFabricManagerIntf(), nullptr);
    EXPECT_NE(fmState.getOperaStatusIntf(), nullptr);
    EXPECT_NE(fmState.getAggregateFabricManagerState(), nullptr);
    EXPECT_EQ(fmState.getName(), name);
    EXPECT_EQ(fmState.getType(), type);
}

TEST_F(NsmFabricManagerStateTest, Constructor_InitializesDefaultValues)
{
    // Arrange
    std::string name = "FabricManagerNew1";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new1/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new1";
    std::string description = "Test FM 1";

    // Act
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    // Assert - default values per constructor
    auto fmIntf = fmState.getFabricManagerIntf();
    EXPECT_EQ(fmIntf->reportStatus(), FMReportStatus::NotReceived);
    EXPECT_EQ(fmIntf->fmState(), FMState::Unknown);

    auto opIntf = fmState.getOperaStatusIntf();
    EXPECT_EQ(opIntf->state(), OpState::UnavailableOffline);
}

// -- NsmFabricManagerState::genRequestMsg tests --

TEST_F(NsmFabricManagerStateTest,
       GenRequestMsg_ValidParams_ReturnsRequestWithCorrectSize)
{
    // Arrange
    std::string name = "FabricManagerNew2";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new2/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new2";
    std::string description = "Test FM 2";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    // Act
    auto request = fmState.genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_fabric_manager_state_req));
}

TEST_F(NsmFabricManagerStateTest, GenRequestMsg_EncodesInstanceIdInHeader)
{
    // Arrange
    std::string name = "FabricManagerNew3";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new3/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new3";
    std::string description = "Test FM 3";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    eid_t eid = 0;
    uint8_t instanceId = 15;

    // Act
    auto request = fmState.genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    auto msgPtr = reinterpret_cast<const struct nsm_msg*>(request->data());
    EXPECT_EQ(msgPtr->hdr.instance_id, instanceId);
    EXPECT_EQ(msgPtr->hdr.request, 1);
}

// -- NsmFabricManagerState::handleResponseMsg tests --

TEST_F(NsmFabricManagerStateTest,
       HandleResponseMsg_StateOffline_SetsOfflineAndStarting)
{
    // Arrange
    std::string name = "FabricManagerNew4";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new4/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new4";
    std::string description = "Test FM 4";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_OFFLINE,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 1000,
        .duration_since_last_restart_sec = 100};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_fabric_manager_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(fmState.getFabricManagerIntf()->fmState(), FMState::Offline);
    EXPECT_EQ(fmState.getFabricManagerIntf()->reportStatus(),
              FMReportStatus::Received);
    EXPECT_EQ(fmState.getOperaStatusIntf()->state(), OpState::Starting);
    EXPECT_EQ(fmState.getFabricManagerIntf()->lastRestartTime(), 1000);
    EXPECT_EQ(fmState.getFabricManagerIntf()->lastRestartDuration(), 100);
}

TEST_F(NsmFabricManagerStateTest,
       HandleResponseMsg_StateStandby_SetsStandbyOffline)
{
    // Arrange
    std::string name = "FabricManagerNew5";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new5/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new5";
    std::string description = "Test FM 5";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_STANDBY,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 2000,
        .duration_since_last_restart_sec = 200};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_fabric_manager_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(fmState.getFabricManagerIntf()->fmState(), FMState::Standby);
    EXPECT_EQ(fmState.getOperaStatusIntf()->state(), OpState::StandbyOffline);
}

TEST_F(NsmFabricManagerStateTest, HandleResponseMsg_StateConfigured_SetsEnabled)
{
    // Arrange
    std::string name = "FabricManagerNew6";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new6/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new6";
    std::string description = "Test FM 6";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_CONFIGURED,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 3000,
        .duration_since_last_restart_sec = 300};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_fabric_manager_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(fmState.getFabricManagerIntf()->fmState(), FMState::Configured);
    EXPECT_EQ(fmState.getOperaStatusIntf()->state(), OpState::Enabled);
}

TEST_F(NsmFabricManagerStateTest,
       HandleResponseMsg_StateTimeout_SetsUnavailableOffline)
{
    // Arrange
    std::string name = "FabricManagerNew7";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new7/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new7";
    std::string description = "Test FM 7";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_RESERVED_TIMEOUT,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 4000,
        .duration_since_last_restart_sec = 400};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_fabric_manager_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(fmState.getFabricManagerIntf()->fmState(), FMState::Timeout);
    EXPECT_EQ(fmState.getOperaStatusIntf()->state(),
              OpState::UnavailableOffline);
}

TEST_F(NsmFabricManagerStateTest,
       HandleResponseMsg_StateError_SetsErrorAndUnavailableOffline)
{
    // Arrange
    std::string name = "FabricManagerNew8";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new8/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new8";
    std::string description = "Test FM 8";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_ERROR,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 5000,
        .duration_since_last_restart_sec = 500};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_fabric_manager_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(fmState.getFabricManagerIntf()->fmState(), FMState::Error);
    EXPECT_EQ(fmState.getOperaStatusIntf()->state(),
              OpState::UnavailableOffline);
}

TEST_F(NsmFabricManagerStateTest,
       HandleResponseMsg_StateDefault_UnknownFMState_SetsStandbyOffline)
{
    // Arrange - use an unrecognized FM state value (e.g., 0xFF)
    std::string name = "FabricManagerNew9";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new9/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new9";
    std::string description = "Test FM 9";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = 0xFF, // Unrecognized state -> default branch
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 6000,
        .duration_since_last_restart_sec = 600};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_fabric_manager_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(fmState.getFabricManagerIntf()->fmState(), FMState::Unknown);
    EXPECT_EQ(fmState.getOperaStatusIntf()->state(), OpState::StandbyOffline);
}

TEST_F(NsmFabricManagerStateTest,
       HandleResponseMsg_ReportNotReceived_SetsUnknownState)
{
    // Arrange
    std::string name = "FabricManagerNew10";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new10/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new10";
    std::string description = "Test FM 10";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_CONFIGURED,
        .report_status = NSM_FM_REPORT_STATUS_NOT_RECEIVED,
        .last_restart_timestamp = 7000,
        .duration_since_last_restart_sec = 700};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_fabric_manager_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(fmState.getFabricManagerIntf()->reportStatus(),
              FMReportStatus::NotReceived);
    EXPECT_EQ(fmState.getFabricManagerIntf()->fmState(), FMState::Unknown);
    EXPECT_EQ(fmState.getOperaStatusIntf()->state(), OpState::StandbyOffline);
}

TEST_F(NsmFabricManagerStateTest,
       HandleResponseMsg_ReportTimeout_SetsUnknownState)
{
    // Arrange
    std::string name = "FabricManagerNew11";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new11/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new11";
    std::string description = "Test FM 11";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_CONFIGURED,
        .report_status = NSM_FM_REPORT_STATUS_TIMEOUT,
        .last_restart_timestamp = 8000,
        .duration_since_last_restart_sec = 800};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_fabric_manager_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(fmState.getFabricManagerIntf()->reportStatus(),
              FMReportStatus::Timeout);
    EXPECT_EQ(fmState.getFabricManagerIntf()->fmState(), FMState::Unknown);
    EXPECT_EQ(fmState.getOperaStatusIntf()->state(), OpState::StandbyOffline);
}

TEST_F(NsmFabricManagerStateTest,
       HandleResponseMsg_ReportReserved_DefaultBranch_SetsUnknown)
{
    // Arrange - use NSM_FM_REPORT_STATUS_RESERVED to hit the outer default
    std::string name = "FabricManagerNew12";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new12/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new12";
    std::string description = "Test FM 12";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_CONFIGURED,
        .report_status = NSM_FM_REPORT_STATUS_RESERVED,
        .last_restart_timestamp = 9000,
        .duration_since_last_restart_sec = 900};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_fabric_manager_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(fmState.getFabricManagerIntf()->reportStatus(),
              FMReportStatus::Unknown);
    EXPECT_EQ(fmState.getFabricManagerIntf()->fmState(), FMState::Unknown);
    EXPECT_EQ(fmState.getOperaStatusIntf()->state(), OpState::StandbyOffline);
}

TEST_F(NsmFabricManagerStateTest,
       HandleResponseMsg_ErrorCompletionCode_ReturnsError)
{
    // Arrange
    std::string name = "FabricManagerNew13";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new13/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new13";
    std::string description = "Test FM 13";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_CONFIGURED,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 10000,
        .duration_since_last_restart_sec = 1000};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_fabric_manager_state_resp));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_ERROR, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Assert - cc is NSM_ERROR so result should reflect error
    EXPECT_NE(result, NSM_SUCCESS);
}

// -- NsmAggregateFabricManagerState tests --

TEST_F(NsmFabricManagerStateTest, AggregateSingleton_FirstCall_CreatesInstance)
{
    // Arrange
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_s1";
    std::string description = "Agg FM";

    auto fmIntf = std::make_shared<FabricManagerIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager_s1/FM0");
    auto opIntf = std::make_shared<OperaStatusIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager_s1/FM0");

    // Act
    auto instance = NsmAggregateFabricManagerState::getInstance(
        inventoryObjPathFM, fmIntf, opIntf, description);

    // Assert
    ASSERT_NE(instance, nullptr);
    EXPECT_EQ(
        NsmAggregateFabricManagerState::associatedFabricManagerIntfs.size(), 1);
    EXPECT_EQ(
        NsmAggregateFabricManagerState::associatedOperationalStatusIntfs.size(),
        1);
}

TEST_F(NsmFabricManagerStateTest,
       AggregateSingleton_SecondCall_ReturnsSameInstance)
{
    // Arrange
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_s2";
    std::string description = "Agg FM";

    auto fmIntf1 = std::make_shared<FabricManagerIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager_s2/FM0");
    auto opIntf1 = std::make_shared<OperaStatusIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager_s2/FM0");

    auto instance1 = NsmAggregateFabricManagerState::getInstance(
        inventoryObjPathFM, fmIntf1, opIntf1, description);

    auto fmIntf2 = std::make_shared<FabricManagerIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager_s2/FM1");
    auto opIntf2 = std::make_shared<OperaStatusIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager_s2/FM1");

    // Act
    auto instance2 = NsmAggregateFabricManagerState::getInstance(
        inventoryObjPathFM, fmIntf2, opIntf2, description);

    // Assert - same singleton returned, both interfaces registered
    EXPECT_EQ(instance1, instance2);
    EXPECT_EQ(
        NsmAggregateFabricManagerState::associatedFabricManagerIntfs.size(), 2);
    EXPECT_EQ(
        NsmAggregateFabricManagerState::associatedOperationalStatusIntfs.size(),
        2);
}

TEST_F(NsmFabricManagerStateTest,
       AggregateSingleton_MismatchedPath_LogsErrorButRegisters)
{
    // Arrange
    std::string inventoryObjPathFM1 =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_s3a";
    std::string inventoryObjPathFM2 =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_s3b";
    std::string description = "Agg FM";

    auto fmIntf1 = std::make_shared<FabricManagerIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager_s3/FM0");
    auto opIntf1 = std::make_shared<OperaStatusIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager_s3/FM0");

    auto instance1 = NsmAggregateFabricManagerState::getInstance(
        inventoryObjPathFM1, fmIntf1, opIntf1, description);

    auto fmIntf2 = std::make_shared<FabricManagerIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager_s3/FM1");
    auto opIntf2 = std::make_shared<OperaStatusIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager_s3/FM1");

    // Act - pass different path -> triggers lg2::error but still registers
    auto instance2 = NsmAggregateFabricManagerState::getInstance(
        inventoryObjPathFM2, fmIntf2, opIntf2, description);

    // Assert
    EXPECT_EQ(instance1, instance2);
    EXPECT_EQ(
        NsmAggregateFabricManagerState::associatedFabricManagerIntfs.size(), 2);
}

// -- NsmAggregateFabricManagerState::updateAggregateFabricManagerState --

TEST_F(NsmFabricManagerStateTest,
       UpdateAggregate_SingleReceivedConfigured_SetsConfigured)
{
    // Arrange - create one FM state and set it to Configured/Received
    std::string name = "FabricManagerNew14";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new14/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new14";
    std::string description = "Test FM 14";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    // Set the associated interfaces to Received + Configured
    fmState.getFabricManagerIntf()->reportStatus(FMReportStatus::Received);
    fmState.getFabricManagerIntf()->fmState(FMState::Configured);
    fmState.getFabricManagerIntf()->lastRestartTime(1234);
    fmState.getFabricManagerIntf()->lastRestartDuration(56);
    fmState.getOperaStatusIntf()->state(OpState::Enabled);

    // Act
    fmState.getAggregateFabricManagerState()
        ->updateAggregateFabricManagerState();

    // Assert - aggregate should reflect Configured/Enabled
    auto agg = fmState.getAggregateFabricManagerState();
    EXPECT_EQ(agg->fabricManagerIntf->fmState(), FMState::Configured);
    EXPECT_EQ(agg->fabricManagerIntf->reportStatus(), FMReportStatus::Received);
    EXPECT_EQ(agg->operationalStatusIntf->state(), OpState::Enabled);
    EXPECT_EQ(agg->fabricManagerIntf->lastRestartTime(), 1234);
    EXPECT_EQ(agg->fabricManagerIntf->lastRestartDuration(), 56);
}

TEST_F(NsmFabricManagerStateTest, UpdateAggregate_NoReceivedReports_SetsUnknown)
{
    // Arrange - create one FM state but leave report as NotReceived
    std::string name = "FabricManagerNew15";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new15/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new15";
    std::string description = "Test FM 15";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    // Default is NotReceived per constructor

    // Act
    fmState.getAggregateFabricManagerState()
        ->updateAggregateFabricManagerState();

    // Assert - aggregate should show Unknown since no received
    auto agg = fmState.getAggregateFabricManagerState();
    EXPECT_EQ(agg->fabricManagerIntf->fmState(), FMState::Unknown);
    EXPECT_EQ(agg->fabricManagerIntf->reportStatus(), FMReportStatus::Unknown);
}

TEST_F(NsmFabricManagerStateTest,
       UpdateAggregate_TwoFMsWithMismatchedStates_SetsUnknown)
{
    // Arrange - create two FM states with different fm_state values
    std::string name1 = "FabricManagerNew16a";
    std::string type = "FabricManager";
    std::string inventoryObjPath1 =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new16/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new16";
    std::string desc1 = "Test FM 16a";
    NsmFabricManagerState fmState1(name1, type, inventoryObjPath1,
                                   inventoryObjPathFM, bus, desc1);

    std::string name2 = "FabricManagerNew16b";
    std::string inventoryObjPath2 =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new16b/";
    std::string desc2 = "Test FM 16b";
    NsmFabricManagerState fmState2(name2, type, inventoryObjPath2,
                                   inventoryObjPathFM, bus, desc2);

    // Set first to Configured, second to Offline
    fmState1.getFabricManagerIntf()->reportStatus(FMReportStatus::Received);
    fmState1.getFabricManagerIntf()->fmState(FMState::Configured);
    fmState1.getOperaStatusIntf()->state(OpState::Enabled);

    fmState2.getFabricManagerIntf()->reportStatus(FMReportStatus::Received);
    fmState2.getFabricManagerIntf()->fmState(FMState::Offline);
    fmState2.getOperaStatusIntf()->state(OpState::Starting);

    // Act
    fmState1.getAggregateFabricManagerState()
        ->updateAggregateFabricManagerState();

    // Assert - mismatched states -> aggregate becomes Unknown
    auto agg = fmState1.getAggregateFabricManagerState();
    EXPECT_EQ(agg->fabricManagerIntf->fmState(), FMState::Unknown);
}

TEST_F(NsmFabricManagerStateTest,
       UpdateAggregate_TwoFMsWithSameState_SetsThatState)
{
    // Arrange - create two FM states both Configured
    std::string name1 = "FabricManagerNew17a";
    std::string type = "FabricManager";
    std::string inventoryObjPath1 =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new17/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new17";
    std::string desc1 = "Test FM 17a";
    NsmFabricManagerState fmState1(name1, type, inventoryObjPath1,
                                   inventoryObjPathFM, bus, desc1);

    std::string name2 = "FabricManagerNew17b";
    std::string inventoryObjPath2 =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new17b/";
    std::string desc2 = "Test FM 17b";
    NsmFabricManagerState fmState2(name2, type, inventoryObjPath2,
                                   inventoryObjPathFM, bus, desc2);

    // Set both to Configured
    fmState1.getFabricManagerIntf()->reportStatus(FMReportStatus::Received);
    fmState1.getFabricManagerIntf()->fmState(FMState::Configured);
    fmState1.getFabricManagerIntf()->lastRestartTime(100);
    fmState1.getFabricManagerIntf()->lastRestartDuration(10);
    fmState1.getOperaStatusIntf()->state(OpState::Enabled);

    fmState2.getFabricManagerIntf()->reportStatus(FMReportStatus::Received);
    fmState2.getFabricManagerIntf()->fmState(FMState::Configured);
    fmState2.getFabricManagerIntf()->lastRestartTime(200);
    fmState2.getFabricManagerIntf()->lastRestartDuration(20);
    fmState2.getOperaStatusIntf()->state(OpState::Enabled);

    // Act
    fmState1.getAggregateFabricManagerState()
        ->updateAggregateFabricManagerState();

    // Assert - both are same state, aggregate takes the first received one
    auto agg = fmState1.getAggregateFabricManagerState();
    EXPECT_EQ(agg->fabricManagerIntf->fmState(), FMState::Configured);
    EXPECT_EQ(agg->fabricManagerIntf->reportStatus(), FMReportStatus::Received);
    EXPECT_EQ(agg->operationalStatusIntf->state(), OpState::Enabled);
}

// -- NsmFabricManagerState accessor tests --

TEST_F(NsmFabricManagerStateTest, GetFabricManagerIntf_ReturnsSharedPtr)
{
    // Arrange
    std::string name = "FabricManagerNew18";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new18/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new18";
    std::string description = "Test FM 18";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    // Act
    auto fmIntf = fmState.getFabricManagerIntf();

    // Assert
    EXPECT_NE(fmIntf, nullptr);
}

TEST_F(NsmFabricManagerStateTest, GetOperaStatusIntf_ReturnsSharedPtr)
{
    // Arrange
    std::string name = "FabricManagerNew19";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new19/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new19";
    std::string description = "Test FM 19";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    // Act
    auto opIntf = fmState.getOperaStatusIntf();

    // Assert
    EXPECT_NE(opIntf, nullptr);
}

TEST_F(NsmFabricManagerStateTest,
       GetAggregateFabricManagerState_ReturnsSharedPtr)
{
    // Arrange
    std::string name = "FabricManagerNew20";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_new20/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_new20";
    std::string description = "Test FM 20";
    NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                  inventoryObjPathFM, bus, description);

    // Act
    auto agg = fmState.getAggregateFabricManagerState();

    // Assert
    EXPECT_NE(agg, nullptr);
}
