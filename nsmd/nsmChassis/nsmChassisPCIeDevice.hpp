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

#include <com/nvidia/NVLink/NVLinkRefClock/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PCIeRefClock/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>
#include <xyz/openbmc_project/PCIe/LTSSMState/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::com::nvidia;
using namespace sdbusplus::server;
using UuidIntf = object_t<Common::server::UUID>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using NVLinkRefClockIntf = object_t<NVLink::server::NVLinkRefClock>;
using PCIeRefClockIntf = object_t<Inventory::Decorator::server::PCIeRefClock>;
using AssociationDefinitionsIntf = object_t<Association::server::Definitions>;
using OperationalStatusIntf =
    object_t<State::Decorator::server::OperationalStatus>;
using HealthIntf = object_t<State::Decorator::server::Health>;
using PCIeDeviceIntf = object_t<Inventory::Item::server::PCIeDevice>;
using LTSSMStateIntf = object_t<PCIe::server::LTSSMState>;

template <typename IntfType>
class NsmChassisPCIeDevice : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmChassisPCIeDevice() = delete;
    NsmChassisPCIeDevice(const std::string& chassisName,
                         const std::string& name) :
        NsmInterfaceProvider<IntfType>(name, "NSM_ChassisPCIeDevice",
                                       chassisInventoryBasePath / chassisName /
                                           "PCIeDevices")
    {}
    NsmChassisPCIeDevice(const std::string& name, dbus::Interfaces inventoryPaths) :
        NsmInterfaceProvider<IntfType>(name, "NSM_ChassisPCIeDevice",
                                       inventoryPaths)
    {}
    virtual requester::Coroutine update(SensorManager& manager,
                                        eid_t eid) override;
};

} // namespace nsm
