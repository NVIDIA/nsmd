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

#include "nsmModeIntf.hpp"

#include "device-configuration.h"

#include "sensorManager.hpp"

#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace nsm
{

NsmModeIntf::NsmModeIntf(SensorManager& manager,
                         std::shared_ptr<NsmDevice> device) :
    ModeIntf(utils::DBusHandler::getBus(),
             "/xyz/openbmc_project/mode/settings"),
    manager(manager), device(device)
{}

ModeIntf::StateOfISTMode NsmModeIntf::istMode(ModeIntf::StateOfISTMode value)
{
    return ModeIntf::istMode(setIstMode(value));
}
ModeIntf::StateOfISTMode NsmModeIntf::setIstMode(ModeIntf::StateOfISTMode value)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_enable_disable_gpu_ist_mode_req));

    auto eid = manager.getEid(device);
    // ModeIntf::StateOfISTMode::Enabled -> sets all 8 GPUs
    uint8_t data = uint8_t(value == ModeIntf::StateOfISTMode::Enabled);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    encode_enable_disable_gpu_ist_mode_req(0, ALL_GPUS_DEVICE_INDEX, data,
                                           requestPtr);

    lg2::debug(
        "NsmModeIntf::setIstMode encode_enable_disable_gpu_ist_mode_req call - value={VALUE}",
        "VALUE", data);

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = manager.SendRecvNsmMsgSync(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error("NsmModeIntf::setIstMode: SendRecvNsmMsgSync failed."
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);
        }
        throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
            WriteFailure();
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_enable_disable_gpu_ist_mode_resp(responseMsg.get(), responseLen,
                                                 &cc, &reasonCode);
    if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmModeIntf::setIstMode: decode_enable_disable_gpu_ist_mode_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
            WriteFailure();
    }
    return getIstMode();
}
ModeIntf::StateOfISTMode NsmModeIntf::getIstMode() const
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));

    auto eid = manager.getEid(device);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    encode_get_fpga_diagnostics_settings_req(0, GET_GPU_IST_MODE_SETTINGS,
                                             requestPtr);

    lg2::debug(
        "NsmModeIntf::getIstMode: encode_get_fpga_diagnostics_settings_req call");

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = manager.SendRecvNsmMsgSync(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error("NsmModeIntf::getIstMode: SendRecvNsmMsgSync failed."
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);
        }
        throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
            WriteFailure();
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint8_t data;

    rc = decode_get_gpu_ist_mode_resp(responseMsg.get(), responseLen, &cc,
                                      &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmModeIntf::getIstMode: decode_get_gpu_ist_mode_resp success");
        return data != 0x00 ? ModeIntf::StateOfISTMode::Enabled
                            : ModeIntf::StateOfISTMode::Disabled;
    }
    else
    {
        lg2::error(
            "NsmModeIntf::getIstMode:  decode_get_gpu_ist_mode_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
            WriteFailure();
    }
}

} // namespace nsm
