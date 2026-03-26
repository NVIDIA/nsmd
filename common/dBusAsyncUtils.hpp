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

struct coGetDbusPropertyBase
{
    const std::string service;
    const std::string objectPath;
    const std::string interface;
    const std::string property;

    bool setRetValueByProperty() noexcept;
    virtual void setRetValue(const PropertyValue& value) = 0;
    virtual void resetRetValue() = 0;

    /** @brief Returning false to make await_suspend() to be called.
     */
    bool await_ready() noexcept;

    /** @brief Called by co_await operator before suspending coroutine. The
     * method will send out NSM request message, register a call back function
     * for the event when D-Bus method done.
     */
    bool await_suspend(std::coroutine_handle<> handle) noexcept;

    /** @brief Constructor of awaitable object to initialize necessary member
     * variables.
     */
    coGetDbusPropertyBase(const std::string& objectPath,
                          const std::string& property,
                          const std::string& interface,
                          const std::string& service = entityManagerService) :
        service(service), objectPath(objectPath), interface(interface),
        property(property)
    {}
};

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

template <typename type, typename = void>
struct coGetDbusProperty : coGetDbusPropertyBase, type
{
    void setRetValue(const PropertyValue& value) override final
    {
        static_cast<type&>(*this) = std::get<type>(value);
    }

    void resetRetValue() override final
    {
        static_cast<type&>(*this) = type();
    }

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    type await_resume() const noexcept
    {
        return static_cast<type>(*this);
    }

    coGetDbusProperty(const std::string& objectPath,
                      const std::string& property, const std::string& interface,
                      const std::string& service = entityManagerService) :
        coGetDbusPropertyBase(objectPath, property, interface, service), type()
    {
#ifdef COVERAGE_DISABLE_COROUTINES
        setRetValueByProperty();
#endif // COVERAGE_DISABLE_COROUTINES
    }
};

template <typename type>
struct coGetDbusProperty<type, std::enable_if_t<std::is_arithmetic_v<type> ||
                                                std::is_enum_v<type>>> :
    coGetDbusPropertyBase
{
    /** @brief For keeping the return value.
     */
    type ret;

    void setRetValue(const PropertyValue& value) override final
    {
        ret = std::get<type>(value);
    }

    void resetRetValue() override final
    {
        ret = type();
    }

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    type await_resume() const noexcept
    {
        return ret;
    }

    coGetDbusProperty(const std::string& objectPath,
                      const std::string& property, const std::string& interface,
                      const std::string& service = entityManagerService) :
        coGetDbusPropertyBase(objectPath, property, interface, service), ret()
    {
#ifdef COVERAGE_DISABLE_COROUTINES
        setRetValueByProperty();
#endif // COVERAGE_DISABLE_COROUTINES
    }

#ifdef COVERAGE_DISABLE_COROUTINES
    operator type() const noexcept
    {
        return static_cast<type>(ret);
    }
#endif // COVERAGE_DISABLE_COROUTINES
};

/** @struct coGetServiceMap
 *
 * An awaitable object needed by co_await operator to get service map which has
 * the interfaces e.g. auto mapperResponse = co_await
 * utils::coGetServiceMap(objPath, dbus::Interfaces{});
 *
 * @tparam type - property data type
 */
struct coGetServiceMap : MapperServiceMap
{
    const std::string objectPath;
    const dbus::Interfaces ifaceList;

    /** @brief Returning false to make await_suspend() to be called.
     */
    bool await_ready() noexcept;

    /** @brief Called by co_await operator before suspending coroutine. The
     * method will send out NSM request message, register a call back function
     * for the event when D-Bus method done.
     */
    bool await_suspend(std::coroutine_handle<> handle) noexcept;

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    MapperServiceMap await_resume() const noexcept
    {
        return static_cast<MapperServiceMap>(*this);
    }

    /** @brief Constructor of awaitable object to initialize necessary member
     * variables.
     */
    coGetServiceMap(const std::string& objectPath,
                    const dbus::Interfaces& ifaceList);
};

struct coGetAllDbusProperty : dbus::PropertyMap
{
    const std::string service;
    const std::string objectPath;
    const std::string interface;

    /** @brief Returning false to make await_suspend() to be called.
     */
    bool await_ready() noexcept;

    /** @brief Called by co_await operator before suspending coroutine. The
     * method will send out NSM request message, register a call back function
     * for the event when D-Bus method done.
     */
    bool await_suspend(std::coroutine_handle<> handle) noexcept;

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    dbus::PropertyMap await_resume() const noexcept
    {
        return static_cast<dbus::PropertyMap>(*this);
    }

    /** @brief Constructor of awaitable object to initialize necessary member
     * variables.
     */
    coGetAllDbusProperty(const std::string& service,
                         const std::string& objectPath,
                         const std::string& interface = "");
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
    bool await_ready() noexcept;

    /** @brief Called by co_await operator before suspending coroutine.
     */
    bool await_suspend(std::coroutine_handle<> handle) noexcept;

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

#ifdef COVERAGE_DISABLE_COROUTINES

    coLogEvent(bool value) :
        service(""), messageId(""), level(Level::Informational), data{},
        success(value)
    {}
    operator bool() const noexcept
    {
        return success;
    }
#endif // COVERAGE_DISABLE_COROUTINES
};

} // namespace utils
