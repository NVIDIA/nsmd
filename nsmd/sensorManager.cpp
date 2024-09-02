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
#include "libnsm/requester/mctp.h"

#include "nsmObject.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <chrono>
#include <iostream>

namespace nsm
{

// Static instance definition
std::unique_ptr<SensorManager> SensorManager::instance;

// Ensuring the constructor remains private and defined here if not explicitly
// declared in the header
SensorManagerImpl::SensorManagerImpl(
    sdbusplus::bus::bus& bus, sdeventplus::Event& event,
    requester::Handler<requester::Request>& handler, InstanceIdDb& instanceIdDb,
    sdbusplus::asio::object_server& objServer,
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable,
    NsmDeviceTable& nsmDevices, eid_t localEid,
    mctp_socket::Manager& sockManager, bool verbose) :
    SensorManager(nsmDevices, localEid), bus(bus), event(event),
    handler(handler), instanceIdDb(instanceIdDb), objServer(objServer),
    eidTable(eidTable), sockManager(sockManager), verbose(verbose)
{
    deferScanInventory = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(&SensorManagerImpl::scanInventory, this));
    inventoryAddedSignal = std::make_unique<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded(
            "/xyz/openbmc_project/inventory"),
        [this](sdbusplus::message::message& msg) {
        this->interfaceAddedHandler(msg);
    });

#ifdef NVIDIA_STANDBYTODC
    // helping with telemetry in standby power and transitions between standby
    // and DC power
    gpioStatusPropertyChangedSignal = std::make_unique<sdbusplus::bus::match_t>(
        sdbusplus::bus::match_t(bus,
                                sdbusplus::bus::match::rules::propertiesChanged(
                                    "/xyz/openbmc_project/GpioStatusHandler",
                                    "xyz.openbmc_project.GpioStatus"),
                                [this](sdbusplus::message::message& msg) {
        this->gpioStatusPropertyChangedHandler(msg);
    }));
#endif
}

void SensorManagerImpl::scanInventory()
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
                    if (NsmObjectFactory::instance().isSupported(interface))
                    {
                        queuedAddedInterfaces.emplace(objPath, interface);
                    }
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

    if (interfaceAddedTaskHandle)
    {
        if (!interfaceAddedTaskHandle.done())
        {
            return;
        }
        interfaceAddedTaskHandle.destroy();
    }
    auto co = interfaceAddedTask();
    interfaceAddedTaskHandle = co.handle;
    if (interfaceAddedTaskHandle.done())
    {
        interfaceAddedTaskHandle = nullptr;
    }
}

void SensorManagerImpl::interfaceAddedHandler(sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objPath;
    dbus::InterfaceMap interfaces;

    msg.read(objPath, interfaces);
    for (const auto& [interface, _] : interfaces)
    {
        if (NsmObjectFactory::instance().isSupported(interface))
        {
            queuedAddedInterfaces.emplace(objPath, interface);
        }
    }

    if (interfaceAddedTaskHandle)
    {
        if (!interfaceAddedTaskHandle.done())
        {
            return;
        }
        interfaceAddedTaskHandle.destroy();
    }
    auto co = interfaceAddedTask();
    interfaceAddedTaskHandle = co.handle;
    if (interfaceAddedTaskHandle.done())
    {
        interfaceAddedTaskHandle = nullptr;
    }
}

requester::Coroutine SensorManagerImpl::interfaceAddedTask()
{
    while (!queuedAddedInterfaces.empty())
    {
        auto [objPath, interface] = queuedAddedInterfaces.front();
        queuedAddedInterfaces.pop();

        co_await NsmObjectFactory::instance().createObjects(*this, interface,
                                                            objPath);
    }
    newSensorEvent = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn(&SensorManagerImpl::_startPolling), this,
                         std::placeholders::_1));
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

