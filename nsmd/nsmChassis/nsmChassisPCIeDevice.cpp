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

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#if defined(ENABLE_PCIE_AER_ERROR)
#include "nsmAERError.hpp"
#endif
#if defined(ENABLE_CLOCK_OUTPUT_STATE)
#include "nsmClockOutputEnableState.hpp"
#endif
#include "nsmCommon.hpp"
#include "nsmDevice.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPCIeFunction.hpp"
#if defined(ENABLE_PCIE_LTSSM_STATE)
#include "nsmPCIeLTSSMState.hpp"
#endif
#include "nsmPCIeLinkSpeed.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <unordered_map>

namespace nsm
{

template <typename IntfType>
requester::Coroutine
    NsmChassisPCIeDevice<IntfType>::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    if constexpr (std::is_same_v<IntfType, UuidIntf>)
    {
        // For UuidIntf, we need to get the device UUID from the device manager.
        mctp::MctpDiscovery& mctpDiscovery = mctp::MctpDiscovery::getInstance();
        uuid_t deviceUuid;
        auto rc = co_await getDeviceUUID(nsmDevice, mctpDiscovery, deviceUuid);
        if (rc == NSM_SW_SUCCESS && !deviceUuid.empty())
        {
            this->invoke(pdiMethod(uuid), deviceUuid);
            co_return NSM_SUCCESS;
        }
        co_return NSM_ERROR;
    }
    // For other interfaces, we don't need to update anything.
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

    dbus::PropertyMap allBaseIfaceProperties;
    auto rc = co_await utils::coGetCachedBaseProperties(objPath, baseInterface,
                                                        allBaseIfaceProperties);
    if (rc != NSM_SUCCESS)
    {
        co_return rc;
    }
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string chassisName{};
    if (allBaseIfaceProperties.count("ChassisName"))
    {
        chassisName =
            std::get<std::string>(allBaseIfaceProperties.at("ChassisName"));
    }
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

    if (type == "NSM_ChassisPCIeDevice")
    {
        uuid_t deviceUuid{};
        if (allCurrentIfaceProperties.count("DEVICE_UUID"))
        {
            deviceUuid =
                std::get<uuid_t>(allCurrentIfaceProperties.at("DEVICE_UUID"));
        }

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
        std::string manufacturer{};
        if (allCurrentIfaceProperties.count("Manufacturer"))
        {
            manufacturer = std::get<std::string>(
                allCurrentIfaceProperties.at("Manufacturer"));
        }

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
        std::string health{};
        if (allCurrentIfaceProperties.count("Health"))
        {
            health =
                std::get<std::string>(allCurrentIfaceProperties.at("Health"));
        }

        auto healthObject = std::make_shared<NsmChassisPCIeDevice<HealthIntf>>(
            chassisName, name);
        healthObject->invoke(pdiMethod(health),
                             HealthIntf::convertHealthTypeFromString(health));
        device->addStaticSensor(healthObject);
    }
    else if (type == "NSM_PCIeDevice")
    {
        std::string deviceType{};
        if (allCurrentIfaceProperties.count("DeviceType"))
        {
            deviceType = std::get<std::string>(
                allCurrentIfaceProperties.at("DeviceType"));
        }
        std::vector<uint64_t> functionIds{};
        if (allCurrentIfaceProperties.count("Functions"))
        {
            functionIds = std::get<std::vector<uint64_t>>(
                allCurrentIfaceProperties.at("Functions"));
        }

        auto pcieDeviceObject =
            NsmChassisPCIeDevice<PCIeDeviceIntf>(chassisName, name);
        pcieDeviceObject.invoke(pdiMethod(deviceType),
                PCIeDeviceIntf::convertDeviceTypesFromString(deviceType));
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
#if defined(ENABLE_PCIE_AER_ERROR)
            const std::string inventoyObjPath =
                chassisInventoryBasePath / chassisName / "PCIeDevices" / name;
            auto aerErrorIntf = std::make_shared<NsmAERErrorStatusIntf>(
                utils::DBusHandler::getBus(), inventoyObjPath.c_str(), 0,
                device);
            auto aerErrorSensor = std::make_shared<NsmPCIeAERErrorStatus>(
                name, "PCIeAerErrorStatus", aerErrorIntf, 0);
            aerErrorIntf->linkAerStatusSensor(aerErrorSensor);
            device->addSensor(aerErrorSensor, AER_ERR_SENSOR_PRIORITY);
#endif
        }
    }
    else if (type == "NSM_MultiPortPCIeDevice")
    {
        auto deviceType = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "DeviceType", interface.c_str());
        auto functionIds =
            co_await utils::coGetDbusProperty<std::vector<uint64_t>>(
                objPath.c_str(), "Functions", interface.c_str());
        auto pcieDeviceObject =
            NsmChassisPCIeDevice<PCIeDeviceIntf>(chassisName, name);
        pcieDeviceObject.invoke(pdiMethod(deviceType),
                PCIeDeviceIntf::convertDeviceTypesFromString(deviceType));
        device->addSensor(std::make_shared<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
                              pcieDeviceObject, 0, 0, 0),
                          PCIE_LINK_SPEED_PCIE_DEVICE_PRIORITY);

        for (auto& id : functionIds)
        {
            auto function = std::make_shared<NsmPCIeFunction>(pcieDeviceObject,
                                                              id, 0, 0, 0);
            device->addStaticSensor(function);
        }
    }
    else if (type == "NSM_LTSSMState")
    {
#if defined(ENABLE_PCIE_LTSSM_STATE)
        uint64_t deviceIndex{};
        if (allCurrentIfaceProperties.count("DeviceIndex"))
        {
            deviceIndex =
                std::get<uint64_t>(allCurrentIfaceProperties.at("DeviceIndex"));
        }
        bool priority{};
        if (allCurrentIfaceProperties.count("Priority"))
        {
            priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
        }
        std::string inventoryObjPath{};
        if (allCurrentIfaceProperties.count("InventoryObjPath"))
        {
            inventoryObjPath = std::get<std::string>(
                allCurrentIfaceProperties.at("InventoryObjPath"));
        }

        auto ltssmStateObject = NsmChassisPCIeDevice<LTSSMStateIntf>(
            name, dbus::Interfaces{inventoryObjPath});
        device->addSensor(
            std::make_shared<NsmPCIeLTSSMState>(ltssmStateObject, deviceIndex),
            priority);
#endif
    }
    else if (type == "NSM_ClockOutputEnableState")
    {
#if defined(ENABLE_CLOCK_OUTPUT_STATE)
        uint64_t instanceNumber{};
        if (allCurrentIfaceProperties.count("InstanceNumber"))
        {
            instanceNumber = std::get<uint64_t>(
                allCurrentIfaceProperties.at("InstanceNumber"));
        }

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
#endif
    }

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

dbus::Interfaces chassisPCIeDeviceInterfaces{
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.Asset",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.Health",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.PCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.MultiPortPCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.LTSSMState",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.ClockOutputEnableState",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.AERErrorStatus"};

REGISTER_NSM_CREATION_FUNCTION(nsmChassisPCIeDeviceCreateSensors,
                               chassisPCIeDeviceInterfaces)

} // namespace nsm
