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

#include "nsmSecurityRBP.hpp"

#include "sensorManager.hpp"

#include <charconv>
#include <fstream>
#include <iostream>

namespace nsm
{

static std::unordered_map<uint16_t, std::string> minSecVersionErrors = {
    {0x02, "Invalid MinimumSecurityVersion"},
    {0x86, "EFUSE Update Failed"},
    {0x87, "IrreversibleConfig Disabled"},
    {0x88, "Nonce Mismatch"},
};

SecurityConfiguration::SecurityConfiguration(
    sdbusplus::bus::bus& bus, const std::string& objPath, const uuid_t& uuidIn,
    std::shared_ptr<ProgressIntf> progressIntfIn, NsmSensor* nsmSensorIn) :
    SecurityConfigIntf(bus, objPath.c_str()),
    uuid(uuidIn), progressIntf(progressIntfIn), nsmSensor(nsmSensorIn)
{}

void SecurityConfiguration::updateState(
    const struct ::nsm_firmware_irreversible_config_request_0_resp& cfg_state)
{
    irreversibleConfigState(cfg_state.irreversible_config_state);
}

void SecurityConfiguration::updateNonce(
    const struct ::nsm_firmware_irreversible_config_request_2_resp& cfg_resp)
{
    nonce(cfg_resp.nonce);
}

void SecurityConfiguration::updateIrreversibleConfig(bool state)
{
    if (startOperation() != NSM_SW_SUCCESS)
    {
        return;
    }
    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_irreversible_config_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    struct ::nsm_firmware_irreversible_config_req cfg_req;
    if (state)
    {
        cfg_req.request_type = ENABLE_IRREVERSIBLE_CFG;
    }
    else
    {
        cfg_req.request_type = DISABLE_IRREVERSIBLE_CFG;
    }

    auto rc = encode_nsm_firmware_irreversible_config_req(0, &cfg_req,
                                                          requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        securityCfgAsyncHandler(request, cfg_req.request_type).detach();
        return;
    }
    lg2::debug("encode_nsm_firmware_irreversible_config_req failed: rc={RC}",
               "RC", rc);
    finishOperation(Progress::OperationStatus::Aborted);
    if (rc == NSM_ERR_INVALID_DATA)
    {
        throw Common::Error::InvalidArgument();
        return;
    }
    throw Common::Error::InternalFailure();
}

requester::Coroutine SecurityConfiguration::securityCfgAsyncHandler(
    std::shared_ptr<Request> request, uint8_t requestType)
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    uint8_t cc = 0;
    uint16_t reason_code = 0;
    auto sendRc = co_await manager.SendRecvNsmMsg(eid, *request, responseMsg,
                                                  responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "securityCfgAsyncHandler: SendRecvNsmMsg error : eid={EID} rc={RC}",
            "EID", eid, "RC", sendRc);
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return sendRc;
    }
    if (requestType == ENABLE_IRREVERSIBLE_CFG)
    {
        struct nsm_firmware_irreversible_config_request_2_resp cfg_2_resp
        {};
        auto sendRc = decode_nsm_firmware_irreversible_config_request_2_resp(
            responseMsg.get(), responseLen, &cc, &reason_code, &cfg_2_resp);
        if (sendRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            lg2::error("decode_nsm_firmware_irreversible_config_request_2_resp"
                       " failed for : eid={EID} rc={RC}",
                       "EID", eid, "RC", sendRc);
            finishOperation(Progress::OperationStatus::Aborted);
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        updateNonce(cfg_2_resp);
        finishOperation(Progress::OperationStatus::Completed);
    }
    else
    {
        auto sendRc = decode_nsm_firmware_irreversible_config_request_1_resp(
            responseMsg.get(), responseLen, &cc, &reason_code);
        if (sendRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            lg2::error("decode_nsm_firmware_irreversible_config_request_1_resp"
                       " failed for : eid={EID} rc={RC}",
                       "EID", eid, "RC", sendRc);
            finishOperation(Progress::OperationStatus::Aborted);
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        finishOperation(Progress::OperationStatus::Completed);
    }
    sendRc = co_await nsmSensor->update(manager, eid);
    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::error("IrreversibleConfig Method is success."
                   "But updating IrreversibleConfigState failed rc={RC}",
                   "RC", sendRc);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

int SecurityConfiguration::startOperation()
{
    if (!mutex.try_lock())
    {
        throw Common::Error::Unavailable();
        return NSM_SW_ERROR;
    }
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long micros = 1000000 * tv.tv_sec + tv.tv_usec;
    progressIntf->startTime(micros, true);
    progressIntf->completedTime(0, true);
    progressIntf->progress(0, true);
    progressIntf->status(Progress::OperationStatus::InProgress, true);
    return NSM_SW_SUCCESS;
}

void SecurityConfiguration::finishOperation(Progress::OperationStatus status)
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
    mutex.unlock();
}

NsmSecurityCfgObject::NsmSecurityCfgObject(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const uuid_t& uuid, std::shared_ptr<ProgressIntf> progressIntfIn) :
    NsmSensor(name, type),
    objectPath(getPath(name))
{
    lg2::info("NsmSecurityCfgObject: create object: {PATH}", "PATH",
              objectPath.c_str());
    securityCfgObject = std::make_unique<SecurityConfiguration>(
        bus, objectPath, uuid, progressIntfIn, this);
}

std::optional<std::vector<uint8_t>>
    NsmSecurityCfgObject::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_irreversible_config_req_command));
    struct ::nsm_firmware_irreversible_config_req cfg_req;
    cfg_req.request_type = QUERY_IRREVERSIBLE_CFG;
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_nsm_firmware_irreversible_config_req(instanceId, &cfg_req,
                                                          requestMsg);
    if (rc)
    {
        lg2::debug("encode_nsm_firmware_irreversible_config_req failed."
                   " eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    // utils::printBuffer(utils::Tx, request);
    return request;
}

uint8_t NsmSecurityCfgObject::handleResponseMsg(const nsm_msg* responseMsg,
                                                size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    struct nsm_firmware_irreversible_config_request_0_resp stateInfo;
    auto rc = decode_nsm_firmware_irreversible_config_request_0_resp(
        responseMsg, responseLen, &cc, &reasonCode, &stateInfo);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        logHandleResponseMsg(
            "decode_nsm_firmware_irreversible_config_request_0_resp",
            reasonCode, cc, rc);
        return rc;
    }
    else
    {
        clearErrorBitMap(
            "decode_nsm_firmware_irreversible_config_request_0_resp");
    }
    securityCfgObject->updateState(stateInfo);
    return cc;
}

