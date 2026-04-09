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

#include "test/mockDBusHandler.hpp"

#define private public
#define protected public

#include "libnsm/firmware-utils.h"

#include "nsmDot.hpp"
#include "test/mockSensorManager.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <vector>

using namespace nsm;
using namespace ::testing;
using sdbusplus::message::unix_fd;

NsmDeviceTable devices;
std::shared_ptr<MockNsmDeviceBase> mockDevice;

/**
 * @brief Creates a temporary file with the given data and returns its file
 * descriptor
 *
 * Creates a temporary file using mkstemp, writes the provided data to it,
 * resets the file position to the beginning, and unlinks the file (keeping
 * the fd open for use). The caller is responsible for closing the fd.
 *
 * @param data The data to write to the temporary file
 * @return File descriptor of the temporary file, or -1 on error
 */
int createTempFileWithData(const std::vector<uint8_t>& data)
{
    char tempPath[] = "/tmp/dot_recovery_test_XXXXXX";
    int fd = mkstemp(tempPath);
    EXPECT_NE(fd, -1);

    if (fd != -1)
    {
        ssize_t written = write(fd, data.data(), data.size());
        EXPECT_EQ(written, static_cast<ssize_t>(data.size()));
        lseek(fd, 0, SEEK_SET);
        unlink(tempPath);
    }

    return fd;
}

class NsmDotTest : public Test, public SensorManagerTest
{
  protected:
    NsmDotTest() : SensorManagerTest(devices)
    {
        uuid_t testUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
        mockDevice = std::dynamic_pointer_cast<MockNsmDeviceBase>(
            mockManager.getNsmDeviceFromStaticUUID(testUuid));

        dotObject = std::make_unique<NsmDotObject>(utils::DBusHandler::getBus(),
                                                   "test_dot", testUuid);
    }

    Response createDotCAKInstallResponse(uint8_t completionCode = NSM_SUCCESS,
                                         uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());

