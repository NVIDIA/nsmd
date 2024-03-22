#include "nsmNumericSensorUtility.hpp"

#include "nsmNumericAggregator.hpp"
#include "nsmPower.hpp"
#include "nsmPowerAggregator.hpp"
#include "nsmTemp.hpp"
#include "nsmTempAggregator.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
static std::map<eid_t,
         std::vector<std::pair<std::shared_ptr<NsmNumericAggregator>, bool>>>
    SensorAggregators;

static inline std::shared_ptr<nsm::NsmSensor> createAggregateNsmSensor(
    const std::string& objPath, const std::string& interface, const uint8_t eid,
    const std::string& name, const std::string& type,
    const std::string& association, const bool priority, const bool aggregate,
    std::map<eid_t, std::vector<std::shared_ptr<NsmSensor>>>& prioritySensors,
    std::map<eid_t, std::deque<std::shared_ptr<NsmSensor>>>& roundRobinSensors)
{
    auto& bus = utils::DBusHandler::getBus();
    std::shared_ptr<NsmSensor> sensor{nullptr};

    std::shared_ptr<NsmNumericAggregator> aggregator{};
    std::shared_ptr<NsmNumericSensor> numeric_sensor{};
    uint8_t telemetryTag{0xFF};
    bool does_aggregate_exist{false};

    // Check if Aggregator object for the NSM Command already exists.
    if (aggregate)
    {
        auto& aggregators = SensorAggregators[eid];

        auto aggregator_itr = std::find_if(
            aggregators.begin(), aggregators.end(),
            [&](const auto& e) { return e.first->getType() == type; });

        // If existing Aggregator has low priority and this NSM Command has
        // high priority, update the existing Aggregator's priority to
        // high, remove it from round-robin queue, and place it in priority
        // queue.
        if (aggregator_itr != aggregators.end())
        {
            does_aggregate_exist = true;
            aggregator = aggregator_itr->first;

            if (priority && !aggregator_itr->second)
            {
                // aggregator_itr->second = true;
                std::erase(roundRobinSensors[eid], aggregator);
                prioritySensors[eid].push_back(aggregator);
            }
        }
    }

    if (type == "NSM_Temp")
    {
        auto sensorId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "SensorId", interface.c_str());
        auto temp_sensor =
            std::make_shared<NsmTemp>(bus, name, type, sensorId, association);
        sensor = temp_sensor;
        numeric_sensor = temp_sensor;
        telemetryTag = sensorId;

        if (aggregate && !does_aggregate_exist)
        {
            aggregator = std::make_shared<NsmTempAggregator>(name, type);
        }
    }
    else if (type == "NSM_Power")
    {
        auto sensorId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "SensorId", interface.c_str());
        auto averagingInterval = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "AveragingInterval", interface.c_str());

        auto power_sensor = std::make_shared<NsmPower>(
            bus, name, type, sensorId, averagingInterval, association);
        sensor = power_sensor;
        numeric_sensor = power_sensor;
        telemetryTag = sensorId;

        if (aggregate && !does_aggregate_exist)
        {
            aggregator = std::make_shared<NsmPowerAggregator>(name, type, 0);
        }
    }
    else
    {
        return sensor;
    }

    lg2::info(
        "Created NSM Sensor : EID={EID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
        "EID", eid, "NAME", name, "TYPE", type, "OBJPATH", objPath);

    if (aggregate)
    {
        if (!does_aggregate_exist)
        {
            SensorAggregators[eid].push_back({aggregator, priority});
            lg2::info(
                "Created NSM Sensor Aggregator : EID={EID}, Name={NAME}, Type={TYPE}",
                "EID", eid, "NAME", name, "TYPE", type);
            if (priority)
            {
                prioritySensors[eid].push_back(aggregator);
            }
            else
            {
                roundRobinSensors[eid].push_back(aggregator);
            }
        }

        auto rc = aggregator->addSensor(telemetryTag, numeric_sensor);
        if (rc == NSM_SW_SUCCESS)
        {
            lg2::info(
                "Added NSM Sensor to Aggregator : EID={EID}, Name={NAME}, Type={TYPE}",
                "EID", eid, "NAME", name, "TYPE", type);
        }
        else
        {
            lg2::error(
                "Failed to add NSM Sensor to Aggregator : RC = {RC}, EID={EID}, Name={NAME}, Type={TYPE}",
                "RC", rc, "EID", eid, "NAME", name, "TYPE", type);
        }
    }

    return sensor;
}

void createNumericNsmSensor(
    const std::string& interface, const std::string& objPath,
    const std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable,
    std::map<eid_t, std::vector<std::shared_ptr<NsmObject>>>& deviceSensors,
    std::map<eid_t, std::vector<std::shared_ptr<NsmSensor>>>& prioritySensors,
    std::map<eid_t, std::deque<std::shared_ptr<NsmSensor>>>& roundRobinSensors)
{
    try
    {
        auto name = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());

        auto association = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Association", interface.c_str());

        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

        auto aggregate = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Aggregator", interface.c_str());

        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());

        eid_t eid = utils::getEidFromUUID(eidTable, uuid);
        auto type = interface.substr(interface.find_last_of('.') + 1);

        if (eid != std::numeric_limits<uint8_t>::max())
        {
            auto sensor = createAggregateNsmSensor(
                objPath, interface, eid, name, type, association, priority,
                aggregate, prioritySensors, roundRobinSensors);

            if (!sensor)
            {
                lg2::error(
                    "Failed to create NSM Sensor : EID={EID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                    "EID", eid, "NAME", name, "TYPE", type, "OBJPATH", objPath);

                return;
            }

            deviceSensors[eid].emplace_back(sensor);

            if (!aggregate)
            {
                if (priority)
                {
                    prioritySensors[eid].emplace_back(sensor);
                }
                else
                {
                    roundRobinSensors[eid].emplace_back(sensor);
                }
            }
        }
        else
        {
            lg2::error(
                "found NSM_Sensor [{NAME}] but not created, EID not Found for UUID={UUID}",
                "UUID", uuid, "NAME", name);
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Error while addSensor for {PATH}, {ERROR}", "PATH", objPath,
                   "ERROR", e);
        return;
    }
}
}