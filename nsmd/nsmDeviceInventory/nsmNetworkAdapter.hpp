#pragma once

#include "libnsm/diagnostics.h"

#include "nsmDbusIfaceOverride/nsmResetIface.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Control/Reset/server.hpp>
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
using ResetDeviceIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::Reset>;

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

class NsmNetworkAdapterDIReset : public NsmObject
{
  public:
    NsmNetworkAdapterDIReset(sdbusplus::bus::bus& bus, const std::string& name,
                             const std::string& type,
                             std::string& inventoryObjPath,
                             std::shared_ptr<NsmDevice> device);

  private:
    std::shared_ptr<ResetDeviceIntf> resetIntf = nullptr;
    std::shared_ptr<NsmNetworkDeviceResetAsyncIntf> resetAsyncIntf = nullptr;
    std::string objPath;
};
} // namespace nsm
