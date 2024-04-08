#pragma once

#include "base.h"

#include "nsmSensorAggregator.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace nsm
{
class NsmNumericSensorValue;

class NsmNumericAggregator : public NsmSensorAggregator
{
  public:
    NsmNumericAggregator(const std::string& name, const std::string& type,
                         bool priority) :
        NsmSensorAggregator(name, type),
        priority(priority){};
    virtual ~NsmNumericAggregator() = default;

    int addSensor(uint8_t tag, std::shared_ptr<NsmNumericSensorValue> sensor);

    const NsmNumericSensorValue* getSensor(uint8_t tag)
    {
        return sensors[tag].get();
    };

    bool priority;

  protected:
    int updateSensorReading(uint8_t tag, double reading,
                            uint64_t timestamp = 0);
    int updateSensorNotWorking(uint8_t tag);

  private:
    std::array<std::shared_ptr<NsmNumericSensorValue>,
               NSM_AGGREGATE_MAX_SAMPLE_TAG_VALUE>
        sensors{};
};
} // namespace nsm
