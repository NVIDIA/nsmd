/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include <com/nvidia/DebugToken/Aggregation/server.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace nsm
{

// Forward declarations
class NsmDebugTokenUnifiedObject;
class NsmDebugTokenAggregationObject;

using DebugTokenAggregationIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::debug_token::Aggregation>;

using token_utils::DebugTokenHeader;
using token_utils::FileTypeDebugToken;

/**
 * @brief Singleton manager for debug token aggregation
 *
 * This class manages a single aggregation object instance that handles
 * multi-device debug token installation. Uses the singleton pattern to
 * ensure only one aggregation object exists across the application.
 */
class DebugTokenAggregationManager
{
  public:
    /**
     * @brief Get the singleton instance
     * @return Pointer to the singleton instance, or nullptr if not initialized
     */
    static DebugTokenAggregationManager* getInstance();

    /**
     * @brief Get the aggregation object
     * @return Shared pointer to the aggregation object
     */
    std::shared_ptr<NsmDebugTokenAggregationObject> getAggregationObject() const
    {
        return aggregationObject;
    }

  private:
    DebugTokenAggregationManager();
    std::shared_ptr<NsmDebugTokenAggregationObject> aggregationObject;
};

/**
 * @brief Object that provides aggregated debug token installation functionality
 *
 * This class implements the com.nvidia.DebugToken.Aggregation interface to
 * support installing debug tokens to multiple NSM devices simultaneously.
 * It parses the aggregated token file containing TLV-encoded tokens for
 * multiple devices and installs each token to the corresponding device based
 * on serial number matching.
 *
 * The object is created at the common path /xyz/openbmc_project/debug_token
 * rather than per-device paths, as it handles multi-device operations.
 */
class NsmDebugTokenAggregationObject : public DebugTokenAggregationIntf
{
  public:
    /**
     * @brief Constructs a new NsmDebugTokenAggregationObject
     *
     * @param bus D-Bus bus interface for communication
     * @param path D-Bus object path (should be base debug token path)
     */
    NsmDebugTokenAggregationObject(sdbusplus::bus::bus& bus,
                                   const std::string& path);

    /**
     * @brief Installs debug tokens to multiple devices
     *
     * Parses the aggregated token file containing TLV-encoded tokens for
     * multiple devices. Extracts serial numbers from each token and matches
     * them to devices. Installs each token to the corresponding device.
     *
     * @param fd File descriptor containing the aggregated token file
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::InternalFailure if file parsing fails
     * @throws Common::Error::InvalidArgument if file format is invalid
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::message::object_path
        installToken(sdbusplus::message::unix_fd fd) override;

  private:
    /**
     * @brief Type alias for serial number to token data mapping
     */
    using SerialNumber = std::string;
    using TokenData = std::vector<uint8_t>;
    using TokenMap = std::multimap<SerialNumber, TokenData>;

    /**
     * @brief Result type for per-device installation status
     * Tuple format: (error_code, error_message, device_name)
     */
    using DeviceResult = std::tuple<uint16_t, std::string, std::string>;
    using AggregatedResults = std::vector<DeviceResult>;

    /**
     * @brief Parse the aggregated token file
     *
     * Reads the file from the file descriptor and parses the header and
     * individual TLV tokens. Extracts serial numbers and token data.
     *
     * @param fd File descriptor to read from
     * @param tokens Output map of serial number to token data
     * @return int 0 on success, -1 on failure
     */
    int parseTokenFile(int fd, TokenMap& tokens);

    /**
     * @brief Parse file header
     *
     * Reads and validates the DebugTokenHeader structure from the file.
     *
     * @param fd File descriptor to read from
     * @param header Output header structure
     * @return int 0 on success, -1 on failure
     */
    int parseHeader(int fd, DebugTokenHeader& header);

    /**
     * @brief Parse TLV tokens from file data
     *
     * Extracts individual TLV-encoded tokens from the file data and
     * matches them to serial numbers.
     *
     * @param fileData Complete file data
     * @param header Token file header
     * @param tokens Output map of serial number to token data
     * @return int 0 on success, -1 on failure
     */
    int parseTlvTokens(const std::vector<uint8_t>& fileData,
                       const DebugTokenHeader& header, TokenMap& tokens);

    /**
     * @brief Extract serial number from TLV structure
     *
     * Parses the TLV structure to extract the device serial number.
     *
     * @param tlvData TLV-encoded token data
     * @return std::string Serial number in hex format, empty if not found
     */
    std::string extractSerialNumber(const std::vector<uint8_t>& tlvData);

    /**
     * @brief Match tokens to devices and install
     *
     * Iterates through all discovered NSM devices, matches them to tokens
     * by serial number, and installs the tokens asynchronously.
     *
     * @param tokens Map of serial number to token data
     * @param statusIntf Interface for reporting operation status
     * @param valueIntf Interface for reporting operation results
     * @return Coroutine that completes when all installations finish
     */
    requester::Coroutine
        installTokensToDevices(TokenMap tokens,
                               std::shared_ptr<AsyncStatusIntf> statusIntf,
                               std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Install token to a single device
     *
     * Installs a token to a specific NSM device using the existing unified
     * token object's install logic. Creates a memfd and delegates to
     * NsmDebugTokenUnifiedObject::installTokenDirect().
     *
     * @param tokenObject Unified debug token object for the device
     * @param tokenData Token data to install
     * @param deviceName Device name for result reporting
     * @param[out] errorCode Error code result (0 = success)
     * @param[out] errorMessage Error message result
     * @return Coroutine that completes when installation finishes
     */
    requester::Coroutine installTokenToDevice(
        std::shared_ptr<NsmDebugTokenUnifiedObject> tokenObject,
        const std::vector<uint8_t>& tokenData, const std::string& deviceName,
        uint16_t& errorCode, std::string& errorMessage);
};

} // namespace nsm
