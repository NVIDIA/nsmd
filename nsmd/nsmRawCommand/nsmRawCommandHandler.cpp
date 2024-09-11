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
#include "nsmRawCommandHandler.hpp"

#include "libnsm/requester/mctp.h"

#include "instance_id.hpp"
#include "sensorManager.hpp"
#include "socket_handler.hpp"
#include "utils.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <xyz/openbmc_project/Common/error.hpp>

#include <fstream>

namespace nsm::nsmRawCommand
{
NsmRawCommandHandler::NsmRawCommandHandler(sdbusplus::bus::bus& bus,
                                           const std::string& path,
                                           uint8_t eid) :
    sdbusplus::xyz::openbmc_project::NSM::server::NSMRawCommand(bus,
                                                                path.c_str()),
    bus(bus), commandStatusPath(path), commandFd(-1), eid(eid),
    completionCode(NSM_ERROR), reasonCode(ERR_NULL), commandResponse(-1)
{
    statusHandler = std::make_unique<
        sdbusplus::server::xyz::openbmc_project::nsm::NSMRawCommandStatus>(
        bus, path.c_str());
    statusHandler->status(
        sdbusplus::common::xyz::openbmc_project::nsm::NSMRawCommandStatus::
            SetOperationStatus::NoCommandInProgress);
    lg2::info("NSMRawCommandHandler initialized on path {PATH}", "PATH", path);
}

std::tuple<sdbusplus::message::object_path, uint8_t>
    NsmRawCommandHandler::sendNSMRawCommand(uint8_t messageType,
                                            uint8_t commandCode,
                                            sdbusplus::message::unix_fd data)
{
    uint8_t rc = static_cast<uint8_t>(NSM_SW_ERROR_COMMAND_FAIL);

    if (statusHandler->status() ==
        sdbusplus::common::xyz::openbmc_project::nsm::NSMRawCommandStatus::
            SetOperationStatus::CommandInProgress)
    {
        lg2::error(
            "NSMRawCommandHandler: Command already in progress, cannot proceed.");
        return std::make_tuple(
            sdbusplus::message::object_path(commandStatusPath), rc);
    }

    statusHandler->status(
        sdbusplus::common::xyz::openbmc_project::nsm::NSMRawCommandStatus::
            SetOperationStatus::CommandInProgress);

    int fd = static_cast<int>(data);

    std::vector<uint8_t> commandData = {};
    if (fd >= 0)
    {
        if (!readDataFromFileDescriptor(fd, commandData))
        {
            lg2::error(
                "NSMRawCommandHandler: Error reading from file descriptor {FD}",
                "FD", fd);
            statusHandler->status(
                sdbusplus::common::xyz::openbmc_project::nsm::
                    NSMRawCommandStatus::SetOperationStatus::InternalFailure);
            return std::make_tuple(
                sdbusplus::message::object_path(commandStatusPath), rc);
        }
    }
    issueNSMCommandAsync(messageType, commandCode, std::move(commandData))
        .detach();
    rc = static_cast<uint8_t>(NSM_SW_SUCCESS);
    return std::make_tuple(sdbusplus::message::object_path(commandStatusPath),
                           rc);
}

std::tuple<uint8_t, uint16_t, sdbusplus::message::unix_fd>
    NsmRawCommandHandler::getNSMCommandResponse()
{
    std::tuple<uint8_t, uint16_t, sdbusplus::message::unix_fd> ret_code(
        completionCode, reasonCode, commandResponse);
    return ret_code;
}

requester::Coroutine NsmRawCommandHandler::issueNSMCommandAsync(
    uint8_t messageType, uint8_t commandCode, std::vector<uint8_t> commandData)
{
    SensorManager& sensorManager = SensorManager::getInstance();

    Request request(sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_req) +
                    commandData.size());
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    int rc = encode_raw_cmd_req(messageType, commandCode, commandData.data(),
                                commandData.size(), requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NSMRawCommandHandler: NSM command encoding failed with error code {RC}",
            "RC", rc);
        reasonCode = ERR_INVALID_RQD;
        completionCode = NSM_ERR_INVALID_DATA;
        statusHandler->status(
            sdbusplus::common::xyz::openbmc_project::nsm::NSMRawCommandStatus::
                SetOperationStatus::InternalFailure);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;

    rc = co_await sensorManager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc)
    {
        lg2::error("NSMRawCommandHandler: NSM command failed with rc={RC}",
                   "RC", rc);
        reasonCode = ERR_INVALID_RQD;
        completionCode = NSM_ERR_INVALID_DATA;
        statusHandler->status(
            sdbusplus::common::xyz::openbmc_project::nsm::NSMRawCommandStatus::
                SetOperationStatus::InternalFailure);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    uint16_t data_size = 0;
    rc = decode_common_resp(responseMsg.get(), responseLen, &completionCode,
                            &data_size, &reasonCode);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NSMRawCommandHandler: NSM command response decoding failed with rc={RC} cc={CC}",
            "RC", rc, "CC", completionCode);
        statusHandler->status(
            sdbusplus::common::xyz::openbmc_project::nsm::NSMRawCommandStatus::
                SetOperationStatus::InternalFailure);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    saveResponseDataToFile(responseMsg.get(), responseLen);

    statusHandler->status(
        sdbusplus::common::xyz::openbmc_project::nsm::NSMRawCommandStatus::
            SetOperationStatus::CommandExecutionComplete);

    co_return NSM_SW_SUCCESS;
}

void NsmRawCommandHandler::saveResponseDataToFile(const nsm_msg* responseMsg,
                                                  size_t responseLen)
{
    // Deleting old response file if exists.
    const char* filename =
        ("/tmp/nsm_response_data" + std::to_string(eid) + ".bin").c_str();
    unlink(filename);

    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        lg2::error("NSMRawCommandHandler: Failed to open {FILE} for writing.",
                   "FILE", filename);
        return;
    }

    ssize_t bytesWritten = write(fd, responseMsg->payload,
                                 responseLen - sizeof(nsm_msg_hdr));
    if (bytesWritten == -1)
    {
        lg2::error("NSMRawCommandHandler: Failed to write to {FILE}", "FILE",
                   filename);
        close(fd);
        return;
    }

    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        lg2::error(
            "NSMRawCommandHandler: Failed to reset file cursor for {FILE}",
            "FILE", filename);
        close(fd);
        return;
    }

    commandResponse = fd;
    return;
}

bool NsmRawCommandHandler::readDataFromFileDescriptor(
    int fd, std::vector<uint8_t>& data)
{
    if (fd < 0)
    {
        // No data to read, empty file descriptor.
        return true;
    }

    constexpr size_t bufferSize = 1024;
    uint8_t buffer[bufferSize];
    ssize_t bytesRead;

    data.clear();

    while ((bytesRead = read(fd, buffer, bufferSize)) > 0)
    {
        data.insert(data.end(), buffer, buffer + bytesRead);
    }

    if (bytesRead < 0)
    {
        lg2::error(
            "NSMRawCommandHandler: Failed to read from file descriptor: {ERROR}",
            "ERROR", std::system_error(errno, std::generic_category()).what());
        return false;
    }

    // Data read successfully from file descriptor.
    return true;
}

} // namespace nsm::nsmRawCommand
