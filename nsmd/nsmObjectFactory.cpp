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

#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmObjectFactory& NsmObjectFactory::instance()
{
    static NsmObjectFactory instance;
    return instance;
}

requester::Coroutine NsmObjectFactory::createObjects(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath)
{
    auto it = creationFunctions.find(interface);
    if (it != creationFunctions.end())
    {
        try
        {
            co_await it->second(manager, interface, objPath);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "NsmObjectFactory::createObjects error: '{ERROR}' for interface: {INTF} and path: {PATH}",
                "ERROR", e, "INTF", interface, "PATH", objPath);
        }
    }
    co_return NSM_SUCCESS;
}

void NsmObjectFactory::registerCreationFunction(const CreationFunction& func,
                                                const std::string interfaceName)
{
    lg2::info("register nsmObject: interfaceName={NAME}", "NAME",
              interfaceName);
    creationFunctions[interfaceName] = func;
}

void NsmObjectFactory::registerCreationFunction(
    const CreationFunction& func, const std::vector<std::string>& interfaces)
{
    for (const auto& interface : interfaces)
    {
        registerCreationFunction(func, interface);
    }
}

} // namespace nsm
