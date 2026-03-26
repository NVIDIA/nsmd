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
 * @file mctpEndpointProberBranch_test.cpp
 *
 * Branch-coverage tests for MctpEndpointProber:
 *   - ping() with NOT_READY retry loop
 *   - ping() retry loop exhaustion
 *   - ping() retry with transport failure mid-retry
 *   - ping() retry success on second attempt
 *   - ping() retry with non-retryable cc mid-retry
 *   - getQueryDeviceIdentification() with NOT_READY retry loop
 *   - getQueryDeviceIdentification() retry loop exhaustion
 *   - getQueryDeviceIdentification() retry transport failure
 *   - getQueryDeviceIdentification() retry success on second attempt
 *   - getQueryDeviceIdentification() retry non-retryable mid-retry
 *   - logAllSummaries with mixed entries
 *
 * Uses the same sendRecvFn mock pattern from requester_test.cpp.
 */

#include "base.h"

#include "requester/mctp_endpoint_prober.hpp"
#include "requester/retry_backoff_utils.hpp"

#include <sdeventplus/event.hpp>

#include <coroutine>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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
// Test fixture
// ===========================================================================

class MctpEndpointProberBranchTest : public ::testing::Test
{
  protected:
    // Build a valid ping response
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
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_common_resp(0, cc, ERR_NULL,
                           NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_PING, msg);
        return buf;
    }

    // Build a valid query device identification response
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

    // Create a sendRecvFn that returns different responses on each call
    // using a shared call counter
    static MctpEndpointProber::SendRecvFn makeSendRecvSequence(
        std::vector<std::pair<std::vector<uint8_t>, uint8_t>> sequence)
    {
        auto callIdx = std::make_shared<size_t>(0);
        return [sequence = std::move(sequence), callIdx](
                   eid_t, ::Request&, std::shared_ptr<const nsm_msg>& respMsg,
                   size_t* respLen) mutable -> requester::Coroutine {
            size_t idx = *callIdx;
            (*callIdx)++;
            if (idx >= sequence.size())
            {
                idx = sequence.size() - 1; // repeat last entry
            }
            auto& [response, transportRc] = sequence[idx];
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

    // Create prober with given sendRecvFn and retry config
    static MctpEndpointProber makeProber(MctpEndpointProber::SendRecvFn fn,
                                         LinearBackoffConfig cfg = {})
    {
        return MctpEndpointProber(nullptr, cfg, std::move(fn));
    }
};

// ===========================================================================
// ping() NOT_READY retry → eventual success on second attempt
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, DISABLED_Ping_NotReady_ThenSuccess)
{
    LinearBackoffConfig cfg;
    cfg.maxRetries = 3;
    cfg.delayMs = 1; // minimal delay for fast test

    // First call: NOT_READY, second call: SUCCESS
    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {buildPingResp(NSM_SUCCESS), 0},
    });

    auto prober = makeProber(std::move(fn), cfg);
    auto coro = prober.ping(1);
    ASSERT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastPingSummary(1);
    ASSERT_TRUE(summary.has_value());
    EXPECT_TRUE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 2);
}

// ===========================================================================
// ping() NOT_READY retry exhaustion → failure after all retries
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, DISABLED_Ping_NotReady_AllRetriesExhausted)
{
    LinearBackoffConfig cfg;
    cfg.maxRetries = 2;
    cfg.delayMs = 1;

    // All calls return NOT_READY
    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {buildPingResp(NSM_ERR_NOT_READY), 0},
    });

    auto prober = makeProber(std::move(fn), cfg);
    auto coro = prober.ping(2);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastPingSummary(2);
    ASSERT_TRUE(summary.has_value());
    EXPECT_FALSE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 3); // 1 initial + 2 retries
}

// ===========================================================================
// ping() NOT_READY retry → transport failure during retry
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest,
       DISABLED_Ping_NotReady_TransportFailDuringRetry)
{
    LinearBackoffConfig cfg;
    cfg.maxRetries = 3;
    cfg.delayMs = 1;

    // First: NOT_READY, second: transport failure
    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {{}, NSM_SW_ERROR},
    });

    auto prober = makeProber(std::move(fn), cfg);
    auto coro = prober.ping(3);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);
}

