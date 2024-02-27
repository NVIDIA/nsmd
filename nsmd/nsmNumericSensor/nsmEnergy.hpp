#pragma once

#include "nsmNumericSensor.hpp"
#include "nsmSensor.hpp"

namespace nsm
{
class NsmEnergy : public NsmNumericSensor
{
  public:
    NsmEnergy(sdbusplus::bus::bus& bus, const std::string& name,
              const std::string& type, uint8_t sensorId,
              const std::vector<utils::Association>& association,
              const std::string& chassis_association);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    static constexpr auto sensor_type = "energy";
};

} // namespace nsm
