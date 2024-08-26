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

#include "asyncOperationManager.hpp"
#include "nsmInterface.hpp"

#include <com/nvidia/ErrorInjection/ErrorInjection/server.hpp>
#include <com/nvidia/ErrorInjection/ErrorInjectionCapability/server.hpp>

namespace nsm
{
using ErrorInjectionIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ErrorInjection::server::ErrorInjection>;
using ErrorInjectionCapabilityIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ErrorInjection::server::ErrorInjectionCapability>;

class NsmSetErrorInjection : public NsmInterfaceProvider<ErrorInjectionIntf>
{
  private:
    SensorManager& manager;

    requester::Coroutine setModeEnabled(bool value,
                                        AsyncOperationStatusType& status,
                                        std::shared_ptr<NsmDevice> device);

  public:
    NsmSetErrorInjection() = delete;
    NsmSetErrorInjection(SensorManager& manager, const path& objPath);

    requester::Coroutine
        errorInjectionModeEnabled(const AsyncSetOperationValueType& value,
                                  AsyncOperationStatusType* status,
                                  std::shared_ptr<NsmDevice> device);
};

class NsmSetErrorInjectionEnabled :
    public NsmInterfaceContainer<ErrorInjectionCapabilityIntf>,
    public NsmObject
{
  private:
    ErrorInjectionCapabilityIntf::Type type;
    SensorManager& manager;

    requester::Coroutine setEnabled(bool value,
                                    AsyncOperationStatusType& status,
                                    std::shared_ptr<NsmDevice> device);

  public:
    NsmSetErrorInjectionEnabled() = delete;
    NsmSetErrorInjectionEnabled(
        const std::string& name, ErrorInjectionCapabilityIntf::Type type,
        SensorManager& manager,
        const Interfaces<ErrorInjectionCapabilityIntf>& interfaces);

    requester::Coroutine enabled(const AsyncSetOperationValueType& value,
                                 AsyncOperationStatusType* status,
                                 std::shared_ptr<NsmDevice> device);
};

} // namespace nsm
