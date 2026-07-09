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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "instance_id.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_prober.hpp"
#include "requester/request_timeout_tracker.hpp"
#include "requester/response_mismatch_tracker.hpp"
#include "requester/retry_backoff_utils.hpp"
#include "socket_manager.hpp"

#undef private
#undef protected

#include "base.h"

#include "common/event.hpp"

#include <coroutine>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

using namespace requester;
using namespace requester::retry;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void allocMessage(const std::vector<uint8_t>& response,
                         std::shared_ptr<const nsm_msg>& responseMsg,
                         size_t* responseLen)
{
    *responseLen = response.size();
    if (*responseLen > 0)
    {
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(*responseLen)),
            [](const nsm_msg* ptr) { free(const_cast<nsm_msg*>(ptr)); });
        memcpy(const_cast<nsm_msg*>(responseMsg.get()), response.data(),
               *responseLen);
    }
}

// ===========================================================================
// retry_backoff_utils.hpp tests
// ===========================================================================

TEST(RetryBackoffUtils, formatSummarySuccess)
{
    OperationSummary s;
    s.opName = "Ping";
    s.eid = 5;
    s.totalAttempts = 1;
    s.success = true;
    s.lastCc = NSM_SUCCESS;
    s.lastReason = ERR_NULL;
    s.totalWaitMs = 0;

    std::string result = formatSummary(s);

    EXPECT_NE(result.find("Ping"), std::string::npos);
    EXPECT_NE(result.find("eid=5"), std::string::npos);
    EXPECT_NE(result.find("successful"), std::string::npos);
    EXPECT_NE(result.find("Total attempts=1"), std::string::npos);
}

TEST(RetryBackoffUtils, formatSummaryFailure)
{
    OperationSummary s;
    s.opName = "QueryDeviceIdentification";
    s.eid = 10;
    s.totalAttempts = 3;
    s.success = false;
    s.lastCc = NSM_ERR_NOT_READY;
    s.lastReason = ERR_NULL;
    s.totalWaitMs = 300;

    std::string result = formatSummary(s);

    EXPECT_NE(result.find("QueryDeviceIdentification"), std::string::npos);
    EXPECT_NE(result.find("eid=10"), std::string::npos);
    EXPECT_NE(result.find("failed after back off"), std::string::npos);
    EXPECT_NE(result.find("Total attempts=3"), std::string::npos);
}

TEST(RetryBackoffUtils, logNotReadyRetryDoesNotCrash)
{
    // logNotReadyRetry only logs; verify it executes without crash
    EXPECT_NO_THROW(logNotReadyRetry("TestOp", 7, 1, 3, 100, ERR_NULL));
}

TEST(RetryBackoffUtils, linearBackoffConfigDefaults)
{
    LinearBackoffConfig cfg{};
    EXPECT_GT(cfg.maxRetries, 0u);
    EXPECT_GT(cfg.delayMs, 0u);
}

TEST(RetryBackoffUtils, operationSummaryDefaultsValid)
{
    OperationSummary s;
    EXPECT_EQ(s.eid, 0);
    EXPECT_EQ(s.totalAttempts, 0);
    EXPECT_FALSE(s.success);
    EXPECT_EQ(s.lastCc, NSM_SUCCESS);
    EXPECT_EQ(s.lastReason, ERR_NULL);
    EXPECT_EQ(s.totalWaitMs, 0u);
}

// ===========================================================================
// DeviceRequestTimeOutTracker tests
// ===========================================================================

class DeviceRequestTimeOutTrackerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Clear static state between tests
        DeviceRequestTimeOutTracker::instances.clear();
    }

    void TearDown() override
    {
        DeviceRequestTimeOutTracker::instances.clear();
    }

    // Helper: build a minimal RequestBase for eid
    static RequestBase makeRequest(eid_t eid, uint8_t tag = 0)
    {
        // Minimal valid NSM ping request bytes
        ::Request msg(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
        auto nsmMsg = reinterpret_cast<nsm_msg*>(msg.data());
        encode_ping_req(0, nsmMsg);
        return RequestBase(eid, tag, std::move(msg));
    }
};

