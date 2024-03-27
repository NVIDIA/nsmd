#pragma once

#include "nsmNumericSensor.hpp"
#include "nsmSensor.hpp"

namespace nsm
{
class NsmTemp : public NsmSensor, public NsmNumericSensor
{
  public:
    NsmTemp(sdbusplus::bus::bus& bus, const std::string& name,
            const std::string& type, uint8_t sensorId,
            const std::string& association);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    static const std::string valueInterface;
    static const std::string valueProperty;

    static constexpr auto sensor_type = "temperature";

    uint8_t sensorId;
};

} // namespace nsm
