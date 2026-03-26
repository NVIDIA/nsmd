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

/**
 * @file requesterDeepBranch_test.cpp
 *
 * Deep branch-coverage tests for:
 *   - request.hpp: RequestRetryTimer::start/stop/callback, Request::send
 *   - handler.hpp: runRegisteredRequest, handleResponseImpl deeper paths
 *   - handler.hpp: SendRecvNsmMsg::await_suspend, HandleResponse
 *
 * Uses the real sdeventplus::Event loop and MockMctpHandler from
 * requesterHandlerBranch_test.cpp patterns.
 */

#include "base.h"

#include "common/event.hpp"
#include "eventManager.hpp"
#include "instance_id.hpp"
#include "requester/handler.hpp"
#include "socket_handler.hpp"
#include "socket_manager.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace requester;

// ---------------------------------------------------------------------------
// Mock mctp_socket::Handler -- controllable sendMsg
// ---------------------------------------------------------------------------

class MockMctpHandlerDeep : public mctp_socket::Handler
{
  public:
    MockMctpHandlerDeep(sdeventplus::Event& event,
                        requester::Handler<requester::Request>& reqHandler,
                        nsm::EventManager& eventManager,
                        mctp_socket::Manager& sockManager) :
        mctp_socket::Handler(event, reqHandler, eventManager, sockManager,
                             false)
    {}

    int registerMctpEndpoint(eid_t /*eid*/, int /*type*/, int /*protocol*/,
                             const std::vector<uint8_t>& /*pathName*/) override
    {
        return 0;
    }

    int sendMsg(uint8_t /*tag*/, eid_t /*eid*/, int /*mctpFd*/,
                const uint8_t* /*nsmMsg*/, size_t /*nsmMsgLen*/) const override
    {
        ++sendCallCount;
        return sendResult;
    }

    mutable int sendResult = 1; // positive = success (bytes sent)
    mutable int sendCallCount = 0;

  private:
    void handleReceivedMsg(sdeventplus::source::IO& /*io*/, int /*fd*/,
                           uint32_t /*revents*/) override
    {}
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<uint8_t> makePingRequest(uint8_t instanceId = 0)
{
    std::vector<uint8_t> msg(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto nsmMsg = reinterpret_cast<nsm_msg*>(msg.data());
    encode_ping_req(instanceId, nsmMsg);
    return msg;
}

static std::vector<uint8_t> makePingResponse(uint8_t instanceId = 0)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_ping_resp(instanceId, ERR_NULL, msg);
    return buf;
}

// ===========================================================================
// Test fixture
// ===========================================================================

using TestHandler = requester::Handler<requester::Request>;

class RequesterDeepBranchTest : public ::testing::Test
{
  protected:
    common::Event event;
    nsm::InstanceIdDb instanceIdDb;
    mctp_socket::Manager sockManager;
    nsm::EventManager eventManager;

    // numRetries=2 so RequestRetryTimer callback fires and exercises the
    // retry branch (numRetries-- > 0 => send again)
    TestHandler handler{event,
                        instanceIdDb,
                        sockManager,
                        /*verbose=*/false,
                        std::chrono::seconds(2),
                        /*numRetries=*/2,
                        std::chrono::milliseconds(50)};

    MockMctpHandlerDeep mockMctpHandler{event, handler, eventManager,
                                        sockManager};

    static constexpr eid_t testEid = 10;
    static constexpr int fakeFd = 42;

    void SetUp() override
    {
        sockManager.registerEndpoint(testEid, fakeFd, 4096);
        handler.setSocketHandler(&mockMctpHandler);
    }

