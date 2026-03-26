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
 * Targets:
 *  - SecurityConfiguration::updateIrreversibleConfig: state=true vs false,
 *    startOperation mutex locked -> Unavailable (via updateIrreversibleConfig)
 *  - SecurityConfiguration::securityCfgAsyncHandler: ENABLE path full success,
 *    DISABLE path full success, DISABLE path cc!=SUCCESS
 *  - NsmSecurityCfgObject: genRequestMsg encode fail (instanceId > MAX)
 *  - NsmMinSecVersionObject: genRequestMsg encode fail (instanceId > MAX),
 *    handleResponseMsg decode fail with cc==0 -> returns rc
 *  - MinSecurityVersion::updateMinSecVersion: MostRestrictiveValue vs
 *    SpecifiedValue request types, startOperation locked -> Unavailable
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
// Fixture: SecurityRBPBranch2Test
// ============================================================================

struct SecurityRBPBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "HGX_EROT_SecCfg_B2";
    const std::string type = "NSM_SecurityCfg";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    eid_t eid = 0;

    const std::string msName = "HGX_EROT_MinSecVer_B2";
    const std::string msType = "NSM_MinSecVersion";
    const uint16_t classification = 0x0001;
    const uint16_t identifier = 0x0002;
    const uint8_t index = 0;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    std::shared_ptr<ProgressIntf> progressIntf;
    std::shared_ptr<NsmSecurityCfgObject> cfgSensor;
    std::shared_ptr<ProgressIntf> msProgressIntf;
    std::shared_ptr<NsmMinSecVersionObject> msSensor;

    SecurityRBPBranch2Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();

        // SecurityCfg sensor
        std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                                   name + "/Progress";
        progressIntf = std::make_shared<ProgressIntf>(bus,
                                                      progressPath.c_str());
        cfgSensor = std::make_shared<NsmSecurityCfgObject>(
            bus, name, type, fpgaUuid, progressIntf);
        ASSERT_NE(cfgSensor, nullptr);

        // MinSecVersion sensor
        std::string msProgressPath = std::string(chassisInventoryBasePath) +
                                     "/" + msName + "/Progress";
        msProgressIntf = std::make_shared<ProgressIntf>(bus,
                                                        msProgressPath.c_str());
        msSensor = std::make_shared<NsmMinSecVersionObject>(
            bus, msName, msType, fpgaUuid, classification, identifier, index,
            msProgressIntf);
        ASSERT_NE(msSensor, nullptr);
    }

    ~SecurityRBPBranch2Test() override
    {
        cfgSensor.reset();
        msSensor.reset();
        progressIntf.reset();
        msProgressIntf.reset();
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NsmSecurityCfgObject::genRequestMsg encode fail
// ============================================================================

TEST_F(SecurityRBPBranch2Test,
       CfgGenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    auto result = cfgSensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// NsmMinSecVersionObject::genRequestMsg encode fail
// ============================================================================

TEST_F(SecurityRBPBranch2Test,
       MinSecVerGenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    auto result = msSensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// NsmMinSecVersionObject::handleResponseMsg: decode fail with cc==0
// returns rc (the "cc ? cc : rc" false-branch, rc != 0)
// ============================================================================

TEST_F(SecurityRBPBranch2Test,
       MinSecVerHandleResponseMsg_DecodeFailCcZero_ReturnsRc)
{
    // Buffer large enough for decode_reason_code_and_cc (valgrind-safe) but
    // too small for full nsm_firmware_security_version_number_resp_command.
    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    msg->payload[1] = NSM_SUCCESS; // cc == 0

    auto rc = msSensor->handleResponseMsg(msg, buf.size());
    // cc == 0, rc != 0 -> returns rc (non-zero)
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmSecurityCfgObject::handleResponseMsg: decode fail with cc==0
// returns rc (the "cc ? cc : rc" false-branch, rc != 0)
// ============================================================================

TEST_F(SecurityRBPBranch2Test,
       DISABLED_CfgHandleResponseMsg_DecodeFailCcZero_ReturnsRc)
{
    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    msg->payload[1] = NSM_SUCCESS; // cc == 0

    auto rc = cfgSensor->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// SecurityConfiguration::updateIrreversibleConfig: state=true (ENABLE)
// Full success path through securityCfgAsyncHandler ENABLE branch
// ============================================================================

TEST_F(SecurityRBPBranch2Test,
       UpdateIrreversibleConfig_EnableTrue_FullSuccessPath)
{
    // Build a valid response for ENABLE path (request_2_resp)
    std::vector<uint8_t> patchResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_2_resp_command),
        0);
    auto patchRespMsg = reinterpret_cast<nsm_msg*>(patchResponse.data());
    struct nsm_firmware_irreversible_config_request_2_resp cfg2Resp{};
    cfg2Resp.nonce = 0x1234;
    [[maybe_unused]] auto encRc =
        encode_nsm_firmware_irreversible_config_request_2_resp(
            0, NSM_SUCCESS, ERR_NULL, &cfg2Resp, patchRespMsg);

    // Build a valid sensorIO response for nsmSensor.update() that follows
    std::vector<uint8_t> sensorResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto sensorRespMsg = reinterpret_cast<nsm_msg*>(sensorResponse.data());
    struct nsm_firmware_irreversible_config_request_0_resp cfgState{};
    cfgState.irreversible_config_state = 1;
    [[maybe_unused]] auto encRc2 =
        encode_nsm_firmware_irreversible_config_request_0_resp(
            0, NSM_SUCCESS, ERR_NULL, &cfgState, sensorRespMsg);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(patchResponse));
    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(sensorResponse));

    EXPECT_NO_THROW(
        cfgSensor->securityCfgObject->updateIrreversibleConfig(true));
}

// ============================================================================
// SecurityConfiguration::securityCfgAsyncHandler: DISABLE path full success
// ============================================================================

TEST_F(SecurityRBPBranch2Test,
       UpdateIrreversibleConfig_DisableFalse_FullSuccessPath)
{
    // Build a valid response for DISABLE path (request_1_resp)
    std::vector<uint8_t> patchResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_1_resp_command),
        0);
    auto patchRespMsg = reinterpret_cast<nsm_msg*>(patchResponse.data());
    [[maybe_unused]] auto encRc =
        encode_nsm_firmware_irreversible_config_request_1_resp(
            0, NSM_SUCCESS, ERR_NULL, patchRespMsg);

    // Build sensorIO response for nsmSensor.update()
    std::vector<uint8_t> sensorResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto sensorRespMsg = reinterpret_cast<nsm_msg*>(sensorResponse.data());
    struct nsm_firmware_irreversible_config_request_0_resp cfgState{};
    cfgState.irreversible_config_state = 0;
    [[maybe_unused]] auto encRc2 =
        encode_nsm_firmware_irreversible_config_request_0_resp(
            0, NSM_SUCCESS, ERR_NULL, &cfgState, sensorRespMsg);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(patchResponse));
    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(sensorResponse));

    EXPECT_NO_THROW(
        cfgSensor->securityCfgObject->updateIrreversibleConfig(false));
}

