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

#include "config.h"

#include "counterProducer.hpp"

#include <phosphor-logging/lg2.hpp>

#include <ranges>

namespace nsm
{

#define DeviceCounterDumpObjectClass                                           \
    DeviceCounterDumpObject<CounterDataType, Size, MemFdBytesSize>
#define DeviceCounterDumpObjectTemplate(ReturnType)                            \
    CountersTemplate ReturnType DeviceCounterDumpObjectClass

CountersTemplate DeviceCounterDumpObjectClass::DeviceCounterDumpObject(
    const std::string& path) :
    CountersIntf(utils::DBusHandler::getBus(), path.c_str()),
    fd(memfd_create("nsm_progress_counters", MFD_ALLOW_SEALING))
{
    lg2::info("Initialized dump object {PATH}", "PATH", path);
}

DeviceCounterDumpObjectTemplate(sdbusplus::message::unix_fd)::getFd()
{
    return sdbusplus::message::unix_fd(fd);
}

DeviceCounterDumpObjectTemplate(bool)::updateCounters(
    const CountersDataRow<CounterDataType, Size>& rowData)
{
    try
    {
        // Check if all counters are zero (empty)
        if (std::ranges::all_of(rowData.counters,
                                [](const auto& val) { return val == 0; }))
        {
            throw std::runtime_error("No counters to write");
        }

        const auto row = static_cast<uint32_t>(rowData.key) %
                         maxRows; // Ensures data rotation in the file

        const off_t pos = row * sizeof(CountersDataRow<CounterDataType, Size>);
        if (!fd.write(pos, reinterpret_cast<const uint8_t*>(&rowData),
                      sizeof(CountersDataRow<CounterDataType, Size>)))
        {
            throw std::runtime_error(strerror(errno));
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to write dump data: Description={DESCRIPTION}, Key={KEY}, Error={ERR}",
            "DESCRIPTION", description(), "KEY",
            static_cast<uint32_t>(rowData.key), "ERR", e.what());
        return false;
    }
    return true;
}

// Explicit template instantiation for the types we use
template class DeviceCounterDumpObject<uint32_t, PollingCountersSize,
                                       SENSOR_PROGRESS_COUNTERS_MEMFD_SIZE>;
template class DeviceCounterDumpObject<int8_t, DiscoveryEventsSize,
                                       DISCOVERY_PROGRESS_COUNTERS_MEMFD_SIZE>;

} // namespace nsm
