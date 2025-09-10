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
#include "types.hpp"

#include <com/nvidia/DebugToken/Action/server.hpp>
#include <com/nvidia/DebugToken/Status/server.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace nsm
{
using DebugTokenActionIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::debug_token::Action>;
using DebugTokenStatusIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::debug_token::Status>;

/**
 * @brief Object that provides debug token management functionality based on the
 * unified debug token API.
 *
 * This class implements the NSM debug token functionality by inheriting from
 * both com.nvidia.DebugToken.Action and com.nvidia.DebugToken.Status
 * interfaces. It provides methods to install and erase debug tokens. It is also
 * responsible for automatically querying token status, and acquiring device ID
 * and maximum token installation buffer size. The class handles asynchronous
 * operations for token management and maintains state information about
 * installed tokens.
 */
class NsmDebugTokenUnifiedObject :
    public NsmObject,
    public DebugTokenActionIntf,
    public DebugTokenStatusIntf
{
  public:
    /**
     * @brief Constructs a new NsmDebugTokenUnifiedObject
     *
     * @param bus D-Bus bus interface for communication
     * @param name Object name for the debug token object instance
     * @param uuid Device UUID associated with this debug token object
     */
    NsmDebugTokenUnifiedObject(sdbusplus::bus::bus& bus,
                               const std::string& name, const uuid_t& uuid);

    /**
     * @brief Erases a debug token of the specified type
     *
     * Initiates an asynchronous operation to erase a debug token from the
     * device. The operation is performed asynchronously and returns immediately
     * with an object path for monitoring the operation status.
     *
     * @param tokenType Type of the token to erase
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::message::object_path eraseToken(uint32_t tokenType);

    /**
     * @brief Installs a debug token from a file descriptor
     *
     * Initiates an asynchronous operation to install a debug token from the
     * provided file descriptor. The token is installed in chunks to handle
     * large files. The operation returns immediately with an object path for
     * monitoring status.
     *
     * @param fd File descriptor containing the debug token data
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::InternalFailure if file operations fail
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::message::object_path
        installToken(sdbusplus::message::unix_fd fd);

  private:
    /**
     * @brief Information structure for tracking token installation progress
     *
     * This structure holds the file descriptor and metadata needed for
     * chunked token installation. It manages the file descriptor lifecycle
     * and tracks installation progress through offset tracking.
     */
    struct TokenInstallationInfo
    {
        TokenInstallationInfo() = delete;
        TokenInstallationInfo(int fd, size_t totalSize) :
            fd(fd), totalSize(totalSize)
        {}
        TokenInstallationInfo(TokenInstallationInfo&& other) = delete;
        TokenInstallationInfo& operator=(TokenInstallationInfo&&) = delete;
        TokenInstallationInfo(const TokenInstallationInfo&) = delete;
        TokenInstallationInfo& operator=(const TokenInstallationInfo&) = delete;

        ~TokenInstallationInfo()
        {
            if (fd >= 0)
            {
                close(fd);
            }
        }

        int fd{-1};
        size_t offset{0};
        size_t totalSize{0};
    };

    /**
     * @brief Asynchronous handler for token erasure operations
     *
     * Handles the actual token erasure by communicating with the NSM device.
     * Updates the async operation status and value interfaces based on the
     * operation result.
     *
     * @param tokenType Type of token to erase
     * @param statusIntf Interface for reporting operation status
     * @param valueIntf Interface for reporting operation result
     * @return Coroutine that completes when the operation finishes
     */
    requester::Coroutine
        eraseTokenAsyncHandler(uint32_t tokenType,
                               std::shared_ptr<AsyncStatusIntf> statusIntf,
                               std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Creates a request for installing a token chunk
     *
     * Reads a chunk of data from the token file and creates an NSM request
     * for installing that chunk. Handles file reading and request encoding.
     *
     * @param info Token installation information containing file descriptor and
     * progress
     * @return Optional request object if successful, nullopt on failure
     */
    std::optional<Request>
        createInstallTokenRequest(std::shared_ptr<TokenInstallationInfo> info);

    /**
     * @brief Asynchronous handler for token installation operations
     *
     * Handles the actual token installation by sending chunks to the NSM
     * device. Manages chunked installation for large token files and updates
     * progress.
     *
     * @param info Token installation information with file descriptor and
     * progress
     * @param statusIntf Interface for reporting operation status
     * @param valueIntf Interface for reporting operation result
     * @return Coroutine that completes when the operation finishes
     */
    requester::Coroutine
        installTokenAsyncHandler(std::shared_ptr<TokenInstallationInfo> info,
                                 std::shared_ptr<AsyncStatusIntf> statusIntf,
                                 std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Queries the current token status from the device
     *
     * Sends a query request to the NSM device to retrieve current token
     * installation status, processing status, and token type information.
     * Updates the D-Bus properties with the retrieved information.
     *
     * @param nsmDevice NSM device to query
     * @return Coroutine that completes when the query finishes
     */
    requester::Coroutine
        queryTokenHandler(std::shared_ptr<NsmDevice> nsmDevice);

    /**
     * @brief Queries device capabilities to determine chunk size
     *
     * Retrieves device capabilities including maximum input buffer size
     * to calculate the optimal chunk size for token installation.
     *
     * @param nsmDevice NSM device to query
     * @return Coroutine that completes when the query finishes
     */
    requester::Coroutine
        deviceCapabilitiesHandler(std::shared_ptr<NsmDevice> nsmDevice);

    /**
     * @brief Queries device ID information
     *
     * Retrieves the device ID from the NSM device and formats it as a
     * hexadecimal string for display purposes.
     *
     * @param nsmDevice NSM device to query
     * @return Coroutine that completes when the query finishes
     */
    requester::Coroutine deviceIdHandler(std::shared_ptr<NsmDevice> nsmDevice);

    /**
     * @brief Updates all device information and token status
     *
     * Performs a comprehensive update by querying device ID, capabilities,
     * and token status. This method coordinates multiple queries to ensure
     * all information is current.
     *
     * @param nsmDevice NSM device to update
     * @return Coroutine that completes when all updates finish
     */
    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice);

    uuid_t uuid;
    size_t installationChunkSize{0};
};
} // namespace nsm
