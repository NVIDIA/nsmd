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

#include "nsmDebugTokenNIC.hpp"

#include "debug-token.h"

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <sys/mman.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/types.hpp>

#include <cstring>
#include <format>
#include <sstream>

namespace nsm
{

NsmDebugTokenNICObject::NsmDebugTokenNICObject(sdbusplus::bus::bus& bus,
                                               const std::string& name,
                                               const uuid_t& uuid) :
    NsmObject(name, "NSM_DebugTokenNIC"),
    DebugTokenIntf(bus, (debugTokenObjectBasePath / name).c_str()), uuid(uuid)
{
    lg2::info("DebugToken: create NIC object: {PATH}", "PATH",
              debugTokenObjectBasePath / name);
}

requester::Coroutine NsmDebugTokenNICObject::disableTokensAsyncHandler(
    std::shared_ptr<Request> request,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        LG2_ERROR_FLT("DebugToken: disableTokens postPatchIO failed "
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
    auto decodeRc = decode_nsm_disable_tokens_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);
    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("DebugToken: decode_nsm_disable_tokens_resp: "
                   "eid={EID} rc={RC} cc={CC} len={LEN}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "LEN", responseLen);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        // coverity[missing_return]
        co_return decodeRc;
    }
    if (reasonCode == successReasonCode)
    {
        valueIntf->value(std::make_tuple(static_cast<uint16_t>(0), "Success"));
        statusIntf->status(AsyncOperationStatusType::Success);
    }
    else
    {
        auto error = std::make_tuple(reasonCode, "Operation failed");
        lg2::error("DebugToken: disableTokens: eid={EID} rc={RC}", "EID", eid,
                   "RC", reasonCode);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDebugTokenNICObject::getRequestAsyncHandler(
    std::shared_ptr<Request> request,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        LG2_ERROR_FLT("DebugToken: getRequest postPatchIO failed "
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
    struct nsm_debug_token_request token_request;
    auto decodeRc = decode_nsm_query_token_parameters_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &token_request);
    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("DebugToken: decode_nsm_query_token_parameters_resp: "
                   "eid={EID} rc={RC} cc={CC} len={LEN}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "LEN", responseLen);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        // coverity[missing_return]
        co_return decodeRc;
    }
    if (reasonCode == tokenAlreadyActiveReasonCode)
    {
        lg2::info("DebugToken: token already active: eid={EID} rc={RC}", "EID",
                  eid, "RC", reasonCode);
        auto error = std::make_tuple(reasonCode, "Token already active");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    int fd = memfd_create("token-request", 0);
    if (fd == -1)
    {
        lg2::error("DebugToken: memfd_create: eid={EID} error={ERROR}", "EID",
                   eid, "ERROR", strerror(errno));
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    const uint8_t* requestPtr =
        reinterpret_cast<const uint8_t*>(&token_request);
    size_t dataLen = sizeof(struct nsm_debug_token_request);
    while (dataLen != 0)
    {
        ssize_t written = write(fd, requestPtr, dataLen);
        if (written < 0)
        {
            lg2::error("DebugToken: write: eid={EID} error={ERROR}", "EID", eid,
                       "ERROR", strerror(errno));
            close(fd);
            statusIntf->status(AsyncOperationStatusType::InternalFailure);
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        requestPtr += written;
        dataLen -= written;
    }
    (void)lseek(fd, 0, SEEK_SET);
    sdbusplus::message::unix_fd unixFd(fd);
    valueIntf->value(unixFd);
    statusIntf->status(AsyncOperationStatusType::Success);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDebugTokenNICObject::getStatusAsyncHandler(
    std::shared_ptr<Request> request,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        LG2_ERROR_FLT("DebugToken: getStatus postPatchIO failed "
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
    nsm_debug_token_status status;
    nsm_debug_token_status_additional_info additionalInfo;
    nsm_debug_token_type tokenType;
    uint32_t timeLeft = 0;
    auto decodeRc = decode_nsm_query_token_status_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &status,
        &additionalInfo, &tokenType, &timeLeft);
    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("DebugToken: decode_nsm_query_token_status_resp: "
                   "eid={EID} rc={RC} cc={CC} len={LEN}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "LEN", responseLen);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        // coverity[missing_return]
        co_return decodeRc;
    }
    DebugToken::TokenTypes dbusTokenType;
    switch (tokenType)
    {
        case NSM_DEBUG_TOKEN_TYPE_FRC:
            dbusTokenType = DebugToken::TokenTypes::FRC;
            break;
        case NSM_DEBUG_TOKEN_TYPE_CRCS:
            dbusTokenType = DebugToken::TokenTypes::CRCS;
            break;
        case NSM_DEBUG_TOKEN_TYPE_CRDT:
            dbusTokenType = DebugToken::TokenTypes::CRDT;
            break;
        case NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE:
            dbusTokenType = DebugToken::TokenTypes::DebugFirmware;
            break;
        default:
            lg2::error("DebugToken: invalid token type received: "
                       "eid={EID} type={TYPE}",
                       "TYPE", tokenType);
            statusIntf->status(AsyncOperationStatusType::InternalFailure);
            // coverity[missing_return]
            co_return NSM_SW_ERROR_DATA;
    }
    DebugToken::TokenStatus dbusStatus;
    switch (status)
    {
        case NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ENDED:
            dbusStatus = DebugToken::TokenStatus::DebugSessionEnded;
            break;
        case NSM_DEBUG_TOKEN_STATUS_OPERATION_FAILURE:
            dbusStatus = DebugToken::TokenStatus::OperationFailure;
            break;
        case NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE:
            dbusStatus = DebugToken::TokenStatus::DebugSessionActive;
            break;
        case NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED:
            dbusStatus = DebugToken::TokenStatus::NoTokenApplied;
            break;
        case NSM_DEBUG_TOKEN_STATUS_CHALLENGE_PROVIDED:
            dbusStatus = DebugToken::TokenStatus::ChallengeProvided;
            break;
        case NSM_DEBUG_TOKEN_STATUS_INSTALLATION_TIMEOUT:
            dbusStatus = DebugToken::TokenStatus::InstallationTimeout;
            break;
        case NSM_DEBUG_TOKEN_STATUS_TOKEN_TIMEOUT:
            dbusStatus = DebugToken::TokenStatus::TokenTimeout;
            break;
        default:
            lg2::error("DebugToken: invalid token status received: "
                       "eid={EID} status={STAT}",
                       "EID", eid, "STAT", status);
            statusIntf->status(AsyncOperationStatusType::InternalFailure);
            // coverity[missing_return]
            co_return NSM_SW_ERROR_DATA;
    }
    DebugToken::AdditionalInfo dbusAdditionalInfo;
    switch (additionalInfo)
    {
        case NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE:
            dbusAdditionalInfo = DebugToken::AdditionalInfo::None;
            break;
        case NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NO_DEBUG_SESSION:
            dbusAdditionalInfo = DebugToken::AdditionalInfo::NoDebugSession;
            break;
        case NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_FIRMWARE_NOT_SECURED:
            dbusAdditionalInfo = DebugToken::AdditionalInfo::FirmwareNotSecured;
            break;
        case NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_END_REQUEST_NOT_ACCEPTED:
            dbusAdditionalInfo =
                DebugToken::AdditionalInfo::DebugSessionEndRequestNotAccepted;
            break;
        case NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_QUERY_DISALLOWED:
            dbusAdditionalInfo =
                DebugToken::AdditionalInfo::DebugSessionQueryDisallowed;
            break;
        case NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_ACTIVE:
            dbusAdditionalInfo = DebugToken::AdditionalInfo::DebugSessionActive;
            break;
        default:
            lg2::error("DebugToken: invalid additional info received: "
                       "eid={EID} info={INFO}",
                       "EID", eid, "INFO", additionalInfo);
            statusIntf->status(AsyncOperationStatusType::InternalFailure);
            // coverity[missing_return]
            co_return NSM_SW_ERROR_DATA;
    }

    valueIntf->value(std::make_tuple(dbusTokenType, dbusStatus,
                                     dbusAdditionalInfo, timeLeft));
    statusIntf->status(AsyncOperationStatusType::Success);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDebugTokenNICObject::installTokenAsyncHandler(
    std::shared_ptr<Request> request,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();
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
    auto decodeRc = decode_nsm_provide_token_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);
    if (decodeRc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: decode_nsm_provide_token_resp: "
                   "eid={EID} rc={RC} cc={CC} len={LEN}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "LEN", responseLen);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        // coverity[missing_return]
        co_return decodeRc;
    }
    if (cc != NSM_SUCCESS)
    {
        lg2::info("DebugToken: token not accepted: eid={EID} cc={CC}", "EID",
                  eid, "CC", cc);
        auto error = std::make_tuple(static_cast<uint16_t>(cc),
                                     "Token not accepted");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    if (reasonCode == successReasonCode)
    {
        valueIntf->value(std::make_tuple(static_cast<uint16_t>(0), "Success"));
        statusIntf->status(AsyncOperationStatusType::Success);
    }
    else if (reasonCode == tokenAlreadyActiveReasonCode)
    {
        lg2::info("DebugToken: token already active: eid={EID} rc={RC}", "EID",
                  eid, "RC", reasonCode);
        auto error = std::make_tuple(reasonCode, "Token already active");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
    }
    else
    {
        auto error = std::make_tuple(reasonCode, "Operation failed");
        lg2::error("DebugToken: installToken: eid={EID} rc={RC}", "EID", eid,
                   "RC", reasonCode);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path NsmDebugTokenNICObject::disableTokens()
{
    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    auto request = std::make_shared<Request>(sizeof(nsm_msg_hdr) +
                                             sizeof(nsm_disable_tokens_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_disable_tokens_req(0, requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        disableTokensAsyncHandler(request, statusIntf, valueIntf).detach();
        return objPath;
    }
    lg2::error("DebugToken: encode_nsm_disable_tokens_req: rc={RC}", "RC", rc);
    statusIntf->status(AsyncOperationStatusType::InternalFailure);
    throw Common::Error::InternalFailure();
}

sdbusplus::message::object_path
    NsmDebugTokenNICObject::getRequest(DebugToken::TokenOpcodes tokenOpcode)
{
    nsm_debug_token_opcode opcode;
    switch (tokenOpcode)
    {
        case DebugToken::TokenOpcodes::CRCS:
            opcode = NSM_DEBUG_TOKEN_OPCODE_CRCS;
            break;
        case DebugToken::TokenOpcodes::CRDT:
            opcode = NSM_DEBUG_TOKEN_OPCODE_CRDT;
            break;
        default:
            lg2::error("DebugToken: unsupported token opcode: op={OP}", "OP",
                       tokenOpcode);
            throw Common::Error::InvalidArgument();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_parameters_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_query_token_parameters_req(0, opcode, requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        getRequestAsyncHandler(request, statusIntf, valueIntf).detach();
        return objPath;
    }
    lg2::error("DebugToken: encode_nsm_query_token_parameters_req: rc={RC}",
               "RC", rc);
    statusIntf->status(AsyncOperationStatusType::InternalFailure);
    if (rc == NSM_ERR_INVALID_DATA)
    {
        throw Common::Error::InvalidArgument();
    }
    throw Common::Error::InternalFailure();
}

sdbusplus::message::object_path
    NsmDebugTokenNICObject::getStatus(DebugToken::TokenTypes tokenType)
{
    nsm_debug_token_type type;
    switch (tokenType)
    {
        case DebugToken::TokenTypes::FRC:
            type = NSM_DEBUG_TOKEN_TYPE_FRC;
            break;
        case DebugToken::TokenTypes::CRCS:
            type = NSM_DEBUG_TOKEN_TYPE_CRCS;
            break;
        case DebugToken::TokenTypes::CRDT:
            type = NSM_DEBUG_TOKEN_TYPE_CRDT;
            break;
        case DebugToken::TokenTypes::DebugFirmware:
            type = NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE;
            break;
        default:
            lg2::error("DebugToken: unsupported token type: type={TYPE}",
                       "TYPE", tokenType);
            throw Common::Error::InvalidArgument();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_status_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_query_token_status_req(0, type, requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        getStatusAsyncHandler(request, statusIntf, valueIntf).detach();
        return objPath;
    }
    lg2::error("DebugToken: encode_nsm_query_token_status_req: rc={RC}", "RC",
               rc);
    statusIntf->status(AsyncOperationStatusType::InternalFailure);
    if (rc == NSM_ERR_INVALID_DATA)
    {
        throw Common::Error::InvalidArgument();
    }
    throw Common::Error::InternalFailure();
}

sdbusplus::message::object_path
    NsmDebugTokenNICObject::installToken(std::vector<uint8_t> tokenData)
{
    if (tokenData.size() == 0 ||
        tokenData.size() > NSM_DEBUG_TOKEN_DATA_MAX_SIZE)
    {
        throw Common::Error::InvalidArgument();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_req_v2) + tokenData.size());
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_provide_token_req(0, tokenData.data(),
                                           tokenData.size(), requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        installTokenAsyncHandler(request, statusIntf, valueIntf).detach();
        return objPath;
    }
    lg2::error("DebugToken: encode_nsm_provide_token_req: rc={RC}", "RC", rc);
    statusIntf->status(AsyncOperationStatusType::InternalFailure);
    throw Common::Error::InternalFailure();
}

requester::Coroutine
    NsmDebugTokenNICObject::update(std::shared_ptr<NsmDevice> nsmDevice)
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
        lg2::debug("DebugToken: queryDeviceId postPatchIO: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
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

} // namespace nsm
