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
#include "progressCounterType.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <string>

namespace nsm
{

#define CountersTemplate                                                       \
    template <typename CounterDataType, std::size_t Size,                      \
              std::size_t MemFdBytesSize>

CountersTemplate class DeviceCounterDumpObject
{
  public:
    DeviceCounterDumpObject(const std::string& path);
    ~DeviceCounterDumpObject();
    bool updateCounters(const CountersDataRow<CounterDataType, Size>& rowData);

  private:
    std::string shmName;
    utils::CustomFD fd;
    static constexpr size_t maxRows =
        MemFdBytesSize / sizeof(CountersDataRow<CounterDataType, Size>);
};

} // namespace nsm
