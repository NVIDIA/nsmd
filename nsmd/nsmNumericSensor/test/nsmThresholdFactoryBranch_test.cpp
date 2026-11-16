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
 * Branch coverage tests for nsmThresholdFactory.cpp & nsmThreshold.cpp
 *
 * Targets branches:
 *   - processThresholdsPair: lower found / upper not found
 *   - processThresholdsPair: upper found / lower not found
 *   - processThresholdsPair: neither found (no-op)
 *   - createNsmThreshold: ParameterId + periodic + aggregated + priority
 *   - NsmThreshold::genRequestMsg encode fail (instanceId=255)
 *   - NsmThreshold::genRequestMsg success
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmNumericSensorFactory.hpp"
#include "nsmThreshold.hpp"
#include "nsmThresholdAggregator.hpp"
#include "nsmThresholdFactory.hpp"
#include "nsmThresholdValue.hpp"

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Stub NsmNumericSensor for testing the factory
// ============================================================================

class StubThresholdBranchSensor : public nsm::NsmNumericSensor
{
  public:
    StubThresholdBranchSensor(const std::string& name, uint8_t sensorId) :
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

struct NsmThresholdFactoryBranchTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t sensorUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nsmDev;

    NsmThresholdFactoryBranchTest() : SensorManagerTest(devices)
    {
        nsmDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(sensorUuid));
        EXPECT_NE(nsmDev, nullptr);
    }

    ~NsmThresholdFactoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<StubThresholdBranchSensor> makeSensor() const
    {
        return std::make_shared<StubThresholdBranchSensor>("BranchSensor", 0);
    }

    NumericSensorInfo makeInfo(const std::string& suffix = "") const
    {
        NumericSensorInfo info{};
        info.name = "BranchSensor" + suffix;
        return info;
    }

    static FeatureThreshold byValue(double value)
    {
        FeatureThreshold threshold{};
        threshold.value = value;
        return threshold;
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
// processThresholdsPair: only lower threshold found, upper absent
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessThresholdsPair_OnlyLowerFound_OneSensorAdded)
{
    auto info = makeInfo("_olf");
    size_t before = nsmDev->deviceSensors.size();
    make(info, {{"LowerCaution", byValue(65.0)}});

    // Only 1 sensor added (lower only, no upper)
    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 1u);
}

// ============================================================================
// processThresholdsPair: only upper threshold found, lower absent
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessThresholdsPair_OnlyUpperFound_OneSensorAdded)
{
    auto info = makeInfo("_ouf");
    size_t before = nsmDev->deviceSensors.size();
    make(info, {{"UpperCaution", byValue(95.0)}});

    // Only 1 sensor added (upper only, no lower)
    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 1u);
}

// ============================================================================
// processThresholdsPair: neither threshold found (names don't match)
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessThresholdsPair_NeitherFound_NoSensorsAdded)
{
    auto info = makeInfo("_nf");
    size_t beforeDev = nsmDev->deviceSensors.size();
    size_t beforeStatic = nsmDev->staticSensors.size();
    make(info, {{"SomeUnrelated", byValue(1.0)}});

    EXPECT_EQ(nsmDev->deviceSensors.size(), beforeDev);
    EXPECT_EQ(nsmDev->staticSensors.size(), beforeStatic);
}

// ============================================================================
// Critical threshold pair: only lower found
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessCriticalPair_OnlyLowerCritical_OneSensorAdded)
{
    auto info = makeInfo("_olcrit");
    size_t before = nsmDev->deviceSensors.size();
    make(info, {{"LowerCritical", byValue(55.0)}});

    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 1u);
}

// ============================================================================
// Fatal threshold pair: only upper found
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessFatalPair_OnlyUpperFatal_OneSensorAdded)
{
    auto info = makeInfo("_oufat");
    size_t before = nsmDev->deviceSensors.size();
    make(info, {{"UpperFatal", byValue(125.0)}});

    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 1u);
}

// ============================================================================
// ParameterId + periodic + aggregated + priority → aggregator with priority
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       CreateNsmThreshold_DynamicPeriodicAggregatedPriority)
{
    auto info = makeInfo("_dpap");
    size_t beforeAgg = nsmDev->sensorAggregators.size();
    make(info, {{"UpperCritical", FeatureThreshold{.parameterId = 42,
                                                   .periodicUpdate = true,
                                                   .priority = true,
                                                   .aggregated = true}}});

    ASSERT_EQ(nsmDev->sensorAggregators.size(), beforeAgg + 1);
    EXPECT_TRUE(nsmDev->sensorAggregators.back()->priority);
}