void SensorManagerImpl::gpioStatusPropertyChangedHandler(
    sdbusplus::message::message& msg)
{
    lg2::debug(
        "SensorManager::gpioStatusPropertyChangedHandler: xyz.openbmc_project.GpioStatus PropertiesChanged signal received.");
    std::string interface;
    std::map<std::string, std::variant<std::string, bool>> properties;
    try
    {
        const std::string propertyName = GPU_PWR_GD_GPIO;
        std::string errStr;
        msg.read(interface, properties);
        auto prop = properties.find(propertyName);
        if (prop == properties.end())
        {
            lg2::error(
                "SensorManager::gpioStatusPropertyChangedHandler: Unable to find property: {PROP}",
                "PROP", propertyName);
            return;
        }

        bool pgood;
        pgood = *std::get_if<bool>(&(prop->second));
        if (pgood == true)
        {
            lg2::info(
                "SensorManager::gpioStatusPropertyChangedHandler: Power transition from standby to DC power detected");

            // Set state to starting for NSM Readiness
            NsmServiceReadyIntf::getInstance().setStateStarting();

            for (auto nsmDevice : nsmDevices)
            {
                // Mark all the round-robin sensors as unrefreshed.
                for (auto sensor : nsmDevice->roundRobinSensors)
                {
                    sensor->isRefreshed = false;
                }

                // Re-queue the static sensors for updation.
                for (auto sensor : nsmDevice->standByToDcRefreshSensors)
                {
                    sensor->isRefreshed = false;
                    nsmDevice->roundRobinSensors.push_back(sensor);
                }

                nsmDevice->isDeviceReady = false;
            }
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        lg2::error(
            "SensorManager::gpioStatusPropertyChangedHandler: Unable to read properties from xyz.openbmc_project.GpioStatus. ERR={ERR}",
            "ERR", e.what());
    }
}

void SensorManagerImpl::checkAllDevicesReady()
{
    for (auto nsmDevice : nsmDevices)
    {
        /*consider only active devices, helps in 2 scenarios
         First only static inventory present.
         Second when a particular device is not responding from starting or not
         present in enumeration etc. For e.g. for hgxb if all 8 gpu's are not
         presnt*/
        if (nsmDevice->isDeviceActive && !nsmDevice->isDeviceReady)
        {
            return;
        }
    }
    lg2::info(
        "SensorManager::checkAllDevices Every Device Checked and Ready. Setting ServiceReady.State to enabled.");
    NsmServiceReadyIntf::getInstance().setStateEnabled();
}

void SensorManagerImpl::_startPolling(
    sdeventplus::source::EventBase& /* source */)
{
    newSensorEvent.reset();
    startPolling();
}

void SensorManagerImpl::startPolling(uuid_t uuid)
{
    auto nsmDevice = getNsmDevice(uuid);
    if (nsmDevice)
    {
        if (!nsmDevice->pollingTimer)
        {
            nsmDevice->pollingTimer = std::make_unique<sdbusplus::Timer>(
                event.get(), std::bind_front(&SensorManagerImpl::doPolling,
                                             this, nsmDevice));
        }

        if (!(nsmDevice->pollingTimer->isRunning()))
        {
            nsmDevice->pollingTimer->start(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::milliseconds(SENSOR_POLLING_TIME)),
                true);
        }

        if (!nsmDevice->pollingTimerLongRunning)
        {
            nsmDevice->pollingTimerLongRunning =
                std::make_unique<sdbusplus::Timer>(
                    event.get(),
                    std::bind_front(&SensorManagerImpl::doPollingLongRunning,
                                    this, nsmDevice));
        }

        if (!(nsmDevice->pollingTimerLongRunning->isRunning()))
        {
            nsmDevice->pollingTimerLongRunning->start(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::milliseconds(
                        SENSOR_POLLING_TIME_LONG_RUNNING)),
                true);
        }
    }
}

