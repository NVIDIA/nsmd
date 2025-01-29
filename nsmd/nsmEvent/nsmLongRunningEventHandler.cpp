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

#include "nsmLongRunningEventHandler.hpp"

#include "deviceManager.hpp"
#include "sensorManager.hpp"

namespace nsm
{
NsmLongRunningEventHandler::NsmLongRunningEventHandler() :
    NsmEvent("NsmLongRunningEventHandler", "NSM_LONG_RUNNING_EVENT_HANDLER")
{}

int NsmLongRunningEventHandler::handle(eid_t eid, NsmType type,
                                       NsmEventId eventId, const nsm_msg* event,
                                       size_t eventLen)
{
    DeviceManager& deviceManager = DeviceManager::getInstance();
    SensorManager& sensorManager = SensorManager::getInstance();
    std::shared_ptr<NsmDevice> nsmDevice = nullptr;
    // update sensors for capabilities refresh
    // Get UUID from EID
    auto uuidOptional = utils::getUUIDFromEID(deviceManager.getEidTable(), eid);
    if (uuidOptional)
    {
        uuid_t uuid = *uuidOptional;
        // findNSMDevice instance for that eid
        nsmDevice = sensorManager.getNsmDevice(uuid);
        if (!nsmDevice)
        {
            lg2::error(
                "LongRunning event : The NSM device has not been discovered for , uuid={UUID}",
                "UUID", uuid);
            return NSM_SW_ERROR_DATA;
        }
    }
    else
    {
        lg2::error("LonrRunning event : No UUID found for EID {EID}", "EID",
                   eid);
        return NSM_SW_ERROR_DATA;
    }

    // Delegate the invocation to the NSM device
    return nsmDevice->invokeLongRunningHandler(eid, type, eventId, event,
                                               eventLen);
}

} // namespace nsm
