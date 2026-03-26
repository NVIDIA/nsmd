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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "libnsm/platform-environmental.h"

#include "nsmLeakDetection.hpp"

#include <xyz/openbmc_project/Common/error.hpp>

namespace nsm
{
requester::Coroutine nsmLeakDetectionCreateSensors(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath);
} // namespace nsm

using namespace nsm;

// Type alias for patch value tuples
using PatchTuple =
    std::tuple<std::string,
               std::variant<bool, uint32_t, double, std::vector<uint8_t>>>;

// ============================================================================
// Fixture
// ============================================================================

struct NsmLeakDetectionBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_BR";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmLeakDetectionBranchTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmLeakDetectionBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    // Helper to create an NsmLeakDetection with a single sensor
    std::shared_ptr<NsmLeakDetection>
        makeSingleSensor(const std::string& sensorName, uint8_t sensorId)
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string n = "LeakBr_" + sensorName;
        std::vector<uint64_t> ids = {sensorId};
        std::vector<std::string> names = {sensorName};
        return std::make_shared<NsmLeakDetection>(
            n, "NSM_LeakDetection", bus, ids, names, chassisPath, 5.0, 0.0);
    }
};

// ============================================================================
// updateLeakDetectorState - sensorId not found (early return)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, UpdateLeakDetectorState_SensorIdNotFound)
{
    auto ld = makeSingleSensor("BrS1", 1);

    // Call with a sensorId not in the map - should return early without crash
    ld->updateLeakDetectorState(42, NSM_LEAK_STATE_NOMINAL_READING);

    // Verify the existing sensor was NOT modified (still unavailable from ctor)
    auto& [assocIntf, opIntf, stateIntf] = ld->leakDetectorStateIntfMap[1];
    EXPECT_FALSE(opIntf->functional());
}

// ============================================================================
// updateLeakDetectorState - LEAK state
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, UpdateLeakDetectorState_Leak)
{
    auto ld = makeSingleSensor("BrS2", 1);

    ld->updateLeakDetectorState(1, NSM_LEAK_STATE_LEAK);

    auto& [assocIntf, opIntf, stateIntf] = ld->leakDetectorStateIntfMap[1];
    EXPECT_TRUE(opIntf->functional());
    EXPECT_EQ(
        opIntf->state(),
        OperationalStatusIntf::convertStateTypeFromString(
            "xyz.openbmc_project.State.Decorator.OperationalStatus.StateType.Enabled"));
    EXPECT_EQ(
        stateIntf->detectorState(),
        StateLeakDetectorIntf::convertDetectorStateEnumFromString(
            "xyz.openbmc_project.State.LeakDetector.DetectorStateEnum.Critical"));
}

// ============================================================================
// updateLeakDetectorState - SENSOR_SHORT
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, UpdateLeakDetectorState_SensorShort)
{
    auto ld = makeSingleSensor("BrS3", 1);

    ld->updateLeakDetectorState(1, NSM_LEAK_STATE_SENSOR_SHORT);

    auto& [assocIntf, opIntf, stateIntf] = ld->leakDetectorStateIntfMap[1];
    EXPECT_TRUE(opIntf->functional());
    EXPECT_EQ(
        opIntf->state(),
        OperationalStatusIntf::convertStateTypeFromString(
            "xyz.openbmc_project.State.Decorator.OperationalStatus.StateType.Degraded"));
    EXPECT_EQ(
        stateIntf->detectorState(),
        StateLeakDetectorIntf::convertDetectorStateEnumFromString(
            "xyz.openbmc_project.State.LeakDetector.DetectorStateEnum.Unavailable"));
}

// ============================================================================
// updateLeakDetectorState - SENSOR_OPEN
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, UpdateLeakDetectorState_SensorOpen)
{
    auto ld = makeSingleSensor("BrS4", 1);

    ld->updateLeakDetectorState(1, NSM_LEAK_STATE_SENSOR_OPEN);

    auto& [assocIntf, opIntf, stateIntf] = ld->leakDetectorStateIntfMap[1];
    EXPECT_TRUE(opIntf->functional());
    EXPECT_EQ(
        opIntf->state(),
        OperationalStatusIntf::convertStateTypeFromString(
            "xyz.openbmc_project.State.Decorator.OperationalStatus.StateType.Degraded"));
}

