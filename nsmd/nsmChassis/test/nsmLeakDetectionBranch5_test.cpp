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
 * Branch coverage tests for nsmLeakDetection.cpp - batch 5
 *
 * Targets remaining uncovered branches:
 * - updateLeakDetectorState: sensorId not in leakDetectorStateIntfMap (early
 *   return)
 * - updateSensorValue: sensorId not in sensorValueIntfMap (early return)
 * - updateLeakDetectorState: default case (invalid leakState)
 * - getSensorInterfaces: sensorId not in map -> nullopt
 * - getSensorInterfaces: sensorId in map -> returns tuple
 * - NsmLeakDetectionThresholdsPatch::getLeakDetectionThresholdsData:
 *   buffer too small -> false
 * - setLeakDetectionThresholdsPatch: asyncPatchInProgress -> Unavailable
 * - setLeakDetectionThresholdsPatch: patchRequestedValues is nullptr (wrong
 *   type) -> InvalidArgument
 * - setLeakDetectionThresholdsPatch: empty patchRequestedValues ->
 *   InvalidArgument
 * - setLeakDetectionThresholdsPatch: non-double threshold value ->
 *   InvalidArgument
 * - setLeakDetectionThresholdsPatch: unrecognized key -> InvalidArgument
 * - setLeakDetectionThresholdsOnDevice: postPatchIO failure
 * - setLeakDetectionThresholdsOnDevice: decode failure (cc!=0)
 * - handleResponseMsg: decode error (rc!=0 || cc!=0)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "libnsm/platform-environmental.h"

#include "nsmLeakDetection.hpp"
#include "test/commonMock.hpp"

#include <xyz/openbmc_project/Common/error.hpp>

namespace nsm
{
requester::Coroutine nsmLeakDetectionCreateSensors(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath);
} // namespace nsm

using namespace nsm;

using PatchTuple =
    std::tuple<std::string,
               std::variant<bool, uint32_t, double, std::vector<uint8_t>>>;

// ============================================================================
// Fixture
// ============================================================================

struct NsmLeakDetectionBranch5Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_B5";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:5";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmLeakDetectionBranch5Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmLeakDetectionBranch5Test()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmLeakDetection> makeSensor(const std::string& prefix,
                                                 std::vector<uint8_t> ids)
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string n = "LeakB5_" + prefix;
        std::vector<uint64_t> idMap;
        std::vector<std::string> nameMap;
        for (auto id : ids)
        {
            idMap.push_back(id);
            nameMap.push_back(prefix + "_s" + std::to_string(id));
        }
        return std::make_shared<NsmLeakDetection>(
            n, "NSM_LeakDetection", bus, idMap, nameMap, chassisPath, 5.0, 0.0);
    }
};

// ============================================================================
// updateLeakDetectorState: sensorId not in map -> early return
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       UpdateLeakDetectorState_InvalidSensorId_EarlyReturn)
{
    auto ld = makeSensor("invid", {1});
    // sensorId 99 not in map -> returns immediately
    EXPECT_NO_THROW(ld->updateLeakDetectorState(99, NSM_LEAK_STATE_LEAK));
}

// ============================================================================
// updateSensorValue: sensorId not in sensorValueIntfMap -> early return
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       UpdateSensorValue_InvalidSensorId_EarlyReturn)
{
    auto ld = makeSensor("invid2", {1});

    // Construct minimal sensor data
    nsm_leak_detection_sensors_data sensor = {};
    sensor.sensor_id = 99; // not in map
    sensor.adc_reading = 1000;

    EXPECT_NO_THROW(ld->updateSensorValue(99, &sensor, 3));
}

