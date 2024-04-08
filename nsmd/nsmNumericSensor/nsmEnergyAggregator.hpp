#pragma once

#include "nsmNumericAggregator.hpp"

namespace nsm
{
class NsmEnergyAggregator : public NsmNumericAggregator
{
  public:
    NsmEnergyAggregator(const std::string& name, const std::string& type,
                        bool priority);
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

  private:
    int handleSamples(const std::vector<TelemetrySample>& samples) override;

    static constexpr uint8_t sensorId = 255;
};
} // namespace nsm