TEST_F(DeviceRequestTimeOutTrackerTest, pushWithTimeout_RecordsFirstTimeout)
{
    auto req = makeRequest(42);
    DeviceRequestTimeOutTracker::pushWithTimeout(req);

    auto& tracker = DeviceRequestTimeOutTracker::instance(42);
    EXPECT_TRUE(tracker.timeoutMessage.has_value());
    EXPECT_EQ(tracker.timeoutMessage->eid, 42);
}

TEST_F(DeviceRequestTimeOutTrackerTest, pushWithTimeout_IgnoresSubsequent)
{
    auto req1 = makeRequest(42, 1);
    auto req2 = makeRequest(42, 2);

    DeviceRequestTimeOutTracker::pushWithTimeout(req1);
    DeviceRequestTimeOutTracker::pushWithTimeout(req2); // should be ignored

    auto& tracker = DeviceRequestTimeOutTracker::instance(42);
    EXPECT_TRUE(tracker.timeoutMessage.has_value());
    // The tag from req1 should be stored (req2 was ignored)
    EXPECT_EQ(tracker.timeoutMessage->tag, 1);
}

TEST_F(DeviceRequestTimeOutTrackerTest, pushWithoutTimeout_PushesRequest)
{
    auto req = makeRequest(10);
    DeviceRequestTimeOutTracker::pushWithoutTimeout(req);

    auto& tracker = DeviceRequestTimeOutTracker::instance(10);
    EXPECT_FALSE(tracker.timeoutMessage.has_value());
    EXPECT_EQ(tracker.size(), 1u);
}

TEST_F(DeviceRequestTimeOutTrackerTest, pushWithoutTimeout_ResetsAfterTimeout)
{
    auto req1 = makeRequest(5);
    // First record a timeout
    DeviceRequestTimeOutTracker::pushWithTimeout(req1);
    {
        auto& tracker = DeviceRequestTimeOutTracker::instance(5);
        EXPECT_TRUE(tracker.timeoutMessage.has_value());
    }

    // Now device responds successfully → reset
    auto req2 = makeRequest(5);
    DeviceRequestTimeOutTracker::pushWithoutTimeout(req2);

    auto& tracker = DeviceRequestTimeOutTracker::instance(5);
    EXPECT_FALSE(tracker.timeoutMessage.has_value());
    EXPECT_EQ(tracker.size(), 1u);
}

TEST_F(DeviceRequestTimeOutTrackerTest, pushWithoutTimeout_MultipleRequests)
{
    // MAXSIZE = 1, so second push should evict first
    auto req1 = makeRequest(7, 1);
    auto req2 = makeRequest(7, 2);

    DeviceRequestTimeOutTracker::pushWithoutTimeout(req1);
    DeviceRequestTimeOutTracker::pushWithoutTimeout(req2);

    auto& tracker = DeviceRequestTimeOutTracker::instance(7);
    EXPECT_EQ(tracker.size(), 1u); // MAXSIZE = 1
}

TEST_F(DeviceRequestTimeOutTrackerTest, logFailures_EmptyInstances)
{
    // Should not crash with empty instances
    EXPECT_NO_THROW(DeviceRequestTimeOutTracker::logFailures());
}

TEST_F(DeviceRequestTimeOutTrackerTest, logFailures_WithTimeout)
{
    auto req = makeRequest(99);
    DeviceRequestTimeOutTracker::pushWithTimeout(req);

    // Should not crash with a timeout message present
    EXPECT_NO_THROW(DeviceRequestTimeOutTracker::logFailures());
}

TEST_F(DeviceRequestTimeOutTrackerTest, logFailures_WithPriorRequests)
{
    // Push some prior requests then a timeout
    auto req1 = makeRequest(20, 0);
    auto req2 = makeRequest(20, 1);
    DeviceRequestTimeOutTracker::pushWithoutTimeout(req1);
    DeviceRequestTimeOutTracker::pushWithoutTimeout(req2);

    auto reqTimeout = makeRequest(20, 2);
    DeviceRequestTimeOutTracker::pushWithTimeout(reqTimeout);

    EXPECT_NO_THROW(DeviceRequestTimeOutTracker::logFailures());
}

TEST_F(DeviceRequestTimeOutTrackerTest, instance_CreatesNewForNewEid)
{
    DeviceRequestTimeOutTracker::instance(100);
    EXPECT_EQ(DeviceRequestTimeOutTracker::instances.size(), 1u);

    DeviceRequestTimeOutTracker::instance(200);
    EXPECT_EQ(DeviceRequestTimeOutTracker::instances.size(), 2u);
}