// ============================================================================
// updateLeakDetectorState: default case (unknown leak state)
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       UpdateLeakDetectorState_UnknownState_DefaultCase)
{
    auto ld = makeSensor("defstate", {1});

    // Use invalid leak state value (0xFF)
    ld->updateLeakDetectorState(1, 0xFF);

    auto& [assocIntf, opIntf, stateIntf] = ld->leakDetectorStateIntfMap[1];
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
// getSensorInterfaces: sensorId not in map -> nullopt
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       GetSensorInterfaces_InvalidSensorId_ReturnsNullopt)
{
    auto ld = makeSensor("gsi", {1});
    auto result = ld->getSensorInterfaces(99);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// getSensorInterfaces: sensorId in map -> returns valid tuple
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       GetSensorInterfaces_ValidSensorId_ReturnsTuple)
{
    auto ld = makeSensor("gsi2", {5});
    auto result = ld->getSensorInterfaces(5);
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// NsmLeakDetectionThresholdsPatch::getLeakDetectionThresholdsData:
// buffer too small -> returns false
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       GetThresholdsData_BufferTooSmall_ReturnsFalse)
{
    auto ld = makeSensor("bufsmall", {1});
    auto sensorIntfs = ld->getSensorInterfaces(1);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        1, sensorValueIntf, thresholdIntf);

    // Allocate a buffer that is too small
    nsm_leak_detection_thresholds_data data = {};
    bool result = patch->getLeakDetectionThresholdsData(&data, 1);
    EXPECT_FALSE(result);
}

// ============================================================================
// setLeakDetectionThresholdsPatch: asyncPatchInProgress -> Unavailable
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       SetThresholdsPatch_AsyncInProgress_Unavailable)
{
    auto ld = makeSensor("async", {1});
    auto sensorIntfs = ld->getSensorInterfaces(1);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        1, sensorValueIntf, thresholdIntf);

    // Set asyncPatchInProgress to true
    patch->asyncPatchInProgress = true;

    AsyncOperationStatusType status{};
    std::vector<PatchTuple> patchValues;
    patchValues.emplace_back(
        "MinAllowableValue",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>(1.0));
    AsyncSetOperationValueType value = patchValues;

    patch->setLeakDetectionThresholdsPatch(value, &status, fpga);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// ============================================================================