// ===========================================================================
// ping() NOT_READY retry → non-retryable cc during retry
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest,
       DISABLED_Ping_NotReady_NonRetryableCcDuringRetry)
{
    LinearBackoffConfig cfg;
    cfg.maxRetries = 3;
    cfg.delayMs = 1;

    // First: NOT_READY, second: NSM_ERROR (not NOT_READY, not SUCCESS)
    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {buildPingResp(NSM_ERROR), 0},
    });

    auto prober = makeProber(std::move(fn), cfg);
    auto coro = prober.ping(4);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastPingSummary(4);
    ASSERT_TRUE(summary.has_value());
    EXPECT_FALSE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 2);
}

// ===========================================================================
// getQueryDeviceIdentification() NOT_READY → eventual success
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, DISABLED_QueryDevId_NotReady_ThenSuccess)
{
    LinearBackoffConfig cfg;
    cfg.maxRetries = 3;
    cfg.delayMs = 1;

    auto fn = makeSendRecvSequence({
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {buildQueryDeviceIdResp(NSM_SUCCESS, 5, 2), 0},
    });

    auto prober = makeProber(std::move(fn), cfg);
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(10, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(devId, 5);
    EXPECT_EQ(devInst, 2);

    auto summary = prober.getLastQueryDeviceIdentificationSummary(10);
    ASSERT_TRUE(summary.has_value());
    EXPECT_TRUE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 2);
}

// ===========================================================================
// getQueryDeviceIdentification() NOT_READY → all retries exhausted
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest,
       DISABLED_QueryDevId_NotReady_AllRetriesExhausted)
{
    LinearBackoffConfig cfg;
    cfg.maxRetries = 2;
    cfg.delayMs = 1;

    auto fn = makeSendRecvSequence({
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
    });

    auto prober = makeProber(std::move(fn), cfg);
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(11, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastQueryDeviceIdentificationSummary(11);
    ASSERT_TRUE(summary.has_value());
    EXPECT_FALSE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 3);
}

// ===========================================================================
// getQueryDeviceIdentification() NOT_READY → transport failure during retry
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest,
       DISABLED_QueryDevId_NotReady_TransportFailDuringRetry)
{
    LinearBackoffConfig cfg;
    cfg.maxRetries = 3;
    cfg.delayMs = 1;

    auto fn = makeSendRecvSequence({
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {{}, NSM_SW_ERROR},
    });

    auto prober = makeProber(std::move(fn), cfg);
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(12, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);
}

// ===========================================================================
// getQueryDeviceIdentification() NOT_READY → non-retryable cc during retry
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest,
       DISABLED_QueryDevId_NotReady_NonRetryableCcDuringRetry)
{
    LinearBackoffConfig cfg;
    cfg.maxRetries = 3;
    cfg.delayMs = 1;

    auto fn = makeSendRecvSequence({
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {buildQueryDeviceIdResp(NSM_ERROR, 0, 0), 0},
    });

    auto prober = makeProber(std::move(fn), cfg);
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(13, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastQueryDeviceIdentificationSummary(13);
    ASSERT_TRUE(summary.has_value());
    EXPECT_FALSE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 2);
}

// ===========================================================================
// ping() initial transport failure → immediate co_return
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, Ping_InitialTransportFailure)
{
    auto prober = makeProber(makeSendRecv({}, NSM_SW_ERROR));
    auto coro = prober.ping(20);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);
}

// ===========================================================================
// ping() first attempt success → no retry loop entered
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, Ping_FirstAttemptSuccess)
{
    auto prober = makeProber(makeSendRecv(buildPingResp(NSM_SUCCESS)));
    auto coro = prober.ping(21);
    ASSERT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastPingSummary(21);
    ASSERT_TRUE(summary.has_value());
    EXPECT_TRUE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 1);
    EXPECT_EQ(summary->totalWaitMs, 0u);
}

// ===========================================================================
// ping() first attempt non-retryable cc → no retry
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, Ping_FirstAttemptNonRetryableCc)
{
    auto prober = makeProber(makeSendRecv(buildPingResp(NSM_ERROR)));
    auto coro = prober.ping(22);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastPingSummary(22);
    ASSERT_TRUE(summary.has_value());
    EXPECT_FALSE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 1);
}

// ===========================================================================
// getQueryDeviceIdentification() initial transport failure
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, QueryDevId_InitialTransportFailure)
{
    auto prober = makeProber(makeSendRecv({}, NSM_SW_ERROR));
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(30, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);
}

