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

#include <com/nvidia/Dump/Counters/server.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message.hpp>

#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

using namespace dbus;
using namespace nsm;
using std::cout;
using std::endl;

using CountersIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::Dump::server::Counters>;

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
static void printData(const std::string& path, const std::string& description,
                      const CountersHeaders& countersHeaders,
                      const CountersBufferCollection&& data)
{
    if (data.empty())
    {
        cout << "No dump data available for " << description << " at path "
             << path << endl;
        return;
    }

    // Print the path and dump information
    std::stringstream header;
    header << "====== " << "Dump data (" << data.size() << " entries, "
           << countersHeaders.size() << " counters) for path (..."
           << path.substr(progressCountersObjectBasePath.string().size()) << ")"
           << " ======";
    cout << header.str() << endl;
    cout << "Description: " << description << endl;

    // Print the header in csv format
    cout << "DumpIteration,Timestamp";
    cout << join(countersHeaders);
    cout << endl;

    // Print the data in csv format
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
void processData(const std::string& path, const std::string& description,
                 const CountersHeaders& countersHeaders, utils::CustomFD& fd)
{
    CountersBufferCollection data;
    size_t rowSize = sizeof(CountersDataHeader) +
                     sizeof(CounterDataType) * countersHeaders.size();
    for (size_t pos = 0; pos < fd.size(); pos += rowSize)
    {
        auto buffer = std::vector<uint8_t>(rowSize);
        if (!fd.read(pos, buffer.data(), buffer.size()))
        {
            lg2::error("Failed to read dump data: {PATH} at position {POS}",
                       "PATH", path, "POS", pos);
            break;
        }
        data.push_back(std::move(buffer));
    }

    // sort the data by timestamp
    std::ranges::sort(data, [](const auto& a, const auto& b) {
        if (a.size() < sizeof(CountersDataHeader) ||
            b.size() < sizeof(CountersDataHeader))
        {
            return false; // Invalid buffers sort last
        }
        auto& rowA = *reinterpret_cast<const CountersDataHeader*>(a.data());
        auto& rowB = *reinterpret_cast<const CountersDataHeader*>(b.data());
        return rowA.timestamp < rowB.timestamp;
    });

    // dumping the output to the console
    printData<CounterDataType>(path, description, countersHeaders,
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
         << "  name    Show data only for specified path suffix (string)\n"
         << "                If not specified, shows data for all devices\n"
         << endl;
}

template <typename T>
static T getProperty(sdbusplus::bus::bus& bus, const std::string& path,
                     const std::string& service,
                     const std::string& propertyName)
{
    sdbusplus::message::message method =
        bus.new_method_call(service.c_str(), path.c_str(),
                            "org.freedesktop.DBus.Properties", "Get");
    method.append("com.nvidia.Dump.Counters", propertyName);
    auto reply = bus.call(method);
    std::variant<T> propertyVariant;
    reply.read(propertyVariant);
    return std::get<T>(propertyVariant);
}

int main(int argc, char* argv[])
{
    std::string targetPathSuffix = ""; // empty string means show all devices

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
            targetPathSuffix = arg;
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
            // Skip if specific path suffix requested and this isn't it
            if (!targetPathSuffix.empty() &&
                path.find(targetPathSuffix) == std::string::npos)
            {
                continue;
            }

            auto service = services.front().first;
            utils::CustomFD fd(getFd(path, service));

            auto countersHeaders = getProperty<CountersHeaders>(
                bus, path, service, "CountersHeaders");
            auto description = getProperty<std::string>(bus, path, service,
                                                        "Description");
            auto counterType = getProperty<CountersIntf::CounterType>(
                bus, path, service, "CounterType");
            if (counterType == CountersIntf::CounterType::INT8)
            {
                processData<int8_t>(path, description, countersHeaders, fd);
            }
            else if (counterType == CountersIntf::CounterType::UINT32)
            {
                processData<uint32_t>(path, description, countersHeaders, fd);
            }
            else
            {
                lg2::error("Unsupported counter type: {TYPE}", "TYPE",
                           counterType);
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
