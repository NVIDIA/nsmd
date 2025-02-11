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

#include "nsmSensor.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

using std::filesystem::path;

namespace nsm
{

template <typename IntfType>
using Interfaces = std::unordered_map<path, std::shared_ptr<IntfType>>;

/**
 * @brief Base class for NsmInterfaceContainer and NsmInterfaceProvider
 *
 * @tparam IntfType type of the PDI (i.e. Asset, Dimension ect.)
 */
template <typename IntfType>
class NsmInterfaces
{
  private:
    Interfaces<IntfType> interfaces;

  public:
    using IntfType_t = IntfType;
    NsmInterfaces() = delete;
    NsmInterfaces(const Interfaces<IntfType>& interfaces) :
        interfaces(interfaces)
    {
        if (interfaces.empty())
        {
            throw std::runtime_error(
                "NsmInterfaces::NsmInterfaces - interfaces cannot be empty");
        }
    }
    NsmInterfaces(const NsmInterfaces<IntfType>&& other) :
        interfaces(std::move(other.interfaces))
    {}

    /**
     * @brief Moves interfaces from other container to the current object
     *
     * @param container Container with interfaces to move
     * @return true if interfaces were moved, false otherwise
     */
    bool moveInterfaces(NsmInterfaces<IntfType>& container)
    {
        auto interfacesMoved = false;
        for (const auto& [path, intf] : container.interfaces)
        {
            // Add the interfaces to the existing sensor if path is not
            // found to avoid duplicated interfaces
            if (interfaces.find(path) == interfaces.end())
            {
                interfaces[path] = intf;
                interfacesMoved = true;
            }
        }
        container.interfaces.clear();
        return interfacesMoved;
    }

    /**
     * @brief Returns interface name
     *
     * @return std::string Interface name
     */
    static std::string interface()
    {
        return IntfType::interface;
    }

    /**
     * @brief Returns size of the interfaces collection
     *
     * @return size_t Size of the interfaces collection
     */
    size_t size()
    {
        return interfaces.size();
    }

    /**
     * @brief Invokes a method on all interfaces and returns a value if
     * requested (const version).
     *
     * @param func Method to invoke.
     * @return The return value of the method.
     */
    template <typename Func,
              typename T = typename std::invoke_result<Func, IntfType&>::type>
    T invoke(Func&& func)
        requires(!std::is_void_v<T>)
    {
        auto it = interfaces.begin();
        T value = func(*it->second);

        for (++it; it != interfaces.end(); ++it)
        {
            T otherValue = func(*it->second);
            if (otherValue != value)
            {
                lg2::error("Different values returned in {TYPE}", "TYPE",
                           utils::typeName<NsmInterfaces<IntfType>>());
                throw std::runtime_error("Different values returned");
            }
        }
        return value;
    }

    /**
     * @brief Invokes a method on all interfaces.
     *
     * @param func Method to invoke.
     * @param args Arguments to pass to the method.
     */
    template <typename Func, typename... Args>
    void invoke(Func&& func, Args&&... args)
    {
        for (auto& [path, pdi] : interfaces)
        {
            if constexpr (std::is_invocable_v<Func, std::string, IntfType&,
                                              Args...>)
            {
                func(path, *pdi, std::forward<Args>(args)...);
            }
            else
            {
                func(*pdi, std::forward<Args>(args)...);
            }
        }
    }
};

// Macro for invoking a method
#define pdiMethod(method, ...)                                                 \
    [](auto& pdi, auto&&... args) -> decltype(auto) {                          \
        return (pdi.method)(std::forward<decltype(args)>(args)...);            \
    }

/**
 * @brief Class which is creating and providing PDI object
 *
 * @tparam IntfType type of the PDI (i.e. Asset, Dimension ect.)
 */
template <typename IntfType>
class NsmInterfaceProvider : public NsmObject, public NsmInterfaces<IntfType>
{
  private:
    static auto createInterfaces(const dbus::Interfaces& objectsPaths)
    {
        Interfaces<IntfType> interfaces;
        for (const auto& path : objectsPaths)
        {
            interfaces.insert(std::make_pair(
                path, std::make_shared<IntfType>(utils::DBusHandler::getBus(),
                                                 path.c_str())));
        }
        return interfaces;
    }

  public:
    NsmInterfaceProvider() = delete;
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const dbus::Interfaces& objectsPaths) :
        NsmObject(name, type),
        NsmInterfaces<IntfType>(createInterfaces(objectsPaths))
    {}
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const path& basePath) :
        NsmObject(name, type),
        NsmInterfaces<IntfType>(createInterfaces({basePath / name}))
    {}
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const Interfaces<IntfType>& interfaces) :
        NsmObject(name, type),
        NsmInterfaces<IntfType>(interfaces)
    {}
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const path& path, std::shared_ptr<IntfType> pdi) :
        NsmObject(name, type),
        NsmInterfaces<IntfType>(Interfaces<IntfType>{{path, pdi}})
    {}
};

/**
 * @brief Class which is using PDI and stores shared PDIs objects collection
 *
 */
template <typename IntfType>
class NsmInterfaceContainer : public NsmInterfaces<IntfType>
{
  public:
    NsmInterfaceContainer() = delete;
    NsmInterfaceContainer(const NsmInterfaceProvider<IntfType>& provider) :
        NsmInterfaces<IntfType>(std::move(provider))
    {}
    NsmInterfaceContainer(const Interfaces<IntfType>& interfaces) :
        NsmInterfaces<IntfType>(interfaces)
    {}
    NsmInterfaceContainer(const std::shared_ptr<IntfType>& pdi) :
        NsmInterfaces<IntfType>(Interfaces<IntfType>{pdi})
    {}
};

} // namespace nsm
