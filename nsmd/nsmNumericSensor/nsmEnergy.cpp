#include "config.h"

#include "nsmEnergy.hpp"

#include "platform-environmental.h"

#include "nsmEnergyAggregator.hpp"
#include "nsmNumericSensorFactory.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

namespace nsm
{
NsmEnergy::NsmEnergy(sdbusplus::bus::bus& bus, const std::string& name,
                     const std::string& type, uint8_t sensorId,
                     const std::vector<utils::Association>& association,
                     [[maybe_unused]] const std::string& chassis_association) :
    NsmNumericSensor(
        name, type, sensorId,
        std::make_shared<NsmNumericSensorValueAggregate>(
            std::make_unique<NsmNumericSensorDbusValue>(
                bus, name, sensor_type, SensorUnit::Joules, association)
#ifdef NVIDIA_SHMEM
                ,
            std::make_unique<NsmNumericSensorShmem>(name, sensor_type,
                                                    chassis_association)
#endif
                ))
{}

std::optional<std::vector<uint8_t>> NsmEnergy::genRequestMsg(eid_t eid,
                                                             uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_current_energy_count_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc =
        encode_get_current_energy_count_req(instanceId, sensorId, requestPtr);
    if (rc)
    {
        lg2::error("encode_get_current_energy_count_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmEnergy::handleResponseMsg(const struct nsm_msg* responseMsg,
                                     size_t responseLen)
{

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint64_t reading = 0;
    std::vector<uint8_t> data(65535, 0);

    auto rc = decode_get_current_energy_count_resp(responseMsg, responseLen,
                                                   &cc, &reason_code, &reading);
    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        sensorValue->updateReading(reading);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_current_energy_count_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

class EnergySensorFactory : public NumericSensorBuilder
{
  public:
    std::shared_ptr<NsmNumericSensor>
        makeSensor([[maybe_unused]] const std::string& interface,
                   [[maybe_unused]] const std::string& objPath,
                   sdbusplus::bus::bus& bus,
                   const NumericSensorInfo& info) override
    {
        return std::make_shared<NsmEnergy>(bus, info.name, info.type,
                                           info.sensorId, info.associations,
                                           info.chassis_association);
    };

    std::shared_ptr<NsmNumericAggregator>
        makeAggregator(const NumericSensorInfo& info) override
    {
        return std::make_shared<NsmEnergyAggregator>(info.name, info.type,
                                                     info.priority);
    };
};

static NumericSensorFactory numericSensorFactory{
    std::make_unique<EnergySensorFactory>()};

REGISTER_NSM_CREATION_FUNCTION(numericSensorFactory.getCreationFunction(),
                               "xyz.openbmc_project.Configuration.NSM_Energy")

} // namespace nsm