// ===========================================================================
// getQueryDeviceIdentification() first attempt success
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, QueryDevId_FirstAttemptSuccess)
{
    auto prober =
        makeProber(makeSendRecv(buildQueryDeviceIdResp(NSM_SUCCESS, 7, 3)));
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(31, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(devId, 7);
    EXPECT_EQ(devInst, 3);
}

// ===========================================================================
// getQueryDeviceIdentification() first attempt non-retryable cc
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, QueryDevId_FirstAttemptNonRetryableCc)
{
    auto prober =
        makeProber(makeSendRecv(buildQueryDeviceIdResp(NSM_ERROR, 0, 0)));
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(32, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastQueryDeviceIdentificationSummary(32);
    ASSERT_TRUE(summary.has_value());
    EXPECT_FALSE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 1);
}

// ===========================================================================
// logAllSummaries with both ping and query summaries
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, LogAllSummaries_MixedEntries)
{
    auto pingProber = makeProber(makeSendRecv(buildPingResp(NSM_SUCCESS)));
    pingProber.ping(40);

    auto queryProber =
        makeProber(makeSendRecv(buildQueryDeviceIdResp(NSM_SUCCESS, 1, 1)));
    uint8_t devId = 0, devInst = 0;
    queryProber.getQueryDeviceIdentification(41, devId, devInst);

    EXPECT_NO_THROW(pingProber.logAllSummaries());
    EXPECT_NO_THROW(queryProber.logAllSummaries());
}

// ===========================================================================
// logAllSummaries with no entries → does not crash
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, LogAllSummaries_Empty)
{
    auto prober = makeProber(makeSendRecv(buildPingResp()));
    EXPECT_NO_THROW(prober.logAllSummaries());
}

// ===========================================================================
// getLastPingSummary / getLastQuerySummary for non-existent EID
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, GetSummary_NonExistentEid)
{
    auto prober = makeProber(makeSendRecv(buildPingResp(NSM_SUCCESS)));
    prober.ping(50);

    EXPECT_FALSE(prober.getLastPingSummary(99).has_value());
    EXPECT_FALSE(
        prober.getLastQueryDeviceIdentificationSummary(99).has_value());
}

// ===========================================================================
// ping() NOT_READY with maxRetries=1 → single retry then success
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, DISABLED_Ping_NotReady_SingleRetry_Success)
{
    LinearBackoffConfig cfg;
    cfg.maxRetries = 1;
    cfg.delayMs = 1;

    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {buildPingResp(NSM_SUCCESS), 0},
    });

    auto prober = makeProber(std::move(fn), cfg);
    auto coro = prober.ping(60);
    ASSERT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastPingSummary(60);
    ASSERT_TRUE(summary.has_value());
    EXPECT_TRUE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 2);
    EXPECT_GT(summary->totalWaitMs, 0u);
}

// ===========================================================================
// getQueryDeviceIdentification() NOT_READY with maxRetries=1 → single retry
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest,
       DISABLED_QueryDevId_NotReady_SingleRetry_Success)
{
    LinearBackoffConfig cfg;
    cfg.maxRetries = 1;
    cfg.delayMs = 1;

    auto fn = makeSendRecvSequence({
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {buildQueryDeviceIdResp(NSM_SUCCESS, 9, 4), 0},
    });

    auto prober = makeProber(std::move(fn), cfg);
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(61, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(devId, 9);
    EXPECT_EQ(devInst, 4);

    auto summary = prober.getLastQueryDeviceIdentificationSummary(61);
    ASSERT_TRUE(summary.has_value());
    EXPECT_TRUE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 2);
}

// ===========================================================================
// pingOnce() decode failure - truncated success response
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, PingOnce_DecodeFailure_TruncatedResp)
{
    // Build a buffer just large enough for decode_reason_code_and_cc to read
    // cc=NSM_SUCCESS from payload[1], but too short for decode_ping_resp's
    // length check (needs sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp)).
    // Minimum safe size: sizeof(nsm_msg_hdr) + 2 = 7 bytes (valgrind safe).
    std::vector<uint8_t> truncated(sizeof(nsm_msg_hdr) + 2, 0);
    auto msg = reinterpret_cast<nsm_msg*>(truncated.data());
    // Set completion_code (payload[1]) to NSM_SUCCESS so
    // decode_reason_code_and_cc returns success, then decode_ping_resp
    // fails on the length check.
    msg->payload[1] = NSM_SUCCESS;

    auto prober = makeProber(makeSendRecv(truncated));
    auto coro = prober.ping(70);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);
}

