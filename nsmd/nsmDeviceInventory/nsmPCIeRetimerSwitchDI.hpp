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
#if defined(ENABLE_CLOCK_OUTPUT_STATE)
#include <xyz/openbmc_project/Inventory/Decorator/PCIeRefClock/server.hpp>
#endif
#include <xyz/openbmc_project/Inventory/Item/Switch/server.hpp>

#include <iostream>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
#if defined(ENABLE_CLOCK_OUTPUT_STATE)
using PCIeRefClockIntf = object_t<Inventory::Decorator::server::PCIeRefClock>;
#endif
using SwitchIntf = object_t<Inventory::Item::server::Switch>;

class NsmPCIeRetimerSwitchDI : public NsmObject
{
  public:
    NsmPCIeRetimerSwitchDI(sdbusplus::bus_t& bus, const std::string& name,
                           const std::vector<utils::Association>& associations,
                           const std::string& type,
                           std::string& inventoryObjPath, uint8_t deviceIdx);

    // Overloaded constructor with port variables
    NsmPCIeRetimerSwitchDI(sdbusplus::bus_t& bus, const std::string& name,
                           const std::vector<utils::Association>& associations,
                           const std::string& type,
                           std::string& inventoryObjPath, uint8_t deviceIdx,
                           uint8_t multiPortType, uint8_t multiPortIndex,
                           uint8_t multiPortUpstreamPort);

    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDefIntf = nullptr;
    std::unique_ptr<SwitchIntf> switchIntf = nullptr;
    uint8_t deviceIndex;
    // Port variables
    uint8_t multiPortType = 0;
    uint8_t multiPortIndex = 0;
    uint8_t multiPortUpstreamPort = 0;
    bool isMultiPciePortEnabled = false;
};

#if defined(ENABLE_CLOCK_OUTPUT_STATE)
class NsmPCIeRetimerSwitchGetClockState : public NsmSensor
{
  public:
    NsmPCIeRetimerSwitchGetClockState(sdbusplus::bus_t& bus,
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
#endif
} // namespace nsm
