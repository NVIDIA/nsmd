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

#include <com/nvidia/Dump/LogInfo/server.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <memory>

namespace nsm
{
using namespace sdbusplus::com::nvidia;
using namespace sdbusplus::server;

using LogInfoIntf = object_t<sdbusplus::com::nvidia::Dump::server::LogInfo>;

using TimeSyncFrom =
    sdbusplus::common::com::nvidia::dump::LogInfo::TimeSyncFrom;

using CmdOperationStatus =
    sdbusplus::common::com::nvidia::dump::LogInfo::OperationStatus;

class NsmLogInfoObject : public NsmObject, public LogInfoIntf
{
  public:
    NsmLogInfoObject(sdbusplus::bus::bus& bus, const std::string& name,
                     const std::string& inventoryPath, const std::string& type,
                     const uuid_t& uuid);

    void getLogInfo(uint64_t recHandle) override;

  private:
    uint8_t startLogInfoCmd();
    void finishLogInfoCmd(OperationStatus opStatus);
    requester::Coroutine
        getLogInfoAsyncHandler(std::shared_ptr<Request> request);

    std::string objPath;
    uuid_t uuid;
    bool cmdInProgress{false};
};
} // namespace nsm
