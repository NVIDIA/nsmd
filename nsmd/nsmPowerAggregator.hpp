#pragma once

#include "nsmNumericAggregator.hpp"

namespace nsm
{
class NsmPowerAggregator : public NsmNumericAggregator
{
  public:
    NsmPowerAggregator(const std::string& name, const std::string& type,
                       uint8_t averagingInterval);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

  private:
    int handleSampleData(uint8_t tag, const uint8_t* data,
                         size_t data_len) override;

    static constexpr uint8_t sensorId = 255;
    uint8_t averagingInterval;
    uint64_t timestamp{};
};

} // namespace nsm
