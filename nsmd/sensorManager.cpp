#include "config.h"

#include "sensorManager.hpp"

#include "libnsm/platform-environmental.h"

#include "nsmTemp.hpp"
#include "utils.hpp"

#include <chrono>
#include <iostream>

namespace nsm
{

SensorManager::SensorManager(sdbusplus::bus::bus& bus,
                             sdeventplus::Event& event,
                             requester::Handler<requester::Request>& handler,
                             nsm::InstanceIdDb& instanceIdDb,
                             sdbusplus::asio::object_server& objServer,
                             std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable) :
    bus(bus),
    event(event), handler(handler), instanceIdDb(instanceIdDb),
    objServer(objServer),
    eidTable(eidTable),
    inventoryAddedSignal(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded(
            "/xyz/openbmc_project/inventory"),
        std::bind_front(&SensorManager::interfaceAddedhandler, this))
{
    scanInventory();
    newSensorEvent = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn(&SensorManager::_startPolling), this,
                         std::placeholders::_1));
}

void SensorManager::addTemp(std::string objPath)
{
    try
    {
        auto name = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name",
            "xyz.openbmc_project.Configuration.NSM_Temp");

        auto sensorId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "SensorId",
            "xyz.openbmc_project.Configuration.NSM_Temp");

        auto association = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Association",
            "xyz.openbmc_project.Configuration.NSM_Temp");

        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority",
            "xyz.openbmc_project.Configuration.NSM_Temp");

        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID",
            "xyz.openbmc_project.Configuration.NSM_Temp");
        
        eid_t eid = utils::getEidFromUUID(eidTable, uuid);
        if(eid != std::numeric_limits<uint8_t>::max())
        {
            // create sensor
            auto sensor = std::make_shared<NsmTemp>(bus, name, priority, sensorId,
                                                    association);
            deviceSensors[eid].emplace_back(sensor);
            if (sensor->isPriority())
            {
                prioritySensors[eid].emplace_back(sensor);
            }
            else
            {
                roundRobinSensors[eid].push(sensor);
            }
        }
        else
        {
            lg2::error("found NSM_Sensor [{NAME}] but not created, EID not Found for UUID={UUID}",
                "UUID", uuid,
                "NAME", name);
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Error while addSensor for {PATH}, {ERROR}", "PATH", objPath,
                   "ERROR", e);
        return;
    }
}

void SensorManager::addNVLink(std::string /*objPath*/)
{
    // place holder
}

