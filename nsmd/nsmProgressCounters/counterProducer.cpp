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

#include "counterProducer.hpp"

#include "globals.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <ranges>
#include <stdexcept>

namespace nsm
{

DeviceCounterDumpObject::DeviceCounterDumpObject(uint8_t eid) :
    CountersIntf(
        utils::DBusHandler::getBus(),
        std::string(progressCountersObjectBasePath / std::to_string(eid))
            .c_str()),
    eid(eid), fd(memfd_create("nsm_progress_counters", MFD_ALLOW_SEALING))
{
    lg2::info("Initialized dump object for device {ID}", "ID",
              static_cast<int>(eid));
}

sdbusplus::message::unix_fd DeviceCounterDumpObject::getFd()
{
    return sdbusplus::message::unix_fd(fd);
}

bool DeviceCounterDumpObject::updateCounters(uint32_t key, uint64_t timestamp,
                                             const CountersArray& counters)
{
    try
    {
        // Check if all counters are zero (empty)
        if (std::ranges::all_of(counters,
                                [](const auto& val) { return val == 0; }))
        {
            throw std::runtime_error("No counters to write");
        }

        const auto row = key % maxRows; // Ensures data rotation in the file

        const off_t pos = row * sizeof(CounterDataRow);
        const CounterDataRow rowData = {key, timestamp, counters};
        if (!fd.write(pos, reinterpret_cast<const uint8_t*>(&rowData),
                      sizeof(CounterDataRow)))
        {
            throw std::runtime_error(
                std::format("Fd write error: {}", strerror(errno)));
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to write dump data for Device={ID}, Key={KEY}, Error={ERR}",
            "ID", eid, "KEY", static_cast<int>(key), "ERR", e.what());
        return false;
    }
    return true;
}

} // namespace nsm
