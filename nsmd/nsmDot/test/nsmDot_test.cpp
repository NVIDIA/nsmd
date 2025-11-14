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

using namespace nsm;
using namespace ::testing;

NsmDeviceTable devices;
std::shared_ptr<MockNsmDeviceBase> mockDevice;

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

    auto callDotCAKInstallAsync()
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        // Valid ECDSA key (96 bytes in hex)
        std::string ecdsaKey = std::string(192, '0'); // 96 bytes * 2 hex chars
        std::string lmsKey = "";

        auto rc = dotObject
                      ->dotCAKInstallAsyncHandler(
                          DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaKey, lmsKey,
                          DotActionIntf::KeyAuthScheme::Ecdsa, ecdsaKey, lmsKey,
                          false, 0, statusInterface, valueInterface)
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

    std::unique_ptr<NsmDotObject> dotObject;
};

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
                                 "", false, 0);

    EXPECT_NE(path, sdbusplus::message::object_path{});
}

TEST_F(NsmDotTest, DotCAKInstallInvalidCAKEcdsaKey)
{
    EXPECT_THROW(
        dotObject->dotCAKInstall(DotActionIntf::KeyAuthScheme::Ecdsa,
                                 "invalid_key", // Invalid key
                                 "", DotActionIntf::KeyAuthScheme::Ecdsa,
                                 std::string(192, '0'), "", false, 0),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, DotCAKInstallInvalidLAKEcdsaKey)
{
    EXPECT_THROW(
        dotObject->dotCAKInstall(DotActionIntf::KeyAuthScheme::Ecdsa,
                                 std::string(192, '0'), "",
                                 DotActionIntf::KeyAuthScheme::Ecdsa,
                                 "invalid_key", // Invalid key
                                 "", false, 0),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDotTest, DotCAKInstallHybridModeWithoutLmsKey)
{
    EXPECT_THROW(
        dotObject->dotCAKInstall(DotActionIntf::KeyAuthScheme::Hybrid,
                                 std::string(192, '0'),
                                 "", // Missing LMS key
                                 DotActionIntf::KeyAuthScheme::Ecdsa,
                                 std::string(192, '0'), "", false, 0),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
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
