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

#include <com/nvidia/DebugToken/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/Progress/server.hpp>

#include <memory>

namespace nsm
{
using namespace sdbusplus::com::nvidia;
using namespace sdbusplus::common::xyz::openbmc_project::common;
using namespace sdbusplus::server;

using DebugTokenIntf = object_t<server::DebugToken>;
using ProgressIntf = object_t<Common::server::Progress>;

class NsmDebugTokenObject : public NsmObject, public DebugTokenIntf
{
  public:
    NsmDebugTokenObject(sdbusplus::bus::bus& bus, const std::string& name,
                        const std::vector<utils::Association>& associations,
                        const std::string& type, const uuid_t& uuid);

    void disableTokens();
    void getRequest(DebugToken::TokenOpcodes tokenOpcode);
    void getStatus(DebugToken::TokenTypes tokenType);
    void installToken(std::vector<uint8_t> tokenData);

  private:
    static std::string getParentChassisPath(
        const std::vector<utils::Association>& associations);
    static std::string
        getName(const std::vector<utils::Association>& associations,
                const std::string& name);

    int startOperation();
    void finishOperation(Progress::OperationStatus status);
    requester::Coroutine
        disableTokensAsyncHandler(std::shared_ptr<Request> request);
    requester::Coroutine
        getRequestAsyncHandler(std::shared_ptr<Request> request);
    requester::Coroutine
        getStatusAsyncHandler(std::shared_ptr<Request> request);
    requester::Coroutine
        installTokenAsyncHandler(std::shared_ptr<Request> request);
    requester::Coroutine update(SensorManager& manager, eid_t eid);

    std::unique_ptr<ProgressIntf> progressIntf = nullptr;

    uuid_t uuid;

    bool opInProgress{false};
};
} // namespace nsm