    int registerWithCallback(eid_t eid, bool& called, const nsm_msg*& respOut,
                             size_t& respLenOut)
    {
        auto req = makePingRequest();
        auto nsmMsg = reinterpret_cast<nsm_msg*>(req.data());
        return handler.registerRequest(
            MCTP_MSG_TAG_REQ, eid, nsmMsg->hdr.nvidia_msg_type,
            nsmMsg->payload[0], std::move(req),
            [&](eid_t /*eid*/, const nsm_msg* resp, size_t len) {
            called = true;
            respOut = resp;
            respLenOut = len;
        });
    }
};

// ===========================================================================
// RequestRetryTimer: callback fires with retries remaining (send called again)
// ===========================================================================

TEST_F(RequesterDeepBranchTest, RetryTimer_CallbackFiresSendRetry)
{
    mockMctpHandler.sendResult = 1;
    mockMctpHandler.sendCallCount = 0;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, respOut, respLen);
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Run event loop for 200ms - the retry timer (50ms interval) should fire
    // at least once, causing Request::send() to be called again
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start <
           std::chrono::milliseconds(200))
    {
        event.run(std::chrono::microseconds(10000));
    }

    // send() was called for the initial request + at least one retry
    EXPECT_GT(mockMctpHandler.sendCallCount, 1);

    // Respond to stop the request
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0, respMsg,
                           respBuf.size());
    EXPECT_TRUE(called);
}

// ===========================================================================
// RequestRetryTimer: all retries exhausted, callback calls stop()
// ===========================================================================

TEST_F(RequesterDeepBranchTest, RetryTimer_AllRetriesExhausted_StopCalled)
{
    mockMctpHandler.sendResult = 1;
    mockMctpHandler.sendCallCount = 0;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, respOut, respLen);
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Run event loop long enough for all retries to exhaust (2 retries * 50ms)
    // plus the instance ID expiry timer (2s) to fire
    auto start = std::chrono::steady_clock::now();
    while (!called)
    {
        event.run(std::chrono::microseconds(50000));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5))
        {
            break;
        }
    }

    // The initial send + 2 retries = 3 sends, then stop() is called by callback
    EXPECT_GE(mockMctpHandler.sendCallCount, 3);
    // Instance ID expiry fires → response handler called with nullptr
    EXPECT_TRUE(called);
    EXPECT_EQ(respOut, nullptr);
    EXPECT_EQ(respLen, 0u);
}

// ===========================================================================
// Request::send failure path (sendMsg returns < 0)
// ===========================================================================

TEST_F(RequesterDeepBranchTest, Request_Send_SocketError)
{
    mockMctpHandler.sendResult = -1;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, respOut, respLen);
    // send() returns NSM_SW_ERROR → start() returns it
    EXPECT_NE(rc, NSM_SUCCESS);
    EXPECT_FALSE(called);
}

// ===========================================================================
// RequestRetryTimer::start with numRetries=0 does not arm timer
// ===========================================================================

TEST_F(RequesterDeepBranchTest, RetryTimer_ZeroRetries_NoTimerArmed)
{
    TestHandler noRetryHandler{event,
                               instanceIdDb,
                               sockManager,
                               /*verbose=*/false,
                               std::chrono::seconds(3),
                               /*numRetries=*/0,
                               std::chrono::milliseconds(50)};
    noRetryHandler.setSocketHandler(&mockMctpHandler);

    constexpr eid_t eid2 = 20;
    sockManager.registerEndpoint(eid2, fakeFd, 4096);
    mockMctpHandler.sendResult = 1;
    mockMctpHandler.sendCallCount = 0;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto req = makePingRequest();
    auto nsmMsg = reinterpret_cast<nsm_msg*>(req.data());
    auto rc = noRetryHandler.registerRequest(
        MCTP_MSG_TAG_REQ, eid2, nsmMsg->hdr.nvidia_msg_type, nsmMsg->payload[0],
        std::move(req), [&](eid_t, const nsm_msg* resp, size_t len) {
        called = true;
        respOut = resp;
        respLen = len;
    });
    EXPECT_EQ(rc, NSM_SUCCESS);
    // Only the initial send, no retry timer
    EXPECT_EQ(mockMctpHandler.sendCallCount, 1);

    // Respond immediately
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
    noRetryHandler.handleResponse(MCTP_MSG_TAG_REQ, eid2, 0, 0, 0, respMsg,
                                  respBuf.size());
    EXPECT_TRUE(called);
}

