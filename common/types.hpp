#pragma once

#include <stdint.h>

#include <sdbusplus/message/types.hpp>

#include <bitset>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using eid_t = uint8_t;
using uuid_t = std::string;
using Request = std::vector<uint8_t>;
using Response = std::vector<uint8_t>;
using Command = uint8_t;
using NsmType = uint8_t;

using MctpMedium = std::string;
using MctpBinding = std::string;
using NetworkId = uint8_t;
using MctpInfo = std::tuple<eid_t, uuid_t, MctpMedium, NetworkId, MctpBinding>;
using MctpInfos = std::vector<MctpInfo>;
using VendorIANA = uint32_t;

namespace nsm
{
using InventoryPropertyId = uint8_t;
using InventoryPropertyData =
    std::variant<bool, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t,
                 uint64_t, int64_t, float, std::string, std::vector<uint8_t>>;
using InventoryProperties =
    std::map<InventoryPropertyId, InventoryPropertyData>;
} // namespace nsm

namespace dbus
{

using ObjectPath = std::string;
using Service = std::string;
using Interface = std::string;
using Interfaces = std::vector<std::string>;
using Property = std::string;
using PropertyType = std::string;
using Value =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string, std::vector<uint8_t>>;

using PropertyMap = std::map<Property, Value>;
using InterfaceMap = std::map<Interface, PropertyMap>;
using ObjectValueTree = std::map<sdbusplus::message::object_path, InterfaceMap>;

typedef struct _pathAssociation
{
    std::string forward;
    std::string reverse;
    std::string path;
} PathAssociation;

} // namespace dbus
