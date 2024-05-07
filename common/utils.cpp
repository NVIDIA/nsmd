/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
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

void convertBitMaskToVector(std::vector<uint8_t>& data,
                            const bitfield8_t* value, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
    {
        if (value[i].bits.bit0)
        {
            data.push_back((i * 8) + 0);
        }
        if (value[i].bits.bit1)
        {
            data.push_back((i * 8) + 1);
        }
        if (value[i].bits.bit2)
        {
            data.push_back((i * 8) + 2);
        }
        if (value[i].bits.bit3)
        {
            data.push_back((i * 8) + 3);
        }
        if (value[i].bits.bit4)
        {
            data.push_back((i * 8) + 4);
        }
        if (value[i].bits.bit5)
        {
            data.push_back((i * 8) + 5);
        }
        if (value[i].bits.bit6)
        {
            data.push_back((i * 8) + 6);
        }
        if (value[i].bits.bit7)
        {
            data.push_back((i * 8) + 7);
        }
    }
}

} // namespace utils
