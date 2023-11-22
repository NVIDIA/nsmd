#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::ElementsAre;

#include "base.h"
#include "platform-environmental.h"
#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmTemp.hpp"

TEST(nsmSensor, GoodTest)
{
	auto bus = sdbusplus::bus::new_default();
	std::string sensorName("dummy_sensor");
    std::string association("/xyz/openbmc_project/inventory/dummy_device");
	uint8_t sensorId = 0;
    bool priority = true;
	nsm::NsmTemp tempSensor(bus, sensorName, priority, sensorId, association);

    EXPECT_EQ(tempSensor.name, sensorName);
    EXPECT_EQ(tempSensor.isPriority(), priority);
    EXPECT_EQ(tempSensor.sensorId, sensorId);
    EXPECT_NE(tempSensor.valueIntf, nullptr);
    EXPECT_NE(tempSensor.availabilityIntf, nullptr);
    EXPECT_NE(tempSensor.operationalStatusIntf, nullptr);

    double reading = 12.34;
    tempSensor.updateReading(true, true, reading);
    EXPECT_DOUBLE_EQ(tempSensor.valueIntf->value(), reading);
    EXPECT_EQ(tempSensor.availabilityIntf->available(), true);
    EXPECT_EQ(tempSensor.operationalStatusIntf->functional(), true);
}
