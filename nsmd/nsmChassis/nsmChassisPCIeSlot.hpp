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
#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Inventory/Item/PCIeSlot/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using PCIeSlotIntf = object_t<Inventory::Item::server::PCIeSlot>;

class NsmChassisPCIeSlot : public NsmInterfaceProvider<PCIeSlotIntf>
{
  public:
    NsmChassisPCIeSlot() = delete;
    NsmChassisPCIeSlot(const std::string& chassisName,
                       const std::string& name) :
        NsmInterfaceProvider<PCIeSlotIntf>(
            name, "NSM_ChassisPCIeSlot", chassisInventoryBasePath / chassisName)
    {}
};

} // namespace nsm