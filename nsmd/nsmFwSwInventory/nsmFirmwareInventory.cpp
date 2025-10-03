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

#include "nsmFirmwareInventory.hpp"

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSetWriteProtected.hpp"
#include "nsmWriteProtectedControl.hpp"

#include <unordered_map>

namespace nsm
{

requester::Coroutine
    nsmFirmwareInventoryCreateSensors(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_WriteProtect";

    dbus::PropertyMap allBaseIfaceProperties;
    auto rc = co_await utils::coGetCachedBaseProperties(objPath, baseInterface,
                                                        allBaseIfaceProperties);
    if (rc != NSM_SUCCESS)
    {
        co_return rc;
    }
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string name{};
    if (allBaseIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allBaseIfaceProperties.at("Name"));
    }
    std::string type{};
    if (allCurrentIfaceProperties.count("Type"))
    {
        type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
    }
    uuid_t uuid{};
    if (allBaseIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
    }

    auto device = manager.getNsmDeviceFromStaticUUID(uuid);

    if (type == "NSM_WriteProtect")
    {
        std::vector<utils::Association> associations{};
        co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                          associations);
        if (!associations.empty())
        {
            auto associationsObject = std::make_shared<
                NsmFirmwareInventory<AssociationDefinitionsIntf>>(name);
            associationsObject->invoke(pdiMethod(associations),
                                       utils::getAssociations(associations));
            device->addStaticSensor(associationsObject);
        }

        diagnostics_enable_disable_wp_data_index dataIndex{};
        if (allBaseIfaceProperties.count("DataIndex"))
        {
            dataIndex =
                (diagnostics_enable_disable_wp_data_index)std::get<uint64_t>(
                    allBaseIfaceProperties.at("DataIndex"));
        }

        switch (dataIndex)
        {
            case RETIMER_EEPROM:
            case BASEBOARD_FRU_EEPROM:
            case PEX_SW_EEPROM:
            case NVSW_EEPROM_BOTH:
            case NVSW_EEPROM_1:
            case NVSW_EEPROM_2:
            case GPU_1_4_SPI_FLASH:
            case GPU_5_8_SPI_FLASH:
            case GPU_SPI_FLASH:
            case CX8_SPI_FLASH:
            case GPU_SPI_FLASH_1:
            case GPU_SPI_FLASH_2:
            case GPU_SPI_FLASH_3:
            case GPU_SPI_FLASH_4:
            case GPU_SPI_FLASH_5:
            case GPU_SPI_FLASH_6:
            case GPU_SPI_FLASH_7:
            case GPU_SPI_FLASH_8:
            case HMC_SPI_FLASH:
            case RETIMER_EEPROM_1:
            case RETIMER_EEPROM_2:
            case RETIMER_EEPROM_3:
            case RETIMER_EEPROM_4:
            case RETIMER_EEPROM_5:
            case RETIMER_EEPROM_6:
            case RETIMER_EEPROM_7:
            case RETIMER_EEPROM_8:
            case CPU_SPI_FLASH_1:
            case CPU_SPI_FLASH_2:
            case CPU_SPI_FLASH_3:
            case CPU_SPI_FLASH_4:
            case CPU_SPI_FLASH_5:
            case CPU_SPI_FLASH_6:
            case CPU_SPI_FLASH_7:
            case CPU_SPI_FLASH_8:
            case CX7_FRU_EEPROM:
            case HMC_FRU_EEPROM:
                break;
            default:
                throw std::out_of_range("Invalid data index");
                break;
        }

        auto pdiObjPath = (firmwareInventoryBasePath / name).string();
        auto settingsIntf = std::make_shared<NsmSetWriteProtected>(
            name, manager, dataIndex, pdiObjPath);
        auto writeProtectControl = std::make_shared<NsmWriteProtectedControl>(
            *settingsIntf, dataIndex);
        device->addDeviceSensors(settingsIntf);
        device->addSensor(writeProtectControl, false);

        auto& asyncDispatcher =
            *AsyncOperationManager::getInstance()->getDispatcher(pdiObjPath);
        asyncDispatcher.addAsyncSetOperation(
            "xyz.openbmc_project.Software.Settings", "WriteProtected",
            AsyncSetOperationInfo{
                std::bind_front(&NsmSetWriteProtected::writeProtected,
                                settingsIntf.get()),
                writeProtectControl, device});
    }
    else if (type == "NSM_Asset")
    {
        std::string manufacturer{};
        if (allCurrentIfaceProperties.count("Manufacturer"))
        {
            manufacturer = std::get<std::string>(
                allCurrentIfaceProperties.at("Manufacturer"));
        }

        auto asset = std::make_shared<NsmFirmwareInventory<NsmAssetIntf>>(name);
        asset->invoke(pdiMethod(manufacturer), manufacturer);
        device->addStaticSensor(asset);
    }
    else if (type == "NSM_FirmwareVersion")
    {
        uint64_t instanceNumber{};
        if (allCurrentIfaceProperties.count("InstanceNumber"))
        {
            instanceNumber = std::get<uint64_t>(
                allCurrentIfaceProperties.at("InstanceNumber"));
        }

        auto firmwareInventoryVersion = NsmFirmwareInventory<VersionIntf>(name);
        firmwareInventoryVersion.invoke(pdiMethod(purpose),
                                        VersionIntf::VersionPurpose::Other);
        auto version = std::make_shared<NsmInventoryProperty<VersionIntf>>(
            firmwareInventoryVersion,
            nsm_inventory_property_identifiers(PCIERETIMER_0_EEPROM_VERSION +
                                               instanceNumber));
        device->addStaticSensor(version);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

dbus::Interfaces firmwareInventoryInterfaces = {
    "xyz.openbmc_project.Configuration.NSM_WriteProtect",
    "xyz.openbmc_project.Configuration.NSM_WriteProtect.Asset",
    "xyz.openbmc_project.Configuration.NSM_WriteProtect.Version",
};

REGISTER_NSM_CREATION_FUNCTION(nsmFirmwareInventoryCreateSensors,
                               firmwareInventoryInterfaces)

} // namespace nsm
