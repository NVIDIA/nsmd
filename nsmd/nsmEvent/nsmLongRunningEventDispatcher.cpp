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

#include "nsmLongRunningEventDispatcher.hpp"

namespace nsm
{
NsmLongRunningEventDispatcher::NsmLongRunningEventDispatcher() :
    NsmEvent("NsmLongRunningEventDispatcher",
             "NSM_LONG_RUNNING_EVENT_DISPATCHER")
{}
int NsmLongRunningEventDispatcher::addEvent(NsmType type, uint8_t command,
                                            std::shared_ptr<NsmEvent> event)
{
    auto& events = eventsMap[type];

    if (events.find(command) != events.end())
    {
        lg2::error(
            "NsmLongRunningEventDispatcher : Command {COMMAND} already exists for NSM Message Type {TYPE}",
            "COMMAND", command, "TYPE", type);
        return NSM_SW_ERROR_DATA;
    }

    events.emplace(command, std::move(event));

    return NSM_SW_SUCCESS;
}

int NsmLongRunningEventDispatcher::handle(eid_t eid, NsmType type,
                                          NsmEventId eventId,
                                          const nsm_msg* event, size_t eventLen)
{
    uint16_t eventState = 0;
    uint8_t dataSize = 0;
    auto rc = decode_nsm_event(event, eventLen, eventId,
                               NSM_NVIDIA_GENERAL_EVENT_CLASS, &eventState,
                               &dataSize);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmLongRunningEventDispatcher : Failed to decode long running event state : EID={EID}",
            "EID", eid);
        return rc;
    }
    nsm_long_running_event_state state{};
    memcpy(&state, &eventState, sizeof(uint16_t));

    auto events = eventsMap.find(state.nvidia_message_type);
    if (events == eventsMap.end())
    {
        lg2::error(
            "No NsmLongRunningEvents found for NSM Message Type {TYPE}: EID={EID}",
            "TYPE", uint8_t(state.nvidia_message_type), "EID", eid);
        return NSM_SW_ERROR_DATA;
    }

    auto eventObj = events->second.find(state.command);
    if (eventObj == events->second.end())
    {
        lg2::error(
            "No NsmLongRunningEvent found for command {COMMAND} in NSM Message Type {TYPE} : EID={EID}",
            "COMMAND", uint8_t(state.command), "TYPE",
            uint8_t(state.nvidia_message_type), "EID", eid);
        return NSM_SW_ERROR_DATA;
    }

    return eventObj->second->handle(eid, type, eventId, event, eventLen);
}

} // namespace nsm
