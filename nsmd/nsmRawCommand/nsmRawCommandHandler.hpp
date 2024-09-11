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

#include "libnsm/base.h"
#include "libnsm/platform-environmental.h"

#include "common/types.hpp"
#include "common/utils.hpp"
#include "requester/handler.hpp"
#include "utils.hpp"

#include <boost/asio/awaitable.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/NSM/NSMRawCommand/server.hpp>
#include <xyz/openbmc_project/NSM/NSMRawCommandStatus/server.hpp>

namespace nsm::nsmRawCommand
{

class NsmRawCommandHandler :
    public sdbusplus::server::xyz::openbmc_project::nsm::NSMRawCommand
{
  public:
    NsmRawCommandHandler(sdbusplus::bus::bus& bus, const std::string& path,
                         uint8_t eid);

    std::tuple<sdbusplus::message::object_path, uint8_t>
        sendNSMRawCommand(uint8_t messageType, uint8_t commandCode,
                          sdbusplus::message::unix_fd data) override;

    std::tuple<uint8_t, uint16_t, sdbusplus::message::unix_fd>
        getNSMCommandResponse() override;

  private:
    std::unique_ptr<
        sdbusplus::server::xyz::openbmc_project::nsm::NSMRawCommandStatus>
        statusHandler;
    sdbusplus::bus::bus& bus;
    sdbusplus::message::object_path commandStatusPath;
    int commandFd; // File descriptor for command response
    uint8_t eid;
    uint8_t completionCode;
    uint16_t reasonCode;
    int mctpFd;
    sdbusplus::message::unix_fd commandResponse;
    requester::Coroutine issueNSMCommandAsync(uint8_t messageType,
                                              uint8_t commandCode,
                                              std::vector<uint8_t> commandData);
    bool readDataFromFileDescriptor(int fd, std::vector<uint8_t>& data);
    void saveResponseDataToFile(const nsm_msg* responseMsg, size_t responseLen);
};
} // namespace nsm::nsmRawCommand
