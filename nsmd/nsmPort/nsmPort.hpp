#pragma once

#include "libnsm/network-ports.h"

#include "common/types.hpp"
#include "nsmDbusIfaceOverride/nsmResetIface.hpp"
#include "nsmDevice.hpp"
#include "nsmHistograms/nsmHistogramInfo.hpp"
#include "nsmObject.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <com/nvidia/Common/GUID/server.hpp>
#include <com/nvidia/Reset/server.hpp>
#include <nsmSensorAggregator.hpp>
#include <phosphor-logging/lg2.hpp>
#ifdef NVIDIA_SHMEM
#include "nsmCommon/sharedMemCommon.hpp"

#include <telemetry_mrd_producer.hpp>
#endif
#include <com/nvidia/NVLink/PortHealthMetrics/server.hpp>
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
using PortHealthMetricsIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::nv_link::PortHealthMetrics>;
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

using NvidiaResetIntf =
    sdbusplus::server::object_t<sdbusplus::server::com::nvidia::Reset>;
using NvidiaResetTypes =
    sdbusplus::server::com::nvidia::Reset::NvidiaResetTypes;

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
using EarlyHealthIndicationValues =
    com::nvidia::nv_link::PortHealthMetrics::EarlyHealthIndicationValues;
using AttentionTriggerReasonValues =
    com::nvidia::nv_link::PortHealthMetrics::AttentionTriggerReasonValues;

class NsmPortStatus : public NsmObject
{
  public:
    NsmPortStatus(sdbusplus::bus_t& bus, std::string& portName, uint8_t portNum,
                  const std::string& type,
                  std::shared_ptr<PortMetricsOem3Intf>& portMetricsOem3Intf,
                  std::string& inventoryObjPath);
    NsmPortStatus() = default;

    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;
    void updateMetricOnSharedMemory() override;
    std::string portName;

  private:
    requester::Coroutine checkPortCharactersticRCAndPopulateRuntimeErr(
        std::shared_ptr<NsmDevice> nsmDevice);
    std::unique_ptr<PortStateIntf> portStateIntf = nullptr;
    std::shared_ptr<PortMetricsOem3Intf> portMetricsOem3Intf = nullptr;
    uint8_t portNumber;
    std::string objPath;
};

class NsmPortCharacteristics : public NsmSensor
{
  public:
    NsmPortCharacteristics(
        sdbusplus::bus_t& bus, std::string& portName, uint8_t portNum,
        const std::string& type, uint8_t deviceType,
        std::shared_ptr<PortMetricsOem3Intf>& portMetricsOem3Intf,
        std::shared_ptr<IBPortIntf> iBPortIntf,
        std::shared_ptr<PortHealthMetricsIntf> portHealthMetricsIntf,
        std::string& inventoryObjPath);
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
    std::shared_ptr<PortHealthMetricsIntf> portHealthMetricsIntf = nullptr;
    static bool isWarningSeverity(EarlyHealthIndicationValues state)
    {
        return state != EarlyHealthIndicationValues::Healthy;
    }

    EarlyHealthIndicationValues previousEarlyHealthIndication =
        EarlyHealthIndicationValues::Unknown;
    bool healthStateInitialized = false;
    uint8_t portNumber;
    // GPU exposes the full port-characteristics telemetry; a switch exposes
    // only the health counters. Gates non-health publishes.
    uint8_t deviceType;
    std::string objPath;
    void updateLinkDownCode(const uint32_t linkDownCode);
    void decodeAttentionTrigger(uint8_t triggerValue);
    // Emits NVLinkPortHealthStateChanged on a health-state transition (the
    // first observation is baselined, not reported). Isolated from
    // handleResponseMsg for readability and to localize the flood policy.
    void emitHealthStateChangeEvent(EarlyHealthIndicationValues newHealthState);
};

class NsmPortMetrics : public NsmSensor
{
  public:
    NsmPortMetrics(
        sdbusplus::bus_t& bus, std::string& portName, uint8_t portNum,
        const std::string& type, const uint8_t deviceType,
        const std::vector<utils::Association>& associations,
        std::string& parentObjPath, std::string& inventoryObjPath,
        std::shared_ptr<IBPortIntf> iBPortIntf,
        std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf,
        std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf);
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
        sdbusplus::bus_t& bus, std::string& portName, uint16_t portNumber,
        const std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf,
        std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf);

    virtual std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    int handleSample(const TelemetrySample& sample) override;
    std::string portName;

  private:
    void updateCounterValues(uint8_t tag,
                             nsm_ethernet_port_counter_data* counterValue);
    void getInterfaceName(const std::string propName, std::string& ifaceName);

    uint16_t portNumber;
    std::string objPath;
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf = nullptr;
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf = nullptr;
    std::unique_ptr<EthPortIntf> ethPortIntf = nullptr;
    std::unordered_map<uint8_t, std::string> tagToPropertyMap;
};

class NsmNetworkAddressAggregator : public NsmSensor
{
  public:
    NsmNetworkAddressAggregator(sdbusplus::bus_t& bus, const std::string& name,
                                const std::string& type,
                                const std::string& objPath,
                                const std::string& nodeGuidObjPath,
                                const std::string& ethernetMacAddressObjPath,
                                const std::string& permanentMacAddressObjPath,
                                uint16_t portNumber);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) final;

  private:
    uint16_t portNumber;
    int8_t linkType = NSM_PORT_PROTOCOL_UNKNOWN;
    std::unique_ptr<LinkTypeIntf> linkTypeIntf = nullptr;
    std::unique_ptr<MACAddressIntf> macAddressIntf = nullptr;
    std::unique_ptr<MACAddressIntf> permanentMacAddressIntf = nullptr;
    std::unique_ptr<GuidIntf> portGuidIntf = nullptr;
    std::unique_ptr<GuidIntf> nodeGuidIntf = nullptr;
    std::vector<NsmSensorAggregator::TelemetrySample> samples;
};

class NsmGetPortECCCounters : public NsmSensor
{
  public:
    NsmGetPortECCCounters(sdbusplus::bus_t& bus, const std::string& name,
                          const std::string& type,
                          const std::string& inventoryObjPath,
                          uint8_t portNumber);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) final;
    void updateMetricOnSharedMemory() override;

  private:
    std::string objPath;
    uint16_t portNumber;
    std::unique_ptr<PortECCIntf> portECCIntf = nullptr;
};

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
/** @class NsmOpticalModuleReset
 *
 *  One instance per CX9 port. DBUS object at .../Ports/Port_<N> exposes
 *  com.nvidia.Reset (ResetType=OpticalModuleGracefulReset) and
 *  Control.ResetAsync (Reset() → NSM cmd 0x06, Target=5, Trigger=0,
 *  Index=port_index).
 */
class NsmOpticalModuleReset : public NsmObject
{
  public:
    NsmOpticalModuleReset(sdbusplus::bus_t& bus, const std::string& name,
                          const std::string& type,
                          const std::string& portObjPath,
                          std::shared_ptr<NsmDevice> device,
                          uint32_t port_index);

  private:
    std::shared_ptr<NvidiaResetIntf> resetIntf = nullptr;
    std::shared_ptr<NsmDeviceResetAsyncIntf> resetAsyncIntf = nullptr;
    std::string objPath;
};
#endif

} // namespace nsm
