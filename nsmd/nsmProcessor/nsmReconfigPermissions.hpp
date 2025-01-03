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
#include "sensorManager.hpp"

#include <com/nvidia/InbandReconfigSettings/server.hpp>

namespace nsm
{
using ReconfigSettingsIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::server::InbandReconfigSettings>;

class NsmReconfigPermissions : public NsmSensor
{
  private:
    ReconfigSettingsIntf::FeatureType feature;
    reconfiguration_permissions_v1_index index;
    std::shared_ptr<ReconfigSettingsIntf> hostConfigIntf;
    std::shared_ptr<ReconfigSettingsIntf> doeConfigIntf;
    requester::Coroutine
        setAllowPermission(reconfiguration_permissions_v1_setting configuration,
                           const uint8_t value,
                           AsyncOperationStatusType& status,
                           std::shared_ptr<NsmDevice> device);

  public:
    NsmReconfigPermissions(const std::string& name, const std::string& type,
                           ReconfigSettingsIntf::FeatureType feature,
                           std::shared_ptr<ReconfigSettingsIntf> hostConfigIntf,
                           std::shared_ptr<ReconfigSettingsIntf> doeConfigIntf);
    NsmReconfigPermissions() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceNumber) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    /**
     * @brief Get the mapped Settings Index for Reconfiguration Permission
     * Feature
     *
     * @param feature PDI enumeration feature type
     * @return reconfiguration_permissions_v1_index Specification settings index
     */
    static reconfiguration_permissions_v1_index
        getIndex(ReconfigSettingsIntf::FeatureType feature);
    requester::Coroutine
        patchHostOneShotConfig(const AsyncSetOperationValueType& value,
                               AsyncOperationStatusType* status,
                               std::shared_ptr<NsmDevice> device);
    requester::Coroutine
        patchDOEOneShotConfig(const AsyncSetOperationValueType& value,
                              AsyncOperationStatusType* status,
                              std::shared_ptr<NsmDevice> device);
    requester::Coroutine
        patchHostPersistentConfig(const AsyncSetOperationValueType& value,
                                  AsyncOperationStatusType* status,
                                  std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        patchDOEPersistentConfig(const AsyncSetOperationValueType& value,
                                 AsyncOperationStatusType* status,
                                 std::shared_ptr<NsmDevice> device);
    requester::Coroutine
        patchHostFLRPersistentConfig(const AsyncSetOperationValueType& value,
                                     AsyncOperationStatusType* status,
                                     std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        patchDOEFLRPersistentConfig(const AsyncSetOperationValueType& value,
                                    AsyncOperationStatusType* status,
                                    std::shared_ptr<NsmDevice> device);
};
} // namespace nsm
