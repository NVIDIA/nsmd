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

#include "dBusAsyncUtils.hpp"
#include "utils.hpp"

#include <boost/asio.hpp>

namespace utils
{

std::string DBusHandler::getService(const char* path,
                                    const char* interface) const
{
    return getServiceMap(path, {interface}).begin()->first;
}

MapperServiceMap
    DBusHandler::getServiceMap(const char* path,
                               const dbus::Interfaces& ifaceList) const
{
    MapperServiceMap mapperResponse;
    auto& bus = DBusHandler::getBus();

    auto mapper = bus.new_method_call(mapperService, mapperPath,
                                      mapperInterface, "GetObject");
    mapper.append(path, ifaceList);

    auto mapperResponseMsg = bus.call(mapper);
    mapperResponseMsg.read(mapperResponse);
    return mapperResponse;
}

GetSubTreeResponse
    DBusHandler::getSubtree(const std::string& searchPath, int depth,
                            const dbus::Interfaces& ifaceList) const
{
    auto& bus = utils::DBusHandler::getBus();
    auto method = bus.new_method_call(mapperService, mapperPath,
                                      mapperInterface, "GetSubTree");
    method.append(searchPath, depth, ifaceList);
    auto reply = bus.call(method);
    GetSubTreeResponse response;
    reply.read(response);
    return response;
}

void DBusHandler::setDbusProperty(const DBusMapping& dBusMap,
                                  const PropertyValue& value) const
{
    auto setDbusValue = [&dBusMap, this](const auto& variant) {
        auto& bus = getBus();
        auto service = getService(dBusMap.objectPath.c_str(),
                                  dBusMap.interface.c_str());
        auto method = bus.new_method_call(
            service.c_str(), dBusMap.objectPath.c_str(), dbusProperties, "Set");
        method.append(dBusMap.interface.c_str(), dBusMap.propertyName.c_str(),
                      variant);
        bus.call_noreply(method);
    };

    if (dBusMap.propertyType == "uint8_t")
    {
        std::variant<uint8_t> v = std::get<uint8_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "bool")
    {
        std::variant<bool> v = std::get<bool>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "int16_t")
    {
        std::variant<int16_t> v = std::get<int16_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "uint16_t")
    {
        std::variant<uint16_t> v = std::get<uint16_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "int32_t")
    {
        std::variant<int32_t> v = std::get<int32_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "uint32_t")
    {
        std::variant<uint32_t> v = std::get<uint32_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "int64_t")
    {
        std::variant<int64_t> v = std::get<int64_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "uint64_t")
    {
        std::variant<uint64_t> v = std::get<uint64_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "double")
    {
        std::variant<double> v = std::get<double>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "string")
    {
        std::variant<std::string> v = std::get<std::string>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "array[object_path]")
    {
        std::variant<std::vector<sdbusplus::message::object_path>> v =
            std::get<std::vector<sdbusplus::message::object_path>>(value);
        setDbusValue(v);
    }
    else
    {
        throw std::invalid_argument("Unsupported Dbus Type");
    }
}

PropertyValue DBusHandler::getDbusPropertyVariant(
    const char* objPath, const char* dbusProp, const char* dbusInterface) const
{
    auto& bus = DBusHandler::getBus();
    auto service = getService(objPath, dbusInterface);
    auto method = bus.new_method_call(service.c_str(), objPath, dbusProperties,
                                      "Get");
    method.append(dbusInterface, dbusProp);
    PropertyValue value{};
    auto reply = bus.call(method);
    reply.read(value);
    return value;
}

PropertyValuesCollection
    DBusHandler::getDbusProperties(const char* objPath,
                                   const char* dbusInterface) const
{
    PropertyValuesCollection properties;
    auto& bus = DBusHandler::getBus();
    auto service = getService(objPath, dbusInterface);
    auto method = bus.new_method_call(service.c_str(), objPath, dbusProperties,
                                      "GetAll");
    method.append(dbusInterface);
    auto reply = bus.call(method);
    reply.read(properties);
    return properties;
}

IDBusHandler& DBusHandler()
{
    return DBusHandler::instance();
}

GetAssociatedObjectsResponse
    DBusHandler::getAssociatedObjects(const std::string& path,
                                      const std::string& association) const
{
    auto& bus = utils::DBusHandler::getBus();
    const auto associationPath = path + "/" + association;
    auto method = bus.new_method_call(mapperService, associationPath.c_str(),
                                      dbusProperties, "Get");
    method.append("xyz.openbmc_project.Association", "endpoints");
    auto reply = bus.call(method);
    GetAssociatedObjectsResponse response;
    reply.read(response);
    return response;
}

std::shared_ptr<sdbusplus::asio::connection>& DBusHandler::getAsioConnection()
{
    static boost::asio::io_context io;
    static auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    return conn;
}

// Single-flight pattern implementation for single-threaded async execution
// for EM configuration PDI properties
requester::Coroutine
    coGetCachedBaseProperties(const std::string& objPath,
                              const std::string& baseInterface,
                              dbus::PropertyMap& cachedProperties)
{
    static std::unordered_map<
        std::string, std::unordered_map<std::string, dbus::PropertyMap>>
        basePropertiesCache;

    static std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::shared_future<dbus::PropertyMap>>>
        pendingRequests;

    // Check if already cached
    auto objPathIt = basePropertiesCache.find(objPath);
    if (objPathIt != basePropertiesCache.end())
    {
        auto interfaceIt = objPathIt->second.find(baseInterface);
        if (interfaceIt != objPathIt->second.end())
        {
            // "Cache hit: Using cached base properties for
            // {OBJPATH}:{INTERFACE}", "OBJPATH", objPath, "INTERFACE",
            // baseInterface);
            cachedProperties = interfaceIt->second;
            co_return NSM_SUCCESS;
        }
    }

    // Check if request is already pending
    auto pendingObjIt = pendingRequests.find(objPath);
    if (pendingObjIt != pendingRequests.end())
    {
        auto pendingInterfaceIt = pendingObjIt->second.find(baseInterface);
        if (pendingInterfaceIt != pendingObjIt->second.end())
        {
            //     "Request pending: Waiting for ongoing request for
            //     {OBJPATH}:{INTERFACE}", "OBJPATH", objPath, "INTERFACE",
            //     baseInterface);

            try
            {
                dbus::PropertyMap result = pendingInterfaceIt->second.get();
                cachedProperties = result;
                co_return NSM_SUCCESS;
            }
            catch (...)
            {
                // If the future threw an exception, we'll try making our own
                // request Continue to the request creation logic below
            }
        }
    }

    // Create a promise for this request
    std::promise<dbus::PropertyMap> promise;
    std::shared_future<dbus::PropertyMap> future = promise.get_future().share();

    // Store the pending request
    pendingRequests[objPath][baseInterface] = future;

    // "Cache miss: First retrieval for {OBJPATH}:{INTERFACE}",
    // "OBJPATH", objPath, "INTERFACE", baseInterface);

    try
    {
        // Make the actual D-Bus call
        dbus::PropertyMap properties = co_await utils::coGetAllDbusProperty(
            utils::entityManagerServiceStr, objPath, baseInterface);

        // Cache the result and clean up pending request
        basePropertiesCache[objPath][baseInterface] = properties;

        // Remove from pending requests
        auto pendingObjIt = pendingRequests.find(objPath);
        if (pendingObjIt != pendingRequests.end())
        {
            pendingObjIt->second.erase(baseInterface);
            if (pendingObjIt->second.empty())
            {
                pendingRequests.erase(objPath);
            }
        }

        // Set the promise result
        promise.set_value(properties);

        cachedProperties = properties;
        co_return NSM_SUCCESS;
    }
    catch (const std::exception& e)
    {
        // Clean up pending request on error
        auto pendingObjIt = pendingRequests.find(objPath);
        if (pendingObjIt != pendingRequests.end())
        {
            pendingObjIt->second.erase(baseInterface);
            if (pendingObjIt->second.empty())
            {
                pendingRequests.erase(objPath);
            }
        }

        // Set the promise exception
        promise.set_exception(std::current_exception());

        lg2::error(
            "Failed to fetch base properties for {OBJPATH}:{INTERFACE}:{ERROR}",
            "OBJPATH", objPath, "INTERFACE", baseInterface, "ERROR", e.what());

        co_return NSM_SW_ERROR;
    }
}

bool coGetDbusPropertyBase::await_ready() noexcept
{
    return false;
}

bool coGetDbusPropertyBase::await_suspend(
    std::coroutine_handle<> handle) noexcept
{
    auto& asioConnection = utils::DBusHandler::getAsioConnection();

    asioConnection->async_method_call(
        [resumeHandle = handle, this](boost::system::error_code ec,
                                      const PropertyValue& value) {
        if (ec)
        {
            lg2::error(
                "error while DbusProperties.Get for intf={INTERFACE}, prop={PROPERTY} and path={OBJECT_PATH}. {ERROR_MESSAGE} ",
                "INTERFACE", interface, "PROPERTY", property, "OBJECT_PATH",
                objectPath, "ERROR_MESSAGE", ec.message());
            resetRetValue();
        }
        else
        {
            setRetValue(value);
        }
        resumeHandle();
    },
        service.c_str(), objectPath.c_str(), "org.freedesktop.DBus.Properties",
        "Get", interface.c_str(), property.c_str());

    return true;
}

bool coGetServiceMap::await_ready() noexcept
{
    return false;
}

bool coGetServiceMap::await_suspend(std::coroutine_handle<> handle) noexcept
{
    auto& asioConnection = utils::DBusHandler::getAsioConnection();

    asioConnection->async_method_call(
        [resumeHandle = handle, this](boost::system::error_code ec,
                                      const MapperServiceMap& value) {
        if (ec)
        {
            lg2::error(
                "error while xyz.openbmc_project.ObjectMapperGetObject for path={OBJECT_PATH}. {ERROR_MESSAGE} ",
                "OBJECT_PATH", objectPath, "ERROR_MESSAGE", ec.message());
        }
        else
        {
            static_cast<MapperServiceMap&>(*this) = value;
        }
        resumeHandle();
    },
        mapperService, mapperPath, mapperInterface, "GetObject",
        objectPath.c_str(), ifaceList);

    return true;
}

bool coGetAllDbusProperty::await_ready() noexcept
{
    return false;
}

bool coGetAllDbusProperty::await_suspend(
    std::coroutine_handle<> handle) noexcept
{
    auto& asioConnection = utils::DBusHandler::getAsioConnection();

    asioConnection->async_method_call(
        [resumeHandle = handle, this](boost::system::error_code ec,
                                      const dbus::PropertyMap& value) {
        if (ec)
        {
            lg2::error(
                "error while coGetAllDbusProperty.GetAll for service={SERVICE}, path={OBJECT_PATH}, interface={IFACE}. {ERROR_MESSAGE} ",
                "SERVICE", service, "OBJECT_PATH", objectPath, "IFACE",
                interface, "ERROR_MESSAGE", ec.message());
        }
        else
        {
            static_cast<dbus::PropertyMap&>(*this) = value;
        }
        resumeHandle();
    },
        service.c_str(), objectPath.c_str(), "org.freedesktop.DBus.Properties",
        "GetAll", interface);

    return true;
}

bool coLogEvent::await_ready() noexcept
{
    return false;
}

bool coLogEvent::await_suspend(std::coroutine_handle<> handle) noexcept
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
            lg2::error("coLogEvent failed: {ERROR}. MessageId={MSG}", "ERROR",
                       ec.message(), "MSG", messageId);
        }
        resumeHandle();
    },
        service.c_str(), "/xyz/openbmc_project/logging",
        "xyz.openbmc_project.Logging.Create", "Create", messageId, level, data);
    return true;
}

coGetServiceMap::coGetServiceMap(const std::string& objectPath,
                                 const dbus::Interfaces& ifaceList) :
    MapperServiceMap(), objectPath(objectPath), ifaceList(ifaceList)
{}
coGetAllDbusProperty::coGetAllDbusProperty(const std::string& service,
                                           const std::string& objectPath,
                                           const std::string& interface) :
    dbus::PropertyMap(), service(service), objectPath(objectPath),
    interface(interface)
{}

} // namespace utils
