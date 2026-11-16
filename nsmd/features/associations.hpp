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

#include "../../common/globals.hpp"
#include "../../common/utils.hpp"
#include "features/feature.hpp"

#include <optional>
#include <string>
#include <vector>

/** @file Association params of the device lists. Each link pair has fixed
 *  forward/backward names in its creator and one list param of targets
 *  relative to a base path (inventorySystemBasePath unless stated); an empty
 *  entry gives no link.
 */

namespace nsm::features
{

/** @brief Absolute target for an entry relative to base, nullopt if empty */
inline std::optional<std::string>
    associationTarget(const std::string& relative,
                      const path& base = inventorySystemBasePath)
{
    if (relative.empty())
    {
        return std::nullopt;
    }
    return utils::makeDBusNameValid((base / relative).string());
}

/** @brief One forward/backward link per non-empty entry of the list param key
 */
inline void addAssociations(std::vector<utils::Association>& associations,
                            const dbus::PropertyMap& params, const char* key,
                            const char* forward, const char* backward,
                            const path& base = inventorySystemBasePath)
{
    for (const auto& relative : arrayParam<std::string>(params, key))
    {
        if (auto target = associationTarget(relative, base))
        {
            associations.push_back({forward, backward, *target});
        }
    }
}

/** @brief One forward/backward link for the scalar param key, for a target
 *  that comes from a device variable through optVar()
 */
inline void addAssociation(std::vector<utils::Association>& associations,
                           const dbus::PropertyMap& params, const char* key,
                           const char* forward, const char* backward,
                           const path& base = inventorySystemBasePath)
{
    if (auto target = associationTarget(
            param<std::string>(params, key).value_or(""), base))
    {
        associations.push_back({forward, backward, *target});
    }
}

} // namespace nsm::features
