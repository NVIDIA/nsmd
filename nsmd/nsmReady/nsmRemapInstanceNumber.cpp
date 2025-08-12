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
#include "nsmObjectFactory.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <phosphor-logging/lg2.hpp>

#include <vector>

namespace nsm
{
requester::Coroutine
    saveNsmMapInstanceTable([[maybe_unused]] SensorManager& manager,
                            const std::string& interface,
                            const std::string& objPath)
{
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }

    auto type = interface.substr(interface.find_last_of('.') + 1);
    mctp::MctpDiscovery& mctpDiscovery = mctp::MctpDiscovery::getInstance();

    uint8_t deviceType = NSM_DEV_ID_UNKNOWN;
    uint8_t deviceRole = NSM_DEV_ROLE_RESERVED;
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
    else if (name == "SMAMapping")
    {
        deviceType = NSM_DEV_ID_MCTP_BRIDGE;
    }
    else if (name == "SXMSMAMapping")
    {
        deviceType = NSM_DEV_ID_MCTP_BRIDGE;
        deviceRole = NSM_MCTP_BRIDGE_DEV_ROLE_SXM_SMA;
    }
    else if (name == "CXMSMAMapping")
    {
        deviceType = NSM_DEV_ID_MCTP_BRIDGE;
        deviceRole = NSM_MCTP_BRIDGE_DEV_ROLE_CX_SMA;
    }
    else if (name == "CX7PCIEBridgeMapping")
    {
        deviceType = NSM_DEV_ID_PCIE_BRIDGE;
        deviceRole = NSM_PCIE_BRIDGE_DEV_ROLE_CX7;
    }
    else if (name == "CX8PCIEBridgeMapping")
    {
        deviceType = NSM_DEV_ID_PCIE_BRIDGE;
        deviceRole = NSM_PCIE_BRIDGE_DEV_ROLE_CX8;
    }
    if (deviceType == NSM_DEV_ID_UNKNOWN)
    {
        lg2::error(
            "Unsupported InstanceNumber mapping table : Name={NAME}, Object_Path={OBJPATH}",
            "NAME", name, "OBJPATH", objPath);
        co_return NSM_ERROR;
    }

    uint16_t deviceTypeAndRole = utils::combineDeviceTypeAndRole(deviceType,
                                                                 deviceRole);

    if (type == "NSM_GetInstanceIDByDeviceInstanceID")
    {
        std::vector<uint64_t> mappingArray{};
        if (allCurrentIfaceProperties.count("MappingArray"))
        {
            mappingArray = std::get<std::vector<uint64_t>>(
                allCurrentIfaceProperties.at("MappingArray"));
        }

        if (mappingArray.size() > 0)
        {
            mctpDiscovery.mapInstanceNumberToInstanceNumber[deviceTypeAndRole] =
                mappingArray;
        }
    }
    else if (type == "NSM_GetInstanceIDByMctpUUID")
    {
        std::vector<std::string> mappingArray{};
        if (allCurrentIfaceProperties.count("MappingArray"))
        {
            mappingArray = std::get<std::vector<std::string>>(
                allCurrentIfaceProperties.at("MappingArray"));
        }

        if (mappingArray.size() > 0)
        {
            mctpDiscovery.mapUuidToInstanceNumber[deviceTypeAndRole] =
                mappingArray;
        }
    }
    else if (type == "NSM_GetInstanceIDByDeviceEID")
    {
        std::vector<uint64_t> mappingArray{};
        if (allCurrentIfaceProperties.count("MappingArray"))
        {
            mappingArray = std::get<std::vector<uint64_t>>(
                allCurrentIfaceProperties.at("MappingArray"));
        }

        if (mappingArray.size() > 0)
        {
            mctpDiscovery.mapEidToInstanceNumber[deviceTypeAndRole] =
                mappingArray;
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
