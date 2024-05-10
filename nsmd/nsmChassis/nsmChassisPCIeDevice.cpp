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

#include "nsmDevice.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPCIeDevice.hpp"
#include "nsmPCIeFunction.hpp"
#include "nsmPCIeLTSSMState.hpp"
#include "utils.hpp"

namespace nsm
{

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
        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());
        auto uuidObject =
            std::make_shared<NsmChassisPCIeDevice<UuidIntf>>(chassisName, name);
        uuidObject->pdi().uuid(uuid);
        addSensor(device, uuidObject);
    }
    else if (type == "NSM_Asset")
    {
        auto assetObject = NsmChassisPCIeDevice<AssetIntf>(chassisName, name);
        auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());
        assetObject.pdi().manufacturer(manufacturer);
        // create sensor
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, BOARD_PART_NUMBER));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, SERIAL_NUMBER));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, MARKETING_NAME));
    }
    else if (type == "NSM_Health")
    {
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());
        auto healthObject = std::make_shared<NsmChassisPCIeDevice<HealthIntf>>(
            chassisName, name);
        healthObject->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        addSensor(device, healthObject);
    }
    else if (type == "NSM_PCIeDevice")
    {
        auto deviceType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "DeviceType", interface.c_str());
        auto deviceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", interface.c_str());
        auto functionIds =
            utils::DBusHandler().getDbusProperty<std::vector<uint64_t>>(
                objPath.c_str(), "Functions", interface.c_str());
        auto pcieDeviceObject =
            NsmChassisPCIeDevice<PCIeDeviceIntf>(chassisName, name);
        pcieDeviceObject.pdi().deviceType(deviceType);
        addSensor(device,
                  std::make_shared<NsmPCIeDevice>(pcieDeviceObject, deviceId),
                  objPath, interface);
        for (auto& id : functionIds)
        {
            addSensor(manager, device,
                      std::make_shared<NsmPCIeFunction>(pcieDeviceObject,
                                                        deviceId, id));
        }
    }
    else if (type == "NSM_LTSSMState")
    {
        auto deviceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "DeviceId", interface.c_str());
        auto ltssmStateObject =
            NsmChassisPCIeDevice<LTSSMStateIntf>(chassisName, name);
        addSensor(
            device,
            std::make_shared<NsmPCIeLTSSMState>(ltssmStateObject, deviceId),
            objPath, interface);
    }
}

std::vector<std::string> chassisPCIeDeviceInterfaces{
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.Asset",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.Health",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.PCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice.LTSSMState"};

REGISTER_NSM_CREATION_FUNCTION(nsmChassisPCIeDeviceCreateSensors,
                               chassisPCIeDeviceInterfaces)

} // namespace nsm
