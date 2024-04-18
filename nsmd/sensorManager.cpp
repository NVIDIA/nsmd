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

#include "config.h"

#include "sensorManager.hpp"

#include "libnsm/device-capability-discovery.h"
#include "libnsm/platform-environmental.h"

#include "nsmObject.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <chrono>
#include <iostream>

namespace nsm
{

// Static instance definition
SensorManager* SensorManager::instance = nullptr;

// Ensuring the constructor remains private and defined here if not explicitly
// declared in the header
SensorManager::SensorManager(
    sdbusplus::bus::bus& bus, sdeventplus::Event& event,
    requester::Handler<requester::Request>& handler, InstanceIdDb& instanceIdDb,
    sdbusplus::asio::object_server& objServer,
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable,
    NsmDeviceTable& nsmDevices, eid_t localEid) :
    bus(bus), event(event), handler(handler), instanceIdDb(instanceIdDb),
    objServer(objServer), eidTable(eidTable), nsmDevices(nsmDevices),
    localEid(localEid)
{
    deferScanInventory = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(&SensorManager::scanInventory, this));
    inventoryAddedSignal = std::make_unique<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded(
            "/xyz/openbmc_project/inventory"),
        [this](sdbusplus::message::message& msg) {
            this->interfaceAddedhandler(msg);
        });
}

void SensorManager::scanInventory()
{
    deferScanInventory.reset();
    try
    {
        std::vector<std::string> ifaceList;
        for (auto [interface, functionData] :
             NsmObjectFactory::instance().creationFunctions)
        {
            ifaceList.emplace_back(interface);
        }

        if (ifaceList.size() == 0)
        {
            return;
        }

        GetSubTreeResponse getSubtreeResponse = utils::DBusHandler().getSubtree(
            "/xyz/openbmc_project/inventory", 0, ifaceList);

        if (getSubtreeResponse.size() == 0)
        {
            return;
        }

        for (const auto& [objPath, mapServiceInterfaces] : getSubtreeResponse)
        {
            for (const auto& [_, interfaces] : mapServiceInterfaces)
            {
                for (const auto& interface : interfaces)
                {
                    NsmObjectFactory::instance().createObjects(*this, interface,
                                                               objPath);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Error while getSubtree of /xyz/openbmc_project/inventory.{ERROR}",
            "ERROR", e);
        return;
    }

    newSensorEvent = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn(&SensorManager::_startPolling), this,
                         std::placeholders::_1));
}

void SensorManager::interfaceAddedhandler(sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objPath;
    dbus::InterfaceMap interfaces;

    msg.read(objPath, interfaces);
    for (const auto& [interface, _] : interfaces)
    {
        NsmObjectFactory::instance().createObjects(*this, interface, objPath);
    }

    newSensorEvent = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn(&SensorManager::_startPolling), this,
                         std::placeholders::_1));
}

void SensorManager::_startPolling(sdeventplus::source::EventBase& /* source */)
{
    newSensorEvent.reset();
    startPolling();
}

void SensorManager::startPolling()
{
    for (auto& nsmDevice : nsmDevices)
    {
        if (!nsmDevice->pollingTimer)
        {
            nsmDevice->pollingTimer = std::make_unique<phosphor::Timer>(
                event.get(),
                std::bind_front(&SensorManager::doPolling, this, nsmDevice));
        }

        if (!(nsmDevice->pollingTimer->isRunning()))
        {
            nsmDevice->pollingTimer->start(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::milliseconds(SENSOR_POLLING_TIME)),
                true);
        }
    }
}

void SensorManager::stopPolling()
{
    for (auto& nsmDevice : nsmDevices)
    {
        nsmDevice->pollingTimer->stop();
    }
}

void SensorManager::doPolling(std::shared_ptr<NsmDevice> nsmDevice)
{
    if (nsmDevice->doPollingTaskHandle)
    {
        if (!(nsmDevice->doPollingTaskHandle.done()))
        {
            return;
        }
        nsmDevice->doPollingTaskHandle.destroy();
    }

    auto co = doPollingTask(nsmDevice);
    nsmDevice->doPollingTaskHandle = co.handle;
    if (nsmDevice->doPollingTaskHandle.done())
    {
        nsmDevice->doPollingTaskHandle = nullptr;
    }
}

requester::Coroutine
    SensorManager::doPollingTask(std::shared_ptr<NsmDevice> nsmDevice)
{
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    uint64_t pollingTimeInUsec = SENSOR_POLLING_TIME * 1000;
    eid_t eid = getEid(nsmDevice);
    do
    {
        sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);

#if false
//place holder: to be implemented once related specification is available
        if (nsmDevice->getEventMode() == GLOBAL_EVENT_GENERATION_ENABLE_POLLING)
        {
            // poll event from device
            co_await pollEvents(eid);
        }
#endif
        // update all priority sensors
        auto& sensors = nsmDevice->prioritySensors;
        for (auto& sensor : sensors)
        {
            co_await sensor->update(*this, eid);
            if (nsmDevice->pollingTimer &&
                !nsmDevice->pollingTimer->isRunning())
            {
                co_return NSM_SW_ERROR;
            }
        }

        // update roundRobin sensors for rest of polling time interval
        auto toBeUpdated = nsmDevice->roundRobinSensors.size();
        do
        {
            if (toBeUpdated == 0)
            {
                break;
            }
            auto sensor = nsmDevice->roundRobinSensors.front();
            nsmDevice->roundRobinSensors.pop_front();
            nsmDevice->roundRobinSensors.push_back(sensor);
            toBeUpdated--;

            co_await sensor->update(*this, eid);
            if (nsmDevice->pollingTimer &&
                !nsmDevice->pollingTimer->isRunning())
            {
                co_return NSM_SW_ERROR;
            }

            sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
        } while ((t1 - t0) >= pollingTimeInUsec);

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
    } while ((t1 - t0) >= pollingTimeInUsec);

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine SensorManager::SendRecvNsmMsg(eid_t eid, Request& request,
                                                   const nsm_msg** responseMsg,
                                                   size_t* responseLen)
{
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    requestMsg->hdr.instance_id = instanceIdDb.next(eid);
    auto rc = co_await requester::SendRecvNsmMsg<RequesterHandler>(
        handler, eid, request, responseMsg, responseLen);
    if (rc)
    {
        lg2::error("SendRecvNsmMsg failed. eid={EID} rc={RC}", "EID", eid, "RC",
                   rc);
    }
    co_return rc;
}

requester::Coroutine SensorManager::pollEvents([[maybe_unused]] eid_t eid)
{
    // placeholder
    co_return NSM_SW_SUCCESS;
}

std::shared_ptr<NsmDevice> SensorManager::getNsmDevice(uuid_t uuid)
{
    return findNsmDeviceByUUID(nsmDevices, uuid);
}

eid_t SensorManager::getEid(std::shared_ptr<NsmDevice> nsmDevice)
{
    return utils::getEidFromUUID(eidTable, nsmDevice->uuid);
}

} // namespace nsm
