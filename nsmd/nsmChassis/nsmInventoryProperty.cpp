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

#include "nsmInventoryProperty.hpp"

#include "nsmInterface.hpp"

namespace nsm
{

NsmInventoryPropertyBase::NsmInventoryPropertyBase(
    const NsmObject& pdi, nsm_inventory_property_identifiers property) :
    NsmSensor(pdi), property(property)
{}

std::optional<Request>
    NsmInventoryPropertyBase::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        instanceId, uint8_t(property), requestPtr);
    if (rc)
    {
        lg2::error(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmInventoryPropertyBase::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    Response data(65535, 0);

    auto rc = decode_get_inventory_information_resp(
        responseMsg, responseLen, &cc, &reasonCode, &dataSize, data.data());
    if (rc)
    {
        lg2::error(
            "responseHandler: decode_get_inventory_information_resp failed. property={NUM} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NUM", int(property), "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        return rc;
    }

    if (cc == NSM_SUCCESS)
    {
        data.resize(dataSize);
        handleResponse(data);
    }
    else
    {
        lg2::error(
            "responseHandler: decode_get_inventory_information_resp is not success CC. property={NUM} rc={RC}",
            "NUM", int(property), "RC", rc);
        return rc;
    }

    return cc;
}

} // namespace nsm
