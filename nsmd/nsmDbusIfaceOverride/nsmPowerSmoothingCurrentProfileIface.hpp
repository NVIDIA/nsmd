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
#include <com/nvidia/PowerSmoothing/CurrentPowerProfile/server.hpp>
#include <com/nvidia/PowerSmoothing/ProfileActionAsync/server.hpp>
namespace nsm
{
using CurrentPowerProfileIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerSmoothing::server::CurrentPowerProfile>;
using ProfileActionAsyncIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerSmoothing::server::ProfileActionAsync>;
class OemCurrentPowerProfileIntf : public CurrentPowerProfileIntf
{
  private:
    std::shared_ptr<NsmDevice> device;
    std::string inventoryObjPath;
    //  signal emission is deferred until the initialization is complete
  public:
    OemCurrentPowerProfileIntf(sdbusplus::bus::bus& bus,
                               const std::string& path,
                               std::string adminProfilePath,
                               std::shared_ptr<NsmDevice> device) :
        CurrentPowerProfileIntf(bus, path.c_str(), action::defer_emit),
        device(device), inventoryObjPath(path)
    {
        CurrentPowerProfileIntf::appliedProfilePath(path);
        CurrentPowerProfileIntf::adminProfilePath(adminProfilePath);
    }
};
} // namespace nsm