void SensorManagerImpl::startPolling()
{
    for (auto& nsmDevice : nsmDevices)
    {
        if (!nsmDevice->pollingTimer)
        {
            nsmDevice->pollingTimer = std::make_unique<sdbusplus::Timer>(
                event.get(), std::bind_front(&SensorManagerImpl::doPolling,
                                             this, nsmDevice));
        }

        if (!(nsmDevice->pollingTimer->isRunning()))
        {
            nsmDevice->pollingTimer->start(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::milliseconds(SENSOR_POLLING_TIME)),
                true);
        }

        if (!nsmDevice->pollingTimerLongRunning)
        {
            nsmDevice->pollingTimerLongRunning =
                std::make_unique<sdbusplus::Timer>(
                    event.get(),
                    std::bind_front(&SensorManagerImpl::doPollingLongRunning,
                                    this, nsmDevice));
        }

        if (!(nsmDevice->pollingTimerLongRunning->isRunning()))
        {
            nsmDevice->pollingTimerLongRunning->start(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::milliseconds(
                        SENSOR_POLLING_TIME_LONG_RUNNING)),
                true);
        }
    }
}

void SensorManagerImpl::stopPolling(uuid_t uuid)
{
    auto nsmDevice = getNsmDevice(uuid);
    if (nsmDevice)
    {
        nsmDevice->pollingTimer->stop();
        nsmDevice->pollingTimerLongRunning->stop();
    }
}

void SensorManagerImpl::stopPolling()
{
    for (auto& nsmDevice : nsmDevices)
    {
        nsmDevice->pollingTimer->stop();
        nsmDevice->pollingTimerLongRunning->stop();
    }
}

void SensorManagerImpl::doPolling(std::shared_ptr<NsmDevice> nsmDevice)
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

void SensorManagerImpl::doPollingLongRunning(
    std::shared_ptr<NsmDevice> nsmDevice)
{
    if (nsmDevice->doPollingTaskHandleLongRunning)
    {
        if (!(nsmDevice->doPollingTaskHandleLongRunning.done()))
        {
            return;
        }
        nsmDevice->doPollingTaskHandleLongRunning.destroy();
    }

    auto co = doPollingTaskLongRunning(nsmDevice);
    nsmDevice->doPollingTaskHandleLongRunning = co.handle;
    if (nsmDevice->doPollingTaskHandleLongRunning.done())
    {
        nsmDevice->doPollingTaskHandleLongRunning = nullptr;
    }
}