// ============================================================================
// Both critical thresholds: lower + upper → two sensors
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessCriticalPair_BothThresholds_TwoSensorsAdded)
{
    auto info = makeInfo("_bcrit");
    size_t before = nsmDev->deviceSensors.size();
    make(info,
         {{"LowerCritical", byValue(60.0)}, {"UpperCritical", byValue(110.0)}});

    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 2u);
}

// ============================================================================
// Both fatal thresholds: lower + upper → two sensors
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessFatalPair_BothThresholds_TwoSensorsAdded)
{
    auto info = makeInfo("_bfat");
    size_t before = nsmDev->deviceSensors.size();
    make(info, {{"LowerFatal", byValue(50.0)}, {"UpperFatal", byValue(130.0)}});

    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 2u);
}

// ============================================================================
// NsmThreshold: genRequestMsg with invalid instanceId → encode fails → nullopt
// ============================================================================

TEST(NsmThresholdBranch, GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    auto mock = std::make_shared<NsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"ThreshBranch", "NSM_Threshold", 0, mock};

    auto result = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// NsmThreshold: genRequestMsg with valid instanceId → returns request
// ============================================================================

TEST(NsmThresholdBranch, GenRequestMsg_ValidInstanceId_ReturnsRequest)
{
    auto mock = std::make_shared<NsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"ThreshBranch", "NSM_Threshold", 5, mock};

    auto result = sensor.genRequestMsg(10, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req));
}

// ============================================================================
// NsmThreshold: handleResponseMsg success path updates reading
// ============================================================================

TEST(NsmThresholdBranch, HandleResponseMsg_Success_UpdatesReading)
{
    auto mock = std::make_shared<NsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"ThreshBranch", "NSM_Threshold", 0, mock};

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto respMsg = reinterpret_cast<nsm_msg*>(response.data());

    const int32_t threshold = 42;
    auto rc = encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 threshold, respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(respMsg, response.size());
    EXPECT_EQ(result, NSM_SUCCESS);
}

// ============================================================================
// NsmThreshold: handleResponseMsg with cc error → NaN path
// ============================================================================

TEST(NsmThresholdBranch, HandleResponseMsg_CCError_NaN)
{
    auto mock = std::make_shared<NsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"ThreshBranch", "NSM_Threshold", 0, mock};

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    response[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto respMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto result = sensor.handleResponseMsg(respMsg, response.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmThreshold: handleResponseMsg decode fail (truncated buffer)
// ============================================================================

TEST(NsmThresholdBranch, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto mock = std::make_shared<NsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"ThreshBranch", "NSM_Threshold", 0, mock};

    // Too short for decode_read_thermal_parameter_resp
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto respMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto result = sensor.handleResponseMsg(respMsg, response.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmThresholdAggregatorBuilder::makeAggregator direct test
// ============================================================================

TEST(NsmThresholdBranch, ThresholdAggregatorBuilder_MakeAggregator)
{
    // NsmThresholdAggregatorBuilder is defined in nsmThresholdFactory.cpp
    // as a class in namespace nsm. Since we use #define private public, we
    // can access it. However, it is a local class. We test it indirectly
    // through the factory with aggregated=true + periodicUpdate=true.
    // The factory test above (DynamicPeriodicAggregatedPriority) covers this.
    // This test verifies NsmThresholdAggregator constructor directly.
    NsmThresholdAggregator agg("testAgg", "NSM_ThermalParameter", false);
    EXPECT_EQ(agg.getName(), "testAgg");
}

// ============================================================================
// Known and unknown tiers mixed: only the known tier is processed
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest, MixedTiers_OnlyKnownTierProcessed)
{
    auto info = makeInfo("_mix");
    size_t before = nsmDev->deviceSensors.size();
    make(info, {{"LowerCaution", byValue(70.0)}, {"Other", byValue(1.0)}});

    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 1u);
}

// ============================================================================
// ParameterId, no periodic update → static + capability refresh
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       CreateNsmThreshold_DynamicNoPeriodicUpdate_StaticCapRefresh)
{
    auto info = makeInfo("_dap");
    size_t beforeStatic = nsmDev->staticSensors.size();
    size_t beforeCap = nsmDev->capabilityRefreshSensors.size();
    make(info, {{"UpperCaution", FeatureThreshold{.parameterId = 7}}});

    EXPECT_GT(nsmDev->staticSensors.size(), beforeStatic);
    EXPECT_GT(nsmDev->capabilityRefreshSensors.size(), beforeCap);
}