// ============================================================================
// updateLeakDetectorState - default (unknown state value)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, UpdateLeakDetectorState_DefaultCase)
{
    auto ld = makeSingleSensor("BrS5", 1);

    ld->updateLeakDetectorState(1, 0xFE); // unknown state value

    auto& [assocIntf, opIntf, stateIntf] = ld->leakDetectorStateIntfMap[1];
    EXPECT_TRUE(opIntf->functional());
    EXPECT_EQ(
        opIntf->state(),
        OperationalStatusIntf::convertStateTypeFromString(
            "xyz.openbmc_project.State.Decorator.OperationalStatus.StateType.None"));
    EXPECT_EQ(
        stateIntf->detectorState(),
        StateLeakDetectorIntf::convertDetectorStateEnumFromString(
            "xyz.openbmc_project.State.LeakDetector.DetectorStateEnum.Unavailable"));
}

// ============================================================================
// updateSensorValue - sensorId not found (early return)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, UpdateSensorValue_SensorIdNotFound)
{
    auto ld = makeSingleSensor("BrS6", 1);

    struct nsm_leak_detection_sensors_data sensor = {};
    sensor.adc_reading = 1500;

    // sensorId 99 does not exist - should return early
    ld->updateSensorValue(99, &sensor, 3);

    // Verify existing sensor value was not changed (default is 0)
    auto& [svIntf, assocIntf, threshIntf] = ld->sensorValueIntfMap[1];
    EXPECT_DOUBLE_EQ(svIntf->value(), 0.0);
}

// ============================================================================
// updateSensorValue - numberOfThresholdLevels = 0 (no threshold branches taken)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, UpdateSensorValue_ZeroThresholds)
{
    auto ld = makeSingleSensor("BrS7", 1);

    struct nsm_leak_detection_sensors_data sensor = {};
    sensor.adc_reading = 3000;

    // numberOfThresholdLevels = 0 means none of the threshold if-branches fire
    ld->updateSensorValue(1, &sensor, 0);

    auto& [svIntf, assocIntf, threshIntf] = ld->sensorValueIntfMap[1];
    EXPECT_DOUBLE_EQ(svIntf->value(), 3.0); // 3000mV -> 3.0V
}

// ============================================================================
// updateSensorValue - numberOfThresholdLevels = 1 (only MIN_LEAK threshold)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, UpdateSensorValue_OneThreshold)
{
    auto ld = makeSingleSensor("BrS8", 1);

    struct nsm_leak_detection_sensors_data sensor = {};
    sensor.adc_reading = 2000;
    sensor.thresholds[NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK] = 400;

    // Only the first if (> MIN_LEAK=0) fires
    ld->updateSensorValue(1, &sensor, 1);

    auto& [svIntf, assocIntf, threshIntf] = ld->sensorValueIntfMap[1];
    EXPECT_DOUBLE_EQ(svIntf->value(), 2.0);
    EXPECT_DOUBLE_EQ(svIntf->minAllowableValue(), 0.4);
}

// ============================================================================
// updateSensorValue - numberOfThresholdLevels = 2 (MIN_LEAK + MAX_LEAK)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, UpdateSensorValue_TwoThresholds)
{
    auto ld = makeSingleSensor("BrS9", 1);

    // Need extra space for thresholds[1]
    std::vector<uint8_t> buf(
        sizeof(nsm_leak_detection_sensors_data) + sizeof(uint16_t), 0);
    auto* sensor =
        reinterpret_cast<nsm_leak_detection_sensors_data*>(buf.data());
    sensor->adc_reading = 2500;
    sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK] = 400;
    sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_LEAK] = 4000;

    ld->updateSensorValue(1, sensor, 2);

    auto& [svIntf, assocIntf, threshIntf] = ld->sensorValueIntfMap[1];
    EXPECT_DOUBLE_EQ(svIntf->value(), 2.5);
    EXPECT_DOUBLE_EQ(svIntf->minAllowableValue(), 0.4);
    EXPECT_DOUBLE_EQ(threshIntf->criticalLow(), 4.0);
}

