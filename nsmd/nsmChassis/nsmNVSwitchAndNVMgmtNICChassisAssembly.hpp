#pragma once

#include "nsmGpuChassis/nsmInterface.hpp"
#include "globals.hpp"

#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Assembly/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssemblyIntf = object_t<Inventory::Item::server::Assembly>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using LocationIntf = object_t<Inventory::Decorator::server::Location>;
using HealthIntf = object_t<State::Decorator::server::Health>;

template <typename IntfType>
class NsmNVSwitchAndNicChassisAssembly : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmNVSwitchAndNicChassisAssembly() = delete;
    NsmNVSwitchAndNicChassisAssembly(const std::string& chassisName,
                                     const std::string& name,
                                     const std::string& type) :
        NsmInterfaceProvider<IntfType>(name, type,
                                       std::string(chassisInventoryBasePath) +
                                           "/" + chassisName + "/")
    {}
};

} // namespace nsm