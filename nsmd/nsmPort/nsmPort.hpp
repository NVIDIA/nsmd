#pragma once

#include "libnsm/network-ports.h"

#include "common/types.hpp"
#include "nsmCommon/sharedMemCommon.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <tal.hpp>
#include <telemetry_mrd_producer.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortInfo/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortState/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/Metrics/IBPort/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem2/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem3/server.hpp>

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

using PortType = sdbusplus::server::xyz::openbmc_project::inventory::decorator::
    PortInfo::PortType;
using PortProtocol = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortInfo::PortProtocol;
using PortLinkStates = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortState::LinkStates;
using PortLinkStatus = sdbusplus::server::xyz::openbmc_project::inventory::
    decorator::PortState::LinkStatusType;

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
    uint8_t portNumber;
    std::string objPath;
};

class NsmPortMetrics : public NsmSensor
{
  public:
    NsmPortMetrics(sdbusplus::bus::bus& bus, std::string& portName,
                   uint8_t portNum, const std::string& type,
                   const uint8_t deviceType,
                   const std::vector<utils::Association>& associations,
                   std::string& parentObjPath, std::string& inventoryObjPath);
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

    std::unique_ptr<IBPortIntf> iBPortIntf = nullptr;
    std::unique_ptr<PortMetricsOem2Intf> portMetricsOem2Intf = nullptr;
    std::unique_ptr<AssociationDefInft> associationDefinitionsIntf = nullptr;
    std::unique_ptr<PortIntf> portIntf = nullptr;

    uint8_t portNumber;
    uint8_t typeOfDevice;
    std::string objPath;
};

} // namespace nsm
