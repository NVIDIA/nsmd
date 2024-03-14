#include "nsmTempAggregator.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmTempAggregator::NsmTempAggregator(const std::string& name,
                                     const std::string& type) :
    NsmNumericAggregator(name, type)
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

int NsmTempAggregator::handleSampleData(uint8_t tag, const uint8_t* data,
                                        size_t data_len)
{
    double reading{};
    auto rc =
        decode_aggregate_temperature_reading_data(data, data_len, &reading);

    if (rc == NSM_SW_SUCCESS)
    {
        updateSensorReading(tag, reading);
    }
    else
    {
        updateSensorNotWorking(tag);
    }

    return rc;
}
} // namespace nsm
