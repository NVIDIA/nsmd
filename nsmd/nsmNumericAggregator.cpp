#include "nsmNumericAggregator.hpp"

#include "platform-environmental.h"

#include "nsmNumericSensor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <limits>

namespace nsm
{
int NsmNumericAggregator::addSensor(uint8_t tag,
                                    std::shared_ptr<NsmNumericSensor> sensor)
{
    if (tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
    {
        return NSM_SW_ERROR_DATA;
    }

    sensors[tag] = sensor;

    return NSM_SW_SUCCESS;
}

int NsmNumericAggregator::updateSensorReading(uint8_t tag, double reading,
                                              uint64_t timestamp)
{
    if (!sensors[tag])
    {
        lg2::warning(
            "updateSensorReading: No NSM Sensor found for Tag {TAG} in "
            "NSM Aggregator {NAME} of type {TYPE}.",
            "TAG", tag, "NAME", getName(), "TYPE", getType());
        return NSM_SW_ERROR_DATA;
    }

    sensors[tag]->updateReading(reading, timestamp);

    return NSM_SW_SUCCESS;
}

int NsmNumericAggregator::updateSensorNotWorking(uint8_t tag)
{
    if (!sensors[tag])
    {
        lg2::warning(
            "updateSensorReading: No NSM Sensor found for Tag {TAG} in "
            "NSM Aggregator {NAME} of type {TYPE}.",
            "TAG", tag, "NAME", getName(), "TYPE", getType());
        return NSM_SW_ERROR_DATA;
    }

    sensors[tag]->updateReading(std::numeric_limits<double>::signaling_NaN());
    sensors[tag]->updateStatus(false, false);

    return NSM_SW_SUCCESS;
}

uint8_t NsmNumericAggregator::handleResponseMsg(const nsm_msg* responseMsg,
                                                size_t responseLen)
{
    uint8_t cc{};
    uint16_t telemetry_count{};
    size_t consumed_len{};
    auto response_data = reinterpret_cast<const uint8_t*>(responseMsg);

    auto rc = decode_aggregate_resp(responseMsg, responseLen, &consumed_len,
                                    &cc, &telemetry_count);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("responseHandler: decode_aggregate_resp failed. "
                   "Type={TYPE} sensor={NAME} rc={RC}.",
                   "TYPE", getType(), "NAME", getName(), "RC", rc);
        return rc;
    }

    while (telemetry_count--)
    {
        uint8_t tag;
        bool valid;
        const uint8_t* data;
        size_t data_len;

        responseLen -= consumed_len;
        response_data += consumed_len;

        auto sample =
            reinterpret_cast<const nsm_aggregate_resp_sample*>(response_data);

        rc = decode_aggregate_resp_sample(sample, responseLen, &consumed_len,
                                          &tag, &valid, &data, &data_len);

        if (rc != NSM_SW_SUCCESS || !valid)
        {
            lg2::error("responseHandler: decode_aggregate_resp_sample failed. "
                       "Type={TYPE}, Tag={TAG}, sensor={NAME}, rc={RC}.",
                       "TYPE", getType(), "TAG", tag, "NAME", getName(), "RC",
                       rc);

            continue;
        }

        rc = handleSampleData(tag, data, data_len);

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::warning("responseHandler: decoding of sample data failed. "
                         "Type={TYPE}, Tag={TAG}, sensor={NAME}, rc={RC}",
                         "TYPE", getType(), "TAG", tag, "NAME", getName(), "RC",
                         rc);
        }
    }

    return cc;
}
} // namespace nsm
