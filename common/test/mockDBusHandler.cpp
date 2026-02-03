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

#include "mockDBusHandler.hpp"

#include "dBusAsyncUtils.hpp"

namespace utils
{
IDBusHandler& DBusHandler()
{
    return MockDBusHandler::instance();
}

const PropertyValuesCollection::value_type
    DBusTest::get(const PropertyValuesCollection& properties,
                  const DbusProp& name)
{
    auto it =
        std::find_if(properties.begin(), properties.end(),
                     [&name](const auto& pair) { return pair.first == name; });
    if (it == properties.end())
        throw std::out_of_range("Property " + name +
                                " not found in collection");
    return *it;
}

// Single-flight pattern implementation for single-threaded async execution
// for EM configuration PDI properties
requester::Coroutine
    coGetCachedBaseProperties(const std::string& /*objPath*/,
                              const std::string& /*baseInterface*/,
                              dbus::PropertyMap& cachedProperties)
{
    // In test mode, just copy the current propertyMap
    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    cachedProperties = propertyMap;
    co_return NSM_SUCCESS;
}

bool coGetDbusPropertyBase::await_ready() noexcept
{
    auto& values = MockDbusAsync::getValues();
    auto it = values[objectPath].find(property);
    if (it == values[objectPath].end())
    {
        lg2::error(
            "error while DbusProperties.Get for intf={INTERFACE}, prop={PROPERTY} and path={OBJECT_PATH}.",
            "INTERFACE", interface, "PROPERTY", property, "OBJECT_PATH",
            objectPath);
        return false;
    }

    setRetValue(it->second);
    return true;
}

bool coGetDbusPropertyBase::await_suspend(
    std::coroutine_handle<> /*handle*/) noexcept
{
    return true;
}

bool coGetServiceMap::await_ready() noexcept
{
    auto& value = utils::MockDbusAsync::getServiceMap();
    ret = value;

    return true;
}

bool coGetServiceMap::await_suspend(std::coroutine_handle<> /*handle*/) noexcept
{
    return true;
}

bool coGetAllDbusProperty::await_ready() noexcept
{
    auto& value = utils::MockDbusAsync::getPropertyMap();
    ret = value;

    return true;
}

bool coGetAllDbusProperty::await_suspend(
    std::coroutine_handle<> /*handle*/) noexcept
{
    return true;
}

bool coLogEvent::await_ready() noexcept
{
    success = utils::MockDbusAsync::getLogEventSuccess();
    return true;
}

bool coLogEvent::await_suspend(std::coroutine_handle<> /*handle*/) noexcept
{
    return true;
}

} // namespace utils
