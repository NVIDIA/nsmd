/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Branch coverage tests for nsmd/nsmNumericSensor/nsmThresholdFactory.cpp
 *
 * Covers:
 * - Constructor
 * - make(): no thresholds → no sensors
 * - make(): unknown tier name → no sensors
 * - make(): LowerCaution/UpperCaution by value → static threshold
 * - make(): LowerCritical/UpperCritical by value → static critical threshold
 * - make(): LowerFatal/UpperFatal by value → static hard-shutdown threshold
 * - make(): Soft shutdown / performance loss tiers by value
 * - make(): ParameterId, no PeriodicUpdate → addStaticSensor
 * - make(): ParameterId, periodicUpdate=true → roundRobin
 * - make(): ParameterId, periodicUpdate=true, priority → priority queue
 * - make(): ParameterId, periodicUpdate=true, aggregated → aggregator
 * - make(): both lower and upper caution → two device sensors
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmNumericSensorFactory.hpp"
#include "nsmThresholdAggregator.hpp"
#include "nsmThresholdFactory.hpp"
#include "nsmThresholdValue.hpp"

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Stub NsmNumericSensor for testing the factory
// ============================================================================

class StubThresholdNumericSensor : public nsm::NsmNumericSensor
{
  public:
    StubThresholdNumericSensor(const std::string& name, uint8_t sensorId) :
        nsm::NsmNumericSensor(
            name, "stub", sensorId,
            std::make_shared<nsm::NsmNumericSensorValueAggregate>())
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
        return "temperature";
    }
};

using Thresholds = std::unordered_map<std::string, FeatureThreshold>;

// ============================================================================
// Test fixture
// ============================================================================

struct NsmThresholdFactoryTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t sensorUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nsmDev;

    NsmThresholdFactoryTest() : SensorManagerTest(devices)
    {
        nsmDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(sensorUuid));
        EXPECT_NE(nsmDev, nullptr);
    }

    ~NsmThresholdFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<StubThresholdNumericSensor> makeSensor() const
    {
        return std::make_shared<StubThresholdNumericSensor>("TestSensor", 0);
    }

    // Unique sensor name per test to avoid D-Bus object conflicts
    NumericSensorInfo makeInfo(const std::string& suffix = "") const
    {
        NumericSensorInfo info{};
        info.name = "TempSensor" + suffix;
        return info;
    }

    // Threshold given by value (static)
    static Thresholds staticThreshold(const std::string& tier, double value)
    {
        FeatureThreshold threshold{};
        threshold.value = value;
        return {{tier, threshold}};
    }

    // Threshold read from the device by ParameterId (dynamic)
    static Thresholds dynamicThreshold(const std::string& tier,
                                       uint64_t parameterId,
                                       bool periodicUpdate = false,
                                       bool priority = false,
                                       bool aggregated = false)
    {
        return {{tier, FeatureThreshold{.parameterId = parameterId,
                                        .periodicUpdate = periodicUpdate,
                                        .priority = priority,
                                        .aggregated = aggregated}}};
    }

    void make(const NumericSensorInfo& info, Thresholds thresholds)
    {
        NsmThresholdFactory factory(mockManager, makeSensor(), info, sensorUuid,
                                    nsmDev, std::move(thresholds));
        auto rc = factory.make();
        EXPECT_EQ(rc.data(), NSM_SUCCESS);
    }
};

// ============================================================================
// Constructor test
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Constructor_Succeeds)
{
    auto sensor = makeSensor();
    auto info = makeInfo();
    EXPECT_NO_THROW(NsmThresholdFactory(mockManager, sensor, info, sensorUuid,
                                        nsmDev, Thresholds{}));
}

// ============================================================================
// make() – no thresholds → no sensors
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_NoThresholds_NoSensorsAdded)
{
    auto info = makeInfo("_esm");
    size_t beforeDev = nsmDev->deviceSensors.size();
    size_t beforeCap = nsmDev->capabilityRefreshSensors.size();
    size_t beforeStatic = nsmDev->staticSensors.size();
    size_t beforeRr = nsmDev->roundRobinSensors.size();
    make(info, {});

    EXPECT_EQ(nsmDev->deviceSensors.size(), beforeDev);
    EXPECT_EQ(nsmDev->capabilityRefreshSensors.size(), beforeCap);
    EXPECT_EQ(nsmDev->staticSensors.size(), beforeStatic);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), beforeRr);
}

// ============================================================================
// make() – tier name that is no threshold tier → no sensors
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_UnknownTier_NoSensorsAdded)
{
    auto info = makeInfo("_nmi");
    size_t beforeDev = nsmDev->deviceSensors.size();
    size_t beforeStatic = nsmDev->staticSensors.size();
    make(info, staticThreshold("SomeUnrelated", 75.0));

    EXPECT_EQ(nsmDev->deviceSensors.size(), beforeDev);
    EXPECT_EQ(nsmDev->staticSensors.size(), beforeStatic);
}

