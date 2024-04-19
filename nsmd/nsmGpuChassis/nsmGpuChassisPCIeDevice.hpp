#pragma once

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using PCIeDeviceIntf = object_t<Inventory::Item::server::PCIeDevice>;

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