        auto rc = encode_nsm_dot_cak_install_resp(0, completionCode, reasonCode,
                                                  msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createDotBypassResponse(uint8_t completionCode = NSM_SUCCESS,
                                     uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());

        auto rc = encode_nsm_dot_cak_bypass_resp(0, completionCode, reasonCode,
                                                 msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createDotLockResponse(uint8_t completionCode = NSM_SUCCESS,
                                   uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());

        std::vector<uint8_t> dotBlob(148, 0x42);
        auto rc = encode_nsm_dot_lock_resp(0, completionCode, reasonCode,
                                           dotBlob.data(), dotBlob.size(), msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response
        createDotUnlockChallengeResponse(uint8_t completionCode = NSM_SUCCESS,
                                         uint16_t reasonCode = 0)
    {
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());

        uint8_t challenge[32];
        std::fill_n(challenge, 32, 0xAB);
        auto rc = encode_nsm_dot_unlock_challenge_resp(
            0, completionCode, reasonCode, challenge, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createDotUnlockResponse(uint8_t completionCode = NSM_SUCCESS,
                                     uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());

        auto rc = encode_nsm_dot_unlock_resp(0, completionCode, reasonCode,
                                             msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createDotCAKRotateResponse(uint8_t completionCode = NSM_SUCCESS,
                                        uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_rotate_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());

        std::vector<uint8_t> dotBlob(148, 0x55);
        auto rc = encode_nsm_dot_cak_rotate_resp(
            0, completionCode, reasonCode, dotBlob.data(), dotBlob.size(), msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createDotGetInfoResponse(uint8_t completionCode = NSM_SUCCESS,
                                      uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());

        uint8_t version = 1;
        uint8_t fuseChangeState = 0;
        uint8_t transfersRemaining = 5;
        std::vector<uint8_t> dotBlob(148, 0x77);

        auto rc = encode_nsm_dot_get_info_resp(
            0, completionCode, reasonCode, version, fuseChangeState,
            transfersRemaining, dotBlob.data(), dotBlob.size(), msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createDotDisableResponse(uint8_t completionCode = NSM_SUCCESS,
                                      uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());

        std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xAA);
        auto rc = encode_nsm_dot_disable_resp(0, completionCode, reasonCode,
                                              dotBlob.data(), msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createDotOverrideResponse(uint8_t completionCode = NSM_SUCCESS,
                                       uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());

        auto rc = encode_nsm_dot_override_resp(0, completionCode, reasonCode,
                                               msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createDotRecoveryResponse(uint8_t completionCode = NSM_SUCCESS,
                                       uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());

        auto rc = encode_nsm_dot_recovery_resp(0, completionCode, reasonCode,
                                               msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    auto callDotCAKInstallAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        std::string ecdsaKey = std::string(192, '0');
        std::string lmsKey = "";

        auto rc = dotObject
                      ->dotCAKInstallAsyncHandler(
                          DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaKey, lmsKey,
                          DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaKey, lmsKey,
                          false, 0, 0, statusInterface, valueInterface)
                      .await_resume();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callBypassAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        auto rc = dotObject->bypassAsyncHandler(statusInterface, valueInterface)
                      .await_resume();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callLockAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        std::string ecdsaKey = std::string(192, '0');       // 96 bytes in hex
        std::string lmsKey = "";
        std::string staticChallenge = std::string(64, '0'); // 32 bytes in hex
        std::string ecdsaSignature = std::string(192, '0'); // 96 bytes in hex

        DotLockParams params{
            .cakKeyAuthScheme = DotActionIntf::KeyAuthScheme::Ecdsa,
            .cakEcdsaKey = ecdsaKey,
            .cakLmsKey = lmsKey,
            .lakKeyAuthScheme = DotActionIntf::KeyAuthScheme::Ecdsa,
            .lakEcdsaKey = ecdsaKey,
            .lakLmsKey = lmsKey,
            .unlockMethod = DotActionIntf::UnlockMethod::StaticValue,
            .staticChallenge = staticChallenge,
            .lockSignatureAuthScheme = DotActionIntf::KeyAuthScheme::Ecdsa,
            .ecdsaSignature = ecdsaSignature,
            .lmsSignature = lmsKey};

        auto rc =
            dotObject->lockAsyncHandler(params, statusInterface, valueInterface)
                .await_resume();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callUnlockChallengeAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        auto rc = dotObject
                      ->unlockChallengeAsyncHandler(
                          DotActionIntf::UnlockType::OwnerUnlock,
                          statusInterface, valueInterface)
                      .await_resume();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callUnlockAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        std::string ecdsaSignature = std::string(192, '0'); // 96 bytes in hex
        std::string lmsSignature = "";

        auto rc = dotObject
                      ->unlockAsyncHandler(DotActionIntf::KeyAuthScheme::Ecdsa,
                                           ecdsaSignature, lmsSignature,
                                           statusInterface, valueInterface)
                      .await_resume();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callCAKRotateAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        std::string ecdsaKey = std::string(192, '0'); // 96 bytes in hex (CAK)
        std::string lmsKey = "";
        std::string ecdsaSignature = std::string(192, '0'); // 96 bytes in hex
        std::string lmsSignature = "";

        auto rc = dotObject
                      ->cakRotateAsyncHandler(
                          DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaKey, lmsKey,
                          DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaSignature,
                          lmsSignature, statusInterface, valueInterface)
                      .await_resume();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callGetInfoAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        auto rc = dotObject
                      ->getInfoAsyncHandler(statusInterface, valueInterface)
                      .await_resume();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callDisableAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        // Valid ECDSA key (96 bytes in hex)
        std::string ecdsaKey = std::string(192, '0'); // 96 bytes * 2 hex chars
        std::string lmsKey = "";
        std::string ecdsaSig = std::string(192, '1'); // 96 bytes * 2 hex chars
        std::string lmsSig = "";

        DotDisableParams params{DotActionIntf::KeyAuthScheme::Ecdsa,
                                ecdsaKey,
                                lmsKey,
                                DotActionIntf::UnlockMethod::OwnerUnlock,
                                "",
                                DotActionIntf::KeyAuthScheme::Ecdsa,
                                ecdsaSig,
                                lmsSig};

        auto rc = dotObject
                      ->disableAsyncHandler(params, statusInterface,
                                            valueInterface)
                      .await_resume();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callOverrideAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        std::string ecdsaSig = std::string(192, '2'); // 96 bytes * 2 hex chars
        std::string lmsSig = "";

        auto rc = dotObject
                      ->overrideAsyncHandler(
                          DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaSig, lmsSig,
                          statusInterface, valueInterface)
                      .await_resume();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callRecoveryAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xAA);
        int fd = createTempFileWithData(dotBlob);
        ASSERT_NE(fd, -1);

        auto rc = dotObject
                      ->recoverDOTAsyncHandler(unix_fd(fd), statusInterface,
                                               valueInterface)
                      .await_resume();
        close(fd);
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    std::unique_ptr<NsmDotObject> dotObject;
};

TEST_F(NsmDotTest, ConstructorSetsInitialDOTStateToUnknown)
{
    EXPECT_EQ(dotObject->dotState(), DotActionIntf::DOTStates::Unknown);
}

TEST_F(NsmDotTest, DotCAKInstallSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotCAKInstallResponse()));

    auto path =
        dotObject->dotCAKInstall(DotActionIntf::KeyAuthScheme::Ecdsa,
                                 std::string(192, '0'), // 96 bytes in hex
                                 "", DotActionIntf::KeyAuthScheme::Ecdsa,
                                 std::string(192, '0'), // 96 bytes in hex
                                 "", false, 0, 0);

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

TEST_F(NsmDotTest, DotCAKInstallInvalidCAKEcdsaKey)
{
    EXPECT_THROW(
        dotObject->dotCAKInstall(DotActionIntf::KeyAuthScheme::Ecdsa,
                                 "invalid_key", "",
                                 DotActionIntf::KeyAuthScheme::Ecdsa,
                                 std::string(192, '0'), "", false, 0, 0),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, DotCAKInstallInvalidLAKEcdsaKey)
{
    EXPECT_THROW(
        dotObject->dotCAKInstall(DotActionIntf::KeyAuthScheme::Ecdsa,
                                 std::string(192, '0'), "",
                                 DotActionIntf::KeyAuthScheme::Ecdsa,
                                 "invalid_key", "", false, 0, 0),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, DotCAKInstallHybridModeWithoutLmsKey)
{
    EXPECT_THROW(
        dotObject->dotCAKInstall(DotActionIntf::KeyAuthScheme::Hybrid,
                                 std::string(192, '0'),
                                 "", // Missing LMS key
                                 DotActionIntf::KeyAuthScheme::Ecdsa,
                                 std::string(192, '0'), "", false, 0, 0),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, DotCAKInstallRejectedWhenISTInProgress)
{
    auto& istProps = utils::MockDbusAsync::propertyMap(
        "/com/nvidia/vera/ist", "com.nvidia.vera.ist.State");
    istProps["IstInProgress"] = bool(true);

    EXPECT_CALL(*mockDevice, postPatchIO).Times(0);

    auto [rc, statusIntf, valueIntf] = callDotCAKInstallAsync();

    EXPECT_EQ(rc, NSM_SW_ERROR);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::ConflictingOperation);
    auto value = valueIntf->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple), static_cast<uint16_t>(NSM_SW_ERROR));
    EXPECT_EQ(std::get<1>(tuple), "CPU IST is in progress");
}

TEST_F(NsmDotTest, DotCAKInstallAsyncHandlerSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotCAKInstallResponse()));

    const auto [rc, statusInterface, valueInterface] = callDotCAKInstallAsync();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple),
              static_cast<uint16_t>(0)); // Completion code (NSM_SUCCESS)
}

TEST_F(NsmDotTest, DotCAKInstallAsyncHandlerDeviceError)
{
    Response errorResponse = createDotCAKInstallResponse(NSM_ERROR, 0xFFFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResponse](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), errorResponse.data(),
                   responseLen);
        }
        co_return NSM_SW_SUCCESS;
    });

    const auto [rc, statusInterface, valueInterface] = callDotCAKInstallAsync();

    /* With combined check, decodeRc is returned on error. When decode succeeds
     * but device reports error, decodeRc is NSM_SW_SUCCESS, but status/value
     * interfaces correctly communicate the error */
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple),
              static_cast<uint16_t>(0xFFFF)); /* Reason code */
    EXPECT_EQ(std::get<1>(tuple), "DOT CAK Install failed");
    /* rc is decodeRc, which is NSM_SW_SUCCESS when decode succeeds */
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotTest, DotCAKInstallAsyncHandlerSendFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotCAKInstallResponse(), NSM_ERROR));

    const auto [rc, statusInterface, _] = callDotCAKInstallAsync();

    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmDotTest, BypassSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotBypassResponse()));

    auto path = dotObject->bypass();

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