MinSecurityVersion::MinSecurityVersion(
    sdbusplus::bus::bus& bus, const std::string& objPath, const uuid_t& uuidIn,
    uint16_t classificationIn, uint16_t identifierIn, uint8_t indexIn,
    std::shared_ptr<ProgressIntf> progressIntfIn, NsmSensor* nsmSensorIn) :
    MinSecVersionIntf(bus, objPath.c_str()),
    uuid(uuidIn), classification(classificationIn), identifier(identifierIn),
    index(indexIn), progressIntf(progressIntfIn), nsmSensor(nsmSensorIn)
{
    securityVersionObject =
        std::make_unique<SecurityVersionIntf>(bus, objPath.c_str());
    auto settingsPath = objPath + "/Settings";
    securityVersionSettingsObject =
        std::make_unique<SecurityVersionIntf>(bus, settingsPath.c_str());
}

inline std::tuple<uint16_t, std::string>
    MinSecurityVersion::getErrorCode(uint16_t cc)
{
    if (minSecVersionErrors.find(cc) != minSecVersionErrors.end())
    {
        return {cc, minSecVersionErrors[cc]};
    }
    return {cc, std::format("Unknown Error: {}", cc)};
}

void MinSecurityVersion::updateProperties(
    const struct ::nsm_firmware_security_version_number_resp& sec_info)
{
    securityVersionObject->version(htole16(sec_info.minimum_security_version));
    securityVersionSettingsObject->version(
        htole16(sec_info.pending_minimum_security_version));
}

void MinSecurityVersion::updateMinSecVersion(
    SecurityCommon::RequestTypes requestType, uint64_t nonce,
    uint16_t reqMinSecVersion)
{
    if (startOperation() != NSM_SW_SUCCESS)
    {
        return;
    }
    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    struct ::nsm_firmware_update_min_sec_ver_req sec_req;
    sec_req.component_classification = htole16(classification);
    sec_req.component_classification_index = index;
    sec_req.component_identifier = htole16(identifier);
    sec_req.nonce = nonce;
    if (requestType == SecurityCommon::RequestTypes::MostRestrictiveValue)
    {
        sec_req.request_type = REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
    }
    else
    {
        sec_req.request_type = REQUEST_TYPE_SPECIFIED_VALUE;
        sec_req.req_min_security_version = htole16(reqMinSecVersion);
    }

    auto rc = encode_nsm_firmware_update_sec_ver_req(0, &sec_req, requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        minSecVersionAsyncHandler(request).detach();
        return;
    }
    lg2::error("encode_nsm_firmware_update_sec_ver_req failed: rc={RC}", "RC",
               rc);
    finishOperation(Progress::OperationStatus::Aborted);
    if (rc == NSM_ERR_INVALID_DATA)
    {
        throw Common::Error::InvalidArgument();
        return;
    }
    throw Common::Error::InternalFailure();
}

