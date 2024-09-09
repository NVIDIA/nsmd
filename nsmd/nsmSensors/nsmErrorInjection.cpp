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

#include "nsmErrorInjection.hpp"

#include "device-configuration.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmErrorInjection::NsmErrorInjection(
    const NsmInterfaceProvider<ErrorInjectionIntf>& provider) :
    NsmSensor(provider),
    NsmInterfaceContainer<ErrorInjectionIntf>(provider)
{}

std::optional<Request> NsmErrorInjection::genRequestMsg(eid_t eid,
                                                        uint8_t instanceNumber)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_error_injection_mode_v1_req(instanceNumber,
                                                     requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_error_injection_mode_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmErrorInjection::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_error_injection_mode_v1 data;

    auto rc = decode_get_error_injection_mode_v1_resp(responseMsg, responseLen,
                                                      &cc, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        pdi().errorInjectionModeEnabled(data.mode);
        pdi().persistentDataModified(data.flags.bits.bit0);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_error_injection_mode_v1_resp sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }

    return cc ? cc : rc;
}

NsmErrorInjectionSupported::NsmErrorInjectionSupported(
    const NsmInterfaceProvider<ErrorInjectionCapabilityIntf>& provider) :
    NsmSensor(provider),
    NsmInterfaceContainer<ErrorInjectionCapabilityIntf>(provider)
{
    for (auto& [_, pdi] : interfaces)
    {
        if (pdi->type() == ErrorInjectionCapabilityIntf::Type::Unknown)
        {
            throw std::invalid_argument(
                "NsmErrorInjectionSupported::ctor: PDI type cannot be Unknown");
        }
    }
}

std::optional<Request>
    NsmErrorInjectionSupported::genRequestMsg(eid_t eid, uint8_t instanceNumber)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_supported_error_injection_types_v1_req(instanceNumber,
                                                                requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_supported_error_injection_types_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmErrorInjectionSupported::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_error_injection_types_mask data;

    auto rc = decode_get_error_injection_types_v1_resp(responseMsg, responseLen,
                                                       &cc, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        for (auto& [_, pdi] : interfaces)
        {
            auto type = pdi->type();
            pdi->supported(data.mask[(int)type / 8] & (1 << ((int)type % 8)));
        }
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_error_injection_types_v1_resp sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }

    return cc ? cc : rc;
}

std::optional<Request>
    NsmErrorInjectionEnabled::genRequestMsg(eid_t eid, uint8_t instanceNumber)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_current_error_injection_types_v1_req(instanceNumber,
                                                              requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_current_error_injection_types_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmErrorInjectionEnabled::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_error_injection_types_mask data;

    auto rc = decode_get_error_injection_types_v1_resp(responseMsg, responseLen,
                                                       &cc, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        for (auto& [_, pdi] : interfaces)
        {
            auto type = pdi->type();
            pdi->enabled(data.mask[(int)type / 8] & (1 << ((int)type % 8)));
        }
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_error_injection_types_v1_resp sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }

    return cc ? cc : rc;
}

} // namespace nsm
