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

#include "nsmDebugTokenUnified.hpp"

#include "debug-token.h"
#include "debug-token/error.h"
#include "debug-token/tlv.h"
#include "debug-token/types.h"

#include "debugTokenUtils.hpp"
#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmSensor.hpp"

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <cstring>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <sstream>
#include <tuple>
#include <vector>

namespace nsm
{

NsmDebugTokenUnifiedObject::NsmDebugTokenUnifiedObject(
    sdbusplus::bus::bus& bus, const std::string& name, const uuid_t& uuid,
    const std::string& debugTokenDeviceType) :
    NsmObject(name, "NSM_DebugTokenUnified"),
    DebugTokenActionIntf(bus, (debugTokenObjectBasePath / name).c_str()),
    DebugTokenStatusIntf(bus, (debugTokenObjectBasePath / name).c_str()),
    uuid(uuid)
{
    lg2::info("DebugToken: create Unified object: {PATH}", "PATH",
              debugTokenObjectBasePath / name);

    this->deviceTypeStr = debugTokenDeviceType;
}

requester::Coroutine NsmDebugTokenUnifiedObject::eraseTokenAsyncHandler(
    uint32_t tokenType, std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();
    auto request = std::make_shared<Request>(sizeof(nsm_msg_hdr) +
                                             sizeof(nsm_erase_token_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_erase_token_req(0, tokenType, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: encode_nsm_erase_token_req: rc={RC}", "RC", rc);
        auto error = std::make_tuple(static_cast<uint16_t>(rc),
                                     std::format("Operation failed: {}", rc));
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        LG2_ERROR_FLT("DebugToken: eraseToken postPatchIO failed "
                      "| rc: {RC}, eid: {EID}",
                      "RC", nsm_sw_codes(sendRc), "EID", eid);
        if (sendRc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            auto error = std::make_tuple(static_cast<uint16_t>(sendRc),
                                         std::format("Unsupported command: {}",
                                                     static_cast<int>(sendRc)));
            valueIntf->value(error);
            statusIntf->status(AsyncOperationStatusType::UnsupportedRequest);
        }
        else
        {
            auto error = std::make_tuple(
                static_cast<uint16_t>(sendRc),
                std::format("Operation failed: {}", static_cast<int>(sendRc)));
            valueIntf->value(error);
            statusIntf->status(AsyncOperationStatusType::WriteFailure);
        }
        // coverity[missing_return]
        co_return sendRc;
    }
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    auto decodeRc = decode_nsm_erase_token_resp(responseMsg.get(), responseLen,
                                                &cc, &reasonCode);
    if (decodeRc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: decode_nsm_erase_token_resp: "
                   "eid={EID} rc={RC} cc={CC} len={LEN}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "LEN", responseLen);
        auto error =
            std::make_tuple(static_cast<uint16_t>(decodeRc),
                            std::format("Operation failed: {}", decodeRc));
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        // coverity[missing_return]
        co_return decodeRc;
    }
    if (cc == NSM_SUCCESS)
    {
        valueIntf->value(std::make_tuple(static_cast<uint16_t>(cc), "Success"));
        statusIntf->status(AsyncOperationStatusType::Success);
        queryTokenHandler(device).detach();
    }
    else
    {
        uint16_t errorCode =
            (reasonCode != ERR_NULL) ? reasonCode : static_cast<uint16_t>(cc);
        auto error = std::make_tuple(
            errorCode,
            std::format("{}: {}", debug_token::Error(errorCode).to_string(),
                        errorCode));
        lg2::error("DebugToken: eraseToken: eid={EID} cc={CC} rc={RC}", "EID",
                   eid, "CC", cc, "RC", reasonCode);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

std::optional<Request> NsmDebugTokenUnifiedObject::createInstallTokenRequest(
    std::shared_ptr<TokenInstallationInfo> info)
{
    if (info->fd < 0)
    {
        lg2::error("DebugToken: Invalid file descriptor: {FD}", "FD", info->fd);
        return std::nullopt;
    }

    size_t bytesToRead = std::min(info->totalSize - info->offset,
                                  installationChunkSize);

    std::unique_ptr<uint8_t[]> buffer(new uint8_t[bytesToRead]);
    auto readBytes = read(info->fd, buffer.get(), bytesToRead);
    if (readBytes <= 0)
    {
        lg2::error("DebugToken: read failed: {ERR}", "ERR",
                   readBytes == 0 ? "unexpected EOF" : strerror(errno));
        return std::nullopt;
    }
    uint32_t lengthRemaining = info->totalSize - info->offset - readBytes;
    auto request = Request(sizeof(nsm_msg_hdr) + sizeof(nsm_install_token_req) -
                           1 + readBytes);
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto encodeRc = encode_nsm_install_token_req(
        0, info->offset, readBytes, lengthRemaining, buffer.get(), requestMsg);
    if (encodeRc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: encode_nsm_install_token_req: rc={RC}", "RC",
                   encodeRc);
        return std::nullopt;
    }

    info->offset += readBytes;
    return request;
}

requester::Coroutine NsmDebugTokenUnifiedObject::installMultiRecordToken(
    std::shared_ptr<TokenInstallationInfo> info,
    const token_utils::DebugTokenHeader& header,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    // Bound the in-memory copy by the on-wire header.fileSize against the
    // actual file size, so a malicious or accidentally-large input cannot
    // cause us to allocate more than the caller supplied. Anything that fails
    // these checks is rejected before we touch the heap.
    const size_t headerFileSize = header.fileSize;
    if (headerFileSize <= sizeof(token_utils::DebugTokenHeader) ||
        headerFileSize > info->totalSize)
    {
        lg2::error(
            "DebugToken: rejecting multi-record file: header.fileSize={HSZ} fdSize={FSZ}",
            "HSZ", static_cast<uint64_t>(headerFileSize), "FSZ",
            static_cast<uint64_t>(info->totalSize));
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(NSM_SW_ERROR),
                            std::format("Operation failed: {}",
                                        static_cast<int>(NSM_SW_ERROR))));
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    std::vector<uint8_t> data(headerFileSize);
    auto fileBytes = read(info->fd, data.data(), headerFileSize);
    if (fileBytes != static_cast<ssize_t>(headerFileSize))
    {
        lg2::error(
            "DebugToken: failed to read multi-record token file: bytes={N} expected={EXP} {ERR}",
            "N", static_cast<int64_t>(fileBytes), "EXP",
            static_cast<uint64_t>(headerFileSize), "ERR",
            fileBytes < 0 ? strerror(errno) : "short read");
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(NSM_SW_ERROR),
                            std::format("Operation failed: {}",
                                        static_cast<int>(NSM_SW_ERROR))));
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    auto records = token_utils::parseTokenRecords(data, header);

    const uint16_t totalRecords = header.numberOfRecords;
    const uint16_t parsedRecords = static_cast<uint16_t>(records.size());
    uint16_t okCount = 0;
    uint16_t failCount = static_cast<uint16_t>(totalRecords - parsedRecords);
    uint16_t statusCode = 1;
    std::string statusMsg;
    if (failCount > 0)
    {
        lg2::error(
            "DebugToken: truncated multi-record file, parsed {N} of {TOTAL}",
            "N", parsedRecords, "TOTAL", totalRecords);
        statusCode = static_cast<uint16_t>(NSM_SW_ERROR);
        statusMsg = "Truncated multi-record file";
    }

    for (uint16_t i = 0; i < parsedRecords; ++i)
    {
        const auto& rec = records[i];
        int memFd = memfd_create("debug_token_record", MFD_CLOEXEC);
        if (memFd < 0)
        {
            lg2::error(
                "DebugToken: memfd_create failed for record {REC}/{TOTAL}: {ERR}",
                "REC", static_cast<uint16_t>(i + 1), "TOTAL", totalRecords,
                "ERR", strerror(errno));
            statusCode = static_cast<uint16_t>(NSM_SW_ERROR);
            statusMsg = std::format("Failed to prepare record {}/{}: {}", i + 1,
                                    totalRecords, strerror(errno));
            failCount++;
            continue;
        }
        auto written = write(memFd, rec.data(), rec.size());
        if (written != static_cast<ssize_t>(rec.size()))
        {
            lg2::error(
                "DebugToken: memfd write failed for record {REC}/{TOTAL}: bytes={N} expected={EXP} {ERR}",
                "REC", static_cast<uint16_t>(i + 1), "TOTAL", totalRecords, "N",
                static_cast<int64_t>(written), "EXP",
                static_cast<uint64_t>(rec.size()), "ERR",
                written < 0 ? strerror(errno) : "short write");
            close(memFd);
            statusCode = static_cast<uint16_t>(NSM_SW_ERROR);
            statusMsg = std::format(
                "Failed to prepare record {}/{}: {}", i + 1, totalRecords,
                written < 0 ? strerror(errno) : "short write");
            failCount++;
            continue;
        }
        lseek(memFd, 0, SEEK_SET);

        uint16_t errCode = 0;
        std::string errMsg;
        // installTokenDirect both returns an rc and sets errCode; treat any
        // non-success return as a failure even if errCode were left unset, so
        // this loop never silently swallows a coroutine error.
        auto rc = co_await installTokenDirect(memFd, rec.size(), errCode,
                                              errMsg);
        close(memFd);
        if (rc == NSM_SW_SUCCESS && errCode == 0)
        {
            okCount++;
        }
        else
        {
            statusCode = (errCode != 0) ? errCode : static_cast<uint16_t>(rc);
            statusMsg =
                errMsg.empty()
                    ? std::string(debug_token::Error(statusCode).to_string())
                    : errMsg;
            failCount++;
        }
        lg2::debug("DebugToken: Record {REC}/{TOTAL}: rc={RC} {MSG}", "REC",
                   static_cast<uint16_t>(i + 1), "TOTAL", totalRecords, "RC",
                   rc, "MSG", errMsg);
    }

    // Refresh device-side token status if any record installed.
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (device && okCount > 0)
    {
        queryTokenHandler(device).detach();
    }

    lg2::info(
        "DebugToken: multi-record install complete: ok={OK} failed={FAIL} total={TOTAL}",
        "OK", okCount, "FAIL", failCount, "TOTAL", totalRecords);

    if (okCount == totalRecords && failCount == 0)
    {
        valueIntf->value(std::make_tuple(static_cast<uint16_t>(0), "Success"));
        statusIntf->status(AsyncOperationStatusType::Success);
    }
    else
    {
        valueIntf->value(std::make_tuple(
            statusCode, statusMsg.empty() ? std::string("Token install failed")
                                          : statusMsg));
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDebugTokenUnifiedObject::installTokenAsyncHandler(
    std::shared_ptr<TokenInstallationInfo> info,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();
    auto request = createInstallTokenRequest(info);
    if (!request)
    {
        auto error =
            std::make_tuple(static_cast<uint16_t>(NSM_SW_ERROR),
                            std::format("Operation failed: {}",
                                        static_cast<int>(NSM_SW_ERROR)));
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        LG2_ERROR_FLT("DebugToken: installToken postPatchIO failed "
                      "| rc: {RC}, eid: {EID}",
                      "RC", nsm_sw_codes(sendRc), "EID", eid);
        if (sendRc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            auto error = std::make_tuple(static_cast<uint16_t>(sendRc),
                                         std::format("Unsupported command: {}",
                                                     static_cast<int>(sendRc)));
            valueIntf->value(error);
            statusIntf->status(AsyncOperationStatusType::UnsupportedRequest);
        }
        else
        {
            auto error = std::make_tuple(
                static_cast<uint16_t>(sendRc),
                std::format("Operation failed: {}", static_cast<int>(sendRc)));
            valueIntf->value(error);
            statusIntf->status(AsyncOperationStatusType::WriteFailure);
        }
        // coverity[missing_return]
        co_return sendRc;
    }
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    auto decodeRc = decode_nsm_install_token_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);
    if (decodeRc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: decode_nsm_install_token_resp: "
                   "eid={EID} rc={RC} cc={CC} len={LEN}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "LEN", responseLen);
        auto error =
            std::make_tuple(static_cast<uint16_t>(decodeRc),
                            std::format("Operation failed: {}", decodeRc));
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        // coverity[missing_return]
        co_return decodeRc;
    }

    if (cc == NSM_SUCCESS)
    {
        if (info->offset == info->totalSize)
        {
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(cc), "Success"));
            statusIntf->status(AsyncOperationStatusType::Success);
            queryTokenHandler(device).detach();
        }
        else
        {
            installTokenAsyncHandler(info, statusIntf, valueIntf).detach();
        }
    }
    else
    {
        uint16_t errorCode =
            (reasonCode != ERR_NULL) ? reasonCode : static_cast<uint16_t>(cc);
        auto error = std::make_tuple(
            errorCode,
            std::format("{}: {}", debug_token::Error(errorCode).to_string(),
                        errorCode));
        lg2::error("DebugToken: installToken: eid={EID} cc={CC} rc={RC}", "EID",
                   eid, "CC", cc, "RC", reasonCode);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path NsmDebugTokenUnifiedObject::eraseToken(
    sdbusplus::server::com::nvidia::debug_token::Action::EraseType eraseType,
    sdbusplus::server::com::nvidia::debug_token::Common::Types tokenType)
{
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;

    uint32_t tokenTypeValue = 0;

    switch (eraseType)
    {
        case EraseTypeEnum::TokenType:
        {
            tokenTypeValue = token_utils::tokenTypeToUint32(tokenType,
                                                            deviceTypeStr);
            if (tokenTypeValue == 0)
            {
                lg2::error(
                    "DebugToken: Invalid or unsupported token type for erase: {TYPE}",
                    "TYPE", sdbusplus::message::convert_to_string(tokenType));
                throw Common::Error::InvalidArgument();
            }
            break;
        }
        case EraseTypeEnum::EraseAll:
        {
            tokenTypeValue = 0xFFFFFFFF;
            break;
        }
        case EraseTypeEnum::EraseAllAndRatchetCounterIncreased:
        {
            tokenTypeValue = 0xFFFFFFFE;
            break;
        }
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }
    eraseTokenAsyncHandler(tokenTypeValue, statusIntf, valueIntf).detach();
    return objPath;
}

sdbusplus::message::object_path
    NsmDebugTokenUnifiedObject::installToken(sdbusplus::message::unix_fd fd)
{
    auto dupFd = dup(fd);
    if (dupFd < 0)
    {
        lg2::error("DebugToken: Failed to duplicate file descriptor: {ERR}",
                   "ERR", strerror(errno));
        throw Common::Error::InternalFailure();
    }
    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }
    auto seekRc = lseek(dupFd, 0, SEEK_SET);
    if (seekRc < 0)
    {
        lg2::error("DebugToken: lseek failed {RETURNCODE} {ERROR}",
                   "RETURNCODE", seekRc, "ERROR", strerror(errno));
        throw Common::Error::InternalFailure();
    }
    struct stat fileStat;
    auto fstatRc = fstat(dupFd, &fileStat);
    if (fstatRc < 0)
    {
        lg2::error("DebugToken: fstat failed {RETURNCODE} {ERROR}",
                   "RETURNCODE", fstatRc, "ERROR", strerror(errno));
        throw Common::Error::InternalFailure();
    }
    if (fileStat.st_size < 0)
    {
        lg2::error("DebugToken: Invalid file size in fd {SIZE}", "SIZE",
                   fileStat.st_size);
        throw Common::Error::InternalFailure();
    }

    auto info = std::make_shared<TokenInstallationInfo>(
        dupFd, static_cast<size_t>(fileStat.st_size));

    // Decide single- vs multi-record up front and dispatch accordingly.
    // isMultiRecordContainer() returns false when the file begins with the
    // spec-mandated TLV identifier "TLV1" (a single-record TLV) and true when
    // the prefix is a DebugTokenHeader (a multi-record container). The header
    // read advances the fd, so rewind to the start before handing it to either
    // install path.
    if (info->totalSize > sizeof(token_utils::DebugTokenHeader))
    {
        token_utils::DebugTokenHeader header{};
        auto bytesRead = read(dupFd, &header, sizeof(header));
        auto seekBack = lseek(dupFd, 0, SEEK_SET);
        if (seekBack < 0)
        {
            lg2::error("DebugToken: lseek failed {RETURNCODE} {ERROR}",
                       "RETURNCODE", seekBack, "ERROR", strerror(errno));
            throw Common::Error::InternalFailure();
        }
        if (bytesRead == static_cast<ssize_t>(sizeof(header)) &&
            token_utils::isMultiRecordContainer(std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(&header), sizeof(header))))
        {
            installMultiRecordToken(info, header, statusIntf, valueIntf)
                .detach();
            return objPath;
        }
    }

    installTokenAsyncHandler(info, statusIntf, valueIntf).detach();
    return objPath;
}

requester::Coroutine NsmDebugTokenUnifiedObject::installTokenDirect(
    int fd, size_t totalSize, uint16_t& errorCode, std::string& errorMessage)
{
    auto info = std::make_shared<TokenInstallationInfo>(fd, totalSize);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        errorCode = static_cast<uint16_t>(NSM_SW_ERROR);
        errorMessage = "Device not found";
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    // Check if installationChunkSize is initialized
    if (installationChunkSize == 0)
    {
        lg2::error("DebugToken: installationChunkSize is not initialized! "
                   "Device capabilities not queried. Call update() first.");
        errorCode = static_cast<uint16_t>(NSM_SW_ERROR);
        errorMessage = "Chunk size not initialized";
        co_return NSM_SW_ERROR;
    }

    // Process chunks until complete
    while (info->offset < info->totalSize)
    {
        auto request = createInstallTokenRequest(info);
        if (!request)
        {
            errorCode = static_cast<uint16_t>(NSM_SW_ERROR);
            errorMessage = "Failed to create request";
            co_return NSM_SW_ERROR;
        }

        // Send the chunk
        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                                   responseLen);
        if (sendRc != NSM_SW_SUCCESS)
        {
            lg2::error("DebugToken: installTokenDirect postPatchIO failed "
                       "rc={RC}, eid={EID}",
                       "RC", utils::nsmSwCodeToString(sendRc), "EID", eid);
            debug_token::Error error(sendRc);
            errorCode = static_cast<uint16_t>(sendRc);
            errorMessage = std::format("{}: {}", error.to_string(), errorCode);
            co_return sendRc;
        }

        // Decode the response
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        auto decodeRc = decode_nsm_install_token_resp(
            responseMsg.get(), responseLen, &cc, &reasonCode);
        if (decodeRc != NSM_SW_SUCCESS)
        {
            lg2::error("DebugToken: decode_nsm_install_token_resp: "
                       "eid={EID} rc={RC} cc={CC} len={LEN}",
                       "EID", eid, "RC", decodeRc, "CC", cc, "LEN",
                       responseLen);
            errorCode = static_cast<uint16_t>(decodeRc);
            debug_token::Error error(decodeRc);
            errorMessage = std::format("{}: {}", error.to_string(), errorCode);
            co_return decodeRc;
        }

        // Check completion code
        if (cc != NSM_SUCCESS)
        {
            lg2::error("DebugToken: installTokenDirect: eid={EID} cc={CC} "
                       "rc={RC}",
                       "EID", eid, "CC", cc, "RC", reasonCode);
            errorCode = reasonCode;
            // Force copy by creating Error object first, then explicitly copy
            // string
            debug_token::Error error(reasonCode);
            errorMessage = std::format("{}: {}", error.to_string(), errorCode);
            co_return reasonCode;
        }
    }