// setLeakDetectionThresholdsPatch: wrong value type (not vector of tuples)
// -> InvalidArgument
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       SetThresholdsPatch_WrongValueType_InvalidArgument)
{
    auto ld = makeSensor("wrongtype", {1});
    auto sensorIntfs = ld->getSensorInterfaces(1);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        1, sensorValueIntf, thresholdIntf);

    AsyncOperationStatusType status{};
    // Pass a string instead of vector of tuples
    AsyncSetOperationValueType value = std::string("wrong");

    EXPECT_THROW_COROUTINE(
        patch->setLeakDetectionThresholdsPatch(value, &status, fpga),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setLeakDetectionThresholdsPatch: empty patchRequestedValues
// -> InvalidArgument
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       SetThresholdsPatch_EmptyValues_InvalidArgument)
{
    auto ld = makeSensor("empty", {1});
    auto sensorIntfs = ld->getSensorInterfaces(1);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        1, sensorValueIntf, thresholdIntf);

    AsyncOperationStatusType status{};
    std::vector<PatchTuple> emptyPatchValues;
    AsyncSetOperationValueType value = emptyPatchValues;

    EXPECT_THROW_COROUTINE(
        patch->setLeakDetectionThresholdsPatch(value, &status, fpga),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setLeakDetectionThresholdsPatch: non-double threshold value
// (std::get_if<double> returns nullptr) -> InvalidArgument
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       SetThresholdsPatch_NonDoubleValue_InvalidArgument)
{
    auto ld = makeSensor("nondbl", {1});
    auto sensorIntfs = ld->getSensorInterfaces(1);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        1, sensorValueIntf, thresholdIntf);

    AsyncOperationStatusType status{};
    std::vector<PatchTuple> patchValues;
    // Use bool instead of double
    patchValues.emplace_back(
        "MinAllowableValue",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>(true));
    AsyncSetOperationValueType value = patchValues;

    EXPECT_THROW_COROUTINE(
        patch->setLeakDetectionThresholdsPatch(value, &status, fpga),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setLeakDetectionThresholdsPatch: unrecognized key -> InvalidArgument
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test,
       SetThresholdsPatch_UnrecognizedKey_InvalidArgument)
{
    auto ld = makeSensor("unrec", {1});
    auto sensorIntfs = ld->getSensorInterfaces(1);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        1, sensorValueIntf, thresholdIntf);

    AsyncOperationStatusType status{};
    std::vector<PatchTuple> patchValues;
    patchValues.emplace_back(
        "UnknownProperty",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>(1.5));
    AsyncSetOperationValueType value = patchValues;

    EXPECT_THROW_COROUTINE(
        patch->setLeakDetectionThresholdsPatch(value, &status, fpga),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setLeakDetectionThresholdsPatch: valid patch with all three keys
// + postPatchIO success + decode success
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test, SetThresholdsPatch_AllThreeKeys_Success)
{
    auto ld = makeSensor("allkeys", {1});
    auto sensorIntfs = ld->getSensorInterfaces(1);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        1, sensorValueIntf, thresholdIntf);

    AsyncOperationStatusType status{};
    std::vector<PatchTuple> patchValues;
    patchValues.emplace_back(
        "MinAllowableValue",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>(0.5));
    patchValues.emplace_back(
        "CriticalLow",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>(2.0));
    patchValues.emplace_back(
        "MaxAllowableValue",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>(3.5));
    AsyncSetOperationValueType value = patchValues;

    // Build success response
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto* respMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_set_leak_detection_thresholds_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(response));

    patch->setLeakDetectionThresholdsPatch(value, &status, fpga);
    // The source code does not explicitly set status to Success on the happy
    // path, so just verify it was not set to a failure status.
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setLeakDetectionThresholdsOnDevice: postPatchIO failure
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test, SetThresholdsOnDevice_PostPatchIO_Failure)
{
    auto ld = makeSensor("ppio", {1});
    auto sensorIntfs = ld->getSensorInterfaces(1);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        1, sensorValueIntf, thresholdIntf);

    AsyncOperationStatusType status{};
    std::vector<PatchTuple> patchValues;
    patchValues.emplace_back(
        "MinAllowableValue",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>(0.5));
    AsyncSetOperationValueType value = patchValues;

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    patch->setLeakDetectionThresholdsPatch(value, &status, fpga);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setLeakDetectionThresholdsOnDevice: decode failure (cc!=0)
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test, SetThresholdsOnDevice_DecodeFailure_CC)
{
    auto ld = makeSensor("decfail", {1});
    auto sensorIntfs = ld->getSensorInterfaces(1);
    ASSERT_TRUE(sensorIntfs.has_value());

    auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();

    auto patch = std::make_shared<NsmLeakDetectionThresholdsPatch>(
        1, sensorValueIntf, thresholdIntf);

    AsyncOperationStatusType status{};
    std::vector<PatchTuple> patchValues;
    patchValues.emplace_back(
        "CriticalLow",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>(2.0));
    AsyncSetOperationValueType value = patchValues;

    // Build error CC response
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto* respMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_set_leak_detection_thresholds_resp(
        0, NSM_ERR_UNSUPPORTED_COMMAND_CODE, ERR_NULL, respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(response));

    patch->setLeakDetectionThresholdsPatch(value, &status, fpga);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// handleResponseMsg: decode error (rc!=0 || cc!=0)
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test, HandleResponseMsg_DecodeError)
{
    auto ld = makeSensor("decerr", {1});

    // Build a minimal error response using encode_reason_code directly
    // (encode_get_leak_detection_info_resp requires non-null sensors_data)
    std::vector<uint8_t> msgBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto* msgPtr = reinterpret_cast<nsm_msg*>(msgBuf.data());

    nsm_header_info header = {};
    header.nsm_msg_type = NSM_RESPONSE;
    header.instance_id = 0;
    header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
    pack_nsm_header(&header, &msgPtr->hdr);
    encode_reason_code(NSM_ERR_UNSUPPORTED_COMMAND_CODE, ERR_NULL,
                       NSM_GET_LEAK_DETECTION_INFO, msgPtr);

    auto result = ld->handleResponseMsg(msgPtr, msgBuf.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// handleResponseMsg: success with valid data
// ============================================================================

TEST_F(NsmLeakDetectionBranch5Test, HandleResponseMsg_Success)
{
    auto ld = makeSensor("succ", {1});

    uint8_t numberOfSensors = 1;
    uint8_t numberOfThresholdLevels = 3;
    size_t sensorInfoSize = sizeof(nsm_leak_detection_sensors_data) +
                            2 * sizeof(uint16_t);

    std::vector<uint8_t> sensorBuf(numberOfSensors * sensorInfoSize, 0);
    auto* s1 =
        reinterpret_cast<nsm_leak_detection_sensors_data*>(sensorBuf.data());
    s1->sensor_id = 1;
    s1->leak_state = NSM_LEAK_STATE_NOMINAL_READING;
    s1->adc_reading = 1500;
    s1->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK] = 200;
    s1->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_LEAK] = 3000;
    s1->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_NORMAL] = 4000;

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
}
