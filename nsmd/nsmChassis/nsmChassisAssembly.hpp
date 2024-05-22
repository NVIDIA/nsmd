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

#include <xyz/openbmc_project/Inventory/Decorator/Area/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Assembly/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using AreaIntf = object_t<Inventory::Decorator::server::Area>;
using AssemblyIntf = object_t<Inventory::Item::server::Assembly>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using LocationIntf = object_t<Inventory::Decorator::server::Location>;
using HealthIntf = object_t<State::Decorator::server::Health>;

template <typename IntfType>
class NsmChassisAssembly : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmChassisAssembly() = delete;
    NsmChassisAssembly(const std::string& chassisName,
                       const std::string& name) :
        NsmInterfaceProvider<IntfType>(name, "NSM_ChassisAssembly",
                                       chassisInventoryBasePath /
                                           chassisName)
    {}
};

} // namespace nsm