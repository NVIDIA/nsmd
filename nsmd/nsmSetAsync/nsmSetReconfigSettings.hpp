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

#include "asyncOperationManager.hpp"
#include "nsmInterface.hpp"

#include <com/nvidia/InbandReconfigSettings/server.hpp>

namespace nsm
{
using ReconfigSettingsIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::server::InbandReconfigSettings>;

class NsmSetReconfigSettings : public NsmInterfaceProvider<ReconfigSettingsIntf>
{
  private:
    SensorManager& manager;
    reconfiguration_permissions_v1_index settingIndex;

    requester::Coroutine
        setAllowPermission(reconfiguration_permissions_v1_setting configuration,
                           const bool value, AsyncOperationStatusType& status,
                           std::shared_ptr<NsmDevice> device);

  public:
    NsmSetReconfigSettings() = delete;
    NsmSetReconfigSettings(const std::string& name, SensorManager& manager,
                           std::string objPath,
                           reconfiguration_permissions_v1_index settingIndex);

    requester::Coroutine
        allowOneShotConfig(const AsyncSetOperationValueType& value,
                           AsyncOperationStatusType* status,
                           std::shared_ptr<NsmDevice> device);
    requester::Coroutine
        allowPersistentConfig(const AsyncSetOperationValueType& value,
                              AsyncOperationStatusType* status,
                              std::shared_ptr<NsmDevice> device);
    requester::Coroutine
        allowFLRPersistentConfig(const AsyncSetOperationValueType& value,
                                 AsyncOperationStatusType* status,
                                 std::shared_ptr<NsmDevice> device);
};

} // namespace nsm
