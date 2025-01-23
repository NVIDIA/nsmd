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

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"
#include "deviceManager.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <vector>

namespace nsm
{
requester::Coroutine
    saveNsmMapInstanceTable([[maybe_unused]] SensorManager& manager,
                            const std::string& interface,
                            const std::string& objPath)
{
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto type = interface.substr(interface.find_last_of('.') + 1);
    DeviceManager& deviceManager = DeviceManager::getInstance();

    uint8_t deviceType = NSM_DEV_ID_UNKNOWN;
    if (name == "GPUMapping")
    {
        deviceType = NSM_DEV_ID_GPU;
    }
    else if (name == "SwitchMapping")
    {
        deviceType = NSM_DEV_ID_SWITCH;
    }
    else if (name == "PCIeBridgeMapping")
    {
        deviceType = NSM_DEV_ID_PCIE_BRIDGE;
    }
    else if (name == "BaseboardMapping")
    {
        deviceType = NSM_DEV_ID_BASEBOARD;
    }
    else if (name == "ERoTMapping")
    {
        deviceType = NSM_DEV_ID_EROT;
    }
    if (deviceType == NSM_DEV_ID_UNKNOWN)
    {
        lg2::error(
            "Unsupported InstanceNumber mapping table : Name={NAME}, Object_Path={OBJPATH}",
            "NAME", name, "OBJPATH", objPath);
        co_return NSM_ERROR;
    }

    if (type == "NSM_GetInstanceIDByDeviceInstanceID")
    {
        auto mappingArray =
            co_await utils::coGetDbusProperty<std::vector<uint64_t>>(
                objPath.c_str(), "MappingArray", interface.c_str());
        if (mappingArray.size() > 0)
        {
            deviceManager.mapInstanceNumberToInstanceNumber[deviceType] =
                mappingArray;
        }
    }
    else if (type == "NSM_GetInstanceIDByMctpUUID")
    {
        auto mappingArray =
            co_await utils::coGetDbusProperty<std::vector<std::string>>(
                objPath.c_str(), "MappingArray", interface.c_str());
        if (mappingArray.size() > 0)
        {
            deviceManager.mapUuidToInstanceNumber[deviceType] = mappingArray;
        }
    }
    else if (type == "NSM_GetInstanceIDByDeviceEID")
    {
        auto mappingArray =
            co_await utils::coGetDbusProperty<std::vector<uint64_t>>(
                objPath.c_str(), "MappingArray", interface.c_str());
        if (mappingArray.size() > 0)
        {
            deviceManager.mapEidToInstanceNumber[deviceType] = mappingArray;
        }
    }
    co_return NSM_SUCCESS;
}

std::vector<std::string> instanceMapTableInterfaces{
    "xyz.openbmc_project.Configuration.NSM_GetInstanceIDByDeviceInstanceID",
    "xyz.openbmc_project.Configuration.NSM_GetInstanceIDByMctpUUID",
    "xyz.openbmc_project.Configuration.NSM_GetInstanceIDByDeviceEID"};

REGISTER_NSM_CREATION_FUNCTION(saveNsmMapInstanceTable,
                               instanceMapTableInterfaces)

} // namespace nsm
