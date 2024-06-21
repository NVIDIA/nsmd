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

#include "nsmWriteProtectedIntf.hpp"

#include "diagnostics.h"

#include "sensorManager.hpp"

#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace nsm
{

NsmWriteProtectedIntf::NsmWriteProtectedIntf(SensorManager& manager,
                                             std::shared_ptr<NsmDevice> device,
                                             uint8_t instanceNumber,
                                             NsmDeviceIdentification deviceType,
                                             const char* path, bool retimer) :
    SettingsIntf(utils::DBusHandler::getBus(), path), manager(manager),
    device(device), instanceNumber(instanceNumber), deviceType(deviceType),
    retimer(retimer)
{
    utils::verifyDeviceAndInstanceNumber(deviceType, instanceNumber, retimer);
}

bool NsmWriteProtectedIntf::getValue(
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
                writeProtected = data.baseboard;
            }
            break;
        default:
            break;
    }
    return writeProtected;
}
int NsmWriteProtectedIntf::getDataIndex(NsmDeviceIdentification deviceType,
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
                dataIndex = BASEBOARD_FRU_EEPROM;
            }
            break;
        default:
            break;
    }
    return dataIndex;
}
bool NsmWriteProtectedIntf::writeProtected(bool value)
{
    return SettingsIntf::writeProtected(setWriteProtected(value));
}
bool NsmWriteProtectedIntf::setWriteProtected(bool value)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));

    auto eid = manager.getEid(device);
    auto dataIndex = getDataIndex(deviceType, instanceNumber, retimer);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(
        0, (diagnostics_enable_disable_wp_data_index)dataIndex, value,
        requestPtr);

    lg2::debug(
        "NsmWriteProtectedIntf::setWriteProtected encode_enable_disable_wp_req call - value={VALUE}, dataIndex={DI}",
        "VALUE", value, "DI", dataIndex);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_enable_disable_wp_req({DI}) failed. eid={EID} rc={RC}",
            "DI", dataIndex, "EID", eid, "RC", rc);
        throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
            WriteFailure();
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = manager.SendRecvNsmMsgSync(eid, request, responseMsg, responseLen);
    if (rc)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error(
                "NsmWriteProtectedIntf::setWriteProtected: SendRecvNsmMsgSync failed."
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
        }
        throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
            WriteFailure();
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_enable_disable_wp_resp(responseMsg.get(), responseLen, &cc,
                                       &reasonCode);
    if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmWriteProtectedIntf::setWriteProtected: decode_enable_disable_wp_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
            WriteFailure();
    }
    return getWriteProtected();
}
bool NsmWriteProtectedIntf::getWriteProtected() const
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));

    auto eid = manager.getEid(device);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(0, GET_WP_SETTINGS,
                                                       requestPtr);

    lg2::debug(
        "NsmWriteProtectedIntf::getWriteProtected: encode_get_fpga_diagnostics_settings_req call");
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmWriteProtectedIntf::getWriteProtected: encode_get_fpga_diagnostics_settings_req(GET_WP_SETTINGS) failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
            WriteFailure();
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = manager.SendRecvNsmMsgSync(eid, request, responseMsg, responseLen);
    if (rc)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error(
                "NsmWriteProtectedIntf::getWriteProtected: SendRecvNsmMsgSync failed."
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
        }
        throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
            WriteFailure();
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_fpga_diagnostics_settings_wp data;

    rc = decode_get_fpga_diagnostics_settings_wp_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmWriteProtectedIntf::getWriteProtected: decode_get_fpga_diagnostics_settings_wp_resp success");
        return getValue(data, deviceType, instanceNumber, retimer);
    }
    else
    {
        lg2::error(
            "NsmWriteProtectedIntf::getWriteProtected:  decode_get_fpga_diagnostics_settings_wp_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
            WriteFailure();
    }
}

} // namespace nsm
