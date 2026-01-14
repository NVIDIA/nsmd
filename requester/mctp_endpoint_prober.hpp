#pragma once
#include "libnsm/base.h"

#include "common/coroutine.hpp"
#include "common/types.hpp"
#include "nsmMsghandler.hpp"
#include "requester/request.hpp"
#include "requester/retry_backoff_utils.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>

namespace requester
{

class MctpEndpointProber
{
  public:
    using SendRecvFn = std::function<requester::Coroutine(
        eid_t, ::Request&, std::shared_ptr<const nsm_msg>&, size_t*)>;

    explicit MctpEndpointProber(std::shared_ptr<nsm::NsmMessageHandler> handler,
                                requester::retry::LinearBackoffConfig cfg = {},
                                SendRecvFn sendRecvFn = nullptr) :
        nsmMsgHandler(std::move(handler)), cfg(cfg),
        sendRecvFn(std::move(sendRecvFn))
    {}

    requester::Coroutine ping(eid_t eid);
    requester::Coroutine
        getQueryDeviceIdentification(eid_t eid, uint8_t& deviceIdentification,
                                     uint8_t& deviceInstance);

    std::optional<requester::retry::OperationSummary>
        getLastPingSummary(eid_t eid) const
    {
        auto it = lastPingSummaryByEid.find(eid);
        if (it == lastPingSummaryByEid.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::optional<requester::retry::OperationSummary>
        getLastQueryDeviceIdentificationSummary(eid_t eid) const
    {
        auto it = lastQuerySummaryByEid.find(eid);
        if (it == lastQuerySummaryByEid.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    void logAllSummaries() const;

  private:
    requester::Coroutine pingOnce(eid_t eid, uint8_t& cc, uint16_t& reason);
    requester::Coroutine queryDeviceIdentificationOnce(eid_t eid, uint8_t& cc,
                                                       uint16_t& reason,
                                                       uint8_t& devId,
                                                       uint8_t& devInst);

    std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler;
    requester::retry::LinearBackoffConfig cfg;
    SendRecvFn sendRecvFn;
    std::map<eid_t, requester::retry::OperationSummary> lastPingSummaryByEid;
    std::map<eid_t, requester::retry::OperationSummary> lastQuerySummaryByEid;
};

} // namespace requester