TEST_F(NsmDotTest, BypassAsyncHandlerSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotBypassResponse()));

    const auto [rc, statusInterface, valueInterface] = callBypassAsync();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple),
              static_cast<uint16_t>(0)); // Completion code (NSM_SUCCESS)
}

TEST_F(NsmDotTest, BypassAsyncHandlerDeviceError)
{
    Response errorResponse = createDotBypassResponse(NSM_ERROR, 0xFFFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResponse](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), errorResponse.data(),
                   responseLen);
        }
        co_return NSM_SW_SUCCESS;
    });

    const auto [rc, statusInterface, valueInterface] = callBypassAsync();

    /* With combined check, decodeRc is returned on error. When decode succeeds
     * but device reports error, decodeRc is NSM_SW_SUCCESS, but status/value
     * interfaces correctly communicate the error */
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple),
              static_cast<uint16_t>(0xFFFF)); /* Reason code */
    EXPECT_EQ(std::get<1>(tuple), "DOT CAK Bypass failed");
    /* rc is decodeRc, which is NSM_SW_SUCCESS when decode succeeds */
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotTest, BypassAsyncHandlerSendFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotBypassResponse(), NSM_ERROR));

    const auto [rc, statusInterface, _] = callBypassAsync();

    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// Mutable State Tests - Lock Operation
