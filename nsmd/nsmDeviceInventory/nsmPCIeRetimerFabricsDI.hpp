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
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Fabric/server.hpp>

#include <iostream>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using UuidIntf = object_t<Common::server::UUID>;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using FabricIntf = object_t<Inventory::Item::server::Fabric>;

class NsmPCIeRetimerFabricDI : public NsmObject
{
  public:
    NsmPCIeRetimerFabricDI(sdbusplus::bus::bus& bus, const std::string& name,
                           const std::vector<utils::Association>& associations,
                           const std::string& type,
                           const std::string& fabricsType);

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDefIntf = nullptr;
    std::unique_ptr<UuidIntf> uuidIntf = nullptr;
    std::unique_ptr<FabricIntf> fabricIntf = nullptr;
};
} // namespace nsm
