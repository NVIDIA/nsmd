#include "nsmAltitudePressure.hpp"

#include "platform-environmental.h"

#include "dBusAsyncUtils.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#ifdef NVIDIA_SHMEM
#include <telemetry_mrd_producer.hpp>
#endif
namespace nsm
{
NsmAltitudePressure::NsmAltitudePressure(
    sdbusplus::bus_t& bus, const std::string& name, const std::string& type,
    const std::vector<utils::Association>& association,
    const std::string& physicalContext, const std::string* implementation,
    const double maxAllowableValue, const double maxValue,
    const double minValue) :
    NsmNumericSensor(
        name, type, 0,
        std::make_shared<NsmNumericSensorValueAggregate>(
            std::make_unique<NsmNumericSensorDbusValue>(
                bus, name, getSensorType(), SensorUnit::Pascals, association,
                physicalContext, implementation, maxAllowableValue, maxValue,
                minValue, nullptr, nullptr)))
{}

std::optional<std::vector<uint8_t>>
    NsmAltitudePressure::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_altitude_pressure_req(instanceId, requestPtr);
    if (rc)
    {
        lg2::debug("encode_get_altitude_pressure_req failed. "
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
    uint16_t reasonCode = ERR_NULL;

    uint32_t reading = 0;

    auto rc = decode_get_altitude_pressure_resp(responseMsg, responseLen, &cc,
                                                &reasonCode, &reading);

    LG2_ERROR_FLT(
        "decode_get_altitude_pressure_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        // unit of pressure is hPa in NSM Command Response and selected unit
        // in SensorValue PDI is Pa. Hence it is converted to Watts.
        sensorValue->updateReading(reading * 100.0);
    }
    else
    {
        sensorValue->updateReading(std::numeric_limits<double>::quiet_NaN());
    }

    return cc ? cc : rc;
}

requester::Coroutine makeNsmAltitudePressure(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();

    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }

    name = utils::makeDBusNameValid(name);

    auto type = interface.substr(interface.find_last_of('.') + 1);

    bool priority{};
    if (allCurrentIfaceProperties.count("Priority"))
    {
        priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
    }
    std::string physicalContext{};
    if (allCurrentIfaceProperties.count("PhysicalContext"))
    {
        physicalContext = std::get<std::string>(
            allCurrentIfaceProperties.at("PhysicalContext"));
    }

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

    double maxValue{std::numeric_limits<double>::infinity()};
    try
    {
        maxValue = utils::DBusHandler().getDbusProperty<double>(
            objPath.c_str(), "MaxValue", interface.c_str());
    }
    catch (const std::exception& e)
    {}

    double minValue{-std::numeric_limits<double>::infinity()};
    try
    {
        minValue = utils::DBusHandler().getDbusProperty<double>(
            objPath.c_str(), "MinValue", interface.c_str());
    }
    catch (const std::exception& e)
    {}

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);

    if (!nsmDevice)
    {
        lg2::error(
            "The UUID of Altitude Pressure Sensor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        co_return NSM_ERROR;
    }

    auto sensor = std::make_shared<NsmAltitudePressure>(
        bus, name, type, associations, physicalContext, implementation.get(),
        maxAllowableValue, maxValue, minValue);
    lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type={TYPE}",
              "UUID", uuid, "NAME", name, "TYPE", type);

    nsmDevice->addSensor(sensor, priority);
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    makeNsmAltitudePressure,
    "xyz.openbmc_project.Configuration.NSM_AltitudePressure")

} // namespace nsm