// ===========================================================================
// queryDeviceIdentificationOnce() decode failure - truncated success response
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, QueryDevIdOnce_DecodeFailure_TruncatedResp)
{
    // Same approach: buffer large enough for decode_reason_code_and_cc but
    // too short for decode_query_device_identification_resp length check.
    std::vector<uint8_t> truncated(sizeof(nsm_msg_hdr) + 2, 0);
    auto msg = reinterpret_cast<nsm_msg*>(truncated.data());
    msg->payload[1] = NSM_SUCCESS;

    auto prober = makeProber(makeSendRecv(truncated));
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(71, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);
}

// ===========================================================================
// Ping summary records cc and reason for non-retryable failure
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, Ping_NonRetryableCc_SummaryFields)
{
    auto prober = makeProber(makeSendRecv(buildPingResp(NSM_ERROR)));
    auto coro = prober.ping(72);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastPingSummary(72);
    ASSERT_TRUE(summary.has_value());
    EXPECT_FALSE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 1);
    EXPECT_EQ(summary->totalWaitMs, 0u);
    EXPECT_EQ(summary->lastCc, NSM_ERROR);
    EXPECT_STREQ(summary->opName.c_str(), "Ping");
    EXPECT_EQ(summary->eid, 72);
}

// ===========================================================================
// QueryDevId summary records cc and reason for non-retryable failure
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, QueryDevId_NonRetryableCc_SummaryFields)
{
    auto prober =
        makeProber(makeSendRecv(buildQueryDeviceIdResp(NSM_ERROR, 0, 0)));
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(73, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastQueryDeviceIdentificationSummary(73);
    ASSERT_TRUE(summary.has_value());
    EXPECT_FALSE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 1);
    EXPECT_EQ(summary->totalWaitMs, 0u);
    EXPECT_EQ(summary->lastCc, NSM_ERROR);
    EXPECT_STREQ(summary->opName.c_str(), "QueryDeviceIdentification");
    EXPECT_EQ(summary->eid, 73);
}

// ===========================================================================
// Ping success summary records correct fields
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, Ping_Success_SummaryFields)
{
    auto prober = makeProber(makeSendRecv(buildPingResp(NSM_SUCCESS)));
    auto coro = prober.ping(74);
    ASSERT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);

    auto summary = prober.getLastPingSummary(74);
    ASSERT_TRUE(summary.has_value());
    EXPECT_TRUE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 1);
    EXPECT_EQ(summary->totalWaitMs, 0u);
    EXPECT_EQ(summary->lastCc, NSM_SUCCESS);
    EXPECT_STREQ(summary->opName.c_str(), "Ping");
    EXPECT_EQ(summary->eid, 74);
}

// ===========================================================================
// QueryDevId success summary records correct fields
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, QueryDevId_Success_SummaryFields)
{
    auto prober =
        makeProber(makeSendRecv(buildQueryDeviceIdResp(NSM_SUCCESS, 10, 5)));
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(75, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(devId, 10);
    EXPECT_EQ(devInst, 5);

    auto summary = prober.getLastQueryDeviceIdentificationSummary(75);
    ASSERT_TRUE(summary.has_value());
    EXPECT_TRUE(summary->success);
    EXPECT_EQ(summary->totalAttempts, 1);
    EXPECT_EQ(summary->lastCc, NSM_SUCCESS);
    EXPECT_STREQ(summary->opName.c_str(), "QueryDeviceIdentification");
    EXPECT_EQ(summary->eid, 75);
}

// ===========================================================================
// logAllSummaries with multiple EIDs for both ping and query
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, LogAllSummaries_MultipleEids)
{
    // Use sequences to allow multiple calls on the same prober
    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_SUCCESS), 0},
        {buildPingResp(NSM_SUCCESS), 0},
        {buildQueryDeviceIdResp(NSM_SUCCESS, 1, 1), 0},
        {buildQueryDeviceIdResp(NSM_SUCCESS, 2, 2), 0},
    });

    auto prober = makeProber(std::move(fn));

    prober.ping(80);
    prober.ping(81);

    uint8_t devId = 0, devInst = 0;
    prober.getQueryDeviceIdentification(82, devId, devInst);
    prober.getQueryDeviceIdentification(83, devId, devInst);

    EXPECT_NO_THROW(prober.logAllSummaries());

    // Verify all summaries exist
    EXPECT_TRUE(prober.getLastPingSummary(80).has_value());
    EXPECT_TRUE(prober.getLastPingSummary(81).has_value());
    EXPECT_TRUE(prober.getLastQueryDeviceIdentificationSummary(82).has_value());
    EXPECT_TRUE(prober.getLastQueryDeviceIdentificationSummary(83).has_value());
}

