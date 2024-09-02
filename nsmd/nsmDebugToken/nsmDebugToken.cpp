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

#include "nsmDebugToken.hpp"

#include "debug-token.h"

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmSensor.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <sstream>

namespace nsm
{

std::string NsmDebugTokenObject::getParentChassisPath(
    const std::vector<utils::Association>& associations)
{
    std::string parent;
    for (const auto& assoc : associations)
    {
        if (assoc.forward == "parent_chassis")
        {
            parent = assoc.absolutePath;
            std::replace(parent.begin(), parent.end(), ' ', '_');
        }
    }
    return parent;
}

std::string NsmDebugTokenObject::getName(
    const std::vector<utils::Association>& associations,
    const std::string& name)
{
    std::string chassisPath = getParentChassisPath(associations);
    if (chassisPath.empty())
    {
        return debugTokenObjectBasePath / name;
    }
    return debugTokenObjectBasePath /
           chassisPath.substr(chassisPath.find_last_of('/') + 1);
}

NsmDebugTokenObject::NsmDebugTokenObject(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const uuid_t& uuid) :
    NsmObject(name, type),
    DebugTokenIntf(bus, getName(associations, name).c_str()), uuid(uuid)
{
    std::string objectPath{getName(associations, name)};
    lg2::info("DebugToken: create object: {PATH}", "PATH", objectPath.c_str());

    progressIntf = std::make_unique<ProgressIntf>(bus, objectPath.c_str());

    sdbusplus::message::unix_fd unixFd(0);
    requestFd(unixFd, true);
}

int NsmDebugTokenObject::startOperation()
{
    if (opInProgress)
    {
        return NSM_SW_ERROR;
    }
    opInProgress = true;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long micros = 1000000 * tv.tv_sec + tv.tv_usec;
    progressIntf->startTime(micros, true);
    progressIntf->completedTime(0, true);
    progressIntf->progress(0, true);
    progressIntf->status(Progress::OperationStatus::InProgress, true);
    return NSM_SW_SUCCESS;
}

void NsmDebugTokenObject::finishOperation(Progress::OperationStatus status)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long micros = 1000000 * tv.tv_sec + tv.tv_usec;
    progressIntf->completedTime(micros, true);
    if (status == Progress::OperationStatus::Completed)
    {
        progressIntf->progress(100, true);
    }
    progressIntf->status(status);
    opInProgress = false;
}