// ============================================================================

TEST_F(NsmDotTest, LockAsyncHandlerSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotLockResponse()));

    const auto [rc, statusInterface, valueInterface] = callLockAsync();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    auto value = valueInterface->value();
    EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(value));
}

TEST_F(NsmDotTest, LockAsyncHandlerDeviceError)
{
    Response errorResponse = createDotLockResponse(NSM_ERROR, 0xFFFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResponse](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), errorResponse.data(),
                   responseLen);
        }
        co_return NSM_SW_SUCCESS;
    });

    const auto [rc, statusInterface, valueInterface] = callLockAsync();

    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple), static_cast<uint16_t>(0xFFFF));
    EXPECT_EQ(std::get<1>(tuple), "DOT Lock failed");
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotTest, LockAsyncHandlerSendFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotLockResponse(), NSM_ERROR));

    const auto [rc, statusInterface, _] = callLockAsync();

    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// Mutable State Tests - Unlock Challenge Operation
// ============================================================================

TEST_F(NsmDotTest, UnlockChallengeAsyncHandlerSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotUnlockChallengeResponse()));

    const auto [rc, statusInterface,
                valueInterface] = callUnlockChallengeAsync();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    auto value = valueInterface->value();
    auto challenge = std::get<std::vector<uint8_t>>(value);
    EXPECT_EQ(challenge.size(), 32); // 32-byte challenge
}

TEST_F(NsmDotTest, UnlockChallengeAsyncHandlerDeviceError)
{
    Response errorResponse = createDotUnlockChallengeResponse(NSM_ERROR,
                                                              0xFFFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResponse](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), errorResponse.data(),
                   responseLen);
        }
        co_return NSM_SW_SUCCESS;
    });

    const auto [rc, statusInterface,
                valueInterface] = callUnlockChallengeAsync();

    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple), static_cast<uint16_t>(0xFFFF));
    EXPECT_EQ(std::get<1>(tuple), "DOT Unlock Challenge failed");
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotTest, UnlockChallengeAsyncHandlerSendFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(
            mockPostPatchIO(createDotUnlockChallengeResponse(), NSM_ERROR));

    const auto [rc, statusInterface, _] = callUnlockChallengeAsync();

    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// Mutable State Tests - Unlock Operation
// ============================================================================

TEST_F(NsmDotTest, UnlockAsyncHandlerSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotUnlockResponse()));

    const auto [rc, statusInterface, valueInterface] = callUnlockAsync();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    auto value = valueInterface->value();
    EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(value));
}