// ============================================================================
// SecurityConfiguration::securityCfgAsyncHandler: DISABLE path
// decode succeeds but cc != NSM_SUCCESS -> finishOperation(Aborted)
// ============================================================================

TEST_F(SecurityRBPBranch2Test,
       SecurityCfgAsync_DisablePath_DecodeOkCcError_Aborted)
{
    // Build response with valid decode but cc == NSM_ERROR for DISABLE path
    std::vector<uint8_t> patchResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_1_resp_command),
        0);
    auto patchRespMsg = reinterpret_cast<nsm_msg*>(patchResponse.data());
    [[maybe_unused]] auto encRc =
        encode_nsm_firmware_irreversible_config_request_1_resp(
            0, NSM_ERROR, ERR_NULL, patchRespMsg);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(patchResponse));

    EXPECT_NO_THROW(
        cfgSensor->securityCfgObject->updateIrreversibleConfig(false));
}

// ============================================================================
// SecurityConfiguration::securityCfgAsyncHandler: postPatchIO fails
// ============================================================================

TEST_F(SecurityRBPBranch2Test, SecurityCfgAsync_PostPatchIOFail_EnablePath)
{
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    EXPECT_NO_THROW(
        cfgSensor->securityCfgObject->updateIrreversibleConfig(true));
}

// ============================================================================
// SecurityConfiguration::updateIrreversibleConfig: mutex already locked
// -> startOperation throws Unavailable
// ============================================================================

TEST_F(SecurityRBPBranch2Test,
       UpdateIrreversibleConfig_MutexLocked_ThrowsUnavailable)
{
    // Lock the mutex via startOperation
    EXPECT_EQ(cfgSensor->securityCfgObject->startOperation(), NSM_SW_SUCCESS);

    // Now calling updateIrreversibleConfig should throw Unavailable //

    EXPECT_THROW(cfgSensor->securityCfgObject->updateIrreversibleConfig(true),
                 sdbusplus::error::xyz::openbmc_project::common::Unavailable);

    // Cleanup
    cfgSensor->securityCfgObject->mutex.unlock();
}

// ============================================================================
// MinSecurityVersion::updateMinSecVersion: MostRestrictiveValue request type
// with full success path
// ============================================================================

