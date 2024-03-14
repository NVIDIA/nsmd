#pragma once

#include "common/types.hpp"
#include "instance_id.hpp"
#include "nsmNumericAggregator.hpp"
#include "nsmSensor.hpp"
#include "requester/handler.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/timer.hpp>

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
    SensorManager(
        sdbusplus::bus::bus& bus, sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable);

    void startPolling();
    void stopPolling();
    void doPolling(eid_t eid);
    void interfaceAddedhandler(sdbusplus::message::message& msg);
    void _startPolling(sdeventplus::source::EventBase& /* source */);
    requester::Coroutine doPollingTask(eid_t eid);
    requester::Coroutine getSensorReading(eid_t eid,
                                          std::shared_ptr<NsmSensor> sensor);
    requester::Coroutine SendRecvNsmMsg(eid_t eid, Request& request,
                                        const nsm_msg** responseMsg,
                                        size_t* responseLen);
    void scanInventory();

  private:
    void addSensor(const std::string& objPath, const std::string& interface,
                   const std::string& type);
    void addNVLink(std::string objPath);

    std::shared_ptr<nsm::NsmSensor>
        createNsmSensor(const std::string& objPath,
                        const std::string& interface, const uint8_t eid,
                        const std::string& name, const std::string& type,
                        const std::string& association, const bool priority,
                        const bool aggregate);

    sdbusplus::bus::bus& bus;
    sdeventplus::Event& event;
    requester::Handler<requester::Request>& handler;
    nsm::InstanceIdDb& instanceIdDb;
    sdbusplus::asio::object_server& objServer;
    std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable;

    sdbusplus::bus::match_t inventoryAddedSignal;
    // sdbusplus::bus::match_t inventoryRemovbedSignal;

    std::map<eid_t, std::vector<std::shared_ptr<NsmSensor>>> deviceSensors;
    std::map<eid_t, std::vector<std::shared_ptr<NsmSensor>>> prioritySensors;
    std::map<eid_t, std::deque<std::shared_ptr<NsmSensor>>> roundRobinSensors;

    std::map<eid_t, std::unique_ptr<phosphor::Timer>> pollingTimers;
    std::map<eid_t, std::coroutine_handle<>> doPollingTaskHandles;

    std::unique_ptr<sdeventplus::source::Defer> newSensorEvent;

    // Contains NSM Sensor Aggregators. Second field in std::pair indicates
    // priority.
    std::map<eid_t, std::vector<
                        std::pair<std::shared_ptr<NsmNumericAggregator>, bool>>>
        SensorAggregators;
};

} // namespace nsm
