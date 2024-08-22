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
#include "platform-environmental.h"

#include "globals.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Chassis/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <iostream>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using LocationIntf = object_t<Inventory::Decorator::server::Location>;
using ChassisIntf = object_t<Inventory::Item::server::Chassis>;

class NsmPCIeRetimerChassis : public NsmObject
{
  public:
    NsmPCIeRetimerChassis(sdbusplus::bus::bus& bus, const std::string& name,
                          const std::vector<utils::Association>& associations,
                          const std::string& type);

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDef_ = nullptr;
    std::unique_ptr<NsmAssetIntf> asset_ = nullptr;
    std::unique_ptr<LocationIntf> location_ = nullptr;
    std::unique_ptr<ChassisIntf> chassis_ = nullptr;
};
} // namespace nsm
