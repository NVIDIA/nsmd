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

#include "nsmReset.hpp"

#include "pci-links.h"
#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>
namespace nsm
{
NsmReset::NsmReset(sdbusplus::bus::bus& bus, const std::string& name,
                   const std::string& type, std::string& inventoryObjPath,
                   std::shared_ptr<NsmDevice> device, const uint8_t deviceIndex) :
    NsmObject(name, type)
{
    lg2::info("NsmReset: create sensor:{NAME}", "NAME", name.c_str());
    resetIntf = std::make_shared<NsmResetIntf>(bus, inventoryObjPath.c_str(),
                                               device, deviceIndex);
    resetIntf->resetType(sdbusplus::common::xyz::openbmc_project::control::
                             processor::Reset::ResetTypes::ForceRestart);
}

static void createNsmResetSensor(SensorManager& manager,
                                 const std::string& interface,
                                 const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto name = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());

        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());

        auto type = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto inventoryObjPath =
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "InventoryObjPath", interface.c_str());

        auto instanceNumber = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", interface.c_str());

        inventoryObjPath = inventoryObjPath + std::to_string(instanceNumber);

        auto deviceIndex = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "DeviceIndex", interface.c_str());

        auto nsmDevice = manager.getNsmDevice(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_Processor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            return;
        }
        auto resetSensor = std::make_shared<NsmReset>(
            bus, name, type, inventoryObjPath, nsmDevice, deviceIndex);
        nsmDevice->deviceSensors.push_back(resetSensor);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Error while addSensor for path {PATH} and interface {INTF}, {ERROR}",
            "PATH", objPath, "INTF", interface, "ERROR", e);
        return;
    }
}

REGISTER_NSM_CREATION_FUNCTION(createNsmResetSensor,
                               "xyz.openbmc_project.Configuration.NSM_GpuReset")
} // namespace nsm