requester::Coroutine MinSecurityVersion::minSecVersionAsyncHandler(
    std::shared_ptr<Request> request)
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    uint8_t cc = 0;
    uint16_t reason_code = 0;
    auto sendRc = co_await manager.SendRecvNsmMsg(eid, *request, responseMsg,
                                                  responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::error("minSecVersionAsyncHandler: SendRecvNsmMsg error :"
                   " eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
        errorCode(getErrorCode(sendRc));
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return sendRc;
    }

    struct ::nsm_firmware_update_min_sec_ver_resp sec_resp;
    {};
    sendRc = decode_nsm_firmware_update_sec_ver_resp(
        responseMsg.get(), responseLen, &cc, &reason_code, &sec_resp);
    if (sendRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("decode_nsm_firmware_update_sec_ver_resp failed for :"
                   " eid={EID} rc={RC}, cc={CC}",
                   "EID", eid, "RC", sendRc, "CC", cc);
        errorCode(getErrorCode(cc));
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    bitfield32_t updateMethodBitfield{sec_resp.update_methods};
    updateMethod(utils::updateMethodsBitfieldToList(updateMethodBitfield));
    finishOperation(Progress::OperationStatus::Completed);
    sendRc = co_await nsmSensor->update(manager, eid);
    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::error("UpdateMinSecVersion Method is success."
                   "But updating sec version properties failed rc={RC}",
                   "RC", sendRc);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

int MinSecurityVersion::startOperation()
{
    if (!mutex.try_lock())
    {
        throw Common::Error::Unavailable();
        return NSM_SW_ERROR;
    }
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long micros = 1000000 * tv.tv_sec + tv.tv_usec;
    progressIntf->startTime(micros, true);
    progressIntf->completedTime(0, true);
    progressIntf->progress(0, true);
    progressIntf->status(Progress::OperationStatus::InProgress, true);
    return NSM_SW_SUCCESS;
}

void MinSecurityVersion::finishOperation(Progress::OperationStatus status)
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
    mutex.unlock();
}

NsmMinSecVersionObject::NsmMinSecVersionObject(
    sdbusplus::bus::bus& bus, const std::string& chassisName,
    const std::string& type, const uuid_t& uuid, uint16_t classificationIn,
    uint16_t identifierIn, uint8_t indexIn,
    std::shared_ptr<ProgressIntf> progressIntfIn) :
    NsmSensor(chassisName, type),
    objectPath(getPath(chassisName)), classification(classificationIn),
    identifier(identifierIn), index(indexIn)
{
    lg2::info("NsmMinSecVersionObject: create object: {PATH}", "PATH",
              objectPath.c_str());
    minSecVersion = std::make_unique<MinSecurityVersion>(
        bus, objectPath, uuid, classification, identifier, index,
        progressIntfIn, this);
}

std::optional<std::vector<uint8_t>>
    NsmMinSecVersionObject::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_security_version_number_req_command));
    struct ::nsm_firmware_security_version_number_req sec_req;
    sec_req.component_classification = htole16(classification);
    sec_req.component_classification_index = index;
    sec_req.component_identifier = htole16(identifier);
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_nsm_query_firmware_security_version_number_req(
        instanceId, &sec_req, requestMsg);
    if (rc)
    {
        lg2::debug(
            "encode_nsm_query_firmware_security_version_number_req failed."
            " eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmMinSecVersionObject::handleResponseMsg(const nsm_msg* responseMsg,
                                                  size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    struct ::nsm_firmware_security_version_number_resp sec_info;
    auto rc = decode_nsm_query_firmware_security_version_number_resp(
        responseMsg, responseLen, &cc, &reasonCode, &sec_info);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        logHandleResponseMsg(
            "decode_nsm_query_firmware_security_version_number_resp",
            reasonCode, cc, rc);
        return rc;
    }
    else
    {
        clearErrorBitMap(
            "decode_nsm_query_firmware_security_version_number_resp");
    }
    minSecVersion->updateProperties(sec_info);
    return cc;
}
} // namespace nsm
