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

#include "nsmSetWriteProtected.hpp"

#include "sensorManager.hpp"

#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace nsm
{

NsmSetWriteProtected::NsmSetWriteProtected(
    const std::string& name, SensorManager& manager,
    const diagnostics_enable_disable_wp_data_index dataIndex,
    const std::string objPath) :
    NsmInterfaceProvider<SettingsIntf>(name, "NSM_WriteProtected",
                                       dbus::Interfaces{objPath}),
    manager(manager), dataIndex(dataIndex)
{}

bool NsmSetWriteProtected::getValue(
    const nsm_fpga_diagnostics_settings_wp& data,
    const diagnostics_enable_disable_wp_data_index dataIndex)
{
    switch (dataIndex)
    {
        case RETIMER_EEPROM:
            return data.retimer;
        case BASEBOARD_FRU_EEPROM:
        case CX7_FRU_EEPROM:
        case HMC_FRU_EEPROM:
            return data.baseboard;
        case PEX_SW_EEPROM:
            return data.pex;
        case NVSW_EEPROM_BOTH:
            return data.nvSwitch;
        case NVSW_EEPROM_1:
            return data.nvSwitch1;
        case NVSW_EEPROM_2:
            return data.nvSwitch2;
        case GPU_1_4_SPI_FLASH:
            return data.gpu1_4;
        case GPU_5_8_SPI_FLASH:
            return data.gpu5_8;
        case GPU_SPI_FLASH_1:
            return data.gpu1;
        case GPU_SPI_FLASH_2:
            return data.gpu2;
        case GPU_SPI_FLASH_3:
            return data.gpu3;
        case GPU_SPI_FLASH_4:
            return data.gpu4;
        case GPU_SPI_FLASH_5:
            return data.gpu5;
        case GPU_SPI_FLASH_6:
            return data.gpu6;
        case GPU_SPI_FLASH_7:
            return data.gpu7;
        case GPU_SPI_FLASH_8:
            return data.gpu8;
        case HMC_SPI_FLASH:
            return data.hmc;
        case RETIMER_EEPROM_1:
            return data.retimer1;
        case RETIMER_EEPROM_2:
            return data.retimer2;
        case RETIMER_EEPROM_3:
            return data.retimer3;
        case RETIMER_EEPROM_4:
            return data.retimer4;
        case RETIMER_EEPROM_5:
            return data.retimer5;
        case RETIMER_EEPROM_6:
            return data.retimer6;
        case RETIMER_EEPROM_7:
            return data.retimer7;
        case RETIMER_EEPROM_8:
            return data.retimer8;
        case CPU_SPI_FLASH_1:
            return data.cpu1;
        case CPU_SPI_FLASH_2:
            return data.cpu2;
        case CPU_SPI_FLASH_3:
            return data.cpu3;
        case CPU_SPI_FLASH_4:
            return data.cpu4;
        default:
            return false;
    }
}

requester::Coroutine NsmSetWriteProtected::writeProtected(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    auto writeProtected = std::get_if<bool>(&value);

    if (!writeProtected)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    // coverity[missing_return]
    co_return co_await setWriteProtected(*writeProtected, *status, device);
}
requester::Coroutine
    NsmSetWriteProtected::setWriteProtected(bool value,
                                            AsyncOperationStatusType& status,
                                            std::shared_ptr<NsmDevice> device)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));

    auto eid = manager.getEid(device);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(0, dataIndex, value, requestPtr);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_enable_disable_wp_req({DI}) failed. eid={EID} rc={RC}",
            "DI", dataIndex, "EID", eid, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error(
                "NsmSetWriteProtected::setWriteProtected: SendRecvNsmMsgSync failed."
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
        }
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_enable_disable_wp_resp(responseMsg.get(), responseLen, &cc,
                                       &reasonCode);
    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmSetWriteProtected::setWriteProtected decode_enable_disable_wp_resp success - value={VALUE}, dataIndex={DI}",
            "VALUE", value, "DI", dataIndex);
    }
    else
    {
        lg2::error(
            "NsmSetWriteProtected::setWriteProtected: decode_enable_disable_wp_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

} // namespace nsm
