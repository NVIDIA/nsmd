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
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorQueueMap.hpp"
#include "utils.hpp"

#include <ranges>

#ifdef LTTNG_TRACING
#include "tracepoints/nsmd-tp.h"
#endif

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
    SensorManager(nsmDevices, localEid), bus(bus), event(event),
    handler(handler), instanceIdDb(instanceIdDb), objServer(objServer),
    eidTable(eidTable), sockManager(sockManager), verbose(verbose)
{
    deferScanInventory = std::make_unique<sdeventplus::source::Defer>(
        event,
        [this](sdeventplus::source::EventBase&) { this->scanInventory(); });
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

SensorManagerImpl::~SensorManagerImpl()
{
    pollingIsRunning = false;
}

void SensorManagerImpl::scanInventory()
{
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

    requester::Coroutine::assign(interfaceAddedTaskHandle,
                                 [&]() -> requester::Coroutine {
        // coverity[missing_return]
        co_return co_await interfaceAddedTask();
    });
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

    requester::Coroutine::assign(interfaceAddedTaskHandle,
                                 [&]() -> requester::Coroutine {
        // coverity[missing_return]
        co_return co_await interfaceAddedTask();
    });
}

requester::Coroutine SensorManagerImpl::interfaceAddedTask()
{
    while (!queuedAddedInterfaces.empty())
    {
        const auto& firstAddedInterface = queuedAddedInterfaces.front();
        const auto& objPath = firstAddedInterface.first;
        const auto& interface = firstAddedInterface.second;
        queuedAddedInterfaces.pop();

        co_await NsmObjectFactory::instance().createObjects(*this, interface,
                                                            objPath);
    }
    newSensorEvent = std::make_unique<sdeventplus::source::Defer>(
        event, [&](sdeventplus::source::EventBase&) { startPolling(); });
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
                // TODO: check with @aishwaryj
                for (auto sensor : nsmDevice->getRoundRobinSensors())
                {
                    sensor->isRefreshed = false;
                }

                // Re-queue the static sensors for updation.
                for (auto sensor : nsmDevice->getStandByToDcRefreshSensors())
                {
                    sensor->isRefreshed = false;
                    nsmDevice->getStaticSensors().push(sensor);
                }

                nsmDevice->changeDeviceReadyState(false);
            }
            checkAllDevicesReady();
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
    bool allDevicesReady = true;
    // Checking only active devices copied on start of polling loop
    for (auto nsmDevice : nsmDevices)
    {
        /*consider only active devices, helps in 2 scenarios
         First only static inventory present.
         Second when a particular device is not responding from starting or not
         present in enumeration etc. For e.g. for hgxb if all 8 gpu's are not
         presnt*/

        if (nsmDevice->shouldLog("checkAllDevicesReady", nsmDevice->isReady()))
        {
            lg2::info(
                "checkAllDevicesReady: EID: {EID}, type: {DT}, role: {ROLE}, instance: {INST}, ready: {READY}, readiness check: {CHECK}",
                "EID", nsmDevice->getEid(), "DT", nsmDevice->getDeviceType(),
                "ROLE", nsmDevice->getDeviceRole(), "INST",
                nsmDevice->getInstanceNumber(), "READY", nsmDevice->isReady(),
                "CHECK", isReadyForReadinessCheck);
        }
        if (nsmDevice->isOnline() && !nsmDevice->isReady())
        {
            allDevicesReady = false;
        }
    }
    if (!NsmServiceReadyIntf::getInstance().isStateEnabled() &&
        isReadyForReadinessCheck && allDevicesReady)
    {
        lg2::info(
            "SensorManager::checkAllDevices Every Device Checked and Ready. Setting ServiceReady.State to enabled.");
        NsmServiceReadyIntf::getInstance().setStateEnabled();
    }
}

