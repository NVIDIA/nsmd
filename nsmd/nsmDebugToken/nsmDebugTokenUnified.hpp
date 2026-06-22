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
#include "debugTokenUtils.hpp"
#include "nsmObjectFactory.hpp"
#include "types.hpp"

#include <com/nvidia/DebugToken/Action/server.hpp>
#include <com/nvidia/DebugToken/Common/server.hpp>
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
     * @param debugTokenDeviceType Device type from Entity Manager configuration
     */
    NsmDebugTokenUnifiedObject(sdbusplus::bus_t& bus, const std::string& name,
                               const uuid_t& uuid,
                               const std::string& debugTokenDeviceType);

    /**
     * @brief Erases a debug token of the specified type
     *
     * Initiates an asynchronous operation to erase a debug token from the
     * device. The operation is performed asynchronously and returns immediately
     * with an object path for monitoring the operation status.
     *
     * @param eraseType Type of erase operation (enum)
     * @param tokenType Type of the token to erase (enum), used when eraseType
     * is TokenType
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::object_path eraseToken(
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType
            eraseType,
        sdbusplus::server::com::nvidia::debug_token::Common::Types tokenType)
        override;

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
    sdbusplus::object_path installToken(sdbusplus::message::unix_fd fd);

    /**
     * @brief Installs a debug token from file descriptor for aggregation
     *
     * Installs a debug token from a file descriptor for aggregation operations.
     * Uses internal structures to track progress and updates token status.
     * Takes ownership of the fd. Returns result via output parameters.
     *
     * @param fd File descriptor containing the debug token data (will be
     * closed)
     * @param totalSize Total size of the token data
     * @param[out] errorCode Error code result (0 = success)
     * @param[out] errorMessage Error message result
     * @return Coroutine that completes when installation finishes
     */
    requester::Coroutine installTokenDirect(int fd, size_t totalSize,
                                            uint16_t& errorCode,
                                            std::string& errorMessage);

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
     * @brief Installs a multi-record debug token file
     *
     * Parses the TLV records contained in a DebugTokenHeader multi-record
     * container and installs each record sequentially via installTokenDirect().
     * The file descriptor in @p info is expected to be positioned at the start
     * of the file. Aggregate status is reported through the supplied async
     * interfaces (full success, all records failed, or partial success).
     *
     * @param info Token installation information with file descriptor and size
     * @param header Parsed multi-record file header
     * @param statusIntf Interface for reporting operation status
     * @param valueIntf Interface for reporting operation result
     * @return Coroutine that completes when the operation finishes
     */
    requester::Coroutine
        installMultiRecordToken(std::shared_ptr<TokenInstallationInfo> info,
                                const token_utils::DebugTokenHeader& header,
                                std::shared_ptr<AsyncStatusIntf> statusIntf,
                                std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Asynchronous handler for single-record token installation
     *
     * Streams a single-record (TLV) token file to the NSM device one chunk
     * per invocation, recursing (via detach) for the next chunk until the
     * entire file has been sent. The single- vs multi-record decision is made
     * up front in installToken(); this handler only ever streams a
     * single-record token. Progress and result are reported through the
     * supplied async interfaces.
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
    std::string deviceTypeStr; // Device type string for token mapping
};
} // namespace nsm
