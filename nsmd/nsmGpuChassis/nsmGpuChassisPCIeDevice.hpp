#pragma once

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>
#include <xyz/openbmc_project/PCIe/LTSSMState/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using UuidIntf = object_t<Common::server::UUID>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using OperationalStatusIntf =
    object_t<State::Decorator::server::OperationalStatus>;
using HealthIntf = object_t<State::Decorator::server::Health>;
using PCIeDeviceIntf = object_t<Inventory::Item::server::PCIeDevice>;
using LTSSMStateIntf = object_t<PCIe::server::LTSSMState>;

template <typename IntfType>
class NsmGpuChassisPCIeDevice : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmGpuChassisPCIeDevice() = delete;
    NsmGpuChassisPCIeDevice(const std::string& chassisName,
                            const std::string& name) :
        NsmInterfaceProvider<IntfType>(
            name, "NSM_GPU_ChassisPCIeDevice",
            "/xyz/openbmc_project/inventory/system/chassis/" + chassisName +
                "/PCIeDevices/")
    {}
};

} // namespace nsm