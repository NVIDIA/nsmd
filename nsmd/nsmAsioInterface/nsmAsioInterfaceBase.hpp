/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include <sdbusplus/asio/object_server.hpp>

#include <memory>
#include <string>

namespace nsm
{

/**
 * @class NsmAsioInterfaceBase
 * @brief Base class for ASIO-based D-Bus interface wrappers
 *
 * Construction contract for every derived class:
 *   - Provide a public static createXxx(...) factory. It must
 *     construct the object via std::make_shared / std::make_unique
 *     and then call initialize(); return nullptr if initialize fails.
 *   - The derived-class constructor is public only to allow
 *     std::make_shared / std::make_unique to reach it. Direct
 *     construction is unsupported and will leave the interface
 *     unpublished on D-Bus.
 *   - initialize() is protected and pure-virtual here; each derived
 *     class implements it to call dbusInterface->initialize().
 *
 * Provides common functionality for D-Bus interface lifecycle management
 * using the sdbusplus/asio pattern (selective property registration).
 *
 * Key benefits over server binding pattern:
 * - Selective property registration: Only add properties that are needed
 * - Automatic lifecycle management: Interface is created/destroyed with object
 * - Clean separation: Isolates asio pattern complexity from sensor logic
 *
 * This base class enables future extensibility for additional interfaces
 * (e.g., Availability, OperationalStatus) while maintaining consistent
 * initialization patterns.
 */
class NsmAsioInterfaceBase
{
  public:
    virtual ~NsmAsioInterfaceBase() = default;

    /**
     * @brief Get the underlying D-Bus interface
     * @return Shared pointer to the dbus_interface object
     */
    std::shared_ptr<sdbusplus::asio::dbus_interface> getInterface() const
    {
        return dbusInterface;
    }

    /**
     * @brief Get the D-Bus object path
     * @return Object path string
     */
    const std::string& getObjectPath() const
    {
        return objectPath;
    }

    /**
     * @brief Get the D-Bus interface name
     * @return Interface name string
     */
    const std::string& getInterfaceName() const
    {
        return interfaceName;
    }

  protected:
    NsmAsioInterfaceBase(sdbusplus::asio::object_server& objServer,
                         const std::string& objectPath,
                         const std::string& interfaceName);

    /**
     * @brief Initialize the interface (make it visible on D-Bus)
     * Called by derived class static create() factory methods.
     * @return true if successful, false otherwise
     */
    virtual bool initialize() = 0;

    sdbusplus::asio::object_server& objServer;
    std::string objectPath;
    std::string interfaceName;
    std::shared_ptr<sdbusplus::asio::dbus_interface> dbusInterface;
};

} // namespace nsm
