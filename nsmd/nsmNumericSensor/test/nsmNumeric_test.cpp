#include "utils.hpp"

#include <sdbusplus/bus.hpp>

#include <memory>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmNumericSensor.hpp"
#include "nsmNumericSensorValue_mock.hpp"

static auto& bus = utils::DBusHandler::getBus();
static const std::string sensorName("dummy_sensor");
static const std::string sensorType("dummy_type");
static const std::vector<utils::Association>
    associations({{"chassis", "all_sensors",
                   "/xyz/openbmc_project/inventory/dummy_device"}});

static const double val{32432.8970};
static const uint64_t timestamp{13432548};

TEST(NsmNumericSensorDbusValue, GoodTest)
{
    nsm::NsmNumericSensorDbusValue value{
        bus, sensorName, sensorType, nsm::SensorUnit::DegreesC, associations};
    value.updateReading(val);

    EXPECT_EQ(value.valueIntf.value(), val);
    EXPECT_EQ(value.valueIntf.unit(), nsm::SensorUnit::DegreesC);
}

TEST(NsmNumericSensorDbusValueTimestamp, GoodTest)
{
    nsm::NsmNumericSensorDbusValueTimestamp value{
        bus, sensorName, sensorType, nsm::SensorUnit::DegreesC, associations};
    value.updateReading(val, timestamp);

    EXPECT_EQ(value.timestampIntf.elapsed(), timestamp);
    EXPECT_EQ(value.valueIntf.value(), val);
    EXPECT_EQ(value.valueIntf.unit(), nsm::SensorUnit::DegreesC);
}

TEST(NsmNumericSensorShmem, GoodTest)
{
    nsm::NsmNumericSensorShmem value{sensorName, sensorType,
                                     associations[0].absolutePath};

    EXPECT_EQ(value.objPath,
              "/xyz/openbmc_project/sensors/dummy_type/dummy_sensor");
    EXPECT_EQ(value.association, "/xyz/openbmc_project/inventory/dummy_device");
}

TEST(NsmNumericSensorDbusStatus, GoodTest)
{
    nsm::NsmNumericSensorDbusStatus status{bus, sensorName, sensorType};
    status.updateStatus(true, false);

    EXPECT_EQ(status.availabilityIntf.available(), true);
    EXPECT_EQ(status.operationalStatusIntf.functional(), false);
}

TEST(NsmNumericSensorAggregator, GoodTest)
{
    auto elem1 = std::make_unique<MockNsmNumericSensorValue>();
    auto elem1Ptr = elem1.get();
    auto elem2 = std::make_unique<MockNsmNumericSensorValue>();
    auto elem2Ptr = elem2.get();

    nsm::NsmNumericSensorValueAggregate aggregator{std::move(elem1),
                                                   std::move(elem2)};

    EXPECT_EQ(aggregator.objects.size(), 2);

    EXPECT_CALL(*elem1Ptr, updateReading(val, timestamp)).Times(1);
    EXPECT_CALL(*elem2Ptr, updateReading(val, timestamp)).Times(1);

    aggregator.updateReading(val, timestamp);
}
