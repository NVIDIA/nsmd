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

#include "nsmChassisPCIeDevice.hpp"

#include "deviceManager.hpp"
#include "nsmClockOutputEnableState.hpp"
#include "nsmDevice.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPCIeFunction.hpp"
#include "nsmPCIeLTSSMState.hpp"
#include "nsmPCIeLinkSpeed.hpp"
#include "utils.hpp"

namespace nsm
{

template <typename IntfType>
requester::Coroutine NsmChassisPCIeDevice<IntfType>::update(
    [[maybe_unused]] SensorManager& manager, [[maybe_unused]] eid_t eid)
{
    DeviceManager& deviceManager = DeviceManager::getInstance();
    auto uuid = utils::getUUIDFromEID(deviceManager.getEidTable(), eid);
    if (uuid)
    {
        if constexpr (std::is_same_v<IntfType, UuidIntf>)
        {
            auto nsmDevice = manager.getNsmDevice(*uuid);
            if (nsmDevice)
            {
                this->pdi().uuid(nsmDevice->deviceUuid);
            }
        }
    }
    co_return NSM_SUCCESS;
}

void nsmChassisPCIeDeviceCreateSensors(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice";

    auto chassisName = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "ChassisName", baseInterface.c_str());
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == "NSM_ChassisPCIeDevice")
    {
        auto deviceUuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "DEVICE_UUID", interface.c_str());
        auto uuidObject =
            std::make_shared<NsmChassisPCIeDevice<UuidIntf>>(chassisName, name);
        auto associations =
            utils::getAssociations(objPath, baseInterface + ".Associations");
        auto associationsObject =
            std::make_shared<NsmChassisPCIeDevice<AssociationDefinitionsIntf>>(
                chassisName, name);

        uuidObject->pdi().uuid(deviceUuid);
        associationsObject->pdi().associations(
            utils::getAssociations(associations));

        device->addStaticSensor(uuidObject);
        device->addStaticSensor(associationsObject);
    }
    else if (type == "NSM_Asset")
    {
        auto assetObject = NsmChassisPCIeDevice<AssetIntf>(chassisName, name);
        auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());
        assetObject.pdi().manufacturer(manufacturer);
        // create sensor
        auto partNumber = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            assetObject, DEVICE_PART_NUMBER);
        auto serialNumber = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            assetObject, SERIAL_NUMBER);
        auto model = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            assetObject, MARKETING_NAME);
        device->addStaticSensor(partNumber);
        device->addStaticSensor(serialNumber);
        device->addStaticSensor(model);
    }
    else if (type == "NSM_Health")
    {
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());
        auto healthObject = std::make_shared<NsmChassisPCIeDevice<HealthIntf>>(
            chassisName, name);
        healthObject->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        device->addStaticSensor(healthObject);
    }
    else if (type == "NSM_PCIeDevice")
    {
        auto deviceType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "DeviceType", interface.c_str());
        auto deviceIndex = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "DeviceIndex", interface.c_str());
        auto functionIds =
            utils::DBusHandler().getDbusProperty<std::vector<uint64_t>>(
                objPath.c_str(), "Functions", interface.c_str());
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto pcieDeviceObject =
            NsmChassisPCIeDevice<PCIeDeviceIntf>(chassisName, name);
        pcieDeviceObject.pdi().deviceType(deviceType);
        device->addSensor(std::make_shared<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
                              pcieDeviceObject, deviceIndex),
                          priority);

        for (auto& id : functionIds)
        {
            auto function = std::make_shared<NsmPCIeFunction>(pcieDeviceObject,
                                                              deviceIndex, id);
            device->addStaticSensor(function);
        }
    }
    else if (type == "NSM_LTSSMState")
    {
        auto deviceIndex = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "DeviceIndex", interface.c_str());
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto inventoryObjPath =
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "InventoryObjPath", interface.c_str());

        auto ltssmStateObject = NsmChassisPCIeDevice<LTSSMStateIntf>(
            name, dbus::Interfaces{inventoryObjPath});
        device->addSensor(
            std::make_shared<NsmPCIeLTSSMState>(ltssmStateObject, deviceIndex),
            priority);
        lg2::debug("Created LTSSMStateIntf sensor {NAME} path: {PATH}", "NAME",
                   name, "PATH", inventoryObjPath);
    }
    else if (type == "NSM_ClockOutputEnableState")
    {
        auto instanceNumber = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", interface.c_str());
        auto deviceType = (NsmDeviceIdentification)utils::DBusHandler()
                              .getDbusProperty<uint64_t>(objPath.c_str(),
                                                         "DeviceType",
                                                         interface.c_str());
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto pcieRefClockObject =
            NsmChassisPCIeDevice<PCIeRefClockIntf>(chassisName, name);
        auto nvLinkRefClockObject =
            NsmChassisPCIeDevice<NVLinkRefClockIntf>(chassisName, name);

        auto pcieRefClock =
            std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
                pcieRefClockObject, PCIE_CLKBUF_INDEX, deviceType,
                instanceNumber);
        auto nvLinkRefClock =
            std::make_shared<NsmClockOutputEnableState<NVLinkRefClockIntf>>(
                nvLinkRefClockObject, NVHS_CLKBUF_INDEX, deviceType,
                instanceNumber);
        device->addSensor(pcieRefClock, priority);
        device->addSensor(nvLinkRefClock, priority);
    }
}

dbus::Interfaces chassisPCIeDeviceInterfaces{
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.Asset",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.Health",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.PCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.LTSSMState",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.ClockOutputEnableState"};

REGISTER_NSM_CREATION_FUNCTION(nsmChassisPCIeDeviceCreateSensors,
                               chassisPCIeDeviceInterfaces)

} // namespace nsm