TEST_F(DeviceRequestTimeOutTrackerTest, instance_ReturnsSameForSameEid)
{
    auto& t1 = DeviceRequestTimeOutTracker::instance(55);
    auto& t2 = DeviceRequestTimeOutTracker::instance(55);
    EXPECT_EQ(&t1, &t2);
    EXPECT_EQ(DeviceRequestTimeOutTracker::instances.size(), 1u);
}

// logTimeOutFailure with no timeout message → Decision 'false' branch at L89
// (timeoutMessage.has_value() == false → skip the if-body)
TEST_F(DeviceRequestTimeOutTrackerTest, logTimeOutFailure_NoTimeout_SkipsIfBody)
{
    auto req = makeRequest(88);
    DeviceRequestTimeOutTracker::pushWithoutTimeout(req);

    auto& tracker = DeviceRequestTimeOutTracker::instance(88);
    EXPECT_FALSE(tracker.timeoutMessage.has_value());

    // Call logTimeOutFailure when timeoutMessage is empty:
    // the if (timeoutMessage.has_value()) body is skipped (FALSE branch).
    EXPECT_NO_THROW(tracker.logTimeOutFailure());
}

// ===========================================================================
// MctpEndpointProber tests
// ===========================================================================

class MctpEndpointProberTest : public ::testing::Test
{
  protected:
    // Build a valid ping response.
    // For NSM_SUCCESS: use full nsm_common_resp-sized buffer.
    // For non-success cc: decode_reason_code_and_cc does an EXACT size check
    // against sizeof(nsm_msg_hdr)+sizeof(nsm_common_non_success_resp), so we
    // must allocate exactly that many bytes.
    static std::vector<uint8_t> buildPingResp(uint8_t cc = NSM_SUCCESS)
    {
        if (cc == NSM_SUCCESS)
        {
            std::vector<uint8_t> buf(
                sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
            auto msg = reinterpret_cast<nsm_msg*>(buf.data());
            encode_ping_resp(0, ERR_NULL, msg);
            return buf;
        }
        // Non-success: encode_reason_code writes into
        // nsm_common_non_success_resp
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_common_resp(0, cc, ERR_NULL,
                           NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_PING, msg);
        return buf;
    }

    // Build a valid query device identification response.
    // Same size constraint for non-success cc.
    static std::vector<uint8_t> buildQueryDeviceIdResp(uint8_t cc = NSM_SUCCESS,
                                                       uint8_t devId = 3,
                                                       uint8_t devInst = 1)
    {
        if (cc == NSM_SUCCESS)
        {
            std::vector<uint8_t> buf(
                sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_device_identification_resp),
                0);
            auto msg = reinterpret_cast<nsm_msg*>(buf.data());
            encode_query_device_identification_resp(0, cc, ERR_NULL, devId,
                                                    devInst, msg);
            return buf;
        }
        // Non-success: encode_reason_code writes into
        // nsm_common_non_success_resp
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_query_device_identification_resp(0, cc, ERR_NULL, devId, devInst,
                                                msg);
        return buf;
    }

    // Create a sendRecvFn that returns a canned response
    static MctpEndpointProber::SendRecvFn
        makeSendRecv(std::vector<uint8_t> response, uint8_t transportRc = 0)
    {
        return [response, transportRc](
                   eid_t, ::Request&, std::shared_ptr<const nsm_msg>& respMsg,
                   size_t* respLen) -> requester::Coroutine {
            if (transportRc != 0)
            {
                // coverity[missing_return]
                co_return transportRc;
            }
            allocMessage(response, respMsg, respLen);
            // coverity[missing_return]
            co_return NSM_SW_SUCCESS;
        };
    }

    // Create prober with a given sendRecvFn
    static MctpEndpointProber makeProber(MctpEndpointProber::SendRecvFn fn)
    {
        return MctpEndpointProber(nullptr, LinearBackoffConfig{},
                                  std::move(fn));
    }
};

TEST_F(MctpEndpointProberTest, getLastPingSummary_NoPingYet_ReturnsNullopt)
{
    auto prober = makeProber(makeSendRecv(buildPingResp()));
    EXPECT_FALSE(prober.getLastPingSummary(42).has_value());
}

