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

#include "globals.hpp"
#include "progressCounterType.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>

using namespace dbus;
using namespace nsm;
using std::cout;
using std::endl;

static constexpr std::array<std::string_view, CountersCount> counterNames = {
    "Priority",  "GPM",        "LongRunning",
    "Static",    "RoundRobin", "PriorityTimeExceeded",
    "PostPatch", "Event",      "Error",
    "Timeout",
};

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
static std::string join(const std::array<T, CountersCount>& collection)
{
    std::stringstream line;
    for (const auto& item : collection)
    {
        line << "," << item;
    }
    return line.str();
}

static void printSortedData(std::vector<CounterDataRow>& data, int deviceEid)
{
    if (data.empty())
    {
        cout << "No dump data available for device " << deviceEid << endl;
        return;
    }

    // sort the data by timestamp
    std::ranges::sort(data,
                      [](const CounterDataRow& a, const CounterDataRow& b) {
        return a.timestamp < b.timestamp;
    });

    // Print the device and dump information
    cout << "=====" << " Device " << deviceEid << " dump data (" << data.size()
         << " entries, " << CountersCount << " counters) " << "=====" << endl;

    // Print the header in csv format
    cout << "DumpIteration,Timestamp";
    cout << join(counterNames);
    cout << endl;

    // Print the data in csv format
    for (const auto& entry : data)
    {
        cout << entry.key << "," << formatTimestamp(entry.timestamp);
        cout << join(entry.counters);
        cout << endl;
    }
    cout << "==========================================================" << endl
         << endl;
}

static void showUsage(const char* programName)
{
    cout
        << "Usage: " << programName << " [OPTIONS] [DEVICE_EID]\n"
        << "\n"
        << "Display progress counter data for NSM devices.\n"
        << "\n"
        << "OPTIONS:\n"
        << "  -h, --help    Show this help message and exit\n"
        << "\n"
        << "ARGUMENTS:\n"
        << "  DEVICE_EID    Show data only for specified device EID (integer)\n"
        << "                If not specified, shows data for all devices\n"
        << endl;
}

int main(int argc, char* argv[])
{
    int targetDeviceEid = -1; // -1 means show all devices

    // Parse command line arguments
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
            try
            {
                targetDeviceEid = std::stoi(arg);
            }
            catch (const std::exception&)
            {
                lg2::error(
                    "Error: Invalid device EID '{ARG}'. Must be an integer.",
                    "ARG", arg);
                showUsage(argv[0]);
                return 1;
            }
        }
    }

    try
    {
        auto& bus = utils::DBusHandler::getBus();

        auto getFd = [&bus](const std::string& path,
                            const std::string& service) -> int {
            sdbusplus::message::message method =
                bus.new_method_call(service.c_str(), path.c_str(),
                                    "com.nvidia.Dump.Counters", "GetFd");
            auto reply = bus.call(method);

            sdbusplus::message::unix_fd unixfd;
            reply.read(unixfd);
            return dup(unixfd); // duplicate the fd before releasing the native
                                // RAII unixfd
        };

        auto objects = utils::DBusHandler().getSubtree(
            progressCountersObjectBasePath, 0, {"com.nvidia.Dump.Counters"});

        for (const auto& [path, services] : objects)
        {
            std::string pathStr = path;
            auto eid = std::stoi(pathStr.substr(pathStr.find_last_of('/') + 1));

            // Skip if specific device requested and this isn't it
            if (targetDeviceEid != -1 && eid != targetDeviceEid)
            {
                continue;
            }

            utils::CustomFD fd(getFd(pathStr, services.front().first));
            std::vector<CounterDataRow> data;
            for (size_t i = 0; i < fd.size(); i += sizeof(CounterDataRow))
            {
                CounterDataRow rowData;
                off_t pos = i;
                if (!fd.read(pos, reinterpret_cast<uint8_t*>(&rowData),
                             sizeof(CounterDataRow)))
                {
                    break;
                }
                data.emplace_back(rowData);
            }

            // dumping the output to the console
            printSortedData(data, eid);

            // If specific device was requested, we're done
            if (targetDeviceEid == eid)
            {
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Error reading dump data: {ERR}", "ERR", e.what());
        return 1;
    }

    return 0;
}
