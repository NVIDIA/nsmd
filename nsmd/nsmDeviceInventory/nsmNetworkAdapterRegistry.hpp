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

#include "common/registry.hpp"
#include "nsmNetworkAdapter.hpp"

#include <memory>
#include <string>

namespace nsm
{

/** @brief A network adapter nsmd created. */
struct NetworkAdapter
{
    std::shared_ptr<NsmDevice> device;

    // Name of the chassis its parent_chassis association points at.
    std::string chassis;
    std::string name;
    std::string path;

    // Null for an adapter without PCIe device mode.
    std::shared_ptr<PCIeDeviceModeIntf> deviceModeIntf;
    std::shared_ptr<NsmPCIeDeviceModeDeviceModeSettingsV2Get> getSensor;
    std::shared_ptr<NsmPCIeDeviceModeDeviceModeSettingsV2Set> setSensor;

    NetworkAdapterId id() const
    {
        return {chassis, name};
    }
};

/** @brief The network adapters nsmd created, by parent chassis and Name. */
using NetworkAdapterRegistry = common::Registry<NetworkAdapter>;

} // namespace nsm
