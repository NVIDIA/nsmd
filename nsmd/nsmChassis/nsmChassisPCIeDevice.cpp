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

#include "nsmPriorityMapping.h"

#include "dBusAsyncUtils.hpp"
#include "deviceManager.hpp"
#include "nsmAERError.hpp"
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
                this->invoke(pdiMethod(uuid), nsmDevice->deviceUuid);
            }
        }
    }

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine
    nsmChassisPCIeDeviceCreateSensors(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice";

    auto chassisName = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "ChassisName", baseInterface.c_str());
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == "NSM_ChassisPCIeDevice")
    {
        auto deviceUuid = co_await utils::coGetDbusProperty<uuid_t>(
            objPath.c_str(), "DEVICE_UUID", interface.c_str());
        auto uuidObject =
            std::make_shared<NsmChassisPCIeDevice<UuidIntf>>(chassisName, name);
        std::vector<utils::Association> associations{};
        co_await utils::coGetAssociations(
            objPath, baseInterface + ".Associations", associations);
        auto associationsObject =
            std::make_shared<NsmChassisPCIeDevice<AssociationDefinitionsIntf>>(
                chassisName, name);

        uuidObject->invoke(pdiMethod(uuid), deviceUuid);
        associationsObject->invoke(pdiMethod(associations),
                                   utils::getAssociations(associations));

        device->addStaticSensor(uuidObject);
        device->addStaticSensor(associationsObject);
    }
    else if (type == "NSM_Asset")
    {
        auto assetObject = NsmChassisPCIeDevice<NsmAssetIntf>(chassisName,
                                                              name);
        auto manufacturer = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());
        assetObject.invoke(pdiMethod(manufacturer), manufacturer);
        // create sensor
        auto partNumber = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            assetObject, DEVICE_PART_NUMBER);
        auto serialNumber =
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(assetObject,
                                                                 SERIAL_NUMBER);
        auto model = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            assetObject, MARKETING_NAME);
        device->addStaticSensor(partNumber);
        device->addStaticSensor(serialNumber);
        device->addStaticSensor(model);
    }
    else if (type == "NSM_Health")
    {
        auto health = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());
        auto healthObject = std::make_shared<NsmChassisPCIeDevice<HealthIntf>>(
            chassisName, name);
        healthObject->invoke(pdiMethod(health),
                             HealthIntf::convertHealthTypeFromString(health));
        device->addStaticSensor(healthObject);
    }
    else if (type == "NSM_PCIeDevice")
    {
        auto deviceType = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "DeviceType", interface.c_str());
        auto functionIds =
            co_await utils::coGetDbusProperty<std::vector<uint64_t>>(
                objPath.c_str(), "Functions", interface.c_str());
        auto pcieDeviceObject =
            NsmChassisPCIeDevice<PCIeDeviceIntf>(chassisName, name);
        pcieDeviceObject.invoke(pdiMethod(deviceType), deviceType);
        device->addSensor(std::make_shared<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
                              pcieDeviceObject, 0),
                          PCIE_LINK_SPEED_PCIE_DEVICE_PRIORITY);

        for (auto& id : functionIds)
        {
            auto function = std::make_shared<NsmPCIeFunction>(pcieDeviceObject,
                                                              0, id);
            device->addStaticSensor(function);
        }
        if (device->getDeviceType() == NSM_DEV_ID_GPU)
        {
            const std::string inventoyObjPath =
                chassisInventoryBasePath / chassisName / "PCIeDevices" / name;
            auto aerErrorIntf = std::make_shared<NsmAERErrorStatusIntf>(
                utils::DBusHandler::getBus(), inventoyObjPath.c_str(), 0,
                device);
            auto aerErrorSensor = std::make_shared<NsmPCIeAERErrorStatus>(
                name, "PCIeAerErrorStatus", aerErrorIntf, 0);
            aerErrorIntf->linkAerStatusSensor(aerErrorSensor);
            device->addSensor(aerErrorSensor, AER_ERR_SENSOR_PRIORITY);
        }
    }
    else if (type == "NSM_LTSSMState")
    {
        auto deviceIndex = co_await utils::coGetDbusProperty<uint64_t>(
            objPath.c_str(), "DeviceIndex", interface.c_str());
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "InventoryObjPath", interface.c_str());

        auto ltssmStateObject = NsmChassisPCIeDevice<LTSSMStateIntf>(
            name, dbus::Interfaces{inventoryObjPath});
        device->addSensor(
            std::make_shared<NsmPCIeLTSSMState>(ltssmStateObject, deviceIndex),
            priority);
    }
    else if (type == "NSM_ClockOutputEnableState")
    {
        auto instanceNumber = co_await utils::coGetDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", interface.c_str());
        auto deviceType = (NsmDeviceIdentification)device->getDeviceType();
        auto pcieRefClockObject =
            NsmChassisPCIeDevice<PCIeRefClockIntf>(chassisName, name);

        auto pcieRefClock =
            std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
                pcieRefClockObject, PCIE_CLKBUF_INDEX, deviceType,
                instanceNumber);
        device->addSensor(pcieRefClock, CLOCK_OUTPUT_ENABLE_STATE_PRIORITY);

        if (deviceType == NSM_DEV_ID_GPU)
        {
            auto nvLinkRefClockObject =
                NsmChassisPCIeDevice<NVLinkRefClockIntf>(chassisName, name);
            auto nvLinkRefClock =
                std::make_shared<NsmClockOutputEnableState<NVLinkRefClockIntf>>(
                    nvLinkRefClockObject, NVHS_CLKBUF_INDEX, deviceType,
                    instanceNumber);
            device->addSensor(nvLinkRefClock,
                              CLOCK_OUTPUT_ENABLE_STATE_PRIORITY);
        }
    }

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

dbus::Interfaces chassisPCIeDeviceInterfaces{
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.Asset",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.Health",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.PCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.LTSSMState",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.ClockOutputEnableState",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.AERErrorStatus"};

REGISTER_NSM_CREATION_FUNCTION(nsmChassisPCIeDeviceCreateSensors,
                               chassisPCIeDeviceInterfaces)

} // namespace nsm
