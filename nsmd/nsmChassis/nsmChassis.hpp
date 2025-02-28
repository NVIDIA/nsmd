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
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Dimension/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PCIeRefClock/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PowerLimit/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Chassis/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/MCTP/UUID/server.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using UuidIntf = object_t<Common::server::UUID>;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using DimensionIntf = object_t<Inventory::Decorator::server::Dimension>;
using LocationIntf = object_t<Inventory::Decorator::server::Location>;
using LocationCodeIntf = object_t<Inventory::Decorator::server::LocationCode>;
using MctpUuidIntf = object_t<MCTP::server::UUID>;
using PowerLimitIntf = object_t<Inventory::Decorator::server::PowerLimit>;
using PCIeRefClockIntf = object_t<Inventory::Decorator::server::PCIeRefClock>;
using ChassisIntf = object_t<Inventory::Item::server::Chassis>;
using ItemIntf = object_t<Inventory::server::Item>;
using PowerStateIntf = object_t<State::server::Chassis>;
using OperationalStatusIntf =
    object_t<State::Decorator::server::OperationalStatus>;
using HealthIntf = object_t<State::Decorator::server::Health>;

template <typename IntfType>
class NsmChassis : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmChassis() = delete;
    NsmChassis(const std::string& name) :
        NsmInterfaceProvider<IntfType>(name, "NSM_Chassis",
                                       chassisInventoryBasePath)
    {}
    NsmChassis(const std::string& name, const std::shared_ptr<IntfType>& pdi) :
        NsmInterfaceProvider<IntfType>(name, "NSM_Chassis", {pdi})
    {}
    virtual requester::Coroutine update(SensorManager& manager,
                                        eid_t eid) override;
};

} // namespace nsm
