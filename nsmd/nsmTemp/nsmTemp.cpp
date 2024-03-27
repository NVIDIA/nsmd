#include "nsmTemp.hpp"

#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmTempAggregator.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

namespace nsm
{
NsmTemp::NsmTemp(sdbusplus::bus::bus& bus, const std::string& name,
                 const std::string& type, uint8_t sensorId,
                 const std::string& association) :
    NsmSensor(name, type),
    NsmNumericSensor(bus, name, sensor_type, SensorUnit::DegreesC, association),
    sensorId(sensorId)
{}

std::optional<std::vector<uint8_t>> NsmTemp::genRequestMsg(eid_t eid,
                                                           uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_temperature_reading_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc =
        encode_get_temperature_reading_req(instanceId, sensorId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_temperature_reading_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmTemp::handleResponseMsg(const struct nsm_msg* responseMsg,
                                   size_t responseLen)
{

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    double reading = 0;

    auto rc = decode_get_temperature_reading_resp(responseMsg, responseLen, &cc,
                                                  &reason_code, &reading);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(reading);
    }
    else
    {
        updateStatus(false, false);
        lg2::error(
            "handleResponseMsg: decode_get_temperature_reading_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

static void createNsmTempSensor([[maybe_unused]] const std::string& interface,
                                [[maybe_unused]] const std::string& objPath,
                                [[maybe_unused]] NsmDeviceTable& nsmDevices)
{
    auto& bus = utils::DBusHandler::getBus();
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
    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = findNsmDeviceByUUID(nsmDevices, uuid);
    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_Temp PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    std::shared_ptr<NsmNumericAggregator> aggregator{};
    // Check if Aggregator object for the NSM Command already exists.
    if (aggregate)
    {
        aggregator = nsmDevice->findAggregatorByType(type);
        if (aggregator)
        {
            // If existing Aggregator has low priority and this NSM Command has
            // high priority, update the existing Aggregator's priority to
            // high, remove it from round-robin queue, and place it in priority
            // queue.
            if (priority && !aggregator->priority)
            {
                aggregator->priority = true;
                std::erase(nsmDevice->roundRobinSensors, aggregator);
                nsmDevice->prioritySensors.push_back(aggregator);
            }
        }
        else
        {
            aggregator =
                std::make_shared<NsmTempAggregator>(name, type, priority);
        }

        nsmDevice->SensorAggregators.push_back(aggregator);
        lg2::info(
            "Created NSM Sensor Aggregator : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        if (priority)
        {
            nsmDevice->prioritySensors.push_back(aggregator);
        }
        else
        {
            nsmDevice->roundRobinSensors.push_back(aggregator);
        }
    }

    auto sensorId = utils::DBusHandler().getDbusProperty<uint64_t>(
        objPath.c_str(), "SensorId", interface.c_str());

    auto sensor =
        std::make_shared<NsmTemp>(bus, name, type, sensorId, association);

    if (aggregate)
    {
        auto rc = aggregator->addSensor(sensorId, sensor);
        if (rc == NSM_SW_SUCCESS)
        {
            lg2::info(
                "Added NSM Sensor to Aggregator : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
        }
        else
        {
            lg2::error(
                "Failed to add NSM Sensor to Aggregator : RC = {RC}, UUID={UUID}, Name={NAME}, Type={TYPE}",
                "RC", rc, "UUID", uuid, "NAME", name, "TYPE", type);
        }
    }
    else
    {
        if (priority)
        {
            nsmDevice->prioritySensors.emplace_back(sensor);
        }
        else
        {
            nsmDevice->roundRobinSensors.emplace_back(sensor);
        }
    }
}

REGISTER_NSM_CREATION_FUNCTION(createNsmTempSensor,
                               "xyz.openbmc_project.Configuration.NSM_Temp")

} // namespace nsm
