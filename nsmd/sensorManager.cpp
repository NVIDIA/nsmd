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

#include "common/sleep.hpp"
#include "nsmObject.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

namespace nsm
{

// Static instance definition
std::unique_ptr<SensorManager> SensorManager::instance;
bool SensorManagerImpl::isReadyForReadinessCheck = false;
bool SensorManagerImpl::isMCTPReadyCheck = false;
bool SensorManagerImpl::isEMReadyCheck = false;
std::map<std::string, std::string> SensorManagerImpl::readynessFailureMap = {};

// Ensuring the constructor remains private and defined here if not
// explicitly declared in the header
SensorManagerImpl::SensorManagerImpl(
    sdbusplus::bus::bus& bus, sdeventplus::Event& event,
    requester::Handler<requester::Request>& handler, InstanceIdDb& instanceIdDb,
    sdbusplus::asio::object_server& objServer,
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable,
    NsmDeviceTable& nsmDevices, eid_t localEid,
    mctp_socket::Manager& sockManager, bool verbose) :
    SensorManager(nsmDevices, localEid),
    bus(bus), event(event), handler(handler), instanceIdDb(instanceIdDb),
    objServer(objServer), eidTable(eidTable), sockManager(sockManager),
    verbose(verbose)
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
    mctpReadinessSigMatch =
        std::make_unique<sdbusplus::bus::match_t>(sdbusplus::bus::match_t(
            bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                "/xyz/openbmc_project/state/configurableStateManager/MCTP",
                "xyz.openbmc_project.State.FeatureReady"),
            [this](sdbusplus::message::message& msg) {
        this->mctpReadinessSigHandler(msg);
    }));
    isMCTPReady();
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

