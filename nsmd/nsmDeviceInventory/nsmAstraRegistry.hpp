// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES
/*
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

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace nsm
{

class NsmAstra;

/** @brief The Astra objects nsmd created, by the chassis each one is on. */
class AstraRegistry
{
  public:
    static AstraRegistry& instance();

    /**
     * @brief Add a group's Astra object unless the chassis already has one.
     *
     * makeAstra runs only if the chassis is free, so nothing is left claimed
     * if it throws.
     *
     * @return The group already owning the chassis, or nullopt once added.
     */
    std::optional<std::string> claimChassis(
        const std::string& chassis,
        const std::function<std::shared_ptr<NsmAstra>()>& makeAstra);

  private:
    AstraRegistry() = default;

    std::map<std::string, std::shared_ptr<NsmAstra>> astras;
};

} // namespace nsm
