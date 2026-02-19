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
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortInfo/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortWidth/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/Metrics/LanError/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem1/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem2/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeClockMode/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeLaneError/server.hpp>
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
using PCIeClockModeIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeClockMode>;
using PCIeTransactionCounterIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeTransactionCounter>;
using PCIeLaneErrorIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeLaneError>;
using PCIeDeviceIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PCIeDevice>;

using LaneErrorIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::LanError>;

using PortType = sdbusplus::server::xyz::openbmc_project::inventory::decorator::
    PortInfo::PortType;
using PortProtocol = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortInfo::PortProtocol;
using ClockMode =
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeClockMode::ClockMode;

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
                     std::shared_ptr<PCIeClockModeIntf> pcieClockModeIntf,
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
    size_t convertEncodedSizeToBytes(const uint32_t& size);
    ClockMode convertEncodedClockModeToEnum(const uint32_t& clockMode);
    std::shared_ptr<PortInfoIntf> portInfoIntf = nullptr;
    std::shared_ptr<PortWidthIntf> portWidthIntf = nullptr;
    std::shared_ptr<PCIeClockModeIntf> pcieClockModeIntf = nullptr;
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

class NsmPCIeECCGroup7 : public NsmPcieGroup
{
  public:
    NsmPCIeECCGroup7(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PortInfoIntf> portInfoIntf,
                     uint8_t deviceIndex);
    NsmPCIeECCGroup7(const std::string& name, const std::string& type,
                     const std::string& inventoryPath,
                     std::shared_ptr<PortInfoIntf> portInfoIntf,
                     uint8_t multiPortType, uint8_t multiPortIndex,
                     uint8_t multiPortUpstreamPortNumber);

    NsmPCIeECCGroup7(const std::string& name, const std::string& type,
                     std::shared_ptr<PCIeDeviceIntf> pcieDeviceIntf,
                     const std::vector<uint64_t>& functionIds,
                     uint8_t multiPortType, uint8_t multiPortIndex,
                     uint8_t multiPortUpstreamPortNumber);

    NsmPCIeECCGroup7() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::string objPath;
    std::shared_ptr<PortInfoIntf> portInfoIntf = nullptr;
    std::shared_ptr<PCIeDeviceIntf> pcieDeviceIntf = nullptr;
    std::vector<uint64_t> functionIds;
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
    NsmPCIeECCGroup10(const std::string& name, const std::string& type,
                      const std::string& inventoryObjPath, uint8_t deviceIndex,
                      bool includeInboundCounters = false);
    NsmPCIeECCGroup10(const std::string& name, const std::string& type,
                      const std::string& inventoryObjPath,
                      uint8_t multiPortType, uint8_t multiPortIndex,
                      uint8_t multiPortUpstreamPortNumber,
                      bool includeInboundCounters = false);
    NsmPCIeECCGroup10() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    std::unique_ptr<sdbusplus::asio::dbus_interface> pcieTransactionCounterIntf;
    const std::string inventoryObjPath;
    bool includeInboundCounters{false};

    uint64_t outboundWritePktCount{0};
    uint64_t outboundWriteTransfer{0};
    uint64_t reqDroppedTag{0};
    uint64_t reqDroppedCreditCompletion{0};
    uint64_t reqDroppedNonPostCredit{0};
    uint64_t inboundTLPCount{0};
    uint64_t inboundTLPsTransfer{0};
    uint64_t outboundReadPktCount{0};
    uint64_t outboundReadTransfer{0};
    uint64_t outboundTLPCount{0};
    uint64_t outboundTLPsTransfer{0};

    uint64_t join64BitCounterLowHigh(uint32_t high, uint32_t low);
    uint64_t convertDwordsSizeToBytes(uint64_t dwords);
    void registerProperties();
};

class NsmPCIeVectorGroup1 : public NsmPcieGroup
{
  public:
    NsmPCIeVectorGroup1(const std::string& name, const std::string& type,
                        const std::string& laneObjPath,
                        std::shared_ptr<PCIeLaneErrorIntf> laneErrorIntf,
                        std::shared_ptr<AssociationDefIntf> laneAssocIntf,
                        uint8_t laneIndex, uint8_t linkSpeedCode,
                        uint8_t multiPortType, uint8_t multiPortIndex,
                        uint8_t multiPortUpstreamPortNumber);
    NsmPCIeVectorGroup1() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::string laneObjPath;
    std::shared_ptr<PCIeLaneErrorIntf> laneErrorIntf = nullptr;
    std::shared_ptr<AssociationDefIntf> laneAssocIntf = nullptr;
    uint8_t laneIndex;
    uint8_t linkSpeedCode;
};

/**
 * @brief Lane Manager that dynamically creates lane sensors
 */
class NsmPCIeLaneManager : public NsmSensor
{
  public:
    NsmPCIeLaneManager(const std::string& name, const std::string& type,
                       const std::string& portObjPath,
                       std::shared_ptr<PortInfoIntf> portInfoIntf,
                       std::shared_ptr<PortWidthIntf> portWidthIntf,
                       uint8_t multiPortType, uint8_t multiPortIndex,
                       uint8_t multiPortUpstreamPortNumber);
    NsmPCIeLaneManager() = default;

    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

    std::optional<std::vector<uint8_t>>
        genRequestMsg([[maybe_unused]] eid_t eid,
                      [[maybe_unused]] uint8_t instanceId) override
    {
        return std::nullopt;
    }
    uint8_t
        handleResponseMsg([[maybe_unused]] const struct nsm_msg* responseMsg,
                          [[maybe_unused]] size_t responseLen) override
    {
        return NSM_SUCCESS;
    }

    bool hasLaneSensor(uint8_t laneIdx) const;
    size_t getLaneSensorCount() const
    {
        return laneSensors.size();
    }

  private:
    std::string portObjPath;
    std::shared_ptr<PortInfoIntf> portInfoIntf;
    std::shared_ptr<PortWidthIntf> portWidthIntf;
    uint8_t multiPortType;
    uint8_t multiPortIndex;
    uint8_t multiPortUpstreamPortNumber;

    std::map<uint8_t, std::shared_ptr<NsmPCIeVectorGroup1>> laneSensors;

    bool lanesInitialized{false};
    uint8_t currentLaneCount{0};
    uint8_t currentSpeedCode{NSM_PCIE_LINK_SPEED_CODE_UNKNOWN};

    void createLaneSensor(std::shared_ptr<NsmDevice> nsmDevice, uint8_t laneIdx,
                          uint8_t speedCode);
    void clearAllLaneSensors();
    uint8_t convertSpeedGbpsToLinkSpeedCode(double speedGbps);
};

} // namespace nsm
