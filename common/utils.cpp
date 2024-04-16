/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



#include "utils.hpp"

#include "base.h"

#include <boost/regex.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>
#include <xyz/openbmc_project/Software/ExtendedVersion/server.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace utils
{

static const boost::regex invalidDBusNameSubString{"[^a-zA-Z0-9._/]+"};

uuid_t convertUUIDToString(const std::vector<uint8_t>& uuidIntArr)
{
    if (uuidIntArr.size() != UUID_INT_SIZE)
    {
        lg2::error("UUID Conversion: Failed, integer UUID size is not {UUIDSZ}",
                   "UUIDSZ", UUID_INT_SIZE);
        return "";
    }
    uuid_t uuidStr(UUID_LEN + 1, 0);

    snprintf(
        const_cast<char*>(uuidStr.data()), uuidStr.size(),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuidIntArr[0], uuidIntArr[1], uuidIntArr[2], uuidIntArr[3],
        uuidIntArr[4], uuidIntArr[5], uuidIntArr[6], uuidIntArr[7],
        uuidIntArr[8], uuidIntArr[9], uuidIntArr[10], uuidIntArr[11],
        uuidIntArr[12], uuidIntArr[13], uuidIntArr[14], uuidIntArr[15]);

    return uuidStr;
}

void printBuffer(bool isTx, const std::vector<uint8_t>& buffer)
{
    if (!buffer.empty())
    {
        std::ostringstream tempStream;
        for (int byte : buffer)
        {
            tempStream << std::setfill('0') << std::setw(2) << std::hex << byte
                       << " ";
        }
        if (isTx)
        {
            lg2::info("Tx: {TX}", "TX", tempStream.str());
        }
        else
        {
            lg2::info("Rx: {RX}", "RX", tempStream.str());
        }
    }
}

void printBuffer(bool isTx, const nsm_msg* buffer, size_t bufferLen)
{
    auto ptr = reinterpret_cast<const uint8_t*>(buffer);
    auto outBuffer =
        std::vector<uint8_t>(ptr, ptr + (sizeof(nsm_msg_hdr) + bufferLen));
    printBuffer(isTx, outBuffer);
}

std::vector<std::string> split(std::string_view srcStr, std::string_view delim,
                               std::string_view trimStr)
{
    std::vector<std::string> out;
    size_t start;
    size_t end = 0;

    while ((start = srcStr.find_first_not_of(delim, end)) != std::string::npos)
    {
        end = srcStr.find(delim, start);
        std::string_view dstStr = srcStr.substr(start, end - start);
        if (!trimStr.empty())
        {
            dstStr.remove_prefix(dstStr.find_first_not_of(trimStr));
            dstStr.remove_suffix(dstStr.size() - 1 -
                                 dstStr.find_last_not_of(trimStr));
        }

        if (!dstStr.empty())
        {
            out.push_back(std::string(dstStr));
        }
    }

    return out;
}

std::string getCurrentSystemTime()
{
    using namespace std::chrono;
    const time_point<system_clock> tp = system_clock::now();
    std::time_t tt = system_clock::to_time_t(tp);
    auto ms = duration_cast<microseconds>(tp.time_since_epoch()) -
              duration_cast<seconds>(tp.time_since_epoch());

    std::stringstream ss;
    ss << std::put_time(std::localtime(&tt), "%F %Z %T.")
       << std::to_string(ms.count());
    return ss.str();
}

std::string DBusHandler::getService(const char* path,
                                    const char* interface) const
{
    using DbusInterfaceList = std::vector<std::string>;
    std::map<std::string, std::vector<std::string>> mapperResponse;
    auto& bus = DBusHandler::getBus();

    auto mapper = bus.new_method_call(mapperService, mapperPath,
                                      mapperInterface, "GetObject");
    mapper.append(path, DbusInterfaceList({interface}));

    auto mapperResponseMsg = bus.call(mapper);
    mapperResponseMsg.read(mapperResponse);
    return mapperResponse.begin()->first;
}

GetSubTreeResponse
    DBusHandler::getSubtree(const std::string& searchPath, int depth,
                            const std::vector<std::string>& ifaceList) const
{

    auto& bus = utils::DBusHandler::getBus();
    auto method = bus.new_method_call(mapperService, mapperPath,
                                      mapperInterface, "GetSubTree");
    method.append(searchPath, depth, ifaceList);
    auto reply = bus.call(method);
    GetSubTreeResponse response;
    reply.read(response);
    return response;
}

void DBusHandler::setDbusProperty(const DBusMapping& dBusMap,
                                  const PropertyValue& value) const
{
    auto setDbusValue = [&dBusMap, this](const auto& variant) {
        auto& bus = getBus();
        auto service =
            getService(dBusMap.objectPath.c_str(), dBusMap.interface.c_str());
        auto method = bus.new_method_call(
            service.c_str(), dBusMap.objectPath.c_str(), dbusProperties, "Set");
        method.append(dBusMap.interface.c_str(), dBusMap.propertyName.c_str(),
                      variant);
        bus.call_noreply(method);
    };

    if (dBusMap.propertyType == "uint8_t")
    {
        std::variant<uint8_t> v = std::get<uint8_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "bool")
    {
        std::variant<bool> v = std::get<bool>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "int16_t")
    {
        std::variant<int16_t> v = std::get<int16_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "uint16_t")
    {
        std::variant<uint16_t> v = std::get<uint16_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "int32_t")
    {
        std::variant<int32_t> v = std::get<int32_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "uint32_t")
    {
        std::variant<uint32_t> v = std::get<uint32_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "int64_t")
    {
        std::variant<int64_t> v = std::get<int64_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "uint64_t")
    {
        std::variant<uint64_t> v = std::get<uint64_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "double")
    {
        std::variant<double> v = std::get<double>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "string")
    {
        std::variant<std::string> v = std::get<std::string>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "array[object_path]")
    {
        std::variant<std::vector<sdbusplus::message::object_path>> v =
            std::get<std::vector<sdbusplus::message::object_path>>(value);
        setDbusValue(v);
    }
    else
    {
        throw std::invalid_argument("Unsupported Dbus Type");
    }
}

PropertyValue DBusHandler::getDbusPropertyVariant(
    const char* objPath, const char* dbusProp, const char* dbusInterface) const
{
    auto& bus = DBusHandler::getBus();
    auto service = getService(objPath, dbusInterface);
    auto method =
        bus.new_method_call(service.c_str(), objPath, dbusProperties, "Get");
    method.append(dbusInterface, dbusProp);
    PropertyValue value{};
    auto reply = bus.call(method);
    reply.read(value);
    return value;
}

// assuming <uuid, eid, mctpMedium, mctpBinding> is unique
// assuming we only have a single MCTP network id 0
// its safe to say if we are given a particular eid it will map to a particular
// uuid. A device can have multiple EIDs
std::optional<uuid_t> getUUIDFromEID(
    const std::multimap<std::string,
                        std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable,
    eid_t eid)
{
    for (const auto& entry : eidTable)
    {
        // Accessing the EID in the tuple
        if (std::get<0>(entry.second) == eid)
        {
            // Returning the UUID as an optional when found
            return entry.first;
        }
    }
    // Returning an empty optional if no UUID is found
    return std::nullopt;
}

eid_t getEidFromUUID(
    const std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
        eidTable,
    uuid_t uuid)
{
    eid_t eid = std::numeric_limits<uint8_t>::max();
    for (const auto& entry : eidTable)
    {
        // TODO: as of now it is hard-coded for PCIe meidium, will handle in
        // seperate MR for selecting the fasted bandwidth medium instead of hard
        // coded value
        // Assuming UUID_LEN is defined correctly and accessible here
        if (entry.first.substr(0, UUID_LEN) == uuid.substr(0, UUID_LEN))
        {
            eid = std::get<0>(
                entry.second); // Accessing the first element (eid) of the tuple
            break;
        }
    }

    if (eid == std::numeric_limits<uint8_t>::max())
    {
        lg2::error("EID not Found for UUID={UUID}", "UUID", uuid);
    }
    else
    {
        lg2::debug("EID={EID} Found for UUID={UUID}", "UUID", uuid, "EID", eid);
    }

    return eid;
}

std::string makeDBusNameValid(const std::string& name)
{
    return boost::regex_replace(name, invalidDBusNameSubString, "_");
}

std::vector<Association> getAssociations(const std::string& objPath,
                                         const std::string& interfaceSubStr)
{
    std::map<std::string, std::vector<std::string>> mapperResponse;
    auto& bus = DBusHandler::getBus();

    auto mapper = bus.new_method_call(mapperService, mapperPath,
                                      mapperInterface, "GetObject");
    mapper.append(objPath, std::vector<std::string>{});

    auto mapperResponseMsg = bus.call(mapper);
    mapperResponseMsg.read(mapperResponse);

    std::vector<Association> associations;

    for (const auto& [service, interfaces] : mapperResponse)
    {
        for (const auto& interface : interfaces)
        {
            if (interface.find(interfaceSubStr) != std::string::npos)
            {
                associations.push_back({});
                auto& association = associations.back();

                association.forward =
                    utils::DBusHandler().getDbusProperty<std::string>(
                        objPath.c_str(), "Forward", interface.c_str());

                association.backward =
                    utils::DBusHandler().getDbusProperty<std::string>(
                        objPath.c_str(), "Backward", interface.c_str());

                association.absolutePath =
                    utils::DBusHandler().getDbusProperty<std::string>(
                        objPath.c_str(), "AbsolutePath", interface.c_str());
                association.absolutePath =
                    utils::makeDBusNameValid(association.absolutePath);
            }
        }
    }

    return associations;
}

} // namespace utils
