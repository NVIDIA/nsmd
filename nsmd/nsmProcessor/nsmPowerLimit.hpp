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

using PowerLimitsIntf = object_t<Control::Power::server::Cap>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
using DecoratorAreaIntf = object_t<Inventory::Decorator::server::Area>;
using ClearPowerLimitIntf =
    object_t<sdbusplus::com::nvidia::Common::server::ClearPowerCap>;
using ClearPowerLimitAsyncIntf =
    object_t<sdbusplus::com::nvidia::Common::server::ClearPowerCapAsync>;

class NsmClearPowerLimitIntf : public ClearPowerLimitIntf
{
  public:
    NsmClearPowerLimitIntf(sdbusplus::bus::bus& bus,
                           const std::string& inventoryObjPath);
    int32_t clearPowerCap() override;
};
class NsmPowerLimit : public NsmSensor, ClearPowerLimitAsyncIntf
{
  public:
    NsmPowerLimit(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::string& type,
        std::shared_ptr<PowerLimitsIntf> powerLimitsIntf,
        std::shared_ptr<NsmClearPowerLimitIntf> clearPowerLimitIntf,
        std::shared_ptr<AssociationDefinitionsIntf> associationDefinitionsIntf,
        std::shared_ptr<NsmDevice> nsmDevice, uint8_t powerLimitId,
        const std::string& path);

    /**
     * @brief AsyncSetOperationHandler for setting the power limit
     * limit
     * @param value[in] Pointer to the target power limit value in Watts
     * @param status[in] Pointer to the async call action result
     * @param device[in] Pointer to the target nsm device
     */
    requester::Coroutine
        setPowerLimit(const AsyncSetOperationValueType& value,
                      AsyncOperationStatusType* status,
                      [[maybe_unused]] std::shared_ptr<NsmDevice> device);
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void handleOfflineState() override;
    sdbusplus::message::object_path clearPowerCap() override;

  private:
    requester::Coroutine
        doClearPowerLimit(std::shared_ptr<AsyncStatusIntf> statusInterface);

    /**
     * @brief set power limit
     * @param status[in] Pointer to the async call action result
     * @param action[in] specifies the operation to perform on the power limit.
     *                   0 = new power limit, 1 = reset to default
     * @param value[in] target power limit in Watts
     */
    requester::Coroutine updatePowerLimit(AsyncOperationStatusType* status,
                                          std::shared_ptr<NsmDevice> nsmDevice,
                                          const uint8_t action,
                                          const uint32_t value);

    std::shared_ptr<PowerLimitsIntf> powerLimitsIntf;
    std::shared_ptr<NsmClearPowerLimitIntf> clearPowerLimitIntf;
    std::shared_ptr<AssociationDefinitionsIntf> associationDefinitionsIntf;
    std::shared_ptr<NsmDevice> nsmDevice;
    uint8_t powerLimitId;
};

class NsmPowerLimitRange : public NsmObject
{
  public:
    NsmPowerLimitRange(const std::string& name, const std::string& type,
                       uint8_t propertyId,
                       std::shared_ptr<PowerLimitsIntf> powerLimitsIntf);
    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

  private:
    std::string propertyName;
    uint8_t propertyId;
    std::shared_ptr<PowerLimitsIntf> powerLimitsIntf = nullptr;
};

class NsmDefaultPowerLimit : public NsmObject
{
  public:
    NsmDefaultPowerLimit(
        const std::string& name, const std::string& type,
        const uint8_t propertyId,
        std::shared_ptr<NsmClearPowerLimitIntf> clearPowerLimitIntf);
    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

  private:
    uint8_t propertyId;
    std::shared_ptr<NsmClearPowerLimitIntf> clearPowerLimitIntf;
    std::string propertyName;
};

void createGPUPowerLimit(std::shared_ptr<NsmDevice> nsmDevice,
                         sdbusplus::bus::bus& bus, const std::string& name,
                         const std::string type,
                         const std::string& inventoryObjPath);
} // namespace nsm
