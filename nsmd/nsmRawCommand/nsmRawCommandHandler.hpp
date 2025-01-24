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
#include "nsmLongRunningEvent.hpp"
#include "timer.hpp"

#include <com/nvidia/Protocol/NSM/Raw/server.hpp>

namespace nsm
{

using NsmRawIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::Protocol::NSM::server::Raw>;

class NsmRawLongRunningEventHandler : public NsmLongRunningEvent
{
  public:
    NsmRawLongRunningEventHandler(const std::string& name,
                                  const std::string& type, bool isLongRunning) :
        NsmLongRunningEvent(name, type, isLongRunning)
    {}

    int handle(eid_t eid, NsmType type, NsmEventId eventId,
               const nsm_msg* event, size_t eventLen) override;
    bool isLongRunnningEventValidated = true;
    std::vector<uint8_t> longRunningEventData;
    uint8_t longRunningRc;
    uint8_t acceptInstanceId = 0xFF;
};

class NsmRawCommandHandler : public NsmRawIntf
{
  private:
    static NsmRawCommandHandler* instance;
    NsmRawCommandHandler(sdbusplus::bus::bus& bus, const char* path);
    requester::Coroutine
        doSendRequest(uint8_t deviceType, uint8_t instanceId,
                      uint8_t messageType, uint8_t commandCode,
                      int duplicateFdHandle,
                      std::shared_ptr<AsyncStatusIntf> statusInterface,
                      std::shared_ptr<AsyncValueIntf> valueInterface);
    requester::Coroutine doSendLongRunningRequest(
        uint8_t deviceType, uint8_t instanceId, bool isLongRunning,
        uint8_t messageType, uint8_t commandCode, int duplicateFdHandle,
        std::shared_ptr<AsyncStatusIntf> statusInterface,
        std::shared_ptr<AsyncValueIntf> valueInterface);

  public:
    sdbusplus::message::object_path
        sendRequest(uint8_t deviceType, uint8_t instanceId, bool isLongRunning,
                    uint8_t messageType, uint8_t commandCode,
                    sdbusplus::message::unix_fd fd) override;

    static void initialize(sdbusplus::bus::bus& bus, const char* path);
    static NsmRawCommandHandler& getInstance();
};

} // namespace nsm
