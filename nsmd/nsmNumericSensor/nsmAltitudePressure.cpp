#include "nsmAltitudePressure.hpp"

#include "platform-environmental.h"

#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

namespace nsm
{
NsmAltitudePressure::NsmAltitudePressure(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::vector<utils::Association>& association) :

    NsmNumericSensor(
        name, type, 0,
        std::make_shared<NsmNumericSensorValueAggregate>(
            std::make_unique<NsmNumericSensorDbusValue>(
                bus, name, sensor_type, SensorUnit::Pascals, association)))
{}

std::optional<std::vector<uint8_t>>
    NsmAltitudePressure::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_altitude_pressure_req(instanceId, requestPtr);
    if (rc)
    {
        lg2::error("encode_get_altitude_pressure_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmAltitudePressure::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint32_t reading = 0;

    auto rc = decode_get_altitude_pressure_resp(responseMsg, responseLen, &cc,
                                                &reason_code, &reading);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // unit of pressure is hPa in NSM Command Response and selected unit
        // in SensorValue PDI is Pa. Hence it is converted to Watts.
        sensorValue->updateReading(reading * 100.0);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_altitude_pressure_resp failed. "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

void makeNsmAltitudePressure(SensorManager& manager,
                             const std::string& interface,
                             const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();

    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    name = utils::makeDBusNameValid(name);

    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto priority = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());

    auto associations =
        utils::getAssociations(objPath, interface + ".Associations");

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of Altitude Pressure Sensor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    auto sensor =
        std::make_shared<NsmAltitudePressure>(bus, name, type, associations);
    lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type={TYPE}",
              "UUID", uuid, "NAME", name, "TYPE", type);

    nsmDevice->deviceSensors.emplace_back(sensor);

    if (priority)
    {
        nsmDevice->prioritySensors.emplace_back(sensor);
    }
    else
    {
        nsmDevice->roundRobinSensors.emplace_back(sensor);
    }
}

REGISTER_NSM_CREATION_FUNCTION(
    makeNsmAltitudePressure,
    "xyz.openbmc_project.Configuration.NSM_AltitudePressure")

} // namespace nsm