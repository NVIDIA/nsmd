#pragma once

#include "libnsm/network-ports.h"

#include "common/types.hpp"
#include "nsmCommon/sharedMemCommon.hpp"
#include "nsmDevice.hpp"
#include "nsmHistograms/nsmHistogramInfo.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <com/nvidia/Common/GUID/server.hpp>
#include <nsmSensorAggregator.hpp>
#include <phosphor-logging/lg2.hpp>
#include <tal.hpp>
#include <telemetry_mrd_producer.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortInfo/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortState/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/Metrics/EthPort/server.hpp>
#include <xyz/openbmc_project/Metrics/IBPort/server.hpp>
#include <xyz/openbmc_project/Metrics/PortECC/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem2/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem3/server.hpp>
#include <xyz/openbmc_project/Metrics/PortPacketCounters/server.hpp>
#include <xyz/openbmc_project/Network/LinkType/server.hpp>
#include <xyz/openbmc_project/Network/MACAddress/server.hpp>

namespace nsm
{
using PortInfoIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::PortInfo>;
using PortStateIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::PortState>;
using PortIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::item::Port>;
using IBPortIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::IBPort>;
using PortMetricsOem2Intf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::PortMetricsOem2>;
using PortMetricsOem3Intf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::PortMetricsOem3>;
using AssociationDefInft = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::association::Definitions>;
using EthPortIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::EthPort>;
using PortPacketCountersIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::PortPacketCounters>;
using LinkTypeIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::network::LinkType>;
using MACAddressIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::network::MACAddress>;
using GuidIntf =
    sdbusplus::server::object_t<sdbusplus::server::com::nvidia::common::GUID>;
using PortECCIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::PortECC>;

using PortType = sdbusplus::server::xyz::openbmc_project::inventory::decorator::
    PortInfo::PortType;
using PortProtocol = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortInfo::PortProtocol;
using PortLinkStates = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortState::LinkStates;
using PortLinkStatus = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortState::LinkStatusType;
using PossibleLinks =
    sdbusplus::server::xyz::openbmc_project::network::LinkType::PossibleLinks;

using LinkDownReasonCodes =
    xyz::openbmc_project::metrics::IBPort::LinkDownReasonCodes;

class NsmPortStatus : public NsmObject
{
  public:
    NsmPortStatus(sdbusplus::bus::bus& bus, std::string& portName,
                  uint8_t portNum, const std::string& type,
                  std::shared_ptr<PortMetricsOem3Intf>& portMetricsOem3Intf,
                  std::string& inventoryObjPath);
    NsmPortStatus() = default;

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;
    void updateMetricOnSharedMemory() override;
    std::string portName;

  private:
    requester::Coroutine
        checkPortCharactersticRCAndPopulateRuntimeErr(SensorManager& manager,
                                                      eid_t eid);
    std::unique_ptr<PortStateIntf> portStateIntf = nullptr;
    std::shared_ptr<PortMetricsOem3Intf> portMetricsOem3Intf = nullptr;
    uint8_t portNumber;
    std::string objPath;
};

class NsmPortCharacteristics : public NsmSensor
{
  public:
    NsmPortCharacteristics(
        sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
        const std::string& type,
        std::shared_ptr<PortMetricsOem3Intf>& portMetricsOem3Intf,
        std::shared_ptr<IBPortIntf> iBPortIntf, std::string& inventoryObjPath);
    NsmPortCharacteristics() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;
    std::string portName;

  private:
    std::unique_ptr<PortInfoIntf> portInfoIntf = nullptr;
    std::shared_ptr<PortMetricsOem3Intf> portMetricsOem3Intf = nullptr;
    std::shared_ptr<IBPortIntf> iBPortIntf = nullptr;
    uint8_t portNumber;
    std::string objPath;
    void updateLinkDownCode(const uint32_t linkDownCode);
};

class NsmPortMetrics : public NsmSensor
{
  public:
    NsmPortMetrics(
        sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
        const std::string& type, const uint8_t deviceType,
        const std::vector<utils::Association>& associations,
        std::string& parentObjPath, std::string& inventoryObjPath,
        std::shared_ptr<IBPortIntf> iBPortIntf,
        std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf,
        std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf,
        std::shared_ptr<PortECCIntf> portECCIntf);
    NsmPortMetrics() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;
    std::string portName;

  private:
    void updateCounterValues(struct nsm_port_counter_data* portData);
    double getBitErrorRate(uint64_t value);

    std::shared_ptr<IBPortIntf> iBPortIntf = nullptr;
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf = nullptr;
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf = nullptr;
    std::shared_ptr<PortECCIntf> portECCIntf = nullptr;
    std::unique_ptr<PortIntf> portIntf = nullptr;
    std::unique_ptr<AssociationDefInft> associationDefinitionsIntf = nullptr;

    uint8_t portNumber;
    uint8_t typeOfDevice;
    std::string objPath;
};

class EthPortTelemetryAggregator : public NsmSensorAggregator
{
  public:
    EthPortTelemetryAggregator(
        sdbusplus::bus::bus& bus, std::string& portName, uint16_t portNumber,
        const std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf,
        std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf);

    virtual std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    virtual int
        handleSamples(const std::vector<TelemetrySample>& samples) override;

    void updateMetricOnSharedMemory() override;
    std::string portName;

  private:
    void updateCounterValues(uint8_t tag,
                             nsm_ethernet_port_counter_data* counterValue);
    void getCounterValue(const std::string propName,
                         nsm_ethernet_port_counter_data& value,
                         std::string& ifaceName);

    uint16_t portNumber;
    std::string objPath;
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf = nullptr;
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf = nullptr;
    std::unique_ptr<EthPortIntf> ethPortIntf = nullptr;
    std::unordered_map<uint8_t, std::string> tagToPropertyMap;
};

class NsmNetworkAddressAggregator : public NsmSensorAggregator
{
  public:
    NsmNetworkAddressAggregator(sdbusplus::bus::bus& bus,
                                const std::string& name,
                                const std::string& type,
                                const std::string& objPath,
                                const std::string& nodeGuidObjPath,
                                const std::string& ethernetMacAddressObjPath,
                                const std::string& permanentMacAddressObjPath,
                                uint16_t portNumber);
    NsmNetworkAddressAggregator() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    int handleSamples(const std::vector<TelemetrySample>& samples) override;
    void getLinkType(const std::vector<TelemetrySample>& samples,
                     int8_t& linkType);

  private:
    uint16_t portNumber;
    int8_t linkType = NSM_PORT_PROTOCOL_UNKNOWN;
    std::unique_ptr<LinkTypeIntf> linkTypeIntf = nullptr;
    std::unique_ptr<MACAddressIntf> macAddressIntf = nullptr;
    std::unique_ptr<MACAddressIntf> permanentMacAddressIntf = nullptr;
    std::unique_ptr<GuidIntf> portGuidIntf = nullptr;
    std::unique_ptr<GuidIntf> nodeGuidIntf = nullptr;
};

class NsmGetPortECCCounters : public NsmSensorAggregator
{
  public:
    NsmGetPortECCCounters(const std::string& name, const std::string& type,
                          const std::string& inventoryObjPath,
                          uint8_t portNumber,
                          std::shared_ptr<PortECCIntf> portECCIntf);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    int handleSamples(const std::vector<TelemetrySample>& samples) override;
    void updateMetricOnSharedMemory() override;

  private:
    std::string objPath;
    uint8_t portNumber;
    std::shared_ptr<PortECCIntf> portECCIntf = nullptr;
};
} // namespace nsm
