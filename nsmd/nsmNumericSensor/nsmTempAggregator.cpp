#include "nsmTempAggregator.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmTempAggregator::NsmTempAggregator(const std::string& name,
                                     const std::string& type, bool priority) :
    NsmNumericAggregator(name, type, priority)
{}

std::optional<std::vector<uint8_t>>
    NsmTempAggregator::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request;

    request.resize(sizeof(nsm_msg_hdr) +
                   sizeof(nsm_get_temperature_reading_req));
    auto requestPtr = reinterpret_cast<nsm_msg*>(request.data());

    auto rc =
        encode_get_temperature_reading_req(instanceId, sensorId, requestPtr);

    if (rc)
    {
        lg2::error("encode_get_temperature_reading_req failed. "
                   "eid={EID}, rc={RC}.",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

int NsmTempAggregator::handleSamples(
    const std::vector<TelemetrySample>& samples)
{
    double reading{};
    uint8_t returnValue = NSM_SW_SUCCESS;

    for (const auto& sample : samples)
    {
        if (sample.tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        {
            continue;
        }

        auto rc = decode_aggregate_temperature_reading_data(
            sample.data, sample.data_len, &reading);

        if (rc == NSM_SW_SUCCESS)
        {
            updateSensorReading(sample.tag, reading);
        }
        else
        {
            lg2::error(
                "decode_aggregate_temperature_reading_data failed. rc={RC}.",
                "RC", rc);
            returnValue = rc;
            updateSensorNotWorking(sample.tag);
        }
    }

    return returnValue;
}
} // namespace nsm
