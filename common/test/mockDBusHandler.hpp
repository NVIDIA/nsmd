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

#include "types.hpp"
#include "utils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace utils
{

struct MockDbusAsync
{
    /** @brief Get the values reference for gtest. */
    static dbus::ObjectValueTree& dbus();

    /** @brief Get the service map reference for gtest. */
    static MapperServiceMap& serviceMap();

    /** @brief Creates empty property map reference for gtest for a given object
     * path.
     *
     * @param objPath - The D-Bus object path
     * @param interface - The D-Bus interface
     *
     * @return The property map reference for the given object path
     */
    static dbus::PropertyMap& propertyMap(const dbus::ObjectPath& objPath,
                                          const dbus::Interface& interface);

    /** @brief Find a property map in the mock DBus data structure
     *
     * @param[in] objPath - The D-Bus object path
     * @param[in] interface - The D-Bus interface
     * @return Pointer to the property map if found, nullptr otherwise
     */
    static const dbus::PropertyMap*
        findPropertyMap(const std::string& objPath,
                        const std::string& interface);

    /** @brief Find a property value in the mock DBus data structure
     *
     * @param[in] objPath - The D-Bus object path
     * @param[in] interface - The D-Bus interface
     * @param[in] property - The property name
     * @return Pointer to the property value if found, nullptr otherwise
     */
    static const dbus::Value* findProperty(const std::string& objPath,
                                           const std::string& interface,
                                           const std::string& property);
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

    std::string getService(const char* path,
                           const char* interface) const override;

    MapperServiceMap
        getServiceMap(const char* path,
                      const dbus::Interfaces& ifaceList) const override;
    MOCK_METHOD(GetSubTreeResponse, getSubtree,
                (const std::string& path, int depth,
                 const dbus::Interfaces& ifaceList),
                (const, override));

    MOCK_METHOD(void, setDbusProperty,
                (const DBusMapping& dBusMap, const PropertyValue& value),
                (const, override));
    PropertyValue
        getDbusPropertyVariant(const char* objPath, const char* dbusProp,
                               const char* dbusInterface) const override;

    PropertyValuesCollection
        getDbusProperties(const char* objPath,
                          const char* dbusInterface) const override;

    MOCK_METHOD(GetAssociatedObjectsResponse, getAssociatedObjects,
                (const std::string& path, const std::string& association),
                (const, override));
};

class DBusTest
{
  protected:
    MockDBusHandler& mockDBus = MockDBusHandler::instance();

    virtual ~DBusTest()
    {
        MockDbusAsync::dbus().clear();
        MockDbusAsync::serviceMap().clear();
    }
};

} // namespace utils