requester::Coroutine SensorManagerImpl::doPollingTaskLongRunning(
    std::shared_ptr<NsmDevice> nsmDevice)
{
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    uint64_t pollingTimeInUsec = SENSOR_POLLING_TIME_LONG_RUNNING * 1000;
    eid_t eid = getEid(nsmDevice);
    do
    {
        sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);

        auto& sensors = nsmDevice->longRunningSensors;

        size_t sensorIndex{0};
        while (sensorIndex < sensors.size())
        {
            auto sensor = sensors[sensorIndex];
            co_await sensor->update(*this, eid);
            if (nsmDevice->pollingTimerLongRunning &&
                !nsmDevice->pollingTimerLongRunning->isRunning())
            {
                co_return NSM_SW_ERROR;
            }

            ++sensorIndex;
        }

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
    } while ((t1 - t0) >= pollingTimeInUsec);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    SensorManagerImpl::doPollingTask(std::shared_ptr<NsmDevice> nsmDevice)
{
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    uint64_t pollingTimeInUsec = SENSOR_POLLING_TIME * 1000;

    do
    {
        sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);

        if (!nsmDevice->isDeviceActive)
        {
            /*lg2::error(
                "SensorManager::doPollingTask : skip polling due to inactive
               device, deviceType:{DEVTYPE} InstanceNumber:{INSTNUM}",
                "DEVTYPE", nsmDevice->getDeviceType(), "INSTNUM",
                nsmDevice->getInstanceNumber());*/
            co_return NSM_ERR_NOT_READY;
        }

        eid_t eid = getEid(nsmDevice);
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

        // Insertion into container NsmDevice::prioritySensors, triggered by
        // Configuration PDI added event, may invalidate container's
        // iterators and hence using index based element access.
        size_t sensorIndex{0};
        while (sensorIndex < sensors.size())
        {
            auto sensor = sensors[sensorIndex];
            co_await sensor->update(*this, eid);
            if (nsmDevice->pollingTimer &&
                !nsmDevice->pollingTimer->isRunning())
            {
                co_return NSM_SW_ERROR;
            }

            ++sensorIndex;
        }

        // update roundRobin sensors for rest of polling time interval
        auto toBeUpdated = nsmDevice->roundRobinSensors.size();
        do
        {
            if (!toBeUpdated)
            {
                // Either we were able to succesfully update all sensors in one
                // iteration or there are no sensors in the queue. Mark ready in
                // both cases.
                if (!nsmDevice->isDeviceReady)
                {
                    nsmDevice->isDeviceReady = true;
                    checkAllDevicesReady();
                }
                break;
            }

            auto sensor = nsmDevice->roundRobinSensors.front();
            nsmDevice->roundRobinSensors.pop_front();
            toBeUpdated--;

            // ServiceReady Logic:
            // The round-robin queue is circular hence encountering the first
            // refreshed sensor marks a "complete iteration" of the queue.
            if (!nsmDevice->isDeviceReady && sensor->isRefreshed)
            {
                // The Device isn't ready but we have found our first
                // refreshed sensor. Mark the device ready.
                nsmDevice->isDeviceReady = true;
                checkAllDevicesReady();
            }

            auto cc = co_await sensor->update(*this, eid);
            sensor->isRefreshed = true;

            if (!(sensor->isStatic && cc == NSM_SUCCESS))
            {
                // Filter out succesfully updated static sensor.
                // Only re-queue non-static sensors or static sensors that
                // failed to update succesfully.
                nsmDevice->roundRobinSensors.push_back(sensor);
            }

            if (nsmDevice->pollingTimer &&
                !nsmDevice->pollingTimer->isRunning())
            {
                co_return NSM_SW_ERROR;
            }

            sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
        } while ((t1 - t0) < pollingTimeInUsec);

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
    } while ((t1 - t0) >= pollingTimeInUsec);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine SensorManagerImpl::SendRecvNsmMsg(
    eid_t eid, Request& request, std::shared_ptr<const nsm_msg>& responseMsg,
    size_t& responseLen, bool isLongRunning)
{
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    uint8_t messageType = requestMsg->hdr.nvidia_msg_type;
    uint8_t commandCode = requestMsg->payload[0];

    auto uuid = utils::getUUIDFromEID(eidTable, eid);
    if (!uuid)
    {
        lg2::error(
            "SensorManager::SendRecvNsmMsg  : No UUID found for EID {EID}",
            "EID", eid);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto nsmDevice = getNsmDevice(*uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "SensorManager::SendRecvNsmMsg : No nsmDevice found for eid={EID} , uuid={UUID}",
            "EID", eid, "UUID", *uuid);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    if (!nsmDevice->isDeviceActive ||
        !nsmDevice->isCommandSupported(messageType, commandCode))
    {
        co_return NSM_ERR_UNSUPPORTED_COMMAND_CODE;
    }

    const nsm_msg* response = nullptr;
    auto rc = co_await requester::SendRecvNsmMsg<RequesterHandler>(
        handler, eid, request, &response, &responseLen, isLongRunning);
    responseMsg = std::shared_ptr<const nsm_msg>(response, [](auto) {
    }); // the memory is allocated and free at sock_handler.cpp
    // NSM_SW_ERROR_NULL: indicates no nsm response which is possible for
    // request that timedout
    if (rc && rc != NSM_SW_ERROR_NULL)
    {
        lg2::error("SendRecvNsmMsg failed. eid={EID} rc={RC}", "EID", eid, "RC",
                   rc);
    }
    co_return rc;
}

requester::Coroutine SensorManagerImpl::pollEvents([[maybe_unused]] eid_t eid)
{
    // placeholder
    co_return NSM_SW_SUCCESS;
}

std::shared_ptr<NsmDevice> SensorManager::getNsmDevice(uuid_t uuid)
{
    auto nsmDevice = findNsmDeviceByUUID(nsmDevices, uuid);
    if (!nsmDevice)
    {
        // check if the uuid is in static inventory format.
        uint8_t deviceType = 0xff;
        uint8_t instanceNumber = 0xff;
        if (parseStaticUuid(uuid, deviceType, instanceNumber) < 0)
        {
            throw std::runtime_error(
                "SensorManager::getNsmDevice: uuid in EM json is not in a valid format(STATIC:d:d), UUID=" +
                uuid);
        }

        nsmDevice = findNsmDeviceByIdentification(nsmDevices, deviceType,
                                                  instanceNumber);
        if (!nsmDevice)
        {
            // create nsmDevice
            nsmDevice = std::make_shared<NsmDevice>(deviceType, instanceNumber);
            nsmDevices.emplace_back(nsmDevice);
            nsmDevice->isDeviceActive = false;
        }
    }
    return nsmDevice;
}

eid_t SensorManagerImpl::getEid(std::shared_ptr<NsmDevice> nsmDevice)
{
    return utils::getEidFromUUID(eidTable, nsmDevice->uuid);
}

uint8_t SensorManagerImpl::SendRecvNsmMsgSync(
    eid_t eid, Request& request, std::shared_ptr<const nsm_msg>& responseMsg,
    size_t& responseLen)
{
    auto mctpFd = sockManager.getSocket(eid);
    uint8_t rc = NSM_REQUESTER_SUCCESS;

    const nsm_msg* response = nullptr;
    // check if there is request in progress.
    while (handler.hasInProgressRequest(eid))
    {
        lg2::info(
            "SendRecvNsmMsgSync: waiting response for in progress request. EID={EID}",
            "EID", eid);
        // waiting for response
        while (1)
        {
            uint8_t tag;
            rc = nsm_recv_any(eid, mctpFd, (uint8_t**)&response, &responseLen,
                              &tag);
            if (rc == (uint8_t)NSM_REQUESTER_EID_MISMATCH)
            {
                lg2::info(
                    "SendRecvNsmMsgSync: received response but not from EID={EID}",
                    "EID", eid);
                continue;
            }

            handler.invalidInProgressRequest(eid, tag);
            if (rc == NSM_REQUESTER_SUCCESS)
            {
                lg2::info(
                    "SendRecvNsmMsgSync: received response and discard it. EID={EID}",
                    "EID", eid);
                // discard the response
                free((void*)response);
                response = nullptr;
                break;
            }
            else
            {
                lg2::error(
                    "SendRecvNsmMsgSync: nsm_recv_any failed RC={RC} EID={EID}",
                    "RC", rc, "EID", eid);
                break;
            }
        }
    }

    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    uint8_t messageType = requestMsg->hdr.nvidia_msg_type;
    uint8_t commandCode = requestMsg->payload[0];

    auto uuid = utils::getUUIDFromEID(eidTable, eid);
    if (!uuid)
    {
        lg2::error("SendRecvNsmMsgSync: No UUID found for EID {EID}", "EID",
                   eid);
        return NSM_ERROR;
    }

    auto nsmDevice = getNsmDevice(*uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "SendRecvNsmMsgSync: No nsmDevice found for eid={EID} , uuid={UUID}",
            "EID", eid, "UUID", *uuid);
        return NSM_ERROR;
    }

    if (!nsmDevice->isDeviceActive ||
        !nsmDevice->isCommandSupported(messageType, commandCode))
    {
        return NSM_ERR_UNSUPPORTED_COMMAND_CODE;
    }

    // if it is supported then only assign instance_id
    requestMsg->hdr.instance_id = instanceIdDb.next(eid);

    if (verbose)
    {
        utils::printBuffer(utils::Tx, request);
    }

    rc = nsm_send_recv(eid, mctpFd, request.data(), request.size(),
                       (uint8_t**)&response, &responseLen);
    responseMsg = std::shared_ptr<const nsm_msg>(response,
                                                 [](const nsm_msg* ptr) {
        free((void*)ptr);
    }); // the memory is allocated at mctp_recv API of /libnsm/requester/mctp.c
        // and should be free after that

    if (rc)
    {
        lg2::error(
            "SendRecvNsmMsgSync: nsm_send_recv failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        responseLen = 0;
    }

    if (verbose && rc == NSM_REQUESTER_SUCCESS)
    {
        utils::printBuffer(utils::Rx, response, responseLen);
    }

    instanceIdDb.free(eid, requestMsg->hdr.instance_id);
    return rc;
}

} // namespace nsm
