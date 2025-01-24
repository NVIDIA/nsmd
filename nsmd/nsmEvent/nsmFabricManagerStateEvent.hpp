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

#include "nsmEvent.hpp"
#include "nsmManagers/nsmFabricManager.hpp"

#include <com/nvidia/State/FabricManager/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using FabricManagerIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::state::FabricManager>;
using OperaStatusIntf = sdbusplus::xyz::openbmc_project::State::Decorator::
    server::OperationalStatus;
using FMState =
    sdbusplus::common::com::nvidia::state::FabricManager::FabricManagerState;
using FMReportStatus = sdbusplus::common::com::nvidia::state::FabricManager::
    FabricManagerReportStatus;
using OpState = sdbusplus::xyz::openbmc_project::State::Decorator::server::
    OperationalStatus::StateType;

class NsmFabricManagerStateEvent : public NsmEvent
{
  public:
    NsmFabricManagerStateEvent(const std::string& name, const std::string& type,
                               std::shared_ptr<FabricManagerIntf> fabricMgrIntf,
                               std::shared_ptr<OperaStatusIntf> opStateIntf,
                               std::shared_ptr<NsmAggregateFabricManagerState>
                                   nsmAggregateFabricManagerState);

    int handle(eid_t eid, NsmType type, NsmEventId eventId,
               const nsm_msg* event, size_t eventLen) override;

  private:
    std::shared_ptr<FabricManagerIntf> fabricManagerIntf = nullptr;
    std::shared_ptr<OperaStatusIntf> operationalStatusIntf = nullptr;
    std::shared_ptr<NsmAggregateFabricManagerState>
        nsmAggregateFabricManagerState = nullptr;
};

} // namespace nsm