TEST_F(MctpEndpointProberTest,
       getLastQueryDeviceIdentificationSummary_NoCallYet_ReturnsNullopt)
{
    auto prober = makeProber(makeSendRecv(buildPingResp()));
    EXPECT_FALSE(
        prober.getLastQueryDeviceIdentificationSummary(42).has_value());
}

TEST_F(MctpEndpointProberTest, ping_Success_FirstAttempt)
{
    auto prober = makeProber(makeSendRecv(buildPingResp(NSM_SUCCESS)));

    auto coro = prober.ping(1);
    ASSERT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastPingSummary(1);
    ASSERT_TRUE(summary.has_value());
    EXPECT_TRUE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 1);
    EXPECT_EQ(summary->eid, 1);
}

TEST_F(MctpEndpointProberTest, ping_NonRetryableFailure)
{
    // cc = NSM_ERROR (not NOT_READY) → non-retryable failure
    auto prober = makeProber(makeSendRecv(buildPingResp(NSM_ERROR)));

    auto coro = prober.ping(2);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastPingSummary(2);
    ASSERT_TRUE(summary.has_value());
    EXPECT_FALSE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 1);
}

TEST_F(MctpEndpointProberTest, ping_TransportFailure)
{
    // sendRecvFn returns non-zero → transport error
    auto prober = makeProber(makeSendRecv({}, NSM_SW_ERROR));

    auto coro = prober.ping(3);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);
}

TEST_F(MctpEndpointProberTest, ping_DifferentEids_GetSeparateSummaries)
{
    // ping eid=4 with failure, eid=5 with success
    auto prober1 = makeProber(makeSendRecv(buildPingResp(NSM_ERROR)));
    auto prober2 = makeProber(makeSendRecv(buildPingResp(NSM_SUCCESS)));

    prober1.ping(4);
    prober2.ping(5);

    EXPECT_FALSE(prober1.getLastPingSummary(5).has_value());
    EXPECT_FALSE(prober2.getLastPingSummary(4).has_value());
    EXPECT_TRUE(prober1.getLastPingSummary(4).has_value());
    EXPECT_TRUE(prober2.getLastPingSummary(5).has_value());
}

TEST_F(MctpEndpointProberTest, getQueryDeviceIdentification_Success)
{
    auto prober =
        makeProber(makeSendRecv(buildQueryDeviceIdResp(NSM_SUCCESS, 3, 1)));

    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(5, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(devId, 3);
    EXPECT_EQ(devInst, 1);

    auto summary = prober.getLastQueryDeviceIdentificationSummary(5);
    ASSERT_TRUE(summary.has_value());
    EXPECT_TRUE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 1);
}

