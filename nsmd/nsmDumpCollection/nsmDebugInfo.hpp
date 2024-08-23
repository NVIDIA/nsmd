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

#include <com/nvidia/Dump/DebugInfo/server.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <memory>

namespace nsm
{
using namespace sdbusplus::com::nvidia;
using namespace sdbusplus::server;

using DebugInfoIntf = object_t<sdbusplus::com::nvidia::Dump::server::DebugInfo>;

using DebugInformationType =
    sdbusplus::common::com::nvidia::dump::DebugInfo::DebugInformationType;

using OperationStatus =
    sdbusplus::common::com::nvidia::dump::DebugInfo::OperationStatus;

class NsmDebugInfoObject : public NsmObject, public DebugInfoIntf
{
  public:
    NsmDebugInfoObject(sdbusplus::bus::bus& bus, const std::string& name,
                       const std::string& inventoryPath,
                       const std::string& type, const uuid_t& uuid);

    void getDebugInfo(DebugInformationType debugInfoType,
                      uint64_t recHandle) override;

  private:
    uint8_t startDebugInfoCmd();
    void finishDebugInfoCmd(OperationStatus opStatus);
    requester::Coroutine
        getDebugInfoAsyncHandler(std::shared_ptr<Request> request);

    std::string objPath;
    uuid_t uuid;
    bool cmdInProgress{false};
};
} // namespace nsm
