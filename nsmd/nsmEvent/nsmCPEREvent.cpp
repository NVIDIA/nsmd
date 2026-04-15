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

#include "nsmCPEREvent.hpp"

#include "device-capability-discovery.h"
#include "platform-environmental.h"

#include "dBusAsyncUtils.hpp"
#include "nsmEventLogRecordV2.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmCPEREvent::NsmCPEREvent(std::shared_ptr<NsmDevice> nsmDevice,
                           const std::string& name, const std::string& type) :
    NsmEvent(name, type), nsmDevice(nsmDevice)
{
    cperRecordData.reserve(MAX_CPER_RECORD_DATA_SIZE_KB * BYTES_PER_KB);
}

int NsmCPEREvent::handle(eid_t eid, NsmType /*type*/, NsmEventId /*eventId*/,
                         const nsm_msg* event, size_t eventLen)
{
    lg2::info("Received CPER Event from EID {EID}.", "EID", eid);

    uint16_t eventState{};
    uint8_t dataSize{};

    auto rc = decode_nsm_event(event, eventLen, NSM_CPER_EVENT,
                               NSM_POLLED_EVENT_CLASS, &eventState, &dataSize);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_nsm_event for CPER failed. rc={RC}, EID={SRC}, Name={NAME}",
            "RC", rc, "SRC", eid, "NAME", getName());

        return rc;
    }

    if (eventHandles.size() >= MAX_EVENT_HANDLES)
    {
        const auto droppedHandle = eventHandles.front();
        eventHandles.pop();
        lg2::warning(
            "CPER Event: queue full (MAX_EVENT_HANDLES={MAX}), dropping oldest event: droppedHandle={DROPPED}, EID={EID}",
            "MAX", (unsigned)MAX_EVENT_HANDLES, "DROPPED", droppedHandle, "EID",
            eid);
    }
    eventHandles.push(eventState);

    lg2::info("CPER Event decoded: eventHandle={EH}, dataSize={DS}, EID={EID}",
              "EH", eventState, "DS", dataSize, "EID", eid);

    if (nsmDevice && eventLogRecordChunkCollector)
    {
        if (eventLogRecordChunkCollector->isRecordCollectionInProgress())
        {
            lg2::info(
                "CPER Event: Record collection is already in progress for EID={EID}, queue size={QS}",
                "EID", eid, "QS", eventHandles.size());
        }
        else
        {
            auto eventHandle = eventHandles.front();
            eventHandles.pop();
            lg2::info(
                "CPER Event: Triggering record chunk collection for event eid = {EID}, handle={EH}, remaining queue size={QS}",
                "EID", eid, "EH", eventHandle, "QS", eventHandles.size());
            nsmDevice->addDumpCollectionSensor(eventLogRecordChunkCollector);
            eventLogRecordChunkCollector->triggerRecordChunkCollection(
                eventHandle, 0);
        }
    }
    else
    {
        lg2::error("CPER Event: NSM device not found for EID={EID}", "EID",
                   eid);
    }

    return NSM_SW_SUCCESS;
}

requester::Coroutine NsmCPEREvent::cperRecordLogger()
{
    static constexpr auto loggerObj = "/xyz/openbmc_project/cperlogger";
    static constexpr auto loggerIntf = "xyz.openbmc_project.CPER";

    lg2::debug("Going to log CPER Record Data of size: {DATA} bytes", "DATA",
               cperRecordData.size());
    if (!cperRecordData.empty())
    {
        try
        {
            auto service = utils::DBusHandler().getService(loggerObj,
                                                           loggerIntf);
            if (service.empty())
            {
                lg2::error("Failed to get CPER Logger service");
                co_return NSM_SW_ERROR;
            }
            lg2::debug("CPER Event: Logging record on service: {SERVICE}",
                       "SERVICE", service);
            auto logSuccess = co_await utils::coDbusMethodCall(
                service, loggerObj, loggerIntf, "CreateLog", cperRecordData);
            if (!logSuccess)
            {
                lg2::error("Failed to notify CPER Logger");
            }
        }
        catch (const std::exception& e)
        {
            lg2::error("Failed to notify CPER Logger: {ERROR}", "ERROR",
                       e.what());
        }
    }
    cperRecordData.clear();

    if (!eventHandles.empty())
    {
        auto eventHandle = eventHandles.front();
        eventHandles.pop();
        lg2::info(
            "CPER Event: Triggering record chunk collection for event eid = {EID}, handle={EH}, remaining queue size={QS}",
            "EID", nsmDevice->getEid(), "EH", eventHandle, "QS",
            eventHandles.size());
        nsmDevice->addDumpCollectionSensor(eventLogRecordChunkCollector);
        eventLogRecordChunkCollector->triggerRecordChunkCollection(eventHandle,
                                                                   0);
    }
    else
    {
        eventLogRecordChunkCollector->markRecordCollectionComplete();
    }
    co_return NSM_SW_SUCCESS;
}

void NsmCPEREvent::logRecordOnRf(std::vector<uint8_t>& recordChunk,
                                 uint8_t returnCode, uint16_t nextEventHandle)
{
    cperRecordData.insert(cperRecordData.end(), recordChunk.begin(),
                          recordChunk.end());
    if (returnCode == NSM_SW_SUCCESS)
    {
        if (nextEventHandle == NO_MORE_HANDLES)
        {
            lg2::info(
                "CPER Event: No more records present on VBIOS, clearing event handles");
            eventHandles = {};
        }
        else if (nextEventHandle == PENDING_HANDLE_VALUE &&
                 eventHandles.empty())
        {
            lg2::info(
                "CPER Event: Pushing next event handle: {EH}, as few more records are expected",
                "EH", nextEventHandle);
            eventHandles.push(nextEventHandle);
        }
    }
    requester::Coroutine::assign(cperRecordLoggerHandle,
                                 [&]() -> requester::Coroutine {
        co_return co_await cperRecordLogger();
    });
}

void NsmCPEREvent::setEventLogRecordChunkCollector(
    std::shared_ptr<NsmEventLogRecordV2> collector)
{
    eventLogRecordChunkCollector = std::move(collector);
}

requester::Coroutine createNsmCPEREvent(std::shared_ptr<NsmDevice> nsmDevice,
                                        const std::string& name,
                                        const std::string& type)
{
    auto event = std::make_shared<NsmCPEREvent>(nsmDevice, name, type);

    auto eventLogRecordV2 = std::make_shared<NsmEventLogRecordV2>(
        name + "_EventLogRecordV2", type, NSM_EVENT_LOG_V2_MODE_GET_DATA, 0,
        event);

    event->setEventLogRecordChunkCollector(eventLogRecordV2);

    lg2::info("Created NSM CPER Event : Name={NAME}, Type={TYPE}", "NAME", name,
              "TYPE", type);

    nsmDevice->addDeviceEvent(event, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                              NSM_CPER_EVENT);
    co_return NSM_SUCCESS;
}

} // namespace nsm
