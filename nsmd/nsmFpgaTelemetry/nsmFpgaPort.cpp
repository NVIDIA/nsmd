// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "nsmFpgaPort.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>

namespace nsm
{

std::optional<NsmFpgaPortObject::Role>
    NsmFpgaPortObject::role(const std::string& property)
{
    static const std::map<std::string, Role> roles = {
        {"LinkStatus", Role::LinkStatus},
        {"Lane", Role::Lane},
        {"LocalFault", Role::LocalFault},
        {"RemoteFault", Role::RemoteFault},
        {"HighBER", Role::HighBer}};
    auto it = roles.find(property);
    if (it == roles.end())
    {
        return std::nullopt;
    }
    return it->second;
}

NsmFpgaPortObject::NsmFpgaPortObject(sdbusplus::bus_t& bus,
                                     const std::string& path) :
    bus(bus), path(path),
    portIntf(std::make_unique<PortIntf>(bus, path.c_str())),
    portInfoIntf(std::make_unique<PortInfoIntf>(bus, path.c_str())),
    portStateIntf(std::make_unique<PortStateIntf>(bus, path.c_str())),
    healthIntf(std::make_unique<FpgaHealthIntf>(bus, path.c_str())),
    availabilityIntf(std::make_unique<AvailabilityIntf>(bus, path.c_str()))
{
    portStateIntf->linkStatus(PortStateIntf::LinkStatusType::LinkDown);
    healthIntf->health(FpgaHealthIntf::HealthType::OK);
    availabilityIntf->available(false);
}

void NsmFpgaPortObject::describe(const std::string& type,
                                 const std::string& protocol)
{
    if (described || type.empty() || protocol.empty())
    {
        return;
    }
    // The generated converters throw on an unknown name, there is no query
    try
    {
        portInfoIntf->type(PortInfoIntf::convertPortTypeFromString(type));
        portInfoIntf->protocol(
            PortInfoIntf::convertPortProtocolFromString(protocol));
        described = true;
    }
    catch (const std::exception&)
    {
        lg2::error("FPGA port {PATH} has an invalid PortType or PortProtocol",
                   "PATH", path);
    }
}

void NsmFpgaPortObject::addItem(const std::string& item, Role role)
{
    items[item].role = role;
    if (role == Role::Lane && !portWidthIntf)
    {
        portWidthIntf = std::make_unique<PortWidthIntf>(bus, path.c_str());
    }
    if (role != Role::Lane && role != Role::LinkStatus && !linkFaultIntf)
    {
        linkFaultIntf = std::make_unique<FpgaLinkFaultIntf>(bus, path.c_str());
    }
    refresh();
}

void NsmFpgaPortObject::publish(const std::string& item, uint8_t state,
                                uint8_t health)
{
    auto it = items.find(item);
    if (it == items.end())
    {
        return;
    }
    it->second.available = true;
    it->second.state = state;
    it->second.health = health;
    refresh();
}

void NsmFpgaPortObject::markUnavailable(const std::string& item)
{
    auto it = items.find(item);
    if (it == items.end())
    {
        return;
    }
    it->second.available = false;
    it->second.state = NSM_FPGA_STATE_UNAVAILABLE_OFFLINE;
    it->second.health = NSM_FPGA_HEALTH_OK;
    refresh();
}

void NsmFpgaPortObject::refresh()
{
    using LinkStatus = PortStateIntf::LinkStatusType;
    using HealthType = FpgaHealthIntf::HealthType;
    using FaultState = FpgaLinkFaultIntf::FaultState;
    // The link is up when every link item and lane that reads is Enabled
    bool available = false;
    bool up = true;
    size_t lanes = 0;
    size_t activeLanes = 0;
    uint8_t worst = NSM_FPGA_HEALTH_OK;
    for (const auto& [name, entry] : items)
    {
        if (entry.available)
        {
            worst = std::max(worst, entry.health);
        }
        if (entry.role == Role::Lane)
        {
            ++lanes;
        }
        if (entry.role != Role::Lane && entry.role != Role::LinkStatus)
        {
            continue;
        }
        if (!entry.available)
        {
            // A lane without a reading leaves the link down
            up = up && entry.role != Role::Lane;
            continue;
        }
        available = true;
        const bool enabled = entry.state == NSM_FPGA_STATE_ENABLED;
        up = up && enabled;
        if (entry.role == Role::Lane && enabled)
        {
            ++activeLanes;
        }
    }
    availabilityIntf->available(available);
    portStateIntf->linkStatus(available && up ? LinkStatus::LinkUp
                                              : LinkStatus::LinkDown);
    static constexpr std::array<HealthType, 3> healths = {
        HealthType::OK, HealthType::Warning, HealthType::Critical};
    healthIntf->health(worst < healths.size() ? healths[worst]
                                              : HealthType::Warning);
    if (portWidthIntf)
    {
        portWidthIntf->width(lanes);
        portWidthIntf->activeWidth(activeLanes);
    }
    if (!linkFaultIntf)
    {
        return;
    }
    // A fault item reads Asserted on Fault, Clear otherwise, Unknown unread
    auto fault = [this](Role role) {
        for (const auto& [name, entry] : items)
        {
            if (entry.role != role)
            {
                continue;
            }
            if (!entry.available)
            {
                return FaultState::Unknown;
            }
            return entry.state == NSM_FPGA_STATE_FAULT ? FaultState::Asserted
                                                       : FaultState::Clear;
        }
        return FaultState::Unknown;
    };
    linkFaultIntf->localFault(fault(Role::LocalFault));
    linkFaultIntf->remoteFault(fault(Role::RemoteFault));
    linkFaultIntf->highBER(fault(Role::HighBer));
}

namespace
{
std::map<std::string, std::weak_ptr<NsmFpgaPortObject>>& portRegistry()
{
    static std::map<std::string, std::weak_ptr<NsmFpgaPortObject>> ports;
    return ports;
}
} // namespace

std::shared_ptr<NsmFpgaPortObject>
    NsmFpgaPortObject::getOrCreate(sdbusplus::bus_t& bus,
                                   const std::string& path, bool& created)
{
    created = false;
    auto& registry = portRegistry();
    if (auto existing = registry[path].lock())
    {
        return existing;
    }
    auto portObject = std::make_shared<NsmFpgaPortObject>(bus, path);
    registry[path] = portObject;
    created = true;
    return portObject;
}

void NsmFpgaPortObject::clear()
{
    portRegistry().clear();
}

} // namespace nsm
