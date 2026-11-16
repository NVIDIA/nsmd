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
#include <string_view>
#include <vector>

namespace nsm::features
{

/** @brief Built-in variable names; an NSM_Telemetry property with one of
 *  these names is not a device variable
 */
bool isBuiltinVariable(std::string_view name);

/** @brief Params as passed to the creator
 *
 *  A var() param takes its variable's value, converted to the type of the
 *  declared value. In every other string and string-array value, the
 *  "{...}" variables are replaced with their text: the built-ins {device},
 *  {instance}, {instance+1}, {parentChassis} (as EM gives it, e.g.
 *  "board/HGX_ProcessorModule_0"), {parentChassisName} (its last path
 *  element, e.g. "HGX_ProcessorModule_0"), then the device variables.
 *  Other "{...}" text, e.g. the per-event fmt fields of MessageArgs, is
 *  kept. Replaced text is not scanned again.
 *
 *  An optVar() param whose variable is missing or does not convert is left
 *  out and the reason added to dropped.
 *
 *  @param[out] error  why the params could not be resolved
 *  @param[out] dropped  why each left-out optVar() param was left out
 *  @return nullopt if a variable has no value, a var() value does not
 *          convert, or a non-text value is used in text
 */
std::optional<dbus::PropertyMap>
    resolveParams(const DeviceContext& dev, const ParamMap& params,
                  std::string& error, std::vector<std::string>& dropped);

} // namespace nsm::features
