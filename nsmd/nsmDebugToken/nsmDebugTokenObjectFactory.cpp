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
#include "common/types.hpp"
#include "common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmDebugTokenNIC.hpp"
#include "nsmDebugTokenUnified.hpp"
#include "nsmObjectFactory.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace nsm
{

static requester::Coroutine createNsmDebugToken(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap allBaseIfaceProperties;
    auto rc = co_await utils::coGetCachedBaseProperties(objPath, interface,
                                                        allBaseIfaceProperties);
    if (rc != NSM_SUCCESS)
    {
        co_return rc;
    }
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string chassisName{};
    if (allBaseIfaceProperties.count("ChassisName"))
    {
        chassisName =
            std::get<std::string>(allBaseIfaceProperties.at("ChassisName"));
    }
    std::vector<std::string> debugTokenType{};
    if (allCurrentIfaceProperties.count("DebugTokenType"))
    {
        debugTokenType = std::get<std::vector<std::string>>(
            allCurrentIfaceProperties.at("DebugTokenType"));
    }
    uuid_t uuid{};
    if (allBaseIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
    }

    auto device = manager.getNsmDeviceFromStaticUUID(uuid);
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
            bus, chassisName, uuid);
        device->addStaticSensor(object);
    }

    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmDebugToken, "xyz.openbmc_project.Configuration.NSM_DebugToken")

} // namespace nsm
