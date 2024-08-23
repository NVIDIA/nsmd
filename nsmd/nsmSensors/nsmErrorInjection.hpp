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

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmInterface.hpp"

#include <com/nvidia/ErrorInjection/ErrorInjection/server.hpp>
#include <com/nvidia/ErrorInjection/ErrorInjectionCapability/server.hpp>

namespace nsm
{
using ErrorInjectionIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ErrorInjection::server::ErrorInjection>;
using ErrorInjectionCapabilityIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ErrorInjection::server::ErrorInjectionCapability>;

class NsmErrorInjection :
    public NsmSensor,
    public NsmInterfaceContainer<ErrorInjectionIntf>
{
  public:
    NsmErrorInjection(const NsmInterfaceProvider<ErrorInjectionIntf>& provider);
    NsmErrorInjection() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
};

class NsmErrorInjectionSupported :
    public NsmSensor,
    public NsmInterfaceContainer<ErrorInjectionCapabilityIntf>
{
  public:
    NsmErrorInjectionSupported(
        const NsmInterfaceProvider<ErrorInjectionCapabilityIntf>& provider);
    NsmErrorInjectionSupported() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
};

class NsmErrorInjectionEnabled : public NsmErrorInjectionSupported
{
  public:
    using NsmErrorInjectionSupported::NsmErrorInjectionSupported;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
};

} // namespace nsm
