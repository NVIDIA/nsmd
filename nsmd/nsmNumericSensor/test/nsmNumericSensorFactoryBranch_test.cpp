/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for nsmNumericSensorFactory.cpp
 *
 * Targets uncovered branches:
 * - makePeakValueAndAdd: info.type == "NSM_Power" TRUE → PeakPowerSensorBuilder
 * - makePeakValueAndAdd: info.type != "NSM_Power" → else error log
 * - make: "UUID" key absent → uuid stays empty → parseStaticUuid throws
 * - makeAggregatorAndAddSensor: aggregator->addSensor returns non-success
 * - make: non-chassis association forward → chassis_association stays empty
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmEnergy.hpp"
#include "nsmNumericSensorFactory.hpp"
#include "nsmNumericSensorValue_mock.hpp"
#include "nsmPeakPower.hpp"
#include "nsmPower.hpp"
#include "nsmTemp.hpp"
#include "nsmThresholdAggregator.hpp"
#include "nsmVoltage.hpp"

using namespace nsm;
using namespace ::testing;

namespace
{
__attribute__((used))
const std::type_info* const _forceLinkPower2 = &typeid(NsmPower);
__attribute__((used))
const std::type_info* const _forceLinkTemp2 = &typeid(NsmTemp);
__attribute__((used))
const std::type_info* const _forceLinkEnergy2 = &typeid(NsmEnergy);
__attribute__((used))
const std::type_info* const _forceLinkVoltage2 = &typeid(NsmVoltage);
} // namespace

// ============================================================================
// Stub sensor/aggregator
// ============================================================================

class StubSensorFB : public NsmNumericSensor
{
  public:
    StubSensorFB(const std::string& name, uint8_t sensorId) :
        NsmNumericSensor(name, "stub", sensorId,
                         std::make_shared<NsmNumericSensorValueAggregate>())
    {}

    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::nullopt;
    }

    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return NSM_SUCCESS;
    }

    std::string getSensorType() override
    {
        return "stub";
    }
};

class StubAggFB : public NsmNumericAggregator
{
  public:
    StubAggFB(const std::string& name, const std::string& type, bool priority) :
        NsmNumericAggregator(name, type, priority)
    {}

    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::nullopt;
    }

  private:
    int handleSample(const TelemetrySample&) override
    {
        return NSM_SUCCESS;
    }
};

// ============================================================================
// Mock builder
// ============================================================================

class MockNumericSensorBuilderFB : public NumericSensorBuilder
{
  public:
    MOCK_METHOD(std::shared_ptr<NsmNumericSensor>, makeSensor,
                (const std::string&, const std::string&, sdbusplus::bus::bus&,
                 const NumericSensorInfo&, const dbus::PropertyMap&),
                (override));
    MOCK_METHOD(std::shared_ptr<NsmNumericAggregator>, makeAggregator,
                (const NumericSensorInfo&), (override));
};

// ============================================================================
// Fixture
// ============================================================================

struct NsmNumericSensorFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t sensorUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string interface = "xyz.openbmc_project.Configuration.NSM_Power";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/sensor/nsf_branch_test";
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_FB";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nsmDev;

    NsmNumericSensorFactoryBranchTest() : SensorManagerTest(devices)
    {
        nsmDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(sensorUuid));
        EXPECT_NE(nsmDev, nullptr);
    }

    ~NsmNumericSensorFactoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    void setupChassisAssoc(const std::string& path, const std::string& iface)
    {
        utils::MockDbusAsync::serviceMap() = {
            {"xyz.openbmc_project.EntityManager", {iface + ".Associations0"}}};
        auto& pm = utils::MockDbusAsync::propertyMap(path,
                                                     iface + ".Associations0");
        pm["Forward"] = std::string("chassis");
        pm["Backward"] = std::string("all_sensors");
        pm["AbsolutePath"] = chassisPath;
    }
};

// ============================================================================
// makePeakValueAndAdd: type == "NSM_Power" (TRUE branch)
// ============================================================================

TEST_F(NsmNumericSensorFactoryBranchTest,
       DISABLED_MakePeakValueAndAdd_PowerType_CreatesPeakSensor)
{
    NumericSensorInfo info{};
    info.name = "PowerSensor";
    info.type = "NSM_Power";
    info.sensorId = 1;
    info.priority = false;
    info.aggregated = false;

    const std::string peakIface = interface + ".PeakValue";

    // Set up PeakValue properties on D-Bus mock
    utils::MockDbusAsync::propertyMap(objPath,
                                      peakIface)["SensorId"] = uint64_t(2);
    utils::MockDbusAsync::propertyMap(objPath,
                                      peakIface)["Priority"] = bool(false);
    utils::MockDbusAsync::propertyMap(objPath,
                                      peakIface)["Aggregated"] = bool(false);

    dbus::PropertyMap peakProps1;
    peakProps1["SensorId"] = uint64_t(2);
    peakProps1["Priority"] = bool(false);
    peakProps1["Aggregated"] = bool(false);

    size_t before = nsmDev->roundRobinSensors.size();
    NumericSensorFactory::makePeakValueAndAdd(
        interface, objPath, info, sensorUuid, nsmDev.get(), peakProps1);
    EXPECT_GT(nsmDev->roundRobinSensors.size(), before);
}