// ===========================================================================
// handleResponseImpl: instance ID mismatch within same EID queue
// ===========================================================================

TEST_F(RequesterDeepBranchTest,
       HandleResponseImpl_InstanceIdMismatch_ReturnsFalse)
{
    mockMctpHandler.sendResult = 1;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, respOut, respLen);
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Response with wrong instance ID (request was allocated ID 0)
    auto respBuf = makePingResponse(99);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());

    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 99, 0, 0, respMsg,
                           respBuf.size());

    // Handler not called because instance ID didn't match
    EXPECT_FALSE(called);

    // Now respond with correct ID
    auto correctResp = makePingResponse(0);
    auto correctMsg = reinterpret_cast<const nsm_msg*>(correctResp.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0, correctMsg,
                           correctResp.size());
    EXPECT_TRUE(called);
}

// ===========================================================================
// handleResponseImpl: response after queue drained → not found
// ===========================================================================

TEST_F(RequesterDeepBranchTest, HandleResponseImpl_QueueDrained_ReturnsFalse)
{
    mockMctpHandler.sendResult = 1;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    registerWithCallback(testEid, called, respOut, respLen);

    // Drain the queue
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0, respMsg,
                           respBuf.size());
    EXPECT_TRUE(called);

    // Now queue is empty, another response should not match
    EXPECT_NO_THROW(handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0,
                                           respMsg, respBuf.size()));
}

// ===========================================================================
// handleResponseImpl: pops front, runs next queued request
// ===========================================================================

TEST_F(RequesterDeepBranchTest, HandleResponseImpl_PopsAndRunsNext)
{
    mockMctpHandler.sendResult = 1;

    bool called1 = false, called2 = false;
    const nsm_msg* resp1 = nullptr;
    const nsm_msg* resp2 = nullptr;
    size_t len1 = 0, len2 = 0;

    registerWithCallback(testEid, called1, resp1, len1);
    registerWithCallback(testEid, called2, resp2, len2);

    // Respond to first request
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0, respMsg,
                           respBuf.size());

    EXPECT_TRUE(called1);
    EXPECT_FALSE(called2);

    // Respond to second request (instance ID = 1)
    auto respBuf2 = makePingResponse(1);
    auto respMsg2 = reinterpret_cast<const nsm_msg*>(respBuf2.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 1, 0, 0, respMsg2,
                           respBuf2.size());
    EXPECT_TRUE(called2);
}

// ===========================================================================
// runRegisteredRequest: instanceIdDb.next() throws → returns NSM_ERROR
// ===========================================================================

TEST_F(RequesterDeepBranchTest,
       RunRegisteredRequest_InstanceIdException_ReturnsError)
{
    mockMctpHandler.sendResult = 1;

    // Exhaust instance IDs for testEid
    auto id = instanceIdDb.next(testEid);

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, respOut, respLen);
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_FALSE(called);

    instanceIdDb.free(testEid, id);
}

// ===========================================================================
// Fake handlers for SendRecvNsmMsg template tests
// ===========================================================================

struct FakeHandlerOk
{
    int registerRequest(uint8_t /*tag*/, eid_t /*eid*/, uint8_t /*type*/,
                        uint8_t /*command*/,
                        std::vector<uint8_t>&& /*requestMsg*/,
                        requester::ResponseHandler&& /*responseHandler*/)
    {
        return NSM_SW_SUCCESS;
    }
};

struct FakeHandlerFail
{
    int registerRequest(uint8_t /*tag*/, eid_t /*eid*/, uint8_t /*type*/,
                        uint8_t /*command*/,
                        std::vector<uint8_t>&& /*requestMsg*/,
                        requester::ResponseHandler&& /*responseHandler*/)
    {
        return NSM_SW_ERROR;
    }
};

