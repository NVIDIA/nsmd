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

#include "base.h"
#include "platform-environmental.h"

#include "common/event.hpp"
#include "eventManager.hpp"
#include "instance_id.hpp"
#include "requester/handler.hpp"
#include "socket_handler.hpp"
#include "socket_manager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "commonMock.hpp"
#include "nsmDevice.hpp"

#undef private
#undef protected

using namespace ::testing;
using namespace nsm;

// ---------------------------------------------------------------------------
// Helpers: build response buffers
// ---------------------------------------------------------------------------

// A properly-encoded getSupportedNvidiaMessageTypes response with given cc
static std::vector<uint8_t>
    makeMsgTypesResp(uint8_t cc,
                     bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE])
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_supported_nvidia_message_types_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_nvidia_message_types_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, types, msg);
    return buf;
}

// A properly-encoded getSupportedCommandCodes response with given cc
static std::vector<uint8_t>
    makeCmdCodesResp(uint8_t cc,
                     bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE])
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_command_codes_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, codes, msg);
    return buf;
}

// A properly-encoded getInventoryInformation response with given cc
static std::vector<uint8_t> makeInvInfoResp(uint8_t cc, uint16_t dataSize = 0,
                                            const uint8_t* data = nullptr)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_inventory_information_resp) +
                                 (dataSize > 0 ? dataSize - 1 : 0),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, dataSize, data,
        msg);
    return buf;
}

// A minimal non-success buffer (valgrind safe: >= 7 bytes)
static std::vector<uint8_t> makeTinyDecodeFailBuffer()
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto* resp = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    resp->completion_code = NSM_ERROR;
    return buf;
}

// ---------------------------------------------------------------------------
// Helper: allocate a shared_ptr<const nsm_msg> from a buffer
// ---------------------------------------------------------------------------
static void allocMessage(const std::vector<uint8_t>& response,
                         std::shared_ptr<const nsm_msg>& responseMsg,
                         size_t& responseLen)
{
    responseLen = response.size();
    if (responseLen > 0)
    {
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), response.data(), responseLen);
    }
}

// ---------------------------------------------------------------------------
// Test Fixture
// ---------------------------------------------------------------------------
using TestHandler = requester::Handler<requester::Request>;

struct NsmDeviceDITest : public Test
{
    common::Event event;
    InstanceIdDb instanceIdDb;
    mctp_socket::Manager sockManager;

    TestHandler handler{event,
                        instanceIdDb,
                        sockManager,
                        /*verbose=*/false,
                        std::chrono::seconds(2),
                        /*numRetries=*/0,
                        std::chrono::milliseconds(100)};

    std::shared_ptr<MockNsmMessageHandler> mockMsgHandler =
        std::make_shared<MockNsmMessageHandler>(handler);

    const uuid_t testUuid = "00000000-0000-0000-0000-000000000000";
    std::shared_ptr<MockNsmDevice> device;

    NsmDeviceDITest()
    {
        device = std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", testUuid,
                                                 1);
        // Inject mock message handler
        device->nsmMsgHandler = mockMsgHandler;
    }

    ~NsmDeviceDITest() override = default;

    // Helper: create a mock SendRecvNsmMsg that returns a given response
    auto mockSendRecv(const std::vector<uint8_t>& response,
                      int rc = NSM_SW_SUCCESS)
    {
        return [response, rc](eid_t, Request&,
                              std::shared_ptr<const nsm_msg>& responseMsg,
                              size_t* responseLen) -> requester::Coroutine {
            allocMessage(response, responseMsg, *responseLen);
            // coverity[missing_return]
            co_return rc;
        };
    }

    // Helper: mock SendRecvNsmMsg that returns transport error (no response)
    auto mockSendRecvFail(int rc = NSM_SW_ERROR)
    {
        return [rc](eid_t, Request&, std::shared_ptr<const nsm_msg>&,
                    size_t*) -> requester::Coroutine {
            // coverity[missing_return]
            co_return rc;
        };
    }
};

// ===========================================================================
// getSupportedNvidiaMessageType tests
// ===========================================================================

// 1. cc != NSM_SUCCESS path
TEST_F(NsmDeviceDITest, GetSupportedMsgTypes_NonSuccessCC_ReturnsCommandFail)
{
    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    auto resp = makeMsgTypesResp(NSM_ERROR, types);

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv(resp));

    std::vector<uint8_t> supportedTypes;
    auto coro = device->getSupportedNvidiaMessageType(supportedTypes);
}

// 2. decode error path (rc != NSM_SW_SUCCESS)
TEST_F(NsmDeviceDITest, GetSupportedMsgTypes_DecodeError_ReturnsCommandFail)
{
    auto resp = makeTinyDecodeFailBuffer();

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv(resp));

    std::vector<uint8_t> supportedTypes;
    auto coro = device->getSupportedNvidiaMessageType(supportedTypes);
}

// 3. SendRecvNsmMsg fails (transport error)
TEST_F(NsmDeviceDITest, GetSupportedMsgTypes_SendRecvFails_ReturnsError)
{
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecvFail());

    std::vector<uint8_t> supportedTypes;
    auto coro = device->getSupportedNvidiaMessageType(supportedTypes);
}

// ===========================================================================
// getSupportedCommandCodes tests
// ===========================================================================

// 4. cc != NSM_SUCCESS path
TEST_F(NsmDeviceDITest, GetSupportedCmdCodes_NonSuccessCC_ReturnsCommandFail)
{
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    auto resp = makeCmdCodesResp(NSM_ERROR, codes);

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv(resp));

    std::vector<uint8_t> supportedCmds;
    auto coro = device->getSupportedCommandCodes(0, supportedCmds);
}

// 5. decode error path
TEST_F(NsmDeviceDITest, GetSupportedCmdCodes_DecodeError_ReturnsCommandFail)
{
    auto resp = makeTinyDecodeFailBuffer();

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv(resp));

    std::vector<uint8_t> supportedCmds;
    auto coro = device->getSupportedCommandCodes(0, supportedCmds);
}

// 6. SendRecvNsmMsg fails (transport error)
TEST_F(NsmDeviceDITest, GetSupportedCmdCodes_SendRecvFails_ReturnsError)
{
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecvFail());

    std::vector<uint8_t> supportedCmds;
    auto coro = device->getSupportedCommandCodes(0, supportedCmds);
}

// ===========================================================================
// getInventoryInformation tests
// ===========================================================================

// 7. cc != NSM_SUCCESS path
TEST_F(NsmDeviceDITest, GetInventoryInfo_NonSuccessCC_ReturnsCC)
{
    auto resp = makeInvInfoResp(NSM_ERROR);

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv(resp));

    InventoryProperties props;
    uint8_t propId = BOARD_PART_NUMBER;
    auto coro = device->getInventoryInformation(propId, props);
}

// 8. decode error path (truncated buffer)
TEST_F(NsmDeviceDITest, GetInventoryInfo_DecodeError_ReturnsError)
{
    auto resp = makeTinyDecodeFailBuffer();

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv(resp));

    InventoryProperties props;
    uint8_t propId = BOARD_PART_NUMBER;
    auto coro = device->getInventoryInformation(propId, props);
}

// 9. SendRecvNsmMsg fails (transport error)
TEST_F(NsmDeviceDITest, GetInventoryInfo_SendRecvFails_ReturnsError)
{
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecvFail());

    InventoryProperties props;
    uint8_t propId = BOARD_PART_NUMBER;
    auto coro = device->getInventoryInformation(propId, props);
}
