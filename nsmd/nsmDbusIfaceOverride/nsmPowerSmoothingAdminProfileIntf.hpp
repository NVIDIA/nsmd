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
#pragma once
#include <com/nvidia/Cpu/AdminPowerProfile/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
namespace nsm
{
using AdminPowerProfileIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::Cpu::server::AdminPowerProfile>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
class OemAdminProfileIntf :
    public AdminPowerProfileIntf,
    public AssociationDefinitionsIntf
{
  private:
    std::shared_ptr<NsmDevice> device;
    std::string parentPath;
    std::string inventoryObjPath;

  public:
    OemAdminProfileIntf(sdbusplus::bus::bus& bus, const std::string& parentPath,
                        std::shared_ptr<NsmDevice> device) :
        AdminPowerProfileIntf(bus,
                              (parentPath + "/profiles/admin_profile").c_str()),
        AssociationDefinitionsIntf(
            bus, (parentPath + "/profiles/admin_profile").c_str()),
        device(device), parentPath(parentPath),
        inventoryObjPath(parentPath + "/profiles/admin_profile")

    {
        std::vector<std::tuple<std::string, std::string, std::string>>
            associations_list;
        std::string forward = "parent_device";
        std::string backward = "admin_override";
        associations_list.emplace_back(forward, backward, parentPath);
        AssociationDefinitionsIntf::associations(associations_list);
    }
    std::string getInventoryObjPath()
    {
        return inventoryObjPath;
    }
};
} // namespace nsm
