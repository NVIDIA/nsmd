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

#include "device-configuration.h"
#include "platform-environmental.h"

#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "nsmSensor.hpp"

#include <sdbusplus/asio/object_server.hpp>

#include <functional>
#include <unordered_map>

namespace nsm
{

enum class SoCDeviceType
{
    GPU,
    MCU
};

using SoCMemberUpdater = std::function<void(const uint8_t*, uint16_t)>;

using SoCModeUpdater =
    std::function<void(std::shared_ptr<sdbusplus::asio::dbus_interface>,
                       const uint8_t*, uint16_t)>;

using SoCPropertyRegistrar =
    std::function<void(std::shared_ptr<sdbusplus::asio::dbus_interface>)>;

struct SoCModePropertyInfo
{
    std::string propertyName;
    SoCPropertyRegistrar registrar;
    SoCModeUpdater updater;
};

using SoCModePropertyMap =
    std::unordered_map<uint32_t, std::vector<SoCModePropertyInfo>>;

const SoCModePropertyMap& getSoCModePropertyMap(SoCDeviceType deviceType,
                                                uint32_t version = 1);

class NsmSoCPowerSmoothing : public NsmSensor
{
  public:
    NsmSoCPowerSmoothing(
        const std::string& name, const std::string& type,
        const std::string& inventoryObjPath,
        std::shared_ptr<sdbusplus::asio::dbus_interface> socIntf,
        uint32_t deviceModeIndex, SoCMemberUpdater updater);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    requester::Coroutine setSoCSetting(const AsyncSetOperationValueType& value,
                                       AsyncOperationStatusType* status,
                                       std::shared_ptr<NsmDevice> nsmDevice);

  private:
    std::string inventoryObjPath;
    std::shared_ptr<sdbusplus::asio::dbus_interface> socIntf;
    uint32_t deviceModeIndex;
    SoCMemberUpdater updater;
};

void createSoCPowerSmoothing(std::shared_ptr<NsmDevice> nsmDevice,
                             sdbusplus::bus_t& bus, const std::string& name,
                             const std::string& type,
                             const std::string& inventoryObjPath,
                             SoCDeviceType deviceType, uint32_t version = 1,
                             bool priority = false);

} // namespace nsm
