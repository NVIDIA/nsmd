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
#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmDotUtils.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <cstring>
#include <exception>
#include <format>
#include <memory>
#include <sstream>
#include <tuple>
#include <vector>

namespace nsm
{

NsmDotObject::NsmDotObject(sdbusplus::bus::bus& bus, const std::string& name,
                           const uuid_t& uuid) :
    NsmObject(name, "NSM_Dot"),
    DotActionIntf(bus, (dotObjectBasePath / name).c_str()), uuid(uuid)
{
    lg2::debug("Dot: create Unified object: {PATH}", "PATH",
               dotObjectBasePath / name);
}

void NsmDotObject::handleSendError(int sendRc, int eid,
                                   std::shared_ptr<AsyncStatusIntf> statusIntf,
                                   std::shared_ptr<AsyncValueIntf> valueIntf)
{
    // Suppress timeout error logs as they're already logged in nsmDevice.cpp
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

requester::Coroutine NsmDotObject::dotCAKInstallAsyncHandler(
    DotActionIntf::KeyAuthScheme cakKeyAuthScheme, std::string cakEcdsaKey,
    std::string cakLmsKey, DotActionIntf::KeyAuthScheme lakKeyAuthScheme,
    std::string lakEcdsaKey, std::string lakLmsKey, bool lockDisable,
    uint32_t minSvn, std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(0xFFFF), "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    uint8_t cakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
    uint8_t cakLmsBuf[dot::LMS_KEY_SIZE] = {0};
    uint8_t lakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
    uint8_t lakLmsBuf[dot::LMS_KEY_SIZE] = {0};

    if (!dot::decodeKeyData(cakEcdsaKey, cakEcdsaBuf, dot::ECDSA_KEY_SIZE))
    {
        lg2::error("Dot: Invalid CAK ECDSA key data");
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        valueIntf->value(std::make_tuple(static_cast<uint16_t>(0xFFFF),
                                         "Invalid CAK ECDSA key data"));
        co_return NSM_ERR_INVALID_DATA;
    }

    if (!cakLmsKey.empty() &&
        !dot::decodeKeyData(cakLmsKey, cakLmsBuf, dot::LMS_KEY_SIZE))
    {
        lg2::error("Dot: Invalid CAK LMS key data");
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        valueIntf->value(std::make_tuple(static_cast<uint16_t>(0xFFFF),
                                         "Invalid CAK LMS key data"));
        co_return NSM_ERR_INVALID_DATA;
    }

    if (!lakEcdsaKey.empty())
    {
        if (!dot::decodeKeyData(lakEcdsaKey, lakEcdsaBuf, dot::ECDSA_KEY_SIZE))
        {
            lg2::error("Dot: Invalid LAK ECDSA key data");
            statusIntf->status(AsyncOperationStatusType::InvalidArgument);
            valueIntf->value(std::make_tuple(static_cast<uint16_t>(0xFFFF),
                                             "Invalid LAK ECDSA key data"));
            co_return NSM_ERR_INVALID_DATA;
        }
    }

    if (!lakLmsKey.empty() &&
        !dot::decodeKeyData(lakLmsKey, lakLmsBuf, dot::LMS_KEY_SIZE))
    {
        lg2::error("Dot: Invalid LAK LMS key data");
        statusIntf->status(AsyncOperationStatusType::InvalidArgument);
        valueIntf->value(std::make_tuple(static_cast<uint16_t>(0xFFFF),
                                         "Invalid LAK LMS key data"));
        co_return NSM_ERR_INVALID_DATA;
    }

    nsm_dot_cak_install_req dotReq;
    uint32_t cakScheme =
        (cakKeyAuthScheme == DotActionIntf::KeyAuthScheme::Ecdsa) ? 0 : 1;
    uint32_t lakScheme =
        (lakKeyAuthScheme == DotActionIntf::KeyAuthScheme::Ecdsa) ? 0 : 1;

    if (!dot::buildKeyAuthData(cakScheme, cakEcdsaBuf, cakLmsBuf,
                               dotReq.cak_pub))
    {
        lg2::error("Dot: Failed to build CAK auth data");
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return NSM_SW_ERROR;
    }

    if (!dot::buildKeyAuthData(lakScheme, lakEcdsaBuf, lakLmsBuf,
                               dotReq.lak_pub))
    {
        lg2::error("Dot: Failed to build LAK auth data");
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return NSM_SW_ERROR;
    }

    dotReq.lock_disable = lockDisable ? 1 : 0;
    dotReq.min_svn = minSvn;

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_install_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_cak_install_req(0, &dotReq, requestMsg);
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
        "EID", eid, "RC", sendRc, "LEN", responseLen);

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
        lg2::error("Dot: decode_nsm_dot_cak_install_resp: "
                   "eid={EID} rc={RC} cc={CC} reasonCode={REASON} len={LEN}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode,
                   "LEN", responseLen);
        auto error = std::make_tuple(reasonCode, "DOT CAK Install failed");
        valueIntf->value(error);
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        co_return decodeRc;
    }

    valueIntf->value(std::make_tuple(static_cast<uint16_t>(cc), "Success"));
    statusIntf->status(AsyncOperationStatusType::Success);
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDotObject::bypassAsyncHandler(
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Dot: Device not found for UUID");
        statusIntf->status(AsyncOperationStatusType::ResourceNotFound);
        valueIntf->value(
            std::make_tuple(static_cast<uint16_t>(0xFFFF), "Device not found"));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    lg2::debug("Dot: bypassAsyncHandler starting: eid={EID}", "EID", eid);

    auto request = std::make_shared<Request>(sizeof(nsm_msg_hdr) +
                                             sizeof(nsm_dot_cak_bypass_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    auto rc = encode_nsm_dot_cak_bypass_req(0, requestMsg);
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
        "EID", eid, "RC", sendRc, "LEN", responseLen);

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
        lg2::error("Dot: decode_nsm_dot_cak_bypass_resp: "
                   "eid={EID} rc={RC} cc={CC} reasonCode={REASON} len={LEN}",
                   "EID", eid, "RC", decodeRc, "CC", cc, "REASON", reasonCode,
                   "LEN", responseLen);
        auto error = std::make_tuple(reasonCode, "DOT CAK Bypass failed");
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
    // Validate CAK ECDSA key
    uint8_t cakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
    if (!dot::decodeKeyData(cakEcdsaKey, cakEcdsaBuf, dot::ECDSA_KEY_SIZE))
    {
        lg2::error("Dot: Invalid CAK ECDSA key data");
        throw Common::Error::InvalidArgument();
    }

    // Validate LAK ECDSA key if provided
    if (!lakEcdsaKey.empty())
    {
        uint8_t lakEcdsaBuf[dot::ECDSA_KEY_SIZE] = {0};
        if (!dot::decodeKeyData(lakEcdsaKey, lakEcdsaBuf, dot::ECDSA_KEY_SIZE))
        {
            lg2::error("Dot: Invalid LAK ECDSA key data");
            throw Common::Error::InvalidArgument();
        }
    }

    // Validate Hybrid mode requirements
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
    auto object = std::make_shared<NsmDotObject>(bus, chassisName, uuid);
    device->addStaticSensor(object);

    lg2::debug("Dot: Created DOT object for chassis: {CHASSIS}", "CHASSIS",
               chassisName);

    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(createNsmDot,
                               "xyz.openbmc_project.Configuration.NSM_DOT")

} // namespace nsm
