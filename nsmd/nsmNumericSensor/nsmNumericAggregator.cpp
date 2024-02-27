#include "nsmNumericAggregator.hpp"

#include "nsmNumericSensor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <limits>

namespace nsm
{
int NsmNumericAggregator::addSensor(
    uint8_t tag, std::shared_ptr<NsmNumericSensorValue> sensor)
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

    return NSM_SW_SUCCESS;
}
} // namespace nsm
