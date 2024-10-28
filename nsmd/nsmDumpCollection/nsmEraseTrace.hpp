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

#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <com/nvidia/Dump/Erase/server.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <memory>

namespace nsm
{
using namespace sdbusplus::com::nvidia;
using namespace sdbusplus::server;

using EraseIntf = object_t<sdbusplus::com::nvidia::Dump::server::Erase>;
using EraseInfoType =
    sdbusplus::common::com::nvidia::dump::Erase::EraseInfoType;
using EraseStatus = sdbusplus::common::com::nvidia::dump::Erase::EraseStatus;
using EraseOperationStatus =
    sdbusplus::common::com::nvidia::dump::Erase::OperationStatus;

class NsmEraseTraceObject : public NsmObject, public EraseIntf
{
  public:
    NsmEraseTraceObject(sdbusplus::bus::bus& bus, const std::string& name,
                        const std::string& inventoryPath,
                        const std::string& type, const uuid_t& uuid);
    void eraseTrace() override;
    void eraseDebugInfo(EraseInfoType infoType) override;
    requester::Coroutine eraseTraceOnDevice();
    requester::Coroutine eraseDebugInfoOnDevice(uint8_t infoType);

  private:
    std::string objPath;
    uuid_t uuid;
};
} // namespace nsm
