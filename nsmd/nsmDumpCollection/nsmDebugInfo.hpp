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

#include "diagnostics.h"

#include "asyncOperationManager.hpp"

#include <com/nvidia/Dump/DebugInfo/server.hpp>

#include <memory>

namespace nsm
{
using namespace sdbusplus::com::nvidia;
using namespace sdbusplus::server;

using DebugInfoIntf = object_t<sdbusplus::com::nvidia::Dump::server::DebugInfo>;

using DebugInformationType =
    sdbusplus::common::com::nvidia::dump::DebugInfo::DebugInformationType;

using DebugDumpType = sdbusplus::common::com::nvidia::dump::DebugInfo::DumpType;

class NsmDebugInfoObject : public NsmObject, public DebugInfoIntf
{
  public:
    NsmDebugInfoObject(sdbusplus::bus_t& bus, const std::string& name,
                       const std::string& inventoryPath,
                       const std::string& type, const uuid_t& uuid,
                       DebugDumpType dumpType);

    sdbusplus::object_path
        getDebugInfo(DebugInformationType debugInfoType,
                     sdbusplus::message::unix_fd fd) override;
    sdbusplus::object_path
        getDiagnostics(sdbusplus::message::unix_fd fd) override;

  private:
    void finish(AsyncOperationStatusType status, uint8_t rc);
    void getDebugInfoAsyncHandler(uint32_t recordHandle);
    requester::Coroutine
        getDebugInfoAsyncHandler(std::shared_ptr<Request> request);
    void getDiagnosticsAsyncHandler(uint32_t recordHandle);
    requester::Coroutine
        getDiagnosticsAsyncHandler(std::shared_ptr<Request> request);

    uuid_t uuid;
    nsm_debug_information_type infoType;
    std::shared_ptr<AsyncStatusIntf> statusInterface;
    std::shared_ptr<AsyncValueIntf> valueInterface;
    sdbusplus::message::unix_fd fd;
    std::vector<uint8_t> buffer;
};

} // namespace nsm
