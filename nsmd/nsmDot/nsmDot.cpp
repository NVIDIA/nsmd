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

#include "nsmDot.hpp"

#include "firmware-utils.h"

#include "dBusAsyncUtils.hpp"
#include "dotErrorHandler.hpp"
#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmDotUtils.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "sensorManager.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exception>
#include <format>
#include <memory>
#include <sstream>
#include <tuple>
#include <vector>

namespace nsm
{

constexpr uint16_t DOT_ERROR_DEVICE_NOT_FOUND = 0xFFFF;
constexpr uint32_t DOT_KEY_AUTH_SCHEME_ECDSA = 0;
constexpr uint32_t DOT_KEY_AUTH_SCHEME_HYBRID = 1;
constexpr uint32_t DOT_LOCK_ENABLE = 0;
constexpr uint32_t DOT_LOCK_DISABLE = 1;
constexpr size_t DOT_SIGNATURE_ECDSA_SIZE = 96;
constexpr size_t DOT_SIGNATURE_LMS_SIZE = 1740;
constexpr size_t DOT_SIGNATURE_TOTAL_SIZE = 1840;
constexpr uint8_t DOT_STATUS_MASK = 0x03;
constexpr uint8_t DOT_STATUS_UNINITIALIZED = 0;
constexpr uint8_t DOT_STATUS_VOLATILE = 1;
constexpr uint8_t DOT_STATUS_LOCKED = 2;
constexpr uint8_t DOT_STATUS_DISABLED = 3;

NsmDotObject::NsmDotObject(sdbusplus::bus::bus& bus, const std::string& name,
                           const uuid_t& uuid) :
    NsmObject(name, "NSM_Dot"),
    DotActionIntf(bus, (dotObjectBasePath / name).c_str()), uuid(uuid)
{
    lg2::debug("Dot: create Unified object: {PATH}", "PATH",
               dotObjectBasePath / name);

    DotActionIntf::dotState(DotActionIntf::DOTStates::Uninitialized);
}

void NsmDotObject::handleSendError(int sendRc, int eid,
                                   std::shared_ptr<AsyncStatusIntf> statusIntf,
                                   std::shared_ptr<AsyncValueIntf> valueIntf)
{
    if (sendRc && sendRc != NSM_SW_ERROR_TIMEOUT)
    {
        lg2::error("Dot: postPatchIO failed: eid={EID} rc={RC} ({RCNAME})",
                   "EID", eid, "RC", sendRc, "RCNAME", nsm_sw_codes(sendRc));
    }

    if (sendRc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
    {
        auto error = std::make_tuple(static_cast<uint16_t>(sendRc),
                                     "Unsupported command");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::UnsupportedRequest);
    }
    else if (sendRc == NSM_SW_ERROR_TIMEOUT)
    {
        auto error = std::make_tuple(static_cast<uint16_t>(sendRc),
                                     "MCTP timeout - device not responding");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
    }
    else
    {
        auto error = std::make_tuple(static_cast<uint16_t>(sendRc),
                                     "Communication or send error");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
    }
}

bool NsmDotObject::buildAndValidateCAKAuthData(
    DotActionIntf::KeyAuthScheme cakKeyAuthScheme,
    const std::string& cakEcdsaKey, const std::string& cakLmsKey,
    uint8_t* output, std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    uint8_t cakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
    uint8_t cakLmsBuf[dot::LMS_KEY_SIZE] = {0};

    if (!dot::decodeKeyData(cakEcdsaKey, cakEcdsaBuf, dot::ECDSA_KEY_SIZE))
    {
        lg2::error("Dot: Invalid CAK ECDSA key data");
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                            "Invalid CAK ECDSA key data"));
        return false;
    }

    if (!cakLmsKey.empty())
    {
        if (!dot::decodeKeyData(cakLmsKey, cakLmsBuf, dot::LMS_KEY_SIZE))
        {
            lg2::error("Dot: Invalid CAK LMS key data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                                "Invalid CAK LMS key data"));
            return false;
        }
    }

    uint32_t cakScheme =
        (cakKeyAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid)
            ? DOT_KEY_AUTH_SCHEME_HYBRID
            : DOT_KEY_AUTH_SCHEME_ECDSA;

    if (!dot::buildKeyAuthData(cakScheme, cakEcdsaBuf, cakLmsBuf, output))
    {
        lg2::error("Dot: Failed to build CAK auth data");
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        return false;
    }

    return true;
}

bool NsmDotObject::buildAndValidateSignatureData(
    DotActionIntf::KeyAuthScheme signatureAuthScheme,
    const std::string& ecdsaSignature, const std::string& lmsSignature,
    uint8_t* output, std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    uint8_t ecdsaSigBuf[DOT_SIGNATURE_ECDSA_SIZE] = {0};
    if (!dot::decodeKeyData(ecdsaSignature, ecdsaSigBuf,
                            DOT_SIGNATURE_ECDSA_SIZE))
    {
        lg2::error("Dot: Invalid ECDSA signature data");
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                            "Invalid ECDSA signature"));
        return false;
    }

    memcpy(output, ecdsaSigBuf, DOT_SIGNATURE_ECDSA_SIZE);

    if (signatureAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid)
    {
        uint8_t lmsSigBuf[DOT_SIGNATURE_LMS_SIZE] = {0};
        if (!dot::decodeKeyData(lmsSignature, lmsSigBuf,
                                DOT_SIGNATURE_LMS_SIZE))
        {
            lg2::error("Dot: Invalid LMS signature data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                                "Invalid LMS signature"));
            return false;
        }
        memcpy(output + DOT_SIGNATURE_ECDSA_SIZE, lmsSigBuf,
               DOT_SIGNATURE_LMS_SIZE);
    }
    else
    {
        memset(output + DOT_SIGNATURE_ECDSA_SIZE, 0, DOT_SIGNATURE_LMS_SIZE);
    }

    return true;
}