// ============================================================================
// make() – LowerCaution by default value → static threshold with 0.0
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_LowerCaution_DefaultValue_DeviceSensor)
{
    auto info = makeInfo("_lcnd");
    size_t before = nsmDev->deviceSensors.size();
    make(info, {{"LowerCaution", FeatureThreshold{}}});

    ASSERT_EQ(nsmDev->deviceSensors.size(), before + 1);
    auto* thresholdValue = dynamic_cast<NsmThresholdValueWarningLow*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->warningLow(), 0.0);
}

// ============================================================================
// make() – LowerCaution with value → static with value
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_LowerCaution_StaticWithValue_DeviceSensorAdded)
{
    auto info = makeInfo("_lcwv");
    size_t before = nsmDev->deviceSensors.size();
    make(info, staticThreshold("LowerCaution", 85.0));

    ASSERT_EQ(nsmDev->deviceSensors.size(), before + 1);
    auto* thresholdValue = dynamic_cast<NsmThresholdValueWarningLow*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->warningLow(), 85.0);
}

// ============================================================================
// make() – UpperCaution by value → static upper warning threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_UpperCaution_Static_DeviceSensorAdded)
{
    auto info = makeInfo("_uc");
    size_t before = nsmDev->deviceSensors.size();
    make(info, staticThreshold("UpperCaution", 95.0));

    ASSERT_EQ(nsmDev->deviceSensors.size(), before + 1);
    auto* thresholdValue = dynamic_cast<NsmThresholdValueWarningHigh*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->warningHigh(), 95.0);
}

// ============================================================================
// make() – LowerCritical by value → static critical threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_LowerCritical_Static_DeviceSensorAdded)
{
    auto info = makeInfo("_lcrit");
    size_t before = nsmDev->deviceSensors.size();
    make(info, staticThreshold("LowerCritical", 70.0));

    ASSERT_EQ(nsmDev->deviceSensors.size(), before + 1);
    auto* thresholdValue = dynamic_cast<NsmThresholdValueCriticalLow*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->criticalLow(), 70.0);
}

// ============================================================================
// make() – UpperCritical by value → static critical high threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_UpperCritical_Static_DeviceSensorAdded)
{
    auto info = makeInfo("_ucrit");
    size_t before = nsmDev->deviceSensors.size();
    make(info, staticThreshold("UpperCritical", 105.0));

    ASSERT_EQ(nsmDev->deviceSensors.size(), before + 1);
    auto* thresholdValue = dynamic_cast<NsmThresholdValueCriticalHigh*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->criticalHigh(), 105.0);
}

// ============================================================================
// make() – LowerFatal by value → static hard-shutdown low threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_LowerFatal_Static_DeviceSensorAdded)
{
    auto info = makeInfo("_lf");
    size_t before = nsmDev->deviceSensors.size();
    make(info, staticThreshold("LowerFatal", 60.0));

    ASSERT_EQ(nsmDev->deviceSensors.size(), before + 1);
    auto* thresholdValue = dynamic_cast<NsmThresholdValueHardShutdownLow*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->hardShutdownLow(), 60.0);
}

// ============================================================================
// make() – UpperFatal by value → static hard-shutdown high threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_UpperFatal_Static_DeviceSensorAdded)
{
    auto info = makeInfo("_uf");
    size_t before = nsmDev->deviceSensors.size();
    make(info, staticThreshold("UpperFatal", 120.0));

    ASSERT_EQ(nsmDev->deviceSensors.size(), before + 1);
    auto* thresholdValue = dynamic_cast<NsmThresholdValueHardShutdownHigh*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->hardShutdownHigh(), 120.0);
}

// ============================================================================
// make() – LowerSoftShutdown by value → static soft-shutdown low threshold
// (STH-REQ-09 — infra-only tier, no sensor configured with it yet)
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_LowerSoftShutdown_Static_DeviceSensorAdded)
{
    auto info = makeInfo("_lss");
    size_t before = nsmDev->deviceSensors.size();
    make(info, staticThreshold("LowerSoftShutdown", 65.0));

    ASSERT_GT(nsmDev->deviceSensors.size(), before);
    auto* thresholdValue = dynamic_cast<NsmThresholdValueSoftShutdownLow*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->softShutdownLow(), 65.0);
}

// ============================================================================
// make() – UpperSoftShutdown by value → static soft-shutdown high threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_UpperSoftShutdown_Static_DeviceSensorAdded)
{
    auto info = makeInfo("_uss");
    size_t before = nsmDev->deviceSensors.size();
    make(info, staticThreshold("UpperSoftShutdown", 110.0));

    ASSERT_GT(nsmDev->deviceSensors.size(), before);
    auto* thresholdValue = dynamic_cast<NsmThresholdValueSoftShutdownHigh*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->softShutdownHigh(), 110.0);
}

