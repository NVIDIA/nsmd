#pragma once

#include "libnsm/network-ports.h"

#include "nsmInterface.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <tal.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/NvSwitch/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Switch/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using UuidIntf = object_t<Common::server::UUID>;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using SwitchIntf = object_t<Inventory::Item::server::Switch>;
using NvSwitchIntf = object_t<Inventory::Item::server::NvSwitch>;

template <typename IntfType>
class NsmSwitchDI : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmSwitchDI() = delete;
    NsmSwitchDI(const std::string& name, const std::string& inventoryObjPath) :
        NsmInterfaceProvider<IntfType>(name, "NSM_NVSwitch", inventoryObjPath),
        objPath(inventoryObjPath + name)
    {}

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::string objPath;
};

} // namespace nsm
