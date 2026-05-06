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

#include "asyncOperationManager.hpp"

#include <com/nvidia/Dump/LogInfo/server.hpp>

#include <memory>

namespace nsm
{
using namespace sdbusplus::com::nvidia;
using namespace sdbusplus::server;

using LogInfoIntf = object_t<sdbusplus::com::nvidia::Dump::server::LogInfo>;

using TimeSyncFrom =
    sdbusplus::common::com::nvidia::dump::LogInfo::TimeSyncFrom;

class NsmLogInfoObject : public NsmObject, public LogInfoIntf
{
  public:
    NsmLogInfoObject(sdbusplus::bus_t& bus, const std::string& name,
                     const std::string& inventoryPath, const std::string& type,
                     const uuid_t& uuid);

    sdbusplus::object_path getLogInfo(sdbusplus::message::unix_fd fd) override;

  private:
    void finish(AsyncOperationStatusType status, uint64_t packedError);
    void getLogInfoAsyncHandler(uint32_t recordHandle);
    requester::Coroutine
        getLogInfoAsyncHandler(std::shared_ptr<Request> request);

    uuid_t uuid;
    std::shared_ptr<AsyncStatusIntf> statusInterface;
    std::shared_ptr<AsyncValueIntf> valueInterface;
    sdbusplus::message::unix_fd fd;
    std::vector<uint8_t> buffer;
    // Record handle last sent on the wire; the stuck-loop guard aborts the
    // fetch if the device echoes it back as the next handle.
    uint32_t currentLogInfoHandle{0};
};
} // namespace nsm
