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

#include "nsmInterface.hpp"
#include "nsmSensor.hpp"
#include "sensorManager.hpp"

#include <com/nvidia/NVLink/NVLinkLED/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LedType/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>

namespace nsm
{
using namespace sdbusplus::com::nvidia::NVLink;
using namespace sdbusplus::server;

using AssociationDefIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::association::Definitions>;
using LedTypeIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::LedType>;
using LocationIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Location>;

using NvlinkLedIntf = object_t<server::NVLinkLED>;
class NsmNvlinkLedIntf : public NsmObject
{
  public:
    NsmNvlinkLedIntf(sdbusplus::bus_t& bus, std::string& name,
                     std::string& type, std::string& inventoryObjPath);

    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

  private:
    std::unique_ptr<NvlinkLedIntf> nvlinkledIntf = nullptr;
    std::string inventoryObjPath;
};

} // namespace nsm
