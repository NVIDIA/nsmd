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

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Chassis/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using UuidIntf = object_t<Common::server::UUID>;
using LocationIntf = object_t<Inventory::Decorator::server::Location>;
using ChassisIntf = object_t<Inventory::Item::server::Chassis>;
using ItemIntf = object_t<Inventory::server::Item>;
using HealthIntf = object_t<State::Decorator::server::Health>;

template <typename IntfType>
class NsmNVSwitchAndNicChassis : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmNVSwitchAndNicChassis() = delete;
    NsmNVSwitchAndNicChassis(const std::string& name, const std::string& type) :
        NsmInterfaceProvider<IntfType>(name, type, chassisInventoryBasePath)
    {}

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::unique_ptr<sdbusplus::asio::dbus_interface> nsmDeviceAssociationIntf;
    std::string name;
};

} // namespace nsm
