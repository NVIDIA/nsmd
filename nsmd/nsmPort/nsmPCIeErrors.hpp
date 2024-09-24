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

#include "nsmInterface.hpp"
#include "sharedMemCommon.hpp"

#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>

namespace nsm
{
using namespace nsm_shmem_utils;
using sdbusplus::server::object_t;
using PCIeEccIntf =
    object_t<sdbusplus::xyz::openbmc_project::PCIe::server::PCIeECC>;

class NsmPCIeErrors :
    public NsmSensor,
    public NsmInterfaceContainer<PCIeEccIntf>
{
  private:
    const uint8_t deviceIndex;
    const uint8_t groupId;
    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    void updateMetricOnSharedMemory() override;
    void handleResponse(const nsm_query_scalar_group_telemetry_group_2& data);
    void handleResponse(const nsm_query_scalar_group_telemetry_group_3& data);
    void handleResponse(const nsm_query_scalar_group_telemetry_group_4& data);

  public:
    NsmPCIeErrors() = delete;
    NsmPCIeErrors(const NsmInterfaceProvider<PCIeEccIntf>& provider,
                  uint8_t deviceIndex, uint8_t groupId);
};

} // namespace nsm