TEST_F(MctpEndpointProberTest, getQueryDeviceIdentification_NonRetryableFailure)
{
    auto prober =
        makeProber(makeSendRecv(buildQueryDeviceIdResp(NSM_ERROR, 0, 0)));

    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(6, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastQueryDeviceIdentificationSummary(6);
    ASSERT_TRUE(summary.has_value());
    EXPECT_FALSE(summary->success);
}

TEST_F(MctpEndpointProberTest, getQueryDeviceIdentification_TransportFailure)
{
    auto prober = makeProber(makeSendRecv({}, NSM_SW_ERROR));

    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(7, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);
}

TEST_F(MctpEndpointProberTest, getQueryDeviceIdentification_NonSuccessCc)
{
    // cc = NSM_ERROR (non-success, non-NOT_READY) via error response
    auto prober =
        makeProber(makeSendRecv(buildQueryDeviceIdResp(NSM_ERROR, 0, 0)));

    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(8, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);
}

TEST_F(MctpEndpointProberTest, logAllSummaries_EmptyDoesNotCrash)
{
    auto prober = makeProber(makeSendRecv(buildPingResp()));
    EXPECT_NO_THROW(prober.logAllSummaries());
}

TEST_F(MctpEndpointProberTest, logAllSummaries_WithSummaries)
{
    auto prober = makeProber(makeSendRecv(buildPingResp(NSM_SUCCESS)));
    prober.ping(1);

    uint8_t devId = 0, devInst = 0;
    MctpEndpointProber prober2(nullptr, LinearBackoffConfig{},
                               makeSendRecv(buildQueryDeviceIdResp()));
    prober2.getQueryDeviceIdentification(2, devId, devInst);

    EXPECT_NO_THROW(prober.logAllSummaries());
    EXPECT_NO_THROW(prober2.logAllSummaries());
}

TEST_F(MctpEndpointProberTest, ping_MultipleEids_IndependentSummaries)
{
    auto prober = makeProber(makeSendRecv(buildPingResp(NSM_SUCCESS)));

    prober.ping(10);
    prober.ping(20);

    auto s10 = prober.getLastPingSummary(10);
    auto s20 = prober.getLastPingSummary(20);
    auto s30 = prober.getLastPingSummary(30); // never pinged

    EXPECT_TRUE(s10.has_value());
    EXPECT_EQ(s10->eid, 10);
    EXPECT_TRUE(s20.has_value());
    EXPECT_EQ(s20->eid, 20);
    EXPECT_FALSE(s30.has_value());
}

// ===========================================================================
// Handler<> tests
// ===========================================================================

using TestHandler = requester::Handler<requester::Request>;

class HandlerTest : public ::testing::Test
{
  protected:
    common::Event event;
    nsm::InstanceIdDb instanceIdDb;
    mctp_socket::Manager socketManager;
    TestHandler handler{event, instanceIdDb, socketManager, false};
};

TEST_F(HandlerTest, ConstructionSucceeds)
{
    // Just verify construction doesn't throw
    SUCCEED();
}

TEST_F(HandlerTest, SetSocketHandler_DoesNotCrash)
{
    mctp_socket::Handler* sockHandler = nullptr;
    EXPECT_NO_THROW(handler.setSocketHandler(sockHandler));
}

TEST_F(HandlerTest, RunRegisteredRequest_EmptyHandlers_ReturnsSuccess)
{
    std::unordered_map<eid_t, TestHandler::RequestQueue> emptyHandlers;
    auto rc = handler.runRegisteredRequest(42, emptyHandlers,
                                           std::chrono::seconds(5));
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST_F(HandlerTest, HandleResponseImpl_NoMatchingEid_ReturnsNotFound)
{
    std::unordered_map<eid_t, TestHandler::RequestQueue> emptyHandlers;
    nsm_msg response{};
    auto result = handler.handleResponseImpl(99, 0, 0, 0, &response,
                                             sizeof(response), emptyHandlers,
                                             std::chrono::seconds(5));
    EXPECT_EQ(result, MatchResult::NotFound);
}

TEST_F(HandlerTest, HandleResponse_NoMatchingRequest_DoesNotCrash)
{
    // handleResponse with no registered request for the eid should log + return
    nsm_msg response{};
    EXPECT_NO_THROW(
        handler.handleResponse(0, 99, 0, 0, 0, &response, sizeof(response)));
}

// NVBug 6368378: GPU may send a response with a different NSM type/command but
// a matching instance ID. nsmd must drop such responses and not invoke the
// request's callback, leaving the outstanding request intact for normal
// timeout.
TEST_F(HandlerTest,
       HandleResponseImpl_TypeMismatch_ReturnsTypeCmdRejected_NoBug6368378)
{
    constexpr eid_t eid = 5;
    constexpr uint8_t instanceId = 7;
    // Request: NSM Capabilities type (0x00), GetSupportedCommandCodes (0x02)
    constexpr uint8_t reqType = 0x00;
    constexpr uint8_t reqCmd = 0x02;
    // Response: NSM PlatformEnvironmentals type (0x03), SetClockLimit (0x10)
    constexpr uint8_t respType = 0x03;
    constexpr uint8_t respCmd = 0x10;

    std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req),
                                0);
    auto* nsmHdr = reinterpret_cast<nsm_msg*>(reqMsg.data());
    nsmHdr->hdr.instance_id = instanceId;
    nsmHdr->hdr.nvidia_msg_type = reqType;
    nsmHdr->payload[0] = reqCmd;

    bool handlerCalled = false;
    auto responseHandler = [&handlerCalled](eid_t, const nsm_msg*, size_t) {
        handlerCalled = true;
    };

    auto request = std::make_unique<requester::Request>(
        -1, eid, 0, event, nullptr, std::move(reqMsg), 0,
        std::chrono::milliseconds(0), false);

    // timer is null: the mismatch path returns before any timer dereference
    std::unordered_map<eid_t, TestHandler::RequestQueue> testHandlers;
    testHandlers[eid].emplace(
        std::make_tuple(std::move(request), std::move(responseHandler),
                        std::unique_ptr<sdbusplus::Timer>{}, true));

    nsm_msg response{};
    auto result = handler.handleResponseImpl(
        eid, instanceId, respType, respCmd, &response, sizeof(response),
        testHandlers, std::chrono::seconds(5));

    EXPECT_EQ(result, MatchResult::TypeCmdRejected);
    EXPECT_FALSE(handlerCalled);
}

