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

#include "libnsm/network-ports.h"

#include "asyncOperationManager.hpp"
#include "nsmInterface.hpp"
#include "utils.hpp"

#include <com/nvidia/NVLink/NVLinkDisableFuture/server.hpp>
#include <sdbusplus/asio/object_server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using NvLinkDisableFutureIntf =
    object_t<sdbusplus::com::nvidia::NVLink::server::NVLinkDisableFuture>;

class NsmDevicePortDisableFuture :
    public NsmInterfaceProvider<NvLinkDisableFutureIntf>
{
  public:
    NsmDevicePortDisableFuture() = delete;
    NsmDevicePortDisableFuture(const std::string& name, const std::string& type,
                               const std::string& inventoryObjPath) :
        NsmInterfaceProvider<NvLinkDisableFutureIntf>(name, type,
                                                      inventoryObjPath),
        objPath(inventoryObjPath + name)
    {}

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

    std::string getInventoryObjectPath()
    {
        return objPath;
    }

    requester::Coroutine
        setDevicePortDisableFuture(bitfield8_t* mask,
                                   AsyncOperationStatusType* status,
                                   std::shared_ptr<NsmDevice> device);

    requester::Coroutine
        setPortDisableFuture(const AsyncSetOperationValueType& value,
                             AsyncOperationStatusType* status,
                             std::shared_ptr<NsmDevice> device);

  private:
    std::string objPath;
    bool asyncPatchInProgress{false};
};

} // namespace nsm
