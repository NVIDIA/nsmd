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
#include "types.hpp"
#include "utils.hpp"

#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <queue>

namespace utils
{

constexpr auto entityManagerService = "xyz.openbmc_project.EntityManager";
const std::string entityManagerServiceStr = "xyz.openbmc_project.EntityManager";

// Forward declaration for logging level type
using Level = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;
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
    bool await_suspend(std::coroutine_handle<> handle)
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
                // can throw std::bad_variant_access
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
        service(service), objectPath(objectPath), interface(interface),
        property(property), ret{}
    {}
};

struct coGetAllDbusProperty
{
    const std::string service;
    const std::string objectPath;
    const std::string interface;

    /** @brief For keeping the return value.
     */
    dbus::PropertyMap ret;

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
    bool await_suspend(std::coroutine_handle<> handle)
    {
        auto& asioConnection = utils::DBusHandler::getAsioConnection();

        asioConnection->async_method_call(
            [resumeHandle = handle, &ret = ret,
             this](boost::system::error_code ec, dbus::PropertyMap value) {
            if (ec)
            {
                lg2::error(
                    "error while coGetAllDbusProperty.GetAll for service={SERVICE}, path={OBJECT_PATH}, interface={IFACE}. {ERROR_MESSAGE} ",
                    "SERVICE", service, "OBJECT_PATH", objectPath, "IFACE",
                    interface, "ERROR_MESSAGE", ec.message());
            }
            else
            {
                // can throw std::bad_variant_access
                ret = value;
            }
            resumeHandle();
        },
            service.c_str(), objectPath.c_str(),
            "org.freedesktop.DBus.Properties", "GetAll", interface);

        return true;
    }

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    dbus::PropertyMap await_resume() const noexcept
    {
        return ret;
    }

    /** @brief Constructor of awaitable object to initialize necessary member
     * variables.
     */
    coGetAllDbusProperty(const std::string& service,
                         const std::string& objectPath,
                         const std::string& interface = "") :
        service(service), objectPath(objectPath), interface(interface), ret{}
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
                    "error while xyz.openbmc_project.ObjectMapperGetObject for path={OBJECT_PATH}. {ERROR_MESSAGE} ",
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
        objectPath(objectPath), ifaceList(ifaceList)
    {}
};

/* @struct coLogEvent
 *
 * An awaitable object for async D-Bus logging operations
 * e.g.
 * bool success = co_await utils::coLogEvent(service, "MessageId", level, data);
 */
struct coLogEvent
{
    const std::string service;
    const std::string messageId;
    const Level level;
    const std::map<std::string, std::string> data;
    bool success = false;

    /** @brief Returning false to make await_suspend() to be called.
     */
    bool await_ready() noexcept
    {
        return false;
    }

    /** @brief Called by co_await operator before suspending coroutine.
     */
    bool await_suspend(std::coroutine_handle<> handle)
    {
        auto& asioConnection = utils::DBusHandler::getAsioConnection();
        auto severity =
            sdbusplus::xyz::openbmc_project::Logging::server::convertForMessage(
                level);

        asioConnection->async_method_call(
            [resumeHandle = handle, &success = success,
             messageId = messageId](boost::system::error_code ec) {
            success = !ec;
            if (ec)
            {
                lg2::error("coLogEvent failed: {ERROR}. MessageId={MSG}",
                           "ERROR", ec.message(), "MSG", messageId);
            }
            resumeHandle();
        },
            service.c_str(), "/xyz/openbmc_project/logging",
            "xyz.openbmc_project.Logging.Create", "Create", messageId, level,
            data);
        return true;
    }

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    bool await_resume() const noexcept
    {
        return success;
    }

    /** @brief Constructor of awaitable object to initialize necessary member
     * variables.
     */
    coLogEvent(const std::string& svc, const std::string& msgId, Level lvl,
               const std::map<std::string, std::string>& logData) :
        service(svc), messageId(msgId), level(lvl), data(logData)
    {}
};

#else

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

template <typename type>
struct coGetDbusProperty
{
    const std::string service;
    const std::string& objectPath;
    const std::string& interface;
    const std::string& property;

    type ret;

    bool await_ready()
    {
        auto& values = MockDbusAsync::getValues();
        auto it = values[objectPath].find(property);
        if (it == values[objectPath].end())
        {
            throw std::out_of_range(std::format(
                "error while DbusProperties.Get for intf={}, prop={} and path={}.",
                interface, property, objectPath));
        }
        else
        {
            // can throw std::bad_variant_access
            ret = std::get<type>(it->second);
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
        service(service), objectPath(objectPath), interface(interface),
        property(property), ret{}
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
        objectPath(objectPath), ifaceList(ifaceList)
    {}
};

struct coGetAllDbusProperty
{
    const std::string service;
    const std::string objectPath;
    const std::string interface;

    /** @brief For keeping the return value.
     */
    dbus::PropertyMap ret;

    /** @brief Returning false to make await_suspend() to be called.
     */
    bool await_ready() noexcept
    {
        auto& value = utils::MockDbusAsync::getPropertyMap();
        ret = value;
        return true;
    }

    /** @brief Called by co_await operator before suspending coroutine. The
     * method will send out NSM request message, register a call back function
     * for the event when D-Bus method done.
     */
    bool await_suspend([[maybe_unused]] std::coroutine_handle<> handle) noexcept
    {
        return true;
    }

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    dbus::PropertyMap await_resume() const noexcept
    {
        return ret;
    }

    /** @brief Constructor of awaitable object to initialize necessary member
     * variables.
     */
    coGetAllDbusProperty(const std::string& service,
                         const std::string& objectPath,
                         const std::string& interface = "") :
        service(service), objectPath(objectPath), interface(interface), ret{}
    {}
};

struct coLogEvent
{
    const std::string service;
    const std::string messageId;
    const Level level;
    const std::map<std::string, std::string> data;
    bool success = false;

    /** @brief Returning true to avoid suspension in mock mode.
     */
    bool await_ready() noexcept
    {
        success = utils::MockDbusAsync::getLogEventSuccess();
        return true;
    }

    /** @brief No-op for mock implementation.
     */
    bool await_suspend([[maybe_unused]] std::coroutine_handle<> handle) noexcept
    {
        return true;
    }

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    bool await_resume() const noexcept
    {
        return success;
    }

    /** @brief Constructor of awaitable object to initialize necessary member
     * variables.
     */
    coLogEvent(const std::string& svc, const std::string& msgId, Level lvl,
               const std::map<std::string, std::string>& logData) :
        service(svc), messageId(msgId), level(lvl), data(logData)
    {}
};

#endif

} // namespace utils