// ============================================================================
// make() – LowerPerformanceLoss by value → static performance-loss low
// threshold (STH-REQ-09 — infra-only tier, no sensor configured with it yet)
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_LowerPerformanceLoss_Static_DeviceSensorAdded)
{
    auto info = makeInfo("_lpl");
    size_t before = nsmDev->deviceSensors.size();
    make(info, staticThreshold("LowerPerformanceLoss", 80.0));

    ASSERT_GT(nsmDev->deviceSensors.size(), before);
    auto* thresholdValue = dynamic_cast<NsmThresholdValuePerformanceLossLow*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->performanceLossLow(), 80.0);
}

// ============================================================================
// make() – UpperPerformanceLoss by value → static performance-loss high
// threshold
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_UpperPerformanceLoss_Static_DeviceSensorAdded)
{
    auto info = makeInfo("_upl");
    size_t before = nsmDev->deviceSensors.size();
    make(info, staticThreshold("UpperPerformanceLoss", 90.0));

    ASSERT_GT(nsmDev->deviceSensors.size(), before);
    auto* thresholdValue = dynamic_cast<NsmThresholdValuePerformanceLossHigh*>(
        nsmDev->deviceSensors.back().get());
    ASSERT_NE(thresholdValue, nullptr);
    EXPECT_DOUBLE_EQ(thresholdValue->intf->performanceLossHigh(), 90.0);
}

// ============================================================================
// make() – both LowerCaution and UpperCaution → two device sensors added
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_BothWarningThresholds_TwoSensorsAdded)
{
    auto info = makeInfo("_bw");
    auto thresholds = staticThreshold("LowerCaution", 75.0);
    thresholds.merge(staticThreshold("UpperCaution", 95.0));

    size_t before = nsmDev->deviceSensors.size();
    make(info, std::move(thresholds));

    // Both lower and upper threshold → two device sensors added
    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 2u);
}

// ============================================================================
// make() – ParameterId, no PeriodicUpdate → addStaticSensor and
// capability refresh
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Make_DynamicNoPeriodicUpdate_StaticSensorAdded)
{
    auto info = makeInfo("_dns");
    size_t beforeStatic = nsmDev->staticSensors.size();
    size_t beforeCapRefresh = nsmDev->capabilityRefreshSensors.size();
    size_t beforeRr = nsmDev->roundRobinSensors.size();
    make(info, dynamicThreshold("LowerCaution", /*parameterId=*/1));

    EXPECT_GT(nsmDev->staticSensors.size(), beforeStatic);
    EXPECT_GT(nsmDev->capabilityRefreshSensors.size(), beforeCapRefresh);
    EXPECT_EQ(nsmDev->roundRobinSensors.size(), beforeRr);
}

// ============================================================================
// make() – ParameterId, periodicUpdate=true, not aggregated →
// roundRobinSensors grows
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_DynamicPeriodicNotAggregated_RoundRobinAdded)
{
    auto info = makeInfo("_dpu");
    size_t before = nsmDev->roundRobinSensors.size();
    make(info, dynamicThreshold("LowerCaution", /*parameterId=*/1,
                                /*periodicUpdate=*/true));

    EXPECT_EQ(nsmDev->roundRobinSensors.size(), before + 1);
    EXPECT_TRUE(nsmDev->sensorAggregators.empty());
}

// ============================================================================
// make() – ParameterId, periodicUpdate=true, priority=true →
// prioritySensors grows
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_DynamicPeriodicPriority_PrioritySensorAdded)
{
    auto info = makeInfo("_dpp");
    size_t before = nsmDev->prioritySensors.size();
    make(info, dynamicThreshold("LowerCaution", /*parameterId=*/2,
                                /*periodicUpdate=*/true, /*priority=*/true));

    EXPECT_EQ(nsmDev->prioritySensors.size(), before + 1);
}

// ============================================================================
// make() – ParameterId, periodicUpdate=true, aggregated=true → new aggregator
// created (NsmThresholdAggregatorBuilder path)
// ============================================================================

TEST_F(NsmThresholdFactoryTest,
       Make_DynamicPeriodicAggregated_AggregatorCreated)
{
    auto info = makeInfo("_dpa");
    size_t before = nsmDev->sensorAggregators.size();
    make(info, dynamicThreshold("LowerCaution", /*parameterId=*/3,
                                /*periodicUpdate=*/true, /*priority=*/false,
                                /*aggregated=*/true));

    ASSERT_EQ(nsmDev->sensorAggregators.size(), before + 1);
    EXPECT_NE(std::dynamic_pointer_cast<NsmThresholdAggregator>(
                  nsmDev->sensorAggregators.back()),
              nullptr);
}