TEST_F(HandlerTest,
       HandleResponseImpl_CommandMismatch_ReturnsTypeCmdRejected_NoBug6368378)
{
    constexpr eid_t eid = 6;
    constexpr uint8_t instanceId = 3;
    constexpr uint8_t reqType = 0x00;
    constexpr uint8_t reqCmd = 0x02;
    // Same type, different command
    constexpr uint8_t respType = 0x00;
    constexpr uint8_t respCmd = 0x09;

    std::vector<uint8_t> reqMsg(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req),
                                0);
    auto* nsmHdr = reinterpret_cast<nsm_msg*>(reqMsg.data());
    nsmHdr->hdr.instance_id = instanceId;
    nsmHdr->hdr.nvidia_msg_type = reqType;
    nsmHdr->payload[0] = reqCmd;

    bool handlerCalled = false;
    auto responseHandler = [&handlerCalled](eid_t, const nsm_msg*, size_t) {
        handlerCalled = true;
    };

    auto request = std::make_unique<requester::Request>(
        -1, eid, 0, event, nullptr, std::move(reqMsg), 0,
        std::chrono::milliseconds(0), false);

    std::unordered_map<eid_t, TestHandler::RequestQueue> testHandlers;
    testHandlers[eid].emplace(
        std::make_tuple(std::move(request), std::move(responseHandler),
                        std::unique_ptr<sdbusplus::Timer>{}, true));

    nsm_msg response{};
    auto result = handler.handleResponseImpl(
        eid, instanceId, respType, respCmd, &response, sizeof(response),
        testHandlers, std::chrono::seconds(5));

    EXPECT_EQ(result, MatchResult::TypeCmdRejected);
    EXPECT_FALSE(handlerCalled);
}

// ===========================================================================
// ResponseMismatchTracker tests
// ===========================================================================

class ResponseMismatchTrackerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        requester::ResponseMismatchTracker::instances.clear();
    }

    // Minimal dummy byte buffers — content irrelevant for dedup/count tests.
    const std::vector<uint8_t> dummyReq{0x0e, 0x02, 0x00, 0x02};
    const std::vector<uint8_t> dummyResp{0x0e, 0x02, 0x03, 0x10};
};

TEST_F(ResponseMismatchTrackerTest, Empty_LogMismatchesDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(
        requester::ResponseMismatchTracker::logMismatches());
    EXPECT_TRUE(requester::ResponseMismatchTracker::instances.empty());
}

TEST_F(ResponseMismatchTrackerTest,
       TypeCmdRejected_FirstMismatch_StoredWithCountOne)
{
    requester::ResponseMismatchTracker::recordTypeCmdRejected(
        5, 7, 0x00, 0x02, 0x03, 0x10, dummyReq, dummyResp.data(),
        dummyResp.size());

    auto& queue = requester::ResponseMismatchTracker::instances[5];
    ASSERT_EQ(queue.size(), 1u);
    EXPECT_EQ(queue.front().reason, requester::MismatchReason::TypeCmdRejected);
    EXPECT_EQ(queue.front().expectedType, 0x00);
    EXPECT_EQ(queue.front().expectedCmd, 0x02);
    EXPECT_EQ(queue.front().gotType, 0x03);
    EXPECT_EQ(queue.front().gotCmd, 0x10);
    EXPECT_EQ(queue.front().count, 1u);
}

TEST_F(ResponseMismatchTrackerTest,
       TypeCmdRejected_Repeated_CountIncrements_NoNewEntry)
{
    requester::ResponseMismatchTracker::recordTypeCmdRejected(
        5, 7, 0x00, 0x02, 0x03, 0x10, dummyReq, dummyResp.data(),
        dummyResp.size());
    requester::ResponseMismatchTracker::recordTypeCmdRejected(
        5, 7, 0x00, 0x02, 0x03, 0x10, dummyReq, dummyResp.data(),
        dummyResp.size());
    requester::ResponseMismatchTracker::recordTypeCmdRejected(
        5, 7, 0x00, 0x02, 0x03, 0x10, dummyReq, dummyResp.data(),
        dummyResp.size());

    auto& queue = requester::ResponseMismatchTracker::instances[5];
    ASSERT_EQ(queue.size(), 1u);
    EXPECT_EQ(queue.front().count, 3u);
}

