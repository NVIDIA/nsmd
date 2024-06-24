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

#include "utils.hpp"

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

} // namespace utils
