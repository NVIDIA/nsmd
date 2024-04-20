#pragma once
#include "pci-links.h"
#include "platform-environmental.h"

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PCIeRefClock/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Switch/server.hpp>

#include <iostream>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using PCIeRefClockIntf = object_t<Inventory::Decorator::server::PCIeRefClock>;
using SwitchIntf = object_t<Inventory::Item::server::Switch>;

class NsmPCIeRetimerSwitchDI : public NsmObject
{
  public:
    NsmPCIeRetimerSwitchDI(sdbusplus::bus::bus& bus, const std::string& name,
                           const std::vector<utils::Association>& associations,
                           const std::string& type,
                           std::string& inventoryObjPath);
    NsmPCIeRetimerSwitchDI() = default;

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDefIntf = nullptr;
    std::unique_ptr<SwitchIntf> switchIntf = nullptr;
};

class NsmPCIeRetimerSwitchGetClockState : public NsmSensor
{
  public:
    NsmPCIeRetimerSwitchGetClockState(sdbusplus::bus::bus& bus,
                                      const std::string& name,
                                      const std::string& type,
                                      const uint64_t& deviceInstance,
                                      std::string& inventoryObjPath);
    NsmPCIeRetimerSwitchGetClockState() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    bool getRetimerClockState(uint32_t clockBuffer);

    std::unique_ptr<PCIeRefClockIntf> pcieRefClockIntf = nullptr;
    uint8_t clkBufIndex;
    uint8_t deviceInstanceNumber;
};
} // namespace nsm