// Store the last captured callback for later invocation
struct FakeHandlerCapture
{
    requester::ResponseHandler lastCallback;
    int registerRequest(uint8_t /*tag*/, eid_t /*eid*/, uint8_t /*type*/,
                        uint8_t /*command*/,
                        std::vector<uint8_t>&& /*requestMsg*/,
                        requester::ResponseHandler&& responseHandler)
    {
        lastCallback = std::move(responseHandler);
        return NSM_SW_SUCCESS;
    }
};

// ===========================================================================
// SendRecvNsmMsg: both nullptr checks
// ===========================================================================

TEST(SendRecvDeepTest, AwaitSuspend_BothNullPtrs_ReturnsFalse)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);

    // Both responseMsg and responseLen are nullptr
    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       nullptr, nullptr);
    bool suspended = awaitable.await_suspend(std::noop_coroutine());
    EXPECT_FALSE(suspended);
    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR_NULL);
}

TEST(SendRecvDeepTest, AwaitSuspend_RegisterOk_Suspends_ThenHandleResponse)
{
    FakeHandlerCapture fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    encode_ping_req(0, reinterpret_cast<nsm_msg*>(req.data()));
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerCapture> awaitable(fakeHandler, 1, req,
                                                            &respMsg, &respLen);
    bool suspended = awaitable.await_suspend(std::noop_coroutine());
    EXPECT_TRUE(suspended);

    // Now simulate a valid response via the captured callback
    nsm_msg fakeResponse{};
    awaitable.HandleResponse(1, &fakeResponse, sizeof(nsm_msg_hdr));

    EXPECT_EQ(awaitable.rc, NSM_SW_SUCCESS);
    EXPECT_EQ(respMsg, &fakeResponse);
    EXPECT_EQ(respLen, sizeof(nsm_msg_hdr));
}

TEST(SendRecvDeepTest, HandleResponse_NullWithZeroLen_SetsTimeout)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    awaitable.resumeHandle = std::noop_coroutine();

    // null response with zero length = timeout
    awaitable.HandleResponse(1, nullptr, 0);
    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR_TIMEOUT);
}

TEST(SendRecvDeepTest, HandleResponse_NonNullWithZeroLen_SetsTimeout)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    awaitable.resumeHandle = std::noop_coroutine();

    nsm_msg fakeResp{};
    // Non-null pointer but zero length = timeout path
    awaitable.HandleResponse(1, &fakeResp, 0);
    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR_TIMEOUT);
}

TEST(SendRecvDeepTest, AwaitSuspend_RegisterFails_ReturnsFalse)
{
    FakeHandlerFail fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    encode_ping_req(0, reinterpret_cast<nsm_msg*>(req.data()));
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerFail> awaitable(fakeHandler, 1, req,
                                                         &respMsg, &respLen);
    bool suspended = awaitable.await_suspend(std::noop_coroutine());
    EXPECT_FALSE(suspended);
    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR);
}

// ===========================================================================
// Instance ID expiry with retry handler: fires callback, calls
// responseHandler(nullptr)
// ===========================================================================

TEST_F(RequesterDeepBranchTest, InstanceIdExpiry_WithRetries)
{
    // Use a very short expiry interval with retries
    TestHandler shortHandler{event,
                             instanceIdDb,
                             sockManager,
                             /*verbose=*/false,
                             std::chrono::seconds(1),
                             /*numRetries=*/1,
                             std::chrono::milliseconds(50)};
    shortHandler.setSocketHandler(&mockMctpHandler);

    constexpr eid_t eid3 = 30;
    sockManager.registerEndpoint(eid3, fakeFd, 4096);
    mockMctpHandler.sendResult = 1;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 42;

    auto req = makePingRequest();
    auto nsmMsg = reinterpret_cast<nsm_msg*>(req.data());
    auto rc = shortHandler.registerRequest(
        MCTP_MSG_TAG_REQ, eid3, nsmMsg->hdr.nvidia_msg_type, nsmMsg->payload[0],
        std::move(req), [&](eid_t, const nsm_msg* resp, size_t len) {
        called = true;
        respOut = resp;
        respLen = len;
    });
    EXPECT_EQ(rc, NSM_SUCCESS);

    auto start = std::chrono::steady_clock::now();
    while (!called)
    {
        event.run(std::chrono::microseconds(50000));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(3))
        {
            break;
        }
    }

    EXPECT_TRUE(called);
    EXPECT_EQ(respOut, nullptr);
    EXPECT_EQ(respLen, 0u);
}