TEST_F(ResponseMismatchTrackerTest,
       TypeCmdRejected_DifferentMismatch_SameEid_AddsNewEntry)
{
    requester::ResponseMismatchTracker::recordTypeCmdRejected(
        5, 7, 0x00, 0x02, 0x03, 0x10, dummyReq, dummyResp.data(),
        dummyResp.size());
    requester::ResponseMismatchTracker::recordTypeCmdRejected(
        5, 3, 0x01, 0x05, 0x02, 0x08, dummyReq, dummyResp.data(),
        dummyResp.size());

    EXPECT_EQ(requester::ResponseMismatchTracker::instances[5].size(), 2u);
}

TEST_F(ResponseMismatchTrackerTest, NotFound_FirstOccurrence_StoredWithCountOne)
{
    requester::ResponseMismatchTracker::recordNotFound(
        5, 0, 7, 0x03, 0x10, dummyResp.data(), dummyResp.size());

    auto& queue = requester::ResponseMismatchTracker::instances[5];
    ASSERT_EQ(queue.size(), 1u);
    EXPECT_EQ(queue.front().reason, requester::MismatchReason::NotFound);
    EXPECT_EQ(queue.front().gotType, 0x03);
    EXPECT_EQ(queue.front().gotCmd, 0x10);
    EXPECT_EQ(queue.front().count, 1u);
}

TEST_F(ResponseMismatchTrackerTest,
       NotFound_Repeated_CountIncrements_NoNewEntry)
{
    requester::ResponseMismatchTracker::recordNotFound(
        5, 0, 7, 0x03, 0x10, dummyResp.data(), dummyResp.size());
    requester::ResponseMismatchTracker::recordNotFound(
        5, 0, 7, 0x03, 0x10, dummyResp.data(), dummyResp.size());
    requester::ResponseMismatchTracker::recordNotFound(
        5, 0, 7, 0x03, 0x10, dummyResp.data(), dummyResp.size());

    auto& queue = requester::ResponseMismatchTracker::instances[5];
    ASSERT_EQ(queue.size(), 1u);
    EXPECT_EQ(queue.front().count, 3u);
}

// Reason must be part of the dedup key: same gotType/gotCmd with different
// reason must produce two separate entries, not count as one.
TEST_F(ResponseMismatchTrackerTest, SameGotTypeCmd_DifferentReason_TwoEntries)
{
    requester::ResponseMismatchTracker::recordTypeCmdRejected(
        5, 3, 0x00, 0x02, 0x03, 0x10, dummyReq, dummyResp.data(),
        dummyResp.size());
    requester::ResponseMismatchTracker::recordNotFound(
        5, 0, 7, 0x03, 0x10, dummyResp.data(), dummyResp.size());

    EXPECT_EQ(requester::ResponseMismatchTracker::instances[5].size(), 2u);
}

TEST_F(ResponseMismatchTrackerTest, RingBufferCap_OldestEvictedWhenFull)
{
    for (uint8_t i = 0; i < 10; ++i)
    {
        requester::ResponseMismatchTracker::recordTypeCmdRejected(
            5, i, 0x00, i, 0x01, i, dummyReq, dummyResp.data(),
            dummyResp.size());
    }
    EXPECT_EQ(requester::ResponseMismatchTracker::instances[5].size(), 10u);

    // 11th distinct entry evicts oldest
    requester::ResponseMismatchTracker::recordTypeCmdRejected(
        5, 10, 0x00, 10, 0x01, 10, dummyReq, dummyResp.data(),
        dummyResp.size());
    EXPECT_EQ(requester::ResponseMismatchTracker::instances[5].size(), 10u);

    // Oldest (expectedCmd=0) gone; newest (expectedCmd=10) at back
    EXPECT_EQ(
        requester::ResponseMismatchTracker::instances[5].back().expectedCmd,
        10);
}

