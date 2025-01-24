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

#include "nsmSetECCMode.hpp"

#include "platform-environmental.h"

namespace nsm
{

NsmSetEccMode::NsmSetEccMode(bool isLongRunning,
                             std::shared_ptr<NsmDevice> device) :
    NsmAsyncLongRunningSensor("NsmSetEccMode", "NSM_ECC", isLongRunning, device,
                              NSM_TYPE_PLATFORM_ENVIRONMENTAL, NSM_SET_ECC_MODE)
{}

std::optional<Request> NsmSetEccMode::genRequestMsg(eid_t eid,
                                                    uint8_t /*instanceId*/)
{
    auto eccMode = getValue<bool>();
    uint8_t requestedECCMode = static_cast<uint8_t>(eccMode);
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_ECC_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // first argument instanceid=0 is irrelevant
    auto rc = encode_set_ECC_mode_req(0, requestedECCMode, requestMsg);

    if (rc)
    {
        lg2::error(
            "NsmSetEccMode::genRequestMsg encode_set_ECC_mode_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
    }
    return request;
}

uint8_t NsmSetEccMode::handleResponseMsg(const nsm_msg* responseMsg,
                                         size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    auto rc = isLongRunning
                  ? decode_set_ECC_mode_event_resp(responseMsg, responseLen,
                                                   &cc, &reasonCode)
                  : decode_set_ECC_mode_resp(responseMsg, responseLen, &cc,
                                             &dataSize, &reasonCode);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info("NsmSetEccMode::handleResponseMsg completed");
    }
    else
    {
        lg2::error(
            "NsmSetEccMode::handleResponseMsg decode_set_ECC_mode_resp failed. cc={CC}, reasonCode={REASON}, rc={RC}",
            "CC", cc, "REASON", reasonCode, "RC", rc);
    }
    return cc ? cc : rc;
}
} // namespace nsm