void SensorManager::scanInventory()
{
    try
    {
        const std::vector<std::string> ifaceList{
            "xyz.openbmc_project.Configuration.NSM_Temp",
            "xyz.openbmc_project.Configuration.NSM_NVLink"};
        auto getSubtreeResponse = utils::DBusHandler().getSubtree(
            "/xyz/openbmc_project/inventory", 0, ifaceList);

        if (getSubtreeResponse.size() == 0)
        {
            return;
        }

        for (const auto& [objPath, mapServiceInterfaces] : getSubtreeResponse)
        {
            for (const auto& [service, interfaces] : mapServiceInterfaces)
            {
                for (const auto& interface : interfaces)
                {
                    if (interface ==
                        "xyz.openbmc_project.Configuration.NSM_Temp")
                    {
                        addTemp(objPath);
                    }
                    else if (interface ==
                             "xyz.openbmc_project.Configuration.NSM_NVLink")
                    {
                        addNVLink(objPath);
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
}

void SensorManager::interfaceAddedhandler(sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objPath;
    dbus::InterfaceMap interfaces;

    msg.read(objPath, interfaces);
    for (const auto& [intfName, properties] : interfaces)
    {
        if (intfName == "xyz.openbmc_project.Configuration.NSM_Temp")
        {
            addTemp(objPath);
        }
        else if (intfName == "xyz.openbmc_project.Configuration.NSM_NVlink")
        {
            addNVLink(objPath);
        }
    }

    newSensorEvent = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn(&SensorManager::_startPolling), this,
                         std::placeholders::_1));
}

void SensorManager::_startPolling(sdeventplus::source::EventBase& /* source */)
{
    startPolling();
}

void SensorManager::startPolling()
{
    for (const auto& [eid, sensors] : deviceSensors)
    {
        if (sensors.size() == 0)
        {
            continue;
        }

        if (pollingTimers.find(eid) == pollingTimers.end())
        {
            pollingTimers[eid] = std::make_unique<phosphor::Timer>(
                event.get(),
                std::bind_front(&SensorManager::doPolling, this, eid));
        }

        if (!pollingTimers[eid]->isRunning())
        {
            pollingTimers[eid]->start(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::milliseconds(SENSOR_POLLING_TIME)),
                true);
        }
    }
}

void SensorManager::stopPolling()
{
    for (const auto& [eid, pollingTimer] : pollingTimers)
    {
        pollingTimer->stop();
    }
}

void SensorManager::doPolling(eid_t eid)
{
    if (doPollingTaskHandles[eid])
    {
        if (!doPollingTaskHandles[eid].done())
        {
            return;
        }
        doPollingTaskHandles[eid].destroy();
    }

    auto co = doPollingTask(eid);
    doPollingTaskHandles[eid] = co.handle;
    if (doPollingTaskHandles[eid].done())
    {
        doPollingTaskHandles[eid] = nullptr;
    }
}

requester::Coroutine SensorManager::doPollingTask(eid_t eid)
{
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    uint64_t pollingTimeInUsec = SENSOR_POLLING_TIME * 1000;

    do
    {
        sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);

        // update all priority sensors
        auto& sensors = prioritySensors[eid];
        for (auto& sensor : sensors)
        {
            co_await getSensorReading(eid, sensor);
            if (pollingTimers[eid] && !pollingTimers[eid]->isRunning())
            {
                co_return NSM_ERROR;
            }
        }

        // update roundRobin sensors for rest of polling time interval
        auto toBeUpdated = roundRobinSensors[eid].size();
        do
        {
            if (toBeUpdated == 0)
            {
                break;
            }
            auto sensor = roundRobinSensors[eid].front();
            roundRobinSensors[eid].pop();
            roundRobinSensors[eid].push(sensor);
            toBeUpdated--;

            co_await getSensorReading(eid, sensor);
            if (pollingTimers[eid] && !pollingTimers[eid]->isRunning())
            {
                co_return NSM_ERROR;
            }

            sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
        } while ((t1 - t0) >= pollingTimeInUsec);

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
    } while ((t1 - t0) >= pollingTimeInUsec);
}

requester::Coroutine
    SensorManager::getSensorReading(eid_t eid,
                                    std::shared_ptr<NsmSensor> sensor)
{

    uint8_t instanceId = instanceIdDb.next(eid);
    auto requestMsg = sensor->genRequestMsg(eid, instanceId);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "getSensorReading: sensor->genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", sensor->name, "EID", eid);
        co_return NSM_ERROR;
    }

    const struct nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    auto rc =
        co_await SendRecvNsmMsg(eid, *requestMsg, &responseMsg, &responseLen);
    if (rc)
    {
        lg2::error(
            "getSensorReading: SendRecvNsmMsg failed, name={NAME} eid={EID}",
            "NAME", sensor->name, "EID", eid);
    }

    rc = sensor->handleResponseMsg(responseMsg, responseLen);
    if (rc)
    {
        lg2::error(
            "getSensorReading: sensor->handleResponseMsg failed, name={NAME} eid={EID}",
            "NAME", sensor->name, "EID", eid);
    }

    co_return rc;
}

requester::Coroutine SensorManager::SendRecvNsmMsg(eid_t eid, Request& request,
                                                   const nsm_msg** responseMsg,
                                                   size_t* responseLen)
{
    auto rc = co_await requester::SendRecvNsmMsg<RequesterHandler>(
        handler, eid, request, responseMsg, responseLen);
    if (rc)
    {
        lg2::error("SendRecvNsmMsg failed. eid={EID} rc={RC}", "EID", eid, "RC",
                   rc);
    }
    co_return rc;
}

} // namespace nsm