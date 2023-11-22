#include "nsmTemp.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

NsmTemp::NsmTemp(sdbusplus::bus::bus& bus, std::string& name, bool priority,
                 uint8_t sensorId, std::string& association) :
    NsmSensor(name, priority),
    sensorId(sensorId)
{
    lg2::info("NsmTemp: create sensor:{NAME}", "NAME", name.c_str());

    std::string objPath = "/xyz/openbmc_project/sensors/temperature/" + name;

    valueIntf = std::make_unique<ValueIntf>(bus, objPath.c_str());
    valueIntf->unit(SensorUnit::DegreesC);

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

void NsmTemp::updateReading(bool available, bool functional, double value)
{
    if (availabilityIntf)
    {
        availabilityIntf->available(available);
    }

    if (operationalStatusIntf)
    {
        operationalStatusIntf->functional(functional);
    }

    if (valueIntf)
    {
        valueIntf->value(value);
    }
}

std::optional<std::vector<uint8_t>> NsmTemp::genRequestMsg(eid_t eid,
                                                           uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_temperature_reading_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc =
        encode_get_temperature_reading_req(instanceId, sensorId, requestPtr);
    if (rc)
    {
        lg2::error("encode_get_temperature_reading_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmTemp::handleResponseMsg(const struct nsm_msg* responseMsg,
                                   size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    real32_t reading = 0;
    std::vector<uint8_t> data(65535, 0);

    auto rc = decode_get_temperature_reading_resp(responseMsg, responseLen, &cc,
                                                  &reading);
    if (rc)
    {
        lg2::error("handleResponseMsg: decode_get_temperature_reading_resp "
                   "failed. sensor={NAME} rc={RC}",
                   "NAME", name, "RC", rc);
    }

    if (cc == NSM_SUCCESS)
    {
        updateReading(true, true, reading);
    }
    else
    {
        updateReading(false, false);
    }

    return cc;
}

} // namespace nsm