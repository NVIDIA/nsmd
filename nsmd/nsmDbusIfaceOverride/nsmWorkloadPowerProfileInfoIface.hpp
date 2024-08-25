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
#include <com/nvidia/PowerProfile/Profile/server.hpp>
#include <com/nvidia/PowerProfile/ProfileInfo/server.hpp>
#include <com/nvidia/PowerProfile/ProfileInfoAsync/server.hpp>
namespace nsm
{
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

using ProfileInfoIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerProfile::server::ProfileInfo>;
using ProfileInfoAsyncIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerProfile::server::ProfileInfoAsync>;
class OemProfileInfoIntf : public ProfileInfoIntf
{
  private:
    std::shared_ptr<NsmDevice> device;
    std::string inventoryObjPath;

  public:
    OemProfileInfoIntf(sdbusplus::bus::bus& bus, const std::string& path,
                       std::shared_ptr<NsmDevice> device) :
        ProfileInfoIntf(bus, path.c_str()),
        device(device), inventoryObjPath(path)
    {}
};

using WorkLoadPowerProfileIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerProfile::server::Profile>;
class OemWorkLoadPowerProfileIntf :
    public WorkLoadPowerProfileIntf,
    public AssociationDefinitionsIntf
{
  private:
    std::shared_ptr<NsmDevice> device;
    std::string parentPath;
    std::string inventoryObjPath;
    uint16_t profileId;
    std::string profileName;

  public:
    OemWorkLoadPowerProfileIntf(sdbusplus::bus::bus& bus,
                                const std::string& parentPath,
                                uint16_t profileId, std::string& profileName,
                                std::shared_ptr<NsmDevice> device) :
        WorkLoadPowerProfileIntf(
            bus, (parentPath + "/workload/profile/" + std::to_string(profileId))
                     .c_str()),
        AssociationDefinitionsIntf(
            bus, (parentPath + "/workload/profile/" + std::to_string(profileId))
                     .c_str()),
        device(device), parentPath(parentPath),
        inventoryObjPath(parentPath + "/workload/profile/" +
                         std::to_string(profileId)),
        profileId(profileId), profileName(profileName)

    {
        std::vector<std::tuple<std::string, std::string, std::string>>
            associations_list;
        std::string forward = "parent_device";
        std::string backward = "workload_power_profile";
        associations_list.emplace_back(forward, backward, parentPath);
        AssociationDefinitionsIntf::associations(associations_list);
        WorkLoadPowerProfileIntf::profileName(profileName);
    }
};

} // namespace nsm
