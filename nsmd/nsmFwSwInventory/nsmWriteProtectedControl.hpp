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

#include "nsmSetWriteProtected.hpp"

#include <xyz/openbmc_project/Software/Settings/server.hpp>

namespace nsm
{
using SettingsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Settings>;
/**
 * @brief Sensor for updating Oem.Nvidia.HardwareWriteProtectedControl in
 * Chassis and WriteProtected in FirmwareInventory
 */
class NsmWriteProtectedControl :
    public NsmSensor,
    public NsmInterfaceContainer<SettingsIntf>
{
  private:
    const diagnostics_enable_disable_wp_data_index dataIndex;

  public:
    NsmWriteProtectedControl(
        const NsmInterfaceProvider<SettingsIntf>& provider,
        const diagnostics_enable_disable_wp_data_index dataIndex);
    NsmWriteProtectedControl() = delete;

    std::optional<Request> genRequestMsg(eid_t eid, uint8_t) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
};

} // namespace nsm