requester::Coroutine NsmDebugTokenObject::disableTokensAsyncHandler(
    std::shared_ptr<Request> request)
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await manager.SendRecvNsmMsg(eid, *request, responseMsg,
                                                  responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: disableTokens SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
        if (sendRc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            errorCode(std::make_tuple(static_cast<uint16_t>(sendRc),
                                      "Unsupported Command"));
        }
        finishOperation(Progress::OperationStatus::Aborted);
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
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return decodeRc;
    }
    if (reasonCode == 0)
    {
        finishOperation(Progress::OperationStatus::Completed);
    }
    else
    {
        finishOperation(Progress::OperationStatus::Failed);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDebugTokenObject::getRequestAsyncHandler(
    std::shared_ptr<Request> request)
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await manager.SendRecvNsmMsg(eid, *request, responseMsg,
                                                  responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: getRequest SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
        if (sendRc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            errorCode(std::make_tuple(static_cast<uint16_t>(sendRc),
                                      "Unsupported Command"));
        }
        finishOperation(Progress::OperationStatus::Aborted);
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
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return decodeRc;
    }
    int fd = memfd_create("token-request", 0);
    if (fd == -1)
    {
        lg2::error("DebugToken: memfd_create: eid={EID} error={ERROR}", "EID",
                   eid, "ERROR", strerror(errno));
        finishOperation(Progress::OperationStatus::Aborted);
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
            finishOperation(Progress::OperationStatus::Aborted);
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        requestPtr += written;
        dataLen -= written;
    }
    (void)lseek(fd, 0, SEEK_SET);
    sdbusplus::message::unix_fd unixFd(fd);
    requestFd(unixFd, true);
    finishOperation(Progress::OperationStatus::Completed);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmDebugTokenObject::getStatusAsyncHandler(std::shared_ptr<Request> request)
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await manager.SendRecvNsmMsg(eid, *request, responseMsg,
                                                  responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: getStatus SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
        if (sendRc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            errorCode(std::make_tuple(static_cast<uint16_t>(sendRc),
                                      "Unsupported Command"));
        }
        finishOperation(Progress::OperationStatus::Aborted);
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
        finishOperation(Progress::OperationStatus::Aborted);
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
            finishOperation(Progress::OperationStatus::Aborted);
            // coverity[missing_return]
            co_return NSM_SW_ERROR_DATA;
    }
    DebugToken::TokenStatus dbusStatus;
    switch (status)
    {
        case NSM_DEBUG_TOKEN_STATUS_QUERY_FAILURE:
            dbusStatus = DebugToken::TokenStatus::QueryFailure;
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
                       "STAT", status);
            finishOperation(Progress::OperationStatus::Aborted);
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
                       "INFO", additionalInfo);
            finishOperation(Progress::OperationStatus::Aborted);
            // coverity[missing_return]
            co_return NSM_SW_ERROR_DATA;
    }

    tokenStatus(std::make_tuple(dbusTokenType, dbusStatus, dbusAdditionalInfo,
                                timeLeft),
                true);
    finishOperation(Progress::OperationStatus::Completed);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDebugTokenObject::installTokenAsyncHandler(
    std::shared_ptr<Request> request)
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await manager.SendRecvNsmMsg(eid, *request, responseMsg,
                                                  responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: installToken SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
        if (sendRc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            errorCode(std::make_tuple(static_cast<uint16_t>(sendRc),
                                      "Unsupported Command"));
        }
        finishOperation(Progress::OperationStatus::Aborted);
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
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return decodeRc;
    }
    if (cc != NSM_SUCCESS)
    {
        lg2::info("DebugToken: token not accepted: eid={EID} cc={CC}", "EID",
                  eid, "CC", cc);
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    if (reasonCode == 0)
    {
        finishOperation(Progress::OperationStatus::Completed);
    }
    else
    {
        lg2::info("DebugToken: token already active: eid={EID} rc={RC}", "EID",
                  eid, "RC", reasonCode);
        finishOperation(Progress::OperationStatus::Failed);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

void NsmDebugTokenObject::disableTokens()
{
    if (startOperation() != NSM_SW_SUCCESS)
    {
        throw Common::Error::Unavailable();
    }
    auto request = std::make_shared<Request>(sizeof(nsm_msg_hdr) +
                                             sizeof(nsm_disable_tokens_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_disable_tokens_req(0, requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        disableTokensAsyncHandler(request).detach();
        return;
    }
    lg2::error("DebugToken: encode_nsm_disable_tokens_req: rc={RC}", "RC", rc);
    finishOperation(Progress::OperationStatus::Aborted);
    throw Common::Error::InternalFailure();
}

void NsmDebugTokenObject::getRequest(DebugToken::TokenOpcodes tokenOpcode)
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
    if (startOperation() != NSM_SW_SUCCESS)
    {
        throw Common::Error::Unavailable();
    }
    if (requestFd() != 0)
    {
        close(requestFd());
        sdbusplus::message::unix_fd unixFd(0);
        requestFd(unixFd, true);
    }
    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_parameters_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_query_token_parameters_req(0, opcode, requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        getRequestAsyncHandler(request).detach();
        return;
    }
    lg2::error("DebugToken: encode_nsm_query_token_parameters_req: rc={RC}",
               "RC", rc);
    finishOperation(Progress::OperationStatus::Aborted);
    if (rc == NSM_ERR_INVALID_DATA)
    {
        throw Common::Error::InvalidArgument();
    }
    throw Common::Error::InternalFailure();
}

void NsmDebugTokenObject::getStatus(DebugToken::TokenTypes tokenType)
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
    if (startOperation() != NSM_SW_SUCCESS)
    {
        throw Common::Error::Unavailable();
    }
    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_status_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_query_token_status_req(0, type, requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        getStatusAsyncHandler(request).detach();
        return;
    }
    lg2::error("DebugToken: encode_nsm_query_token_status_req: rc={RC}", "RC",
               rc);
    finishOperation(Progress::OperationStatus::Aborted);
    if (rc == NSM_ERR_INVALID_DATA)
    {
        throw Common::Error::InvalidArgument();
    }
    throw Common::Error::InternalFailure();
}

void NsmDebugTokenObject::installToken(std::vector<uint8_t> tokenData)
{
    if (tokenData.size() == 0 ||
        tokenData.size() > NSM_DEBUG_TOKEN_DATA_MAX_SIZE)
    {
        throw Common::Error::InvalidArgument();
    }
    if (startOperation() != NSM_SW_SUCCESS)
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
        installTokenAsyncHandler(request).detach();
        return;
    }
    lg2::error("DebugToken: encode_nsm_provide_token_req: rc={RC}", "RC", rc);
    finishOperation(Progress::OperationStatus::Aborted);
    throw Common::Error::InternalFailure();
}

requester::Coroutine NsmDebugTokenObject::update(SensorManager& manager,
                                                 eid_t eid)
{
    auto request = std::make_shared<Request>(sizeof(nsm_msg_hdr) +
                                             sizeof(nsm_query_device_ids_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_query_device_ids_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DebugToken: encode_nsm_query_device_ids_req: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await manager.SendRecvNsmMsg(eid, *request, responseMsg,
                                                  responseLen);
    if (sendRc)
    {
        lg2::error("DebugToken: queryDeviceId SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
        if (sendRc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            errorCode(std::make_tuple(static_cast<uint16_t>(sendRc),
                                      "Unsupported Command"));
        }
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return sendRc;
    }
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint8_t deviceId[NSM_DEBUG_TOKEN_DEVICE_ID_SIZE] = {0};
    auto decodeRc = decode_nsm_query_device_ids_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, deviceId);
    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("DebugToken: decode_nsm_query_device_ids_resp: "
                   "eid={EID} rc={RC} cc={CC} len={LEN}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "LEN", responseLen);
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return decodeRc;
    }
    std::ostringstream oss;
    oss << "0x";
    oss << std::hex << std::uppercase << std::setfill('0');
    for (auto i = 0; i < NSM_DEBUG_TOKEN_DEVICE_ID_SIZE; ++i)
    {
        oss << std::setw(2) << static_cast<int>(deviceId[i]);
    }
    tokenDeviceID(oss.str());
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

} // namespace nsm
