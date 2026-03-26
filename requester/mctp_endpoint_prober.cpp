#include "config.h"

#include "requester/mctp_endpoint_prober.hpp"

#include "common/event.hpp"
#include "common/sleep.hpp"
#include "common/utils.hpp"
#include "requester/retry_backoff_utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace requester
{

requester::Coroutine MctpEndpointProber::pingOnce(eid_t eid, uint8_t& cc,
                                                  uint16_t& reason)
{
    ::Request req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto msg = reinterpret_cast<nsm_msg*>(req.data());

    auto rc = encode_ping_req(DEFAULT_INSTANCE_ID, msg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("ping encode failed. eid={EID} rc={RC}", "EID", eid, "RC",
                   utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> respMsg;
    size_t respLen = 0;
    rc = co_await sendRecvFn(eid, req, respMsg, &respLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    cc = NSM_SUCCESS;
    reason = ERR_NULL;
    rc = decode_ping_resp(respMsg.get(), respLen, &cc, &reason);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "ping decode failed. eid={EID} cc={CC} reasonCode={REASONCODE} and rc={RC}",
            "EID", eid, "CC", utils::nsmCompletionCodeToString(cc),
            "REASONCODE", utils::nsmReasonCodeToString(reason), "RC",
            utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine MctpEndpointProber::queryDeviceIdentificationOnce(
    eid_t eid, uint8_t& cc, uint16_t& reason, uint8_t& devId, uint8_t& devInst)
{
    ::Request request(sizeof(nsm_msg_hdr) +
                      sizeof(nsm_query_device_identification_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_nsm_query_device_identification_req(DEFAULT_INSTANCE_ID,
                                                         requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_nsm_query_device_identification_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await sendRecvFn(eid, request, responseMsg, &responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    cc = NSM_SUCCESS;
    reason = ERR_NULL;
    rc = decode_query_device_identification_resp(
        responseMsg.get(), responseLen, &cc, &reason, &devId, &devInst);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_query_device_identification_resp failed. eid={EID} cc={CC} reasonCode={REASONCODE} rc={RC}",
            "EID", eid, "CC", utils::nsmCompletionCodeToString(cc),
            "REASONCODE", utils::nsmReasonCodeToString(reason), "RC",
            utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine MctpEndpointProber::ping(eid_t eid)
{
    using requester::retry::formatSummary;
    using requester::retry::LinearBackoffAttempt;
    using requester::retry::LinearBackoffStats;
    using requester::retry::logNotReadyRetry;

    uint8_t cc{};
    uint16_t reason{};
    auto rc = co_await pingOnce(eid, cc, reason);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }
    if (cc == NSM_SUCCESS)
    {
        lastPingSummaryByEid[eid] = {"Ping", eid, 1, true, cc, reason, 0};
        lg2::info("{MSG}", "MSG", formatSummary(lastPingSummaryByEid[eid]));
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    if (cc != NSM_ERR_NOT_READY)
    {
        lastPingSummaryByEid[eid] = {"Ping", eid, 1, false, cc, reason, 0};
        lg2::info("{MSG}", "MSG", formatSummary(lastPingSummaryByEid[eid]));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    LinearBackoffStats stats{};
    stats.config = cfg;

    common::Event event;
    const uint32_t delayMs = cfg.delayMs;

    for (uint8_t attempt = 1; attempt <= cfg.maxRetries; ++attempt)
    {
        stats.attempts.push_back(
            LinearBackoffAttempt{attempt, delayMs, cc, reason, NSM_SW_SUCCESS});
        stats.totalWaitMs += delayMs;

        logNotReadyRetry("ping", eid, attempt, cfg.maxRetries, delayMs, reason);
        co_await common::Sleep(event, static_cast<uint64_t>(delayMs) * 1000,
                               common::NonPriority);

        cc = 0;
        reason = 0;
        rc = co_await pingOnce(eid, cc, reason);
        if (rc)
        {
            lg2::error(
                "ping retry send/recv failed. eid={EID} rc={RC} attempt={ATTEMPT}",
                "EID", eid, "RC", utils::nsmSwCodeToString(rc), "ATTEMPT",
                attempt);
            // coverity[missing_return]
            co_return rc;
        }
        if (cc == NSM_SUCCESS)
        {
            const uint8_t totalAttempts = static_cast<uint8_t>(1 + attempt);
            lastPingSummaryByEid[eid] = {"Ping", eid,    totalAttempts,    true,
                                         cc,     reason, stats.totalWaitMs};
            lg2::info("{MSG}", "MSG", formatSummary(lastPingSummaryByEid[eid]));
            // coverity[missing_return]
            co_return NSM_SW_SUCCESS;
        }
        if (cc != NSM_ERR_NOT_READY)
        {
            const uint8_t totalAttempts = static_cast<uint8_t>(1 + attempt);
            lastPingSummaryByEid[eid] = {
                "Ping", eid,    totalAttempts,    false,
                cc,     reason, stats.totalWaitMs};
            lg2::info("{MSG}", "MSG", formatSummary(lastPingSummaryByEid[eid]));
            // coverity[missing_return]
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
    }

    const uint8_t totalAttempts = static_cast<uint8_t>(1 + cfg.maxRetries);
    lastPingSummaryByEid[eid] = {"Ping", eid,    totalAttempts,    false,
                                 cc,     reason, stats.totalWaitMs};
    lg2::info("{MSG}", "MSG", formatSummary(lastPingSummaryByEid[eid]));
    // coverity[missing_return]
    co_return NSM_SW_ERROR_COMMAND_FAIL;
}

requester::Coroutine MctpEndpointProber::getQueryDeviceIdentification(
    eid_t eid, uint8_t& deviceIdentification, uint8_t& deviceInstance)
{
    using requester::retry::formatSummary;
    using requester::retry::LinearBackoffAttempt;
    using requester::retry::LinearBackoffStats;
    using requester::retry::logNotReadyRetry;

    uint8_t cc{};
    uint16_t reason{};
    uint8_t devId{};
    uint8_t devInst{};
    auto rc = co_await queryDeviceIdentificationOnce(eid, cc, reason, devId,
                                                     devInst);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }
    if (cc == NSM_SUCCESS)
    {
        deviceIdentification = devId;
        deviceInstance = devInst;
        lastQuerySummaryByEid[eid] = {
            "QueryDeviceIdentification", eid, 1, true, cc, reason, 0};
        lg2::info("{MSG}", "MSG", formatSummary(lastQuerySummaryByEid[eid]));
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    if (cc != NSM_ERR_NOT_READY)
    {
        lastQuerySummaryByEid[eid] = {
            "QueryDeviceIdentification", eid, 1, false, cc, reason, 0};
        lg2::info("{MSG}", "MSG", formatSummary(lastQuerySummaryByEid[eid]));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    LinearBackoffStats stats{};
    stats.config = cfg;

    common::Event event;
    const uint32_t delayMs = cfg.delayMs;

    for (uint8_t attempt = 1; attempt <= cfg.maxRetries; ++attempt)
    {
        stats.attempts.push_back(
            LinearBackoffAttempt{attempt, delayMs, cc, reason, NSM_SW_SUCCESS});
        stats.totalWaitMs += delayMs;

        logNotReadyRetry("queryDeviceIdentification", eid, attempt,
                         cfg.maxRetries, delayMs, reason);
        co_await common::Sleep(event, static_cast<uint64_t>(delayMs) * 1000,
                               common::NonPriority);

        cc = 0;
        reason = 0;
        devId = 0;
        devInst = 0;
        rc = co_await queryDeviceIdentificationOnce(eid, cc, reason, devId,
                                                    devInst);
        if (rc)
        {
            lg2::error(
                "queryDeviceIdentification retry send/recv failed. eid={EID} rc={RC} attempt={ATTEMPT}",
                "EID", eid, "RC", rc, "ATTEMPT", attempt);
            // coverity[missing_return]
            co_return rc;
        }
        if (cc == NSM_SUCCESS)
        {
            deviceIdentification = devId;
            deviceInstance = devInst;
            const uint8_t totalAttempts = static_cast<uint8_t>(1 + attempt);
            lastQuerySummaryByEid[eid] = {"QueryDeviceIdentification",
                                          eid,
                                          totalAttempts,
                                          true,
                                          cc,
                                          reason,
                                          stats.totalWaitMs};
            lg2::info("{MSG}", "MSG",
                      formatSummary(lastQuerySummaryByEid[eid]));
            // coverity[missing_return]
            co_return NSM_SW_SUCCESS;
        }
        if (cc != NSM_ERR_NOT_READY)
        {
            const uint8_t totalAttempts = static_cast<uint8_t>(1 + attempt);
            lastQuerySummaryByEid[eid] = {"QueryDeviceIdentification",
                                          eid,
                                          totalAttempts,
                                          false,
                                          cc,
                                          reason,
                                          stats.totalWaitMs};
            lg2::info("{MSG}", "MSG",
                      formatSummary(lastQuerySummaryByEid[eid]));
            // coverity[missing_return]
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
    }

    const uint8_t totalAttempts = static_cast<uint8_t>(1 + cfg.maxRetries);
    lastQuerySummaryByEid[eid] = {"QueryDeviceIdentification",
                                  eid,
                                  totalAttempts,
                                  false,
                                  cc,
                                  reason,
                                  stats.totalWaitMs};
    lg2::info("{MSG}", "MSG", formatSummary(lastQuerySummaryByEid[eid]));
    // coverity[missing_return]
    co_return NSM_SW_ERROR_COMMAND_FAIL;
}

void MctpEndpointProber::logAllSummaries() const
{
    using requester::retry::formatSummary;

    lg2::info("MctpEndpointProber last operation summaries:");

    for (const auto& [eid, summary] : lastPingSummaryByEid)
    {
        lg2::info("{MSG}", "MSG", formatSummary(summary));
    }
    for (const auto& [eid, summary] : lastQuerySummaryByEid)
    {
        lg2::info("{MSG}", "MSG", formatSummary(summary));
    }
}

} // namespace requester
