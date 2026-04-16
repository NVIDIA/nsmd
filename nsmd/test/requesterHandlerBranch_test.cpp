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

#include "common/event.hpp"
#include "eventManager.hpp"
#include "instance_id.hpp"
#include "requester/handler.hpp"
#include "socket_handler.hpp"
#include "socket_manager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <chrono>
#include <coroutine>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace requester;

// ---------------------------------------------------------------------------
// Mock mctp_socket::Handler -- provides controllable sendMsg
// ---------------------------------------------------------------------------

class MockMctpHandler : public mctp_socket::Handler
{
  public:
    MockMctpHandler(sdeventplus::Event& event,
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
                const uint8_t* /*nsmMsg*/, size_t /*nsmMsgLen*/) override
    {
        return sendResult;
    }

    int sendResult = 1; // positive = success (bytes sent)

  private:
    void handleReceivedMsg(sdeventplus::source::IO& /*io*/, int /*fd*/,
                           uint32_t /*revents*/) override
    {}
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Build a minimal valid NSM request (ping) message
static std::vector<uint8_t> makePingRequest(uint8_t instanceId = 0)
{
    std::vector<uint8_t> msg(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto nsmMsg = reinterpret_cast<nsm_msg*>(msg.data());
    encode_ping_req(instanceId, nsmMsg);
    return msg;
}

/// Build a minimal valid NSM ping response
static std::vector<uint8_t> makePingResponse(uint8_t instanceId = 0)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_ping_resp(instanceId, ERR_NULL, msg);
    return buf;
}

// ---------------------------------------------------------------------------
// Fake handlers for SendRecvNsmMsg template tests
// ---------------------------------------------------------------------------

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
// Test fixture
// ===========================================================================

using TestHandler = requester::Handler<requester::Request>;

class RequesterHandlerBranchTest : public ::testing::Test
{
  protected:
    common::Event event;
    nsm::InstanceIdDb instanceIdDb;
    mctp_socket::Manager sockManager;
    nsm::EventManager eventManager;

    // Use very short timeouts so the tests run quickly
    TestHandler handler{event,
                        instanceIdDb,
                        sockManager,
                        /*verbose=*/false,
                        std::chrono::seconds(2),
                        /*numRetries=*/0,
                        std::chrono::milliseconds(100)};

    // Mock MCTP socket handler for sendMsg control
    MockMctpHandler mockMctpHandler{event, handler, eventManager, sockManager};

    static constexpr eid_t testEid = 10;
    static constexpr int fakeFd = 42;

    void SetUp() override
    {
        // Register the endpoint with a fake fd and buffer size
        sockManager.registerEndpoint(testEid, fakeFd, 4096);
        // Provide the socket handler to the requester handler
        handler.setSocketHandler(&mockMctpHandler);
    }

    /// Register a request with a response callback that records invocation
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
// registerRequest + runRegisteredRequest tests
// ===========================================================================

// Successful registration: instance ID allocated, timer started, request sent
TEST_F(RequesterHandlerBranchTest, RegisterRequest_Success)
{
    mockMctpHandler.sendResult = 1; // send succeeds

    bool called = false;
    const nsm_msg* resp = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, resp, respLen);
    EXPECT_EQ(rc, NSM_SUCCESS);
    // Response handler not called yet (still waiting)
    EXPECT_FALSE(called);
}

// Registration when send fails: start() returns error, instance ID freed
TEST_F(RequesterHandlerBranchTest, RegisterRequest_SendFails)
{
    mockMctpHandler.sendResult = -1; // send fails (negative = error)

    bool called = false;
    const nsm_msg* resp = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, resp, respLen);
    // start() fails → returns NSM_SW_ERROR, which is non-zero
    EXPECT_NE(rc, NSM_SUCCESS);
    EXPECT_FALSE(called);

    // Instance ID should have been freed; next allocation should succeed
    uint8_t id = 0;
    EXPECT_NO_THROW(id = instanceIdDb.next(testEid));
    instanceIdDb.free(testEid, id);
}

