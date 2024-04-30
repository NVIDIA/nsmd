#pragma once

#include "nsmGpuChassis/nsmInterface.hpp"
#include "globals.hpp"

#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using UuidIntf = object_t<Common::server::UUID>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using LocationIntf = object_t<Inventory::Decorator::server::Location>;
using ChassisIntf = object_t<Inventory::Item::server::Chassis>;
using HealthIntf = object_t<State::Decorator::server::Health>;

template <typename IntfType>
class NsmNVSwitchAndNicChassis : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmNVSwitchAndNicChassis() = delete;
    NsmNVSwitchAndNicChassis(const std::string& name, const std::string& type) :
        NsmInterfaceProvider<IntfType>(
            name, type, std::string(chassisInventoryBasePath) + "/")
    {}
};

} // namespace nsm