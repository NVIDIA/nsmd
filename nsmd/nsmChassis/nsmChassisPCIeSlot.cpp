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

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmInterface.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPCIeLinkSpeed.hpp"
#include "utils.hpp"

namespace nsm
{

void nsmChassisPCIeSlotCreateSensors(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath)
{

    auto chassisName = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "ChassisName", interface.c_str());
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto deviceId = utils::DBusHandler().getDbusProperty<uint64_t>(
        objPath.c_str(), "InstanceNumber", interface.c_str());
    auto slotType = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "SlotType", interface.c_str());
    auto device = manager.getNsmDevice(uuid);

    auto pcieSlotProvider = NsmChassisPCIeSlot(chassisName, name);
    pcieSlotProvider.pdi().slotType(
        PCIeSlotIntf::convertSlotTypesFromString(slotType));
    addSensor(device,
              std::make_shared<NsmPCIeLinkSpeed<PCIeSlotIntf>>(pcieSlotProvider,
                                                               deviceId),
              objPath, interface);
}

REGISTER_NSM_CREATION_FUNCTION(
    nsmChassisPCIeSlotCreateSensors,
    "xyz.openbmc_project.Configuration.NSM_ChassisPCIeSlot")

} // namespace nsm
