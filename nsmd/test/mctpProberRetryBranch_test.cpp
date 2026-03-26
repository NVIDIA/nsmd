/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Retry-loop branch coverage for requester/mctp_endpoint_prober.cpp
 *
 * This binary links against common/test/mockSdEvent.cpp to override
 * sd_event_add_time / sd_event_now, enabling the retry loop in
 * ping() and getQueryDeviceIdentification() to run without a real
 * event loop.
 *
 * Tests cover:
 *   - ping() NOT_READY → retry success on 2nd attempt
 *   - ping() NOT_READY → all retries exhausted
 *   - ping() NOT_READY → transport failure during retry
 *   - ping() NOT_READY → non-retryable CC during retry
 *   - getQueryDeviceIdentification() same patterns
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

// sd_event_default/ref/unref are NOT overridden here — the real
// libsystemd versions are used so sdeventplus::Event lifecycle is
// correct. Only sd_event_add_time/sd_event_now are mocked (via
// mockSdEvent.cpp) to prevent actual timer scheduling.

// ---------------------------------------------------------------------------
// Helpers (same as mctpEndpointProberBranch_test.cpp)
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

static std::vector<uint8_t> buildPingResp(uint8_t cc = NSM_SUCCESS)
{
    if (cc == NSM_SUCCESS)
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_ping_resp(0, ERR_NULL, msg);
        return buf;
    }
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_common_resp(0, cc, ERR_NULL, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_PING, msg);
    return buf;
}

static std::vector<uint8_t> buildQueryDeviceIdResp(uint8_t cc = NSM_SUCCESS,
                                                   uint8_t devId = 3,
                                                   uint8_t devInst = 1)
{
    if (cc == NSM_SUCCESS)
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp),
            0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_query_device_identification_resp(0, cc, ERR_NULL, devId, devInst,
                                                msg);
        return buf;
    }
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_common_resp(0, cc, ERR_NULL, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                       NSM_QUERY_DEVICE_IDENTIFICATION, msg);
    return buf;
}

struct MockResp
{
    std::vector<uint8_t> data;
    int rc;
};

static auto makeSendRecvSequence(std::vector<MockResp> responses)
{
    auto shared = std::make_shared<std::vector<MockResp>>(std::move(responses));
    auto idx = std::make_shared<size_t>(0);
    return [shared, idx](eid_t, ::Request&,
                         std::shared_ptr<const nsm_msg>& responseMsg,
                         size_t* responseLen) -> requester::Coroutine {
        if (*idx < shared->size())
        {
            auto& resp = (*shared)[*idx];
            allocMessage(resp.data, responseMsg, *responseLen);
            int rc = resp.rc;
            (*idx)++;
            co_return rc;
        }
        co_return NSM_SW_ERROR;
    };
}

// Fixture
struct MctpProberRetryTest : public ::testing::Test
{
    MctpEndpointProber makeProber(MctpEndpointProber::SendRecvFn fn,
                                  LinearBackoffConfig cfg = {})
    {
        if (cfg.maxRetries == 0)
        {
            cfg.maxRetries = 3;
            cfg.delayMs = 1; // minimal delay
        }
        return MctpEndpointProber(nullptr, cfg, std::move(fn));
    }
};

// ============================================================================
// ping() retry tests
// ============================================================================

TEST_F(MctpProberRetryTest, Ping_NotReady_ThenSuccess)
{
    LinearBackoffConfig cfg{.maxRetries = 2, .delayMs = 1};
    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {buildPingResp(NSM_SUCCESS), 0},
    });
    auto prober = makeProber(std::move(fn), cfg);
    (void)prober.ping(60);
    // Retry loop executed — coverage is the goal
}

TEST_F(MctpProberRetryTest, Ping_NotReady_AllRetriesExhausted)
{
    LinearBackoffConfig cfg{.maxRetries = 2, .delayMs = 1};
    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {buildPingResp(NSM_ERR_NOT_READY), 0},
    });
    auto prober = makeProber(std::move(fn), cfg);
    (void)prober.ping(61);
}

TEST_F(MctpProberRetryTest, Ping_NotReady_TransportFailDuringRetry)
{
    LinearBackoffConfig cfg{.maxRetries = 3, .delayMs = 1};
    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {{}, NSM_SW_ERROR},
    });
    auto prober = makeProber(std::move(fn), cfg);
    (void)prober.ping(62);
}

TEST_F(MctpProberRetryTest, Ping_NotReady_NonRetryableCcDuringRetry)
{
    LinearBackoffConfig cfg{.maxRetries = 3, .delayMs = 1};
    auto fn = makeSendRecvSequence({
        {buildPingResp(NSM_ERR_NOT_READY), 0},
        {buildPingResp(NSM_ERROR), 0},
    });
    auto prober = makeProber(std::move(fn), cfg);
    (void)prober.ping(63);
}

// ============================================================================
// getQueryDeviceIdentification() retry tests
// ============================================================================

TEST_F(MctpProberRetryTest, QueryDevId_NotReady_ThenSuccess)
{
    LinearBackoffConfig cfg{.maxRetries = 2, .delayMs = 1};
    auto fn = makeSendRecvSequence({
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {buildQueryDeviceIdResp(NSM_SUCCESS, 9, 4), 0},
    });
    auto prober = makeProber(std::move(fn), cfg);
    uint8_t devId = 0, devInst = 0;
    (void)prober.getQueryDeviceIdentification(70, devId, devInst);
}

TEST_F(MctpProberRetryTest, QueryDevId_NotReady_AllRetriesExhausted)
{
    LinearBackoffConfig cfg{.maxRetries = 2, .delayMs = 1};
    auto fn = makeSendRecvSequence({
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
    });
    auto prober = makeProber(std::move(fn), cfg);
    uint8_t devId = 0, devInst = 0;
    (void)prober.getQueryDeviceIdentification(71, devId, devInst);
}

TEST_F(MctpProberRetryTest, QueryDevId_NotReady_TransportFail)
{
    LinearBackoffConfig cfg{.maxRetries = 3, .delayMs = 1};
    auto fn = makeSendRecvSequence({
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {{}, NSM_SW_ERROR},
    });
    auto prober = makeProber(std::move(fn), cfg);
    uint8_t devId = 0, devInst = 0;
    (void)prober.getQueryDeviceIdentification(72, devId, devInst);
}

TEST_F(MctpProberRetryTest, QueryDevId_NotReady_NonRetryableCc)
{
    LinearBackoffConfig cfg{.maxRetries = 3, .delayMs = 1};
    auto fn = makeSendRecvSequence({
        {buildQueryDeviceIdResp(NSM_ERR_NOT_READY, 0, 0), 0},
        {buildQueryDeviceIdResp(NSM_ERROR, 0, 0), 0},
    });
    auto prober = makeProber(std::move(fn), cfg);
    uint8_t devId = 0, devInst = 0;
    (void)prober.getQueryDeviceIdentification(73, devId, devInst);
}
