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

#include "nsmGpuOperationalStatus.hpp"

#include "nsmDevice.hpp"
#ifdef NVIDIA_SHMEM
#include "sharedMemCommon.hpp"
#endif

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::server::
    OperationalStatus::StateType;

NsmGpuOperationalStatus::NsmGpuOperationalStatus(
    sdbusplus::bus_t& bus, const std::string& name, const std::string& type,
    const std::string& inventoryObjPath) :
    NsmObject(name, type), inventoryObjPath(inventoryObjPath)
{
    operationalStatusIntf = std::make_unique<GpuOperationalStatusIntf>(
        bus, inventoryObjPath.c_str());
    operationalStatusIntf->state(StateType::None);
    operationalStatusIntf->functional(false);
    updateMetricOnSharedMemory();
}

requester::Coroutine
    NsmGpuOperationalStatus::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    if (nsmDevice->getEid() == 0)
    {
        operationalStatusIntf->state(StateType::None);
        operationalStatusIntf->functional(false);
    }
    else if (nsmDevice->isOnline())
    {
        operationalStatusIntf->state(StateType::Enabled);
        operationalStatusIntf->functional(true);
    }
    else
    {
        operationalStatusIntf->state(StateType::UnavailableOffline);
        operationalStatusIntf->functional(false);
    }

    updateMetricOnSharedMemory();
    co_return NSM_SW_SUCCESS;
}

void NsmGpuOperationalStatus::handleOfflineState()
{
    operationalStatusIntf->state(StateType::UnavailableOffline);
    operationalStatusIntf->functional(false);
    updateMetricOnSharedMemory();
}

void NsmGpuOperationalStatus::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    std::vector<uint8_t> smbusData;
    auto stateStr = GpuOperationalStatusIntf::convertStateTypeToString(
        operationalStatusIntf->state());
    nv::sensor_aggregation::DbusVariantType stateValue{stateStr};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, GpuOperationalStatusIntf::interface, "State",
        smbusData, stateValue);
#endif
}

} // namespace nsm
