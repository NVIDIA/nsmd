#include "nsmPowerAggregator.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmPowerAggregator::NsmPowerAggregator(const std::string& name,
                                       const std::string& type, bool priority,
                                       uint8_t averagingInterval) :
    NsmNumericAggregator(name, type, priority),
    averagingInterval(averagingInterval)
{}

std::optional<std::vector<uint8_t>>
    NsmPowerAggregator::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request;

    request.resize(sizeof(nsm_msg_hdr) +
                   sizeof(nsm_get_current_power_draw_req));
    auto requestPtr = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_power_draw_req(instanceId, sensorId,
                                                averagingInterval, requestPtr);

    if (rc)
    {
        lg2::error("encode_get_temperature_reading_req failed. "
                   "eid={EID}, rc={RC}.",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

int NsmPowerAggregator::handleSamples(
    const std::vector<TelemetrySample>& samples)
{
    uint32_t reading{};
    uint8_t returnValue = NSM_SW_SUCCESS;

    for (const auto& sample : samples)
    {
        if (sample.tag == TIMESTAMP)
        {
            auto rc = decode_aggregate_timestamp_data(
                sample.data, sample.data_len, &timestamp);

            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error("decode_aggregate_timestamp_data failed. rc={RC}.",
                           "RC", rc);
                returnValue = rc;
            }
        }
        else if (sample.tag <= NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        {
            auto rc = decode_aggregate_get_current_power_draw_reading(
                sample.data, sample.data_len, &reading);

            if (rc == NSM_SW_SUCCESS)
            {
                // unit of power is milliwatt in NSM Command Response and
                // selected unit in SensorValue PDI is Watts. Hence it is
                // converted to Watts.
                updateSensorReading(sample.tag, reading / 1000.0, timestamp);
            }
            else
            {
                lg2::error(
                    "decode_aggregate_get_current_power_draw_reading failed. rc={RC}.",
                    "RC", rc);
                returnValue = rc;
                updateSensorNotWorking(sample.tag);
            }
        }
    }

    return returnValue;
}
} // namespace nsm
