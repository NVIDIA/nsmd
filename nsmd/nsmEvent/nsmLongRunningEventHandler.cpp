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

#include "requester/mctp_endpoint_discovery.hpp"
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
    auto nsmDevice =
        mctp::MctpDiscovery::getInstance().getNsmDeviceFromEid(eid);
    if (!nsmDevice)
    {
        lg2::error(
            "LongRunning event : The NSM device has not been discovered for , eid={EID}",
            "EID", eid);
        return NSM_SW_ERROR_DATA;
    }
    return nsmDevice->invokeLongRunningHandler(eid, type, eventId, event,
                                               eventLen);
}

} // namespace nsm
