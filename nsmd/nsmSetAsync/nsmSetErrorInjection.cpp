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
#include "nsmSetErrorInjection.hpp"

#include "device-configuration.h"

#include "nsmErrorInjection/nsmErrorInjection.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace nsm
{

NsmSetErrorInjection::NsmSetErrorInjection(SensorManager& manager,
                                           const path& objPath) :
    NsmInterfaceProvider<ErrorInjectionIntf>("ErrorInjection",
                                             "NSM_ErrorInjection", objPath),
    manager(manager)
{}

requester::Coroutine NsmSetErrorInjection::errorInjectionModeEnabled(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const auto enabledValue = std::get_if<bool>(&value);

    if (!enabledValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    // coverity[missing_return]
    co_return co_await setModeEnabled(*enabledValue, *status, device);
}
requester::Coroutine
    NsmSetErrorInjection::setModeEnabled(bool value,
                                         AsyncOperationStatusType& status,
                                         std::shared_ptr<NsmDevice> device)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_error_injection_mode_v1_req));

    auto eid = manager.getEid(device);
    auto mode = uint8_t(value);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_set_error_injection_mode_v1_req(0, mode, requestPtr);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_set_error_injection_mode_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await device->postPatchIO(eid, request, responseMsg, responseLen);
    if (rc)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error(
                "NsmSetErrorInjection::setModeEnabled: postPatchIO failed."
                "eid={EID} rc={RC}",
                "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        }
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_set_error_injection_mode_v1_resp(responseMsg.get(), responseLen,
                                                 &cc, &reasonCode);
    if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmSetErrorInjection::setModeEnabled: decode_set_error_injection_mode_v1_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmSetErrorInjectionCapabilities::NsmSetErrorInjectionCapabilities(
    const std::string& name, SensorManager& manager,
    const Interfaces<ErrorInjectionCapabilityIntf>& interfaces,
    std::shared_ptr<NsmErrorInjectionEnabled> enabledSensor) :
    NsmInterfaceContainer<ErrorInjectionCapabilityIntf>(interfaces),
    NsmObject(name, "NSM_ErrorInjectionCapability"), manager(manager),
    enabledSensor(std::move(enabledSensor))
{
    if (!this->enabledSensor)
    {
        throw std::invalid_argument(
            "NsmSetErrorInjectionCapabilities: enabledSensor cannot be null");
    }
}

std::optional<ErrorInjectionCapabilityIntf::Type>
    NsmSetErrorInjectionCapabilities::resolveTypeName(const std::string& name)
{
    // Match against types registered on this device, not the full enum, so a
    // capability the device does not expose is rejected.
    std::optional<ErrorInjectionCapabilityIntf::Type> resolved;
    invoke([&resolved, &name](const auto& pdi) {
        auto type = pdi.type();
        if (type == ErrorInjectionCapabilityIntf::Type::Unknown)
        {
            return;
        }
        auto typeName = ErrorInjectionCapabilityIntf::convertTypeToString(type);
        typeName = typeName.substr(typeName.find_last_of('.') + 1);
        if (typeName == name)
        {
            resolved = type;
        }
    });
    return resolved;
}

requester::Coroutine NsmSetErrorInjectionCapabilities::capabilitiesEnabled(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const auto* entries = std::get_if<std::vector<
        std::tuple<std::string, std::variant<bool, uint32_t, double,
                                             std::vector<uint8_t>>>>>(&value);

    if (!entries)
    {
        lg2::error(
            "capabilitiesEnabled: expected an array of (name, bool) entries");
        *status = AsyncOperationStatusType::InvalidArgument;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }

    if (entries->empty())
    {
        lg2::error("capabilitiesEnabled: empty capability list");
        *status = AsyncOperationStatusType::InvalidArgument;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }

    // Validate the whole batch before touching the device, tracking conflicts
    // by mask bit rather than type so aliased types cannot disagree.
    std::map<ErrorInjectionCapabilityIntf::Type, bool> overrides;
    std::map<uint8_t, bool> requestedBits;
    for (const auto& [name, entryValue] : *entries)
    {
        const auto* enabledValue = std::get_if<bool>(&entryValue);
        if (!enabledValue)
        {
            lg2::error("capabilitiesEnabled: value for {NAME} is not a boolean",
                       "NAME", name);
            *status = AsyncOperationStatusType::InvalidArgument;
            // coverity[missing_return]
            co_return NSM_SW_ERROR_DATA;
        }

        auto type = resolveTypeName(name);
        if (!type)
        {
            lg2::error(
                "capabilitiesEnabled: unsupported capability type {NAME}",
                "NAME", name);
            *status = AsyncOperationStatusType::InvalidArgument;
            // coverity[missing_return]
            co_return NSM_SW_ERROR_DATA;
        }

        // One bit asked to be both set and cleared is a conflict; repeating a
        // bit with the same value stays valid so snapshots are idempotent.
        auto bitPos = getErrorInjectionBitPosition(*type);
        auto [bitIt, bitInserted] = requestedBits.emplace(bitPos,
                                                          *enabledValue);
        if (!bitInserted && bitIt->second != *enabledValue)
        {
            lg2::error(
                "capabilitiesEnabled: conflicting values requested for capability {NAME}",
                "NAME", name);
            *status = AsyncOperationStatusType::InvalidArgument;
            // coverity[missing_return]
            co_return NSM_SW_ERROR_DATA;
        }

        overrides[*type] = *enabledValue;
    }

    // coverity[missing_return]
    co_return co_await applyEnabledOverrides(overrides, *status, device);
}

requester::Coroutine NsmSetErrorInjectionCapabilities::applyEnabledOverrides(
    const std::map<ErrorInjectionCapabilityIntf::Type, bool>& overrides,
    AsyncOperationStatusType& status, std::shared_ptr<NsmDevice> device)
{
    // Queue behind any in-flight mask write. writeMask() reads the mask back
    // and republishes it before releasing, so each waiter seeds from what the
    // device actually holds rather than from pre-write state.
    co_await maskSemaphore.acquire(device->getEid());
    uint8_t rc = NSM_SW_ERROR;
    try
    {
        rc = co_await writeMask(overrides, status, device);
    }
    catch (const std::exception& e)
    {
        lg2::error("applyEnabledOverrides: Exception during writeMask: {ERROR}",
                   "ERROR", e.what());
        status = AsyncOperationStatusType::WriteFailure;
        rc = NSM_SW_ERROR;
    }
    // Released on exactly one path, so a throw cannot leak the lock and a
    // double release cannot hand it to two waiters at once.
    maskSemaphore.release();
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmSetErrorInjectionCapabilities::writeMask(
    const std::map<ErrorInjectionCapabilityIntf::Type, bool>& overrides,
    AsyncOperationStatusType& status, std::shared_ptr<NsmDevice> device)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_error_injection_types_mask_req));

    auto eid = manager.getEid(device);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());

    // Re-key overrides by mask bit, since several types share one bit. Keying
    // by type would let an untouched alias whose PDI still reads true OR the
    // bit back on and drop a disable.
    std::map<uint8_t, bool> bitOverrides;
    for (const auto& [type, enabled] : overrides)
    {
        bitOverrides[getErrorInjectionBitPosition(type)] = enabled;
    }

    // Seeded from every sibling's current state; bit positions are EI enum
    // values, not D-Bus Type ordinals.
    nsm_error_injection_types_mask data = {0, 0, 0, 0, 0, 0, 0, 0};
    invoke([&data, &bitOverrides](const auto& pdi) {
        if (pdi.type() == ErrorInjectionCapabilityIntf::Type::Unknown)
        {
            return;
        }
        auto bitPos = getErrorInjectionBitPosition(pdi.type());
        auto found = bitOverrides.find(bitPos);
        auto setValue = int(found != bitOverrides.end() ? found->second
                                                        : pdi.enabled());
        data.mask[bitPos / 8] |= setValue << (bitPos % 8);
    });
    auto rc = encode_set_current_error_injection_types_v1_req(0, &data,
                                                              requestPtr);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_set_current_error_injection_types_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await device->postPatchIO(eid, request, responseMsg, responseLen);
    if (rc)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error(
                "NsmSetErrorInjectionCapabilities::writeMask: postPatchIO failed."
                "eid={EID} rc={RC}",
                "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        }
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_set_current_error_injection_types_v1_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);
    if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmSetErrorInjectionCapabilities::writeMask: decode_set_current_error_injection_types_v1_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
    }
    else
    {
        // Reuse the polling sensor's read flow: update() issues the same
        // GetCurrentErrorInjectionTypes and republishes every PDI from the
        // response. setImpl runs the same refresh after the handler returns;
        // repeating it here, inside the lock, is what lets the next waiter
        // reseed from device state instead of from what we asked for.
        const auto readRc = co_await enabledSensor->update(device);
        if (readRc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "NsmSetErrorInjectionCapabilities::writeMask: read-back failed, publishing the acknowledged mask. eid={EID} rc={RC}",
                "EID", eid, "RC", readRc);
            invoke([&data](auto& pdi) {
                if (pdi.type() == ErrorInjectionCapabilityIntf::Type::Unknown)
                {
                    return;
                }
                auto bitPos = getErrorInjectionBitPosition(pdi.type());
                pdi.enabled((data.mask[bitPos / 8] & (1 << (bitPos % 8))) != 0);
            });
        }
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmSetErrorInjectionEnabled::NsmSetErrorInjectionEnabled(
    const std::string& name, ErrorInjectionCapabilityIntf::Type type,
    const Interfaces<ErrorInjectionCapabilityIntf>& interfaces,
    std::shared_ptr<NsmSetErrorInjectionCapabilities> capabilities) :
    NsmInterfaceContainer<ErrorInjectionCapabilityIntf>(interfaces),
    NsmObject(name, "NSM_ErrorInjectionCapability"), type(type),
    capabilities(std::move(capabilities))
{
    if (type == ErrorInjectionCapabilityIntf::Type::Unknown)
    {
        throw std::invalid_argument(
            "NsmSetErrorInjectionEnabled::ctor: PDI type cannot be Unknown");
    }
    if (!this->capabilities)
    {
        throw std::invalid_argument(
            "NsmSetErrorInjectionEnabled::ctor: capabilities cannot be null");
    }
}

