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

#include <com/nvidia/Common/ClearPowerCap/server.hpp>
#include <com/nvidia/Common/ClearPowerCapAsync/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Control/Power/Cap/server.hpp>
#include <xyz/openbmc_project/Control/Power/Mode/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Area/server.hpp>

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
using PowerCapIntf = object_t<Control::Power::server::Cap>;
using PowerModeIntf = object_t<Control::Power::server::Mode>;
using Mode = sdbusplus::xyz::openbmc_project::Control::Power::server::Mode;
using DecoratorAreaIntf = object_t<Inventory::Decorator::server::Area>;
using ClearPowerCapIntf =
    object_t<sdbusplus::com::nvidia::Common::server::ClearPowerCap>;
using ClearPowerCapAsyncIntf =
    object_t<sdbusplus::com::nvidia::Common::server::ClearPowerCapAsync>;

class NsmChassisClearPowerCapAsyncIntf : ClearPowerCapAsyncIntf
{
  public:
    using ClearPowerCapAsyncIntf::ClearPowerCapAsyncIntf;

  private:
    sdbusplus::message::object_path clearPowerCap() override;
};

class NsmPowerControl :
    public NsmObject,
    public PowerCapIntf,
    public ClearPowerCapIntf
{
  public:
    NsmPowerControl(sdbusplus::bus::bus &bus, const std::string &name,
		    const std::vector<utils::Association> &associations,
		    std::string &type, const std::string &path, const std::string& physicalContext);
    virtual uint32_t minPowerCapValue() const override;
    virtual uint32_t maxPowerCapValue() const override;
    virtual uint32_t defaultPowerCap() const override;
    int32_t clearPowerCap() override;
    virtual uint32_t powerCap(uint32_t value) override;
    void updatePowerCapValue(const std::string &childName, uint32_t value);

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsInft =
        nullptr;
    std::unique_ptr<PowerModeIntf> powerModeIntf = nullptr;
    std::unique_ptr<DecoratorAreaIntf> decoratorAreaIntf = nullptr;
    std::unique_ptr<NsmChassisClearPowerCapAsyncIntf> clearPowerCapAsyncIntf =
        nullptr;
    std::map<std::string, double> powerCapChildValues;
};
} // namespace nsm