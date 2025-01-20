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
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <unordered_map>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using ItemIntf = object_t<Inventory::server::Item>;
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

class NsmAggregateFabricManagerState
{
  private:
    NsmAggregateFabricManagerState(const std::string& inventoryObjPath,
                                   const std::string& description);

    static std::vector<std::shared_ptr<FabricManagerIntf>>
        associatedFabricManagerIntfs;
    static std::vector<std::shared_ptr<OperaStatusIntf>>
        associatedOperationalStatusIntfs;
    static std::string aggregateFMObjPath;
    std::shared_ptr<FabricManagerIntf> fabricManagerIntf;
    std::shared_ptr<OperaStatusIntf> operationalStatusIntf;
    std::shared_ptr<ManagementServiceIntf> managementServiceIntf = nullptr;
    std::shared_ptr<ItemIntf> itemIntf = nullptr;

    static std::shared_ptr<NsmAggregateFabricManagerState>
        nsmAggregateFabricManagerState;

  public:
    static std::shared_ptr<NsmAggregateFabricManagerState> getInstance(
        const std::string& inventoryObjPath,
        std::shared_ptr<FabricManagerIntf> associatedFabricManagerIntf,
        std::shared_ptr<OperaStatusIntf> associatedOperationalStatusIntf,
        const std::string& description);

    void updateAggregateFabricManagerState();
};

class NsmFabricManagerState : public NsmSensor
{
  public:
    NsmFabricManagerState(const std::string& name, const std::string& type,
                          std::string& inventoryObjPath,
                          std::string& inventoryObjPathFM,
                          sdbusplus::bus_t& bus, std::string& description);
    NsmFabricManagerState() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    std::shared_ptr<FabricManagerIntf> getFabricManagerIntf();
    std::shared_ptr<OperaStatusIntf> getOperaStatusIntf();
    std::shared_ptr<NsmAggregateFabricManagerState>
        getAggregateFabricManagerState();

  private:
    std::shared_ptr<FabricManagerIntf> fabricManagerIntf = nullptr;
    std::shared_ptr<OperaStatusIntf> operationalStatusIntf = nullptr;
    std::shared_ptr<NsmAggregateFabricManagerState>
        nsmAggregateFabricManagerState;
    std::string objPath;
};

} // namespace nsm
