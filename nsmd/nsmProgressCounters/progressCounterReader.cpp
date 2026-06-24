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

#include "progressCounterType.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <format>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>

using namespace nsm;
using std::cout;
using std::endl;

template <typename CounterDataType>
struct CountersDataReadRow
{
    CountersDataHeader header;
    std::vector<CounterDataType> counters;
    CountersDataReadRow(const std::vector<uint8_t>& buffer, size_t countersSize)
    {
        if (buffer.size() != (sizeof(CountersDataHeader) +
                              sizeof(CounterDataType) * countersSize))
        {
            throw std::runtime_error("Invalid buffer size");
        }
        header = *reinterpret_cast<const CountersDataHeader*>(buffer.data());
        counters = std::vector<CounterDataType>(countersSize, 0);
        auto data = reinterpret_cast<const CounterDataType*>(
            buffer.data() + sizeof(CountersDataHeader));
        for (size_t i = 0; i < countersSize; i++)
        {
            counters[i] = data[i];
        }
    }
};

using CountersBufferCollection = std::vector<std::vector<uint8_t>>;

static std::string formatTimestamp(uint64_t monotonicMicroseconds)
{
    using namespace std::chrono;

    static int64_t offset = []() {
        auto steadyNow = steady_clock::now();
        auto systemNow = system_clock::now();
        auto systemMicros =
            duration_cast<microseconds>(systemNow.time_since_epoch()).count();
        auto steadyMicros =
            duration_cast<microseconds>(steadyNow.time_since_epoch()).count();
        return systemMicros - steadyMicros;
    }();

    auto tp =
        system_clock::time_point{microseconds{monotonicMicroseconds + offset}};

    // Using local time zone
    auto zoned = zoned_time{current_zone(), tp};
    return std::format("{:%FT%T%z}", zoned);
}

template <typename T>
static std::string join(const std::vector<T>& collection)
{
    std::stringstream line;
    for (const auto& item : collection)
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            line << "," << item;
        }
        else
        {
            line << "," << std::to_string(item);
        }
    }
    return line.str();
}

template <typename CounterDataType>
static void printData(const std::string& shmName,
                      const std::string& description,
                      const CountersHeaders& countersHeaders,
                      const CountersBufferCollection&& data)
{
    if (data.empty())
    {
        cout << "No dump data available for " << description << " at "
             << shmName << endl;
        return;
    }

    std::stringstream header;
    header << "====== " << "Dump data (" << data.size() << " entries, "
           << countersHeaders.size() << " counters) for " << shmName
           << " ======";
    cout << header.str() << endl;
    cout << "Description: " << description << endl;

    cout << "DumpIteration,Timestamp";
    cout << join(countersHeaders);
    cout << endl;

    for (const auto& buffer : data)
    {
        CountersDataReadRow<CounterDataType> row(buffer,
                                                 countersHeaders.size());
        cout << row.header.key << "," << formatTimestamp(row.header.timestamp);
        cout << join(row.counters);
        cout << endl;
    }
    cout << std::string(header.str().size(), '=') << endl << endl;
}

template <typename CounterDataType>
void processData(const std::string& shmName, const std::string& description,
                 const CountersHeaders& countersHeaders, utils::CustomFD& fd)
{
    CountersBufferCollection data;
    size_t rowSize = sizeof(CountersDataHeader) +
                     sizeof(CounterDataType) * countersHeaders.size();
    for (size_t pos = 0; pos + rowSize <= fd.size(); pos += rowSize)
    {
        auto buffer = std::vector<uint8_t>(rowSize);
        if (!fd.read(pos, buffer.data(), buffer.size()))
        {
            lg2::error("Failed to read dump data: {SHM} at position {POS}",
                       "SHM", shmName, "POS", pos);
            break;
        }
        const auto* hdr =
            reinterpret_cast<const CountersDataHeader*>(buffer.data());
        if (hdr->timestamp == 0)
        {
            continue; // skip zero-initialized (never-written) rows
        }
        data.push_back(std::move(buffer));
    }

    std::ranges::sort(data, [](const auto& a, const auto& b) {
        if (a.size() < sizeof(CountersDataHeader) ||
            b.size() < sizeof(CountersDataHeader))
        {
            return false;
        }
        auto& rowA = *reinterpret_cast<const CountersDataHeader*>(a.data());
        auto& rowB = *reinterpret_cast<const CountersDataHeader*>(b.data());
        return rowA.timestamp < rowB.timestamp;
    });

    printData<CounterDataType>(shmName, description, countersHeaders,
                               std::move(data));
}