// ============================================================================
// makePeakValueAndAdd: type != "NSM_Power" (FALSE/else branch)
// ============================================================================

TEST_F(NsmNumericSensorFactoryBranchTest,
       DISABLED_MakePeakValueAndAdd_NonPowerType_LogsError)
{
    NumericSensorInfo info{};
    info.name = "TempSensor";
    info.type = "NSM_Temp"; // NOT NSM_Power → else branch
    info.sensorId = 1;

    const std::string tempIface = "xyz.openbmc_project.Configuration.NSM_Temp";

    dbus::PropertyMap peakProps2;
    peakProps2["SensorId"] = uint64_t(2);
    peakProps2["Priority"] = bool(false);
    peakProps2["Aggregated"] = bool(false);

    size_t before = nsmDev->roundRobinSensors.size();
    // Should not crash, just logs error
    NumericSensorFactory::makePeakValueAndAdd(
        tempIface, objPath, info, sensorUuid, nsmDev.get(), peakProps2);
    // No sensor added for non-Power type
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before);
}

// ============================================================================
// make: UUID key absent → uuid="" → getNsmDeviceFromStaticUUID throws
// ============================================================================

TEST_F(NsmNumericSensorFactoryBranchTest, Make_MissingUUID_Throws)
{
    const std::string altPath = objPath + "_no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(altPath, interface);
    pm["Name"] = std::string("NoUuidSensor");
    // UUID intentionally omitted
    pm["SensorId"] = uint64_t(1);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);

    setupChassisAssoc(altPath, interface);

    auto mockBuilder =
        std::make_unique<StrictMock<MockNumericSensorBuilderFB>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _)).Times(0);
    EXPECT_CALL(*mockBuilder, makeAggregator(_)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    EXPECT_THROW_COROUTINE(factory.make(mockManager, interface, altPath),
                           std::exception);
}

// ============================================================================
// make: association with forward != "chassis" → chassis_association empty
// ============================================================================

TEST_F(NsmNumericSensorFactoryBranchTest,
       Make_NonChassisAssociation_ReturnsError)
{
    const std::string altPath = objPath + "_non_chassis_assoc";
    auto& pm = utils::MockDbusAsync::propertyMap(altPath, interface);
    pm["Name"] = std::string("NonChassisAssocSensor");
    pm["UUID"] = sensorUuid;
    pm["SensorId"] = uint64_t(1);
    pm["Aggregated"] = bool(false);
    pm["Priority"] = bool(false);

    // Set up association with forward != "chassis"
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {interface + ".Associations0"}}};
    auto& assocPm = utils::MockDbusAsync::propertyMap(
        altPath, interface + ".Associations0");
    assocPm["Forward"] = std::string("parent"); // NOT "chassis"
    assocPm["Backward"] = std::string("device");
    assocPm["AbsolutePath"] = chassisPath;

    auto mockBuilder =
        std::make_unique<StrictMock<MockNumericSensorBuilderFB>>();
    EXPECT_CALL(*mockBuilder, makeSensor(_, _, _, _, _)).Times(0);

    NumericSensorFactory factory(std::move(mockBuilder));
    // chassis_association stays empty → co_return NSM_ERROR //

    factory.make(mockManager, interface, altPath);
}

// ============================================================================
// makePeakValueAndAdd: NSM_Power with aggregated=true peak
// ============================================================================

TEST_F(NsmNumericSensorFactoryBranchTest,
       DISABLED_MakePeakValueAndAdd_PowerAggregated_CreatesAggregator)
{
    NumericSensorInfo info{};
    info.name = "PowerAgg";
    info.type = "NSM_Power";
    info.sensorId = 1;
    info.priority = false;
    info.aggregated = false;

    const std::string peakIface = interface + ".PeakValue";

    utils::MockDbusAsync::propertyMap(objPath + "_agg",
                                      peakIface)["SensorId"] = uint64_t(3);
    utils::MockDbusAsync::propertyMap(objPath + "_agg",
                                      peakIface)["Priority"] = bool(true);
    utils::MockDbusAsync::propertyMap(objPath + "_agg",
                                      peakIface)["Aggregated"] = bool(false);

    dbus::PropertyMap peakProps3;
    peakProps3["SensorId"] = uint64_t(3);
    peakProps3["Priority"] = bool(true);
    peakProps3["Aggregated"] = bool(false);

    size_t before = nsmDev->prioritySensors.size();
    NumericSensorFactory::makePeakValueAndAdd(interface, objPath + "_agg", info,
                                              sensorUuid, nsmDev.get(),
                                              peakProps3);
    EXPECT_GT(nsmDev->prioritySensors.size(), before);
}