// ===========================================================================
// Instance ID expiry fires deferred event, next queued request starts
// ===========================================================================

TEST_F(RequesterDeepBranchTest, InstanceIdExpiry_DeferredStartsNextRequest)
{
    TestHandler shortHandler{event,
                             instanceIdDb,
                             sockManager,
                             /*verbose=*/false,
                             std::chrono::seconds(1),
                             /*numRetries=*/0,
                             std::chrono::milliseconds(50)};
    shortHandler.setSocketHandler(&mockMctpHandler);

    constexpr eid_t eid4 = 40;
    sockManager.registerEndpoint(eid4, fakeFd, 4096);
    mockMctpHandler.sendResult = 1;

    bool called1 = false, called2 = false;
    const nsm_msg* resp1 = nullptr;
    const nsm_msg* resp2 = nullptr;
    size_t len1 = 0, len2 = 0;

    auto req1 = makePingRequest();
    auto nsmMsg1 = reinterpret_cast<nsm_msg*>(req1.data());
    shortHandler.registerRequest(MCTP_MSG_TAG_REQ, eid4,
                                 nsmMsg1->hdr.nvidia_msg_type,
                                 nsmMsg1->payload[0], std::move(req1),
                                 [&](eid_t, const nsm_msg* resp, size_t len) {
        called1 = true;
        resp1 = resp;
        len1 = len;
    });

    auto req2 = makePingRequest();
    auto nsmMsg2 = reinterpret_cast<nsm_msg*>(req2.data());
    shortHandler.registerRequest(MCTP_MSG_TAG_REQ, eid4,
                                 nsmMsg2->hdr.nvidia_msg_type,
                                 nsmMsg2->payload[0], std::move(req2),
                                 [&](eid_t, const nsm_msg* resp, size_t len) {
        called2 = true;
        resp2 = resp;
        len2 = len;
    });

    // Wait for first to expire
    auto start = std::chrono::steady_clock::now();
    while (!called1)
    {
        event.run(std::chrono::microseconds(50000));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(3))
        {
            break;
        }
    }

    EXPECT_TRUE(called1);
    EXPECT_EQ(resp1, nullptr);

    // Run event loop to process deferred event and start second request
    event.run(std::chrono::microseconds(50000));

    // Respond to second request
    if (!called2)
    {
        auto respBuf = makePingResponse(1);
        auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
        shortHandler.handleResponse(MCTP_MSG_TAG_REQ, eid4, 1, 0, 0, respMsg,
                                    respBuf.size());
    }

    EXPECT_TRUE(called2);
}

// ===========================================================================
// Large request message triggers setSendBufferSize path
// ===========================================================================

TEST_F(RequesterDeepBranchTest, RegisterRequest_LargeMsg_SetsSendBuffer)
{
    mockMctpHandler.sendResult = 1;

    // Create a request larger than registered buffer (4096)
    std::vector<uint8_t> largeReq(5000, 0);
    auto nsmMsg = reinterpret_cast<nsm_msg*>(largeReq.data());
    encode_ping_req(0, nsmMsg);

    bool called = false;
    auto rc = handler.registerRequest(
        MCTP_MSG_TAG_REQ, testEid, nsmMsg->hdr.nvidia_msg_type,
        nsmMsg->payload[0], std::move(largeReq),
        [&](eid_t, const nsm_msg*, size_t) { called = true; });
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Respond to clean up
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0, respMsg,
                           respBuf.size());
    EXPECT_TRUE(called);
}

