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

#include "nsmWriteProtectedControl.hpp"

#include "device-configuration.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmWriteProtectedControl::NsmWriteProtectedControl(
    const NsmInterfaceProvider<SettingsIntf>& provider,
    const diagnostics_enable_disable_wp_data_index dataIndex) :
    NsmGroupSensor(provider),
    NsmInterfaceContainer(provider), dataIndex(dataIndex)
{}

std::optional<Request> NsmWriteProtectedControl::genRequestMsg(eid_t eid,
                                                               uint8_t)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(0, GET_WP_SETTINGS,
                                                       requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_fpga_diagnostics_settings_req(GET_WP_SETTINGS) failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmWriteProtectedControl::handleResponse(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_fpga_diagnostics_settings_wp data;

    auto rc = decode_get_fpga_diagnostics_settings_wp_resp(
        responseMsg, responseLen, &cc, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        invoke(pdiMethod(writeProtected),
               NsmSetWriteProtected::getValue(data, dataIndex));
        clearErrorBitMap("decode_get_fpga_diagnostics_settings_wp_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_fpga_diagnostics_settings_wp_resp",
                             reasonCode, cc, rc);
    }

    return cc ? cc : rc;
}
} // namespace nsm