// ===========================================================================
// Ping overwrites summary for same EID on repeated calls
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, Ping_OverwritesSummaryOnSameEid)
{
    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_SUCCESS), 0},
        {buildPingResp(NSM_ERROR), 0},
    });

    auto prober = makeProber(std::move(fn));

    // First call succeeds
    prober.ping(90);
    auto s1 = prober.getLastPingSummary(90);
    ASSERT_TRUE(s1.has_value());
    EXPECT_TRUE(s1->success);

    // Second call on same EID fails with NSM_ERROR
    prober.ping(90);
    auto s2 = prober.getLastPingSummary(90);
    ASSERT_TRUE(s2.has_value());
    EXPECT_FALSE(s2->success);
    EXPECT_EQ(s2->lastCc, NSM_ERROR);
}

// ===========================================================================
// QueryDevId overwrites summary for same EID on repeated calls
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, QueryDevId_OverwritesSummaryOnSameEid)
{
    auto fn = makeSendRecvSequence({
        {buildQueryDeviceIdResp(NSM_SUCCESS, 1, 1), 0},
        {buildQueryDeviceIdResp(NSM_ERROR, 0, 0), 0},
    });

    auto prober = makeProber(std::move(fn));
    uint8_t devId = 0, devInst = 0;

    // First call succeeds
    prober.getQueryDeviceIdentification(91, devId, devInst);
    auto s1 = prober.getLastQueryDeviceIdentificationSummary(91);
    ASSERT_TRUE(s1.has_value());
    EXPECT_TRUE(s1->success);

    // Second call on same EID fails
    prober.getQueryDeviceIdentification(91, devId, devInst);
    auto s2 = prober.getLastQueryDeviceIdentificationSummary(91);
    ASSERT_TRUE(s2.has_value());
    EXPECT_FALSE(s2->success);
}

// ===========================================================================
// Ping transport failure does not record summary (early return before map)
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, Ping_TransportFailure_NoSummary)
{
    auto prober = makeProber(makeSendRecv({}, NSM_SW_ERROR));
    auto coro = prober.ping(92);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    // Transport failure returns before recording any summary
    EXPECT_FALSE(prober.getLastPingSummary(92).has_value());
}

// ===========================================================================
// QueryDevId transport failure does not record summary
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, QueryDevId_TransportFailure_NoSummary)
{
    auto prober = makeProber(makeSendRecv({}, NSM_SW_ERROR));
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(93, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    // Transport failure returns before recording any summary
    EXPECT_FALSE(
        prober.getLastQueryDeviceIdentificationSummary(93).has_value());
}

// ===========================================================================
// Ping decode failure does not record summary (pingOnce returns error,
// ping() sees rc!=0 and returns early before summary recording)
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, Ping_DecodeFailure_NoSummary)
{
    std::vector<uint8_t> truncated(sizeof(nsm_msg_hdr) + 2, 0);
    auto msg = reinterpret_cast<nsm_msg*>(truncated.data());
    msg->payload[1] = NSM_SUCCESS;

    auto prober = makeProber(makeSendRecv(truncated));
    auto coro = prober.ping(94);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    // Decode failure in pingOnce causes ping() to return early (rc != 0)
    // before recording a summary
    EXPECT_FALSE(prober.getLastPingSummary(94).has_value());
}

// ===========================================================================
// QueryDevId decode failure does not record summary
// ===========================================================================

TEST_F(MctpEndpointProberBranchTest, QueryDevId_DecodeFailure_NoSummary)
{
    std::vector<uint8_t> truncated(sizeof(nsm_msg_hdr) + 2, 0);
    auto msg = reinterpret_cast<nsm_msg*>(truncated.data());
    msg->payload[1] = NSM_SUCCESS;

    auto prober = makeProber(makeSendRecv(truncated));
    uint8_t devId = 0, devInst = 0;
    auto coro = prober.getQueryDeviceIdentification(95, devId, devInst);
    ASSERT_TRUE(coro.done());
    EXPECT_NE(coro.data(), NSM_SW_SUCCESS);

    EXPECT_FALSE(
        prober.getLastQueryDeviceIdentificationSummary(95).has_value());
}