// ============================================================================
// updateSensorValue - numberOfThresholdLevels = 3 (all three thresholds)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, UpdateSensorValue_ThreeThresholds)
{
    auto ld = makeSingleSensor("BrS10", 1);

    // Need extra space for thresholds[0..2]
    std::vector<uint8_t> buf(
        sizeof(nsm_leak_detection_sensors_data) + 2 * sizeof(uint16_t), 0);
    auto* sensor =
        reinterpret_cast<nsm_leak_detection_sensors_data*>(buf.data());
    sensor->adc_reading = 1800;
    sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK] = 300;
    sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_LEAK] = 3500;
    sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_NORMAL] = 4500;

    ld->updateSensorValue(1, sensor, 3);

    auto& [svIntf, assocIntf, threshIntf] = ld->sensorValueIntfMap[1];
    EXPECT_DOUBLE_EQ(svIntf->value(), 1.8);
    EXPECT_DOUBLE_EQ(svIntf->minAllowableValue(), 0.3);
    EXPECT_DOUBLE_EQ(threshIntf->criticalLow(), 3.5);
    EXPECT_DOUBLE_EQ(svIntf->maxAllowableValue(), 4.5);
}

// ============================================================================
// genRequestMsg - encode fails (instanceId > NSM_INSTANCE_MAX)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, GenRequestMsg_EncodeFails)
{
    auto ld = makeSingleSensor("BrS11", 1);

    // instanceId > NSM_INSTANCE_MAX (31) causes pack_nsm_header to fail
    auto result = ld->genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// genRequestMsg - encode succeeds
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, GenRequestMsg_EncodeSucceeds)
{
    auto ld = makeSingleSensor("BrS12", 1);

    auto result = ld->genRequestMsg(10, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
}

// ============================================================================
// handleResponseMsg - decode error (rc != NSM_SW_SUCCESS)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, HandleResponseMsg_DecodeError)
{
    auto ld = makeSingleSensor("BrS13", 1);

    // Buffer meeting valgrind minimum: nsm_msg_hdr +
    // nsm_common_non_success_resp
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = ld->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// handleResponseMsg - cc != NSM_SUCCESS
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, HandleResponseMsg_BadCompletionCode)
{
    auto ld = makeSingleSensor("BrS14", 1);

    // Encode a response with error CC
    uint8_t dummy = 0;
    size_t msgLen = sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_leak_detection_info_resp);
    std::vector<uint8_t> msgBuf(msgLen, 0);
    auto* msgPtr = reinterpret_cast<nsm_msg*>(msgBuf.data());

    auto rc = encode_get_leak_detection_info_resp(0, NSM_ERROR, ERR_NULL, 0, 0,
                                                  &dummy, 0, msgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = ld->handleResponseMsg(msgPtr, msgLen);
    EXPECT_EQ(result, NSM_ERROR);
}

// ============================================================================
// handleResponseMsg - success path with sensor data triggering
// updateLeakDetectorState and updateSensorValue
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest, HandleResponseMsg_SuccessWithSensorData)
{
    auto ld = makeSingleSensor("BrS15", 1);

    uint8_t numberOfSensors = 1;
    uint8_t numberOfThresholdLevels = 3;
    size_t sensorInfoSize = sizeof(nsm_leak_detection_sensors_data) +
                            2 * sizeof(uint16_t);

    std::vector<uint8_t> sensorBuf(sensorInfoSize, 0);
    auto* sensor =
        reinterpret_cast<nsm_leak_detection_sensors_data*>(sensorBuf.data());
    sensor->sensor_id = 1;
    sensor->leak_state = NSM_LEAK_STATE_LEAK;
    sensor->adc_reading = 2500;
    sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK] = 300;
    sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_LEAK] = 3500;
    sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_NORMAL] = 4500;

    size_t msgLen = sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_leak_detection_info_resp) - 1 +
                    sensorBuf.size();
    std::vector<uint8_t> msgBuf(msgLen, 0);
    auto* msgPtr = reinterpret_cast<nsm_msg*>(msgBuf.data());

    auto rc = encode_get_leak_detection_info_resp(
        0, NSM_SUCCESS, ERR_NULL, numberOfSensors, numberOfThresholdLevels,
        sensorBuf.data(), sensorBuf.size(), msgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = ld->handleResponseMsg(msgPtr, msgLen);
    EXPECT_EQ(result, NSM_SUCCESS);

    // Verify state was updated to LEAK/Critical
    auto& [assocIntf, opIntf, stateIntf] = ld->leakDetectorStateIntfMap[1];
    EXPECT_TRUE(opIntf->functional());
    EXPECT_EQ(
        stateIntf->detectorState(),
        StateLeakDetectorIntf::convertDetectorStateEnumFromString(
            "xyz.openbmc_project.State.LeakDetector.DetectorStateEnum.Critical"));

    // Verify sensor values
    auto& [svIntf, sAssocIntf, threshIntf] = ld->sensorValueIntfMap[1];
    EXPECT_DOUBLE_EQ(svIntf->value(), 2.5);
}