requester::Coroutine NsmSetErrorInjectionEnabled::enabled(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const auto enabledValue = std::get_if<bool>(&value);

    if (!enabledValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    // coverity[missing_return]
    co_return co_await setEnabled(*enabledValue, *status, device);
}

requester::Coroutine
    NsmSetErrorInjectionEnabled::setEnabled(bool value,
                                            AsyncOperationStatusType& status,
                                            std::shared_ptr<NsmDevice> device)
{
    // Single-type patches are just a one-entry batch. Routing them through the
    // same owner keeps one read-modify-write implementation and one lock, so a
    // per-type write and a batch write cannot interleave.
    // coverity[missing_return]
    co_return co_await capabilities->applyEnabledOverrides({{type, value}},
                                                           status, device);
}

NsmSetErrorInjectionPayload::NsmSetErrorInjectionPayload(
    const std::string& name, SensorManager& manager,
    const Interfaces<ErrorInjectionPayloadIntf>& interfaces,
    std::shared_ptr<NsmActivateErrorInjectionPayloadIntf> activateIntf,
    uint16_t errorInjectionType, uint16_t errorInjectionSubtype) :
    NsmInterfaceContainer<ErrorInjectionPayloadIntf>(interfaces),
    NsmObject(name, "NSM_ErrorInjectionPayload"), manager(manager),
    activateIntf(activateIntf), errorInjectionType(errorInjectionType),
    errorInjectionSubtype(errorInjectionSubtype)
{}

requester::Coroutine NsmSetErrorInjectionPayload::setErrorInjectionPayload(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    std::vector<uint8_t> faultPayload;
    const auto* payloadValue = std::get_if<std::vector<
        std::tuple<std::string, std::variant<bool, uint32_t, double,
                                             std::vector<uint8_t>>>>>(&value);

    if (!payloadValue)
    {
        lg2::error(
            "setErrorInjectionPayload: Failed to get payload values - invalid type");
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (payloadValue->empty())
    {
        lg2::error("setErrorInjectionPayload: Empty payload values list");
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    for (const auto& [key, val] : *payloadValue)
    {
        if (key == "FaultBitMap")
        {
            const auto* faultBitMapValue =
                std::get_if<std::vector<uint8_t>>(&val);
            if (!faultBitMapValue)
            {
                lg2::error(
                    "setErrorInjectionPayload: Invalid type for FaultBitMap");
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            faultPayload = *faultBitMapValue;
        }
        else
        {
            lg2::error("setErrorInjectionPayload: Unrecognized key {KEY}",
                       "KEY", key);
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
    }
    co_return co_await setPayload(faultPayload, *status, device);
}

requester::Coroutine
    NsmSetErrorInjectionPayload::setPayload(std::vector<uint8_t> faultBitMap,
                                            AsyncOperationStatusType& status,
                                            std::shared_ptr<NsmDevice> device)
{
    auto eid = manager.getEid(device);
    if (errorInjectionType == EI_DEVICE_ERRORS)
    {
        // Insert 2 bytes at front for subtype
        faultBitMap.insert(faultBitMap.begin(), 2, 0);
        *reinterpret_cast<uint16_t*>(faultBitMap.data()) =
            errorInjectionSubtype;
    }

    size_t requestSize = sizeof(nsm_msg_hdr) +
                         sizeof(nsm_set_error_injection_payload_req) -
                         sizeof(uint8_t) + faultBitMap.size();
    Request request(requestSize);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_set_error_injection_payload_req(
        0, faultBitMap.data(), faultBitMap.size(), errorInjectionType,
        errorInjectionSubtype, requestPtr);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_set_error_injection_payload_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await device->postPatchIO(eid, request, responseMsg, responseLen);
    if (rc)
    {
        lg2::error(
            "NsmSetErrorInjectionPayload::setPayload: postPatchIO failed."
            "eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_set_error_injection_payload_resp(responseMsg.get(), responseLen,
                                                 &cc, &reasonCode);
    LG2_ERROR_FLT(
        "NsmSetErrorInjectionPayload::setPayload failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
    {
        status = AsyncOperationStatusType::WriteFailure;
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

} // namespace nsm
