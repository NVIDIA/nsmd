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

#include "nsmKeyMgmt.hpp"

#include "firmware-utils.h"

#include "globals.hpp"
#include "nsmFirmwareUtilsCommon.hpp"
#include "nsmSensor.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmKeyMgmt::NsmKeyMgmt(sdbusplus::bus::bus& bus, const std::string& chassisName,
                       const std::string& type, const uuid_t& uuid,
                       std::shared_ptr<ProgressIntf> progressIntf,
                       uint16_t componentClassification,
                       uint16_t componentIdentifier,
                       uint8_t componentClassificationIndex) :
    NsmSensor(chassisName, type),
    SecSigningIntf(bus, getPath(chassisName).c_str()),
    SecSigningConfigIntf(bus, getPath(chassisName).c_str()), uuid(uuid),
    progressIntf(progressIntf),
    componentClassification(componentClassification),
    componentIdentifier(componentIdentifier),
    componentClassificationIndex(componentClassificationIndex)
{
    auto settingsPath = getPath(chassisName) + "/Settings";
    settingsObject = std::make_unique<SecSigningIntf>(bus,
                                                      settingsPath.c_str());
}

int NsmKeyMgmt::startOperation()
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

void NsmKeyMgmt::finishOperation(Progress::OperationStatus status)
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

requester::Coroutine
    NsmKeyMgmt::revokeKeysAsyncHandler(std::shared_ptr<Request> request)
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
        lg2::error("KeyMgmt - revokeKeys - "
                   "SendRecvNsmMsg: eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
        errorCode(getErrorCode(NSM_FW_UPDATE_CODE_AUTH_KEY_PERM, sendRc));
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return sendRc;
    }
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint32_t updMethod = 0;
    auto decodeRc = decode_nsm_code_auth_key_perm_update_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &updMethod);
    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("KeyMgmt - revokeKeys - "
                   "decode_nsm_code_auth_key_perm_update_resp: "
                   "eid={EID} rc={RC} cc={CC} len={LEN}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "LEN", responseLen);
        errorCode(
            getErrorCode(NSM_FW_UPDATE_CODE_AUTH_KEY_PERM, cc, reasonCode));
        finishOperation(Progress::OperationStatus::Aborted);
        // coverity[missing_return]
        co_return decodeRc;
    }
    bitfield32_t updateMethodBitfield{updMethod};
    updateMethod(utils::updateMethodsBitfieldToList(updateMethodBitfield));
    finishOperation(Progress::OperationStatus::Completed);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

void NsmKeyMgmt::revokeKeys(SecurityCommon::RequestTypes requestType,
                            uint64_t nonce, std::vector<uint8_t> indices)
{
    if (startOperation() != NSM_SW_SUCCESS)
    {
        throw Common::Error::Unavailable();
    }
    nsm_code_auth_key_perm_request_type type;
    std::vector<uint8_t> bitmap;
    if (requestType == SecurityCommon::RequestTypes::MostRestrictiveValue)
    {
        type = NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
        if (indices.size() != 0)
        {
            throw Common::Error::InvalidArgument();
        }
    }
    else if (requestType == SecurityCommon::RequestTypes::SpecifiedValue)
    {
        type = NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_SPECIFIED_VALUE;
        if (indices.size() == 0)
        {
            throw Common::Error::InvalidArgument();
        }
        try
        {
            bitmap = utils::indicesToBitmap(indices, bitmapLength);
        }
        catch (std::exception&)
        {
            throw Common::Error::InvalidArgument();
        }
    }
    else
    {
        throw Common::Error::InvalidArgument();
    }
    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_update_req) +
        bitmap.size());
    auto requestMsg = reinterpret_cast<nsm_msg*>(request->data());
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        0, type, componentClassification, componentIdentifier,
        componentClassificationIndex, nonce, bitmap.size(), bitmap.data(),
        requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        revokeKeysAsyncHandler(request).detach();
        return;
    }
    lg2::error("KeyMgmt - revokeKeys - "
               "encode_nsm_code_auth_key_perm_update_req: rc={RC}",
               "RC", rc);
    finishOperation(Progress::OperationStatus::Aborted);
    if (rc == NSM_ERR_INVALID_DATA)
    {
        throw Common::Error::InvalidArgument();
    }
    throw Common::Error::InternalFailure();
}

std::optional<std::vector<uint8_t>>
    NsmKeyMgmt::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_query_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_query_req(
        instanceId, componentClassification, componentIdentifier,
        componentClassificationIndex, requestMsg);
    if (rc)
    {
        lg2::debug(
            "KeyMgmt - genRequestMsg - "
            "encode_nsm_code_auth_key_perm_query_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmKeyMgmt::handleResponseMsg(const nsm_msg* responseMsg,
                                      size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t activeComponentKeyIndex;
    uint16_t pendingComponentKeyIndex;
    uint8_t permissionBitmapLength;
    auto rc = decode_nsm_code_auth_key_perm_query_resp(
        responseMsg, responseLen, &cc, &reasonCode, &activeComponentKeyIndex,
        &pendingComponentKeyIndex, &permissionBitmapLength, NULL, NULL, NULL,
        NULL);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        logHandleResponseMsg("decode_nsm_code_auth_key_perm_query_resp",
                             reasonCode, cc, rc);
        return rc;
    }
    else
    {
        clearErrorBitMap("decode_nsm_code_auth_key_perm_query_resp");
    }
    std::vector<uint8_t> activeComponentKeyPermBitmap(permissionBitmapLength);
    std::vector<uint8_t> pendingComponentKeyPermBitmap(permissionBitmapLength);
    std::vector<uint8_t> efuseKeyPermBitmap(permissionBitmapLength);
    std::vector<uint8_t> pendingEfuseKeyPermBitmap(permissionBitmapLength);
    rc = decode_nsm_code_auth_key_perm_query_resp(
        responseMsg, responseLen, &cc, &reasonCode, &activeComponentKeyIndex,
        &pendingComponentKeyIndex, &permissionBitmapLength,
        activeComponentKeyPermBitmap.data(),
        pendingComponentKeyPermBitmap.data(), efuseKeyPermBitmap.data(),
        pendingEfuseKeyPermBitmap.data());
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("KeyMgmt - handleResponseMsg - "
                   "decode_nsm_code_auth_key_perm_query_resp failed."
                   "rc={RC} cc={CC} reasonCode={RSC}",
                   "RC", rc, "CC", cc, "RSC", reasonCode);
        return rc;
    }

    bitmapLength = permissionBitmapLength;

    auto activeIndices = utils::bitmapToIndices(efuseKeyPermBitmap);
    auto pendingIndices = utils::bitmapToIndices(pendingEfuseKeyPermBitmap);

    signingKeyIndex(activeComponentKeyIndex);
    trustedKeys(activeIndices.first);
    revokedKeys(activeIndices.second);

    settingsObject->signingKeyIndex(pendingComponentKeyIndex);
    settingsObject->trustedKeys(pendingIndices.first);
    settingsObject->revokedKeys(pendingIndices.second);

    auto activeComponentIndices =
        utils::bitmapToIndices(activeComponentKeyPermBitmap);
    auto pendingComponentIndices =
        utils::bitmapToIndices(pendingComponentKeyPermBitmap);
    for (auto& slot : fwSlotObjects)
    {
        slot->update(
            activeComponentKeyIndex, pendingComponentKeyIndex,
            activeComponentIndices.first, activeComponentIndices.second,
            pendingComponentIndices.first, pendingComponentIndices.second);
    }

    return cc;
}

} // namespace nsm
