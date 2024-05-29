#pragma once

#include "libnsm/network-ports.h"

#include "common/types.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmProcessor/nsmProcessor.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortInfo/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortWidth/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem1/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>

namespace nsm
{
using AssociationDefIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::association::Definitions>;
using PortInfoIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::PortInfo>;
using PortWidthIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::PortWidth>;
using PortIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::item::Port>;
using PCIeEccIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeECC>;

using PortType = sdbusplus::server::xyz::openbmc_project::inventory::decorator::
    PortInfo::PortType;
using PortProtocol = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortInfo::PortProtocol;

class NsmPort : public NsmObject
{
  public:
    NsmPort(sdbusplus::bus::bus& bus, std::string& portName,
            const std::string& type,
            const std::vector<utils::Association>& associations,
            const std::string& inventoryObjPath);

    std::string portName;

  private:
    std::unique_ptr<PortIntf> portIntf = nullptr;
    std::unique_ptr<AssociationDefIntf> associationDefIntf = nullptr;
};

class NsmPCIeECCGroup1 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup1(const std::string& name, const std::string& type,
                     std::shared_ptr<PortInfoIntf> portInfoIntf,
                     std::shared_ptr<PortWidthIntf> portWidthIntf,
                     uint8_t deviceIndex);
    NsmPCIeECCGroup1() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    double convertEncodedSpeedToGbps(const uint32_t& speed);
    std::shared_ptr<PortInfoIntf> portInfoIntf = nullptr;
    std::shared_ptr<PortWidthIntf> portWidthIntf = nullptr;
};

class NsmPCIeECCGroup2 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup2(const std::string& name, const std::string& type,
                     std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                     uint8_t deviceIndex);
    NsmPCIeECCGroup2() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::shared_ptr<PCIeEccIntf> pcieEccIntf = nullptr;
};

class NsmPCIeECCGroup3 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup3(const std::string& name, const std::string& type,
                     std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                     uint8_t deviceIndex);
    NsmPCIeECCGroup3() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::shared_ptr<PCIeEccIntf> pcieEccIntf = nullptr;
};

class NsmPCIeECCGroup4 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup4(const std::string& name, const std::string& type,
                     std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                     uint8_t deviceIndex);
    NsmPCIeECCGroup4() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::shared_ptr<PCIeEccIntf> pcieEccIntf = nullptr;
};

} // namespace nsm
