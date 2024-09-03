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
#include "platform-environmental.h"

#include "asyncOperationManager.hpp"
#include "nsmObject.hpp"
#include "utils.hpp"

#include <stdint.h>

#include <com/nvidia/ClockMode/server.hpp>
#include <com/nvidia/Common/ClearClockLimAsync/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Area/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Cpu/OperatingConfig/server.hpp>

#include <cstdint>
#include <iostream>
#include <limits>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using ProcessorModeIntf =
    sdbusplus::server::object_t<sdbusplus::server::com::nvidia::ClockMode>;
using Mode = sdbusplus::server::com::nvidia::ClockMode::Mode;
using DecoratorAreaIntf = object_t<Inventory::Decorator::server::Area>;
using CpuOperatingConfigIntf =
    object_t<Inventory::Item::Cpu::server::OperatingConfig>;
using clearClockLimAsyncIntf =
    object_t<sdbusplus::server::com::nvidia::common::ClearClockLimAsync>;

class NsmClearClockLimAsyncIntf : public clearClockLimAsyncIntf
{
  public:
    NsmClearClockLimAsyncIntf(sdbusplus::bus::bus& bus, const char* path,
                              std::shared_ptr<NsmDevice> device);

    sdbusplus::message::object_path clearClockLimit() override;
    requester::Coroutine doClearClockLimitOnDevice(
        std::shared_ptr<AsyncStatusIntf> statusInterface);
    requester::Coroutine clearReqClockLimit(AsyncOperationStatusType* status);

  private:
    std::shared_ptr<NsmDevice> device;
};

class NsmChassisClockControl : public NsmSensor
{
  public:
    NsmChassisClockControl(
        sdbusplus::bus::bus& bus, const std::string& name,
        std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf,
        std::shared_ptr<NsmClearClockLimAsyncIntf> nsmClearClockLimAsyncIntf,
        const std::vector<utils::Association>& associations, std::string& type,
        const std::string& inventoryObjPath, const std::string& physicalContext,
        const std::string& clockMode);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

    requester::Coroutine
        setMinClockLimits(const AsyncSetOperationValueType& value,
                          [[maybe_unused]] AsyncOperationStatusType* status,
                          std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        setMaxClockLimits(const AsyncSetOperationValueType& value,
                          [[maybe_unused]] AsyncOperationStatusType* status,
                          std::shared_ptr<NsmDevice> device);

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsInft =
        nullptr;
    std::shared_ptr<ProcessorModeIntf> processorModeIntf = nullptr;
    std::shared_ptr<DecoratorAreaIntf> decoratorAreaIntf = nullptr;
    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf;
    std::shared_ptr<NsmClearClockLimAsyncIntf> nsmClearClockLimAsyncIntf;
    std::string inventoryObjPath;
};

} // namespace nsm
