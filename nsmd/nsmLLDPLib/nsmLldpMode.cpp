/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION &
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
#include "nsmLldpMode.hpp"

#include "device-configuration.h"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstring>

namespace nsm
{

namespace
{

uint8_t lldpModeTypeToRaw(NsmLldpMode::LLDPModeType v)
{
    using LLDPModeType = NsmLldpMode::LLDPModeType;
    switch (v)
    {
        case LLDPModeType::Off:
            return NSM_LLDP_DIR_MODE_OFF;
        case LLDPModeType::Mandatory:
            return NSM_LLDP_DIR_MODE_MANDATORY;
        case LLDPModeType::All:
            return NSM_LLDP_DIR_MODE_ALL;
    }
    return NSM_LLDP_DIR_MODE_OFF;
}

NsmLldpMode::LLDPModeType rawToLldpModeType(uint8_t v)
{
    using LLDPModeType = NsmLldpMode::LLDPModeType;
    switch (v)
    {
        case NSM_LLDP_DIR_MODE_OFF:
            return LLDPModeType::Off;
        case NSM_LLDP_DIR_MODE_MANDATORY:
            return LLDPModeType::Mandatory;
        case NSM_LLDP_DIR_MODE_ALL:
            return LLDPModeType::All;
        default:
            lg2::warning(
                "NsmLldpMode: device returned reserved/invalid TX/RX value {VAL}; mapping to Off",
                "VAL", v);
            return LLDPModeType::Off;
    }
}

uint8_t dcbxModeTypeToRaw(NsmLldpMode::DCBXModeType v)
{
    using DCBXModeType = NsmLldpMode::DCBXModeType;
    return v == DCBXModeType::Enabled ? NSM_LLDP_DCBX_ENABLED
                                      : NSM_LLDP_DCBX_DISABLED;
}

NsmLldpMode::DCBXModeType rawToDcbxModeType(uint8_t v)
{
    using DCBXModeType = NsmLldpMode::DCBXModeType;
    return v == NSM_LLDP_DCBX_ENABLED ? DCBXModeType::Enabled
                                      : DCBXModeType::Disabled;
}

} // namespace

NsmLldpMode::NsmLldpMode(sdbusplus::bus_t& bus, const std::string& name,
                         const std::string& type, const std::string& objectPath,
                         const std::string& networkAdapterPath,
                         std::shared_ptr<NsmDevice> device) :
    NsmSensor(name, type), objectPath_(objectPath), device_(std::move(device))
{
    intf_ = std::make_shared<LLDPModesIntf>(bus, objectPath_.c_str());
    intf_->txMode(LLDPModeType::Off);
    intf_->rxMode(LLDPModeType::Off);
    intf_->dcbxMode(DCBXModeType::Disabled);
    intf_->associations(
        {{"network_adapter", "lldp_mode_settings", networkAdapterPath}});

    lg2::info("NsmLldpMode created: name={NAME} path={PATH}", "NAME", name,
              "PATH", objectPath_);
}

NsmLldpMode::LLDPModeType NsmLldpMode::txMode() const
{
    return intf_->txMode();
}
NsmLldpMode::LLDPModeType NsmLldpMode::rxMode() const
{
    return intf_->rxMode();
}
NsmLldpMode::DCBXModeType NsmLldpMode::dcbxMode() const
{
    return intf_->dcbxMode();
}

std::optional<std::vector<uint8_t>>
    NsmLldpMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_req), 0);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, static_cast<uint32_t>(DEVICE_MODE_LLDP), requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmLldpMode: encode_get_device_mode_settings_v2_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmLldpMode::handleResponseMsg(const nsm_msg* responseMsg,
                                       size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    constexpr size_t maxModeBytes = 16;
    uint8_t currentData[maxModeBytes] = {};
    uint8_t pendingData[maxModeBytes] = {};
    uint16_t currentLength = 0;
    uint16_t pendingLength = 0;

    auto rc = decode_get_device_mode_settings_v2_resp(
        responseMsg, responseLen, &cc, &reasonCode, currentData, &currentLength,
        pendingData, &pendingLength);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmLldpMode: decode_get_device_mode_settings_v2_resp failed. reasonCode={RC2} cc={CC} rc={RC}",
            "RC2", reasonCode, "CC", cc, "RC", rc);
        return cc ? cc : rc;
    }
    if (currentLength != sizeof(nsm_lldp_mode_bitfield))
    {
        lg2::error(
            "NsmLldpMode: device returned invalid current-mode payload length {LEN} for idx 24 (expected 1 byte)",
            "LEN", currentLength);
        return NSM_SW_ERROR_LENGTH;
    }

    struct nsm_lldp_mode_bitfield view = {};
    memcpy(&view, &currentData[0], sizeof(view));

    intf_->txMode(rawToLldpModeType(view.tx_mode));
    intf_->rxMode(rawToLldpModeType(view.rx_mode));
    intf_->dcbxMode(rawToDcbxModeType(view.dcbx_mode));
    return NSM_SUCCESS;
}

bool NsmLldpMode::dcbxPrecheck(LLDPModeType tx, LLDPModeType rx,
                               DCBXModeType dcbx)
{
    if (dcbx != DCBXModeType::Enabled)
    {
        return true;
    }
    return tx == LLDPModeType::All && rx == LLDPModeType::All;
}

