#pragma once

#include "nsmDevice.hpp"
#include "nsmNumericAggregator.hpp"
#include "nsmNumericSensor.hpp"
#include "nsmObjectFactory.hpp"

#include <string>
#include <vector>

namespace nsm
{

struct NumericSensorInfo
{
    std::string name;
    std::string type;
    uint8_t sensorId;
    std::vector<utils::Association> associations;
    std::string chassis_association;
    bool priority;
    bool aggregated;
};

class NumericSensorBuilder
{
  public:
    virtual ~NumericSensorBuilder() = default;

    virtual std::shared_ptr<NsmNumericSensor>
        makeSensor(const std::string& interface, const std::string& objPath,
                   sdbusplus::bus::bus& bus, const NumericSensorInfo& info) = 0;

    virtual std::shared_ptr<NsmNumericAggregator>
        makeAggregator(const NumericSensorInfo& info) = 0;
};

class NumericSensorFactory
{
  public:
    NumericSensorFactory(std::unique_ptr<NumericSensorBuilder> builder) :
        builder(std::move(builder)){};

    CreationFunction getCreationFunction();

    void make(SensorManager& manager, const std::string& interface,
              const std::string& objPath);

  private:
    std::unique_ptr<NumericSensorBuilder> builder;
};

}; // namespace nsm
