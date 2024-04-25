#include "nsmPCIeLTSSMState.hpp"

#include "libnsm/pci-links.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmPCIeLTSSMState::NsmPCIeLTSSMState(
    const NsmInterfaceProvider<LTSSMStateIntf>& provider, uint8_t deviceId) :
    NsmSensor(provider), NsmInterfaceContainer(provider), deviceId(deviceId)
{}

std::optional<Request> NsmPCIeLTSSMState::genRequestMsg(eid_t eid,
                                                        uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, deviceId,
                                                         6, requestPtr);
    if (rc)
    {
        lg2::error(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPCIeLTSSMState::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_6 data = {};
    uint16_t size = 0;

    auto rc = decode_query_scalar_group_telemetry_v1_group6_resp(
        responseMsg, responseLen, &cc, &size, &reasonCode, &data);
    if (rc)
    {
        lg2::error(
            "responseHandler: decode_query_scalar_group_telemetry_v1_group6_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        return rc;
    }

    if (cc == NSM_SUCCESS)
    {
        pdi().ltssmState(LTSSMStateIntf::State(data.ltssm_state));
    }
    else
    {
        pdi().ltssmState(LTSSMStateIntf::State::NA);
        lg2::error(
            "responseHandler: decode_query_scalar_group_telemetry_v1_group6_resp is not success CC. rc={RC}",
            "RC", rc);
        return rc;
    }

    return cc;
}

} // namespace nsm