static void showUsage(const char* programName)
{
    cout << "Usage: " << programName << " [OPTIONS] [name]\n"
         << "\n"
         << "Display progress counters data for NSM devices.\n"
         << "\n"
         << "OPTIONS:\n"
         << "  -h, --help    Show this help message and exit\n"
         << "\n"
         << "ARGUMENTS:\n"
         << "  name    Show data only for specified name suffix (string)\n"
         << "                If not specified, shows data for all devices\n"
         << endl;
}

int main(int argc, char* argv[])
{
    std::string targetSuffix = "";

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            showUsage(argv[0]);
            return 0;
        }
        else
        {
            targetSuffix = arg;
        }
    }

    static const CountersHeaders pollingHeaders = {
        "Priority",
        "DumpCollection",
        "GPM",
        "LongRunning",
        "Static",
        "RoundRobin",
        "PriorityTimeExceeded",
        "PostPatch",
        "Event",
        "Error",
        "Timeout",
    };

    static const CountersHeaders discoveryHeaders = {
        "InterfaceAdded_Signal",
        "InterfaceRemoved_Signal",
        "Connectivity_Available",
        "Online_coSetdeviceStateOnlineTask_RC",
        "Online_ping_RC",
        "Online_getQueryDeviceIdentification_RC",
        "Online_mapNsmDeviceUsingEid_Success",
        "Online_getSupportedNvidiaMessageType_RC",
        "Online_getSupportedCommandCodes0_RC",
        "Online_getSupportedCommandCodes1_RC",
        "Online_getSupportedCommandCodes2_RC",
        "Online_getSupportedCommandCodes3_RC",
        "Online_getSupportedCommandCodes4_RC",
        "Online_getSupportedCommandCodes5_RC",
        "Online_getSupportedCommandCodes6_RC",
        "Online_getFRU_RC",
        "Offline_coSetdeviceStateOfflineTask_RC",
        "Offline_mapNsmDeviceUsingEid_Success",
    };

    DIR* dir = opendir("/dev/shm");
    if (!dir)
    {
        lg2::error("Failed to open /dev/shm: {ERR}", "ERR", strerror(errno));
        return 1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name = entry->d_name;
        bool isPolling = name.starts_with("nsm_polling_");
        bool isDiscovery = name.starts_with("nsm_discovery_");

        if (!isPolling && !isDiscovery)
        {
            continue;
        }
        if (!targetSuffix.empty() &&
            name.find(targetSuffix) == std::string::npos)
        {
            continue;
        }

        std::string shmName = "/" + name;
        int rawFd = shm_open(shmName.c_str(), O_RDONLY, 0);
        if (rawFd < 0)
        {
            lg2::error("Failed to open shm {SHM}: {ERR}", "SHM", shmName,
                       "ERR", strerror(errno));
            continue;
        }
        utils::CustomFD fd(rawFd);

        if (isPolling)
        {
            processData<uint32_t>(shmName,
                                  "Polling Progress Counters for " + name,
                                  pollingHeaders, fd);
        }
        else
        {
            processData<int8_t>(shmName,
                                "Discovery Progress Counters for " + name,
                                discoveryHeaders, fd);
        }
    }
    closedir(dir);
    return 0;
}
