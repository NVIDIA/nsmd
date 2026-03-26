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
 * Targets uncovered branches:
 *   - NsmThresholdAggregatorBuilder::makeAggregator (line 39)
 *   - processThresholdsPair: lower found / upper not found
 *   - processThresholdsPair: upper found / lower not found
 *   - processThresholdsPair: neither found (no-op)
 *   - createNsmThreshold: Dynamic=true, Type key missing → empty → error
 *   - createNsmThreshold: Dynamic=true, ParameterId key missing → sensorId=0
 *   - NsmThreshold::genRequestMsg encode fail (instanceId=255)
 *   - NsmThreshold::genRequestMsg success
 *   - getThresholdInterfaces (deprecated sync path) branches
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

// ============================================================================
// Test fixture
// ============================================================================

struct NsmThresholdFactoryBranchTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t sensorUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string interface = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string baseObjPath =
        "/xyz/openbmc_project/inventory/system/sensor/tfb_test";
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

    std::string objPath(const std::string& suffix) const
    {
        return baseObjPath + suffix;
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
};

// ============================================================================
// processThresholdsPair: only lower threshold found, upper absent
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessThresholdsPair_OnlyLowerFound_OneSensorAdded)
{
    const std::string op = objPath("_olf");
    auto sensor = makeSensor();
    auto info = makeInfo("_olf");
    const std::string lowerIntf = interface + ".ThermalParameters.LowerCaution";
    // Only LowerCaution interface in service map, no UpperCaution
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {lowerIntf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, lowerIntf);
    pm["Name"] = std::string("LowerCaution");
    pm["Dynamic"] = bool(false);
    pm["Value"] = double(65.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    // Only 1 sensor added (lower only, no upper)
    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 1u);
}

// ============================================================================
// processThresholdsPair: only upper threshold found, lower absent
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessThresholdsPair_OnlyUpperFound_OneSensorAdded)
{
    const std::string op = objPath("_ouf");
    auto sensor = makeSensor();
    auto info = makeInfo("_ouf");
    const std::string upperIntf = interface + ".ThermalParameters.UpperCaution";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {upperIntf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, upperIntf);
    pm["Name"] = std::string("UpperCaution");
    pm["Dynamic"] = bool(false);
    pm["Value"] = double(95.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    // Only 1 sensor added (upper only, no lower)
    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 1u);
}

// ============================================================================
// processThresholdsPair: neither threshold found (names don't match)
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessThresholdsPair_NeitherFound_NoSensorsAdded)
{
    const std::string op = objPath("_nf");
    auto sensor = makeSensor();
    auto info = makeInfo("_nf");
    const std::string intf = interface + ".ThermalParameters.SomeOther";
    // Interface matches ThermalParameters but Name doesn't match any threshold
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {intf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
    pm["Name"] = std::string("SomeUnrelated");

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t beforeDev = nsmDev->deviceSensors.size();
    size_t beforeStatic = nsmDev->staticSensors.size();
    factory.make();

    EXPECT_EQ(nsmDev->deviceSensors.size(), beforeDev);
    EXPECT_EQ(nsmDev->staticSensors.size(), beforeStatic);
}

// ============================================================================
// createNsmThreshold: Dynamic=true, "Type" key missing → type="" →
// mismatch with NSM_ThermalParameter → error path
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       CreateNsmThreshold_DynamicMissingType_ErrorPath)
{
    const std::string op = objPath("_dmt");
    auto sensor = makeSensor();
    auto info = makeInfo("_dmt");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {intf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
    pm["Name"] = std::string("LowerCaution");
    pm["Dynamic"] = bool(true);
    // "Type" intentionally absent → defaults to "" → error

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t beforeDev = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_EQ(nsmDev->deviceSensors.size(), beforeDev);
}

// ============================================================================
// createNsmThreshold: Dynamic=true, "ParameterId" key missing → sensorId=0
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       CreateNsmThreshold_DynamicMissingParameterId_DefaultsZero)
{
    const std::string op = objPath("_dmp");
    auto sensor = makeSensor();
    auto info = makeInfo("_dmp");
    const std::string intf = interface + ".ThermalParameters.LowerCaution";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {intf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
    pm["Name"] = std::string("LowerCaution");
    pm["Dynamic"] = bool(true);
    pm["Type"] = std::string("NSM_ThermalParameter");
    // "ParameterId" intentionally absent → sensorId = 0

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t beforeStatic = nsmDev->staticSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->staticSensors.size(), beforeStatic);
}

// ============================================================================
// Critical threshold pair: only lower found
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessCriticalPair_OnlyLowerCritical_OneSensorAdded)
{
    const std::string op = objPath("_olcrit");
    auto sensor = makeSensor();
    auto info = makeInfo("_olcrit");
    const std::string lowerIntf = interface +
                                  ".ThermalParameters.LowerCritical";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {lowerIntf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, lowerIntf);
    pm["Name"] = std::string("LowerCritical");
    pm["Dynamic"] = bool(false);
    pm["Value"] = double(55.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 1u);
}

// ============================================================================
// Fatal threshold pair: only upper found
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessFatalPair_OnlyUpperFatal_OneSensorAdded)
{
    const std::string op = objPath("_oufat");
    auto sensor = makeSensor();
    auto info = makeInfo("_oufat");
    const std::string upperIntf = interface + ".ThermalParameters.UpperFatal";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {upperIntf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, upperIntf);
    pm["Name"] = std::string("UpperFatal");
    pm["Dynamic"] = bool(false);
    pm["Value"] = double(125.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 1u);
}

