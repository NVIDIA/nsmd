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
#include "config.h"

#include "asyncOperationManager.hpp"
#include "nsmObject.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <com/nvidia/Common/ClearPowerCap/server.hpp>
#include <com/nvidia/Common/ClearPowerCapAsync/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Control/Power/Cap/server.hpp>
#include <xyz/openbmc_project/Control/Power/Mode/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Area/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using PowerCapIntf = object_t<Control::Power::server::Cap>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
using PowerModeIntf = object_t<Control::Power::server::Mode>;
using Mode = sdbusplus::xyz::openbmc_project::Control::Power::server::Mode;
using DecoratorAreaIntf = object_t<Inventory::Decorator::server::Area>;
using ClearPowerCapIntf =
    object_t<sdbusplus::com::nvidia::Common::server::ClearPowerCap>;
using ClearPowerCapAsyncIntf =
    object_t<sdbusplus::com::nvidia::Common::server::ClearPowerCapAsync>;

class NsmClearPowerCapIntf : public ClearPowerCapIntf
{
  public:
    NsmClearPowerCapIntf(sdbusplus::bus::bus& bus,
                         const std::string& inventoryObjPath);
    int32_t clearPowerCap() override;
};
class NsmProcessorModulePowerControl : public NsmSensor, ClearPowerCapAsyncIntf
{
  public:
    NsmProcessorModulePowerControl(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::string& type, std::shared_ptr<PowerCapIntf> powerCapIntf,
        std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf,
        const std::string& path,
        const std::vector<std::tuple<std::string, std::string, std::string>>&
            associations_list);
    requester::Coroutine
        setModulePowerCap(const AsyncSetOperationValueType& value,
                          AsyncOperationStatusType* status,
                          [[maybe_unused]] std::shared_ptr<NsmDevice> device);
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    sdbusplus::message::object_path clearPowerCap() override;

  private:
    requester::Coroutine doClearPowerCapOnModule(
        std::shared_ptr<AsyncStatusIntf> statusInterface);
    requester::Coroutine
        updatePowerLimitOnModule(AsyncOperationStatusType* status,
                                 const uint8_t action, const uint32_t value);
    std::unique_ptr<AssociationDefinitionsIntf> associationDefinitionsIntf =
        nullptr;
    std::unique_ptr<PowerModeIntf> powerModeIntf = nullptr;
    std::shared_ptr<PowerCapIntf> powerCapIntf = nullptr;
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf;
    std::unique_ptr<DecoratorAreaIntf> decoratorAreaIntf = nullptr;
    bool patchPowerLimitInProgress = false;
    const std::string path;
};

class NsmModulePowerLimit : public NsmObject
{
  public:
    NsmModulePowerLimit(std::string& name, std::string& type,
                        uint8_t propertyId,
                        std::shared_ptr<PowerCapIntf> powerCapIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::string propertyName;
    uint8_t propertyId;
    std::shared_ptr<PowerCapIntf> powerCapIntf = nullptr;
};

class NsmDefaultModulePowerLimit : public NsmObject
{
  public:
    NsmDefaultModulePowerLimit(
        const std::string& name, const std::string& type,
        std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf;
};
} // namespace nsm