// Duplicate EID: second request queued behind first
TEST_F(RequesterHandlerBranchTest, RegisterRequest_DuplicateEid_Queued)
{
    mockMctpHandler.sendResult = 1;

    bool called1 = false, called2 = false;
    const nsm_msg* resp1 = nullptr;
    const nsm_msg* resp2 = nullptr;
    size_t len1 = 0, len2 = 0;

    auto rc1 = registerWithCallback(testEid, called1, resp1, len1);
    EXPECT_EQ(rc1, NSM_SUCCESS);

    // Second registration for same EID: timer is already running,
    // so it just gets queued (runRegisteredRequest returns NSM_SUCCESS)
    auto rc2 = registerWithCallback(testEid, called2, resp2, len2);
    EXPECT_EQ(rc2, NSM_SUCCESS);

    // Neither response handler called yet
    EXPECT_FALSE(called1);
    EXPECT_FALSE(called2);
}

// ===========================================================================
// handleResponse tests
// ===========================================================================

// Matching response: handler called with response, instance ID freed,
// queued request started
TEST_F(RequesterHandlerBranchTest, HandleResponse_MatchingInstanceId)
{
    mockMctpHandler.sendResult = 1;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, respOut, respLen);
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Build a response with the expected instance ID (first allocation = 0)
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());

    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0, respMsg,
                           respBuf.size());

    EXPECT_TRUE(called);
    EXPECT_NE(respOut, nullptr);
    EXPECT_EQ(respLen, respBuf.size());
}

// Non-matching instance ID: handler NOT called, requestFound = false path
TEST_F(RequesterHandlerBranchTest, HandleResponse_NonMatchingInstanceId)
{
    mockMctpHandler.sendResult = 1;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, respOut, respLen);
    EXPECT_EQ(rc, NSM_SUCCESS);

    auto respBuf = makePingResponse(31); // wrong instance ID
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());

    // Should log error but not crash
    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 31, 0, 0, respMsg,
                           respBuf.size());

    EXPECT_FALSE(called);
}

// No registered request for EID: requestFound = false
TEST_F(RequesterHandlerBranchTest, HandleResponse_NoRegisteredRequest)
{
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());

    // No crash expected
    EXPECT_NO_THROW(handler.handleResponse(MCTP_MSG_TAG_REQ, 99, 0, 0, 0,
                                           respMsg, respBuf.size()));
}

// Response arrives for first of two queued requests: pops front,
// runs the next queued request
TEST_F(RequesterHandlerBranchTest, HandleResponse_PopsAndRunsNextQueued)
{
    mockMctpHandler.sendResult = 1;

    bool called1 = false, called2 = false;
    const nsm_msg* resp1 = nullptr;
    const nsm_msg* resp2 = nullptr;
    size_t len1 = 0, len2 = 0;

    registerWithCallback(testEid, called1, resp1, len1);
    registerWithCallback(testEid, called2, resp2, len2);

    // Respond to the first request (instanceId = 0)
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());

    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0, respMsg,
                           respBuf.size());

    EXPECT_TRUE(called1);
    // Second request should now be the front and running;
    // it hasn't been responded to yet
    EXPECT_FALSE(called2);

    // Now respond to the second request (instanceId = 1, since it's the next)
    auto respBuf2 = makePingResponse(1);
    auto respMsg2 = reinterpret_cast<const nsm_msg*>(respBuf2.data());

    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 1, 0, 0, respMsg2,
                           respBuf2.size());

    EXPECT_TRUE(called2);
}

// ===========================================================================
// Instance ID expiry callback (timer fires)
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, InstanceIdExpiry_CallsHandlerWithNull)
{
    // Use a very short expiry interval
    TestHandler shortHandler{event,
                             instanceIdDb,
                             sockManager,
                             /*verbose=*/false,
                             std::chrono::seconds(1),
                             /*numRetries=*/0,
                             std::chrono::milliseconds(50)};
    shortHandler.setSocketHandler(&mockMctpHandler);

    constexpr eid_t eid2 = 20;
    sockManager.registerEndpoint(eid2, fakeFd, 4096);
    mockMctpHandler.sendResult = 1;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 42; // non-zero to detect reset

    auto req = makePingRequest();
    auto nsmMsg = reinterpret_cast<nsm_msg*>(req.data());
    auto rc = shortHandler.registerRequest(
        MCTP_MSG_TAG_REQ, eid2, nsmMsg->hdr.nvidia_msg_type, nsmMsg->payload[0],
        std::move(req), [&](eid_t, const nsm_msg* resp, size_t len) {
        called = true;
        respOut = resp;
        respLen = len;
    });
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Run the event loop until the instance ID expiry timer fires
    // The timer is 1 second; we'll loop for up to 3 seconds
    auto start = std::chrono::steady_clock::now();
    while (!called)
    {
        event.run(std::chrono::microseconds(50000)); // 50ms per iteration
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(3))
        {
            break;
        }
    }

    // The expiry callback should have invoked the response handler with nullptr
    EXPECT_TRUE(called);
    EXPECT_EQ(respOut, nullptr);
    EXPECT_EQ(respLen, 0u);
}

