/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

#include "features/feature.hpp"

#include <optional>
#include <string>
#include <vector>

namespace nsm::features
{

/** @brief Convert an override value to the type of a list value
 *
 *  Accepts the same type, an integer for a double, or an integer of another
 *  width when it fits the target type.
 *
 *  @param[in] target - list value whose type is kept
 *  @param[in] value - override value
 *  @return converted value, or std::nullopt when the types are incompatible
 */
std::optional<dbus::Value> normalize(const dbus::Value& target,
                                     const dbus::Value& value);

/** @brief Set one property of a feature
 *
 *  Only a key in params, or a key declared in optional, can be set. New keys
 *  are never added.
 *
 *  @param[in,out] feature - feature to update
 *  @param[in] key - property name
 *  @param[in] value - override value
 *  @param[out] error - reason when the override is rejected
 *  @return true when applied
 */
bool applyOverride(Feature& feature, const std::string& key,
                   const dbus::Value& value, std::string& error);

struct ApplyStats
{
    size_t applied = 0;
    size_t rejected = 0;
};

/** @brief Apply overrides to a device's feature list
 *
 *  Enable / Disable are applied first, then property overrides, each in
 *  record order. Rejected overrides are logged and skipped; with
 *  NSM_STRICT_OVERRIDES a rejection aborts nsmd.
 *
 *  @param[in,out] features - copy of the device list defaults
 *  @param[in] overrides - one override layer (NSM_Telemetry or NSM_Policy)
 *  @param[in] deviceName - EM Name, for logs
 */
ApplyStats applyOverrides(FeatureList& features,
                          const std::vector<Override>& overrides,
                          const std::string& deviceName);

/** @brief Printable form of a D-Bus value, for logs */
std::string valueToString(const dbus::Value& value);

} // namespace nsm::features
