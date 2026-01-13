#pragma once

#include "libnsm/base.h"
#include "common/types.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace requester::retry
{
/**
 * Backoff Strategy: Constant delay for NOT_READY
 *
 * - Immediate first attempt (no delay).
 * - On cc == NSM_ERR_NOT_READY, retry up to maxRetries with a constant delay:
 *     delay(k) = delayMs, for k in [1..maxRetries]
 * - Stop on first success, on first non-NOT_READY cc, or after maxRetries.
 *
 * Telemetry:
 * - LinearBackoffStats: per-attempt delay and observed codes, total wait.
 * - OperationSummary: op outcome (attempts, success/fail, last cc/reason, total wait).
 */
struct LinearBackoffConfig
{
    uint8_t  maxRetries{static_cast<uint8_t>(RETRY_BACKOFF_MAX_RETRIES)};
    uint32_t delayMs{static_cast<uint32_t>(RETRY_BACKOFF_DELAY_MS)};
};

struct LinearBackoffAttempt
{
    uint8_t      attemptIndex{0};
    uint32_t     delayMs{0};
    uint8_t      completionCode{0};
    uint16_t     reasonCode{0};
    nsm_sw_codes returnCode{NSM_SW_SUCCESS};
};

struct LinearBackoffStats
{
    LinearBackoffConfig               config{};
    std::vector<LinearBackoffAttempt> attempts{};
    uint32_t                          totalWaitMs{0};
};

struct OperationSummary
{
    std::string opName;
    eid_t eid{0};
    uint8_t totalAttempts{0};
    bool success{false};
    uint8_t lastCc{NSM_SUCCESS};
    uint16_t lastReason{ERR_NULL};
    uint32_t totalWaitMs{0};
};

inline void logNotReadyRetry(const char* opName, eid_t eid, uint8_t attempt,
                             uint8_t maxRetries, uint32_t delayMs, uint16_t reason)
{
    lg2::info(
        "{OP} not ready; retrying. eid={EID} attempt={ATTEMPT}/{MAX} delayMs={DELAY} reasonCode={REASON}",
        "OP", opName, "EID", eid, "ATTEMPT", attempt, "MAX", maxRetries,
        "DELAY", delayMs, "REASON", reason);
}

inline std::string formatSummary(const OperationSummary& s)
{
    return s.opName + ", Total attempts=" + std::to_string(s.totalAttempts) +
           ", " + s.opName + "=" + (s.success ? "successful" : "failed after back off");
}

} // namespace requester::retry

