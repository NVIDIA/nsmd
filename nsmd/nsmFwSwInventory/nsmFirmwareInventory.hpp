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
#include "globals.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using AssociationDefinitionsIntf = object_t<Association::server::Definitions>;

template <typename IntfType>
class NsmFirmwareInventory : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmFirmwareInventory() = delete;
    NsmFirmwareInventory(const std::string& name) :
        NsmInterfaceProvider<IntfType>(name, "NSM_FirmwareInventory",
                                       firmwareInventoryBasePath)
    {}
    NsmFirmwareInventory(const std::string& name, const path& inventoryPath,
                         const std::shared_ptr<IntfType>& pdi) :
        NsmInterfaceProvider<IntfType>(name, "NSM_FirmwareInventory",
                                       inventoryPath, pdi)
    {}
};

} // namespace nsm
