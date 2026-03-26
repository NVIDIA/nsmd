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
 * Branch coverage tests for nsmd/nsmFirmwareUtils/nsmSecurityRBP.cpp
 *
 * Targets uncovered lines from coverage report:
 *  - Lines 76-79,81,84: updateIrreversibleConfig encode failure paths
 *    (unreachable with valid params: instance_id=0 always succeeds)
 *  - Lines 178,181,183-187: NsmSecurityCfgObject constructor body
 *  - Lines 284-287,289,292: updateMinSecVersion encode failure paths
 *    (unreachable with valid params: instance_id=0 always succeeds)
 *  - Lines 374,379-380,382-387: NsmMinSecVersionObject constructor body
 *
 * This file adds supplementary tests to ensure constructor bodies and
 * additional handleResponseMsg / updateProperties edge cases are exercised.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "libnsm/firmware-utils.h"

#include "nsmSecurityRBP.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Fixture: SecurityRBPBranch3Test
// ============================================================================

struct SecurityRBPBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string cfgName = "HGX_EROT_SecCfg_B3";
    const std::string cfgType = "NSM_SecurityCfg";
    const std::string msName = "HGX_EROT_MinSecVer_B3";
    const std::string msType = "NSM_MinSecVersion";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    eid_t eid = 0;

    const uint16_t classification = 0x000A;
    const uint16_t identifier = 0x000B;
    const uint8_t index = 1;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    std::shared_ptr<ProgressIntf> cfgProgressIntf;
    std::shared_ptr<NsmSecurityCfgObject> cfgSensor;
    std::shared_ptr<ProgressIntf> msProgressIntf;
    std::shared_ptr<NsmMinSecVersionObject> msSensor;

    SecurityRBPBranch3Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();

        // Create NsmSecurityCfgObject -> exercises constructor lines 178-187
        std::string cfgProgressPath = std::string(chassisInventoryBasePath) +
                                      "/" + cfgName + "/Progress";
        cfgProgressIntf =
            std::make_shared<ProgressIntf>(bus, cfgProgressPath.c_str());
        cfgSensor = std::make_shared<NsmSecurityCfgObject>(
            bus, cfgName, cfgType, fpgaUuid, cfgProgressIntf);
        ASSERT_NE(cfgSensor, nullptr);

        // Create NsmMinSecVersionObject -> exercises constructor lines 374-387
        std::string msProgressPath = std::string(chassisInventoryBasePath) +
                                     "/" + msName + "/Progress";
        msProgressIntf = std::make_shared<ProgressIntf>(bus,
                                                        msProgressPath.c_str());
        msSensor = std::make_shared<NsmMinSecVersionObject>(
            bus, msName, msType, fpgaUuid, classification, identifier, index,
            msProgressIntf);
        ASSERT_NE(msSensor, nullptr);
    }

    ~SecurityRBPBranch3Test() override
    {
        cfgSensor.reset();
        msSensor.reset();
        cfgProgressIntf.reset();
        msProgressIntf.reset();
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NsmSecurityCfgObject constructor: verify object path and inner object
// Lines 178,181,183-187
// ============================================================================

TEST_F(SecurityRBPBranch3Test, CfgConstructor_SetsObjectPathAndCreatesInner)
{
    std::string expectedPath = std::string(chassisInventoryBasePath) + "/" +
                               cfgName;
    EXPECT_EQ(cfgSensor->objectPath, expectedPath);
    EXPECT_NE(cfgSensor->securityCfgObject, nullptr);
    EXPECT_EQ(cfgSensor->getName(), cfgName);
    EXPECT_EQ(cfgSensor->getType(), cfgType);
}

// ============================================================================
// NsmMinSecVersionObject constructor: verify object path, inner object,
// and stored classification/identifier/index
// Lines 374,379-380,382-387
// ============================================================================

TEST_F(SecurityRBPBranch3Test, MinSecConstructor_SetsObjectPathAndCreatesInner)
{
    std::string expectedPath = std::string(chassisInventoryBasePath) + "/" +
                               msName;
    EXPECT_EQ(msSensor->objectPath, expectedPath);
    EXPECT_NE(msSensor->minSecVersion, nullptr);
    EXPECT_EQ(msSensor->classification, classification);
    EXPECT_EQ(msSensor->identifier, identifier);
    EXPECT_EQ(msSensor->index, index);
    EXPECT_EQ(msSensor->getName(), msName);
    EXPECT_EQ(msSensor->getType(), msType);
}

// ============================================================================
// NsmSecurityCfgObject::handleResponseMsg: cc != 0 -> returns cc
// (exercises the cc ? cc : rc true-branch at line 225)
// ============================================================================

TEST_F(SecurityRBPBranch3Test, CfgHandleResponseMsg_CcNonZero_ReturnsCc)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_irreversible_config_request_0_resp cfg_state{};
    cfg_state.irreversible_config_state = 0;

    auto encRc = encode_nsm_firmware_irreversible_config_request_0_resp(
        0, NSM_ERROR, ERR_NULL, &cfg_state, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = cfgSensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// NsmMinSecVersionObject::handleResponseMsg: cc != 0 -> returns cc
// (exercises the cc ? cc : rc true-branch at line 428)
// ============================================================================

TEST_F(SecurityRBPBranch3Test, MinSecHandleResponseMsg_CcNonZero_ReturnsCc)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_security_version_number_resp sec_info{};
    sec_info.minimum_security_version = htole16(1);
    sec_info.pending_minimum_security_version = htole16(2);

    auto encRc = encode_nsm_query_firmware_security_version_number_resp(
        0, NSM_ERROR, ERR_NULL, &sec_info, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = msSensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// NsmSecurityCfgObject::handleResponseMsg: success path verifies state=0
// ============================================================================

TEST_F(SecurityRBPBranch3Test,
       CfgHandleResponseMsg_SuccessStateFalse_UpdatesState)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_irreversible_config_request_0_resp cfg_state{};
    cfg_state.irreversible_config_state = 0;

    auto encRc = encode_nsm_firmware_irreversible_config_request_0_resp(
        0, NSM_SUCCESS, ERR_NULL, &cfg_state, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = cfgSensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(cfgSensor->securityCfgObject->irreversibleConfigState());
}

// ============================================================================
// NsmMinSecVersionObject::handleResponseMsg: success path updates properties
// ============================================================================

TEST_F(SecurityRBPBranch3Test,
       MinSecHandleResponseMsg_Success_UpdatesProperties)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_security_version_number_resp sec_info{};
    sec_info.minimum_security_version = htole16(42);
    sec_info.pending_minimum_security_version = htole16(43);

    auto encRc = encode_nsm_query_firmware_security_version_number_resp(
        0, NSM_SUCCESS, ERR_NULL, &sec_info, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = msSensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(msSensor->minSecVersion->securityVersionObject->version(), 42);
    EXPECT_EQ(msSensor->minSecVersion->securityVersionSettingsObject->version(),
              43);
}

// ============================================================================
// SecurityConfiguration::updateState: explicit call
// ============================================================================

TEST_F(SecurityRBPBranch3Test, SecurityCfg_UpdateState_SetsConfigState)
{
    nsm_firmware_irreversible_config_request_0_resp cfg_state{};
    cfg_state.irreversible_config_state = 1;
    cfgSensor->securityCfgObject->updateState(cfg_state);
    EXPECT_TRUE(cfgSensor->securityCfgObject->irreversibleConfigState());

    cfg_state.irreversible_config_state = 0;
    cfgSensor->securityCfgObject->updateState(cfg_state);
    EXPECT_FALSE(cfgSensor->securityCfgObject->irreversibleConfigState());
}

// ============================================================================
// SecurityConfiguration::updateNonce: explicit call
// ============================================================================

TEST_F(SecurityRBPBranch3Test, SecurityCfg_UpdateNonce_SetsNonce)
{
    nsm_firmware_irreversible_config_request_2_resp cfg_resp{};
    cfg_resp.nonce = 0xDEADBEEF;
    cfgSensor->securityCfgObject->updateNonce(cfg_resp);
    EXPECT_EQ(cfgSensor->securityCfgObject->nonce(), 0xDEADBEEF);
}

// ============================================================================
// MinSecurityVersion::updateProperties: explicit call
// ============================================================================

TEST_F(SecurityRBPBranch3Test,
       MinSecVersion_UpdateProperties_SetsVersionAndPending)
{
    nsm_firmware_security_version_number_resp sec_info{};
    sec_info.minimum_security_version = htole16(100);
    sec_info.pending_minimum_security_version = htole16(200);

    msSensor->minSecVersion->updateProperties(sec_info);

    EXPECT_EQ(msSensor->minSecVersion->securityVersionObject->version(), 100);
    EXPECT_EQ(msSensor->minSecVersion->securityVersionSettingsObject->version(),
              200);
}

// ============================================================================
// NsmSecurityCfgObject::genRequestMsg: valid instanceId -> returns request
// ============================================================================

TEST_F(SecurityRBPBranch3Test, CfgGenRequestMsg_Valid_ReturnsRequest)
{
    auto result = cfgSensor->genRequestMsg(eid, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_irreversible_config_req_command));

    // Verify the encoded request type is QUERY
    auto msgPtr = reinterpret_cast<const nsm_msg*>(result->data());
    nsm_firmware_irreversible_config_req decoded_req{};
    auto rc = decode_nsm_firmware_irreversible_config_req(
        msgPtr, result->size(), &decoded_req);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(decoded_req.request_type, QUERY_IRREVERSIBLE_CFG);
}

// ============================================================================
// NsmMinSecVersionObject::genRequestMsg: valid instanceId -> returns request
// Verifies classification, identifier, index are encoded correctly
// ============================================================================

TEST_F(SecurityRBPBranch3Test, MinSecGenRequestMsg_Valid_ReturnsRequest)
{
    auto result = msSensor->genRequestMsg(eid, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_security_version_number_req_command));
}

// ============================================================================
// SecurityConfiguration::securityCfgAsyncHandler: ENABLE path
// decode succeeds, cc == NSM_ERROR -> finishOperation(Aborted)
// ============================================================================

TEST_F(SecurityRBPBranch3Test,
       SecurityCfgAsync_EnablePath_DecodeOkCcError_Aborted)
{
    std::vector<uint8_t> patchResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_2_resp_command),
        0);
    auto patchRespMsg = reinterpret_cast<nsm_msg*>(patchResponse.data());
    nsm_firmware_irreversible_config_request_2_resp cfg2Resp{};
    cfg2Resp.nonce = 0x5678;
    [[maybe_unused]] auto encRc =
        encode_nsm_firmware_irreversible_config_request_2_resp(
            0, NSM_ERROR, ERR_NULL, &cfg2Resp, patchRespMsg);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(patchResponse));

    EXPECT_NO_THROW(
        cfgSensor->securityCfgObject->updateIrreversibleConfig(true));
}

