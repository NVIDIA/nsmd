#pragma once

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using PCIeDeviceIntf = object_t<Inventory::Item::server::PCIeDevice>;
class NsmPCIeDevice :
    public NsmSensor,
    public NsmInterfaceContainer<PCIeDeviceIntf>
{
  public:
    NsmPCIeDevice(const NsmInterfaceProvider<PCIeDeviceIntf>& provider,
                  uint8_t deviceId);
    NsmPCIeDevice() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    const uint8_t deviceId;
};

} // namespace nsm