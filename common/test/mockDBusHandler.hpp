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

#include "utils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace utils
{

struct MockDbusAsync
{
    struct DbusPropsMap :
        std::map<std::string, std::map<DbusProp, PropertyValue>>
    {
        void push(const std::string& objPath,
                  const std::pair<DbusProp, PropertyValue>& property)
        {
            (*this)[objPath][property.first] = property.second;
        }
    };

    /** @brief Get the values reference for gtest. */
    static auto& getValues()
    {
        static DbusPropsMap values{};
        return values;
    }

    static auto& getServiceMap()
    {
        static MapperServiceMap map{};
        return map;
    }

    static auto& getPropertyMap()
    {
        static dbus::PropertyMap propertyMap{};
        return propertyMap;
    }

    static auto& getLogEventSuccess()
    {
        static bool logEventSuccess = true; // Default to success for tests
        return logEventSuccess;
    }
};

class MockDBusHandler : public IDBusHandler
{
    MockDBusHandler() = default;

  public:
    static MockDBusHandler& instance()
    {
        static MockDBusHandler mockDBusHandler;
        return mockDBusHandler;
    }

    MOCK_METHOD(std::string, getService,
                (const char* path, const char* interface), (const, override));
    MOCK_METHOD(MapperServiceMap, getServiceMap,
                (const char* path, const dbus::Interfaces& ifaceList),
                (const, override));
    MOCK_METHOD(GetSubTreeResponse, getSubtree,
                (const std::string& path, int depth,
                 const dbus::Interfaces& ifaceList),
                (const, override));

    MOCK_METHOD(void, setDbusProperty,
                (const DBusMapping& dBusMap, const PropertyValue& value),
                (const, override));

    MOCK_METHOD(PropertyValue, getDbusPropertyVariant,
                (const char* objPath, const char* dbusProp,
                 const char* dbusInterface),
                (const, override));
    MOCK_METHOD(PropertyValuesCollection, getDbusProperties,
                (const char* objPath, const char* dbusInterface),
                (const, override));
    MOCK_METHOD(GetAssociatedObjectsResponse, getAssociatedObjects,
                (const std::string& path, const std::string& association),
                (const, override));
};

struct SdBusTestError : public sdbusplus::exception::exception
{
    int error = 0;
    SdBusTestError(int error) : error(error) {}
    const char* name() const noexcept override
    {
        return "";
    };
    const char* description() const noexcept override
    {
        return "";
    };
    int get_errno() const noexcept override
    {
        return error;
    };
};

class DBusTest
{
  protected:
    MockDBusHandler& mockDBus = MockDBusHandler::instance();

    static const PropertyValuesCollection::value_type
        get(const PropertyValuesCollection& properties, const DbusProp& name);
    template <typename T>
    static T get(const PropertyValuesCollection& properties,
                 const DbusProp& name)
    {
        return std::get<T>(get(properties, name).second);
    }
};

} // namespace utils
