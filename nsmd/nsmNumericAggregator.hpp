#pragma once

#include "base.h"

#include "nsmSensor.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nsm
{
class NsmNumericAggregator;
class NsmNumericSensor;

/** @class NumericNsmSensorAggregator
 *
 * Abstract class to provide common functionalities of NSM Aggregate command.
 * To add a support for new NSM Aggregate command, derive publicly from it
 * and put command specific details (typically calls to libnsm) in its two pure
 * virtual methods.
 */
class NsmNumericAggregator : public NsmSensor
{
  public:
    NsmNumericAggregator(const std::string& name, const std::string& type,
                         bool priority) :
        NsmSensor(name, type),
        priority(priority){};
    virtual ~NsmNumericAggregator() = default;

    int addSensor(uint8_t tag, std::shared_ptr<NsmNumericSensor> sensor);
    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override;

    const NsmNumericSensor* getSensor(uint8_t tag)
    {
        return sensors[tag].get();
    };

    bool priority;

  protected:
    int updateSensorReading(uint8_t tag, double reading,
                            uint64_t timestamp = 0);
    int updateSensorNotWorking(uint8_t tag);

  private:
    /** @brief this function will be called for each telemetry sample found in
     * response message. You might want to call updateSensorReading after
     * decoding value from the data. Special Tag values (i.e. timestamp, uuid
     * etc) are expected to be handled by this function.
     *
     *  @param[in] tag - tag of telemetry sample
     *  @param[in] data - data of telemetry sample
     *  @param[in] data_len - number of bytes in data of telemetry sample
     *  @return nsm_completion_codes
     */
    virtual int handleSampleData(uint8_t tag, const uint8_t* data,
                                 size_t data_len) = 0;

  protected:
    enum SpecialTag : uint8_t
    {
        UUDI = 0xFE,
        TIMESTAMP = 0xFF
    };

  private:
    std::array<std::shared_ptr<NsmNumericSensor>,
               NSM_AGGREGATE_MAX_SAMPLE_TAG_VALUE>
        sensors{};
};
} // namespace nsm
