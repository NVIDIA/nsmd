#include "nsmAltitudePressure.hpp"

#include "platform-environmental.h"

#include "dBusAsyncUtils.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>
namespace nsm
{
NsmAltitudePressure::NsmAltitudePressure(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::vector<utils::Association>& association,
    const std::string& physicalContext, const std::string* implementation,
    const double maxAllowableValue) :
    NsmNumericSensor(name, type, 0,
                     std::make_shared<NsmNumericSensorValueAggregate>(
                         std::make_unique<NsmNumericSensorDbusValue>(
                             bus, name, getSensorType(), SensorUnit::Pascals,
                             association, physicalContext, implementation,
                             maxAllowableValue, nullptr, nullptr)))
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
        clearErrorBitMap("decode_get_altitude_pressure_resp");
    }
    else
    {
        sensorValue->updateReading(std::numeric_limits<double>::quiet_NaN());

        logHandleResponseMsg("decode_get_altitude_pressure_resp", reason_code,
                             cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

requester::Coroutine makeNsmAltitudePressure(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();

    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    name = utils::makeDBusNameValid(name);

    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto priority = co_await utils::coGetDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());

    auto physicalContext = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "PhysicalContext", interface.c_str());

    std::unique_ptr<std::string> implementation{};
    try
    {
        implementation = std::make_unique<std::string>(
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "Implementation", interface.c_str()));
    }
    catch (const std::exception& e)
    {}

    double maxAllowableValue{std::numeric_limits<double>::infinity()};
    try
    {
        maxAllowableValue = utils::DBusHandler().getDbusProperty<double>(
            objPath.c_str(), "MaxAllowableOperatingValue", interface.c_str());
    }
    catch (const std::exception& e)
    {}

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of Altitude Pressure Sensor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto sensor = std::make_shared<NsmAltitudePressure>(
        bus, name, type, associations, physicalContext, implementation.get(),
        maxAllowableValue);
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
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    makeNsmAltitudePressure,
    "xyz.openbmc_project.Configuration.NSM_AltitudePressure")

} // namespace nsm