// ============================================================================
// handleResponseMsg - sensor in response not in map (exercises
// updateLeakDetectorState/updateSensorValue early return within the loop)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       HandleResponseMsg_SensorNotInMap_EarlyReturns)
{
    auto ld = makeSingleSensor("BrS16", 1);

    // Build response with sensorId=99 (not in map)
    uint8_t numberOfSensors = 1;
    uint8_t numberOfThresholdLevels = 1;
    struct nsm_leak_detection_sensors_data sensorData = {};
    sensorData.sensor_id = 99;
    sensorData.leak_state = NSM_LEAK_STATE_NOMINAL_READING;
    sensorData.adc_reading = 1000;
    size_t sensorDataLen = sizeof(nsm_leak_detection_sensors_data);

    size_t msgLen = sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_leak_detection_info_resp) - 1 +
                    sensorDataLen;
    std::vector<uint8_t> msgBuf(msgLen, 0);
    auto* msgPtr = reinterpret_cast<nsm_msg*>(msgBuf.data());

    auto rc = encode_get_leak_detection_info_resp(
        0, NSM_SUCCESS, ERR_NULL, numberOfSensors, numberOfThresholdLevels,
        reinterpret_cast<uint8_t*>(&sensorData), sensorDataLen, msgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = ld->handleResponseMsg(msgPtr, msgLen);
    EXPECT_EQ(result, NSM_SUCCESS);

    // The existing sensor should still be at default (not touched)
    auto& [svIntf, sAssocIntf, threshIntf] = ld->sensorValueIntfMap[1];
    EXPECT_DOUBLE_EQ(svIntf->value(), 0.0);
}

// ============================================================================
// getLeakDetectionThresholdsData - dataLen < requiredLen
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       GetLeakDetectionThresholdsData_InsufficientBuffer)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch1";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    std::vector<uint8_t> buf(2, 0);
    auto* data =
        reinterpret_cast<nsm_leak_detection_thresholds_data*>(buf.data());

    EXPECT_FALSE(patchObj->getLeakDetectionThresholdsData(data, 2));
}

// ============================================================================
// getLeakDetectionThresholdsData - sufficient buffer succeeds
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       GetLeakDetectionThresholdsData_SufficientBuffer)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch2";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());
    svIntf->minAllowableValue(0.5);
    thIntf->criticalLow(1.0);
    svIntf->maxAllowableValue(3.5);

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(5, svIntf,
                                                                      thIntf);

    size_t dataLen = sizeof(nsm_leak_detection_thresholds_data) +
                     ((expectedNumberOfThresholdLevels - 1) * sizeof(uint16_t));
    std::vector<uint8_t> buf(dataLen, 0);
    auto* data =
        reinterpret_cast<nsm_leak_detection_thresholds_data*>(buf.data());

    EXPECT_TRUE(patchObj->getLeakDetectionThresholdsData(data, dataLen));
    EXPECT_EQ(data->sensor_id, 5);
    EXPECT_EQ(data->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK], 500);
    EXPECT_EQ(data->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_LEAK], 1000);
    EXPECT_EQ(data->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_NORMAL], 3500);
}

