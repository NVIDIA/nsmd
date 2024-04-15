#pragma once

#include "libnsm/network-ports.h"

#include "common/types.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Metrics/IBPort/server.hpp>
#include <xyz/openbmc_project/Metrics/PortMetricsOem2/server.hpp>

namespace nsm
{

using IBPortIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::IBPort>;
using PortMetricsOem2Intf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::metrics::PortMetricsOem2>;
using AssociationDefinitionsInft = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::association::Definitions>;

class NsmPort : public NsmSensor
{
  public:
    NsmPort(sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
            const std::string& type, std::string& parentObjPath,
            std::string& inventoryObjPath);
    NsmPort() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    std::string portName;

  private:
    void updateCounterValues(struct nsm_port_counter_data* portData);

    std::unique_ptr<IBPortIntf> iBPortIntf = nullptr;
    std::unique_ptr<PortMetricsOem2Intf> portMetricsOem2Intf = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsIntf =
        nullptr;

    uint8_t portNumber;
};

} // namespace nsm