// ===========================================================================
// Verbose mode: Request with verbose=true exercises logging paths
// ===========================================================================

TEST_F(RequesterDeepBranchTest, VerboseMode_SendAndReceive)
{
    TestHandler verboseHandler{event,
                               instanceIdDb,
                               sockManager,
                               /*verbose=*/true,
                               std::chrono::seconds(5),
                               /*numRetries=*/1,
                               std::chrono::milliseconds(50)};
    verboseHandler.setSocketHandler(&mockMctpHandler);

    constexpr eid_t eid5 = 50;
    sockManager.registerEndpoint(eid5, fakeFd, 4096);
    mockMctpHandler.sendResult = 1;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto req = makePingRequest();
    auto nsmMsg = reinterpret_cast<nsm_msg*>(req.data());
    auto rc = verboseHandler.registerRequest(
        MCTP_MSG_TAG_REQ, eid5, nsmMsg->hdr.nvidia_msg_type, nsmMsg->payload[0],
        std::move(req), [&](eid_t, const nsm_msg* resp, size_t len) {
        called = true;
        respOut = resp;
        respLen = len;
    });
    EXPECT_EQ(rc, NSM_SUCCESS);

    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
    verboseHandler.handleResponse(MCTP_MSG_TAG_REQ, eid5, 0, 0, 0, respMsg,
                                  respBuf.size());
    EXPECT_TRUE(called);
}

// ===========================================================================
// Multiple EIDs with different request states
// ===========================================================================

TEST_F(RequesterDeepBranchTest, MultipleEids_IndependentLifecycles)
{
    mockMctpHandler.sendResult = 1;

    constexpr eid_t eidA = 60;
    constexpr eid_t eidB = 70;
    sockManager.registerEndpoint(eidA, fakeFd, 4096);
    sockManager.registerEndpoint(eidB, fakeFd, 4096);

    bool calledA = false, calledB = false;
    const nsm_msg* respA = nullptr;
    const nsm_msg* respB = nullptr;
    size_t lenA = 0, lenB = 0;

    auto reqA = makePingRequest();
    auto nsmA = reinterpret_cast<nsm_msg*>(reqA.data());
    handler.registerRequest(MCTP_MSG_TAG_REQ, eidA, nsmA->hdr.nvidia_msg_type,
                            nsmA->payload[0], std::move(reqA),
                            [&](eid_t, const nsm_msg* r, size_t l) {
        calledA = true;
        respA = r;
        lenA = l;
    });

    auto reqB = makePingRequest();
    auto nsmB = reinterpret_cast<nsm_msg*>(reqB.data());
    handler.registerRequest(MCTP_MSG_TAG_REQ, eidB, nsmB->hdr.nvidia_msg_type,
                            nsmB->payload[0], std::move(reqB),
                            [&](eid_t, const nsm_msg* r, size_t l) {
        calledB = true;
        respB = r;
        lenB = l;
    });

    // Respond to B first
    auto respBufB = makePingResponse(0);
    auto respMsgB = reinterpret_cast<const nsm_msg*>(respBufB.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, eidB, 0, 0, 0, respMsgB,
                           respBufB.size());

    EXPECT_FALSE(calledA);
    EXPECT_TRUE(calledB);

    // Respond to A
    auto respBufA = makePingResponse(0);
    auto respMsgA = reinterpret_cast<const nsm_msg*>(respBufA.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, eidA, 0, 0, 0, respMsgA,
                           respBufA.size());

    EXPECT_TRUE(calledA);
}

// ===========================================================================
// handleResponse for completely unknown EID
// ===========================================================================

TEST_F(RequesterDeepBranchTest, HandleResponse_UnknownEid_NoMatch)
{
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());

    // No registered request for EID 255
    EXPECT_NO_THROW(handler.handleResponse(MCTP_MSG_TAG_REQ, 255, 0, 0, 0,
                                           respMsg, respBuf.size()));
}