TEST_F(NsmDotTest, UnlockAsyncHandlerDeviceError)
{
    Response errorResponse = createDotUnlockResponse(NSM_ERROR, 0xFFFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResponse](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), errorResponse.data(),
                   responseLen);
        }
        co_return NSM_SW_SUCCESS;
    });

    const auto [rc, statusInterface, valueInterface] = callUnlockAsync();

    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple), static_cast<uint16_t>(0xFFFF));
    EXPECT_EQ(std::get<1>(tuple), "DOT Unlock failed");
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotTest, UnlockAsyncHandlerSendFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotUnlockResponse(), NSM_ERROR));

    const auto [rc, statusInterface, _] = callUnlockAsync();

    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// Mutable State Tests - CAK Rotate Operation
// ============================================================================

TEST_F(NsmDotTest, CAKRotateAsyncHandlerSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotCAKRotateResponse()));

    const auto [rc, statusInterface, valueInterface] = callCAKRotateAsync();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    auto value = valueInterface->value();
    EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(value));
}

TEST_F(NsmDotTest, CAKRotateAsyncHandlerDeviceError)
{
    Response errorResponse = createDotCAKRotateResponse(NSM_ERROR, 0xFFFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResponse](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), errorResponse.data(),
                   responseLen);
        }
        co_return NSM_SW_SUCCESS;
    });

    const auto [rc, statusInterface, valueInterface] = callCAKRotateAsync();

    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple), static_cast<uint16_t>(0xFFFF));
    EXPECT_EQ(std::get<1>(tuple), "DOT CAK Rotate failed");
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotTest, CAKRotateAsyncHandlerSendFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotCAKRotateResponse(), NSM_ERROR));

    const auto [rc, statusInterface, _] = callCAKRotateAsync();

    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// GetInfo Operation Tests
// ============================================================================

TEST_F(NsmDotTest, GetInfoSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotGetInfoResponse()));

    auto path = dotObject->getInfo();

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

TEST_F(NsmDotTest, GetInfoAsyncHandlerSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotGetInfoResponse()));

    const auto [rc, statusInterface, valueInterface] = callGetInfoAsync();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    auto value = valueInterface->value();
    EXPECT_TRUE(std::holds_alternative<
                std::tuple<uint8_t, uint8_t, uint8_t, std::vector<uint8_t>>>(
        value));
}

TEST_F(NsmDotTest, GetInfoAsyncHandlerDeviceError)
{
    Response errorResponse = createDotGetInfoResponse(NSM_ERROR, 0xFFFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResponse](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), errorResponse.data(),
                   responseLen);
        }
        co_return NSM_SW_SUCCESS;
    });

    const auto [rc, statusInterface, valueInterface] = callGetInfoAsync();

    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple), static_cast<uint16_t>(0xFFFF));
    EXPECT_EQ(std::get<1>(tuple), "DOT Get Info failed");
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotTest, GetInfoAsyncHandlerSendFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotGetInfoResponse(), NSM_ERROR));

    const auto [rc, statusInterface, _] = callGetInfoAsync();

    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// Public Method Success Tests for Mutable Operations
// ============================================================================

TEST_F(NsmDotTest, LockSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotLockResponse()));

    std::string ecdsaKey = std::string(192, '0');
    std::string lmsKey = "";
    std::string staticChallenge = std::string(64, '0');
    std::string ecdsaSignature = std::string(192, '0');

    auto path = dotObject->lock(
        DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaKey, lmsKey,
        DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaKey, lmsKey,
        DotActionIntf::UnlockMethod::StaticValue, staticChallenge,
        DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaSignature, lmsKey);

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

TEST_F(NsmDotTest, UnlockChallengeSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotUnlockChallengeResponse()));

    auto path =
        dotObject->unlockChallenge(DotActionIntf::UnlockType::OwnerUnlock);

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

TEST_F(NsmDotTest, UnlockSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotUnlockResponse()));

    std::string ecdsaSignature = std::string(192, '0');
    std::string lmsSignature = "";

    auto path = dotObject->unlock(DotActionIntf::KeyAuthScheme::Ecdsa,
                                  ecdsaSignature, lmsSignature);

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

TEST_F(NsmDotTest, CAKRotateSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotCAKRotateResponse()));

    std::string ecdsaKey = std::string(192, '0');       // New CAK key
    std::string lmsKey = "";
    std::string ecdsaSignature = std::string(192, '0'); // LAK signature
    std::string lmsSignature = "";

    auto path = dotObject->cakRotate(
        DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaKey, lmsKey,
        DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaSignature, lmsSignature);

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

// ============================================================================
// Input Validation Tests for Mutable Operations
// ============================================================================

