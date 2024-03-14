#include "nsmPowerAggregator.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmPowerAggregator::NsmPowerAggregator(const std::string& name,
                                       const std::string& type,
                                       uint8_t averagingInterval) :
    NsmNumericAggregator(name, type),
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

int NsmPowerAggregator::handleSampleData(uint8_t tag, const uint8_t* data,
                                         size_t data_len)
{
    uint32_t reading{};

    if (tag == TIMESTAMP)
    {
        return decode_aggregate_timestamp_data(data, data_len, &timestamp);
    }
    else if (tag <= NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
    {
        auto rc = decode_aggregate_get_current_power_draw_reading(
            data, data_len, &reading);

        if (rc != NSM_SW_SUCCESS)
        {
            updateSensorNotWorking(tag);
            return rc;
        }

        // unit of power is milliwatt in NSM Command Response and selected unit
        // in SensorValue PDI is Watts. Hence it is converted to Watts.
        updateSensorReading(tag, reading / 1000.0, timestamp);

        return NSM_SW_SUCCESS;
    }
    else
    {
        return NSM_SW_ERROR_DATA;
    }
}
} // namespace nsm
