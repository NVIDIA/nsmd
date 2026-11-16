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

#include "common/coroutine.hpp"
#include "common/globals.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmDebugTokenAggregation.hpp"
#include "nsmDebugTokenFeatures.hpp"
#include "nsmDebugTokenNIC.hpp"
#include "nsmDebugTokenUnified.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#ifdef ENABLE_DEBUG_TOKEN
namespace nsm::features
{

requester::Coroutine createDebugTokenFeature(SensorManager& manager,
                                             const DeviceContext& dev,
                                             const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("DebugToken feature of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }

    DebugTokenAggregationManager::getInstance();

    auto& bus = utils::DBusHandler::getBus();

    std::string chassisName =
        param<std::string>(params, "ChassisName").value_or("");
    auto debugTokenType = arrayParam<std::string>(params, "DebugTokenType");
    const uuid_t& uuid = dev.staticUuid;
    std::string debugTokenDeviceType =
        param<std::string>(params, "DebugTokenDeviceType").value_or("");

    auto device = dev.device;
    if (std::find(debugTokenType.begin(), debugTokenType.end(), "NIC") !=
        debugTokenType.end())
    {
        auto object = std::make_shared<NsmDebugTokenNICObject>(bus, chassisName,
                                                               uuid);
        device->addStaticSensor(object);
    }
    if (std::find(debugTokenType.begin(), debugTokenType.end(), "Unified") !=
        debugTokenType.end())
    {
        auto object = std::make_shared<NsmDebugTokenUnifiedObject>(
            bus, chassisName, uuid, debugTokenDeviceType);
        device->addSensor(object, PollingType::RoundRobin);
        manager.debugTokenList.push_back(object);
    }

    co_return NSM_SUCCESS;
}

} // namespace nsm::features
#endif