TEST_F(NsmDotTest, LockInvalidCAKKey)
{
    std::string invalidKey = "invalid";
    std::string validKey = std::string(192, '0');
    std::string lmsKey = "";
    std::string staticChallenge = std::string(64, '0');
    std::string ecdsaSignature = std::string(192, '0');

    EXPECT_THROW(
        dotObject->lock(DotActionIntf::KeyAuthScheme::Ecdsa, invalidKey, lmsKey,
                        DotActionIntf::KeyAuthScheme::Ecdsa, validKey, lmsKey,
                        DotActionIntf::UnlockMethod::StaticValue,
                        staticChallenge, DotActionIntf::KeyAuthScheme::Ecdsa,
                        ecdsaSignature, lmsKey),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, LockInvalidLAKKey)
{
    std::string validKey = std::string(192, '0');
    std::string invalidKey = "invalid";
    std::string lmsKey = "";
    std::string staticChallenge = std::string(64, '0');
    std::string ecdsaSignature = std::string(192, '0');

    EXPECT_THROW(
        dotObject->lock(DotActionIntf::KeyAuthScheme::Ecdsa, validKey, lmsKey,
                        DotActionIntf::KeyAuthScheme::Ecdsa, invalidKey, lmsKey,
                        DotActionIntf::UnlockMethod::StaticValue,
                        staticChallenge, DotActionIntf::KeyAuthScheme::Ecdsa,
                        ecdsaSignature, lmsKey),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, LockHybridModeWithoutLmsKey)
{
    std::string validKey = std::string(192, '0');
    std::string lmsKey = "";
    std::string staticChallenge = std::string(64, '0');
    std::string ecdsaSignature = std::string(192, '0');

    EXPECT_THROW(
        dotObject->lock(DotActionIntf::KeyAuthScheme::Hybrid, validKey,
                        lmsKey, // Missing LMS key
                        DotActionIntf::KeyAuthScheme::Ecdsa, validKey, lmsKey,
                        DotActionIntf::UnlockMethod::StaticValue,
                        staticChallenge, DotActionIntf::KeyAuthScheme::Ecdsa,
                        ecdsaSignature, lmsKey),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, UnlockInvalidEcdsaSignature)
{
    std::string invalidSignature = "invalid";
    std::string lmsSignature = "";

    EXPECT_THROW(
        dotObject->unlock(DotActionIntf::KeyAuthScheme::Ecdsa, invalidSignature,
                          lmsSignature),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, UnlockHybridModeWithoutLmsSignature)
{
    std::string ecdsaSignature = std::string(192, '0');
    std::string lmsSignature = "";

    EXPECT_THROW(
        dotObject->unlock(DotActionIntf::KeyAuthScheme::Hybrid, ecdsaSignature,
                          lmsSignature),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, CAKRotateInvalidCAKKey)
{
    std::string invalidKey = "invalid";
    std::string validKey = std::string(192, '0');
    std::string lmsKey = "";
    std::string ecdsaSignature = std::string(192, '0');
    std::string lmsSignature = "";

    EXPECT_THROW(
        dotObject->cakRotate(DotActionIntf::KeyAuthScheme::Ecdsa, invalidKey,
                             lmsKey, DotActionIntf::KeyAuthScheme::Ecdsa,
                             ecdsaSignature, lmsSignature),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, CAKRotateHybridModeWithoutLmsKey)
{
    std::string validKey = std::string(192, '0');
    std::string lmsKey = "";
    std::string ecdsaSignature = std::string(192, '0');
    std::string lmsSignature = "";

    EXPECT_THROW(
        dotObject->cakRotate(DotActionIntf::KeyAuthScheme::Hybrid, validKey,
                             lmsKey, // Missing LMS key for CAK
                             DotActionIntf::KeyAuthScheme::Ecdsa,
                             ecdsaSignature, lmsSignature),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, CAKRotateInvalidSignature)
{
    std::string validKey = std::string(192, '0');
    std::string lmsKey = "";
    std::string invalidSignature = "invalid";
    std::string lmsSignature = "";

    EXPECT_THROW(
        dotObject->cakRotate(DotActionIntf::KeyAuthScheme::Ecdsa, validKey,
                             lmsKey, DotActionIntf::KeyAuthScheme::Ecdsa,
                             invalidSignature, lmsSignature),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, CAKRotateHybridModeWithoutLmsSignature)
{
    std::string validKey = std::string(192, '0');
    std::string lmsKey = "";
    std::string ecdsaSignature = std::string(192, '0');
    std::string lmsSignature = ""; // Missing LMS signature for Hybrid

    EXPECT_THROW(
        dotObject->cakRotate(DotActionIntf::KeyAuthScheme::Ecdsa, validKey,
                             lmsKey, DotActionIntf::KeyAuthScheme::Hybrid,
                             ecdsaSignature, lmsSignature),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, DisableSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotDisableResponse()));

    auto path = dotObject->disable(
        DotActionIntf::KeyAuthScheme::Ecdsa, std::string(192, '0'), "",
        DotActionIntf::UnlockMethod::OwnerUnlock, "",
        DotActionIntf::KeyAuthScheme::Ecdsa, std::string(192, '1'), "");

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

TEST_F(NsmDotTest, DisableInvalidLAKEcdsaKey)
{
    EXPECT_THROW(
        dotObject->disable(DotActionIntf::KeyAuthScheme::Ecdsa, "invalid_key",
                           "", DotActionIntf::UnlockMethod::OwnerUnlock, "",
                           DotActionIntf::KeyAuthScheme::Ecdsa,
                           std::string(192, '1'), ""),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, DisableHybridModeWithoutLmsKey)
{
    EXPECT_THROW(
        dotObject->disable(
            DotActionIntf::KeyAuthScheme::Hybrid, std::string(192, '0'),
            "", // Missing LMS key
            DotActionIntf::UnlockMethod::OwnerUnlock, "",
            DotActionIntf::KeyAuthScheme::Ecdsa, std::string(192, '1'), ""),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, DisableHybridSignatureWithoutLmsSignature)
{
    EXPECT_THROW(
        dotObject->disable(
            DotActionIntf::KeyAuthScheme::Ecdsa, std::string(192, '0'), "",
            DotActionIntf::UnlockMethod::OwnerUnlock, "",
            DotActionIntf::KeyAuthScheme::Hybrid, std::string(192, '1'),
            ""), // Missing LMS signature
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, DisableStaticUnlockMethodWithoutChallenge)
{
    EXPECT_THROW(
        dotObject->disable(
            DotActionIntf::KeyAuthScheme::Ecdsa, std::string(192, '0'), "",
            DotActionIntf::UnlockMethod::StaticValue,
            "", // Missing static challenge
            DotActionIntf::KeyAuthScheme::Ecdsa, std::string(192, '1'), ""),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, DisableAsyncHandlerSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotDisableResponse()));

    const auto [rc, statusInterface, valueInterface] = callDisableAsync();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    auto value = valueInterface->value();
    auto blob = std::get<std::vector<uint8_t>>(value);
    EXPECT_EQ(blob.size(), DOT_BLOB_SIZE);
}

TEST_F(NsmDotTest, DisableAsyncHandlerDeviceError)
{
    Response errorResponse = createDotDisableResponse(NSM_ERROR, 0xFFFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResponse](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen) -> requester::Coroutine {
        // Set correct length for error response (non-success resp is smaller)
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), errorResponse.data(),
                   responseLen);
        }
        co_return NSM_SW_SUCCESS;
    });

    const auto [rc, statusInterface, valueInterface] = callDisableAsync();

    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple),
              static_cast<uint16_t>(0xFFFF)); /* Reason code */
    EXPECT_EQ(std::get<1>(tuple), "DOT Disable failed");
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotTest, DisableAsyncHandlerSendFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotDisableResponse(), NSM_ERROR));

    const auto [rc, statusInterface, _] = callDisableAsync();

    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmDotTest, OverrideSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotOverrideResponse()));

    auto path = dotObject->override(DotActionIntf::KeyAuthScheme::Ecdsa,
                                    std::string(192, '2'), "");

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

TEST_F(NsmDotTest, OverrideHybridSignatureWithoutLmsSignature)
{
    EXPECT_THROW(
        dotObject->override(DotActionIntf::KeyAuthScheme::Hybrid,
                            std::string(192, '2'),
                            ""), // Missing LMS signature
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, OverrideAsyncHandlerSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotOverrideResponse()));

    const auto [rc, statusInterface, valueInterface] = callOverrideAsync();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    auto value = valueInterface->value();
    auto blob = std::get<std::vector<uint8_t>>(value);
    EXPECT_TRUE(blob.empty()); // Override returns empty vector on success
}