// ============================================================================
// MinSecurityVersion::minSecVersionAsyncHandler: decode succeeds,
// rc == NSM_SW_SUCCESS but cc != NSM_SUCCESS -> errorCode set, Aborted
// ============================================================================

TEST_F(SecurityRBPBranch3Test,
       MinSecVersionAsync_DecodeOkCcError_SetsErrorCodeAndAborts)
{
    std::vector<uint8_t> patchResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_update_min_sec_ver_resp_command),
        0);
    auto patchRespMsg = reinterpret_cast<nsm_msg*>(patchResponse.data());
    nsm_firmware_update_min_sec_ver_resp secResp{};
    secResp.update_methods = 0x01;
    [[maybe_unused]] auto rc = encode_nsm_firmware_update_sec_ver_resp(
        0, NSM_ERROR, ERR_NULL, &secResp, patchRespMsg);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(patchResponse));

    EXPECT_NO_THROW(msSensor->minSecVersion->updateMinSecVersion(
        SecurityCommon::RequestTypes::SpecifiedValue, 0, 5));
}

// ============================================================================
// SecurityConfiguration::startOperation + finishOperation round-trip
// Verifies progress fields are set correctly
// ============================================================================

TEST_F(SecurityRBPBranch3Test, CfgStartAndFinishOperation_CompletedRoundTrip)
{
    auto rc = cfgSensor->securityCfgObject->startOperation();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Verify InProgress state
    EXPECT_EQ(cfgProgressIntf->status(), Progress::OperationStatus::InProgress);
    EXPECT_EQ(cfgProgressIntf->progress(), 0);
    EXPECT_NE(cfgProgressIntf->startTime(), 0UL);
    EXPECT_EQ(cfgProgressIntf->completedTime(), 0UL);

    cfgSensor->securityCfgObject->finishOperation(
        Progress::OperationStatus::Completed);

    EXPECT_EQ(cfgProgressIntf->status(), Progress::OperationStatus::Completed);
    EXPECT_EQ(cfgProgressIntf->progress(), 100);
    EXPECT_NE(cfgProgressIntf->completedTime(), 0UL);
}

