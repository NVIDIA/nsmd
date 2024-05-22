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

#include "deviceManager.hpp"
#include "eventHandler.hpp"
#include "sensorManager.hpp"

namespace nsm
{

int logEvent(const std::string& messageId, Level level,
             const std::map<std::string, std::string>& data)
{
    static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
    static constexpr auto logInterface = "xyz.openbmc_project.Logging.Create";
    auto& bus = utils::DBusHandler::getBus();

    try
    {
        auto service = utils::DBusHandler().getService(logObjPath,
                                                       logInterface);
        auto severity =
            sdbusplus::xyz::openbmc_project::Logging::server::convertForMessage(
                level);
        auto method = bus.new_method_call(service.c_str(), logObjPath,
                                          logInterface, "Create");
        method.append(messageId, level, data);
        bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "logEvent : Failed to create D-Bus log entry for message registry, {ERROR}. MessageId={MSG}",
            "ERROR", e, "MSG", messageId);

        return NSM_SW_ERROR;
    }

    return NSM_SW_SUCCESS;
};

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
    DeviceManager& deviceManager = DeviceManager::getInstance();
    SensorManager& sensorManager = SensorManager::getInstance();

    auto uuidOptional = utils::getUUIDFromEID(deviceManager.getEidTable(), eid);
    if (!uuidOptional)
    {
        lg2::error("Nsm Event : No UUID found for EID {EID}", "EID", eid);

        return;
    }

    uuid_t uuid = *uuidOptional;

    auto nsmDevice = sensorManager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        lg2::error(
            "Nsm Event : The NSM device has not been discovered for EID={EID}, uuid={UUID}",
            "EID", eid, "UUID", uuid);

        return;
    }

    nsmDevice->eventDispatcher.handle(eid, type, eventId, event, eventLen);
}

} // namespace nsm