TEST_F(NsmDotTest, OverrideAsyncHandlerDeviceError)
{
    Response errorResponse = createDotOverrideResponse(NSM_ERROR, 0xFFFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResponse](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen) -> requester::Coroutine {
        // Set correct length for error response (non-success resp is smaller)
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), errorResponse.data(),
                   responseLen);
        }
        co_return NSM_SW_SUCCESS;
    });

    const auto [rc, statusInterface, valueInterface] = callOverrideAsync();

    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple),
              static_cast<uint16_t>(0xFFFF)); /* Reason code */
    EXPECT_EQ(std::get<1>(tuple), "DOT Override failed");
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotTest, OverrideAsyncHandlerSendFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotOverrideResponse(), NSM_ERROR));

    const auto [rc, statusInterface, _] = callOverrideAsync();

    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmDotTest, RecoverySuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotRecoveryResponse()));

    // Valid DOT blob (1024 bytes)
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xAA);
    int fd = createTempFileWithData(dotBlob);
    ASSERT_NE(fd, -1);

    auto path = dotObject->recoverDOT(unix_fd(fd));
    close(fd);

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

TEST_F(NsmDotTest, RecoveryEmptyDotData)
{
    EXPECT_THROW(
        dotObject->recoverDOT(unix_fd(-1)),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, RecoveryAsyncHandlerSuccess)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotRecoveryResponse()));

    const auto [rc, statusInterface, valueInterface] = callRecoveryAsync();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    auto value = valueInterface->value();
    auto blob = std::get<std::vector<uint8_t>>(value);
    EXPECT_TRUE(blob.empty()); // Recovery returns empty vector on success
}

