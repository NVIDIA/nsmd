/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION &
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

#include "nsmAsioSensorTypeInterface.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

namespace
{
constexpr const char* kSensorTypeInterface = "xyz.openbmc_project.Sensor.Type";
constexpr const char* kImplementationProperty = "Implementation";
constexpr const char* kReadingBasisProperty = "ReadingBasis";
constexpr const char* kImplementationTypePrefix =
    "xyz.openbmc_project.Sensor.Type.ImplementationType.";
constexpr const char* kReadingBasisTypePrefix =
    "xyz.openbmc_project.Sensor.Type.ReadingBasisType.";

bool hasValue(const std::string* p)
{
    return p != nullptr && !p->empty();
}
} // namespace

std::unique_ptr<NsmAsioSensorTypeInterface> NsmAsioSensorTypeInterface::create(
    sdbusplus::asio::object_server& objServer, const std::string& objectPath,
    const std::string* implementation, const std::string* readingBasis)
{
    // Both null: no Sensor.Type interface added; matches the pre-merge
    // state for hardware that supplied neither property.
    if (!hasValue(implementation) && !hasValue(readingBasis))
    {
        return nullptr;
    }

    auto intf = std::make_unique<NsmAsioSensorTypeInterface>(
        objServer, objectPath, implementation, readingBasis);

    if (!intf->initialize())
    {
        lg2::error("Failed to initialize Sensor.Type interface: path={PATH}",
                   "PATH", objectPath);
        return nullptr;
    }

    return intf;
}

NsmAsioSensorTypeInterface::NsmAsioSensorTypeInterface(
    sdbusplus::asio::object_server& objServer, const std::string& objectPath,
    const std::string* implementation, const std::string* readingBasis) :
    NsmAsioInterfaceBase(objServer, objectPath, kSensorTypeInterface)
{
    if (!dbusInterface)
    {
        return;
    }

    // Selective registration: each property is registered only if the
    // caller provided a value. Single-property paths supply only one.
    if (hasValue(implementation))
    {
        dbusInterface->register_property(
            kImplementationProperty,
            std::string(kImplementationTypePrefix) + *implementation);
    }

    if (hasValue(readingBasis))
    {
        dbusInterface->register_property(kReadingBasisProperty,
                                         std::string(kReadingBasisTypePrefix) +
                                             *readingBasis);
    }
}

bool NsmAsioSensorTypeInterface::initialize()
{
    if (!dbusInterface)
    {
        lg2::error("Cannot initialize Sensor.Type interface: null: path={PATH}",
                   "PATH", objectPath);
        return false;
    }
    return dbusInterface->initialize();
}

} // namespace nsm
