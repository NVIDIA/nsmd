#include "nsmRetimerPort.hpp"

#include <xyz/openbmc_project/Inventory/Decorator/PortInfo/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortState/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem1/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

namespace nsm
{
using AssociationDefIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::association::Definitions>;
using PortInfoIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::PortInfo>;
using PortStateIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::PortState>;
using PortWidthIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::PortWidth>;
using PortIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::item::Port>;
using PCIeEccIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeECC>;
using ChasisStateIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::state::Chassis>;
using HealthIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::state::decorator::Health>;

using PortType = sdbusplus::server::xyz::openbmc_project::inventory::decorator::
    PortInfo::PortType;
using PortProtocol = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortInfo::PortProtocol;

class NsmGpuPciePort : public NsmObject
{
  public:
    NsmGpuPciePort(sdbusplus::bus::bus& bus, const std::string& name,
                const std::string& type, const std::string& health, const std::string& chasisState,
                const std::vector<utils::Association>& associations,
                const std::string& inventoryObjPath);

  private:
    std::unique_ptr<AssociationDefIntf> associationDefIntf = nullptr;
    std::unique_ptr<ChasisStateIntf> chasisStateIntf = nullptr;
    std::unique_ptr<HealthIntf> healthIntf = nullptr;
};

class NsmGpuPciePortInfo : public NsmObject
{
  public:
    NsmGpuPciePortInfo(const std::string& name, const std::string& type,
                    const std::string& portType,
                    const std::string& portProtocol,
                    std::shared_ptr<PortInfoIntf> portInfoIntf);

  private:
    std::shared_ptr<PortInfoIntf> portInfoIntf;
};

} // namespace nsm

