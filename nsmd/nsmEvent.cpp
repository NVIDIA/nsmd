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

#include "nsmEvent.hpp"

#include "dBusAsyncUtils.hpp"
#include "eventHandler.hpp"
#include "progressCounters.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <coroutine>
#include <mutex>
#include <optional>

namespace nsm
{

requester::Coroutine
    logEventAsync(const std::string& messageId, Level level,
                  const std::map<std::string, std::string>& data)
{
    static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
    static constexpr auto logInterface = "xyz.openbmc_project.Logging.Create";

    // Cache the logging service since it's fixed after first discovery
    static std::optional<std::string> cachedLoggingService;
    static std::mutex serviceCacheMutex;

    std::string service;
    {
        std::lock_guard<std::mutex> lock(serviceCacheMutex);
        if (cachedLoggingService)
        {
            service = *cachedLoggingService;
        }
    }

    // If service not cached, look it up using existing utility
    if (service.empty())
    {
        auto serviceMap = co_await utils::coGetServiceMap(logObjPath,
                                                          {logInterface});

        if (serviceMap.empty())
        {
            lg2::error(
                "logEventAsync : Failed to lookup logging service. MessageId={MSG}",
                "MSG", messageId);
            co_return NSM_SW_ERROR;
        }

        service = serviceMap.begin()->first;

        // Cache the service for future calls
        {
            std::lock_guard<std::mutex> lock(serviceCacheMutex);
            if (!cachedLoggingService) // Double-check in case another coroutine
                                       // cached it
            {
                cachedLoggingService = service;
            }
        }
    }

    // Make the logging call asynchronously
    auto logSuccess = co_await utils::coLogEvent(service, messageId, level,
                                                 data);

    if (!logSuccess)
    {
        lg2::error(
            "logEventAsync : Failed to create log entry. MessageId={MSG}",
            "MSG", messageId);
        co_return NSM_SW_ERROR;
    }

    co_return NSM_SW_SUCCESS;
}

int EventDispatcher::addEvent(NsmType type, NsmEventId eventId,
                              std::shared_ptr<NsmEvent> event)
{
    auto& events = eventsMap[type];

    if (events.find(eventId) != events.end())
    {
        return NSM_SW_ERROR_DATA;
    }

    events.emplace(eventId, std::move(event));

    return NSM_SW_SUCCESS;
}

int EventDispatcher::handle(eid_t eid, NsmType type, NsmEventId eventId,
                            const nsm_msg* event, size_t eventLen) const
{
    auto events = eventsMap.find(type);
    if (events == eventsMap.end())
    {
        lg2::error(
            "No NsmEvents found for NSM Message Type {TYPE} : EventId={ID}, EID={EID}",
            "TYPE", type, "ID", eventId, "EID", eid);
        return NSM_SW_ERROR_DATA;
    }

    auto eventObj = events->second.find(eventId);
    if (eventObj == events->second.end())
    {
        lg2::error(
            "No NsmEvent found for EventId {ID} in NSM Message Type {TYPE} : EID={EID}",
            "ID", eventId, "TYPE", type, "EID", eid);
        return NSM_SW_ERROR_DATA;
    }

    return eventObj->second->handle(eid, type, eventId, event, eventLen);
}

int DelegatingEventHandler::enableDelegation(NsmEventId eventId)
{
    if (handlers.find(eventId) != handlers.end())
    {
        lg2::error(
            "Delegation failed for EventId {ID} for NSM Message Type {TYPE} : Event handler already exists.",
            "ID", eventId, "TYPE", nsmType());
        return NSM_SW_ERROR_DATA;
    }

    handlers.emplace(eventId,
                     std::bind_front(&DelegatingEventHandler::delegate, this));

    return NSM_SW_SUCCESS;
}

void DelegatingEventHandler::delegate(eid_t eid, NsmType type,
                                      NsmEventId eventId, const nsm_msg* event,
                                      size_t eventLen)
{
    auto nsmDevice =
        mctp::MctpDiscovery::getInstance().getNsmDeviceFromEid(eid);

    if (!nsmDevice)
    {
        lg2::error(
            "Nsm Event : The NSM device has not been discovered for EID={EID}",
            "EID", eid);

        return;
    }

    uint8_t rc = nsmDevice->eventDispatcher.handle(eid, type, eventId, event,
                                                   eventLen);
    nsmDevice->progressCounters().increment(ProgressCounterType::Event, rc);
}

} // namespace nsm