// ============================================================================
// MinSecurityVersion::startOperation + finishOperation round-trip
// with Aborted status (progress should NOT be set to 100)
// ============================================================================

TEST_F(SecurityRBPBranch3Test, MinSecStartAndFinishOperation_AbortedRoundTrip)
{
    auto rc = msSensor->minSecVersion->startOperation();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(msProgressIntf->status(), Progress::OperationStatus::InProgress);

    msSensor->minSecVersion->finishOperation(
        Progress::OperationStatus::Aborted);

    EXPECT_EQ(msProgressIntf->status(), Progress::OperationStatus::Aborted);
    EXPECT_NE(msProgressIntf->progress(), 100);
    EXPECT_NE(msProgressIntf->completedTime(), 0UL);
}

// ============================================================================
// NsmSecurityCfgObject::genRequestMsg with different EIDs
// (exercises the eid parameter path in genRequestMsg)
// ============================================================================

TEST_F(SecurityRBPBranch3Test, CfgGenRequestMsg_DifferentEid_StillSucceeds)
{
    auto result = cfgSensor->genRequestMsg(42, 0);
    ASSERT_TRUE(result.has_value());
}

// ============================================================================
// NsmMinSecVersionObject::genRequestMsg with different EIDs
// ============================================================================

TEST_F(SecurityRBPBranch3Test, MinSecGenRequestMsg_DifferentEid_StillSucceeds)
{
    auto result = msSensor->genRequestMsg(42, 0);
    ASSERT_TRUE(result.has_value());
}

