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

#include "dBusAsyncUtils.hpp"

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
    auto mapperResponse = DBusHandler().getServiceMap(objPath.c_str());

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

Associations getAssociations(const std::vector<Association>& associations)
{
    Associations tuples;
    for (auto& association : associations)
    {
        tuples.emplace_back(association.forward, association.backward,
                            association.absolutePath);
    }
    return tuples;
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

std::string getDeviceNameFromDeviceType(const uint8_t deviceType)
{
    switch (deviceType)
    {
        case 0:
            return "GPU";
        case 1:
            return "SWITCH";
        case 2:
            return "BRIDGE";
        case 3:
            return "BASEBOARD";
        case 4:
            return "EROT";
        default:
            return "NSM_DEV_ID_UNKNOWN";
    }
}

std::string getDeviceInstanceName(const uint8_t deviceType,
                                  const uint8_t instanceNumber)
{
    std::string deviceInstanceName = getDeviceNameFromDeviceType(deviceType);
    deviceInstanceName += "_";
    deviceInstanceName += std::to_string(static_cast<int>(instanceNumber));
    return deviceInstanceName;
}

requester::Coroutine coGetAssociations(const std::string& objPath,
                                       const std::string& interfaceSubStr,
                                       std::vector<Association>& associations)
{
    auto mapperResponse = co_await utils::coGetServiceMap(objPath,
                                                          dbus::Interfaces{});

    for (const auto& [service, interfaces] : mapperResponse)
    {
        for (const auto& interface : interfaces)
        {
            if (interface.find(interfaceSubStr) != std::string::npos)
            {
                associations.push_back({});
                auto& association = associations.back();

                association.forward =
                    co_await utils::coGetDbusProperty<std::string>(
                        objPath.c_str(), "Forward", interface.c_str());

                association.backward =
                    co_await utils::coGetDbusProperty<std::string>(
                        objPath.c_str(), "Backward", interface.c_str());

                association.absolutePath =
                    co_await utils::coGetDbusProperty<std::string>(
                        objPath.c_str(), "AbsolutePath", interface.c_str());
                association.absolutePath =
                    utils::makeDBusNameValid(association.absolutePath);
            }
        }
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}
// Function to convert bitfield256_t to bitmap
std::vector<uint8_t> bitfield256_tToBitMap(bitfield256_t bf)
{
    std::vector<uint8_t> bitmap(32, 0); // 32 bytes = 256 bits

    // Iterate over each bitfield32_t in the bitfield256_t
    for (int i = 0; i < 8; i++)
    {
        uint32_t byte = bf.fields[i].byte;
        // Iterate over each bit in the bitfield32_t
        for (int j = 0; j < 32; j++)
        {
            // If the bit is set, set the corresponding bit in the bitmap
            // i * 4 accounts for the 4 bytes (32 bits) per bitfield32_t
            // element j / 8 determines which byte within the 4 bytes the
            // current bit belongs to.
            if (byte & (1 << j))
            {
                bitmap[i * 4 + j / 8] |= (1 << (j % 8));
            }
        }
    }
    return bitmap;
}

// Function to convert bitfield256_t to bitmap
std::vector<uint8_t> bitfield256_tToBitArray(bitfield256_t bf)
{
    std::vector<uint8_t> bitmap(32, 0); // 32 bytes = 256 bits

    // Iterate over each bitfield32_t in the bitfield256_t
    for (int i = 0; i < 8; i++)
    {
        uint32_t byte = bf.fields[i].byte;
        bitmap[i * 4 + 0] = (byte >> 24) & 0xFF;
        bitmap[i * 4 + 1] = (byte >> 16) & 0xFF;
        bitmap[i * 4 + 2] = (byte >> 8) & 0xFF;
        bitmap[i * 4 + 3] = (byte >> 0) & 0xFF;
    }
    return bitmap;
}

std::pair<std::vector<uint8_t>, std::vector<uint8_t>>
    bitmapToIndices(const std::vector<uint8_t>& bitmap)
{
    std::vector<uint8_t> zeroIndices, oneIndices;
    uint8_t index = 0;
    for (auto byte : bitmap)
    {
        for (auto bit = 0; bit < 8; ++bit)
        {
            if (byte & 0x01)
            {
                oneIndices.emplace_back(index++);
            }
            else
            {
                zeroIndices.emplace_back(index++);
            }
            byte >>= 1;
        }
    }

    return std::make_pair(zeroIndices, oneIndices);
}

std::vector<uint8_t> indicesToBitmap(const std::vector<uint8_t>& indices,
                                     const size_t size)
{
    constexpr const size_t maxBitmapSize = 8; // maximum size used by ERoT
    if (size > maxBitmapSize)
    {
        throw std::invalid_argument(
            "Requested bitmap size larger than maximum allowed value");
    }
    if (indices.empty())
    {
        return std::vector<uint8_t>(size, 0);
    }
    uint8_t maxIndex = *std::max_element(indices.begin(), indices.end());
    std::vector<uint8_t> bitmap;
    if (size == 0)
    {
        bitmap.resize(maxIndex / 8 + 1, 0);
    }
    else if (maxIndex > size * 8 - 1)
    {
        throw std::invalid_argument("Index out of bounds for specified size");
    }
    else
    {
        bitmap.resize(size, 0);
    }
    for (auto& index : indices)
    {
        size_t bitmapIndex = index / 8;
        size_t bitmapBit = index % 8;
        bitmap[bitmapIndex] |= 1 << bitmapBit;
    }

    return bitmap;
}

bool utils::bitfield256_err_code::isBitSet(const int& errCode)
{
    if (errCode == NSM_SUCCESS || errCode == NSM_SW_SUCCESS)
    {
        return true; // No need to track these error codes
    }

    uint8_t fieldIndex = errCode / 32;
    uint8_t bitIndex = errCode % 32;
    uint32_t& byte = bitMap.fields[fieldIndex].byte;

    if (!(byte & (1 << bitIndex)))
    {
        byte |= (1 << bitIndex);
        return false;
    }

    return true;
}

std::string utils::bitfield256_err_code::getSetBits() const
{
    std::ostringstream oss;

    for (size_t i = 0; i < 8; ++i)
    {
        uint32_t byte = bitMap.fields[i].byte;

        while (byte > 0)
        {
            // Get the position of the least significant set bit
            int position = __builtin_ctz(byte);
            oss << (i * 32 + position) << ", ";

            // Remove the least significant set bit
            byte &= (byte - 1);
        }
    }

    std::string result = oss.str();
    if (!result.empty())
    {
        result.pop_back();
        result.pop_back();
        // removed trailing , and whitespace
    }

    return result.empty() ? "No err code" : result;
}
std::vector<sdbusplus::common::xyz::openbmc_project::software::SecurityCommon::
                UpdateMethods>
    updateMethodsBitfieldToList(bitfield32_t updateMethodBitfield)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;

    std::vector<SecurityCommon::UpdateMethods> updateMethods;
    if (updateMethodBitfield.bits.bit0)
    {
        updateMethods.emplace_back(SecurityCommon::UpdateMethods::Automatic);
    }
    if (updateMethodBitfield.bits.bit2)
    {
        updateMethods.emplace_back(
            SecurityCommon::UpdateMethods::MediumSpecificReset);
    }
    if (updateMethodBitfield.bits.bit3)
    {
        updateMethods.emplace_back(SecurityCommon::UpdateMethods::SystemReboot);
    }
    if (updateMethodBitfield.bits.bit4)
    {
        updateMethods.emplace_back(SecurityCommon::UpdateMethods::DCPowerCycle);
    }
    if (updateMethodBitfield.bits.bit5)
    {
        updateMethods.emplace_back(SecurityCommon::UpdateMethods::ACPowerCycle);
    }
    if (updateMethodBitfield.bits.bit16)
    {
        updateMethods.emplace_back(SecurityCommon::UpdateMethods::WarmReset);
    }
    if (updateMethodBitfield.bits.bit17)
    {
        updateMethods.emplace_back(SecurityCommon::UpdateMethods::HotReset);
    }
    if (updateMethodBitfield.bits.bit18)
    {
        updateMethods.emplace_back(SecurityCommon::UpdateMethods::FLR);
    }

    return updateMethods;
}
// Function to convert bitmap to bitfield256_t
bitfield256_t bitMapToBitfield256_t(const std::vector<uint8_t>& bitmap)
{
    bitfield256_t bf = {0}; // Initialize all fields to 0

    // Ensure the bitmap has the correct size
    if (bitmap.size() != 32)
    {
        return bf;
    }

    // Iterate over each bitfield32_t in the bitfield256_t
    for (int i = 0; i < 8; i++)
    {
        uint32_t& byte = bf.fields[i].byte;
        // Iterate over each bit in the bitfield32_t
        for (int j = 0; j < 32; j++)
        {
            // Check if the corresponding bit in the bitmap is set
            // i * 4 accounts for the 4 bytes (32 bits) per bitfield32_t
            // j / 8 determines which byte within the 4 bytes the current bit
            // belongs to
            if (bitmap[i * 4 + j / 8] & (1 << (j % 8)))
            {
                byte |= (1U << j);
            }
        }
    }

    return bf;
}

std::string vectorTo256BitHexString(const std::vector<uint8_t>& value)
{
    // Ensure the vector has exactly 32 bytes (256 bits)
    if (value.size() != 32)
    {
        return "0x" + std::string(64, '0');
    }

    // Convert the vector to a hex string
    std::stringstream ss;
    ss << "0x";
    for (const auto& byte : value)
    {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(byte);
    }
    return ss.str();
}

} // namespace utils
