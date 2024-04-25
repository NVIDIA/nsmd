#pragma once

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/PCIe/LTSSMState/server.hpp>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using LTSSMStateIntf = object_t<PCIe::server::LTSSMState>;

class NsmPCIeLTSSMState :
    public NsmSensor,
    public NsmInterfaceContainer<LTSSMStateIntf>
{
  public:
    NsmPCIeLTSSMState(const NsmInterfaceProvider<LTSSMStateIntf>& provider,
                      uint8_t deviceId);
    NsmPCIeLTSSMState() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    uint8_t deviceId;
};

} // namespace nsm