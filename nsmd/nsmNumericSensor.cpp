#include "config.h"

#include "nsmNumericSensor.hpp"

#include <telemetry_mrd_producer.hpp>

namespace nsm
{
const std::string NsmNumericSensor::valueInterface{
    "xyz.openbmc_project.Sensor.Value"};
const std::string NsmNumericSensor::valueProperty{"Value"};

NsmNumericSensor::NsmNumericSensor(sdbusplus::bus::bus& bus,
                                   const std::string& name,
                                   const std::string& sensor_type,
                                   const SensorUnit unit,
                                   const std::string& association) :
    objPath("/xyz/openbmc_project/sensors/" + sensor_type + '/' + name),
    association(association)
{
    valueIntf = std::make_unique<ValueIntf>(bus, objPath.c_str());
    valueIntf->unit(unit);

    availabilityIntf = std::make_unique<AvailabilityIntf>(bus, objPath.c_str());
    availabilityIntf->available(true);

    operationalStatusIntf =
        std::make_unique<OperationalStatusIntf>(bus, objPath.c_str());
    operationalStatusIntf->functional(true);

    associationDefinitionsIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, objPath.c_str());
    associationDefinitionsIntf->associations(
        {{"chassis", "all_sensors", association.c_str()}});
}

void NsmNumericSensor::updateReading(double value, uint64_t timestamp)
{
    if (availabilityIntf)
    {
        availabilityIntf->available(true);
    }

    if (operationalStatusIntf)
    {
        operationalStatusIntf->functional(true);
    }

    if (valueIntf)
    {
        valueIntf->value(value);
    }

#ifdef NVIDIA_SHMEM
    DbusVariantType valueVariant{value};

    if (!timestamp)
    {
        timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

    nv::shmem::AggregationService::updateTelemetry(objPath, valueInterface,
                                                   valueProperty, valueVariant,
                                                   timestamp, 0, association);
#endif
}

void NsmNumericSensor::updateStatus(bool available, bool functional)
{
    if (availabilityIntf)
    {
        availabilityIntf->available(available);
    }

    if (operationalStatusIntf)
    {
        operationalStatusIntf->functional(functional);
    }
}
} // namespace nsm
