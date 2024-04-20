#pragma once

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Dimension/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PowerLimit/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using UuidIntf = object_t<Common::server::UUID>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using DimensionIntf = object_t<Inventory::Decorator::server::Dimension>;
using LocationIntf = object_t<Inventory::Decorator::server::Location>;
using LocationCodeIntf = object_t<Inventory::Decorator::server::LocationCode>;
using PowerLimitIntf = object_t<Inventory::Decorator::server::PowerLimit>;
using ChassisIntf = object_t<Inventory::Item::server::Chassis>;
using PowerStateIntf = object_t<State::server::Chassis>;
using OperationalStatusIntf =
    object_t<State::Decorator::server::OperationalStatus>;
using HealthIntf = object_t<State::Decorator::server::Health>;

template <typename IntfType>
class NsmGpuChassis : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmGpuChassis() = delete;
    NsmGpuChassis(const std::string& name) :
        NsmInterfaceProvider<IntfType>(
            name, "NSM_GPU_Chassis",
            "/xyz/openbmc_project/inventory/system/chassis/")
    {}
};

} // namespace nsm