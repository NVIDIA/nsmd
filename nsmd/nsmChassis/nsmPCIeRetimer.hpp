#pragma once
#include "platform-environmental.h"

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Chassis/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <iostream>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using LocationIntf = object_t<Inventory::Decorator::server::Location>;
using ChassisIntf = object_t<Inventory::Item::server::Chassis>;

class NsmPCIeRetimerChassis : public NsmObject
{
  public:
    NsmPCIeRetimerChassis(sdbusplus::bus::bus& bus, const std::string& name,
                          const std::vector<utils::Association>& associations,
                          const std::string& type);

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDef_ = nullptr;
    std::unique_ptr<AssetIntf> asset_ = nullptr;
    std::unique_ptr<LocationIntf> location_ = nullptr;
    std::unique_ptr<ChassisIntf> chassis_ = nullptr;
};
} // namespace nsm
