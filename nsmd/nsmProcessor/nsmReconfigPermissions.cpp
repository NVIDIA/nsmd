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

#include "nsmReconfigPermissions.hpp"

#include "device-configuration.h"

#include <phosphor-logging/lg2.hpp>

#include <stdexcept>

namespace nsm
{

NsmReconfigPermissions::NsmReconfigPermissions(
    const NsmInterfaceProvider<ReconfigSettingsIntf>& provider,
    ReconfigSettingsIntf::FeatureType feature) :
    NsmSensor(provider), NsmInterfaceContainer(provider), feature(feature)
{
    if (feature >= ReconfigSettingsIntf::FeatureType::Unknown)
    {
        throw std::invalid_argument(
            "Invalid feature '" + std::to_string(uint64_t(feature)) +
            "'. Feature can be only lower than FeatureType::Unknown");
    }
}

std::optional<Request>
    NsmReconfigPermissions::genRequestMsg(eid_t eid, uint8_t instanceNumber)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceNumber, reconfiguration_permissions_v1_index(feature),
        requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_reconfiguration_permissions_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmReconfigPermissions::handleResponseMsg(const struct nsm_msg* responseMsg,
                                              size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_reconfiguration_permissions_v1 data;

    auto rc = decode_get_reconfiguration_permissions_v1_resp(
        responseMsg, responseLen, &cc, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        pdi().ReconfigSettingsIntf::allowOneShotConfig(data.oneshot);
        pdi().ReconfigSettingsIntf::allowPersistentConfig(data.persistent);
        pdi().ReconfigSettingsIntf::allowFLRPersistentConfig(
            data.flr_persistent);
        pdi().type(feature);
    }
    else
    {
        pdi().type(ReconfigSettingsIntf::FeatureType::Unknown);
        lg2::error(
            "handleResponseMsg: decode_get_reconfiguration_permissions_v1_resp sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }

    return cc ? cc : rc;
}
} // namespace nsm
