#include "nsmNumericSensor.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class MockNsmNumericSensorValue : public nsm::NsmNumericSensorValue
{
  public:
    MOCK_METHOD(void, updateReading, (double value, uint64_t timestamp),
                (override));
};
