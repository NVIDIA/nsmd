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

#include "nsmChassisPCIeSlot.hpp"

#include "dBusAsyncUtils.hpp"
#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmInterface.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPCIeLinkSpeed.hpp"
#include "utils.hpp"

namespace nsm
{

requester::Coroutine
    nsmChassisPCIeSlotCreateSensors(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath)
{
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string chassisName{};
    if (allCurrentIfaceProperties.count("ChassisName"))
    {
        chassisName =
            std::get<std::string>(allCurrentIfaceProperties.at("ChassisName"));
    }
    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    unsigned long long deviceIndex{};
    if (allCurrentIfaceProperties.count("DeviceIndex"))
    {
        deviceIndex = std::get<unsigned long long>(
            allCurrentIfaceProperties.at("DeviceIndex"));
    }
    std::string slotType{};
    if (allCurrentIfaceProperties.count("SlotType"))
    {
        slotType =
            std::get<std::string>(allCurrentIfaceProperties.at("SlotType"));
    }
    bool priority{};
    if (allCurrentIfaceProperties.count("Priority"))
    {
        priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
    }

    auto device = manager.getNsmDevice(uuid);

    auto pcieSlotProvider = NsmChassisPCIeSlot<PCIeSlotIntf>(chassisName, name);
    pcieSlotProvider.invoke(pdiMethod(slotType),
                            PCIeSlotIntf::convertSlotTypesFromString(slotType));
    device->addSensor(std::make_shared<NsmPCIeLinkSpeed<PCIeSlotIntf>>(
                          pcieSlotProvider, deviceIndex),
                      priority);

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);
    auto associationsObject =
        std::make_shared<NsmChassisPCIeSlot<AssociationDefinitionsIntf>>(
            chassisName, name);
    associationsObject->invoke(pdiMethod(associations),
                               utils::getAssociations(associations));
    device->addStaticSensor(associationsObject);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    nsmChassisPCIeSlotCreateSensors,
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeSlot")

} // namespace nsm
