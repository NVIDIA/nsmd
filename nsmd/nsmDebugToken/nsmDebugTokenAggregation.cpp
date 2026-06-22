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

#include "nsmDebugTokenAggregation.hpp"

#include "common/globals.hpp"
#include "common/utils.hpp"
#include "nsmDebugTokenUnified.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "sensorManager.hpp"

#include <debug-token/tlv.h>
#include <debug-token/types.h>
#include <endian.h>
#include <fcntl.h>
#include <libnsm/diagnostics.h>
#include <sys/mman.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <fstream>

namespace nsm
{

DebugTokenAggregationManager* DebugTokenAggregationManager::getInstance()
{
    static std::unique_ptr<DebugTokenAggregationManager> instance{
        new DebugTokenAggregationManager()};
    return instance.get();
}

DebugTokenAggregationManager::DebugTokenAggregationManager()
{
    auto& bus = utils::DBusHandler::getBus();
    aggregationObject = std::make_shared<NsmDebugTokenAggregationObject>(
        bus, std::string(debugTokenObjectBasePath));
    lg2::info("DebugToken: Created aggregation manager and object at {PATH}",
              "PATH", debugTokenObjectBasePath);
}

NsmDebugTokenAggregationObject::NsmDebugTokenAggregationObject(
    sdbusplus::bus_t& bus, const std::string& path) :
    DebugTokenAggregationIntf(bus, path.c_str())
{
    lg2::info("DebugToken: Created Aggregation object at: {PATH}", "PATH",
              path);
}

sdbusplus::object_path
    NsmDebugTokenAggregationObject::installToken(sdbusplus::message::unix_fd fd)
{
    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    TokenMap tokens;
    auto parseRc = parseTokenFile(fd, tokens);

    if (parseRc != 0 || tokens.empty())
    {
        lg2::error("DebugToken: Failed to parse aggregated token file");
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InvalidArgument();
    }

    lg2::info("DebugToken: Parsed {COUNT} tokens from aggregated file", "COUNT",
              tokens.size());

    installTokensToDevices(std::move(tokens), statusIntf, valueIntf).detach();
    return objPath;
}

int NsmDebugTokenAggregationObject::parseHeader(int fd,
                                                DebugTokenHeader& header)
{
    lseek(fd, 0, SEEK_SET);
    auto readBytes = read(fd, &header, sizeof(DebugTokenHeader));
    if (readBytes != sizeof(DebugTokenHeader))
    {
        lg2::error("DebugToken: Failed to read file header");
        return -1;
    }

    if (header.type != FileTypeDebugToken)
    {
        lg2::error("DebugToken: Invalid file type: {TYPE}", "TYPE",
                   header.type);
        return -1;
    }

    uint16_t numRecords = header.numberOfRecords;
    uint32_t fileSize = header.fileSize;
    lg2::debug(
        "DebugToken: Header parsed - version={VER}, records={REC}, size={SIZE}",
        "VER", header.version, "REC", numRecords, "SIZE", fileSize);

    return 0;
}

int NsmDebugTokenAggregationObject::parseTokenFile(int fd, TokenMap& tokens)
{
    DebugTokenHeader header{};
    if (parseHeader(fd, header) != 0)
    {
        return -1;
    }

    struct stat fileStat;
    if (fstat(fd, &fileStat) < 0)
    {
        lg2::error("DebugToken: fstat failed: {ERR}", "ERR", strerror(errno));
        return -1;
    }

    std::vector<uint8_t> fileData(fileStat.st_size);
    lseek(fd, 0, SEEK_SET);
    auto readBytes = read(fd, fileData.data(), fileStat.st_size);
    if (readBytes != fileStat.st_size)
    {
        lg2::error("DebugToken: Failed to read file data");
        return -1;
    }

    return parseTlvTokens(fileData, header, tokens);
}

std::string NsmDebugTokenAggregationObject::extractSerialNumber(
    const std::vector<uint8_t>& tlvData)
{
    try
    {
        debug_token::tlv_decoder::Structure tlvStructure(tlvData);
        const auto& serialItem =
            tlvStructure.get(debug_token::types::Common::DeviceSerialNumber);
        auto serialBytes = serialItem.getValue<std::vector<uint8_t>>();

        std::stringstream ss;
        ss << "0x" << std::hex << std::uppercase;
        for (const auto& byte : serialBytes)
        {
            ss << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        return ss.str();
    }
    catch (const std::exception& e)
    {
        lg2::warning("DebugToken: Failed to extract serial number: {ERR}",
                     "ERR", e.what());
        return "";
    }
}

int NsmDebugTokenAggregationObject::parseTlvTokens(
    const std::vector<uint8_t>& fileData, const DebugTokenHeader& header,
    TokenMap& tokens)
{
    auto records = token_utils::parseTokenRecords(fileData, header);

    for (size_t i = 0; i < records.size(); ++i)
    {
        const auto& rec = records[i];
        std::vector<uint8_t> singleTlvData(rec.begin(), rec.end());

        std::string serialNumber = extractSerialNumber(singleTlvData);
        if (serialNumber.empty())
        {
            lg2::warning(
                "DebugToken: Token {INDEX} missing serial number, skipping",
                "INDEX", i);
            continue;
        }

        lg2::debug("DebugToken: Parsed token {INDEX} for serial {SERIAL}",
                   "INDEX", i, "SERIAL", serialNumber);
        tokens.emplace(serialNumber, singleTlvData);
    }

    if (records.size() < header.numberOfRecords)
    {
        lg2::error(
            "DebugToken: truncated multi-record file, parsed {N} of {TOTAL}",
            "N", records.size(), "TOTAL",
            static_cast<uint16_t>(header.numberOfRecords));
        return -1;
    }

    if (tokens.empty())
    {
        lg2::error("DebugToken: No valid tokens found in file");
        return -1;
    }

    lg2::info("DebugToken: Successfully parsed {COUNT} TLV token(s)", "COUNT",
              tokens.size());
    return 0;
}

requester::Coroutine NsmDebugTokenAggregationObject::installTokensToDevices(
    TokenMap tokens, std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    AggregatedResults results;
    size_t successCount = 0;
    size_t failureCount = 0;

    lg2::info("DebugToken: Installing tokens to devices (found {COUNT} tokens)",
              "COUNT", tokens.size());

    auto& sensorManager = SensorManager::getInstance();

    std::map<std::string, std::shared_ptr<NsmDebugTokenUnifiedObject>>
        deviceMap;

    for (const auto& tokenObject : sensorManager.debugTokenList)
    {
        if (!tokenObject)
        {
            continue;
        }

        std::string deviceSerial = tokenObject->tokenDeviceID();
        if (deviceSerial.empty())
        {
            lg2::debug(
                "DebugToken: Token object has empty TokenDeviceID, skipping");
            continue;
        }

        lg2::debug("DebugToken: Found debug token object with serial {SERIAL}",
                   "SERIAL", deviceSerial);

        deviceMap.emplace(deviceSerial, tokenObject);
    }

    lg2::info(
        "DebugToken: Found {COUNT} debug token objects with serial numbers",
        "COUNT", deviceMap.size());

    for (const auto& [tokenSerial, tokenData] : tokens)
    {
        lg2::info("DebugToken: Processing token for serial {SERIAL}", "SERIAL",
                  tokenSerial);

        auto deviceIt = deviceMap.find(tokenSerial);
        if (deviceIt == deviceMap.end())
        {
            lg2::warning(
                "DebugToken: No device found with serial {SERIAL}, ignoring unmatched token",
                "SERIAL", tokenSerial);
            continue;
        }

        auto tokenObject = deviceIt->second;
        std::string deviceName = tokenObject->getName();

        lg2::info(
            "DebugToken: Found tokenObject for serial {SERIAL}, device name={NAME}",
            "SERIAL", tokenSerial, "NAME", deviceName);
        lg2::info("DebugToken: TokenObject TokenDeviceID={DEVID}", "DEVID",
                  tokenObject->tokenDeviceID());
        lg2::info(
            "DebugToken: Installing token for device {NAME} with serial {SERIAL}",
            "NAME", deviceName, "SERIAL", tokenSerial);

        uint16_t errorCode = 0;
        std::string errorMessage;

        auto rc = co_await installTokenToDevice(
            tokenObject, tokenData, deviceName, errorCode, errorMessage);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "DebugToken: installTokenToDevice returned rc={RC} for device {NAME}",
                "RC", rc, "NAME", deviceName);
        }

        results.emplace_back(errorCode, errorMessage, deviceName);

        if (errorCode == 0)
        {
            successCount++;
        }
        else
        {
            failureCount++;
        }
    }

