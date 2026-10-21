// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once

#include "fpga/fpga-tables.h"

#include "nsmFpgaStatus.hpp"
#include "nsmNumericSensor/nsmNumericSensor.hpp"
#include "nsmPort/nsmPort.hpp"
#include "nsmPort/nsmPortInfo.hpp"
#include "utils.hpp"

#include <com/nvidia/Ethernet/LinkFault/server.hpp>
#include <sdbusplus/server/object.hpp>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace nsm
{

// The port, port info, state, width and availability interfaces come from
// the nsmd port and sensor headers, only the link fault one is new
using FpgaLinkFaultIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::Ethernet::server::LinkFault>;

// One Port object of an FPGA link, fed by the state items that share its
// path. Each item publishes one PortProperty: the link status, one lane of
// a multi-lane link, or a fault indication of an Ethernet link.
class NsmFpgaPortObject
{
  public:
    enum class Role
    {
        LinkStatus,
        Lane,
        LocalFault,
        RemoteFault,
        HighBer
    };
    // The role an entity-manager PortProperty names, nullopt if unknown
    static std::optional<Role> role(const std::string& property);

    NsmFpgaPortObject(sdbusplus::bus_t& bus, const std::string& path);

    // Port type and protocol, set by the first item that names them
    void describe(const std::string& type, const std::string& protocol);
    // An item joins the port; a lane item widens it
    void addItem(const std::string& item, Role role);
    // The item decoded a state, or lost its reading
    void publish(const std::string& item, uint8_t state, uint8_t health);
    void markUnavailable(const std::string& item);

    bool available() const
    {
        return availabilityIntf->available();
    }
    PortStateIntf::LinkStatusType linkStatus() const
    {
        return portStateIntf->linkStatus();
    }
    FpgaHealthIntf::HealthType health() const
    {
        return healthIntf->health();
    }
    const PortWidthIntf* width() const
    {
        return portWidthIntf.get();
    }
    const FpgaLinkFaultIntf* faults() const
    {
        return linkFaultIntf.get();
    }

    // One object per port path, shared by its items and dropped with them
    static std::shared_ptr<NsmFpgaPortObject>
        getOrCreate(sdbusplus::bus_t& bus, const std::string& path,
                    bool& created);
    static void clear();

  private:
    struct Entry
    {
        Role role;
        bool available = false;
        uint8_t state = NSM_FPGA_STATE_UNAVAILABLE_OFFLINE;
        uint8_t health = NSM_FPGA_HEALTH_OK;
    };
    // Recompute every property from the items' last readings
    void refresh();

    sdbusplus::bus_t& bus;
    std::string path;
    bool described = false;
    std::map<std::string, Entry> items;
    std::unique_ptr<PortIntf> portIntf;
    std::unique_ptr<PortInfoIntf> portInfoIntf;
    std::unique_ptr<PortStateIntf> portStateIntf;
    std::unique_ptr<FpgaHealthIntf> healthIntf;
    std::unique_ptr<AvailabilityIntf> availabilityIntf;
    // Built when the first lane item joins
    std::unique_ptr<PortWidthIntf> portWidthIntf;
    // Built when the first fault item joins
    std::unique_ptr<FpgaLinkFaultIntf> linkFaultIntf;
};

} // namespace nsm
