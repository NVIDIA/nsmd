#pragma once

#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/NetworkInterface/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using PCIeDeviceIntf = object_t<Inventory::Item::server::PCIeDevice>;
using NetworkInterfaceIntf =
    object_t<Inventory::Item::server::NetworkInterface>;

class NsmNetworkAdapterDI : public NsmObject
{
  public:
    NsmNetworkAdapterDI(sdbusplus::bus::bus& bus, const std::string& name,
                        const std::vector<utils::Association>& associations,
                        const std::string& type,
                        const std::string& inventoryObjPath);

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDefIntf = nullptr;
    std::unique_ptr<PCIeDeviceIntf> pcieDeviceIntf = nullptr;
    std::unique_ptr<NetworkInterfaceIntf> networkInterfaceIntf = nullptr;
};
} // namespace nsm
