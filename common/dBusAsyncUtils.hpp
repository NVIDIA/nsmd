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

#include "coroutine.hpp"
#include "utils.hpp"

#include <queue>

namespace utils
{

constexpr auto entityManagerService = "xyz.openbmc_project.EntityManager";

#ifndef MOCK_DBUS_ASYNC_UTILS
/** @struct coGetDbusProperty
 *
 * An awaitable object needed by co_await operator to get D-Bus Property value
 * e.g.
 * rc = co_await
 * coGetDbusProperty<std::string>("/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_1/HGX_GPU_SXM_1",
 * "UUID", "xyz.openbmc_project.Configuration.NSM_Chassis",
 * "xyz.openbmc_project.EntityManager");
 *
 * @tparam type - property data type
 */
template <typename type>
struct coGetDbusProperty
{
    const std::string service;
    const std::string& objectPath;
    const std::string& interface;
    const std::string& property;

    /** @brief For keeping the return value.
     */
    type ret;

    /** @brief Returning false to make await_suspend() to be called.
     */
    bool await_ready() noexcept
    {
        return false;
    }

    /** @brief Called by co_await operator before suspending coroutine. The
     * method will send out NSM request message, register a call back function
     * for the event when D-Bus method done.
     */
    bool await_suspend(std::coroutine_handle<> handle) noexcept
    {
        auto& asioConnection = utils::DBusHandler::getAsioConnection();

        asioConnection->async_method_call(
            [resumeHandle = handle, &ret = ret,
             this](boost::system::error_code ec, PropertyValue value) {
            if (ec)
            {
                lg2::error(
                    "error while DbusProperties.Get for intf={INTERFACE}, prop={PROPERTY} and path={OBJECT_PATH}. {ERROR_MESSAGE} ",
                    "INTERFACE", interface, "PROPERTY", property, "OBJECT_PATH",
                    objectPath, "ERROR_MESSAGE", ec.message());
                ret = type();
            }
            else
            {
                ret = std::get<type>(value);
            }
            resumeHandle();
        },
            service.c_str(), objectPath.c_str(),
            "org.freedesktop.DBus.Properties", "Get", interface.c_str(),
            property.c_str());

        return true;
    }

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    type await_resume() const noexcept
    {
        return ret;
    }

    /** @brief Constructor of awaitable object to initialize necessary member
     * variables.
     */
    coGetDbusProperty(const std::string& objectPath,
                      const std::string& property, const std::string& interface,
                      const std::string service = entityManagerService) :
        service(service),
        objectPath(objectPath), interface(interface), property(property), ret{}
    {}
};

/** @struct coGetServiceMap
 *
 * An awaitable object needed by co_await operator to get service map which has
 * the interfaces e.g. auto mapperResponse = co_await
 * utils::coGetServiceMap(objPath, dbus::Interfaces{});
 *
 * @tparam type - property data type
 */
struct coGetServiceMap
{
    const std::string& objectPath;
    const dbus::Interfaces& ifaceList;

    /** @brief For keeping the return value.
     */
    MapperServiceMap ret;

    /** @brief Returning false to make await_suspend() to be called.
     */
    bool await_ready() noexcept
    {
        return false;
    }

    /** @brief Called by co_await operator before suspending coroutine. The
     * method will send out NSM request message, register a call back function
     * for the event when D-Bus method done.
     */
    bool await_suspend(std::coroutine_handle<> handle) noexcept
    {
        auto& asioConnection = utils::DBusHandler::getAsioConnection();

        asioConnection->async_method_call(
            [resumeHandle = handle, &ret = ret,
             this](boost::system::error_code ec, MapperServiceMap value) {
            if (ec)
            {
                lg2::error(
                    "error while xyz.openbmc_project.ObjectMapperGetObject for intf={INTERFACE} and path={OBJECT_PATH}. {ERROR_MESSAGE} ",
                    "OBJECT_PATH", objectPath, "ERROR_MESSAGE", ec.message());
            }
            else
            {
                ret = value;
            }
            resumeHandle();
        },
            mapperService, mapperPath, mapperInterface, "GetObject",
            objectPath.c_str(), ifaceList);

        return true;
    }

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    MapperServiceMap await_resume() const noexcept
    {
        return ret;
    }

    /** @brief Constructor of awaitable object to initialize necessary member
     * variables.
     */
    coGetServiceMap(const std::string& objectPath,
                    const dbus::Interfaces& ifaceList) :
        objectPath(objectPath),
        ifaceList(ifaceList)
    {}
};

#else

struct MockDbusAsync
{
    /** @brief Get the values reference for gtest. */
    static auto& getValues()
    {
        static std::queue<PropertyValue> values{};
        return values;
    }

    static auto& getServiceMap()
    {
        static MapperServiceMap map{};
        return map;
    }
};

template <typename type>
struct coGetDbusProperty
{
    const std::string service;
    const std::string& objectPath;
    const std::string& interface;
    const std::string& property;

    type ret;

    bool await_ready() noexcept
    {
        auto& values = utils::MockDbusAsync::getValues();
        if (values.empty())
        {
            ret = type();
        }
        else
        {
            ret = std::get<type>(values.front());
            values.pop();
        }

        return true;
    }

    bool await_suspend([[maybe_unused]] std::coroutine_handle<> handle) noexcept
    {
        return true;
    }

    type await_resume() const noexcept
    {
        return ret;
    }

    coGetDbusProperty(const std::string& objectPath,
                      const std::string& property, const std::string& interface,
                      const std::string service = entityManagerService) :
        service(service),
        objectPath(objectPath), interface(interface), property(property), ret{}
    {}
};

struct coGetServiceMap
{
    const std::string& objectPath;
    const dbus::Interfaces& ifaceList;

    MapperServiceMap ret;

    bool await_ready() noexcept
    {
        auto& value = utils::MockDbusAsync::getServiceMap();
        ret = value;

        return true;
    }

    bool await_suspend([[maybe_unused]] std::coroutine_handle<> handle) noexcept
    {
        return true;
    }

    MapperServiceMap await_resume() const noexcept
    {
        return ret;
    }

    coGetServiceMap(const std::string& objectPath,
                    const dbus::Interfaces& ifaceList) :
        objectPath(objectPath),
        ifaceList(ifaceList)
    {}
};

#endif

} // namespace utils