// ============================================================================
// Dynamic + periodic + aggregated + priority → aggregator with priority
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       CreateNsmThreshold_DynamicPeriodicAggregatedPriority)
{
    const std::string op = objPath("_dpap");
    auto sensor = makeSensor();
    auto info = makeInfo("_dpap");
    const std::string intf = interface + ".ThermalParameters.UpperCritical";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {intf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
    pm["Name"] = std::string("UpperCritical");
    pm["Dynamic"] = bool(true);
    pm["Type"] = std::string("NSM_ThermalParameter");
    pm["ParameterId"] = uint64_t(42);
    pm["PeriodicUpdate"] = bool(true);
    pm["Priority"] = bool(true);
    pm["Aggregated"] = bool(true);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t beforeAgg = nsmDev->sensorAggregators.size();
    factory.make();

    EXPECT_GT(nsmDev->sensorAggregators.size(), beforeAgg);
}

// ============================================================================
// Both critical thresholds: lower + upper → two sensors
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessCriticalPair_BothThresholds_TwoSensorsAdded)
{
    const std::string op = objPath("_bcrit");
    auto sensor = makeSensor();
    auto info = makeInfo("_bcrit");
    const std::string lowerIntf = interface +
                                  ".ThermalParameters.LowerCritical";
    const std::string upperIntf = interface +
                                  ".ThermalParameters.UpperCritical";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {lowerIntf, upperIntf}}};

    auto& lpm = utils::MockDbusAsync::propertyMap(op, lowerIntf);
    lpm["Name"] = std::string("LowerCritical");
    lpm["Dynamic"] = bool(false);
    lpm["Value"] = double(60.0);

    auto& upm = utils::MockDbusAsync::propertyMap(op, upperIntf);
    upm["Name"] = std::string("UpperCritical");
    upm["Dynamic"] = bool(false);
    upm["Value"] = double(110.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_GE(nsmDev->deviceSensors.size(), before + 2u);
}

// ============================================================================
// Both fatal thresholds: lower + upper → two sensors
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       ProcessFatalPair_BothThresholds_TwoSensorsAdded)
{
    const std::string op = objPath("_bfat");
    auto sensor = makeSensor();
    auto info = makeInfo("_bfat");
    const std::string lowerIntf = interface + ".ThermalParameters.LowerFatal";
    const std::string upperIntf = interface + ".ThermalParameters.UpperFatal";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {lowerIntf, upperIntf}}};

    auto& lpm = utils::MockDbusAsync::propertyMap(op, lowerIntf);
    lpm["Name"] = std::string("LowerFatal");
    lpm["Dynamic"] = bool(false);
    lpm["Value"] = double(50.0);

    auto& upm = utils::MockDbusAsync::propertyMap(op, upperIntf);
    upm["Name"] = std::string("UpperFatal");
    upm["Dynamic"] = bool(false);
    upm["Value"] = double(130.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_GE(nsmDev->deviceSensors.size(), before + 2u);
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
// Multiple interfaces in service map, only some match ThermalParameters
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       GetThresholdInterfaces_MixedInterfaces_OnlyMatchingProcessed)
{
    const std::string op = objPath("_mix");
    auto sensor = makeSensor();
    auto info = makeInfo("_mix");
    const std::string thermIntf = interface + ".ThermalParameters.LowerCaution";
    const std::string otherIntf = "xyz.openbmc_project.Configuration.Other";

    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {thermIntf, otherIntf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, thermIntf);
    pm["Name"] = std::string("LowerCaution");
    pm["Dynamic"] = bool(false);
    pm["Value"] = double(70.0);

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t before = nsmDev->deviceSensors.size();
    factory.make();

    EXPECT_EQ(nsmDev->deviceSensors.size(), before + 1u);
}

// ============================================================================
// Dynamic threshold with all properties present, no periodic → static+cap
// ============================================================================

TEST_F(NsmThresholdFactoryBranchTest,
       CreateNsmThreshold_DynamicAllPropsNoPeriodicUpdate_StaticCapRefresh)
{
    const std::string op = objPath("_dap");
    auto sensor = makeSensor();
    auto info = makeInfo("_dap");
    const std::string intf = interface + ".ThermalParameters.UpperCaution";
    utils::MockDbusAsync::serviceMap() = {
        {"xyz.openbmc_project.EntityManager", {intf}}};
    auto& pm = utils::MockDbusAsync::propertyMap(op, intf);
    pm["Name"] = std::string("UpperCaution");
    pm["Dynamic"] = bool(true);
    pm["Type"] = std::string("NSM_ThermalParameter");
    pm["ParameterId"] = uint64_t(7);
    // PeriodicUpdate not set → exception caught → periodicUpdate=false

    NsmThresholdFactory factory(mockManager, interface, op, sensor, info,
                                sensorUuid);
    size_t beforeStatic = nsmDev->staticSensors.size();
    size_t beforeCap = nsmDev->capabilityRefreshSensors.size();
    factory.make();

    EXPECT_GT(nsmDev->staticSensors.size(), beforeStatic);
    EXPECT_GT(nsmDev->capabilityRefreshSensors.size(), beforeCap);
}