// ===========================================================================
// Instance ID expiry with queued requests: after expiry, next request starts
// ===========================================================================

TEST_F(RequesterHandlerBranchTest,
       InstanceIdExpiry_WithQueuedRequest_StartsNext)
{
    TestHandler shortHandler{event,
                             instanceIdDb,
                             sockManager,
                             /*verbose=*/false,
                             std::chrono::seconds(1),
                             /*numRetries=*/0,
                             std::chrono::milliseconds(50)};
    shortHandler.setSocketHandler(&mockMctpHandler);

    constexpr eid_t eid3 = 30;
    sockManager.registerEndpoint(eid3, fakeFd, 4096);
    mockMctpHandler.sendResult = 1;

    bool called1 = false, called2 = false;
    const nsm_msg* resp1 = nullptr;
    const nsm_msg* resp2 = nullptr;
    size_t len1 = 0, len2 = 0;

    // Register first request
    auto req1 = makePingRequest();
    auto nsmMsg1 = reinterpret_cast<nsm_msg*>(req1.data());
    shortHandler.registerRequest(MCTP_MSG_TAG_REQ, eid3,
                                 nsmMsg1->hdr.nvidia_msg_type,
                                 nsmMsg1->payload[0], std::move(req1),
                                 [&](eid_t, const nsm_msg* resp, size_t len) {
        called1 = true;
        resp1 = resp;
        len1 = len;
    });

    // Register second request (queued)
    auto req2 = makePingRequest();
    auto nsmMsg2 = reinterpret_cast<nsm_msg*>(req2.data());
    shortHandler.registerRequest(MCTP_MSG_TAG_REQ, eid3,
                                 nsmMsg2->hdr.nvidia_msg_type,
                                 nsmMsg2->payload[0], std::move(req2),
                                 [&](eid_t, const nsm_msg* resp, size_t len) {
        called2 = true;
        resp2 = resp;
        len2 = len;
    });

    // Wait for first request's instance ID to expire
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
    EXPECT_EQ(resp1, nullptr); // expired
    EXPECT_EQ(len1, 0u);

    // The second request should now be running; respond to it
    // The deferred event needs to fire to start the second request
    event.run(std::chrono::microseconds(50000));

    if (!called2)
    {
        // Second request is now the front; respond with correct instanceId
        // Instance 0 was used and freed; next allocation = 1
        auto respBuf = makePingResponse(1);
        auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
        shortHandler.handleResponse(MCTP_MSG_TAG_REQ, eid3, 1, 0, 0, respMsg,
                                    respBuf.size());
    }

    EXPECT_TRUE(called2);
}

// ===========================================================================
// Request::send error path (socket send returns negative)
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, Request_SendError_ReturnsFailure)
{
    mockMctpHandler.sendResult = -1; // sendMsg returns < 0

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, respOut, respLen);
    // request->start() calls send() which fails → returns NSM_SW_ERROR
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ===========================================================================
// Request with numRetries=0 (no retry timer armed)
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, Request_ZeroRetries_NoTimerArmed)
{
    // The fixture already uses numRetries=0
    mockMctpHandler.sendResult = 1;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, respOut, respLen);
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Respond immediately
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0, respMsg,
                           respBuf.size());

    EXPECT_TRUE(called);
}

