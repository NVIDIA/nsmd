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

#include "nsmSoftwareSettings.hpp"

#include "device-configuration.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmSoftwareSettings::NsmSoftwareSettings(
    const NsmInterfaceProvider<SettingsIntf>& provider,
    NsmDeviceIdentification deviceType, uint8_t instanceId) :
    NsmSensor(provider), NsmInterfaceContainer(provider),
    deviceType(deviceType), instanceId(instanceId)
{}

std::optional<Request> NsmSoftwareSettings::genRequestMsg(eid_t eid,
                                                          uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_WP_SETTINGS, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_fpga_diagnostics_settings_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmSoftwareSettings::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_fpga_diagnostics_settings_wp data;

    auto rc = decode_get_fpga_diagnostics_settings_wp_resp(
        responseMsg, responseLen, &cc, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        switch (deviceType)
        {
            case NSM_DEV_ID_GPU:
                pdi().writeProtected(instanceId < 4 ? data.gpu1_4
                                                    : data.gpu5_8);
                break;
            case NSM_DEV_ID_SWITCH:
                pdi().writeProtected(data.nvSwitch);
                break;
            case NSM_DEV_ID_PCIE_BRIDGE:
                pdi().writeProtected(data.pex);
                break;
            case NSM_DEV_ID_BASEBOARD:
                pdi().writeProtected(data.baseboard);
                break;
            default:
                lg2::error(
                    "handleResponseMsg: decode_get_fpga_diagnostics_settings_wp_resp sensor={NAME}, deviceType={DEVICETYPE}",
                    "NAME", getName(), "DEVICETYPE", (uint8_t)deviceType);
                break;
        }
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_fpga_diagnostics_settings_wp_resp sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}
} // namespace nsm
