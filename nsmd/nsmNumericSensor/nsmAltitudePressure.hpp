#pragma once

#include "nsmNumericSensor.hpp"
#include "nsmSensor.hpp"

namespace nsm
{
class NsmAltitudePressure : public NsmNumericSensor
{
  public:
    NsmAltitudePressure(sdbusplus::bus::bus& bus, const std::string& name,
                        const std::string& type,
                        const std::vector<utils::Association>& association,
                        const std::string& physicalContext,
                        const std::string* implementation,
                        const double maxAllowableValue);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    std::string getSensorType() override
    {
        return "altitude";
    }
};

} // namespace nsm
