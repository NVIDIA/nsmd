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

#include "nsmClockOutputEnableState.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmClockOutputEnableStateBase::NsmClockOutputEnableStateBase(
    const NsmObject& provider, clock_output_enable_state_index bufferIndex,
    NsmDeviceIdentification deviceType, uint8_t instanceNumber, bool retimer) :
    NsmSensor(provider),
    bufferIndex(bufferIndex), deviceType(deviceType),
    instanceNumber(instanceNumber), retimer(retimer)
{}

std::optional<Request>
    NsmClockOutputEnableStateBase::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_clock_output_enabled_state_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(instanceId, bufferIndex,
                                                       requestPtr);
    if (rc)
    {
        lg2::debug(
            "encode_get_clock_output_enable_state_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmClockOutputEnableStateBase::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t size = 0;
    uint32_t data = 0;

    auto rc = decode_get_clock_output_enable_state_resp(
        responseMsg, responseLen, &cc, &reasonCode, &size, &data);

    if (rc == NSM_SUCCESS && cc == NSM_SUCCESS)
    {
        clearErrorBitMap("decode_get_clock_output_enable_state_resp");
    }
    else
    {
        memset(&data, 0, sizeof(data));
        logHandleResponseMsg("decode_get_clock_output_enable_state_resp",
                             reasonCode, cc, rc);
    }
    handleResponse(data);

    return cc ? cc : rc;
}

bool NsmClockOutputEnableStateBase::getPCIeClockBufferData(
    const nsm_pcie_clock_buffer_data& data) const
{
    auto result = false;
    switch (deviceType)
    {
        case NSM_DEV_ID_GPU:
            switch (instanceNumber)
            {
                case 0:
                    result = data.clk_buf_gpu1;
                    break;
                case 1:
                    result = data.clk_buf_gpu2;
                    break;
                case 2:
                    result = data.clk_buf_gpu3;
                    break;
                case 3:
                    result = data.clk_buf_gpu4;
                    break;
                case 4:
                    result = data.clk_buf_gpu5;
                    break;
                case 5:
                    result = data.clk_buf_gpu6;
                    break;
                case 6:
                    result = data.clk_buf_gpu7;
                    break;
                case 7:
                    result = data.clk_buf_gpu8;
                    break;
                default:
                    break;
            }
            break;
        case NSM_DEV_ID_SWITCH:
            switch (instanceNumber)
            {
                case 0:
                    result = data.clk_buf_nvSwitch_1;
                    break;
                case 1:
                    result = data.clk_buf_nvSwitch_2;
                    break;
                default:
                    break;
            }
            break;
        case NSM_DEV_ID_PCIE_BRIDGE:
            result = data.clk_buf_nvlinkMgmt_nic;
            break;
        case NSM_DEV_ID_BASEBOARD:
            if (retimer)
            {
                switch (instanceNumber)
                {
                    case 0:
                        result = data.clk_buf_retimer1;
                        break;
                    case 1:
                        result = data.clk_buf_retimer2;
                        break;
                    case 2:
                        result = data.clk_buf_retimer3;
                        break;
                    case 3:
                        result = data.clk_buf_retimer4;
                        break;
                    case 4:
                        result = data.clk_buf_retimer5;
                        break;
                    case 5:
                        result = data.clk_buf_retimer6;
                        break;
                    case 6:
                        result = data.clk_buf_retimer7;
                        break;
                    case 7:
                        result = data.clk_buf_retimer8;
                        break;
                    default:
                        break;
                }
            }
            break;
        default:
            break;
    }
    return result;
}

bool NsmClockOutputEnableStateBase::getNVHSClockBufferData(
    const nsm_nvhs_clock_buffer_data& data) const
{
    auto result = false;
    switch (deviceType)
    {
        case NSM_DEV_ID_GPU:
            switch (instanceNumber)
            {
                case 0:
                    result = data.clk_buf_sxm_nvhs1;
                    break;
                case 1:
                    result = data.clk_buf_sxm_nvhs2;
                    break;
                case 2:
                    result = data.clk_buf_sxm_nvhs3;
                    break;
                case 3:
                    result = data.clk_buf_sxm_nvhs4;
                    break;
                case 4:
                    result = data.clk_buf_sxm_nvhs5;
                    break;
                case 5:
                    result = data.clk_buf_sxm_nvhs6;
                    break;
                case 6:
                    result = data.clk_buf_sxm_nvhs7;
                    break;
                case 7:
                    result = data.clk_buf_sxm_nvhs8;
                    break;
                default:
                    break;
            }
            break;
        case NSM_DEV_ID_SWITCH:
            switch (instanceNumber)
            {
                case 0:
                    result = data.clk_buf_nvSwitch_nvhs1;
                    break;
                case 1:
                    result = data.clk_buf_nvSwitch_nvhs2;
                    break;
                case 2:
                    result = data.clk_buf_nvSwitch_nvhs3;
                    break;
                case 3:
                    result = data.clk_buf_nvSwitch_nvhs4;
                    break;
                default:
                    break;
            }
            break;
        case NSM_DEV_ID_PCIE_BRIDGE:
        case NSM_DEV_ID_BASEBOARD:
        default:
            break;
    }
    return result;
}

} // namespace nsm
