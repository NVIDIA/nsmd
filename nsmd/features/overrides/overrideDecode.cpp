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

#include "overrideDecode.hpp"

#include <cstdlib>

namespace nsm::features
{

void decodeFeatureOverrides(const std::string& feature,
                            const FeatureOverrides& values,
                            const std::string& source,
                            std::vector<Override>& out)
{
    for (const auto& [key, value] : values)
    {
        if (key != enabledKey)
        {
            out.push_back({Override::Action::Set, feature, key, value});
            continue;
        }
        const auto* enabled = std::get_if<bool>(&value);
        if (!enabled)
        {
            lg2::error("{SOURCE}: {FEATURE}.Enabled must be a bool, ignored",
                       "SOURCE", source, "FEATURE", feature);
#ifdef NSM_STRICT_OVERRIDES
            std::abort();
#endif
            continue;
        }
        out.push_back(
            {*enabled ? Override::Action::Enable : Override::Action::Disable,
             feature,
             {},
             {}});
    }
}

std::vector<Override> decodeFeatureOverrides(const FeaturesMap& features,
                                             const std::string& source)
{
    std::vector<Override> out;
    for (const auto& [feature, property] : features)
    {
        decodeFeatureOverrides(feature, std::get<FeatureOverrides>(property),
                               source, out);
    }
    return out;
}

} // namespace nsm::features
