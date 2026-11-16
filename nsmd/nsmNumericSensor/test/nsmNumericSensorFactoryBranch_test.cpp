/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for the peak value and association branches of the
 * numeric sensor feature creators:
 * - createPowerFeature: PeakValueEnabled with priority peak sensor
 * - createPowerFeature: PeakValueEnabled with aggregated peak sensor
 * - makeFeatureSensorInfo: only non-chassis associations → NSM_ERROR
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "features/creators.hpp"
#include "nsmNumericSensorFactory.hpp"
#include "nsmPeakPower.hpp"

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Fixture
// ============================================================================

struct NsmNumericSensorFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t sensorUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
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

    features::DeviceContext makeDev()
    {
        features::DeviceContext dev;
        dev.name = "HGX_GPU_0";
        dev.kind = "gpu";
        dev.staticUuid = sensorUuid;
        dev.platformPrefix = "HGX_";
        dev.device = nsmDev;
        return dev;
    }

    static dbus::PropertyMap powerParams(const std::string& name)
    {
        return {{"Name", name},
                {"SensorId", uint64_t(1)},
                {"PhysicalContext", std::string("GPU")},
                {"Priority", false},
                {"Aggregated", false},
                {"ChassisAssociationPaths",
                 std::vector<std::string>{"chassis/HGX_FB"}}};
    }
};

// ============================================================================
// PeakValueEnabled: priority peak sensor → priority queue
// ============================================================================

TEST_F(NsmNumericSensorFactoryBranchTest, PowerPeakValue_Priority_InPriorityQ)
{
    auto params = powerParams("PowerSensor");
    params["PeakValueEnabled"] = true;
    params["PeakValueSensorId"] = uint64_t(2);
    params["PeakValuePriority"] = true;
    params["PeakValueAggregated"] = false;

    const size_t rrBefore = nsmDev->roundRobinSensors.size();
    const size_t prioBefore = nsmDev->prioritySensors.size();
    auto rc = features::createPowerFeature(mockManager, makeDev(), params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Power sensor (low priority) in round robin, peak sensor in priority
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), rrBefore + 1);
    EXPECT_EQ(nsmDev->prioritySensors.size(), prioBefore + 1);
}

// ============================================================================
// PeakValueEnabled: aggregated peak sensor → peak power aggregator created
// ============================================================================

TEST_F(NsmNumericSensorFactoryBranchTest,
       PowerPeakValue_Aggregated_CreatesAggregator)
{
    auto params = powerParams("PowerAgg");
    params["PeakValueEnabled"] = true;
    params["PeakValueSensorId"] = uint64_t(3);
    params["PeakValuePriority"] = false;
    params["PeakValueAggregated"] = true;

    const size_t before = nsmDev->sensorAggregators.size();
    auto rc = features::createPowerFeature(mockManager, makeDev(), params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(nsmDev->sensorAggregators.size(), before + 1);
    EXPECT_EQ(nsmDev->sensorAggregators.back()->getType(),
              "NSM_Power_PeakValue");
}

// ============================================================================
// Only non-chassis associations → chassis_association empty → NSM_ERROR
// ============================================================================

TEST_F(NsmNumericSensorFactoryBranchTest,
       Power_NonChassisAssociation_ReturnsError)
{
    auto params = powerParams("NonChassisAssocSensor");
    params.erase("ChassisAssociationPaths");
    params["ProcessorAssociationPaths"] = std::vector<std::string>{"HGX_GPU_0"};

    const size_t before = nsmDev->roundRobinSensors.size();
    auto rc = features::createPowerFeature(mockManager, makeDev(), params);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before);
}
