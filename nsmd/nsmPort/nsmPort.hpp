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
#include <xyz/openbmc_project/Metrics/PortOpticalModuleMetrics/server.hpp>
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
using PortOpticalModuleMetricsIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::PortOpticalModuleMetrics>;

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
    NsmPortStatus(sdbusplus::bus::bus& bus, std::string& portName,
                  uint8_t portNum, const std::string& type,
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
        sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
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
        sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
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
        sdbusplus::bus::bus& bus, std::string& portName, uint16_t portNumber,
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
    NsmNetworkAddressAggregator(sdbusplus::bus::bus& bus,
                                const std::string& name,
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
    NsmGetPortECCCounters(sdbusplus::bus::bus& bus, const std::string& name,
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

/**
 * @brief CX optical module per-lane telemetry sensor.
 *
 * Collects per-lane RX power (mW), TX power (mW), TX bias current (mA),
 * and SNR (dB) from CX-9 NIC ports via NSM cmd 0x14 group 0x09.
 *
 * Created per-port when Entity Manager publishes
 * NSM_NVLinkWithNetworkPortAddresses with "OpticalModuleTelemetrySupported":
 * true.
 *
 * Depends on NsmSensorAggregatorPaginated (aggregate-and-page-infra feature).
 */
class NsmOpticalModuleTelemetry : public NsmSensorAggregatorPaginated
{
  public:
    NsmOpticalModuleTelemetry(sdbusplus::bus_t& bus,
                              const std::string& portName,
                              const std::string& type,
                              const std::string& portObjPath,
                              uint16_t portNumber);

  protected:
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    int handleSample(const TelemetrySample& sample) override;

    void postUpdate() override;
    void resetState() override;

    /** @brief Map a tag 0x20 encoded scaling factor to its multiplier.
     *  Unrecognized encodings fall back to the 1x default and are logged. */
    uint8_t biasScaleFromEncoding(uint8_t encoded);

    /** @brief Convert every buffered raw sample into the units the D-Bus
     *  properties declare. Called from postUpdate(), once the whole
     *  collection - including the tag 0x20 scaling factor - has arrived. */
    void convertSamples();

  private:
    uint16_t portNumber_;
    std::string objPath_;

    std::unique_ptr<PortOpticalModuleMetricsIntf> opticalMetricsIntf_;

    enum : uint8_t
    {
        NUMBER_OF_LANES = 8
    };

    /* Per-lane accumulation buffers — NUMBER_OF_LANES elements each.
     * Parenthesized (not braced) init: NUMBER_OF_LANES converts to double
     * non-narrowingly, so brace-init would select the initializer_list<double>
     * constructor and produce a 2-element {8.0, 0.0} vector instead of 8
     * zeros. */
    std::vector<double> rxPowerMW_ = std::vector<double>(NUMBER_OF_LANES, 0.0);
    std::vector<double> txPowerMW_ = std::vector<double>(NUMBER_OF_LANES, 0.0);
    std::vector<double> txBiasmA_ = std::vector<double>(NUMBER_OF_LANES, 0.0);
    std::vector<double> snrDB_ = std::vector<double>(NUMBER_OF_LANES, 0.0);

    /* Raw samples as received on the wire. handleSample() only decodes and
     * stores; convertSamples() applies the units once the collection is
     * complete. Bias in particular cannot be converted at sample time: its
     * tag 0x20 scaling factor follows the bias tags in tag order and may
     * land on a later page. */
    std::vector<uint16_t> rawTxPower_ = std::vector<uint16_t>(NUMBER_OF_LANES,
                                                              0);
    std::vector<uint16_t> rawRxPower_ = std::vector<uint16_t>(NUMBER_OF_LANES,
                                                              0);
    std::vector<uint16_t> rawBiasCurrent_ =
        std::vector<uint16_t>(NUMBER_OF_LANES, 0);
    std::vector<uint32_t> rawSnr_ = std::vector<uint32_t>(NUMBER_OF_LANES, 0);

    /* Encoded tx_bias_scaling_factor from tag 0x20, as received. Zero is
     * Multiply_1x, which is also what the spec directs consumers to assume
     * when the tag is absent -- as pre-0x20 firmware leaves it -- so the
     * zero-initialized value is already the correct fallback. */
    uint8_t encodedBiasScale_ = NSM_OPTICAL_MODULE_BIAS_SCALE_1X;

    static constexpr uint8_t kGroupId = NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE;

    /* SNR raw value -> dB: raw / kSnrRawToDbScale (e.g. 5665 -> 22.13 dB) */
    static constexpr double kSnrRawToDbScale = 256.0;

    /* TX/RX optical power raw value -> mW: the device reports a 1 uW LSB,
     * so mW = raw / 1000. */
    static constexpr double kOpticalPowerRawToMilliWatts = 1000.0;

    /* TX bias current raw value -> mA: the device reports a 2 uA LSB before
     * the tag 0x20 scaling factor, so mA = raw * 2 * scale / 1000. */
    static constexpr double kBiasRawToMicroAmps = 2.0;
    static constexpr double kMicroAmpsToMilliAmps = 1000.0;
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
    NsmOpticalModuleReset(sdbusplus::bus::bus& bus, const std::string& name,
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