TEST_F(ResponseMismatchTrackerTest, MultipleEids_TrackedIndependently)
{
    requester::ResponseMismatchTracker::recordTypeCmdRejected(
        5, 7, 0x00, 0x02, 0x03, 0x10, dummyReq, dummyResp.data(),
        dummyResp.size());
    requester::ResponseMismatchTracker::recordNotFound(
        6, 0, 3, 0x01, 0x08, dummyResp.data(), dummyResp.size());

    EXPECT_EQ(requester::ResponseMismatchTracker::instances[5].size(), 1u);
    EXPECT_EQ(requester::ResponseMismatchTracker::instances[6].size(), 1u);
}

// ===========================================================================
// SendRecvNsmMsg tests
// ===========================================================================

// Minimal fake RequesterHandler for SendRecvNsmMsg template tests
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

TEST(SendRecvNsmMsgTest, AwaitReady_ReturnsFalse)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    EXPECT_FALSE(awaitable.await_ready());
}

TEST(SendRecvNsmMsgTest, AwaitResume_ReturnsInitialRc)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    // Initial rc is NSM_ERROR as set in constructor
    EXPECT_EQ(awaitable.await_resume(), NSM_ERROR);
}

TEST(SendRecvNsmMsgTest,
     AwaitSuspend_NullResponseMsgPtr_ReturnsFalseImmediately)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    size_t respLen = 0;

    // Pass nullptr for responseMsg parameter
    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       nullptr, &respLen);
    bool suspended = awaitable.await_suspend(std::noop_coroutine());
    EXPECT_FALSE(suspended);
    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR_NULL);
}

TEST(SendRecvNsmMsgTest,
     AwaitSuspend_NullResponseLenPtr_ReturnsFalseImmediately)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;

    // Pass nullptr for responseLen parameter
    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, nullptr);
    bool suspended = awaitable.await_suspend(std::noop_coroutine());
    EXPECT_FALSE(suspended);
    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR_NULL);
}

TEST(SendRecvNsmMsgTest, AwaitSuspend_RegisterFails_ReturnsFalse)
{
    FakeHandlerFail fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    // encode a minimal ping request
    encode_ping_req(0, reinterpret_cast<nsm_msg*>(req.data()));
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerFail> awaitable(fakeHandler, 1, req,
                                                         &respMsg, &respLen);
    bool suspended = awaitable.await_suspend(std::noop_coroutine());
    EXPECT_FALSE(suspended);
    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR);
}

TEST(SendRecvNsmMsgTest, AwaitSuspend_RegisterSucceeds_Suspends)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    encode_ping_req(0, reinterpret_cast<nsm_msg*>(req.data()));
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    bool suspended = awaitable.await_suspend(std::noop_coroutine());
    EXPECT_TRUE(suspended);
}

TEST(SendRecvNsmMsgTest, HandleResponse_NullResponse_SetsTimeout)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    awaitable.resumeHandle = std::noop_coroutine();
    awaitable.HandleResponse(1, nullptr, 0);

    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR_TIMEOUT);
}

TEST(SendRecvNsmMsgTest, HandleResponse_ValidResponse_SetsSuccess)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    awaitable.resumeHandle = std::noop_coroutine();

    nsm_msg fakeResponse{};
    awaitable.HandleResponse(1, &fakeResponse, sizeof(nsm_msg_hdr));

    EXPECT_EQ(awaitable.rc, NSM_SW_SUCCESS);
    EXPECT_EQ(respMsg, &fakeResponse);
    EXPECT_EQ(respLen, sizeof(nsm_msg_hdr));
}

TEST(SendRecvNsmMsgTest, HandleResponse_ZeroLength_SetsTimeout)
{
    FakeHandlerOk fakeHandler;
    std::vector<uint8_t> req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    const nsm_msg* respMsg = nullptr;
    size_t respLen = 0;

    requester::SendRecvNsmMsg<FakeHandlerOk> awaitable(fakeHandler, 1, req,
                                                       &respMsg, &respLen);
    awaitable.resumeHandle = std::noop_coroutine();

    nsm_msg fakeResponse{};
    // length = 0 with non-null pointer also means timeout
    awaitable.HandleResponse(1, &fakeResponse, 0);

    EXPECT_EQ(awaitable.rc, NSM_SW_ERROR_TIMEOUT);
}
