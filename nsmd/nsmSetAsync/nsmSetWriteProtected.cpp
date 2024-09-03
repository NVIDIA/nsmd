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

#include "diagnostics.h"

#include "sensorManager.hpp"

#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace nsm
{

NsmSetWriteProtected::NsmSetWriteProtected(const std::string& name,
                                           SensorManager& manager,
                                           uint8_t instanceNumber,
                                           NsmDeviceIdentification deviceType,
                                           std::string objPath, bool retimer) :
    NsmInterfaceProvider<SettingsIntf>(name, "NSM_WriteProtected",
                                       dbus::Interfaces{objPath}),
    manager(manager), instanceNumber(instanceNumber), deviceType(deviceType),
    retimer(retimer)
{
    utils::verifyDeviceAndInstanceNumber(deviceType, instanceNumber, retimer);
}

bool NsmSetWriteProtected::getValue(
    const nsm_fpga_diagnostics_settings_wp& data,
    NsmDeviceIdentification deviceType, uint8_t instanceNumber, bool retimer)
{
    auto writeProtected = false;
    switch (deviceType)
    {
        case NSM_DEV_ID_GPU:
            switch (instanceNumber)
            {
                case 0:
                    writeProtected = data.gpu1;
                    break;
                case 1:
                    writeProtected = data.gpu2;
                    break;
                case 2:
                    writeProtected = data.gpu3;
                    break;
                case 3:
                    writeProtected = data.gpu4;
                    break;
                case 4:
                    writeProtected = data.gpu5;
                    break;
                case 5:
                    writeProtected = data.gpu6;
                    break;
                case 6:
                    writeProtected = data.gpu7;
                    break;
                case 7:
                    writeProtected = data.gpu8;
                    break;
                default:
                    break;
            }
            break;
        case NSM_DEV_ID_SWITCH:
            switch (instanceNumber)
            {
                case 0:
                    writeProtected = data.nvSwitch1;
                    break;
                case 1:
                    writeProtected = data.nvSwitch2;
                    break;
                default:
                    break;
            }
            break;
        case NSM_DEV_ID_PCIE_BRIDGE:
            writeProtected = data.pex;
            break;
        case NSM_DEV_ID_BASEBOARD:
            if (retimer)
            {
                switch (instanceNumber)
                {
                    case 0:
                        writeProtected = data.retimer1;
                        break;
                    case 1:
                        writeProtected = data.retimer2;
                        break;
                    case 2:
                        writeProtected = data.retimer3;
                        break;
                    case 3:
                        writeProtected = data.retimer4;
                        break;
                    case 4:
                        writeProtected = data.retimer5;
                        break;
                    case 5:
                        writeProtected = data.retimer6;
                        break;
                    case 6:
                        writeProtected = data.retimer7;
                        break;
                    case 7:
                        writeProtected = data.retimer8;
                        break;
                    default:
                        break;
                }
            }
            else
            {
                writeProtected = data.hmc;
            }
            break;
        default:
            break;
    }
    return writeProtected;
}
int NsmSetWriteProtected::getDataIndex(NsmDeviceIdentification deviceType,
                                       uint8_t instanceNumber, bool retimer)
{
    auto dataIndex = 0;
    switch (deviceType)
    {
        case NSM_DEV_ID_GPU:
            switch (instanceNumber)
            {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                    dataIndex = GPU_SPI_FLASH_1 + instanceNumber;
                    break;
                default:
                    break;
            }
            break;
        case NSM_DEV_ID_SWITCH:
            switch (instanceNumber)
            {
                case 0:
                case 1:
                    dataIndex = NVSW_EEPROM_1 + instanceNumber;
                    break;
                default:
                    break;
            }
            break;
        case NSM_DEV_ID_PCIE_BRIDGE:
            dataIndex = PEX_SW_EEPROM;
            break;
        case NSM_DEV_ID_BASEBOARD:
            if (retimer)
            {
                switch (instanceNumber)
                {
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                        dataIndex = RETIMER_EEPROM_1 + instanceNumber;
                        break;
                    default:
                        break;
                }
            }
            else
            {
                dataIndex = HMC_SPI_FLASH;
            }
            break;
        default:
            break;
    }
    return dataIndex;
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
    auto dataIndex = getDataIndex(deviceType, instanceNumber, retimer);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(
        0, (diagnostics_enable_disable_wp_data_index)dataIndex, value,
        requestPtr);

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