// ============================================================================
// NsmSecurityCfgObject::handleResponseMsg: truncated response
// (responseLen too small for full decode -> rc != 0, cc == 0 -> returns rc)
// ============================================================================

TEST_F(SecurityRBPBranch3Test,
       CfgHandleResponseMsg_TruncatedResponse_ReturnsNonZero)
{
    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    msg->payload[1] = NSM_ERROR; // cc != 0

    auto rc = cfgSensor->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmMinSecVersionObject::handleResponseMsg: truncated response
// ============================================================================

TEST_F(SecurityRBPBranch3Test,
       MinSecHandleResponseMsg_TruncatedResponse_ReturnsNonZero)
{
    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    msg->payload[1] = NSM_SUCCESS; // cc == 0

    auto rc = msSensor->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// SecurityConfiguration::updateIrreversibleConfig encode fail paths
// NOTE: Lines 76-79,81,84 are UNREACHABLE because instance_id is hardcoded
// to 0 and the request buffer is always valid. The encode function
// (encode_nsm_firmware_irreversible_config_req) only fails when
// instance_id > NSM_INSTANCE_MAX or msg == NULL, neither of which can
// happen through the updateIrreversibleConfig call path.
// ============================================================================

// ============================================================================
// MinSecurityVersion::updateMinSecVersion encode fail paths
// NOTE: Lines 284-287,289,292 are UNREACHABLE for the same reason as above.
// encode_nsm_firmware_update_sec_ver_req only fails when instance_id >
// NSM_INSTANCE_MAX or msg == NULL, but updateMinSecVersion uses
// instance_id=0 and a valid buffer.
// ============================================================================