void SensorManagerImpl::mctpReadinessSigHandler(
    sdbusplus::message::message& msg)
{
    lg2::info(
        "SensorManager::mctpReadinessSigHandler: xyz.openbmc_project.State.FeatureReady PropertiesChanged signal received.");
    std::string interface;
    std::map<std::string, std::variant<std::string, bool>> properties;
    try
    {
        const std::string propertyName = "State";
        std::string errStr;
        msg.read(interface, properties);
        auto prop = properties.find(propertyName);
        if (prop == properties.end())
        {
            lg2::error(
                "SensorManager::mctpReadinessSigHandler: Unable to find property: {PROP}",
                "PROP", propertyName);
            return;
        }

        std::string state;
        state = *std::get_if<std::string>(&(prop->second));
        if (state == "xyz.openbmc_project.State.FeatureReady.States.Enabled")
        {
            isMCTPReadyCheck = true;
            lg2::info("isMCTPReadyCheck::true");
            readynessFailureMap["isMCTPReady"] = "True";
            isNSMPollReady();
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        lg2::error(
            "SensorManager::mctpReadinessSigHandler: Unable to read properties from xyz.openbmc_project.State.FeatureReady. ERR={ERR}",
            "ERR", e.what());
        readynessFailureMap["isMCTPReady"] =
            "SensorManager::mctpReadinessSigHandler ";
        readynessFailureMap["isMCTPReady"] += e.what();
    }
}

bool SensorManagerImpl::isEMReady()
{
    lg2::info("isEMReady Enter");
    constexpr auto emService = "xyz.openbmc_project.EntityManager";
    constexpr auto nsmReadinesspath =
        "/xyz/openbmc_project/inventory/system/chassis/NSM_Readiness/NSM_Poll_Readyness";
    constexpr auto DBUS_PROPERTY_IFACE = "org.freedesktop.DBus.Properties";
    constexpr auto ifaceName =
        "xyz.openbmc_project.Configuration.NSM_Poll_Ready";
    auto& bus = utils::DBusHandler::getBus();

    std::variant<std::string> property;
    bool ready = false;

    try
    {
        auto method = bus.new_method_call(emService, nsmReadinesspath,
                                          DBUS_PROPERTY_IFACE, "Get");
        method.append(ifaceName, "Status");
        using namespace std::chrono_literals;
        sdbusplus::SdBusDuration timeout{2s};

        auto reply = bus.call(method, timeout);
        if (reply.is_method_error())
        {
            readynessFailureMap["isEMReady"] =
                "Failed reading Status DBus property";
            return false;
        }
        reply.read(property);
        auto value = std::get<std::string>(property);

        if (value == "Enabled")
        {
            ready = true;
        }
    }
    catch (const std::exception& e)
    {
        readynessFailureMap["isEMReady"] = e.what();
        lg2::error(
            "SensorManagerImpl::isEMReady: Unable to read properties from xyz.openbmc_project.Configuration.NSM_Poll_Ready. ERR={ERR}",
            "ERR", e.what());
        ready = false;
    }
    // if em is ready already skip check
    if (!isEMReadyCheck)
    {
        isEMReadyCheck = ready;
        if (isEMReadyCheck)
        {
            markEMReady();
        }
    }
    lg2::info("isEMReadyCheck {CHECK}", "CHECK", isEMReadyCheck);
    lg2::info("isEMReady Exit");
    return ready;
}

void SensorManagerImpl::markEMReady()
{
    lg2::info("isEMReady : True");
    readynessFailureMap["isEMReady"] = "True";
    isNSMPollReady();
}

bool SensorManagerImpl::isMCTPReady()
{
    lg2::info("isMCTPReady Enter");
    constexpr auto csmService =
        "xyz.openbmc_project.State.ConfigurableStateManager";
    constexpr auto mctpReadinesspath =
        "/xyz/openbmc_project/state/configurableStateManager/MCTP";
    constexpr auto DBUS_PROPERTY_IFACE = "org.freedesktop.DBus.Properties";
    constexpr auto ifaceName = "xyz.openbmc_project.State.FeatureReady";
    auto& bus = utils::DBusHandler::getBus();
    bool ready = false;

    std::variant<std::string> property;

    try
    {
        auto method = bus.new_method_call(csmService, mctpReadinesspath,
                                          DBUS_PROPERTY_IFACE, "Get");
        method.append(ifaceName, "State");
        using namespace std::chrono_literals;
        sdbusplus::SdBusDuration timeout{2s};
        auto reply = bus.call(method, timeout);
        if (reply.is_method_error())
        {
            readynessFailureMap["isMCTPReady"] =
                "Failed reading State DBus property";
            return false;
        }
        reply.read(property);
        auto value = std::get<std::string>(property);
        readynessFailureMap["isMCTPReady"] = value;
        if (value == "xyz.openbmc_project.State.FeatureReady.States.Enabled")
        {
            ready = true;
        }
    }
    catch (const std::exception& e)
    {
        ready = false;
        lg2::error(
            "SensorManagerImpl::isMCTPReady: Unable to read properties from xyz.openbmc_project.State.FeatureReady. ERR={ERR}",
            "ERR", e.what());
        readynessFailureMap["isMCTPReady"] = e.what();
    }

    // if MCTP is ready already skip check
    if (!isMCTPReadyCheck)
    {
        isMCTPReadyCheck = ready;
        if (isMCTPReadyCheck)
        {
            lg2::info("isMCTPReadyCheck : True");
            readynessFailureMap["isMCTPReadyCheck"] = "True";
            isNSMPollReady();
        }
    }
    lg2::info("isMCTPReadyCheck {CHECK}", "CHECK", isMCTPReadyCheck);
    lg2::info("isMCTPReady Exit");
    return false;
}

bool SensorManagerImpl::isNSMPollReady()
{
    lg2::info("isNSMPollReady Enter");
    if (!isReadyForReadinessCheck)
    {
        isReadyForReadinessCheck = isMCTPReadyCheck && isEMReadyCheck;
        if (isReadyForReadinessCheck)
        {
            lg2::info("isNSMPollReady : True");
            readynessFailureMap["isNSMPollReady"] = "true";
        }
    }
    lg2::info("isReadyForReadinessCheck {CHECK}", "CHECK",
              isReadyForReadinessCheck);
    lg2::info("isNSMPollReady Exit");
    return isReadyForReadinessCheck;
}

void SensorManagerImpl::dumpReadinessLogs()
{
    lg2::error("******dumpReadinesLogs Start*****");
    for (std::map<std::string, std::string>::const_iterator it =
             readynessFailureMap.begin();
         it != readynessFailureMap.end(); ++it)
    {
        std::string fname = it->first;
        std::string flog = it->second;
        lg2::error("dumpReadinessLogs {FNAME}: {LOGMSG}", "FNAME", fname,
                   "LOGMSG", flog);
    }
    lg2::error("******dumpReadinesLogs End*****");
}

/*
This is called only when device is not ready
 */
void SensorManagerImpl::checkAllDevicesReady()
{
    for (auto nsmDevice : nsmDevices)
    {
        /*consider only active devices, helps in 2 scenarios
         First only static inventory present.
         Second when a particular device is not responding from starting or not
         present in enumeration etc. For e.g. for hgxb if all 8 gpu's are not
         presnt*/
        if (nsmDevice->isDeviceActive && !nsmDevice->isDeviceReady &&
            isReadyForReadinessCheck)
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
        nsmDevice->stopPolling = false;
        doPolling(nsmDevice);
        doPollingLongRunning(nsmDevice);
    }
}

void SensorManagerImpl::startPolling()
{
    for (auto& nsmDevice : nsmDevices)
    {
        nsmDevice->stopPolling = false;
        doPolling(nsmDevice);
        doPollingLongRunning(nsmDevice);
    }
}

void SensorManagerImpl::stopPolling(uuid_t uuid)
{
    auto nsmDevice = getNsmDevice(uuid);
    if (nsmDevice)
    {
        nsmDevice->stopPolling = true;
    }
}

void SensorManagerImpl::stopPolling()
{
    for (auto& nsmDevice : nsmDevices)
    {
        nsmDevice->stopPolling = true;
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
    uint64_t inActiveSleepTimeInUsec = INACTIVE_SLEEP_TIME_IN_MS * 1000;
    uint64_t pollingTimeInUsec = SENSOR_POLLING_TIME_LONG_RUNNING * 1000;

    do
    {
        if (!nsmDevice->isDeviceActive)
        {
            // Sleep. Wait for the device to get active.
            co_await common::Sleep(event, inActiveSleepTimeInUsec,
                                   common::Priority);
            continue;
        }

        eid_t eid = getEid(nsmDevice);
        auto& sensors = nsmDevice->longRunningSensors;
        size_t sensorIndex{0};

        while (sensorIndex < sensors.size())
        {
            auto sensor = sensors[sensorIndex];
            co_await sensor->update(*this, eid);
            if (nsmDevice->stopPolling)
            {
                // coverity[missing_return]
                co_return NSM_SW_ERROR;
            }

            ++sensorIndex;
        }

        co_await common::Sleep(
            event, pollingTimeInUsec,
            common::NonPriority); // The timer for long running commands can
                                  // have a normal priority
    } while (true);

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    SensorManagerImpl::doPollingTask(std::shared_ptr<NsmDevice> nsmDevice)
{
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    /**
     * @brief The maximum allowable deviation in microseconds from the desired
     * polling interval. If the difference between the polling time and the
     * elapsed time is less than this buffer, the system will skip the sleep
     * operation and proceed with the next polling cycle, ensuring minimal
     * delay.
     */
    uint64_t allowedBufferInUsec = ALLOWED_BUFFER_IN_MS * 1000;
    uint64_t inActiveSleepTimeInUsec = INACTIVE_SLEEP_TIME_IN_MS * 1000;
    uint64_t pollingTimeInUsec = SENSOR_POLLING_TIME * 1000;

    do
    {
        auto timerEventPriority = common::Priority;

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);

        if (!nsmDevice->isDeviceActive)
        {
            // Sleep. Wait for the device to get active.
            co_await common::Sleep(event, inActiveSleepTimeInUsec,
                                   common::Priority);
            continue;
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
        const size_t prioritySensorCount = sensors.size();

        // Insertion into container NsmDevice::prioritySensors, triggered by
        // Configuration PDI added event, may invalidate container's
        // iterators and hence using index based element access.
        size_t sensorIndex{0};

        while (sensorIndex < prioritySensorCount)
        {
            auto sensor = sensors[sensorIndex];
            co_await sensor->update(*this, eid);
            if (nsmDevice->stopPolling)
            {
                // coverity[missing_return]
                co_return NSM_SW_ERROR;
            }

            ++sensorIndex;
        }

        // update roundRobin sensors for rest of polling time interval
        auto toBeUpdated = nsmDevice->roundRobinSensors.size();

        // Make sure the first round-robin sensor is not compared
        // to an uninitialised timestamp
        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);

        do
        {
            if (!toBeUpdated)
            {
                // Either we were able to succesfully update all sensors in one
                // iteration or there are no sensors in the queue. Mark ready in
                // both cases.
                if (!nsmDevice->isDeviceReady && isReadyForReadinessCheck)
                {
                    nsmDevice->isDeviceReady = true;
                    checkAllDevicesReady();
                }
                break;
            }

            auto sensor = nsmDevice->roundRobinSensors.front();

            nsmDevice->roundRobinSensors.pop_front();
            toBeUpdated--;

            if (!sensor->needsUpdate(t1))
            {
                // Skip the RR-sensor
                nsmDevice->roundRobinSensors.push_back(sensor);
                continue;
            }

            // ServiceReady Logic:
            // The round-robin queue is circular hence encountering the first
            // refreshed sensor marks a "complete iteration" of the queue.
            if (!nsmDevice->isDeviceReady && sensor->isRefreshed &&
                isReadyForReadinessCheck)
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

            if (nsmDevice->stopPolling)
            {
                // coverity[missing_return]
                co_return NSM_SW_ERROR;
            }

            sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
            sensor->setLastUpdatedTimeStamp(t1);

        } while ((t1 - t0) < pollingTimeInUsec);

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);

        if (prioritySensorCount == 0)
        {
            // The timer event for devices with no priority sensors can be
            // of low priority.
            timerEventPriority = common::NonPriority;
        }

        uint64_t diff = t1 - t0;
        if (diff > pollingTimeInUsec)
        {
            // We have already crossed the polling interval. Don't sleep
            continue;
        }

        uint64_t sleepDeltaInUsec = pollingTimeInUsec - diff;
        if (sleepDeltaInUsec < allowedBufferInUsec)
        {
            // If the delta is within the allowed buffer, we can skip sleeping
            // and continue polling.
            continue;
        }
        co_await common::Sleep(event, sleepDeltaInUsec, timerEventPriority);

    } while (true);

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine SensorManagerImpl::SendRecvNsmMsg(
    eid_t eid, Request& request, std::shared_ptr<const nsm_msg>& responseMsg,
    size_t& responseLen)
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
        // coverity[missing_return]
        co_return NSM_ERR_UNSUPPORTED_COMMAND_CODE;
    }

    const nsm_msg* response = nullptr;
    auto rc = co_await requester::SendRecvNsmMsg<RequesterHandler>(
        handler, eid, request, &response, &responseLen);
    responseMsg = std::shared_ptr<const nsm_msg>(response, [](auto) {
    }); // the memory is allocated and free at sock_handler.cpp
    // NSM_SW_ERROR_NULL: indicates no nsm response which is possible for
    // request that timedout
    if (rc && rc != NSM_SW_ERROR_NULL)
    {
        lg2::error("SendRecvNsmMsg failed. eid={EID} rc={RC}", "EID", eid, "RC",
                   rc);
    }
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine SensorManagerImpl::pollEvents([[maybe_unused]] eid_t eid)
{
    // placeholder
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

std::shared_ptr<NsmDevice> SensorManager::getNsmDevice(uint8_t deviceType,
                                                       uint8_t instanceNumber)
{
    return findNsmDeviceByIdentification(nsmDevices, deviceType,
                                         instanceNumber);
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

} // namespace nsm
