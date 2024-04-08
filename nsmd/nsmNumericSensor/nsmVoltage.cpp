#include "nsmVoltage.hpp"

#include "platform-environmental.h"

#include "nsmNumericSensorFactory.hpp"
#include "nsmVoltageAggregator.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

namespace nsm
{
NsmVoltage::NsmVoltage(sdbusplus::bus::bus& bus, const std::string& name,
                       const std::string& type, uint8_t sensorId,
                       const std::vector<utils::Association>& association) :
    NsmNumericSensor(
        name, type, sensorId,
        std::make_unique<NsmNumericSensorDbusValue>(
            bus, name, sensor_type, SensorUnit::Volts, association))
{}

std::optional<std::vector<uint8_t>>
    NsmVoltage::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_voltage_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_voltage_req(instanceId, sensorId, requestPtr);
    if (rc)
    {
        lg2::error("encode_get_voltage_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmVoltage::handleResponseMsg(const struct nsm_msg* responseMsg,
                                      size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint32_t reading = 0;
    std::vector<uint8_t> data(65535, 0);

    auto rc = decode_get_voltage_resp(responseMsg, responseLen, &cc,
                                      &reason_code, &reading);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // unit of voltage is microvolts in NSM Command Response and selected
        // unit in SensorValue PDI is Volts. Hence it is converted to Volts.
        sensorValue->updateReading(reading / 1'000'000.0);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_voltage_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

class VoltageSensorFactory : public NumericSensorBuilder
{
  public:
    std::shared_ptr<NsmNumericSensor>
        makeSensor([[maybe_unused]] const std::string& interface,
                   [[maybe_unused]] const std::string& objPath,
                   sdbusplus::bus::bus& bus,
                   const NumericSensorInfo& info) override
    {
        return std::make_shared<NsmVoltage>(bus, info.name, info.type,
                                            info.sensorId, info.associations);
    };

    std::shared_ptr<NsmNumericAggregator>
        makeAggregator(const NumericSensorInfo& info) override
    {
        return std::make_shared<NsmVoltageAggregator>(info.name, info.type,
                                                      info.priority);
    };
};

static NumericSensorFactory numericSensorFactory{
    std::make_unique<VoltageSensorFactory>()};

REGISTER_NSM_CREATION_FUNCTION(numericSensorFactory.getCreationFunction(),
                               "xyz.openbmc_project.Configuration.NSM_Voltage")

} // namespace nsm