// ===========================================================================
// Handler with numRetries > 0 (retry timer armed)
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, Request_WithRetries_TimerArmed)
{
    TestHandler retryHandler{event,
                             instanceIdDb,
                             sockManager,
                             /*verbose=*/false,
                             std::chrono::seconds(5),
                             /*numRetries=*/2,
                             std::chrono::milliseconds(100)};
    retryHandler.setSocketHandler(&mockMctpHandler);

    constexpr eid_t eid4 = 40;
    sockManager.registerEndpoint(eid4, fakeFd, 4096);
    mockMctpHandler.sendResult = 1;

    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto req = makePingRequest();
    auto nsmMsg = reinterpret_cast<nsm_msg*>(req.data());
    auto rc = retryHandler.registerRequest(
        MCTP_MSG_TAG_REQ, eid4, nsmMsg->hdr.nvidia_msg_type, nsmMsg->payload[0],
        std::move(req), [&](eid_t, const nsm_msg* resp, size_t len) {
        called = true;
        respOut = resp;
        respLen = len;
    });
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Respond before retries exhaust
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
    retryHandler.handleResponse(MCTP_MSG_TAG_REQ, eid4, 0, 0, 0, respMsg,
                                respBuf.size());

    EXPECT_TRUE(called);
}

// ===========================================================================
// handleResponseImpl: empty handlers[eid] path
// ===========================================================================

TEST_F(RequesterHandlerBranchTest,
       HandleResponse_EmptyEidQueue_ReturnsFalseNoMatch)
{
    // Register and then respond to drain the queue
    mockMctpHandler.sendResult = 1;
    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    registerWithCallback(testEid, called, respOut, respLen);

    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0, respMsg,
                           respBuf.size());
    EXPECT_TRUE(called);

    // Now the queue for testEid is empty; send another response
    // Should not crash, and the "no matching request" path is taken
    EXPECT_NO_THROW(handler.handleResponse(MCTP_MSG_TAG_REQ, testEid, 0, 0, 0,
                                           respMsg, respBuf.size()));
}

// ===========================================================================
// Large request triggers setSendBufferSize
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, RegisterRequest_LargeMsg_SetsSendBuffer)
{
    mockMctpHandler.sendResult = 1;

    // Create a request larger than the registered send buffer size
    // sockManager was registered with bufSize=4096, so make msg > 4096
    std::vector<uint8_t> largeReq(5000, 0);
    // Fill in minimal NSM header
    auto nsmMsg = reinterpret_cast<nsm_msg*>(largeReq.data());
    encode_ping_req(0, nsmMsg);

    bool called = false;
    auto rc = handler.registerRequest(
        MCTP_MSG_TAG_REQ, testEid, nsmMsg->hdr.nvidia_msg_type,
        nsmMsg->payload[0], std::move(largeReq),
        [&](eid_t, const nsm_msg*, size_t) { called = true; });
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ===========================================================================
// Instance ID exception in runRegisteredRequest
// ===========================================================================

TEST_F(RequesterHandlerBranchTest,
       RunRegisteredRequest_InstanceIdException_ReturnsError)
{
    mockMctpHandler.sendResult = 1;

    // Exhaust the instance ID for testEid by locking it
    auto id = instanceIdDb.next(testEid);
    (void)id;

    // Now try to register - runRegisteredRequest will try instanceIdDb.next()
    // which throws because testEid is already locked
    bool called = false;
    const nsm_msg* respOut = nullptr;
    size_t respLen = 0;

    auto rc = registerWithCallback(testEid, called, respOut, respLen);
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_FALSE(called);

    // Clean up
    instanceIdDb.free(testEid, id);
}

// ===========================================================================
// Verbose mode construction
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, VerboseMode_Construction)
{
    TestHandler verboseHandler{event,
                               instanceIdDb,
                               sockManager,
                               /*verbose=*/true,
                               std::chrono::seconds(5),
                               /*numRetries=*/0,
                               std::chrono::milliseconds(100)};
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
// runRegisteredRequest: empty queue returns NSM_SUCCESS immediately
// ===========================================================================

TEST_F(RequesterHandlerBranchTest,
       RunRegisteredRequest_EmptyQueue_ReturnsSuccess)
{
    // Call runRegisteredRequest directly with an empty handlers map
    std::unordered_map<
        eid_t, std::queue<std::tuple<std::unique_ptr<requester::Request>,
                                     requester::ResponseHandler,
                                     std::unique_ptr<sdbusplus::Timer>, bool>>>
        emptyHandlers;
    auto rc = handler.runRegisteredRequest(testEid, emptyHandlers,
                                           std::chrono::seconds(2));
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ===========================================================================
// handleResponseImpl: direct call with empty handlers map
// ===========================================================================

TEST_F(RequesterHandlerBranchTest,
       HandleResponseImpl_EmptyHandlers_ReturnsFalse)
{
    std::unordered_map<
        eid_t, std::queue<std::tuple<std::unique_ptr<requester::Request>,
                                     requester::ResponseHandler,
                                     std::unique_ptr<sdbusplus::Timer>, bool>>>
        emptyHandlers;
    auto respBuf = makePingResponse(0);
    auto respMsg = reinterpret_cast<const nsm_msg*>(respBuf.data());

    bool found = handler.handleResponseImpl(testEid, 0, 0, 0, respMsg,
                                            respBuf.size(), emptyHandlers,
                                            std::chrono::seconds(2));
    EXPECT_FALSE(found);
}

// ===========================================================================
// SendRecvNsmMsg: operator T() conversion tests (COVERAGE_DISABLE_COROUTINES)
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, SendRecvNsmMsg_OperatorT_IntConversion)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    // Initial rc = NSM_ERROR
#ifdef COVERAGE_DISABLE_COROUTINES
    int val = awaitable;
    EXPECT_EQ(val, static_cast<int>(NSM_ERROR));

    uint8_t u8val = awaitable;
    EXPECT_EQ(u8val, static_cast<uint8_t>(NSM_ERROR));

    bool bval = awaitable;
    EXPECT_TRUE(bval); // NSM_ERROR != 0
#else
    EXPECT_EQ(awaitable.await_resume(), NSM_ERROR);
#endif
}

