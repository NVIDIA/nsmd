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
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <com/nvidia/DebugToken/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/message/types.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nsm
{
using namespace sdbusplus::com::nvidia;
using namespace sdbusplus::common::xyz::openbmc_project::common;
using namespace sdbusplus::server;

using DebugTokenIntf = object_t<server::DebugToken>;

constexpr const auto successReasonCode = 0;
constexpr const auto tokenAlreadyActiveReasonCode = 1;

class NsmDebugTokenNICObject : public NsmObject, public DebugTokenIntf
{
  public:
    NsmDebugTokenNICObject(sdbusplus::bus_t& bus, const std::string& name,
                           const uuid_t& uuid);

    sdbusplus::object_path disableTokens();
    sdbusplus::object_path getRequest(DebugToken::TokenOpcodes tokenOpcode);
    sdbusplus::object_path getStatus(DebugToken::TokenTypes tokenType);
    sdbusplus::object_path installToken(std::vector<uint8_t> tokenData);

  private:
    requester::Coroutine
        disableTokensAsyncHandler(std::shared_ptr<Request> request,
                                  std::shared_ptr<AsyncStatusIntf> statusIntf,
                                  std::shared_ptr<AsyncValueIntf> valueIntf);
    requester::Coroutine
        getRequestAsyncHandler(std::shared_ptr<Request> request,
                               std::shared_ptr<AsyncStatusIntf> statusIntf,
                               std::shared_ptr<AsyncValueIntf> valueIntf);
    requester::Coroutine
        getStatusAsyncHandler(std::shared_ptr<Request> request,
                              std::shared_ptr<AsyncStatusIntf> statusIntf,
                              std::shared_ptr<AsyncValueIntf> valueIntf);
    requester::Coroutine
        installTokenAsyncHandler(std::shared_ptr<Request> request,
                                 std::shared_ptr<AsyncStatusIntf> statusIntf,
                                 std::shared_ptr<AsyncValueIntf> valueIntf);
    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice);

    uuid_t uuid;
};
} // namespace nsm
