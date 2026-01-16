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

#include <fcntl.h>    // For ftruncate
#include <sys/mman.h> // for memfd_create
#include <sys/stat.h> // for fstat
#include <unistd.h>   // for write and lseek

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
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace utils
{

bool isPreferred(const std::tuple<MctpMedium, MctpBinding>& currentMctpInfo,
                 const std::tuple<MctpMedium, MctpBinding>& newMctpInfo)
{
    auto currentMedium = std::get<0>(currentMctpInfo);
    auto newMedium = std::get<0>(newMctpInfo);
    auto currentBinding = std::get<1>(currentMctpInfo);
    auto newBinding = std::get<1>(newMctpInfo);

    // Helper lambda to safely get priority with default for unknown keys
    constexpr int defaultPriority = INT_MIN;

    auto getMediumPriority = [](const MctpMedium& medium) {
        auto it = mediumPriority.find(medium);
        return (it != mediumPriority.end()) ? it->second : defaultPriority;
    };

    auto getBindingPriority = [](const MctpBinding& binding) {
        auto it = bindingPriority.find(binding);
        return (it != bindingPriority.end()) ? it->second : defaultPriority;
    };

    int currentMediumPri = getMediumPriority(currentMedium);
    int newMediumPri = getMediumPriority(newMedium);

    if (currentMediumPri == newMediumPri)
    {
        return getBindingPriority(currentBinding) >=
               getBindingPriority(newBinding);
    }
    else
    {
        return currentMediumPri >= newMediumPri;
    }
}

static const boost::regex invalidDBusNameSubString{"[^a-zA-Z0-9._/]+"};
static const uint32_t INVALID_UINT32_VALUE = 0xFFFFFFFF;
static const uint8_t INVALID_UINT8_VALUE = 0xFF;
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

std::string convertHexToString(const std::vector<uint8_t>& data,
                               const size_t dataSize)
{
    std::string result("");
    if (dataSize == 0)
    {
        return result;
    }

    std::vector<uint8_t> nvu8ArrVal(dataSize, 0);
    memcpy(nvu8ArrVal.data(), data.data(), dataSize);

    for (const auto& token : nvu8ArrVal)
    {
        result += std::format("{:02x}", token);
    }
    return result;
}

std::string convertMsgToString(bool isTx, const std::vector<uint8_t>& buffer,
                               uint8_t tag, eid_t eid)
{
    if (buffer.empty())
    {
        return "";
    }
    // Length of "EID: 1d, TAG: 03, Tx: "
    constexpr size_t headerSize = 22;
    // Length of "89 "
    constexpr size_t hexWithSpaceSize = 3;
    std::string output(headerSize + buffer.size() * hexWithSpaceSize, '\0');
    sprintf(output.data(), "EID: %02x, TAG: %02x, %s: ", eid, tag,
            isTx ? "Tx" : "Rx");
    for (size_t i = 0; i < buffer.size(); i++)
    {
        snprintf(&output[headerSize + i * hexWithSpaceSize],
                 hexWithSpaceSize + 1, "%02x ", buffer[i]);
    }
    // Remove trailing space
    output.pop_back();

    return output;
}

void printBuffer(bool isTx, const std::vector<uint8_t>& buffer, uint8_t tag,
                 eid_t eid)
{
    if (!buffer.empty())
    {
        lg2::info("{OUTPUT}", "OUTPUT",
                  convertMsgToString(isTx, buffer, tag, eid));
    }
}

bool isValidDbusString(std::string_view input)
{
    const uint8_t* token = std::bit_cast<const uint8_t*>(input.data());
    const uint8_t* end = token + input.size();

    while (token < end)
    {
        uint32_t codepoint = 0;
        int numBytes = 0;

        // utf8 can be up to 4 bytes, check the leading byte to determine its
        // size
        if (*token < 0x80) // 1-byte - ascii
        {
            codepoint = *token;
            numBytes = 1;
        }
        else if ((*token & 0xE0) == 0xC0) // 2-byte
        {
            codepoint = *token & 0x1F;
            numBytes = 2;
        }
        else if ((*token & 0xF0) == 0xE0) // 3-byte
        {
            codepoint = *token & 0x0F;
            numBytes = 3;
        }
        else if ((*token & 0xF8) == 0xF0) // 4-byte
        {
            codepoint = *token & 0x07;
            numBytes = 4;
        }
        else
        {
            // the leading byte is invalid
            return false;
        }

        // check if it's overflow
        if (token + numBytes > end)
            return false;

        // check if the subsequent byte is 10xxxxxx and do OR to codepoint
        for (int i = 1; i < numBytes; ++i)
        {
            if ((token[i] & 0xC0) != 0x80)
            {
                return false;
            }
            codepoint = (codepoint << 6) | (token[i] & 0x3F);
        }

        // check if it's overlong
        if ((numBytes == 2 && codepoint < 0x80) ||
            (numBytes == 3 && codepoint < 0x800) ||
            (numBytes == 4 && codepoint < 0x10000))
        {
            return false;
        }

        // it can't be 0x0000 or higher than 0x10FFFF based on the dbus spec.
        if (codepoint > 0x10FFFF || codepoint == 0x0000)
            return false;

        token += numBytes;
    }

    return true;
}

void printBuffer(bool isTx, const uint8_t* ptr, size_t bufferLen, uint8_t tag,
                 eid_t eid)
{
    auto outBuffer = std::vector<uint8_t>(ptr, ptr + bufferLen);
    printBuffer(isTx, outBuffer, tag, eid);
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

uint64_t getCurrentSteadyClockTimestamp()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

uint64_t getCurrentSteadyClockTimestampUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
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
        if (std::string_view(entry.first)
                .starts_with(std::string_view(uuid.data(), UUID_LEN)))
        {
            eid = std::get<0>(
                entry.second); // Accessing the first element (eid) of the tuple
            break;
        }
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
                    DBusHandler().getDbusProperty<std::string>(
                        objPath.c_str(), "Forward", interface.c_str());

                association.backward =
                    DBusHandler().getDbusProperty<std::string>(
                        objPath.c_str(), "Backward", interface.c_str());

                association.absolutePath =
                    DBusHandler().getDbusProperty<std::string>(
                        objPath.c_str(), "AbsolutePath", interface.c_str());
                association.absolutePath =
                    makeDBusNameValid(association.absolutePath);
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
        case 5:
            return "MCTPBRIDGE";
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
    auto mapperResponse = co_await coGetServiceMap(objPath, dbus::Interfaces{});

    for (const auto& [service, interfaces] : mapperResponse)
    {
        for (const auto& interface : interfaces)
        {
            if (interface.find(interfaceSubStr) != std::string::npos)
            {
                associations.push_back({});
                auto& association = associations.back();

                auto allCurrentIfaceProperties = co_await coGetAllDbusProperty(
                    entityManagerServiceStr, objPath.c_str(),
                    interface.c_str());

                std::string forward{};
                if (allCurrentIfaceProperties.count("Forward"))
                {
                    forward = std::get<std::string>(
                        allCurrentIfaceProperties.at("Forward"));
                }
                association.forward = forward;

                std::string backward{};
                if (allCurrentIfaceProperties.count("Backward"))
                {
                    backward = std::get<std::string>(
                        allCurrentIfaceProperties.at("Backward"));
                }
                association.backward = backward;

                std::string absolutePath{};
                if (allCurrentIfaceProperties.count("AbsolutePath"))
                {
                    absolutePath = std::get<std::string>(
                        allCurrentIfaceProperties.at("AbsolutePath"));
                }
                association.absolutePath = absolutePath;
                association.absolutePath =
                    makeDBusNameValid(association.absolutePath);
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

Bitfield256::Bitfield256()
{
    clear();
}
bool Bitfield256::setBit(const uint8_t& bitNumber)
{
    uint8_t fieldIndex = bitNumber / 32;
    uint8_t bitIndex = bitNumber % 32;
    uint32_t& byte = fields[fieldIndex].byte;
    bool wasSet = byte & (1 << bitIndex);
    byte |= (1 << bitIndex);
    return !wasSet;
}

bool Bitfield256::isAnyBitSet() const
{
    return std::any_of(std::begin(fields), std::end(fields),
                       [](const auto& field) { return field.byte != 0; });
}

std::string Bitfield256::getSetBits() const
{
    std::ostringstream oss;

    for (size_t i = 0; i < 8; ++i)
    {
        uint32_t byte = fields[i].byte;

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
        result.erase(result.size() - 2); // Remove trailing ", "
    }

    return result;
}
void Bitfield256::clear()
{
    memset(fields, 0, sizeof(fields));
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

    // Iterate over each group of 4 bytes in the bitmap
    for (int i = 0; i < 8; i++)
    {
        uint32_t byte = 0;
        byte |= static_cast<uint32_t>(bitmap[i * 4 + 0]) << 24;
        byte |= static_cast<uint32_t>(bitmap[i * 4 + 1]) << 16;
        byte |= static_cast<uint32_t>(bitmap[i * 4 + 2]) << 8;
        byte |= static_cast<uint32_t>(bitmap[i * 4 + 3]);
        bf.fields[i].byte = byte;
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

void readFdToBuffer(int fd, std::vector<uint8_t>& buffer)
{
    if (fd < 0)
    {
        throw std::runtime_error("readFdToBuffer - Invalid file descriptor");
    }
    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        throw std::runtime_error("readFdToBuffer - lseek failed");
    }
    struct stat fileStat;
    if (fstat(fd, &fileStat) < 0)
    {
        throw std::runtime_error("readFdToBuffer - fstat failed" +
                                 std::string(strerror(errno)));
    }
    if (fileStat.st_size < 0)
    {
        throw std::runtime_error("readFdToBuffer - Invalid file size in fd");
    }
    buffer.resize(fileStat.st_size);
    ssize_t bytesRead = read(fd, buffer.data(), buffer.size());
    if (bytesRead < 0)
    {
        throw std::runtime_error("readFdToBuffer - Fd read failed" +
                                 std::string(strerror(errno)));
    }
    else if (static_cast<size_t>(bytesRead) != buffer.size())
    {
        throw std::runtime_error(
            "readFdToBuffer - Read fewer bytes than expected");
    }
}

void writeBufferToFd(int fd, const std::vector<uint8_t>& buffer)
{
    if (fd < 0)
    {
        throw std::runtime_error("writeBufferToFd - Invalid file descriptor");
    }
    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        throw std::runtime_error("writeBufferToFd - lseek failed");
    }
    size_t totalBytesWritten = 0;
    while (totalBytesWritten < buffer.size())
    {
        ssize_t bytesWritten = write(fd, buffer.data() + totalBytesWritten,
                                     buffer.size() - totalBytesWritten);
        if (bytesWritten < 0)
        {
            throw std::runtime_error("writeBufferToFd - write failed: " +
                                     std::string(strerror(errno)));
        }
        totalBytesWritten += bytesWritten;
    }
    if (ftruncate(fd, buffer.size()) < 0)
    {
        throw std::runtime_error("writeBufferToFd - ftruncate failed: " +
                                 std::string(strerror(errno)));
    }
}

void appendBufferToFd(int fd, const std::vector<uint8_t>& buffer)
{
    if (fd < 0)
    {
        throw std::runtime_error("appendBufferToFd - Invalid file descriptor");
    }
    size_t totalBytesWritten = 0;
    while (totalBytesWritten < buffer.size())
    {
        ssize_t bytesWritten = write(fd, buffer.data() + totalBytesWritten,
                                     buffer.size() - totalBytesWritten);
        if (bytesWritten < 0)
        {
            throw std::runtime_error("appendBufferToFd - write failed: " +
                                     std::string(strerror(errno)));
        }
        totalBytesWritten += bytesWritten;
    }
}

std::string requestMsgToHexString(std::vector<uint8_t>& requestMsg)
{
    std::ostringstream oss;
    for (const auto& byte : requestMsg)
    {
        oss << std::setfill('0') << std::setw(2) << std::hex
            << static_cast<int>(byte) << " ";
    }
    return oss.str();
}

double convertAndScaleDownUint32ToDouble(uint32_t value, double scaleFactor)
{
    if (value == INVALID_UINT32_VALUE)
    {
        return static_cast<double>(INVALID_UINT32_VALUE);
    }
    else
    {
        return static_cast<double>(value) / scaleFactor;
    }
}

double convertAndScaleDownUint8ToDouble(uint8_t value, double scaleFactor)
{
    if (value == INVALID_UINT8_VALUE)
    {
        return static_cast<double>(INVALID_UINT8_VALUE);
    }
    else
    {
        return static_cast<double>(value) / scaleFactor;
    }
}

double uint64ToDoubleSafeConvert(uint64_t value)
{
    if (value > MAX_SAFE_INTEGER_IN_DOUBLE)
    {
        lg2::error(
            "Warning: Uint64 Value ({VAL}) exceeds safe range for double precision. Capping to maximum safe value.",
            "VAL", value);
        return static_cast<double>(MAX_SAFE_INTEGER_IN_DOUBLE);
    }
    return static_cast<double>(value);
}

double int64ToDoubleSafeConvert(int64_t value)
{
    if (value < 0)
    {
        if (static_cast<uint64_t>(-value) > MAX_SAFE_INTEGER_IN_DOUBLE)
        {
            lg2::error(
                "Warning: Int64 Value ({VAL}) exceeds safe range for double precision. Capping to maximum safe value.",
                "VAL", value);
            return static_cast<double>(-MAX_SAFE_INTEGER_IN_DOUBLE);
        }
    }
    else
    {
        if (static_cast<uint64_t>(value) > MAX_SAFE_INTEGER_IN_DOUBLE)
        {
            lg2::error(
                "Warning: Int64 Value ({VAL}) exceeds safe range for double precision. Capping to maximum safe value.",
                "VAL", value);
            return static_cast<double>(MAX_SAFE_INTEGER_IN_DOUBLE);
        }
    }

    return static_cast<double>(value);
}

uint16_t combineDeviceTypeAndRole(uint8_t deviceType, uint8_t deviceRole)
{
    return (uint16_t)((deviceRole << 8) |
                      deviceType); // Role in high byte, Type in low byte
}

void getDeviceTypeAndRole(uint16_t combined, uint8_t* deviceType,
                          uint8_t* deviceRole)
{
    *deviceType = (uint8_t)(combined & 0xFF); // Type is in low byte
    *deviceRole = (uint8_t)(combined >> 8);   // Role is in high byte
}

void convertMacAddressToString(const uint8_t* macAddress,
                               size_t macAddressDataLen,
                               std::string& macAddressString)
{
    const static size_t MAC_FORMAT_LENGTH = sizeof("XX:XX:XX:XX:XX:XX");
    if (macAddressDataLen < MAC_ADDRESS_DATA_LEN)
    {
        lg2::error(
            "convertMacAddressToString - Invalid mac address data length: {LEN}",
            "LEN", macAddressDataLen);
        macAddressString.clear();
        return;
    }
    char buffer[MAC_FORMAT_LENGTH];
    snprintf(buffer, MAC_FORMAT_LENGTH, "%02x:%02x:%02x:%02x:%02x:%02x",
             macAddress[0], macAddress[1], macAddress[2], macAddress[3],
             macAddress[4], macAddress[5]);
    macAddressString = buffer;
}

void convertGuid64ToString(uint64_t guid, std::string& guidString)
{
    guidString = std::format("{:04X}-{:04X}-{:04X}-{:04X}",
                             (guid >> 48) & 0xFFFF, (guid >> 32) & 0xFFFF,
                             (guid >> 16) & 0xFFFF, guid & 0xFFFF);
}
// Single-flight pattern implementation for single-threaded async execution
// for EM configuration PDI properties
requester::Coroutine coGetCachedBaseProperties(
    [[maybe_unused]] const std::string& objPath,
    [[maybe_unused]] const std::string& baseInterface,
    [[maybe_unused]] dbus::PropertyMap& cachedProperties)
{
#ifndef MOCK_DBUS_ASYNC_UTILS
    static std::unordered_map<
        std::string, std::unordered_map<std::string, dbus::PropertyMap>>
        basePropertiesCache;

    static std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::shared_future<dbus::PropertyMap>>>
        pendingRequests;

    // Check if already cached
    auto objPathIt = basePropertiesCache.find(objPath);
    if (objPathIt != basePropertiesCache.end())
    {
        auto interfaceIt = objPathIt->second.find(baseInterface);
        if (interfaceIt != objPathIt->second.end())
        {
            // "Cache hit: Using cached base properties for
            // {OBJPATH}:{INTERFACE}", "OBJPATH", objPath, "INTERFACE",
            // baseInterface);
            cachedProperties = interfaceIt->second;
            co_return NSM_SUCCESS;
        }
    }

    // Check if request is already pending
    auto pendingObjIt = pendingRequests.find(objPath);
    if (pendingObjIt != pendingRequests.end())
    {
        auto pendingInterfaceIt = pendingObjIt->second.find(baseInterface);
        if (pendingInterfaceIt != pendingObjIt->second.end())
        {
            //     "Request pending: Waiting for ongoing request for
            //     {OBJPATH}:{INTERFACE}", "OBJPATH", objPath, "INTERFACE",
            //     baseInterface);

            try
            {
                dbus::PropertyMap result = pendingInterfaceIt->second.get();
                cachedProperties = result;
                co_return NSM_SUCCESS;
            }
            catch (...)
            {
                // If the future threw an exception, we'll try making our own
                // request Continue to the request creation logic below
            }
        }
    }

    // Create a promise for this request
    std::promise<dbus::PropertyMap> promise;
    std::shared_future<dbus::PropertyMap> future = promise.get_future().share();

    // Store the pending request
    pendingRequests[objPath][baseInterface] = future;

    // "Cache miss: First retrieval for {OBJPATH}:{INTERFACE}",
    // "OBJPATH", objPath, "INTERFACE", baseInterface);

    try
    {
        // Make the actual D-Bus call
        dbus::PropertyMap properties = co_await utils::coGetAllDbusProperty(
            utils::entityManagerServiceStr, objPath, baseInterface);

        // Cache the result and clean up pending request
        basePropertiesCache[objPath][baseInterface] = properties;

        // Remove from pending requests
        auto pendingObjIt = pendingRequests.find(objPath);
        if (pendingObjIt != pendingRequests.end())
        {
            pendingObjIt->second.erase(baseInterface);
            if (pendingObjIt->second.empty())
            {
                pendingRequests.erase(objPath);
            }
        }

        // Set the promise result
        promise.set_value(properties);

        cachedProperties = properties;
        co_return NSM_SUCCESS;
    }
    catch (const std::exception& e)
    {
        // Clean up pending request on error
        auto pendingObjIt = pendingRequests.find(objPath);
        if (pendingObjIt != pendingRequests.end())
        {
            pendingObjIt->second.erase(baseInterface);
            if (pendingObjIt->second.empty())
            {
                pendingRequests.erase(objPath);
            }
        }

        // Set the promise exception
        promise.set_exception(std::current_exception());

        lg2::error(
            "Failed to fetch base properties for {OBJPATH}:{INTERFACE}:{ERROR}",
            "OBJPATH", objPath, "INTERFACE", baseInterface, "ERROR", e.what());

        co_return NSM_SW_ERROR;
    }
#else
    // In test mode, just copy the current propertyMap
    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    cachedProperties = propertyMap;
    co_return NSM_SUCCESS;
#endif
}

int parseStaticUuid(uuid_t& uuid, uint8_t& deviceType, uint8_t& instanceNumber,
                    uint8_t& deviceRole, std::string& remapPropName,
                    std::vector<std::string>& remapPropValues)
{
    int n1 = -1;
    int n2 = -1;
    char propName[128] = {0};
    char propValue[128] = {0};

    // Use sscanf with width specifiers to avoid buffer overflow
    int numParsed = std::sscanf(uuid.c_str(), "STATIC:%d:%d:%127[^:]:%127s",
                                &n1, &n2, propName, propValue);
    if (numParsed != 4)
    {
        return -3; // Parsing failed
    }

    if (n1 < 0 || n1 > 0xffff)
    {
        return -1;
    }
    if (n2 < 0 || n2 > 0xff)
    {
        return -2;
    }

    utils::getDeviceTypeAndRole(n1, &deviceType, &deviceRole);
    instanceNumber = static_cast<uint8_t>(n2);
    remapPropName = std::string(propName);
    // Check if there are more values (contains another colon)
    if (strchr(propValue, ':') == nullptr)
    {
        // Single value - fast path
        remapPropValues.clear();
        remapPropValues.reserve(1); // Avoid reallocation
        remapPropValues.emplace_back(propValue);
        return 0;
    }

    // Multiple values - parse the remainder
    remapPropValues.clear();
    remapPropValues.reserve(2); // Pre-allocate for common cases

    // Parse firstValue which contains multiple colon-separated values
    char* token = strtok(propValue, ":");
    while (token != nullptr)
    {
        remapPropValues.emplace_back(token);
        token = strtok(nullptr, ":");
    }

    return 0;
}

CustomFD::CustomFD(int fd) : fd(fd)
{
    if (fd < 0)
    {
        throw std::runtime_error(std::format(
            "CustomFD - Invalid file descriptor: {}", strerror(errno)));
    }
    fileSize = getFileSize();
}

CustomFD::~CustomFD()
{
    if (fd >= 0)
    {
        close(fd);
    }
}

int CustomFD::operator()() const
{
    return fd;
}

CustomFD::operator int() const
{
    return fd;
}
size_t CustomFD::getFileSize() const
{
    struct stat fileStat;
    if (fstat(fd, &fileStat) < 0)
    {
        return 0;
    }
    return fileStat.st_size;
}

size_t CustomFD::size() const
{
    return fileSize;
}

bool CustomFD::write(off_t pos, const uint8_t* data, const size_t size)
{
    if (static_cast<size_t>(pos) > fileSize || !data || !size ||
        lseek(fd, pos, SEEK_SET) < 0)
    {
        return false;
    }
    size_t totalWritten = 0;
    while (totalWritten < size)
    {
        ssize_t written = ::write(fd, data + totalWritten, size - totalWritten);
        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        totalWritten += written;
    }
    fileSize = std::max(fileSize, static_cast<size_t>(pos + size));
    return true;
}

bool CustomFD::read(const off_t pos, uint8_t* data, const size_t size)
{
    if ((pos + size) > fileSize || !data || !size ||
        lseek(fd, pos, SEEK_SET) < 0)
    {
        return false;
    }
    size_t totalRead = 0;
    while (totalRead < size)
    {
        ssize_t bytesRead = ::read(fd, data + totalRead, size - totalRead);
        if (bytesRead < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        if (bytesRead == 0)
        {
            return false; // EOF
        }
        totalRead += bytesRead;
    }
    return true;
}

} // namespace utils
