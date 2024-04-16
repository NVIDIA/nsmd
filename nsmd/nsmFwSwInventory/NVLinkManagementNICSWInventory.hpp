/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <iostream>

namespace nsm
{
typedef uint8_t enum8;
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using OperationalStatusIntf =
    object_t<State::Decorator::server::OperationalStatus>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using SoftwareIntf = object_t<Software::server::Version>;

class NsmSWInventoryDriverVersionAndStatus : public NsmSensor
{
  public:
    NsmSWInventoryDriverVersionAndStatus(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::vector<utils::Association>& associations,
        const std::string& type, const std::string& manufacturer);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateValue(enum8 driverState, std::string driverVersion);
    std::unique_ptr<SoftwareIntf> softwareVer_ = nullptr;
    std::unique_ptr<OperationalStatusIntf> operationalStatus_ = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDef_ = nullptr;
    std::unique_ptr<AssetIntf> asset_ = nullptr;
    std::string assoc;

    // to be consumed by unit tests
    enum8 driverState_ = 0;
    std::string driverVersion_ = "";
};
} // namespace nsm