TEST_F(RequesterHandlerBranchTest, SendRecvNsmMsg_OperatorT_AfterHandleResponse)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    awaitable.resumeHandle = std::noop_coroutine();

    // Simulate successful response
    nsm_msg fakeResponse{};
    awaitable.HandleResponse(1, &fakeResponse, sizeof(nsm_msg_hdr));

#ifdef COVERAGE_DISABLE_COROUTINES
    int val = awaitable;
    EXPECT_EQ(val, static_cast<int>(NSM_SW_SUCCESS));

    uint8_t u8val = awaitable;
    EXPECT_EQ(u8val, NSM_SW_SUCCESS);
#else
    EXPECT_EQ(awaitable.await_resume(), NSM_SW_SUCCESS);
#endif
}

TEST_F(RequesterHandlerBranchTest,
       SendRecvNsmMsg_OperatorT_AfterTimeoutResponse)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    awaitable.resumeHandle = std::noop_coroutine();

    // Simulate timeout (null response)
    awaitable.HandleResponse(1, nullptr, 0);

#ifdef COVERAGE_DISABLE_COROUTINES
    int val = awaitable;
    EXPECT_EQ(val, static_cast<int>(NSM_SW_ERROR_TIMEOUT));

    uint8_t u8val = awaitable;
    EXPECT_EQ(u8val, static_cast<uint8_t>(NSM_SW_ERROR_TIMEOUT));
#else
    EXPECT_EQ(awaitable.await_resume(), NSM_SW_ERROR_TIMEOUT);
#endif
}

// ===========================================================================
// SendRecvNsmMsg: HandleResponse with null response but non-zero length
// ===========================================================================

TEST_F(RequesterHandlerBranchTest,
       SendRecvNsmMsg_HandleResponse_NullRespNonZeroLen_SetsTimeout)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    awaitable.resumeHandle = std::noop_coroutine();

    // null response with non-zero length: condition is (nullptr || !length)
    // nullptr == true, so timeout path taken regardless of length
    awaitable.HandleResponse(1, nullptr, 42);
    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR_TIMEOUT);
}

// ===========================================================================
// SendRecvNsmMsg: await_suspend with only responseMsg null
// ===========================================================================

TEST_F(RequesterHandlerBranchTest,
       SendRecvNsmMsg_AwaitSuspend_OnlyResponseMsgNull)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       nullptr, &respLen);
    bool suspended = awaitable.await_suspend(std::noop_coroutine());
    EXPECT_FALSE(suspended);
    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR_NULL);
}

