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

#include "dBusAsyncUtils.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSetWriteProtected.hpp"
#include "nsmWriteProtectedControl.hpp"
#include "utils.hpp"

namespace nsm
{

requester::Coroutine
    nsmFirmwareInventoryCreateSensors(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_FirmwareInventory";

    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto deviceType =
        (NsmDeviceIdentification) co_await utils::coGetDbusProperty<uint64_t>(
            objPath.c_str(), "DeviceType", baseInterface.c_str());
    auto instanceNumber = co_await utils::coGetDbusProperty<uint64_t>(
        objPath.c_str(), "InstanceNumber", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == "NSM_FirmwareInventory")
    {
        std::vector<utils::Association> associations{};
        co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                          associations);
        if (!associations.empty())
        {
            auto associationsObject = std::make_shared<
                NsmFirmwareInventory<AssociationDefinitionsIntf>>(name);
            associationsObject->pdi().associations(
                utils::getAssociations(associations));
            device->addStaticSensor(associationsObject);
        }

        auto pdiObjPath = (firmwareInventoryBasePath / name).string();
        auto retimer = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "IsRetimer", interface.c_str());
        auto settingsIntf = std::make_shared<NsmSetWriteProtected>(
            name, manager, instanceNumber, deviceType, pdiObjPath, retimer);
        auto writeProtectControl = std::make_shared<NsmWriteProtectedControl>(
            *settingsIntf, deviceType, instanceNumber, retimer);
        device->deviceSensors.emplace_back(settingsIntf);
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
        auto manufacturer = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());
        auto asset = std::make_shared<NsmFirmwareInventory<NsmAssetIntf>>(name);
        asset->pdi().manufacturer(manufacturer);
        device->addStaticSensor(asset);
    }
    else if (type == "NSM_FirmwareVersion")
    {
        auto firmwareInventoryVersion = NsmFirmwareInventory<VersionIntf>(name);
        firmwareInventoryVersion.pdi().purpose(
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
    "xyz.openbmc_project.Configuration.NSM_FirmwareInventory",
    "xyz.openbmc_project.Configuration.NSM_FirmwareInventory.Asset",
    "xyz.openbmc_project.Configuration.NSM_FirmwareInventory.Version",
};

REGISTER_NSM_CREATION_FUNCTION(nsmFirmwareInventoryCreateSensors,
                               firmwareInventoryInterfaces)

} // namespace nsm
