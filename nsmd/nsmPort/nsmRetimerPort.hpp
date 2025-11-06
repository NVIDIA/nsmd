#pragma once

#include "libnsm/network-ports.h"
#include "libnsm/pci-links.h"

#include "common/types.hpp"
#include "nsmCommon/nsmPcieGroup.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#ifdef NVIDIA_SHMEM
#include <telemetry_mrd_producer.hpp>
#endif
#include <com/nvidia/PCIe/AERErrorStatus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortInfo/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortWidth/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/Metrics/LanError/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem1/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem2/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeTransactionCounter/server.hpp>

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
using PortMetricsOem2Intf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::PortMetricsOem2>;
using AERErrorStatusIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::pc_ie::AERErrorStatus>;
using PCIeEccIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeECC>;
using PCIeTransactionCounterIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeTransactionCounter>;

using LaneErrorIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::LanError>;

using PortType = sdbusplus::server::xyz::openbmc_project::inventory::decorator::
    PortInfo::PortType;
using PortProtocol = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortInfo::PortProtocol;

class NsmRetimerAERErrorStatusIntf : public AERErrorStatusIntf
{
  public:
    NsmRetimerAERErrorStatusIntf(sdbusplus::bus::bus& bus, const char* path) :
        AERErrorStatusIntf(bus, path)
    {}
    sdbusplus::message::object_path clearAERStatus() override
    {
        // Empty implementation - no action taken
        return sdbusplus::message::object_path();
    }
};

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
                     const std::string& inventoryPath,
                     std::shared_ptr<PortInfoIntf> portInfoIntf,
                     std::shared_ptr<PortWidthIntf> portWidthIntf,
                     uint8_t deviceIndex);
    NsmPCIeECCGroup1(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PortInfoIntf> portInfoIntf,
                     std::shared_ptr<PortWidthIntf> portWidthIntf,
                     uint8_t multiPortType, uint8_t multiPortIndex,
                     uint8_t multiPortUpstreamPortNumber);
    NsmPCIeECCGroup1() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    std::string objPath;
    double convertEncodedSpeedToGbps(const uint32_t& speed);
    size_t convertEncodedWidthToActualWidth(const uint32_t& width);
    std::shared_ptr<PortInfoIntf> portInfoIntf = nullptr;
    std::shared_ptr<PortWidthIntf> portWidthIntf = nullptr;
};

class NsmPCIeECCGroup2 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup2(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                     uint8_t deviceIndex);
    NsmPCIeECCGroup2(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                     uint8_t multiPortType, uint8_t multiPortIndex,
                     uint8_t multiPortUpstreamPortNumber);
    NsmPCIeECCGroup2() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    std::string objPath;
    std::shared_ptr<PCIeEccIntf> pcieEccIntf = nullptr;
};

class NsmPCIeECCGroup3 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup3(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                     uint8_t deviceIndex);
    NsmPCIeECCGroup3(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                     uint8_t multiPortType, uint8_t multiPortIndex,
                     uint8_t multiPortUpstreamPortNumber);
    NsmPCIeECCGroup3() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    std::string objPath;
    std::shared_ptr<PCIeEccIntf> pcieEccIntf = nullptr;
};

class NsmPCIeECCGroup4 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup4(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                     uint8_t deviceIndex);
    NsmPCIeECCGroup4(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                     uint8_t multiPortType, uint8_t multiPortIndex,
                     uint8_t multiPortUpstreamPortNumber);
    NsmPCIeECCGroup4() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    std::string objPath;
    std::shared_ptr<PCIeEccIntf> pcieEccIntf = nullptr;
};

class NsmPCIeECCGroup5 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup5(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf,
                     uint8_t deviceIndex);
    NsmPCIeECCGroup5(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf,
                     uint8_t multiPortType, uint8_t multiPortIndex,
                     uint8_t multiPortUpstreamPortNumber);
    NsmPCIeECCGroup5() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    std::string objPath;
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf = nullptr;
};

class NsmPCIeECCGroup8 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup8(const std::string& name, const std::string& type,
                     std::shared_ptr<LaneErrorIntf> laneErrorIntf,
                     uint8_t deviceIndex, const std::string& inventoryObjPath);
    NsmPCIeECCGroup8(const std::string& name, const std::string& type,
                     std::shared_ptr<LaneErrorIntf> laneErrorIntf,
                     const std::string& inventoryObjPath, uint8_t multiPortType,
                     uint8_t multiPortIndex,
                     uint8_t multiPortUpstreamPortNumber);
    NsmPCIeECCGroup8() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::shared_ptr<LaneErrorIntf> laneErrorIntf;
    const std::string inventoryObjPath;
    void updateMetricOnSharedMemory();
};

class NsmPCIeECCGroup9 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup9(
        const std::string& name, const std::string& type,
        const std::string& inventoryPath,
        std::shared_ptr<NsmRetimerAERErrorStatusIntf> aerErrorStatusIntf,
        uint8_t deviceIndex);
    NsmPCIeECCGroup9(
        const std::string& name, const std::string& type,
        const std::string& inventoryPath,
        std::shared_ptr<NsmRetimerAERErrorStatusIntf> aerErrorStatusIntf,
        uint8_t multiPortType, uint8_t multiPortIndex,
        uint8_t multiPortUpstreamPortNumber);
    NsmPCIeECCGroup9() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::string objPath;
    std::shared_ptr<NsmRetimerAERErrorStatusIntf> aerErrorStatusIntf = nullptr;
};

class NsmPCIeECCGroup10 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup10(sdbusplus::bus::bus& bus, const std::string& name,
                      const std::string& type,
                      const std::string& inventoryObjPath, uint8_t deviceIndex);
    NsmPCIeECCGroup10(sdbusplus::bus::bus& bus, const std::string& name,
                      const std::string& type,
                      const std::string& inventoryObjPath,
                      uint8_t multiPortType, uint8_t multiPortIndex,
                      uint8_t multiPortUpstreamPortNumber);
    NsmPCIeECCGroup10() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::unique_ptr<PCIeTransactionCounterIntf> pcieTransactionCounterIntf;
    const std::string inventoryObjPath;
};

} // namespace nsm
