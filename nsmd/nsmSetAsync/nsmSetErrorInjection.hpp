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
#include "coroutineSemaphore.hpp"
#include "nsmActivateErrorInjectAsyncIntf.hpp"
#include "nsmErrorInjection/nsmErrorInjection.hpp"
#include "nsmInterface.hpp"

#include <com/nvidia/ErrorInjection/ErrorInjection/server.hpp>
#include <com/nvidia/ErrorInjection/ErrorInjectionCapability/server.hpp>
#include <com/nvidia/ErrorInjection/ErrorInjectionPayload/server.hpp>

#include <map>
#include <optional>
#include <string>

namespace nsm
{
using ErrorInjectionIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ErrorInjection::server::ErrorInjection>;
using ErrorInjectionCapabilityIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ErrorInjection::server::ErrorInjectionCapability>;
using ErrorInjectionPayloadIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ErrorInjection::server::ErrorInjectionPayload>;

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

/**
 * @brief Sole owner of this device's error-injection type mask.
 *
 * The device command writes all 64 type bits at once with no "leave unchanged"
 * encoding, so every write is a read-modify-write. Both the aggregate batch
 * property and the per-type Enabled property route through here.
 */
class NsmSetErrorInjectionCapabilities :
    public NsmInterfaceContainer<ErrorInjectionCapabilityIntf>,
    public NsmObject
{
  private:
    SensorManager& manager;
    /** @brief Serializes mask writes on this device. Waiters block rather than
     *         fail, and resume once the holder has republished its result. */
    common::CoroutineSemaphore maskSemaphore;
    /** @brief The polling sensor for this device's mask. Its update() is the
     *         read-and-republish flow reused after a write. */
    std::shared_ptr<NsmErrorInjectionEnabled> enabledSensor;

    /** @brief Performs the mask read-modify-write. Called with maskSemaphore
     *         held. */
    requester::Coroutine writeMask(
        const std::map<ErrorInjectionCapabilityIntf::Type, bool>& overrides,
        AsyncOperationStatusType& status, std::shared_ptr<NsmDevice> device);

  public:
    NsmSetErrorInjectionCapabilities() = delete;
    NsmSetErrorInjectionCapabilities(
        const std::string& name, SensorManager& manager,
        const Interfaces<ErrorInjectionCapabilityIntf>& interfaces,
        std::shared_ptr<NsmErrorInjectionEnabled> enabledSensor);

    /**
     * @brief Batch entry point. Takes a(sv) of (type leaf name, bool); absent
     *        types keep their value. Bad names are reported through @p status,
     *        never thrown -- a throw here is swallowed by the promise.
     */
    requester::Coroutine
        capabilitiesEnabled(const AsyncSetOperationValueType& value,
                            AsyncOperationStatusType* status,
                            std::shared_ptr<NsmDevice> device);

    /**
     * @brief Applies @p overrides on top of the siblings' current state in one
     *        device write, then reads the mask back and republishes it. Blocks
     *        while another write is in flight.
     */
    requester::Coroutine applyEnabledOverrides(
        const std::map<ErrorInjectionCapabilityIntf::Type, bool>& overrides,
        AsyncOperationStatusType& status, std::shared_ptr<NsmDevice> device);

    /** @brief Resolve a capability type leaf name (e.g. "ThermalErrors") to a
     *         type actually registered on this device, or nullopt. */
    std::optional<ErrorInjectionCapabilityIntf::Type>
        resolveTypeName(const std::string& name);
};

class NsmSetErrorInjectionEnabled :
    public NsmInterfaceContainer<ErrorInjectionCapabilityIntf>,
    public NsmObject
{
  private:
    ErrorInjectionCapabilityIntf::Type type;
    /** @brief Owns the mask RMW; this per-type setter is a thin adapter onto
     *         it so both patch forms share one code path and one lock. */
    std::shared_ptr<NsmSetErrorInjectionCapabilities> capabilities;

    requester::Coroutine setEnabled(bool value,
                                    AsyncOperationStatusType& status,
                                    std::shared_ptr<NsmDevice> device);

  public:
    NsmSetErrorInjectionEnabled() = delete;
    NsmSetErrorInjectionEnabled(
        const std::string& name, ErrorInjectionCapabilityIntf::Type type,
        const Interfaces<ErrorInjectionCapabilityIntf>& interfaces,
        std::shared_ptr<NsmSetErrorInjectionCapabilities> capabilities);

    requester::Coroutine enabled(const AsyncSetOperationValueType& value,
                                 AsyncOperationStatusType* status,
                                 std::shared_ptr<NsmDevice> device);
};

class NsmSetErrorInjectionPayload :
    public NsmInterfaceContainer<ErrorInjectionPayloadIntf>,
    public NsmObject
{
  private:
    SensorManager& manager;
    std::shared_ptr<NsmActivateErrorInjectionPayloadIntf> activateIntf;
    uint16_t errorInjectionType;
    uint16_t errorInjectionSubtype;
    requester::Coroutine setPayload(std::vector<uint8_t> faultBitMap,
                                    AsyncOperationStatusType& status,
                                    std::shared_ptr<NsmDevice> device);

  public:
    NsmSetErrorInjectionPayload() = delete;
    NsmSetErrorInjectionPayload(
        const std::string& name, SensorManager& manager,
        const Interfaces<ErrorInjectionPayloadIntf>& interfaces,
        std::shared_ptr<NsmActivateErrorInjectionPayloadIntf> activateIntf,
        uint16_t errorInjectionType, uint16_t errorInjectionSubtype);

    requester::Coroutine
        setErrorInjectionPayload(const AsyncSetOperationValueType& value,
                                 AsyncOperationStatusType* status,
                                 std::shared_ptr<NsmDevice> device);
};

} // namespace nsm
