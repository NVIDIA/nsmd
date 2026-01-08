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

#include "nsmApSkuId.hpp"

#include "nsmCommon.hpp"
#include "nsmUpdateApSkuId.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <endian.h>

#include <iomanip>
#include <sstream>

namespace nsm
{

ApSkuIdConfiguration::ApSkuIdConfiguration(
    sdbusplus::bus::bus& bus, const std::string& objPath, const uuid_t& uuidIn,
    std::shared_ptr<ProgressIntf> progressIntfIn, NsmSensor& nsmSensor) :
    ApSkuIdIntf(bus, objPath.c_str()), uuid(uuidIn),
    progressIntf(progressIntfIn), nsmSensor(nsmSensor)
{}

NsmApSkuIdObject::NsmApSkuIdObject(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const uuid_t& uuidIn, std::shared_ptr<ProgressIntf> progressIntfIn,
    uint16_t classificationIn, uint16_t identifierIn, uint8_t indexIn,
    const std::vector<utils::Association>& associations) :
    NsmSensor(name, type), objectPath(getPath(name)),
    classification(classificationIn), identifier(identifierIn), index(indexIn)
{
    lg2::info("NsmApSkuIdObject: create object: {PATH}", "PATH",
              objectPath.c_str());
    apSkuIdObject = std::make_unique<ApSkuIdConfiguration>(
        bus, objectPath, uuidIn, progressIntfIn, *this);

    if (!associations.empty())
    {
        std::vector<utils::Association> filteredAssociations;
        for (const auto& assoc : associations)
        {
            if (assoc.forward == "inventory_SKU")
            {
                filteredAssociations.push_back(assoc);
            }
        }

        if (!filteredAssociations.empty())
        {
            associationDefinitionsIntf =
                std::make_unique<AssociationDefinitionsIntf>(
                    bus, objectPath.c_str());
            associationDefinitionsIntf->associations(
                utils::getAssociations(filteredAssociations));
        }
    }
}

std::optional<std::vector<uint8_t>>
    NsmApSkuIdObject::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_get_erot_state_info_req));
    struct ::nsm_firmware_erot_state_info_req erot_req = {};
    erot_req.component_classification = classification;
    erot_req.component_classification_index = index;
    erot_req.component_identifier = identifier;
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &erot_req, requestMsg);
    if (rc)
    {
        std::string msg = utils::requestMsgToHexString(request);
        lg2::error(
            "encode_nsm_query_get_erot_state_parameters_req failed: eid={EID}, rc={RC}, NSM_Request={MSG}",
            "EID", eid, "RC", rc, "MSG", msg);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmApSkuIdObject::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    nsm_firmware_erot_state_info_resp erot_info = {};

    auto rc = decode_nsm_query_get_erot_state_parameters_resp(
        responseMsg, responseLen, &cc, &reason_code, &erot_info);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_nsm_query_get_erot_state_parameters_resp failed: rc={RC}, cc={CC}, reasonCode={REASONCODE}",
            "RC", rc, "CC", (int)cc, "REASONCODE", (int)reason_code);
        free(erot_info.slot_info);
        return cc ? cc : rc;
    }

    uint32_t apSkuIdValue = erot_info.fq_resp_hdr.ap_sku_id;

    std::string apSkuIdStr = formatApSkuId(apSkuIdValue);
    apSkuIdObject->sku(apSkuIdStr);

    lg2::debug("AP SKU ID received: {SKUID}", "SKUID", apSkuIdValue);

    free(erot_info.slot_info);
    return cc ? cc : rc;
}

} // namespace nsm
