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

dbus::ObjectValueTree& MockDbusAsync::dbus()
{
    static dbus::ObjectValueTree map{};
    return map;
}

MapperServiceMap& MockDbusAsync::serviceMap()
{
    static MapperServiceMap map{};
    return map;
}

dbus::PropertyMap& MockDbusAsync::propertyMap(const dbus::ObjectPath& objPath,
                                              const dbus::Interface& interface)
{
    auto& dbusMap = dbus();
    auto& propertyMap = dbusMap[objPath][interface];
    propertyMap.clear();
    return propertyMap;
}

const dbus::PropertyMap*
    MockDbusAsync::findPropertyMap(const std::string& objPath,
                                   const std::string& interface)
{
    auto& values = MockDbusAsync::dbus();
    auto objectIt = values.find(objPath);
    if (objectIt == values.end())
    {
        return nullptr;
    }
    auto interfaceIt = objectIt->second.find(interface);
    if (interfaceIt == objectIt->second.end())
    {
        return nullptr;
    }
    return &(interfaceIt->second);
}

const dbus::Value* MockDbusAsync::findProperty(const std::string& objPath,
                                               const std::string& interface,
                                               const std::string& property)
{
    auto propertyMap = findPropertyMap(objPath, interface);
    if (propertyMap == nullptr)
    {
        return nullptr;
    }

    auto it = propertyMap->find(property);
    if (it == propertyMap->end())
    {
        return nullptr;
    }
    return &(it->second);
}

/** @brief Convert stored variant to PropertyValue with proper type conversions
 *
 * Handles special cases like object_path vectors and unsigned char vectors
 * that need conversion to match PropertyValue variant types.
 *
 * @param[in] value - The variant value to convert
 * @return PropertyValue with properly converted type
 */
PropertyValue convertToPropertyValue(const dbus::Value& value)
{
    return std::visit([](auto&& arg) -> PropertyValue {
        using T = std::decay_t<decltype(arg)>;
        // Special case: convert std::vector<object_path> to
        // std::vector<string>
        if constexpr (std::is_same_v<
                          T, std::vector<sdbusplus::message::object_path>>)
        {
            std::vector<std::string> result;
            result.reserve(arg.size());
            for (const auto& path : arg)
            {
                result.push_back(static_cast<std::string>(path));
            }
            return PropertyValue{std::move(result)};
        }
        else if constexpr (std::is_same_v<T, std::vector<unsigned char>>)
        {
            // Fallback to string if vector<unsigned char> is found (not
            // covered by PropertyValue)
            return PropertyValue{std::string(arg.begin(), arg.end())};
        }
        else
        {
            return PropertyValue(arg);
        }
    }, value);
}

std::string MockDBusHandler::getService(const char* /*path*/,
                                        const char* /*interface*/) const
{
    // Return a default service name for mock testing
    // In real scenarios, this would query object-mapper
    return "xyz.openbmc_project.NSM";
}

MapperServiceMap
    MockDBusHandler::getServiceMap(const char* /*path*/,
                                   const dbus::Interfaces& /*ifaceList*/) const
{
    // Return the mock service map that tests have configured
    return MockDbusAsync::serviceMap();
}

PropertyValue MockDBusHandler::getDbusPropertyVariant(
    const char* objPath, const char* dbusProp, const char* dbusInterface) const
{
    auto value = MockDbusAsync::findProperty(objPath, dbusInterface, dbusProp);
    if (value == nullptr)
    {
        throw std::runtime_error(
            std::string("Property not found in mock data: ") + dbusProp +
            " for interface: " + dbusInterface + " and path: " + objPath);
    }

    return convertToPropertyValue(*value);
}

PropertyValuesCollection
    MockDBusHandler::getDbusProperties(const char* objPath,
                                       const char* dbusInterface) const
{
    auto propertyMap = MockDbusAsync::findPropertyMap(objPath, dbusInterface);
    if (propertyMap == nullptr)
    {
        throw std::runtime_error(
            std::string("Interface not found in mock data: ") + dbusInterface +
            " for path: " + objPath);
    }

    // Return all properties for this interface
    PropertyValuesCollection result;
    for (const auto& [propName, propValue] : *propertyMap)
    {
        result.emplace_back(propName, convertToPropertyValue(propValue));
    }
    return result;
}

// Single-flight pattern implementation for single-threaded async execution
// for EM configuration PDI properties
requester::Coroutine
    coGetCachedBaseProperties(const std::string& objPath,
                              const std::string& baseInterface,
                              dbus::PropertyMap& cachedProperties)
{
    // In test mode, just copy the current propertyMap
    auto propertyMap = MockDbusAsync::findPropertyMap(objPath, baseInterface);
    if (propertyMap == nullptr)
    {
        lg2::error("Property map not found in mock data: {OBJPATH}:{INTERFACE}",
                   "OBJPATH", objPath, "INTERFACE", baseInterface);
        co_return NSM_SW_ERROR;
    }
    cachedProperties = *propertyMap;
    co_return NSM_SUCCESS;
}

bool coGetDbusPropertyBase::setRetValueByProperty() noexcept
{
    auto value = MockDbusAsync::findProperty(objectPath, interface, property);
    if (value == nullptr)
    {
        lg2::error(
            "Property not found in mock data: {PROPERTY} for interface: {INTERFACE} and path: {OBJECT_PATH}",
            "PROPERTY", property, "INTERFACE", interface, "OBJECT_PATH",
            objectPath);
        return false;
    }

    setRetValue(convertToPropertyValue(*value));
    return true;
}

bool coGetDbusPropertyBase::await_ready() noexcept
{
    return setRetValueByProperty();
}

bool coGetDbusPropertyBase::await_suspend(
    std::coroutine_handle<> /*handle*/) noexcept
{
    return true;
}

bool coGetServiceMap::await_ready() noexcept
{
    auto& value = utils::MockDbusAsync::serviceMap();
    static_cast<MapperServiceMap&>(*this) = value;

    return true;
}

bool coGetServiceMap::await_suspend(std::coroutine_handle<> /*handle*/) noexcept
{
    return true;
}

bool coGetAllDbusProperty::await_ready() noexcept
{
    auto propertyMap = MockDbusAsync::findPropertyMap(objectPath, interface);
    if (propertyMap == nullptr)
    {
        lg2::error(
            "Property map not found in mock data: {OBJECT_PATH}:{INTERFACE}",
            "OBJECT_PATH", objectPath, "INTERFACE", interface);
        return false;
    }
    static_cast<dbus::PropertyMap&>(*this) = *propertyMap;

    return true;
}

bool coGetAllDbusProperty::await_suspend(
    std::coroutine_handle<> /*handle*/) noexcept
{
    return true;
}

bool coLogEvent::await_ready() noexcept
{
    success = true; // Default to success for tests
    return true;
}

bool coLogEvent::await_suspend(std::coroutine_handle<> /*handle*/) noexcept
{
    return true;
}

coGetServiceMap::coGetServiceMap(const std::string& objectPath,
                                 const dbus::Interfaces& ifaceList) :
    MapperServiceMap(utils::MockDbusAsync::serviceMap()),
    objectPath(objectPath), ifaceList(ifaceList)
{}
coGetAllDbusProperty::coGetAllDbusProperty(const std::string& service,
                                           const std::string& objectPath,
                                           const std::string& interface) :
    dbus::PropertyMap([&] {
        auto it = MockDbusAsync::findPropertyMap(objectPath, interface);
        return it ? *it : dbus::PropertyMap{};
    }()),
    service(service), objectPath(objectPath), interface(interface)
{}

} // namespace utils
