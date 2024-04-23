#pragma once

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/State/Chassis/server.hpp>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using PowerStateIntf = object_t<State::server::Chassis>;

class NsmPowerSupplyStatus : public NsmSensor, public NsmInterfaceContainer<PowerStateIntf>
{
  public:
    NsmPowerSupplyStatus(
        std::shared_ptr<NsmInterfaceProvider<PowerStateIntf>> pdi,
        uint8_t gpuInstanceId);
    NsmPowerSupplyStatus() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    uint8_t gpuInstanceId;
};

} // namespace nsm