TEST_F(SecurityRBPBranch2Test, MinSecVersion_MostRestrictiveValue_FullSuccess)
{
    // Build postPatchIO response
    std::vector<uint8_t> patchResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_update_min_sec_ver_resp_command),
        0);
    auto patchRespMsg = reinterpret_cast<nsm_msg*>(patchResponse.data());
    struct nsm_firmware_update_min_sec_ver_resp secResp{};
    secResp.update_methods = 0x01;
    [[maybe_unused]] auto rc = encode_nsm_firmware_update_sec_ver_resp(
        0, NSM_SUCCESS, ERR_NULL, &secResp, patchRespMsg);

    // Build sensorIO response for nsmSensor.update()
    std::vector<uint8_t> sensorResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_resp_command),
        0);
    auto sensorRespMsg = reinterpret_cast<nsm_msg*>(sensorResponse.data());
    struct nsm_firmware_security_version_number_resp secInfo{};
    secInfo.minimum_security_version = htole16(5);
    secInfo.pending_minimum_security_version = htole16(6);
    [[maybe_unused]] auto rc2 =
        encode_nsm_query_firmware_security_version_number_resp(
            0, NSM_SUCCESS, ERR_NULL, &secInfo, sensorRespMsg);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(patchResponse));
    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(sensorResponse));

    EXPECT_NO_THROW(msSensor->minSecVersion->updateMinSecVersion(
        SecurityCommon::RequestTypes::MostRestrictiveValue, 0, 0));
}

// ============================================================================
// MinSecurityVersion::updateMinSecVersion: SpecifiedValue request type
// with full success path
// ============================================================================

TEST_F(SecurityRBPBranch2Test, MinSecVersion_SpecifiedValue_FullSuccess)
{
    // Build postPatchIO response
    std::vector<uint8_t> patchResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_update_min_sec_ver_resp_command),
        0);
    auto patchRespMsg = reinterpret_cast<nsm_msg*>(patchResponse.data());
    struct nsm_firmware_update_min_sec_ver_resp secResp{};
    secResp.update_methods = 0x02;
    [[maybe_unused]] auto rc = encode_nsm_firmware_update_sec_ver_resp(
        0, NSM_SUCCESS, ERR_NULL, &secResp, patchRespMsg);

    // Build sensorIO response for nsmSensor.update()
    std::vector<uint8_t> sensorResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_resp_command),
        0);
    auto sensorRespMsg = reinterpret_cast<nsm_msg*>(sensorResponse.data());
    struct nsm_firmware_security_version_number_resp secInfo{};
    secInfo.minimum_security_version = htole16(10);
    secInfo.pending_minimum_security_version = htole16(11);
    [[maybe_unused]] auto rc2 =
        encode_nsm_query_firmware_security_version_number_resp(
            0, NSM_SUCCESS, ERR_NULL, &secInfo, sensorRespMsg);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(patchResponse));
    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(sensorResponse));

    EXPECT_NO_THROW(msSensor->minSecVersion->updateMinSecVersion(
        SecurityCommon::RequestTypes::SpecifiedValue, 12345, 10));
}

// ============================================================================
// MinSecurityVersion::updateMinSecVersion: mutex already locked
// -> startOperation throws Unavailable
// ============================================================================

TEST_F(SecurityRBPBranch2Test,
       MinSecVersion_UpdateMinSecVersion_MutexLocked_ThrowsUnavailable)
{
    EXPECT_EQ(msSensor->minSecVersion->startOperation(), NSM_SW_SUCCESS);

    EXPECT_THROW(msSensor->minSecVersion->updateMinSecVersion(
                     SecurityCommon::RequestTypes::MostRestrictiveValue, 0, 0),
                 sdbusplus::error::xyz::openbmc_project::common::Unavailable);

    msSensor->minSecVersion->mutex.unlock();
}

// ============================================================================
// MinSecurityVersion::minSecVersionAsyncHandler: postPatchIO fail
// ============================================================================

TEST_F(SecurityRBPBranch2Test, MinSecVersionAsync_PostPatchIOFail)
{
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    EXPECT_NO_THROW(msSensor->minSecVersion->updateMinSecVersion(
        SecurityCommon::RequestTypes::SpecifiedValue, 0, 5));
}

// ============================================================================
// MinSecurityVersion::minSecVersionAsyncHandler: decode OK but cc != SUCCESS
// ============================================================================