TEST_F(RequesterHandlerBranchTest,
       SendRecvNsmMsg_AwaitSuspend_OnlyResponseLenNull)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, nullptr);
    bool suspended = awaitable.await_suspend(std::noop_coroutine());
    EXPECT_FALSE(suspended);
    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// SendRecvNsmMsg: constructor initializes rc to NSM_ERROR
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, SendRecvNsmMsg_Constructor_InitializesRc)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    EXPECT_EQ(awaitable.rc, NSM_ERROR);
    EXPECT_EQ(awaitable.eid, 1);
    EXPECT_EQ(awaitable.responseMsg, &respMsg);
    EXPECT_EQ(awaitable.responseLen, &respLen);
}

// ===========================================================================
// SendRecvNsmMsg: await_ready always returns false
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, SendRecvNsmMsg_AwaitReady_ReturnsFalse)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    EXPECT_FALSE(awaitable.await_ready());
}

// ===========================================================================
// SendRecvNsmMsg: await_resume returns current rc
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, SendRecvNsmMsg_AwaitResume_ReturnsRc)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    EXPECT_EQ(awaitable.await_resume(), NSM_ERROR);

    // After setting rc manually
    awaitable.rc = NSM_SW_SUCCESS;
    EXPECT_EQ(awaitable.await_resume(), NSM_SW_SUCCESS);
}

// ===========================================================================
// SendRecvNsmMsg: await_suspend register succeeds, stores handle
// ===========================================================================

TEST_F(RequesterHandlerBranchTest,
       SendRecvNsmMsg_AwaitSuspend_Success_StoresHandle)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    encode_ping_req(0, reinterpret_cast<nsm_msg*>(req.data()));
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    auto handle = std::noop_coroutine();
    bool suspended = awaitable.await_suspend(handle);
    EXPECT_TRUE(suspended);
    EXPECT_EQ(awaitable.resumeHandle, handle);
}

// ===========================================================================
// SendRecvNsmMsg: await_suspend register fails, rc set to error
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, SendRecvNsmMsg_AwaitSuspend_Fail_SetsErrorRc)
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
// SendRecvNsmMsg: HandleResponse valid sets responseMsg and responseLen
// ===========================================================================

TEST_F(RequesterHandlerBranchTest,
       SendRecvNsmMsg_HandleResponse_ValidSetsOutputs)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    awaitable.resumeHandle = std::noop_coroutine();

    nsm_msg fakeResponse{};
    awaitable.HandleResponse(1, &fakeResponse, sizeof(nsm_msg));

    EXPECT_EQ(awaitable.rc, NSM_SW_SUCCESS);
    EXPECT_EQ(respMsg, &fakeResponse);
    EXPECT_EQ(respLen, sizeof(nsm_msg));
}

// ===========================================================================
// lg2::details::log_convert specialization test (COVERAGE_DISABLE_COROUTINES)
// ===========================================================================

#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(RequesterHandlerBranchTest, LogConvert_SendRecvNsmMsg_Specialization)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    awaitable.rc = NSM_SW_SUCCESS;

    // Exercise the log_convert specialization - this should not crash
    // and should produce a tuple with the correct uint64_t value
    lg2::info("Test log with SendRecvNsmMsg rc={RC}", "RC", awaitable);
}

TEST_F(RequesterHandlerBranchTest, LogConvert_SendRecvNsmMsg_ErrorValue)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    // rc defaults to NSM_ERROR
    lg2::error("Test log with error rc={RC}", "RC", awaitable);
}
#endif // COVERAGE_DISABLE_COROUTINES

// ===========================================================================
// Multiple EIDs: requests for different EIDs are independent
// ===========================================================================

TEST_F(RequesterHandlerBranchTest, MultipleEids_IndependentProcessing)
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

    // Register requests for both EIDs
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

    // Respond to eidB first
    auto respBufB = makePingResponse(0);
    auto respMsgB = reinterpret_cast<const nsm_msg*>(respBufB.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, eidB, 0, 0, 0, respMsgB,
                           respBufB.size());

    EXPECT_FALSE(calledA);
    EXPECT_TRUE(calledB);

    // Respond to eidA
    auto respBufA = makePingResponse(0);
    auto respMsgA = reinterpret_cast<const nsm_msg*>(respBufA.data());
    handler.handleResponse(MCTP_MSG_TAG_REQ, eidA, 0, 0, 0, respMsgA,
                           respBufA.size());

    EXPECT_TRUE(calledA);
}
