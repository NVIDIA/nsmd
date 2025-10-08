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

#include "libnsm/pci-links.h"

#include "asyncOperationManager.hpp"
#include "nsmInterface.hpp"
#include "nsmSensor.hpp"
#include "sensorManager.hpp"

#include <nsmSensorAggregator.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/PCIe/PCIePortConfigurationInfo/server.hpp>

namespace nsm
{
using PCIePortConfigurationInfoIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIePortConfigurationInfo>;

class NsmPCIePortConfigurationInfo : public NsmSensorAggregator
{
  public:
    NsmPCIePortConfigurationInfo(const std::string& name,
                                 const std::string& type,
                                 std::shared_ptr<PCIePortConfigurationInfoIntf>
                                     pciePortConfigurationInfoIntf,
                                 uint8_t portNumber, uint8_t portType,
                                 uint8_t portIndex,
                                 const std::string& inventoryObjPath);
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    int handleSample(const TelemetrySample& sample) override;

    void updatePresetProperty(uint8_t tag, uint8_t preset0, uint8_t preset1);
    void updateTxAmplitudeProperty(uint8_t txAmplitude);

    requester::Coroutine
        setPortConfiguration(const AsyncSetOperationValueType& value,
                             [[maybe_unused]] AsyncOperationStatusType* status,
                             std::shared_ptr<NsmDevice> device);

  private:
    std::shared_ptr<PCIePortConfigurationInfoIntf>
        pciePortConfigurationInfoIntf;
    static const std::unordered_map<uint8_t, std::string> tagToPropertyMap;
    const uint8_t portNumber;
    const uint8_t portType;
    const uint8_t portIndex;
    const std::string inventoryObjPath;
};

} // namespace nsm
