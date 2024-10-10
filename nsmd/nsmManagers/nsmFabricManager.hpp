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
#include "network-ports.h"

#include "common/types.hpp"
#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <com/nvidia/State/FabricManager/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Inventory/Item/ManagementService/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <unordered_map>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using ManagementServiceIntf =
    object_t<Inventory::Item::server::ManagementService>;
using OperaStatusIntf = sdbusplus::xyz::openbmc_project::State::Decorator::
    server::OperationalStatus;
using FabricManagerIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::state::FabricManager>;
using OpState = sdbusplus::xyz::openbmc_project::State::Decorator::server::
    OperationalStatus::StateType;
using FMState =
    sdbusplus::common::com::nvidia::state::FabricManager::FabricManagerState;
using FMReportStatus = sdbusplus::common::com::nvidia::state::FabricManager::
    FabricManagerReportStatus;

class NsmFabricManagerState : public NsmObject
{
  public:
    NsmFabricManagerState(const std::string& name, const std::string& type,
                          std::string& inventoryObjPath,
                          std::shared_ptr<FabricManagerIntf> fabricMgrIntf,
                          std::shared_ptr<OperaStatusIntf> opStateIntf,
                          std::shared_ptr<ManagementServiceIntf> mgmtSerIntf);
    NsmFabricManagerState() = default;

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::shared_ptr<FabricManagerIntf> fabricManagerIntf = nullptr;
    std::shared_ptr<OperaStatusIntf> operationalStatusIntf = nullptr;
    std::shared_ptr<ManagementServiceIntf> managementServiceIntf = nullptr;
    std::string objPath;
};

} // namespace nsm