TEST_F(SecurityRBPBranch2Test, MinSecVersionAsync_DecodedOkCcError)
{
    std::vector<uint8_t> patchResponse(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_update_min_sec_ver_resp_command),
        0);
    auto patchRespMsg = reinterpret_cast<nsm_msg*>(patchResponse.data());
    struct nsm_firmware_update_min_sec_ver_resp secResp{};
    secResp.update_methods = 0x01;
    [[maybe_unused]] auto rc = encode_nsm_firmware_update_sec_ver_resp(
        0, NSM_ERROR, ERR_NULL, &secResp, patchRespMsg);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(patchResponse));

    EXPECT_NO_THROW(msSensor->minSecVersion->updateMinSecVersion(
        SecurityCommon::RequestTypes::MostRestrictiveValue, 0, 0));
}

// ============================================================================
// SecurityConfiguration::securityCfgAsyncHandler: ENABLE path
// decode fails (buffer too short) -> finishOperation(Aborted)
// ============================================================================

TEST_F(SecurityRBPBranch2Test, SecurityCfgAsync_EnablePath_DecodeFail_Aborted)
{
    // Short buffer that triggers decode failure for request_2_resp
    std::vector<uint8_t> shortResponse(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    shortResponse[sizeof(nsm_msg_hdr) + 1] = NSM_SUCCESS; // cc = SUCCESS
    // rc will be non-zero because buffer is too short for full decode

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(shortResponse));

    EXPECT_NO_THROW(
        cfgSensor->securityCfgObject->updateIrreversibleConfig(true));
}

// ============================================================================
// SecurityConfiguration::securityCfgAsyncHandler: DISABLE path
// decode fails (buffer too short, cc=SUCCESS) -> rc!=0 -> Aborted
// ============================================================================

TEST_F(SecurityRBPBranch2Test,
       SecurityCfgAsync_DisablePath_DecodeFailCcSuccess_Aborted)
{
    std::vector<uint8_t> shortResponse(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    shortResponse[sizeof(nsm_msg_hdr) + 1] = NSM_SUCCESS;

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(shortResponse));
    // In coverage mode (COVERAGE_DISABLE_COROUTINES) coroutine suspension
    // points behave differently, causing nsmSensor.update() at line 142 to
    // execute. Use a non-coroutine lambda so the returned Coroutine is
    // constructed via Coroutine(T&&) which initialises promise.data, avoiding
    // the uninitialized-value read that valgrind detects when the coroutine
    // path produces a Coroutine{handle} with promise.data uninitialised.
    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .Times(AtMost(1))
        .WillRepeatedly([](eid_t, Request&, std::shared_ptr<const nsm_msg>&,
                           size_t& /*responseLen*/,
                           bool) -> requester::Coroutine {
#ifdef COVERAGE_DISABLE_COROUTINES
        return requester::Coroutine(NSM_SUCCESS);
#else
        co_return NSM_SUCCESS;
#endif
    });

    EXPECT_NO_THROW(
        cfgSensor->securityCfgObject->updateIrreversibleConfig(false));
}

// ============================================================================
// SecurityConfiguration::finishOperation: Completed vs non-Completed status
// ============================================================================

TEST_F(SecurityRBPBranch2Test, CfgFinishOperation_Completed_SetsProgress100)
{
    EXPECT_EQ(cfgSensor->securityCfgObject->startOperation(), NSM_SW_SUCCESS);
    cfgSensor->securityCfgObject->finishOperation(
        Progress::OperationStatus::Completed);

    EXPECT_EQ(cfgSensor->securityCfgObject->progressIntf->progress(), 100);
    EXPECT_EQ(cfgSensor->securityCfgObject->progressIntf->status(),
              Progress::OperationStatus::Completed);
}

TEST_F(SecurityRBPBranch2Test,
       CfgFinishOperation_NonCompleted_DoesNotSetProgress100)
{
    EXPECT_EQ(cfgSensor->securityCfgObject->startOperation(), NSM_SW_SUCCESS);
    cfgSensor->securityCfgObject->finishOperation(
        Progress::OperationStatus::Aborted);

    EXPECT_NE(cfgSensor->securityCfgObject->progressIntf->progress(), 100);
}

TEST_F(SecurityRBPBranch2Test, MinSecFinishOperation_Completed_SetsProgress100)
{
    EXPECT_EQ(msSensor->minSecVersion->startOperation(), NSM_SW_SUCCESS);
    msSensor->minSecVersion->finishOperation(
        Progress::OperationStatus::Completed);

    EXPECT_EQ(msSensor->minSecVersion->progressIntf->progress(), 100);
}

TEST_F(SecurityRBPBranch2Test,
       MinSecFinishOperation_NonCompleted_DoesNotSetProgress100)
{
    EXPECT_EQ(msSensor->minSecVersion->startOperation(), NSM_SW_SUCCESS);
    msSensor->minSecVersion->finishOperation(
        Progress::OperationStatus::Aborted);

    EXPECT_NE(msSensor->minSecVersion->progressIntf->progress(), 100);
}