bool NsmDotObject::buildAndValidateLAKAuthData(
    DotActionIntf::KeyAuthScheme lakKeyAuthScheme,
    const std::string& lakEcdsaKey, const std::string& lakLmsKey,
    uint8_t* output, std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    uint8_t lakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
    uint8_t lakLmsBuf[dot::LMS_KEY_SIZE] = {0};

    if (!lakEcdsaKey.empty())
    {
        if (!dot::decodeKeyData(lakEcdsaKey, lakEcdsaBuf, dot::ECDSA_KEY_SIZE))
        {
            lg2::error("Dot: Invalid LAK ECDSA key data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                                "Invalid LAK ECDSA key data"));
            return false;
        }
    }

    if (!lakLmsKey.empty())
    {
        if (!dot::decodeKeyData(lakLmsKey, lakLmsBuf, dot::LMS_KEY_SIZE))
        {
            lg2::error("Dot: Invalid LAK LMS key data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                                "Invalid LAK LMS key data"));
            return false;
        }
    }

    uint32_t lakScheme =
        (lakKeyAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid)
            ? DOT_KEY_AUTH_SCHEME_HYBRID
            : DOT_KEY_AUTH_SCHEME_ECDSA;

    if (!dot::buildKeyAuthData(lakScheme, lakEcdsaBuf, lakLmsBuf, output))
    {
        lg2::error("Dot: Failed to build LAK auth data");
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        return false;
    }

    return true;
}

requester::Coroutine NsmDotObject::dotCAKInstallAsyncHandler(
    DotActionIntf::KeyAuthScheme cakKeyAuthScheme, std::string cakEcdsaKey,
    std::string cakLmsKey, DotActionIntf::KeyAuthScheme lakKeyAuthScheme,
    std::string lakEcdsaKey, std::string lakLmsKey, bool lockDisable,
    uint32_t minSvn, std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(DOT_ERROR_DEVICE_NOT_FOUND, "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    nsm_dot_cak_install_req dotReq;

    if (!buildAndValidateCAKAuthData(cakKeyAuthScheme, cakEcdsaKey, cakLmsKey,
                                     dotReq.cak_pub, statusIntf, valueIntf))
    {
        co_return NSM_ERR_INVALID_DATA;
    }

    if (!buildAndValidateLAKAuthData(lakKeyAuthScheme, lakEcdsaKey, lakLmsKey,
                                     dotReq.lak_pub, statusIntf, valueIntf))
    {
        co_return NSM_ERR_INVALID_DATA;
    }

    dotReq.lock_disable = lockDisable ? DOT_LOCK_DISABLE : DOT_LOCK_ENABLE;
    dotReq.min_svn = minSvn;

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_install_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_cak_install_req(0, &dotReq, requestMsg);
    std::string msg = utils::requestMsgToHexString(*request);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("Dot: encode_nsm_dot_cak_install_req: rc={RC}", "RC", rc);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    lg2::debug(
        "Dot: dotCAKInstall request encoded, sending to eid={EID} reqSize={SIZE}",
        "EID", eid, "SIZE", request->size());

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);

    lg2::debug(
        "Dot: dotCAKInstall postPatchIO returned: eid={EID} rc={RC} responseLen={LEN}",
        "EID", eid, "RC", utils::nsmSwCodeToString(sendRc), "LEN", responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        handleSendError(sendRc, eid, statusIntf, valueIntf);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;

    lg2::debug("Dot: dotCAKInstall response received: eid={EID} len={LEN}",
               "EID", eid, "LEN", responseLen);

    auto decodeRc = decode_nsm_dot_cak_install_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);

    lg2::debug(
        "Dot: dotCAKInstall decode result: eid={EID} decodeRc={RC} cc={CC} reasonCode={REASON}",
        "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode);

    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "Dot: decode_nsm_dot_cak_install_resp: "
            "eid={EID} rc={RC} cc={CC} reasonCode={REASON} len={LEN} msg={MSG}",
            "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode, "LEN",
            responseLen, "MSG", msg);
        auto error = dot::formatDotDeviceError(cc, reasonCode,
                                               NSM_FW_DOT_CAK_INSTALL);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        co_return decodeRc;
    }

    lg2::info("Dot: CAK Install completed successfully: eid={EID}", "EID", eid);
    valueIntf->value(std::make_tuple(static_cast<uint16_t>(cc), "Success"));
    statusIntf->status(AsyncOperationStatusType::Success);
    dotStatusSensor_->update(device).detach();
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDotObject::bypassAsyncHandler(
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(DOT_ERROR_DEVICE_NOT_FOUND, "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    lg2::debug("Dot: bypassAsyncHandler starting: eid={EID}", "EID", eid);

    auto request = std::make_shared<Request>(sizeof(nsm_msg_hdr) +
                                             sizeof(nsm_dot_cak_bypass_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_cak_bypass_req(0, requestMsg);
    std::string msg = utils::requestMsgToHexString(*request);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("Dot: encode_nsm_dot_cak_bypass_req: rc={RC}", "RC", rc);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    lg2::debug(
        "Dot: bypass request encoded, sending to eid={EID} reqSize={SIZE}",
        "EID", eid, "SIZE", request->size());

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);

    lg2::debug(
        "Dot: bypass postPatchIO returned: eid={EID} rc={RC} responseLen={LEN}",
        "EID", eid, "RC", utils::nsmSwCodeToString(sendRc), "LEN", responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        handleSendError(sendRc, eid, statusIntf, valueIntf);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;

    lg2::debug("Dot: bypass decoding response: eid={EID} len={LEN}", "EID", eid,
               "LEN", responseLen);

    auto decodeRc = decode_nsm_dot_cak_bypass_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);

    lg2::debug(
        "Dot: bypass decode result: eid={EID} decodeRc={RC} cc={CC} reasonCode={REASON}",
        "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode);

    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "Dot: decode_nsm_dot_cak_bypass_resp: "
            "eid={EID} rc={RC} cc={CC} reasonCode={REASON} len={LEN} msg={MSG}",
            "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode, "LEN",
            responseLen, "MSG", msg);
        auto error = dot::formatDotDeviceError(cc, reasonCode,
                                               NSM_FW_DOT_CAK_BYPASS);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        co_return decodeRc;
    }

    valueIntf->value(std::make_tuple(static_cast<uint16_t>(cc), "Success"));
    statusIntf->status(AsyncOperationStatusType::Success);
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path
    NsmDotObject::dotCAKInstall(DotActionIntf::KeyAuthScheme cakKeyAuthScheme,
                                std::string cakEcdsaKey, std::string cakLmsKey,
                                DotActionIntf::KeyAuthScheme lakKeyAuthScheme,
                                std::string lakEcdsaKey, std::string lakLmsKey,
                                bool lockDisable, uint32_t minSvn)
{
    uint8_t cakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
    if (!dot::decodeKeyData(cakEcdsaKey, cakEcdsaBuf, dot::ECDSA_KEY_SIZE))
    {
        lg2::error("Dot: Invalid CAK ECDSA key data");
        throw Common::Error::InvalidArgument();
    }

    if (!lakEcdsaKey.empty())
    {
        uint8_t lakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
        if (!dot::decodeKeyData(lakEcdsaKey, lakEcdsaBuf, dot::ECDSA_KEY_SIZE))
        {
            lg2::error("Dot: Invalid LAK ECDSA key data");
            throw Common::Error::InvalidArgument();
        }
    }

    if (cakKeyAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        cakLmsKey.empty())
    {
        lg2::error("Dot: Hybrid mode requires CAK LMS key");
        throw Common::Error::InvalidArgument();
    }

    if (lakKeyAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        lakLmsKey.empty())
    {
        lg2::error("Dot: Hybrid mode requires LAK LMS key");
        throw Common::Error::InvalidArgument();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    dotCAKInstallAsyncHandler(cakKeyAuthScheme, cakEcdsaKey, cakLmsKey,
                              lakKeyAuthScheme, lakEcdsaKey, lakLmsKey,
                              lockDisable, minSvn, statusIntf, valueIntf)
        .detach();

    return objPath;
}

sdbusplus::message::object_path NsmDotObject::bypass()
{
    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    bypassAsyncHandler(statusIntf, valueIntf).detach();

    return objPath;
}

requester::Coroutine
    NsmDotObject::lockAsyncHandler(const DotLockParams& params,
                                   std::shared_ptr<AsyncStatusIntf> statusIntf,
                                   std::shared_ptr<AsyncValueIntf> valueIntf)
{
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(DOT_ERROR_DEVICE_NOT_FOUND, "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    nsm_dot_lock_req dotReq = {};

    if (!buildAndValidateCAKAuthData(params.cakKeyAuthScheme,
                                     params.cakEcdsaKey, params.cakLmsKey,
                                     dotReq.cak_pub, statusIntf, valueIntf))
    {
        co_return NSM_ERR_INVALID_DATA;
    }

    if (!buildAndValidateLAKAuthData(params.lakKeyAuthScheme,
                                     params.lakEcdsaKey, params.lakLmsKey,
                                     dotReq.lak_pub, statusIntf, valueIntf))
    {
        co_return NSM_ERR_INVALID_DATA;
    }

    dotReq.unlock_method = static_cast<uint32_t>(params.unlockMethod);

    if (params.unlockMethod == DotActionIntf::UnlockMethod::StaticValue)
    {
        if (!dot::decodeKeyData(params.staticChallenge, dotReq.s_challenge,
                                DOT_STATIC_CHALLENGE_SIZE))
        {
            lg2::error("Dot: Invalid static challenge data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                                "Invalid static challenge"));
            co_return NSM_ERR_INVALID_DATA;
        }
    }
    else
    {
        memset(dotReq.s_challenge, 0, DOT_STATIC_CHALLENGE_SIZE);
    }

    if (!buildAndValidateSignatureData(
            params.lockSignatureAuthScheme, params.ecdsaSignature,
            params.lmsSignature, dotReq.signature, statusIntf, valueIntf))
    {
        co_return NSM_ERR_INVALID_DATA;
    }

    auto request = std::make_shared<Request>(sizeof(nsm_msg_hdr) +
                                             sizeof(nsm_dot_lock_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_lock_req(0, &dotReq, requestMsg);
    std::string msg = utils::requestMsgToHexString(*request);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("Dot: encode_nsm_dot_lock_req: rc={RC}", "RC", rc);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    lg2::debug("Dot: lock request encoded, sending to eid={EID} reqSize={SIZE}",
               "EID", eid, "SIZE", request->size());

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);

    lg2::debug(
        "Dot: lock postPatchIO returned: eid={EID} rc={RC} responseLen={LEN}",
        "EID", eid, "RC", utils::nsmSwCodeToString(sendRc), "LEN", responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        handleSendError(sendRc, eid, statusIntf, valueIntf);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);

    lg2::debug("Dot: lock response received: eid={EID} len={LEN}", "EID", eid,
               "LEN", responseLen);

    auto decodeRc = decode_nsm_dot_lock_resp(responseMsg.get(), responseLen,
                                             &cc, &reasonCode, dotBlob.data());

    lg2::debug(
        "Dot: lock decode result: eid={EID} decodeRc={RC} cc={CC} reasonCode={REASON}",
        "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode);

    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "Dot: decode_nsm_dot_lock_resp: "
            "eid={EID} rc={RC} cc={CC} reasonCode={REASON} len={LEN} msg={MSG}",
            "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode, "LEN",
            responseLen, "MSG", msg);
        auto error = dot::formatDotDeviceError(cc, reasonCode, NSM_FW_DOT_LOCK);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        co_return decodeRc;
    }

    if (dotBlob.empty())
    {
        lg2::error("Dot: lock response blob is empty");
        auto error = std::make_tuple(static_cast<uint16_t>(NSM_SW_ERROR),
                                     "DOT Lock blob is empty");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return NSM_SW_ERROR;
    }

    lg2::info("Dot: Lock completed successfully: eid={EID}", "EID", eid);
    valueIntf->value(dotBlob);
    statusIntf->status(AsyncOperationStatusType::Success);
    dotStatusSensor_->update(device).detach();
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path NsmDotObject::lock(
    DotActionIntf::KeyAuthScheme cakKeyAuthScheme, std::string cakEcdsaKey,
    std::string cakLmsKey, DotActionIntf::KeyAuthScheme lakKeyAuthScheme,
    std::string lakEcdsaKey, std::string lakLmsKey,
    DotActionIntf::UnlockMethod unlockMethod, std::string staticChallenge,
    DotActionIntf::KeyAuthScheme lockSignatureAuthScheme,
    std::string ecdsaSignature, std::string lmsSignature)
{
    uint8_t cakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
    if (!dot::decodeKeyData(cakEcdsaKey, cakEcdsaBuf, dot::ECDSA_KEY_SIZE))
    {
        lg2::error("Dot: Invalid CAK ECDSA key data");
        throw Common::Error::InvalidArgument();
    }

    uint8_t lakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
    if (!dot::decodeKeyData(lakEcdsaKey, lakEcdsaBuf, dot::ECDSA_KEY_SIZE))
    {
        lg2::error("Dot: Invalid LAK ECDSA key data");
        throw Common::Error::InvalidArgument();
    }

    if (cakKeyAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        cakLmsKey.empty())
    {
        lg2::error("Dot: Hybrid mode requires CAK LMS key");
        throw Common::Error::InvalidArgument();
    }

    if (lakKeyAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        lakLmsKey.empty())
    {
        lg2::error("Dot: Hybrid mode requires LAK LMS key");
        throw Common::Error::InvalidArgument();
    }

    if (lockSignatureAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        lmsSignature.empty())
    {
        lg2::error("Dot: Hybrid signature mode requires LMS signature");
        throw Common::Error::InvalidArgument();
    }

    if (unlockMethod == DotActionIntf::UnlockMethod::StaticValue &&
        staticChallenge.empty())
    {
        lg2::error("Dot: Static unlock method requires static challenge");
        throw Common::Error::InvalidArgument();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    DotLockParams params{
        cakKeyAuthScheme, cakEcdsaKey,     cakLmsKey,
        lakKeyAuthScheme, lakEcdsaKey,     lakLmsKey,
        unlockMethod,     staticChallenge, lockSignatureAuthScheme,
        ecdsaSignature,   lmsSignature};

    lockAsyncHandler(params, statusIntf, valueIntf).detach();

    return objPath;
}

requester::Coroutine NsmDotObject::unlockChallengeAsyncHandler(
    DotActionIntf::UnlockType unlockType,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    lg2::debug("Dot: unlockChallengeAsyncHandler started for unlockType={TYPE}",
               "TYPE", static_cast<uint32_t>(unlockType));

    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(DOT_ERROR_DEVICE_NOT_FOUND, "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    nsm_dot_unlock_challenge_req unlockChallengeReq;

    if (unlockType == DotActionIntf::UnlockType::OwnerUnlock)
    {
        unlockChallengeReq.unlock_type = 1;
    }
    else if (unlockType == DotActionIntf::UnlockType::VendorUnlock)
    {
        unlockChallengeReq.unlock_type = 2;
    }
    else
    {
        lg2::error("Dot: Invalid unlock type: {TYPE}", "TYPE",
                   static_cast<uint32_t>(unlockType));
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                            "Invalid unlock type"));
        co_return NSM_ERR_INVALID_DATA;
    }

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_unlock_challenge_req(0, &unlockChallengeReq,
                                                  requestMsg);
    std::string msg = utils::requestMsgToHexString(*request);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("Dot: encode_nsm_dot_unlock_challenge_req: rc={RC}", "RC",
                   rc);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    lg2::debug(
        "Dot: unlockChallenge request encoded, sending to eid={EID} reqSize={SIZE}",
        "EID", eid, "SIZE", request->size());

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);

    lg2::debug("Dot: unlockChallenge postPatchIO returned: eid={EID} rc={RC} "
               "responseLen={LEN}",
               "EID", eid, "RC", utils::nsmSwCodeToString(sendRc), "LEN",
               responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        handleSendError(sendRc, eid, statusIntf, valueIntf);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    std::vector<uint8_t> challenge(DOT_CHALLENGE_SIZE);

    auto decodeRc = decode_nsm_dot_unlock_challenge_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, challenge.data());

    lg2::debug("Dot: unlockChallenge decode result: eid={EID} decodeRc={RC} "
               "cc={CC} reasonCode={REASON}",
               "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode);

    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("Dot: decode_nsm_dot_unlock_challenge_resp: eid={EID} "
                   "rc={RC} cc={CC} reasonCode={REASON} len={LEN} msg={MSG}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode,
                   "LEN", responseLen, "MSG", msg);
        auto error = dot::formatDotDeviceError(cc, reasonCode,
                                               NSM_FW_DOT_UNLOCK_CHALLENGE);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        co_return decodeRc;
    }

    if (challenge.empty())
    {
        lg2::error("Dot: unlock challenge response is empty");
        auto error = std::make_tuple(static_cast<uint16_t>(NSM_SW_ERROR),
                                     "DOT Unlock Challenge response is empty");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return NSM_SW_ERROR;
    }

    lg2::info("Dot: Unlock Challenge retrieved successfully: eid={EID}", "EID",
              eid);
    valueIntf->value(challenge);
    statusIntf->status(AsyncOperationStatusType::Success);
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path
    NsmDotObject::unlockChallenge(DotActionIntf::UnlockType unlockType)
{
    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    unlockChallengeAsyncHandler(unlockType, statusIntf, valueIntf).detach();

    return objPath;
}

requester::Coroutine NsmDotObject::unlockAsyncHandler(
    DotActionIntf::KeyAuthScheme unlockSignatureAuthScheme,
    std::string ecdsaSignature, std::string lmsSignature,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    lg2::debug("Dot: unlockAsyncHandler started");

    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(DOT_ERROR_DEVICE_NOT_FOUND, "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    nsm_dot_unlock_req dotReq = {};

    if (!buildAndValidateSignatureData(unlockSignatureAuthScheme,
                                       ecdsaSignature, lmsSignature,
                                       dotReq.signature, statusIntf, valueIntf))
    {
        co_return NSM_ERR_INVALID_DATA;
    }

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_unlock_req(0, &dotReq, requestMsg);
    std::string msg = utils::requestMsgToHexString(*request);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("Dot: encode_nsm_dot_unlock_req: rc={RC}", "RC", rc);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    lg2::debug(
        "Dot: unlock request encoded, sending to eid={EID} reqSize={SIZE}",
        "EID", eid, "SIZE", request->size());

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);

    lg2::debug(
        "Dot: unlock postPatchIO returned: eid={EID} rc={RC} responseLen={LEN}",
        "EID", eid, "RC", utils::nsmSwCodeToString(sendRc), "LEN", responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        handleSendError(sendRc, eid, statusIntf, valueIntf);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;

    auto decodeRc = decode_nsm_dot_unlock_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode);

    lg2::debug("Dot: unlock decode result: eid={EID} decodeRc={RC} cc={CC} "
               "reasonCode={REASON}",
               "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode);

    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("Dot: decode_nsm_dot_unlock_resp: eid={EID} rc={RC} "
                   "cc={CC} reasonCode={REASON} len={LEN} msg={MSG}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode,
                   "LEN", responseLen, "MSG", msg);
        auto error = dot::formatDotDeviceError(cc, reasonCode,
                                               NSM_FW_DOT_UNLOCK);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        co_return decodeRc;
    }

    lg2::info("Dot: Unlock completed successfully: eid={EID}", "EID", eid);
    valueIntf->value(std::vector<uint8_t>{});
    statusIntf->status(AsyncOperationStatusType::Success);
    dotStatusSensor_->update(device).detach();
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path
    NsmDotObject::unlock(DotActionIntf::KeyAuthScheme unlockSignatureAuthScheme,
                         std::string ecdsaSignature, std::string lmsSignature)
{
    if (unlockSignatureAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        lmsSignature.empty())
    {
        lg2::error("Dot: Hybrid signature mode requires LMS signature");
        throw Common::Error::InvalidArgument();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    unlockAsyncHandler(unlockSignatureAuthScheme, ecdsaSignature, lmsSignature,
                       statusIntf, valueIntf)
        .detach();

    return objPath;
}

sdbusplus::message::object_path NsmDotObject::getInfo()
{
    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    getInfoAsyncHandler(statusIntf, valueIntf).detach();

    return objPath;
}

sdbusplus::message::object_path NsmDotObject::cakRotate(
    DotActionIntf::KeyAuthScheme cakKeyAuthScheme, std::string cakEcdsaKey,
    std::string cakLmsKey,
    DotActionIntf::KeyAuthScheme cakRotateSignatureAuthScheme,
    std::string ecdsaSignature, std::string lmsSignature)
{
    if (cakEcdsaKey.empty())
    {
        lg2::error("Dot: CAKRotate requires CAK ECDSA key");
        throw Common::Error::InvalidArgument();
    }

    if (cakKeyAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        cakLmsKey.empty())
    {
        lg2::error("Dot: Hybrid CAK auth scheme requires CAK LMS key");
        throw Common::Error::InvalidArgument();
    }

    if (ecdsaSignature.empty())
    {
        lg2::error("Dot: CAKRotate requires ECDSA signature");
        throw Common::Error::InvalidArgument();
    }

    if (cakRotateSignatureAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        lmsSignature.empty())
    {
        lg2::error("Dot: Hybrid signature scheme requires LMS signature");
        throw Common::Error::InvalidArgument();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    cakRotateAsyncHandler(cakKeyAuthScheme, cakEcdsaKey, cakLmsKey,
                          cakRotateSignatureAuthScheme, ecdsaSignature,
                          lmsSignature, statusIntf, valueIntf)
        .detach();

    return objPath;
}

requester::Coroutine NsmDotObject::getInfoAsyncHandler(
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(DOT_ERROR_DEVICE_NOT_FOUND, "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    lg2::debug("Dot: getInfoAsyncHandler starting: eid={EID}", "EID", eid);

    auto request = std::make_shared<Request>(sizeof(nsm_msg_hdr) +
                                             sizeof(nsm_dot_get_info_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_get_info_req(0, requestMsg);
    std::string msg = utils::requestMsgToHexString(*request);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("Dot: encode_nsm_dot_get_info_req: rc={RC}", "RC", rc);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    lg2::debug(
        "Dot: getInfo request encoded, sending to eid={EID} reqSize={SIZE}",
        "EID", eid, "SIZE", request->size());

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);

    lg2::debug(
        "Dot: getInfo postPatchIO returned: eid={EID} rc={RC} responseLen={LEN}",
        "EID", eid, "RC", utils::nsmSwCodeToString(sendRc), "LEN", responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        handleSendError(sendRc, eid, statusIntf, valueIntf);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t version = 0;
    uint8_t fuseChangeState = 0;
    uint8_t transfersRemaining = 0;
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);

    lg2::debug("Dot: getInfo decoding response: eid={EID} len={LEN}", "EID",
               eid, "LEN", responseLen);

    auto decodeRc = decode_nsm_dot_get_info_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &version,
        &fuseChangeState, &transfersRemaining, dotBlob.data());

    lg2::debug(
        "Dot: getInfo decode result: eid={EID} decodeRc={RC} cc={CC} reasonCode={REASON}",
        "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode);

    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "Dot: decode_nsm_dot_get_info_resp: "
            "eid={EID} rc={RC} cc={CC} reasonCode={REASON} len={LEN} msg={MSG}",
            "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode, "LEN",
            responseLen, "MSG", msg);
        auto error = dot::formatDotDeviceError(cc, reasonCode,
                                               NSM_FW_DOT_GET_INFO);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return decodeRc;
    }

    if (dotBlob.empty())
    {
        lg2::error("Dot: getInfo response blob is empty");
        auto error = std::make_tuple(static_cast<uint16_t>(NSM_SW_ERROR),
                                     "DOT Get Info blob is empty");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return NSM_SW_ERROR;
    }

    lg2::debug(
        "Dot: getInfo success: eid={EID} version={VER} fuseState={FUSE} transfers={TRANS}",
        "EID", eid, "VER", version, "FUSE", fuseChangeState, "TRANS",
        transfersRemaining);

    auto getInfoResult = std::make_tuple(version, fuseChangeState,
                                         transfersRemaining, dotBlob);
    valueIntf->value(getInfoResult);
    statusIntf->status(AsyncOperationStatusType::Success);
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDotObject::cakRotateAsyncHandler(
    DotActionIntf::KeyAuthScheme cakKeyAuthScheme, std::string cakEcdsaKey,
    std::string cakLmsKey,
    DotActionIntf::KeyAuthScheme cakRotateSignatureAuthScheme,
    std::string ecdsaSignature, std::string lmsSignature,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(DOT_ERROR_DEVICE_NOT_FOUND, "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    nsm_dot_cak_rotate_req dotReq = {};

    if (!buildAndValidateCAKAuthData(cakKeyAuthScheme, cakEcdsaKey, cakLmsKey,
                                     dotReq.new_cak, statusIntf, valueIntf))
    {
        co_return NSM_ERR_INVALID_DATA;
    }

    // Build signature data using helper function
    if (!buildAndValidateSignatureData(cakRotateSignatureAuthScheme,
                                       ecdsaSignature, lmsSignature,
                                       dotReq.signature, statusIntf, valueIntf))
    {
        co_return NSM_ERR_INVALID_DATA;
    }

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_rotate_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_cak_rotate_req(0, &dotReq, requestMsg);
    std::string msg = utils::requestMsgToHexString(*request);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("Dot: encode_nsm_dot_cak_rotate_req: rc={RC}", "RC", rc);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    lg2::debug("Dot: cakRotate request encoded, sending to eid={EID} "
               "reqSize={SIZE}",
               "EID", eid, "SIZE", request->size());

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);

    lg2::debug(
        "Dot: cakRotate postPatchIO returned: eid={EID} rc={RC} responseLen={LEN}",
        "EID", eid, "RC", utils::nsmSwCodeToString(sendRc), "LEN", responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        handleSendError(sendRc, eid, statusIntf, valueIntf);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);

    auto decodeRc = decode_nsm_dot_cak_rotate_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, dotBlob.data());

    lg2::debug("Dot: cakRotate decode result: eid={EID} decodeRc={RC} cc={CC} "
               "reasonCode={REASON}",
               "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode);

    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("Dot: decode_nsm_dot_cak_rotate_resp: eid={EID} rc={RC} "
                   "cc={CC} reasonCode={REASON} len={LEN} msg={MSG}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode,
                   "LEN", responseLen, "MSG", msg);
        auto error = dot::formatDotDeviceError(cc, reasonCode,
                                               NSM_FW_DOT_CAK_ROTATE);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        co_return decodeRc;
    }

    if (dotBlob.empty())
    {
        lg2::error("Dot: cakRotate response blob is empty");
        auto error = std::make_tuple(static_cast<uint16_t>(NSM_SW_ERROR),
                                     "DOT CAK Rotate blob is empty");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return NSM_SW_ERROR;
    }

    lg2::debug("Dot: CAK Rotate successful: eid={EID}", "EID", eid);
    valueIntf->value(dotBlob);
    statusIntf->status(AsyncOperationStatusType::Success);
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path NsmDotObject::disable(
    DotActionIntf::KeyAuthScheme lakKeyAuthScheme, std::string lakEcdsaKey,
    std::string lakLmsKey, DotActionIntf::UnlockMethod unlockMethod,
    std::string staticChallenge,
    DotActionIntf::KeyAuthScheme disableSignatureAuthScheme,
    std::string ecdsaSignature, std::string lmsSignature)
{
    uint8_t lakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
    if (!dot::decodeKeyData(lakEcdsaKey, lakEcdsaBuf, dot::ECDSA_KEY_SIZE))
    {
        lg2::error("Dot: Invalid LAK ECDSA key data");
        throw Common::Error::InvalidArgument();
    }

    if (lakKeyAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        lakLmsKey.empty())
    {
        lg2::error("Dot: Hybrid mode requires LAK LMS key");
        throw Common::Error::InvalidArgument();
    }

    if (disableSignatureAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        lmsSignature.empty())
    {
        lg2::error("Dot: Hybrid signature mode requires LMS signature");
        throw Common::Error::InvalidArgument();
    }

    if (unlockMethod == DotActionIntf::UnlockMethod::StaticValue &&
        staticChallenge.empty())
    {
        lg2::error("Dot: Static unlock method requires static challenge");
        throw Common::Error::InvalidArgument();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    DotDisableParams params{lakKeyAuthScheme, lakEcdsaKey,
                            lakLmsKey,        unlockMethod,
                            staticChallenge,  disableSignatureAuthScheme,
                            ecdsaSignature,   lmsSignature};

    disableAsyncHandler(params, statusIntf, valueIntf).detach();

    return objPath;
}

requester::Coroutine NsmDotObject::disableAsyncHandler(
    const DotDisableParams& params, std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    lg2::debug("Dot: disableAsyncHandler started");

    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(DOT_ERROR_DEVICE_NOT_FOUND, "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    nsm_dot_disable_req dotReq = {};

    if (!buildAndValidateLAKAuthData(params.lakKeyAuthScheme,
                                     params.lakEcdsaKey, params.lakLmsKey,
                                     dotReq.lak_pub, statusIntf, valueIntf))
    {
        co_return NSM_ERR_INVALID_DATA;
    }

    dotReq.unlock_method = static_cast<uint32_t>(params.unlockMethod);

    if (params.unlockMethod == DotActionIntf::UnlockMethod::StaticValue)
    {
        if (!dot::decodeKeyData(params.staticChallenge, dotReq.s_challenge,
                                DOT_STATIC_CHALLENGE_SIZE))
        {
            lg2::error("Dot: Invalid static challenge data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                                "Invalid static challenge"));
            co_return NSM_ERR_INVALID_DATA;
        }
    }
    else
    {
        memset(dotReq.s_challenge, 0, DOT_STATIC_CHALLENGE_SIZE);
    }

    uint8_t ecdsaSigBuf[DOT_SIGNATURE_ECDSA_SIZE] = {0};
    if (!dot::decodeKeyData(params.ecdsaSignature, ecdsaSigBuf,
                            DOT_SIGNATURE_ECDSA_SIZE))
    {
        lg2::error("Dot: Invalid ECDSA signature data");
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                            "Invalid ECDSA signature"));
        co_return NSM_ERR_INVALID_DATA;
    }

    memcpy(dotReq.signature, ecdsaSigBuf, DOT_SIGNATURE_ECDSA_SIZE);

    if (params.disableSignatureAuthScheme ==
        DotActionIntf::KeyAuthScheme::Hybrid)
    {
        uint8_t lmsSigBuf[DOT_SIGNATURE_LMS_SIZE] = {0};
        if (!dot::decodeKeyData(params.lmsSignature, lmsSigBuf,
                                DOT_SIGNATURE_LMS_SIZE))
        {
            lg2::error("Dot: Invalid LMS signature data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                                "Invalid LMS signature"));
            co_return NSM_ERR_INVALID_DATA;
        }
        memcpy(dotReq.signature + DOT_SIGNATURE_ECDSA_SIZE, lmsSigBuf,
               DOT_SIGNATURE_LMS_SIZE);
    }
    else
    {
        memset(dotReq.signature + DOT_SIGNATURE_ECDSA_SIZE, 0,
               DOT_SIGNATURE_LMS_SIZE);
    }

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_disable_req(0, &dotReq, requestMsg);
    std::string msg = utils::requestMsgToHexString(*request);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("Dot: encode_nsm_dot_disable_req: rc={RC}", "RC", rc);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    lg2::debug(
        "Dot: disable request encoded, sending to eid={EID} reqSize={SIZE}",
        "EID", eid, "SIZE", request->size());

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);

    lg2::debug(
        "Dot: disable postPatchIO returned: eid={EID} rc={RC} responseLen={LEN}",
        "EID", eid, "RC", sendRc, "LEN", responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        handleSendError(sendRc, eid, statusIntf, valueIntf);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);

    lg2::debug("Dot: disable response received: eid={EID} len={LEN}", "EID",
               eid, "LEN", responseLen);

    auto decodeRc = decode_nsm_dot_disable_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, dotBlob.data());

    lg2::debug(
        "Dot: disable decode result: eid={EID} decodeRc={RC} cc={CC} reasonCode={REASON}",
        "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode);

    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "Dot: decode_nsm_dot_disable_resp: "
            "eid={EID} rc={RC} cc={CC} reasonCode={REASON} len={LEN} msg={MSG}",
            "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode, "LEN",
            responseLen, "MSG", msg);
        auto error = dot::formatDotDeviceError(cc, reasonCode,
                                               NSM_FW_DOT_DISABLE);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        co_return decodeRc;
    }

    if (dotBlob.empty())
    {
        lg2::info("Dot: disable response blob is empty: eid={EID}", "EID", eid);
        auto error = std::make_tuple(static_cast<uint16_t>(NSM_SW_ERROR),
                                     "DOT Disable blob is empty");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return NSM_SW_ERROR;
    }

    lg2::info("Dot: Disable completed successfully: eid={EID}", "EID", eid);
    valueIntf->value(dotBlob);
    statusIntf->status(AsyncOperationStatusType::Success);
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path NsmDotObject::override(
    DotActionIntf::KeyAuthScheme vendorSignatureAuthScheme,
    std::string ecdsaSignature, std::string lmsSignature)
{
    if (vendorSignatureAuthScheme == DotActionIntf::KeyAuthScheme::Hybrid &&
        lmsSignature.empty())
    {
        lg2::error("Dot: Hybrid signature mode requires LMS signature");
        throw Common::Error::InvalidArgument();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }

    overrideAsyncHandler(vendorSignatureAuthScheme, ecdsaSignature,
                         lmsSignature, statusIntf, valueIntf)
        .detach();

    return objPath;
}

requester::Coroutine NsmDotObject::overrideAsyncHandler(
    DotActionIntf::KeyAuthScheme vendorSignatureAuthScheme,
    std::string ecdsaSignature, std::string lmsSignature,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    lg2::debug("Dot: overrideAsyncHandler started");

    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(DOT_ERROR_DEVICE_NOT_FOUND, "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    nsm_dot_override_req dotReq = {};

    if (vendorSignatureAuthScheme == DotActionIntf::KeyAuthScheme::Ecdsa)
    {
        uint8_t ecdsaSigBuf[DOT_SIGNATURE_ECDSA_SIZE] = {0};
        if (!dot::decodeKeyData(ecdsaSignature, ecdsaSigBuf,
                                DOT_SIGNATURE_ECDSA_SIZE))
        {
            lg2::error("Dot: Invalid ECDSA signature data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                                "Invalid ECDSA signature"));
            co_return NSM_ERR_INVALID_DATA;
        }
        memcpy(dotReq.signature, ecdsaSigBuf, DOT_SIGNATURE_ECDSA_SIZE);
        memset(dotReq.signature + DOT_SIGNATURE_ECDSA_SIZE, 0,
               DOT_SIGNATURE_LMS_SIZE);
    }
    else
    {
        uint8_t ecdsaSigBuf[DOT_SIGNATURE_ECDSA_SIZE] = {0};
        if (!dot::decodeKeyData(ecdsaSignature, ecdsaSigBuf,
                                DOT_SIGNATURE_ECDSA_SIZE))
        {
            lg2::error("Dot: Invalid ECDSA signature data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                                "Invalid ECDSA signature"));
            co_return NSM_ERR_INVALID_DATA;
        }

        uint8_t lmsSigBuf[DOT_SIGNATURE_LMS_SIZE] = {0};
        if (!dot::decodeKeyData(lmsSignature, lmsSigBuf,
                                DOT_SIGNATURE_LMS_SIZE))
        {
            lg2::error("Dot: Invalid LMS signature data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(
                std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                                "Invalid LMS signature"));
            co_return NSM_ERR_INVALID_DATA;
        }

        memcpy(dotReq.signature, ecdsaSigBuf, DOT_SIGNATURE_ECDSA_SIZE);
        memcpy(dotReq.signature + DOT_SIGNATURE_ECDSA_SIZE, lmsSigBuf,
               DOT_SIGNATURE_LMS_SIZE);
    }

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_override_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_override_req(0, &dotReq, requestMsg);
    std::string msg = utils::requestMsgToHexString(*request);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("Dot: encode_nsm_dot_override_req: rc={RC}", "RC", rc);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    lg2::debug(
        "Dot: override request encoded, sending to eid={EID} reqSize={SIZE}",
        "EID", eid, "SIZE", request->size());

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);

    lg2::debug(
        "Dot: override postPatchIO returned: eid={EID} rc={RC} responseLen={LEN}",
        "EID", eid, "RC", sendRc, "LEN", responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        handleSendError(sendRc, eid, statusIntf, valueIntf);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;

    auto decodeRc = decode_nsm_dot_override_resp(responseMsg.get(), responseLen,
                                                 &cc, &reasonCode);

    lg2::debug("Dot: override decode result: eid={EID} decodeRc={RC} cc={CC} "
               "reasonCode={REASON}",
               "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode);

    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "Dot: decode_nsm_dot_override_resp: eid={EID} rc={RC} cc={CC} "
            "reasonCode={REASON} len={LEN} msg={MSG}",
            "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode, "LEN",
            responseLen, "MSG", msg);
        auto error = dot::formatDotDeviceError(cc, reasonCode,
                                               NSM_FW_DOT_OVERRIDE);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        co_return decodeRc;
    }

    lg2::info("Dot: Override completed successfully: eid={EID}", "EID", eid);
    valueIntf->value(std::vector<uint8_t>{});
    statusIntf->status(AsyncOperationStatusType::Success);
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path
    NsmDotObject::recoverDOT(sdbusplus::message::unix_fd dotData)
{
    if (dotData.fd < 0)
    {
        lg2::error("Dot: Invalid file descriptor for DOT blob data");
        throw Common::Error::InvalidArgument();
    }

    auto dupFd = dup(dotData.fd);
    if (dupFd < 0)
    {
        lg2::error("Dot: Failed to duplicate file descriptor: {ERR}", "ERR",
                   strerror(errno));
        throw Common::Error::InternalFailure();
    }

    auto seekRc = lseek(dupFd, 0, SEEK_SET);
    if (seekRc < 0)
    {
        lg2::error("Dot: lseek failed: {RETURNCODE} {ERROR}", "RETURNCODE",
                   seekRc, "ERROR", strerror(errno));
        close(dupFd);
        throw Common::Error::InternalFailure();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        close(dupFd);
        throw Common::Error::Unavailable();
    }

    recoverDOTAsyncHandler(sdbusplus::message::unix_fd(dupFd), statusIntf,
                           valueIntf)
        .detach();

    return objPath;
}

requester::Coroutine NsmDotObject::recoverDOTAsyncHandler(
    sdbusplus::message::unix_fd dotData,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    lg2::debug("Dot: recoveryAsyncHandler started");

    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(DOT_ERROR_DEVICE_NOT_FOUND, "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    nsm_dot_recovery_req dotReq = {};
    std::vector<uint8_t> dotDataBuffer;

    try
    {
        utils::readFdToBuffer(dotData.fd, dotDataBuffer);
    }
    catch (const std::exception& e)
    {
        lg2::error("Dot: Failed to read DOT blob from file descriptor: {ERROR}",
                   "ERROR", e.what());
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                            "Failed to read DOT blob from file descriptor"));
        co_return NSM_ERR_INVALID_DATA;
    }

    if (dotDataBuffer.empty())
    {
        lg2::error("Dot: DOT blob data is empty");
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                            "DOT blob data is empty"));
        co_return NSM_ERR_INVALID_DATA;
    }

    if (dotDataBuffer.size() != DOT_BLOB_SIZE)
    {
        lg2::error(
            "Dot: Invalid DOT blob size: expected={EXPECTED}, actual={ACTUAL}",
            "EXPECTED", DOT_BLOB_SIZE, "ACTUAL", dotDataBuffer.size());
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(NSM_ERR_INVALID_DATA),
                            "Invalid DOT blob size"));
        co_return NSM_ERR_INVALID_DATA;
    }

    std::copy_n(dotDataBuffer.data(), DOT_BLOB_SIZE, dotReq.dot_blob);

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_recovery_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_recovery_req(0, &dotReq, requestMsg);
    std::string msg = utils::requestMsgToHexString(*request);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("Dot: encode_nsm_dot_recovery_req: rc={RC}", "RC", rc);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        throw Common::Error::InternalFailure();
    }

    lg2::debug(
        "Dot: recovery request encoded, sending to eid={EID} reqSize={SIZE}",
        "EID", eid, "SIZE", request->size());

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await device->postPatchIO(eid, *request, responseMsg,
                                               responseLen);

    lg2::debug(
        "Dot: recovery postPatchIO returned: eid={EID} rc={RC} responseLen={LEN}",
        "EID", eid, "RC", sendRc, "LEN", responseLen);

    if (sendRc != NSM_SW_SUCCESS)
    {
        handleSendError(sendRc, eid, statusIntf, valueIntf);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;

    auto decodeRc = decode_nsm_dot_recovery_resp(responseMsg.get(), responseLen,
                                                 &cc, &reasonCode);

    lg2::debug("Dot: recovery decode result: eid={EID} decodeRc={RC} cc={CC} "
               "reasonCode={REASON}",
               "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode);

    if (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "Dot: decode_nsm_dot_recovery_resp: eid={EID} rc={RC} cc={CC} "
            "reasonCode={REASON} len={LEN} msg={MSG}",
            "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode, "LEN",
            responseLen, "MSG", msg);
        auto error = dot::formatDotDeviceError(cc, reasonCode,
                                               NSM_FW_DOT_RECOVERY);
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        co_return decodeRc;
    }

    valueIntf->value(std::vector<uint8_t>{});
    statusIntf->status(AsyncOperationStatusType::Success);
    co_return NSM_SW_SUCCESS;
}

static requester::Coroutine createNsmDot(SensorManager& manager,
                                         const std::string& interface,
                                         const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap allBaseIfaceProperties;
    auto rc = co_await utils::coGetCachedBaseProperties(objPath, interface,
                                                        allBaseIfaceProperties);
    if (rc != NSM_SUCCESS)
    {
        co_return rc;
    }

    std::string chassisName{};
    if (allBaseIfaceProperties.count("ChassisName"))
    {
        chassisName =
            std::get<std::string>(allBaseIfaceProperties.at("ChassisName"));
    }

    uuid_t uuid{};
    if (allBaseIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
    }

    auto device = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Failed to find device for UUID in chassis: {CHASSIS}",
                   "CHASSIS", chassisName);
        co_return NSM_SW_ERROR;
    }

    auto object = std::make_shared<NsmDotObject>(bus, chassisName, uuid);
    auto dotStatusSensor = std::make_shared<NsmDotStatusObject>(
        chassisName, "NSM_Dot_Status", object.get(), uuid);
    object->dotStatusSensor_ = dotStatusSensor.get();
    device->addStaticSensor(object);
    device->addSensor(dotStatusSensor, PollingType::RoundRobin);

    lg2::debug(
        "Dot: Created DOT object and status sensor for chassis: {CHASSIS}",
        "CHASSIS", chassisName);

    co_return NSM_SUCCESS;
}

NsmDotStatusObject::NsmDotStatusObject(const std::string& name,
                                       const std::string& type,
                                       DotActionIntf* dotActionIntf,
                                       const uuid_t& uuid) :
    NsmSensor(name, type), dotActionIntf_(dotActionIntf), uuid_(uuid)
{
    std::copy(std::begin(uuid), std::end(uuid), std::begin(uuid_));
}

std::optional<std::vector<uint8_t>>
    NsmDotStatusObject::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_status_req(instanceId, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmDotStatusObject: encode_nsm_dot_get_status_req failed: eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmDotStatusObject::handleResponseMsg(const nsm_msg* responseMsg,
                                              size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint8_t status = 0;

    auto rc = decode_nsm_dot_get_status_resp(responseMsg, responseLen, &cc,
                                             &reasonCode, &status);

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::debug(
            "NsmDotStatusObject: decode_nsm_dot_get_status_resp failed: rc={RC} cc={CC} reason={REASON}",
            "RC", rc, "CC", cc, "REASON", reasonCode);
        return cc ? cc : rc;
    }

    std::string statusText;
    DotActionIntf::DOTStates currentDotState;

    switch (status & DOT_STATUS_MASK)
    {
        case DOT_STATUS_UNINITIALIZED:
            statusText = "Uninitialized";
            currentDotState = DotActionIntf::DOTStates::Uninitialized;
            break;
        case DOT_STATUS_VOLATILE:
            statusText = "Volatile";
            currentDotState = DotActionIntf::DOTStates::Volatile;
            break;
        case DOT_STATUS_LOCKED:
            statusText = "Locked";
            currentDotState = DotActionIntf::DOTStates::Locked;
            break;
        case DOT_STATUS_DISABLED:
            statusText = "Disabled";
            currentDotState = DotActionIntf::DOTStates::Disabled;
            break;
        default:
            if (shouldLog("Unexpected DOT status", nsm_sw_codes(status)))
            {
                lg2::error("Dot: Unexpected DOT status received: {STATUS}",
                           "STATUS", status);
            }
            statusText = "Unknown";
            currentDotState = DotActionIntf::DOTStates::Uninitialized;
            break;
    }

    lg2::debug("Dot: Status update: status={STATUS} ({TEXT})", "STATUS", status,
               "TEXT", statusText);

    dotActionIntf_->dotState(currentDotState);

    return NSM_SW_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(createNsmDot,
                               "xyz.openbmc_project.Configuration.NSM_DOT")

} // namespace nsm