    errorCode = 0;
    errorMessage = "Success";
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDebugTokenUnifiedObject::queryTokenHandler(
    std::shared_ptr<NsmDevice> nsmDevice)
{
    auto eid = nsmDevice->getEid();
    auto request = std::make_shared<Request>(sizeof(nsm_msg_hdr) +
                                             sizeof(nsm_query_token_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_query_token_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: encode_nsm_query_token_req: rc={RC}", "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await nsmDevice->sensorIO(eid, *request, responseMsg,
                                               responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::debug("DebugToken: queryToken sensorIO "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", utils::nsmSwCodeToString(sendRc));
        // coverity[missing_return]
        co_return sendRc;
    }
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    size_t tlvPayloadLen = 0;
    auto decodeRc = decode_nsm_query_token_resp(responseMsg.get(), responseLen,
                                                &cc, &reasonCode, nullptr,
                                                &tlvPayloadLen);
    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        if (shouldLog("decode_nsm_query_token_resp", reasonCode, cc, decodeRc))
        {
            lg2::error("DebugToken: decode_nsm_query_token_resp: "
                       "eid={EID} ret={RET} cc={CC} rc={RC} len={LEN}",
                       "EID", eid, "RET", decodeRc, "CC", cc, "RC", reasonCode,
                       "LEN", responseLen);
        }
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    std::vector<uint8_t> tlvPayload(tlvPayloadLen);
    decodeRc = decode_nsm_query_token_resp(responseMsg.get(), responseLen, &cc,
                                           &reasonCode, tlvPayload.data(),
                                           &tlvPayloadLen);
    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        if (shouldLog("decode_nsm_query_token_resp", reasonCode, cc, decodeRc))
        {
            LG2_ERROR_FLT("decode_nsm_query_token_resp failure "
                          "| reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                          "REASONCODE", reasonCode, "CC", cc, "RC", decodeRc);
        }
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    auto tlv = debug_token::tlv_decoder::Structure();
    try
    {
        tlv.decode(tlvPayload);
    }
    catch (const std::exception& e)
    {
        if (shouldLog("queryToken_TLV_decode", reasonCode, cc, NSM_SW_ERROR))
        {
            lg2::error("DebugToken: queryToken TLV decode: {ERR}", "ERR",
                       e.what());
        }
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    uint8_t installStatus = 0;
    uint8_t procStatus = 0;
    try
    {
        installStatus =
            tlv.get(debug_token::types::InstallationStatus).getValue<uint8_t>();
    }
    catch (const std::exception& e)
    {
        if (shouldLog("get_installation_status", reasonCode, cc, NSM_SW_ERROR))
        {
            lg2::error("DebugToken: failed to get installation status: {ERR}",
                       "ERR", e.what());
        }
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    try
    {
        procStatus =
            tlv.get(debug_token::types::ProcessingStatus).getValue<uint8_t>();
    }
    catch (const std::exception& e)
    {
        if (installStatus != 0)
        {
            if (shouldLog("get_processing_status", reasonCode, cc,
                          NSM_SW_ERROR))
            {
                lg2::error("DebugToken: failed to get processing status: {ERR}",
                           "ERR", e.what());
            }
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        // ProcessingStatus not present, set to false and continue
        if (shouldLog("get_processing_status", reasonCode, cc, NSM_SW_SUCCESS))
        {
            lg2::info(
                "DebugToken: ProcessingStatus not present, setting to false");
        }
        procStatus = 0;
    }
    installationStatus(installStatus != 0);
    processingStatus(procStatus != 0);

    std::vector<uint32_t> tokenTypesSubtypes;
    try
    {
        tokenTypesSubtypes = tlv.get(debug_token::types::TokenTypeSubtypeList)
                                 .getValue<std::vector<uint32_t>>();
    }
    catch (const std::exception& e)
    {
        if (shouldLog("get_token_types", reasonCode, cc, NSM_SW_ERROR))
        {
            lg2::info("DebugToken: failed to get token types: {ERR}", "ERR",
                      e.what());
        }
    }
    if (tokenTypesSubtypes.size() % 2 != 0)
    {
        if (shouldLog("invalid_token_types_size", reasonCode, cc, NSM_SW_ERROR))
        {
            lg2::error(
                "DebugToken: invalid token types / subtypes size: {SIZE}",
                "SIZE", tokenTypesSubtypes.size());
        }
        co_return NSM_SW_ERROR;
    }
    if (installStatus != 0 && tokenTypesSubtypes.size() == 0)
    {
        if (shouldLog("token_installed_but_no_token_types", reasonCode, cc,
                      NSM_SW_ERROR))
        {
            lg2::error(
                "DebugToken: token installed but no token types / subtypes reported");
        }
        co_return NSM_SW_ERROR;
    }
    using TokenTypeEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;
    using TokenSubtypeEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::SubTypes;

    std::vector<std::tuple<TokenTypeEnum, std::vector<TokenSubtypeEnum>>>
        tokenTypesEnum;

    auto tokenTypeItr = tokenTypesSubtypes.begin();
    while (tokenTypeItr != tokenTypesSubtypes.end())
    {
        auto type = *tokenTypeItr++;
        auto subtype = *tokenTypeItr++;

        TokenTypeEnum typeEnum = token_utils::tokenTypeToEnum(type,
                                                              deviceTypeStr);
        std::vector<TokenSubtypeEnum> subtypeEnums =
            token_utils::tokenSubtypeBitmapToEnumArray(type, subtype,
                                                       deviceTypeStr);

        tokenTypesEnum.push_back(std::make_tuple(typeEnum, subtypeEnums));
    }
    tokenType(tokenTypesEnum);

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDebugTokenUnifiedObject::deviceCapabilitiesHandler(
    std::shared_ptr<NsmDevice> nsmDevice)
{
    auto eid = nsmDevice->getEid();
    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_capabilities_v2_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_get_device_capabilities_v2_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("DebugToken: encode_nsm_get_device_capabilities_v2_req: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await nsmDevice->sensorIO(eid, *request, responseMsg,
                                               responseLen);
    if (sendRc)
    {
        lg2::debug("DebugToken: deviceCapabilitiesHandler sensorIO: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", utils::nsmSwCodeToString(sendRc));
        // coverity[missing_return]
        co_return sendRc;
    }
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint8_t timestampGeneration = 0;
    uint32_t maximumInputBufferSize = 0;
    rc = decode_nsm_get_device_capabilities_v2_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &timestampGeneration,
        &maximumInputBufferSize);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        LG2_ERROR_FLT("decode_nsm_get_device_capabilities_v2_resp failure "
                      "| reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                      "REASONCODE", reasonCode, "CC", cc, "RC",
                      nsm_sw_codes(rc));
        // coverity[missing_return]
        co_return cc ? cc : rc;
    }
    installationChunkSize =
        NSM_DEBUG_TOKEN_INSTALL_CHUNK_SIZE(maximumInputBufferSize);

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine NsmDebugTokenUnifiedObject::deviceIdHandler(
    std::shared_ptr<NsmDevice> nsmDevice)
{
    auto eid = nsmDevice->getEid();
    auto request = std::make_shared<Request>(sizeof(nsm_msg_hdr) +
                                             sizeof(nsm_query_device_ids_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_query_device_ids_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("DebugToken: encode_nsm_query_device_ids_req: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await nsmDevice->sensorIO(eid, *request, responseMsg,
                                               responseLen);
    if (sendRc)
    {
        lg2::debug("DebugToken: deviceIdHandler sensorIO "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", utils::nsmSwCodeToString(sendRc));
        // coverity[missing_return]
        co_return sendRc;
    }
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    size_t deviceIdLen = 0;
    rc = decode_nsm_query_device_ids_resp(responseMsg.get(), responseLen, &cc,
                                          &reasonCode, nullptr, &deviceIdLen);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        LG2_ERROR_FLT("decode_nsm_query_device_ids_resp failure "
                      "| reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                      "REASONCODE", reasonCode, "CC", cc, "RC",
                      nsm_sw_codes(rc));
        // coverity[missing_return]
        co_return cc ? cc : rc;
    }
    std::vector<uint8_t> deviceId(deviceIdLen);
    rc = decode_nsm_query_device_ids_resp(responseMsg.get(), responseLen, &cc,
                                          &reasonCode, deviceId.data(),
                                          &deviceIdLen);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        LG2_ERROR_FLT("decode_nsm_query_device_ids_resp failure "
                      "| reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                      "REASONCODE", reasonCode, "CC", cc, "RC",
                      nsm_sw_codes(rc));
        // coverity[missing_return]
        co_return cc ? cc : rc;
    }
    std::stringstream oss;
    oss << "0x";
    for (const auto& byte : deviceId)
    {
        oss << std::format("{:02X}", byte);
    }
    tokenDeviceID(oss.str());
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine
    NsmDebugTokenUnifiedObject::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    int rc = NSM_SUCCESS;
    if (tokenDeviceID().empty())
    {
        auto idRc = co_await deviceIdHandler(nsmDevice);
        if (idRc != NSM_SUCCESS)
        {
            rc = idRc;
        }
    }
    if (installationChunkSize == 0)
    {
        auto capRc = co_await deviceCapabilitiesHandler(nsmDevice);
        if (capRc != NSM_SUCCESS)
        {
            rc = capRc;
        }
    }
    auto queryTokenRc = co_await queryTokenHandler(nsmDevice);
    if (queryTokenRc != NSM_SUCCESS)
    {
        rc = queryTokenRc;
    }
    if (rc != NSM_SUCCESS)
    {
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

} // namespace nsm
