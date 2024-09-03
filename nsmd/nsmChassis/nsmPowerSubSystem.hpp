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
#include "nsmObject.hpp"
#include "utils.hpp"

#include <stdint.h>

#include <com/nvidia/PowerSupply/PowerSupplyInfo/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PowerSupply/server.hpp>

#include <cstdint>
#include <iostream>
#include <limits>

namespace nsm
{
// using SensorUnit =
// sdbusplus::xyz::openbmc_project::Sensor::server::Value::Unit;
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using PowerSupplyInfoIntf =
    object_t<sdbusplus::com::nvidia::PowerSupply::server::PowerSupplyInfo>;
using PowerSupplyIntf = object_t<Inventory::Item::server::PowerSupply>;
class NsmPowerPowerSupply : public NsmObject
{
  public:
    NsmPowerPowerSupply(sdbusplus::bus::bus& bus, std::string& name,
                        const std::vector<utils::Association>& associations,
                        std::string& type, std::string& path,
                        std::string& powerSupplyType);

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsInft =
        nullptr;
    std::unique_ptr<PowerSupplyInfoIntf> powerSupplyInfoIntf = nullptr;
    std::unique_ptr<PowerSupplyIntf> powerSupplyIntf = nullptr;
};
} // namespace nsm
