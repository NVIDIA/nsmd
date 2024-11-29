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

#define private public
#define protected public

#include "nsmRawCommandHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace nsm;
using namespace ::testing;
using sdbusplus::message::unix_fd;

class NsmRawCommandHandlerTest : public Test, public SensorManagerTest
{
  protected:
    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(0, 0)},
    };

    NsmRawCommandHandlerTest() : SensorManagerTest(devices) {}
    utils::CustomFD fd{memfd_create("nsmRawCommand", 0)};

    Response response(uint8_t messageType, uint8_t commandCode)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_common_resp(0, NSM_SUCCESS, 0, messageType,
                                     commandCode, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    auto sendRequest(uint8_t deviceType, uint8_t instanceId,
                     uint8_t messageType, uint8_t commandCode)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        auto rc = NsmRawCommandHandler::getInstance()
                      .doSendRequest(deviceType, instanceId, false, messageType,
                                     commandCode, dup(fd), statusInterface,
                                     valueInterface)
                      .await_resume();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }
};

TEST(NsmRawCommandHandler, InitializeTest)
{
    EXPECT_THROW(NsmRawCommandHandler::getInstance(), std::runtime_error);
    NsmRawCommandHandler::initialize(utils::DBusHandler::getBus(),
                                     "/nsmRawCommand");
    EXPECT_NO_THROW(NsmRawCommandHandler::getInstance());
}

TEST_F(NsmRawCommandHandlerTest, GoodTestSendRequest)
{
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(response(0, 0)));
    auto path = NsmRawCommandHandler::getInstance().sendRequest(0, 0, false, 0,
                                                                0, unix_fd(fd));
    EXPECT_NE(path, sdbusplus::message::object_path{});
}
TEST_F(NsmRawCommandHandlerTest, BadTestSendRequest)
{
    EXPECT_THROW(
        NsmRawCommandHandler::getInstance().sendRequest(
            NSM_DEV_ID_EROT + 1, 0, false, 0, 0, unix_fd(fd)),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
    EXPECT_THROW(
        NsmRawCommandHandler::getInstance().sendRequest(
            0, 0, false, NSM_TYPE_FIRMWARE + 1, 0, unix_fd(fd)),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
    for (size_t i = 0; i < AsyncOperationManager::getInstance()->maxObjectCount;
         i++)
    {
        AsyncOperationManager::getInstance()->getNewStatusInterface();
    }
    EXPECT_THROW(NsmRawCommandHandler::getInstance().sendRequest(
                     0, 0, false, 0, 0, unix_fd(fd)),
                 sdbusplus::error::xyz::openbmc_project::common::Unavailable);
    for (auto& interface :
         AsyncOperationManager::getInstance()->statusInterfaces)
    {
        interface.reset();
    }
    for (auto& interface :
         AsyncOperationManager::getInstance()->valueInterfaces)
    {
        interface.reset();
    }
    AsyncOperationManager::getInstance()->statusInterfaces.clear();
    AsyncOperationManager::getInstance()->valueInterfaces.clear();
    AsyncOperationManager::getInstance()->currentObjectCount = 0;
}

TEST_F(NsmRawCommandHandlerTest, BadTestNoDevice)
{
    const auto [rc, statusInterface, _] = sendRequest(0, 1, 0, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::InvalidArgument);
}
TEST_F(NsmRawCommandHandlerTest, BadTestUnsupportedCommand)
{
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(response(0, 0),
                                     NSM_ERR_UNSUPPORTED_COMMAND_CODE));
    const auto [rc, statusInterface, valueInterface] = sendRequest(0, 0, 0, 0);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(std::get<uint8_t>(valueInterface->value()), NSM_SW_SUCCESS);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    std::vector<uint8_t> data;
    utils::readFdToBuffer(fd, data);
    EXPECT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], NSM_ERR_UNSUPPORTED_COMMAND_CODE);
    EXPECT_EQ(data[1], 0);
    EXPECT_EQ(data[2], 0);
}

TEST_F(NsmRawCommandHandlerTest, BadTestWriteFailure)
{
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(response(0, 0), NSM_ERROR));
    const auto [rc, statusInterface, _] = sendRequest(0, 0, 0, 0);
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}
TEST_F(NsmRawCommandHandlerTest, BadTestDecodeError)
{
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_common_resp(0, NSM_ERROR, ERR_NOT_SUPPORTED, 0, 0, msg);
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(response));
    const auto [rc, statusInterface, _] = sendRequest(0, 0, 0, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}
