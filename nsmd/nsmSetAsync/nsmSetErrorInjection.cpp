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
    rc = co_await manager.postPatchNsmCommand(eid, request, responseMsg,
                                              responseLen);
    if (rc)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error(
                "NsmSetErrorInjection::setModeEnabled: postPatchNsmCommand failed."
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
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

NsmSetErrorInjectionEnabled::NsmSetErrorInjectionEnabled(
    const std::string& name, ErrorInjectionCapabilityIntf::Type type,
    SensorManager& manager,
    const Interfaces<ErrorInjectionCapabilityIntf>& interfaces) :
    NsmInterfaceContainer<ErrorInjectionCapabilityIntf>(interfaces),
    NsmObject(name, "NSM_ErrorInjectionCapability"), type(type),
    manager(manager)
{
    if (type == ErrorInjectionCapabilityIntf::Type::Unknown)
    {
        throw std::invalid_argument(
            "NsmSetErrorInjectionEnabled::ctor: PDI type cannot be Unknown");
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
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_error_injection_types_mask_req));

    auto eid = manager.getEid(device);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    nsm_error_injection_types_mask data = {0, 0, 0, 0, 0, 0, 0, 0};
    invoke([&data, this, value](const auto& pdi) {
        // Update the error injection mask based on the PDI type.
        // If the PDI type matches the current type, use the provided value.
        // Otherwise, retain the current enabled state to ensure only the mask
        // for the current type is updated, preserving the state of other types.
        auto type = int(pdi.type());
        auto setValue = int(pdi.type() == this->type ? value : pdi.enabled());
        data.mask[type / 8] |= setValue << (type % 8);
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
    rc = co_await manager.postPatchNsmCommand(eid, request, responseMsg,
                                              responseLen);
    if (rc)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error(
                "NsmSetErrorInjectionEnabled::setEnabled: postPatchNsmCommand failed."
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
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
            "NsmSetErrorInjectionEnabled::setEnabled: decode_set_current_error_injection_types_v1_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmSetErrorInjectionPayload::NsmSetErrorInjectionPayload(
    const std::string& name, SensorManager& manager,
    const Interfaces<ErrorInjectionPayloadIntf>& interfaces,
    std::shared_ptr<NsmActivateErrorInjectionPayloadIntf> activateIntf) :
    NsmInterfaceContainer<ErrorInjectionPayloadIntf>(interfaces),
    NsmObject(name, "NSM_ErrorInjectionPayload"), manager(manager),
    activateIntf(activateIntf)
{}

requester::Coroutine NsmSetErrorInjectionPayload::setErrorInjectionPayload(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    uint32_t faultBitMap;
    uint32_t errorInjectionId;
    const std::vector<std::tuple<std::string, uint32_t>>* payloadValue =
        std::get_if<std::vector<std::tuple<std::string, uint32_t>>>(&value);

    if (payloadValue == nullptr)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    for (auto val : *payloadValue)
    {
        if (std::get<0>(val) == "FaultBitMap")
        {
            faultBitMap = std::get<1>(val);
        }
        else if (std::get<0>(val) == "errorInjectionId")
        {
            errorInjectionId = std::get<1>(val);
        }
    }
    // coverity[missing_return]
    co_return co_await setPayload(faultBitMap, errorInjectionId, *status,
                                  device);
}

requester::Coroutine NsmSetErrorInjectionPayload::setPayload(
    uint32_t faultBitMap, uint32_t errorInjectionId,
    AsyncOperationStatusType& status, std::shared_ptr<NsmDevice> device)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_error_injection_payload_req));

    auto eid = manager.getEid(device);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    struct nsm_error_injection_payload data = {0, 0, 0};
    data.offset = 0;
    data.error_injection_id = errorInjectionId;
    data.fault_reason_bit_map = faultBitMap;
    auto rc = encode_set_error_injection_payload_req(0, &data, requestPtr);

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
    rc = co_await manager.postPatchNsmCommand(eid, request, responseMsg,
                                              responseLen);
    if (rc)
    {
        lg2::error(
            "NsmSetErrorInjectionPayload::setPayload: postPatchNsmCommand failed."
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
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
