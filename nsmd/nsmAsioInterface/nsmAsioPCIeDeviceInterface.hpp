/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include "nsmAsioInterfaceBase.hpp"

#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeSlot/server.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace nsm
{

using PCIeDeviceServer =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PCIeDevice;
using PCIeSlotServer =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PCIeSlot;

/**
 * @class NsmAsioPCIeDeviceInterface
 * @brief ASIO wrapper for PCIeDevice D-Bus interface with selective property
 *        registration.
 *
 * Registers only the properties needed for single-port PCIe devices
 * (DeviceType, link generation/lanes, and per-function properties).
 * BusNumber is omitted as it is only applicable to multi-port devices.
 */
class NsmAsioPCIeDeviceInterface : public NsmAsioInterfaceBase
{
  public:
    static std::shared_ptr<NsmAsioPCIeDeviceInterface>
        createSinglePortDevice(sdbusplus::asio::object_server& objServer,
                               const std::string& objectPath,
                               const std::string& deviceType,
                               const std::vector<uint64_t>& functionIds);

    NsmAsioPCIeDeviceInterface(sdbusplus::asio::object_server& objServer,
                               const std::string& objectPath,
                               const std::string& deviceType,
                               const std::vector<uint64_t>& functionIds);

    void updateLinkSpeed(const std::string& pcieType,
                         const std::string& generationInUse,
                         const std::string& maxPcieType, size_t lanesInUse,
                         size_t maxLanes);

    void updateFunction(uint8_t functionId, const std::string& vendorId,
                        const std::string& deviceId,
                        const std::string& classCode,
                        const std::string& revisionId,
                        const std::string& functionType,
                        const std::string& deviceClass,
                        const std::string& subsystemVendorId,
                        const std::string& subsystemId);

  private:
    bool initialize() override;
    std::vector<uint64_t> functionIds;
};

} // namespace nsm
