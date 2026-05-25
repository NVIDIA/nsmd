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

#pragma once

#include "nsmAsioInterfaceBase.hpp"

#include <string>

namespace nsm
{

/**
 * @class NsmAsioSensorTypeInterface
 * @brief Boost-ASIO wrapper for xyz.openbmc_project.Sensor.Type with
 *        selective per-property registration.
 *
 * Background — consolidation of conditional D-Bus interfaces.
 * The upstream phosphor-dbus-interfaces sync consolidates the pre-merge
 * standalone interface xyz.openbmc_project.Sensor.ReadingBasis (carrying
 * the ReadingBasis property) into xyz.openbmc_project.Sensor.Type (which
 * pre-merge carried only Implementation). After the merge, Sensor.Type
 * has BOTH Implementation and ReadingBasis properties.
 *
 * Pre-merge nsmd published the two properties via TWO independently-
 * conditional typed bindings (object_t<Sensor::Type> guarded by
 * "implementation != nullptr"; object_t<Sensor::ReadingBasis> guarded by
 * "readingBasis != nullptr"). Those guards are semantic, not stylistic:
 * real hardware supplies neither / one / both, and the dbus_interface
 * itself never appeared on the bus for hardware that supplied neither.
 *
 * A naïve post-merge object_t<Sensor::Type> typed binding would publish
 * BOTH properties at their YAML defaults (Implementation=Unknown,
 * ReadingBasis=Unknown) on every sensor instance — including hardware
 * that supplied neither. That silently changes the observable bus
 * surface from "absent" to "Unknown", which a Redfish consumer cannot
 * distinguish from "actually unknown".
 *
 * Boost-ASIO with selective register_property is the only pattern that
 * preserves the per-hardware optionality. This class encapsulates that
 * pattern so every producer that publishes Sensor.Type (currently
 * NsmNumericSensorDbusValue and NsmNumericSensorComposite) uses the
 * same correct implementation.
 *
 * Design contract:
 *   - The static create() factory takes the two property values as
 *     nullable const std::string* pointers. It returns nullptr if BOTH
 *     are null: in that case no D-Bus interface is added at all (matches
 *     the pre-merge behaviour for hardware that supplied neither).
 *   - When at least one is non-null, the interface is added via
 *     add_interface(path, "xyz.openbmc_project.Sensor.Type"), each
 *     non-null property is register_property'd, and initialize() is
 *     called exactly once.
 *   - The enum-string values are passed as fully-qualified upstream
 *     enum strings (e.g. "xyz.openbmc_project.Sensor.Type.
 *     ImplementationType.<Value>"); callers supply the bare enum
 *     value (e.g. "Physical"), the factory prefixes it.
 */
class NsmAsioSensorTypeInterface : public NsmAsioInterfaceBase
{
  public:
    /**
     * @brief Static factory.
     *
     * @param objServer       sdbusplus asio object server (downstream
     *                        canonical: SensorManager::getInstance().
     *                        getObjServer()).
     * @param objectPath      D-Bus object path the sensor lives at.
     * @param implementation  Optional. When non-null and non-empty,
     *                        registers Implementation property with
     *                        the full-FQN enum-string conversion.
     * @param readingBasis    Optional. When non-null and non-empty,
     *                        registers ReadingBasis property with the
     *                        full-FQN enum-string conversion.
     *
     * @return  An owning unique_ptr when at least one property is
     *          supplied AND initialize() succeeds. Returns nullptr when
     *          BOTH properties are null/empty (intentional: matches the
     *          pre-merge "no interface added" state for hardware that
     *          supplied neither) or when initialize() fails.
     */
    static std::unique_ptr<NsmAsioSensorTypeInterface>
        create(sdbusplus::asio::object_server& objServer,
               const std::string& objectPath, const std::string* implementation,
               const std::string* readingBasis);

    NsmAsioSensorTypeInterface(sdbusplus::asio::object_server& objServer,
                               const std::string& objectPath,
                               const std::string* implementation,
                               const std::string* readingBasis);

  private:
    bool initialize() override;
};

} // namespace nsm
