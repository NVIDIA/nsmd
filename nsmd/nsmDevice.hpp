#pragma once

#include "common/types.hpp"
#include "nsmNumericAggregator.hpp"
#include "nsmObject.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/timer.hpp>

#include <coroutine>
#include <deque>

namespace nsm
{

class NsmDevice;
using NsmDeviceTable = std::vector<std::shared_ptr<NsmDevice>>;

class NsmDevice
{
  public:
    NsmDevice(uuid_t uuid) : uuid(uuid)
    {}

    std::shared_ptr<sdbusplus::asio::dbus_interface> fruDeviceIntf;

    eid_t eid;
    uuid_t uuid;
    std::unique_ptr<phosphor::Timer> pollingTimer;
    std::coroutine_handle<> doPollingTaskHandle;
    std::vector<std::shared_ptr<NsmObject>> deviceSensors;
    std::vector<std::shared_ptr<NsmSensor>> prioritySensors;
    std::deque<std::shared_ptr<NsmSensor>> roundRobinSensors;
    std::vector<std::shared_ptr<NsmNumericAggregator>> SensorAggregators;

    std::shared_ptr<NsmNumericAggregator>
        findAggregatorByType(std::string type);
};

std::shared_ptr<NsmDevice> findNsmDeviceByUUID(NsmDeviceTable& nsmDevices,
                                               uuid_t uuid);

} // namespace nsm
