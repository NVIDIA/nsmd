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

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using PCIeDeviceIntf = object_t<Inventory::Item::server::PCIeDevice>;

class NsmPCIeFunction :
    public NsmSensor,
    public NsmInterfaceContainer<PCIeDeviceIntf>
{
  public:
    NsmPCIeFunction(const NsmInterfaceProvider<PCIeDeviceIntf>& provider,
                    uint8_t deviceId, uint8_t functionId);
    NsmPCIeFunction() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    const uint8_t deviceId;
    const uint8_t functionId;
};

} // namespace nsm