// ============================================================================
// setLeakDetectionThresholdsPatch - asyncPatchInProgress = true
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsPatch_InProgress_ReturnsUnavailable)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch3";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);
    patchObj->asyncPatchInProgress = true;

    AsyncOperationStatusType status = {};
    std::vector<PatchTuple> patches;
    patches.push_back({"MinAllowableValue", double(0.5)});
    AsyncSetOperationValueType value = patches;

    patchObj->setLeakDetectionThresholdsPatch(value, &status, fpga);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// ============================================================================
// setLeakDetectionThresholdsPatch - invalid patch value type (get_if nullptr)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsPatch_InvalidValueType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch4";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());
    svIntf->minAllowableValue(0.5);
    thIntf->criticalLow(1.0);
    svIntf->maxAllowableValue(3.5);

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    AsyncOperationStatusType status = {};
    // Pass a string (wrong variant type - not a vector of tuples)
    AsyncSetOperationValueType value = std::string("invalid");

    EXPECT_THROW_COROUTINE(
        patchObj->setLeakDetectionThresholdsPatch(value, &status, fpga),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setLeakDetectionThresholdsPatch - empty values vector
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsPatch_EmptyValues_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch5";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());
    svIntf->minAllowableValue(0.5);
    thIntf->criticalLow(1.0);
    svIntf->maxAllowableValue(3.5);

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    AsyncOperationStatusType status = {};
    std::vector<PatchTuple> emptyPatches;
    AsyncSetOperationValueType value = emptyPatches;

    EXPECT_THROW_COROUTINE(
        patchObj->setLeakDetectionThresholdsPatch(value, &status, fpga),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setLeakDetectionThresholdsPatch - non-double threshold value (bool)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsPatch_NonDoubleValue_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch6";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());
    svIntf->minAllowableValue(0.5);
    thIntf->criticalLow(1.0);
    svIntf->maxAllowableValue(3.5);

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    AsyncOperationStatusType status = {};
    std::vector<PatchTuple> patches;
    patches.push_back({"MinAllowableValue", true}); // bool instead of double
    AsyncSetOperationValueType value = patches;

    EXPECT_THROW_COROUTINE(
        patchObj->setLeakDetectionThresholdsPatch(value, &status, fpga),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setLeakDetectionThresholdsPatch - unknown key in patch
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsPatch_UnknownKey_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch7";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());
    svIntf->minAllowableValue(0.5);
    thIntf->criticalLow(1.0);
    svIntf->maxAllowableValue(3.5);

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    AsyncOperationStatusType status = {};
    std::vector<PatchTuple> patches;
    patches.push_back({"SomeUnknownKey", double(1.5)});
    AsyncSetOperationValueType value = patches;

    EXPECT_THROW_COROUTINE(
        patchObj->setLeakDetectionThresholdsPatch(value, &status, fpga),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setLeakDetectionThresholdsPatch - valid patch (MinAllowableValue) success
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsPatch_MinAllowableValue_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch8";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());
    svIntf->minAllowableValue(0.5);
    thIntf->criticalLow(1.0);
    svIntf->maxAllowableValue(3.5);

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    // Build success response
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto* respMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_set_leak_detection_thresholds_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    AsyncOperationStatusType status = {};
    std::vector<PatchTuple> patches;
    patches.push_back({"MinAllowableValue", double(0.75)});
    AsyncSetOperationValueType value = patches;

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    patchObj->setLeakDetectionThresholdsPatch(value, &status, fpga);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setLeakDetectionThresholdsPatch - valid patch (CriticalLow) success
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsPatch_CriticalLow_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch9";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());
    svIntf->minAllowableValue(0.5);
    thIntf->criticalLow(1.0);
    svIntf->maxAllowableValue(3.5);

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto* respMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_set_leak_detection_thresholds_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    AsyncOperationStatusType status = {};
    std::vector<PatchTuple> patches;
    patches.push_back({"CriticalLow", double(1.5)});
    AsyncSetOperationValueType value = patches;

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    patchObj->setLeakDetectionThresholdsPatch(value, &status, fpga);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setLeakDetectionThresholdsPatch - valid patch (MaxAllowableValue) success
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsPatch_MaxAllowableValue_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch10";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());
    svIntf->minAllowableValue(0.5);
    thIntf->criticalLow(1.0);
    svIntf->maxAllowableValue(3.5);

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto* respMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_set_leak_detection_thresholds_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    AsyncOperationStatusType status = {};
    std::vector<PatchTuple> patches;
    patches.push_back({"MaxAllowableValue", double(4.0)});
    AsyncSetOperationValueType value = patches;

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    patchObj->setLeakDetectionThresholdsPatch(value, &status, fpga);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setLeakDetectionThresholdsOnDevice - postPatchIO failure
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsOnDevice_PostPatchIOFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch11";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    size_t thresholdsDataLen =
        sizeof(nsm_leak_detection_thresholds_data) +
        ((expectedNumberOfThresholdLevels - 1) * sizeof(uint16_t));
    std::vector<uint8_t> buf(thresholdsDataLen, 0);
    auto* data =
        reinterpret_cast<nsm_leak_detection_thresholds_data*>(buf.data());
    data->sensor_id = 1;

    AsyncOperationStatusType status = {};

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO)
        .WillOnce(mockPostPatchIO(
            static_cast<nsm_completion_codes>(NSM_SW_ERROR_COMMAND_FAIL)));

    patchObj->setLeakDetectionThresholdsOnDevice(data, thresholdsDataLen,
                                                 &status, fpga);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setLeakDetectionThresholdsOnDevice - decode response failure (error CC)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsOnDevice_DecodeRespFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch12";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    size_t thresholdsDataLen =
        sizeof(nsm_leak_detection_thresholds_data) +
        ((expectedNumberOfThresholdLevels - 1) * sizeof(uint16_t));
    std::vector<uint8_t> buf(thresholdsDataLen, 0);
    auto* data =
        reinterpret_cast<nsm_leak_detection_thresholds_data*>(buf.data());
    data->sensor_id = 1;

    // Build error response
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto* respMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_set_leak_detection_thresholds_resp(0, NSM_ERROR, ERR_NULL,
                                                        respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    AsyncOperationStatusType status = {};

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));

    patchObj->setLeakDetectionThresholdsOnDevice(data, thresholdsDataLen,
                                                 &status, fpga);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setLeakDetectionThresholdsPatch - non-double value (uint32_t variant)
// ============================================================================

TEST_F(NsmLeakDetectionBranchTest,
       SetLeakDetectionThresholdsPatch_Uint32Value_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string sp = "/xyz/openbmc_project/sensors/voltage/BrPatch13";
    auto svIntf = std::make_shared<SensorValueIntf>(bus, sp.c_str());
    auto thIntf = std::make_shared<SensorThresholdCriticalIntf>(bus,
                                                                sp.c_str());
    svIntf->minAllowableValue(0.5);
    thIntf->criticalLow(1.0);
    svIntf->maxAllowableValue(3.5);

    auto patchObj = std::make_shared<NsmLeakDetectionThresholdsPatch>(1, svIntf,
                                                                      thIntf);

    AsyncOperationStatusType status = {};
    std::vector<PatchTuple> patches;
    patches.push_back(
        {"CriticalLow", uint32_t(500)}); // uint32 instead of double
    AsyncSetOperationValueType value = patches;

    EXPECT_THROW_COROUTINE(
        patchObj->setLeakDetectionThresholdsPatch(value, &status, fpga),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}
