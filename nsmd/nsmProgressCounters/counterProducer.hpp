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

#pragma once
#include "config.h"

#include "progressCounterType.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <com/nvidia/Dump/Counters/server.hpp>

#include <unordered_map>

using CountersIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::Dump::server::Counters>;

namespace nsm
{
class DeviceCounterDumpObject : public CountersIntf
{
  public:
    DeviceCounterDumpObject(uint8_t eid);
    bool updateCounters(uint32_t key, uint64_t timestamp,
                        const CountersArray& counters);

  private:
    uint8_t eid;
    utils::CustomFD fd;
    static constexpr size_t maxRows = SENSOR_PROGRESS_COUNTERS_MEMFD_SIZE /
                                      sizeof(CounterDataRow);

    sdbusplus::message::unix_fd getFd() override;
};

} // namespace nsm
