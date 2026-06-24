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

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cstring>

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <ranges>

namespace nsm
{

#define DeviceCounterDumpObjectClass                                           \
    DeviceCounterDumpObject<CounterDataType, Size, MemFdBytesSize>
#define DeviceCounterDumpObjectTemplate(ReturnType)                            \
    CountersTemplate ReturnType DeviceCounterDumpObjectClass

// Derives "/nsm_polling_0" from "/xyz/.../polling/0"
static std::string toShmName(const std::string& path)
{
    auto last = path.rfind('/');
    auto prev = path.rfind('/', last - 1);
    std::string tail = path.substr(prev + 1);
    std::replace(tail.begin(), tail.end(), '/', '_');
    return "/nsm_" + tail;
}

CountersTemplate DeviceCounterDumpObjectClass::DeviceCounterDumpObject(
    const std::string& path) :
    shmName(toShmName(path)),
    fd([&]() {
        shm_unlink(shmName.c_str());
        int rawFd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0644);
        if (rawFd >= 0 && ftruncate(rawFd, MemFdBytesSize) < 0)
        {
            lg2::error("Failed to size shm {SHM}: {ERR}", "SHM", shmName,
                       "ERR", strerror(errno));
            close(rawFd);
            rawFd = -1;
        }
        return rawFd;
    }())
{
    lg2::info("Initialized shm object {SHM}", "SHM", shmName);
}

CountersTemplate DeviceCounterDumpObjectClass::~DeviceCounterDumpObject()
{
    shm_unlink(shmName.c_str());
}

DeviceCounterDumpObjectTemplate(bool)::updateCounters(
    const CountersDataRow<CounterDataType, Size>& rowData)
{
    try
    {
        // Check if all counters are zero (nothing to record)
        if (std::ranges::all_of(rowData.counters,
                                [](const auto& val) { return val == 0; }))
        {
            return false;
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
            "Failed to write dump data: Shm={SHM}, Key={KEY}, Error={ERR}",
            "SHM", shmName, "KEY",
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
