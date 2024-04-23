#pragma once

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using OperationalStatusIntf =
    object_t<State::Decorator::server::OperationalStatus>;

class NsmGpuPresenceAndPowerStatus :
    public NsmSensor,
    public NsmInterfaceContainer<OperationalStatusIntf>
{
  public:
    NsmGpuPresenceAndPowerStatus(
        std::shared_ptr<NsmInterfaceProvider<OperationalStatusIntf>> pdi,
        uint8_t gpuInstanceId);
    NsmGpuPresenceAndPowerStatus() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    uint8_t gpuInstanceId;
};

} // namespace nsm