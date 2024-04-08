#include "nsmVoltageAggregator.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmVoltageAggregator::NsmVoltageAggregator(const std::string& name,
                                           const std::string& type,
                                           bool priority) :
    NsmNumericAggregator(name, type, priority)
{}

std::optional<std::vector<uint8_t>>
    NsmVoltageAggregator::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request;

    request.resize(sizeof(nsm_msg_hdr) +
                   sizeof(nsm_get_temperature_reading_req));
    auto requestPtr = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_get_voltage_req(instanceId, sensorId, requestPtr);

    if (rc)
    {
        lg2::error("encode_get_votage_req failed. "
                   "eid={EID}, rc={RC}.",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

int NsmVoltageAggregator::handleSamples(
    const std::vector<TelemetrySample>& samples)
{
    uint32_t reading{};
    uint8_t returnValue = NSM_SW_SUCCESS;

    for (const auto& sample : samples)
    {
        if (sample.tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        {
            continue;
        }

        auto rc = decode_aggregate_voltage_data(sample.data, sample.data_len,
                                                &reading);

        if (rc == NSM_SW_SUCCESS)
        {
            // unit of voltage is microvolts in NSM Command Response and
            // selected unit in SensorValue PDI is Volts. Hence it is converted
            // to Volts.
            updateSensorReading(sample.tag, reading / 1'000'000.0);
        }
        else
        {
            lg2::error("decode_aggregate_voltage_data failed. rc={RC}.", "RC",
                       rc);
            returnValue = rc;
            updateSensorNotWorking(sample.tag);
        }
    }

    return returnValue;
}
} // namespace nsm
