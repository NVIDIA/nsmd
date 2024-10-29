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

#include "device-configuration.h"
#include "diagnostics.h"

#include "asyncOperationManager.hpp"
#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Software/Settings/server.hpp>

namespace nsm
{
using SettingsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Settings>;

class NsmSetWriteProtected : public NsmInterfaceProvider<SettingsIntf>
{
  private:
    SensorManager& manager;
    const diagnostics_enable_disable_wp_data_index dataIndex;

    requester::Coroutine setWriteProtected(bool value,
                                           AsyncOperationStatusType& status,
                                           std::shared_ptr<NsmDevice> device);

  public:
    NsmSetWriteProtected() = delete;

    /**
     * @brief Construct a new Nsm Set Write Protected object
     *
     * @param name Name of the object
     * @param manager Sensor manager object
     * @param dataIndex Index for diagnostics enable/disable write protection
     * data
     * @param objPath DBus object path
     */
    NsmSetWriteProtected(
        const std::string& name, SensorManager& manager,
        const diagnostics_enable_disable_wp_data_index dataIndex,
        const std::string objPath);

    requester::Coroutine writeProtected(const AsyncSetOperationValueType& value,
                                        AsyncOperationStatusType* status,
                                        std::shared_ptr<NsmDevice> device);

    /**
     * @brief Get the Write Protected value from
     * nsm_fpga_diagnostics_settings_wp structure depending on the data index
     *
     * @param data Data structure to get value from
     * @param dataIndex Data index to identify the specific value
     * @return bool WriteProtect value from data structure
     */
    static bool
        getValue(const nsm_fpga_diagnostics_settings_wp& data,
                 const diagnostics_enable_disable_wp_data_index dataIndex);
};

} // namespace nsm
