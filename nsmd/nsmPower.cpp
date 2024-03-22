#include "nsmPower.hpp"
#include "nsmObjectFactory.hpp"

#include "platform-environmental.h"

#include "nsmNumericSensorUtility.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

namespace nsm
{
NsmPower::NsmPower(sdbusplus::bus::bus& bus, const std::string& name,
                   const std::string& type, uint8_t sensorId,
                   uint8_t averagingInterval, const std::string& association) :
    NsmSensor(name, type),
    NsmNumericSensor(bus, name, sensor_type, SensorUnit::Watts, association),
    sensorId(sensorId), averagingInterval(averagingInterval)
{}

std::optional<std::vector<uint8_t>> NsmPower::genRequestMsg(eid_t eid,
                                                            uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_current_power_draw_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_current_power_draw_req(instanceId, sensorId,
                                                averagingInterval, requestPtr);
    if (rc)
    {
        lg2::error("encode_get_current_power_draw_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPower::handleResponseMsg(const struct nsm_msg* responseMsg,
                                    size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint32_t reading = 0;
    std::vector<uint8_t> data(65535, 0);

    auto rc = decode_get_current_power_draw_resp(responseMsg, responseLen, &cc,
                                                 &reason_code, &reading);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // unit of power is milliwatt in NSM Command Response and selected unit
        // in SensorValue PDI is Watts. Hence it is converted to Watts.
        updateReading(reading / 1000.0);
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

REGISTER_NSM_CREATION_FUNCTION(createNumericNsmSensor,
                               "xyz.openbmc_project.Configuration.NSM_Power")

} // namespace nsm
