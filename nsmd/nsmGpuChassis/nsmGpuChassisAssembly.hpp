#pragma once

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Inventory/Decorator/Area/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Assembly/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using AreaIntf = object_t<Inventory::Decorator::server::Area>;
using AssemblyIntf = object_t<Inventory::Item::server::Assembly>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using LocationIntf = object_t<Inventory::Decorator::server::Location>;
using HealthIntf = object_t<State::Decorator::server::Health>;

template <typename IntfType>
class NsmGpuChassisAssembly : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmGpuChassisAssembly() = delete;
    NsmGpuChassisAssembly(const std::string& chassisName,
                          const std::string& name) :
        NsmInterfaceProvider<IntfType>(
            name, "NSM_GPU_ChassisAssembly",
            "/xyz/openbmc_project/inventory/system/chassis/" + chassisName +
                "/")
    {}
};

} // namespace nsm