requester::Coroutine NsmLldpMode::applyBitfield(
    LLDPModeType tx, LLDPModeType rx, DCBXModeType dcbx,
    AsyncOperationStatusType* status, std::shared_ptr<NsmDevice> nsmDevice)
{
    struct nsm_lldp_mode_bitfield view = {};
    view.tx_mode = lldpModeTypeToRaw(tx);
    view.rx_mode = lldpModeTypeToRaw(rx);
    view.dcbx_mode = dcbxModeTypeToRaw(dcbx);

    uint8_t encoded = 0;
    memcpy(&encoded, &view, sizeof(encoded));

    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req), 0);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_settings_v2_req(
        0, static_cast<uint32_t>(DEVICE_MODE_LLDP), &encoded, sizeof(encoded),
        requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmLldpMode: encode_set_device_mode_settings_v2_req failed rc={RC}",
            "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto ioRc = co_await nsmDevice->postPatchIO(nsmDevice->getEid(), request,
                                                responseMsg, responseLen);
    if (ioRc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmLldpMode::applyBitfield postPatchIO failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", ioRc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return ioRc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    rc = decode_set_device_mode_settings_v2_resp(responseMsg.get(), responseLen,
                                                 &cc, &reasonCode);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmLldpMode::applyBitfield Set response failed. rc={RC} cc={CC} reasonCode={RCN}",
            "RC", rc, "CC", cc, "RCN", reasonCode);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return cc ? cc : rc;
    }

    intf_->txMode(tx);
    intf_->rxMode(rx);
    intf_->dcbxMode(dcbx);
    *status = AsyncOperationStatusType::Success;
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmLldpMode::setTxMode(const AsyncSetOperationValueType& value,
                           AsyncOperationStatusType* status,
                           std::shared_ptr<NsmDevice> nsmDevice)
{
    if (asyncPatchInProgress_)
    {
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }
    asyncPatchInProgress_ = true;

    const std::string* enumStr = std::get_if<std::string>(&value);
    if (!enumStr)
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress_ = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    LLDPModeType requested;
    try
    {
        requested = sdbusplus::server::com::nvidia::network::lldp::Modes::
            convertLLDPModeTypeFromString(*enumStr);
    }
    catch (const sdbusplus::exception::InvalidEnumString&)
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress_ = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (!dcbxPrecheck(requested, intf_->rxMode(), intf_->dcbxMode()))
    {
        lg2::info(
            "NsmLldpMode::setTxMode rejected: DCBX=Enabled requires TX=All & RX=All");
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress_ = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    auto rc = co_await applyBitfield(requested, intf_->rxMode(),
                                     intf_->dcbxMode(), status, nsmDevice);
    asyncPatchInProgress_ = false;
    co_return rc;
}

requester::Coroutine
    NsmLldpMode::setRxMode(const AsyncSetOperationValueType& value,
                           AsyncOperationStatusType* status,
                           std::shared_ptr<NsmDevice> nsmDevice)
{
    if (asyncPatchInProgress_)
    {
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }
    asyncPatchInProgress_ = true;

    const std::string* enumStr = std::get_if<std::string>(&value);
    if (!enumStr)
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress_ = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    LLDPModeType requested;
    try
    {
        requested = sdbusplus::server::com::nvidia::network::lldp::Modes::
            convertLLDPModeTypeFromString(*enumStr);
    }
    catch (const sdbusplus::exception::InvalidEnumString&)
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress_ = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (!dcbxPrecheck(intf_->txMode(), requested, intf_->dcbxMode()))
    {
        lg2::info(
            "NsmLldpMode::setRxMode rejected: DCBX=Enabled requires TX=All & RX=All");
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress_ = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    auto rc = co_await applyBitfield(intf_->txMode(), requested,
                                     intf_->dcbxMode(), status, nsmDevice);
    asyncPatchInProgress_ = false;
    co_return rc;
}

requester::Coroutine
    NsmLldpMode::setDcbxMode(const AsyncSetOperationValueType& value,
                             AsyncOperationStatusType* status,
                             std::shared_ptr<NsmDevice> nsmDevice)
{
    if (asyncPatchInProgress_)
    {
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }
    asyncPatchInProgress_ = true;

    const std::string* enumStr = std::get_if<std::string>(&value);
    if (!enumStr)
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress_ = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    DCBXModeType requested;
    try
    {
        requested = sdbusplus::server::com::nvidia::network::lldp::Modes::
            convertDCBXModeTypeFromString(*enumStr);
    }
    catch (const sdbusplus::exception::InvalidEnumString&)
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress_ = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (!dcbxPrecheck(intf_->txMode(), intf_->rxMode(), requested))
    {
        lg2::info(
            "NsmLldpMode::setDcbxMode rejected: DCBX=Enabled requires TX=All & RX=All");
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress_ = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    auto rc = co_await applyBitfield(intf_->txMode(), intf_->rxMode(),
                                     requested, status, nsmDevice);
    asyncPatchInProgress_ = false;
    co_return rc;
}

} // namespace nsm
