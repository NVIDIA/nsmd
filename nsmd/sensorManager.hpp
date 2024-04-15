#pragma once

#include "common/types.hpp"
#include "instance_id.hpp"
#include "nsmDevice.hpp"
#include "requester/handler.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/timer.hpp>

#include <memory>

namespace nsm
{

using RequesterHandler = requester::Handler<requester::Request>;

/**
 * @brief SensorManager
 *
 * This class manages the NSM sensors defined by EM configuration PDI.
 * SensorManager register callback function to create sensor when there is new
 * NSM device inventory added to D-Bus and provides function calls for other
 * classes to start/stop sensor monitoring.
 *
 */
class SensorManager
{
  public:
    // Delete copy constructor and copy assignment operator
    SensorManager(const SensorManager&) = delete;
    SensorManager& operator=(const SensorManager&) = delete;

    // Static method to access the instance of the class
    static SensorManager& getInstance()
    {
        if (!instance)
        {
            throw std::runtime_error(
                "SensorManager instance not initialized yet");
        }
        return *instance;
    }

    // Static method to initialize the instance
    static void initialize(
        sdbusplus::bus::bus& bus, sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices, eid_t localEid)
    {
        static SensorManager inst(bus, event, handler, instanceIdDb, objServer,
                                  eidTable, nsmDevices, localEid);
        instance = &inst;
    }

    // Regular methods as before
    void startPolling();
    void stopPolling();
    void doPolling(std::shared_ptr<NsmDevice> nsmDevice);
    void interfaceAddedhandler(sdbusplus::message::message& msg);
    void _startPolling(sdeventplus::source::EventBase& /* source */);
    requester::Coroutine doPollingTask(std::shared_ptr<NsmDevice> nsmDevice);

    requester::Coroutine SendRecvNsmMsg(eid_t eid, Request& request,
                                        const nsm_msg** responseMsg,
                                        size_t* responseLen);
    void scanInventory();

    requester::Coroutine pollEvents(eid_t eid);
    eid_t getLocalEid()
    {
        return localEid;
    }
    eid_t getEid(std::shared_ptr<NsmDevice> nsmDevice);
    std::shared_ptr<NsmDevice> getNsmDevice(uuid_t uuid);

  private:
    // Private constructor
    SensorManager(
        sdbusplus::bus::bus& bus, sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices, eid_t localEid);

    // Instance variables as before
    static SensorManager* instance;
    sdbusplus::bus::bus& bus;
    sdeventplus::Event& event;
    requester::Handler<requester::Request>& handler;
    nsm::InstanceIdDb& instanceIdDb;
    sdbusplus::asio::object_server& objServer;
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable;

    std::unique_ptr<sdbusplus::bus::match_t> inventoryAddedSignal;
    std::unique_ptr<sdeventplus::source::Defer> deferScanInventory;
    std::unique_ptr<sdeventplus::source::Defer> newSensorEvent;

    NsmDeviceTable& nsmDevices;
    eid_t localEid;
};
} // namespace nsm
