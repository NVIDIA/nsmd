#pragma once

#include "nsmNumericSensor.hpp"
#include "nsmSensor.hpp"

namespace nsm
{
class NsmVoltage : public NsmNumericSensor
{
  public:
    NsmVoltage(sdbusplus::bus::bus& bus, const std::string& name,
               const std::string& type, uint8_t sensorId,
               const std::vector<utils::Association>& association);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    static constexpr auto sensor_type = "voltage";
};

} // namespace nsm
