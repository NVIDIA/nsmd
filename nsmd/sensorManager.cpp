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

SensorManager::SensorManager(
    sdbusplus::bus::bus& bus, sdeventplus::Event& event,
    requester::Handler<requester::Request>& handler,
    nsm::InstanceIdDb& instanceIdDb, sdbusplus::asio::object_server& objServer,
    std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable,
    NsmDeviceTable& nsmDevices, eid_t localEid) :
    bus(bus),
    event(event), handler(handler), instanceIdDb(instanceIdDb),
    objServer(objServer), eidTable(eidTable), nsmDevices(nsmDevices),
    localEid(localEid)
{
    deferScanInventory = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn(&SensorManager::scanInventory), this));

    inventoryAddedSignal = std::make_unique<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded(
            "/xyz/openbmc_project/inventory"),
        std::bind_front(&SensorManager::interfaceAddedhandler, this));
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
            co_await getSensorReading(eid, sensor);
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

            co_await getSensorReading(eid, sensor);
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

requester::Coroutine
    SensorManager::getSensorReading(eid_t eid,
                                    std::shared_ptr<NsmSensor> sensor)
{
    auto requestMsg = sensor->genRequestMsg(eid, 0);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "getSensorReading: sensor->genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", sensor->getName(), "EID", eid);
        co_return NSM_SW_ERROR;
    }

    const struct nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    auto rc =
        co_await SendRecvNsmMsg(eid, *requestMsg, &responseMsg, &responseLen);
    if (rc)
    {
        lg2::error(
            "getSensorReading: SendRecvNsmMsg failed, name={NAME} eid={EID}",
            "NAME", sensor->getName(), "EID", eid);
    }

    rc = sensor->handleResponseMsg(responseMsg, responseLen);
    if (rc)
    {
        lg2::error(
            "getSensorReading: sensor->handleResponseMsg failed, name={NAME} eid={EID}",
            "NAME", sensor->getName(), "EID", eid);
    }

    co_return rc;
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