void SensorManagerImpl::startPolling()
{
    pollingIsRunning = true;
    newSensorEvent.reset();
    for (auto& nsmDevice : nsmDevices)
    {
        if (nsmDevice->task.done())
        {
            lg2::info("Starting device task for nsmDevice({DT},{INST},{ROLE})",
                      "DT", nsmDevice->getDeviceType(), "INST",
                      nsmDevice->getInstanceNumber(), "ROLE",
                      nsmDevice->getDeviceRole());
            nsmDevice->task = deviceTask(nsmDevice);
        }
    }
}

requester::Coroutine SensorManagerImpl::updateLongRunningSensor(
    std::shared_ptr<NsmDevice> nsmDevice, std::shared_ptr<NsmObject> sensor,
    std::shared_ptr<LimitedSensorQueue> sensors)
{
    uint64_t t1 = 0;
    co_await sensor->update(nsmDevice);
    sensor->isRefreshed = true;
    sensors->next();

    sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
    sensor->setLastUpdatedTimeStamp(t1);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine SensorManagerImpl::refreshCommandMatrix(
    std::shared_ptr<NsmDevice> nsmDevice)
{
    auto rc = co_await nsmDevice->refreshCommandMatrix();
    if (nsmDevice->shouldLog("Failed to refresh command matrix",
                             nsm_sw_codes(rc)))
    {
        LG2_ERROR("Failed to refresh command matrix, rc={RC}, eid={EID}", "RC",
                  rc, "EID", nsmDevice->getEid());
    }
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine
    SensorManagerImpl::pollPrioritySensors(std::shared_ptr<NsmDevice> nsmDevice)
{
#ifdef LTTNG_TRACING
    lttng_ust_tracepoint(nsmd, priority_polling_started, nsmDevice->getEid());
#endif
    LimitedSensorQueue sensors(nsmDevice->getPrioritySensors());
    while (sensors.hasSensorsToUpdate())
    {
        co_await sensors.current()->update(nsmDevice);
        sensors.next();
    }
#ifdef LTTNG_TRACING
    lttng_ust_tracepoint(nsmd, priority_polling_ended, nsmDevice->getEid());
#endif
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

/**
 * @brief The maximum allowable deviation in microseconds from the desired
 * polling interval. If the difference between the polling time and the
 * elapsed time is less than this buffer, the system will skip the sleep
 * operation and proceed with the next polling cycle, ensuring minimal
 * delay.
 */
static const uint64_t allowedBufferInUsec = ALLOWED_BUFFER_IN_MS * 1000;

/**
 * @brief The desired polling interval in microseconds. This is the time
 * between each polling cycle.
 */
static const uint64_t pollingTimeInUsec = SENSOR_POLLING_TIME * 1000;

requester::Coroutine SensorManagerImpl::pollNonPrioritySensors(
    std::shared_ptr<NsmDevice> nsmDevice, uint64_t t0)
{
    uint64_t t1 = 0;
    auto longRunningQueue = std::make_shared<LimitedSensorQueue>(
        nsmDevice->getLongRunningSensors());
    SensorQueueMap sensors = SensorQueueUnorderedMap({
        {PollingType::GpuPerformanceMonitoring,
         std::make_shared<LimitedSensorQueue>(nsmDevice->getGpmSensors())},
        {PollingType::LongRunning, longRunningQueue},
        {PollingType::Static,
         std::make_shared<LimitedSensorQueue>(nsmDevice->getStaticSensors())},
        {PollingType::RoundRobin, std::make_shared<LimitedSensorQueue>(
                                      nsmDevice->getRoundRobinSensors())},
    });

    PollingType pollingType;
    // Start polling sensors state machine
    sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
    while (sensors.hasSensorsToUpdate() &&
           (t1 - t0) < (pollingTimeInUsec - allowedBufferInUsec))
    {
        pollingType = nsmDevice->getNonPriorityPollingType();
        // Change state machine polling type to next sensor type
        sensors.nextSensorState(nsmDevice->getNonPriorityPollingType());

        auto& pollingSensors = *sensors[pollingType];
        if (!pollingSensors.hasSensorsToUpdate())
        {
            sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
            continue;
        }

        auto sensor = pollingSensors.current();
        // Move to next sensor in the queue
        pollingSensors.next();

        // ServiceReady Logic:
        // The sensor queue is circular hence encountering the first
        // refreshed sensor marks a "complete iteration" of the queue.
        if (pollingType == PollingType::RoundRobin && !nsmDevice->isReady() &&
            sensor->isRefreshed)
        {
            // The Device isn't ready but we have found our first
            // refreshed sensor. Mark the device ready.
            nsmDevice->changeDeviceReadyState(true);
            checkAllDevicesReady();
        }

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
        if (!sensor->needsUpdate(t1))
        {
            // Skip to next sensor
            continue;
        }

        if (pollingType == PollingType::LongRunning)
        {
            // Assign new coroutine for long running sensor if there is no
            // other coroutine running for it

            if (nsmDevice->longRunningTask.done())
            {
                nsmDevice->longRunningTask = updateLongRunningSensor(
                    nsmDevice, sensor, longRunningQueue);
            }
            // Skip to next sensor
            continue;
        }

        // Update sensor
        auto cc = co_await sensor->update(nsmDevice);
        sensor->isRefreshed = true;

        if (pollingType == PollingType::Static && cc == NSM_SUCCESS)
        {
            // Remove static sensor from the queue if it was successfully
            // updated
            std::erase(nsmDevice->getStaticSensors(), sensor);
        }

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
        sensor->setLastUpdatedTimeStamp(t1);
    }

    // Either we were able to succesfully update all sensors in one
    // iteration or there are no sensors in the queue. Mark ready in
    // both cases.
    if (!sensors.hasSensorsToUpdate() && !nsmDevice->isReady())
    {
        nsmDevice->changeDeviceReadyState(true);
        checkAllDevicesReady();
    }

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    SensorManagerImpl::deviceTask(std::shared_ptr<NsmDevice> nsmDevice)
{
    uint64_t t0 = 0;
    uint64_t t1 = 0;

    while (pollingIsRunning)
    {
        sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);

        if (!nsmDevice->isOnline())
        {
            const uint64_t inActiveSleepTimeInUsec = INACTIVE_SLEEP_TIME_IN_MS *
                                                     1000;
            co_await common::Sleep(event, inActiveSleepTimeInUsec,
                                   common::Priority);
            continue;
        }
        else if (!nsmDevice->allCommandCodesAreRetrieved())
        {
            co_await refreshCommandMatrix(nsmDevice);
        }

        if (nsmDevice->isOnline())
        {
            co_await pollPrioritySensors(nsmDevice);
            co_await pollNonPrioritySensors(nsmDevice, t0);
        }

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
        auto sleepTime = (t1 - t0) < pollingTimeInUsec
                             ? pollingTimeInUsec - (t1 - t0)
                             : allowedBufferInUsec; // sleep for at least
                                                    // allowedBufferInUsec to
                                                    // ensure minimal delay and
                                                    // CPU usage
        if (sleepTime > 0)
        {
            // The timer event for devices with no priority sensors can be
            // of low priority.
            co_await common::Sleep(event, sleepTime,
                                   nsmDevice->getPrioritySensors().empty()
                                       ? common::NonPriority
                                       : common::Priority);
        }
    }

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

std::shared_ptr<NsmDevice>
    SensorManagerImpl::getNsmDevice(uint8_t deviceType, uint8_t instanceNumber,
                                    uint8_t deviceRole)
{
    return mctp::MctpDiscovery::getInstance().getNsmDeviceByIdentification(
        deviceType, instanceNumber, deviceRole);
}
std::shared_ptr<NsmDevice>
    SensorManagerImpl::getNsmDeviceFromStaticUUID(uuid_t uuid)
{
    return mctp::MctpDiscovery::getInstance().getNsmDeviceFromStaticUUID(uuid);
}

eid_t SensorManagerImpl::getEid(std::shared_ptr<NsmDevice> nsmDevice)
{
    return nsmDevice->getEid();
}

} // namespace nsm
