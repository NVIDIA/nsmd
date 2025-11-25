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

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmSensor.hpp"

#include <errno.h>
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

NsmDebugTokenUnifiedObject::NsmDebugTokenUnifiedObject(sdbusplus::bus::bus& bus,
                                                       const std::string& name,
                                                       const uuid_t& uuid) :
    NsmObject(name, "NSM_DebugTokenUnified"),
    DebugTokenActionIntf(bus, (debugTokenObjectBasePath / name).c_str()),
    DebugTokenStatusIntf(bus, (debugTokenObjectBasePath / name).c_str()),
    uuid(uuid)
{
    lg2::info("DebugToken: create Unified object: {PATH}", "PATH",
              debugTokenObjectBasePath / name);
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
                                         "Unsupported command");
            valueIntf->value(error);
            statusIntf->status(AsyncOperationStatusType::UnsupportedRequest);
        }
        else
        {
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
        auto error = std::make_tuple(
            reasonCode, debug_token::Error(reasonCode).to_string());
        lg2::error("DebugToken: eraseToken: eid={EID} rc={RC}", "EID", eid,
                   "RC", reasonCode);
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
    if (readBytes < 0)
    {
        lg2::error("DebugToken: read failed: {ERR}", "ERR", strerror(errno));
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
                                         "Unsupported command");
            valueIntf->value(error);
            statusIntf->status(AsyncOperationStatusType::UnsupportedRequest);
        }
        else
        {
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
        auto error = std::make_tuple(
            reasonCode, debug_token::Error(reasonCode).to_string());
        lg2::error("DebugToken: installToken: eid={EID} cc={CC} rc={RC}", "EID",
                   eid, "CC", cc, "RC", reasonCode);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path
    NsmDebugTokenUnifiedObject::eraseToken(uint32_t tokenType)
{
    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }
    eraseTokenAsyncHandler(tokenType, statusIntf, valueIntf).detach();
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

    auto info = std::make_shared<TokenInstallationInfo>(dupFd,
                                                        fileStat.st_size);
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
                       "RC", static_cast<int>(sendRc), "EID", eid);
            debug_token::Error error(sendRc);
            errorCode = static_cast<uint16_t>(sendRc);
            errorMessage = std::string(error.to_string());
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
            errorMessage = std::string(error.to_string());
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
            errorMessage = std::string(error.to_string());
            co_return reasonCode;
        }
    }

    // Installation complete - update token status
    queryTokenHandler(device).detach();

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
    auto sendRc = co_await nsmDevice->postPatchIO(eid, *request, responseMsg,
                                                  responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::debug("DebugToken: queryToken postPatchIO "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", nsm_sw_codes(sendRc));
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
        lg2::error("DebugToken: decode_nsm_query_token_resp: "
                   "eid={EID} ret={RET} cc={CC} rc={RC} len={LEN}",
                   "EID", eid, "RET", decodeRc, "CC", cc, "RC", reasonCode,
                   "LEN", responseLen);
        // coverity[missing_return]
        co_return decodeRc;
    }
    std::vector<uint8_t> tlvPayload(tlvPayloadLen);
    decodeRc = decode_nsm_query_token_resp(responseMsg.get(), responseLen, &cc,
                                           &reasonCode, tlvPayload.data(),
                                           &tlvPayloadLen);
    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        LG2_ERROR_FLT("decode_nsm_query_token_resp failure "
                      "| reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                      "REASONCODE", reasonCode, "CC", cc, "RC", decodeRc);
        // coverity[missing_return]
        co_return decodeRc;
    }
    auto tlv = debug_token::tlv_decoder::Structure();
    try
    {
        tlv.decode(tlvPayload);
    }
    catch (const std::exception& e)
    {
        lg2::error("DebugToken: queryToken TLV decode: {ERR}", "ERR", e.what());
        // coverity[missing_return]
        co_return decodeRc;
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
        lg2::error("DebugToken: failed to get installation status: {ERR}",
                   "ERR", e.what());
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
        lg2::error("DebugToken: failed to get processing status: {ERR}", "ERR",
                   e.what());
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
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
        lg2::info("DebugToken: failed to get token types: {ERR}", "ERR",
                  e.what());
    }
    if (tokenTypesSubtypes.size() % 2 != 0)
    {
        lg2::error("DebugToken: invalid token types / subtypes size: {SIZE}",
                   "SIZE", tokenTypesSubtypes.size());
        co_return NSM_SW_ERROR;
    }
    if (installStatus != 0 && tokenTypesSubtypes.size() == 0)
    {
        lg2::error(
            "DebugToken: token installed but no token types / subtypes reported");
        co_return NSM_SW_ERROR;
    }
    std::vector<std::tuple<uint32_t, uint32_t>> tokenTypes;
    auto tokenTypeItr = tokenTypesSubtypes.begin();
    while (tokenTypeItr != tokenTypesSubtypes.end())
    {
        auto type = *tokenTypeItr++;
        auto subtype = *tokenTypeItr++;
        tokenTypes.push_back(std::make_tuple(type, subtype));
    }
    tokenType(tokenTypes);

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
    auto sendRc = co_await nsmDevice->postPatchIO(eid, *request, responseMsg,
                                                  responseLen);
    if (sendRc)
    {
        lg2::debug("DebugToken: deviceCapabilitiesHandler postPatchIO: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
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
    auto sendRc = co_await nsmDevice->postPatchIO(eid, *request, responseMsg,
                                                  responseLen);
    if (sendRc)
    {
        lg2::debug("DebugToken: deviceIdHandler postPatchIO "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", nsm_sw_codes(sendRc));
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