TEST_F(NsmDotTest, RecoveryAsyncHandlerInvalidDotBlob)
{
    EXPECT_CALL(*mockDevice, postPatchIO).Times(0);

    const auto [_, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    // Invalid DOT blob (wrong size - only 10 bytes instead of 1024)
    std::vector<uint8_t> invalidDotBlob(10, 0xAA);
    int fd = createTempFileWithData(invalidDotBlob);
    ASSERT_NE(fd, -1);

    auto rc = dotObject
                  ->recoverDOTAsyncHandler(unix_fd(fd), statusInterface,
                                           valueInterface)
                  .await_resume();
    close(fd);

    EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::InvalidArgument);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple), static_cast<uint16_t>(NSM_ERR_INVALID_DATA));
}

TEST_F(NsmDotTest, RecoveryAsyncHandlerEmptyDotBlob)
{
    EXPECT_CALL(*mockDevice, postPatchIO).Times(0);

    const auto [_, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    // Empty DOT blob
    std::vector<uint8_t> emptyDotBlob;
    int fd = createTempFileWithData(emptyDotBlob);
    ASSERT_NE(fd, -1);

    auto rc = dotObject
                  ->recoverDOTAsyncHandler(unix_fd(fd), statusInterface,
                                           valueInterface)
                  .await_resume();
    close(fd);

    EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::InvalidArgument);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple), static_cast<uint16_t>(NSM_ERR_INVALID_DATA));
}

TEST_F(NsmDotTest, RecoveryAsyncHandlerDeviceError)
{
    Response errorResponse = createDotRecoveryResponse(NSM_ERROR, 0xFFFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResponse](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen) -> requester::Coroutine {
        // Set correct length for error response (non-success resp is smaller)
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), errorResponse.data(),
                   responseLen);
        }
        co_return NSM_SW_SUCCESS;
    });

    const auto [rc, statusInterface, valueInterface] = callRecoveryAsync();

    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    auto value = valueInterface->value();
    auto tuple = std::get<std::tuple<uint16_t, std::string>>(value);
    EXPECT_EQ(std::get<0>(tuple),
              static_cast<uint16_t>(0xFFFF)); /* Reason code */
    EXPECT_EQ(std::get<1>(tuple), "DOT Recovery failed");
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDotTest, RecoveryAsyncHandlerSendFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDotRecoveryResponse(), NSM_ERROR));

    const auto [rc, statusInterface, _] = callRecoveryAsync();

    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}