    size_t unmatchedTokens =
        tokens.size() > results.size() ? tokens.size() - results.size() : 0;
    lg2::info(
        "DebugToken: Installation complete - success={SUCCESS}, "
        "failure={FAILURE}, tokens_processed={TOTAL}, unmatched_tokens={UNMATCHED}",
        "SUCCESS", successCount, "FAILURE", failureCount, "TOTAL",
        results.size(), "UNMATCHED", unmatchedTokens);

    valueIntf->value(results);
    if (failureCount > 0)
    {
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
    }
    else
    {
        statusIntf->status(AsyncOperationStatusType::Success);
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDebugTokenAggregationObject::installTokenToDevice(
    std::shared_ptr<NsmDebugTokenUnifiedObject> tokenObject,
    const std::vector<uint8_t>& tokenData,
    [[maybe_unused]] const std::string& deviceName, uint16_t& errorCode,
    std::string& errorMessage)
{
    int memFd = memfd_create("debug_token", 0);
    if (memFd < 0)
    {
        lg2::error("DebugToken: memfd_create failed: {ERR}", "ERR",
                   strerror(errno));
        errorCode = 1;
        errorMessage = "Failed to create memfd";
        co_return NSM_SW_ERROR;
    }

    ssize_t written = write(memFd, tokenData.data(), tokenData.size());
    if (written != static_cast<ssize_t>(tokenData.size()))
    {
        lg2::error("DebugToken: write to memfd failed: {ERR}", "ERR",
                   strerror(errno));
        close(memFd);
        errorCode = 1;
        errorMessage = "Failed to write to memfd";
        co_return NSM_SW_ERROR;
    }

    lseek(memFd, 0, SEEK_SET);

    auto installRc = co_await tokenObject->installTokenDirect(
        memFd, tokenData.size(), errorCode, errorMessage);

    co_return installRc;
}

} // namespace nsm
