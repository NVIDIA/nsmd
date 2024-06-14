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

#include "nsmDevice.hpp"
#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Software/Settings/server.hpp>

#include <functional>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using SettingsIntf = object_t<Software::server::Settings>;

class NsmWriteProtectedIntf : public SettingsIntf
{
  private:
    SensorManager& manager;
    std::shared_ptr<NsmDevice> device;
    uint8_t instanceNumber;
    NsmDeviceIdentification deviceType;
    bool retimer;

    bool writeProtected(bool value) override;
    bool setWriteProtected(bool value);
    bool getWriteProtected() const;

  public:
    NsmWriteProtectedIntf() = delete;
    NsmWriteProtectedIntf(SensorManager& manager,
                          std::shared_ptr<NsmDevice> device,
                          uint8_t instanceNumber,
                          NsmDeviceIdentification deviceType, const char* path,
                          bool retimer = false);

    /**
     * @brief Get the Write Protected value from
     * nsm_fpga_diagnostics_settings_wp structure depending on device type and
     * instance number
     *
     * @param data Data structure to get value from
     * @param deviceType Device type
     * @param instanceNumber Device instance number
     * @param retimer Retimer flag for Baseboard device
     * @return bool WriteProtect value from data structure
     */
    static bool getValue(const nsm_fpga_diagnostics_settings_wp& data,
                         NsmDeviceIdentification deviceType,
                         uint8_t instanceNumber, bool retimer);

    /**
     * @brief Get the Write Protected data index depending on device type and
     * instance number
     *
     * @param deviceType Device type
     * @param instanceNumber Device instance number
     * @param retimer Retimer flag for Baseboard device
     * @return int dataIndex Data index for device type and instance number
     */
    static int getDataIndex(NsmDeviceIdentification deviceType,
                            uint8_t instanceNumber, bool retimer = false);
};

} // namespace nsm
