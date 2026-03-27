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
 * Branch coverage tests for nsmLeakDetection.cpp - batch 7
 *
 * Targets:
 * - NsmLeakDetection::getSensorInterfaces: sensorId not in map → nullopt
 * - NsmLeakDetectionThresholdsPatch::getLeakDetectionThresholdsData:
 *   insufficient buffer → returns false
 * - NsmLeakDetectionThresholdsPatch::setLeakDetectionThresholdsPatch:
 *   asyncPatchInProgress=true → Unavailable + NSM_SW_ERROR
 *   invalid value type (not vector-of-tuples) → throws InvalidArgument
 *   empty vector of tuples → throws InvalidArgument
 *   vector with non-double value → throws InvalidArgument
 *   vector with unrecognized key → throws InvalidArgument
 *   vector with "MinAllowableValue" key → exercises that branch
 *   vector with "CriticalLow" key → exercises that branch
 *   vector with "MaxAllowableValue" key → exercises that branch
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

using namespace nsm;

// PatchValue type used in setLeakDetectionThresholdsPatch
using PatchTuple =
    std::tuple<std::string,
               std::variant<bool, uint32_t, double, std::vector<uint8_t>>>;
using PatchVector = std::vector<PatchTuple>;

// ============================================================================
// Fixture
// ============================================================================

struct NsmLeakDetectionBranch7Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_B7";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:7";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDev;

    NsmLeakDetectionBranch7Test() : SensorManagerTest(devices)
    {
        mockDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(mockDev, nullptr);
    }

    ~NsmLeakDetectionBranch7Test()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmLeakDetection> makeSensor(const std::string& prefix,
                                                 std::vector<uint8_t> ids)
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string n = "LeakB7_" + prefix;
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

    // Build a patch object for sensor 0 from a sensor with id 0
    std::shared_ptr<NsmLeakDetectionThresholdsPatch>
        makePatch(std::shared_ptr<NsmLeakDetection> ld, uint8_t sensorId)
    {
        auto sensorIntfs = ld->getSensorInterfaces(sensorId);
        if (!sensorIntfs.has_value())
        {
            return nullptr;
        }
        auto [sensorValueIntf, thresholdIntf] = sensorIntfs.value();
        return std::make_shared<NsmLeakDetectionThresholdsPatch>(
            sensorId, sensorValueIntf, thresholdIntf);
    }
};

// ============================================================================
// NsmLeakDetection::getSensorInterfaces — false branch (sensorId not in map)
// ============================================================================

TEST_F(NsmLeakDetectionBranch7Test, GetSensorInterfaces_NotFound_ReturnsNullopt)
{
    auto ld = makeSensor("si_notfound", {3});
    // Sensor 3 is registered; 99 is not
    auto result = ld->getSensorInterfaces(99);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NsmLeakDetectionBranch7Test, GetSensorInterfaces_Found_ReturnsValue)
{
    auto ld = makeSensor("si_found", {5});
    auto result = ld->getSensorInterfaces(5);
    ASSERT_TRUE(result.has_value());
    auto [svi, thi] = result.value();
    EXPECT_NE(svi, nullptr);
    EXPECT_NE(thi, nullptr);
}

// ============================================================================
// NsmLeakDetectionThresholdsPatch::getLeakDetectionThresholdsData
// — insufficient buffer → returns false
// ============================================================================

TEST_F(NsmLeakDetectionBranch7Test, GetThresholdsData_InsufficientBuffer_False)
{
    auto ld = makeSensor("buf_small", {0});
    auto patch = makePatch(ld, 0);
    ASSERT_NE(patch, nullptr);

    struct nsm_leak_detection_thresholds_data data = {};
    // dataLen = 0 → less than required → returns false
    EXPECT_FALSE(patch->getLeakDetectionThresholdsData(&data, 0));
}

TEST_F(NsmLeakDetectionBranch7Test, GetThresholdsData_SufficientBuffer_True)
{
    auto ld = makeSensor("buf_ok", {1});
    auto patch = makePatch(ld, 1);
    ASSERT_NE(patch, nullptr);

    // Allocate enough space for the flexible array struct
    const size_t thresholdsDataLen =
        sizeof(struct nsm_leak_detection_thresholds_data) +
        ((expectedNumberOfThresholdLevels - 1) * sizeof(uint16_t));
    std::vector<uint8_t> buf(thresholdsDataLen, 0);
    auto* data = reinterpret_cast<struct nsm_leak_detection_thresholds_data*>(
        buf.data());
    EXPECT_TRUE(patch->getLeakDetectionThresholdsData(data, thresholdsDataLen));
    EXPECT_EQ(data->sensor_id, 1);
}

// ============================================================================
// NsmLeakDetectionThresholdsPatch::setLeakDetectionThresholdsPatch
// — asyncPatchInProgress == true → Unavailable + NSM_SW_ERROR
// ============================================================================

TEST_F(NsmLeakDetectionBranch7Test,
       SetThresholdsPatch_AsyncInProgress_ReturnsUnavailable)
{
    auto ld = makeSensor("async_prog", {2});
    auto patch = makePatch(ld, 2);
    ASSERT_NE(patch, nullptr);

    patch->asyncPatchInProgress = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value(true); // any valid value
    // co_return NSM_SW_ERROR with status = Unavailable (no throw, no co_await)
    auto result =
        patch->setLeakDetectionThresholdsPatch(value, &status, mockDev).data();
    EXPECT_EQ(result, NSM_SW_ERROR);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// ============================================================================
// setLeakDetectionThresholdsPatch — !patchRequestedValues (wrong variant type)
// → throws InvalidArgument
// ============================================================================

TEST_F(NsmLeakDetectionBranch7Test,
       SetThresholdsPatch_WrongVariantType_ThrowsInvalidArg)
{
    auto ld = makeSensor("wrong_type", {3});
    auto patch = makePatch(ld, 3);
    ASSERT_NE(patch, nullptr);

    // Pass a bool — std::get_if<PatchVector> returns nullptr → throw
    AsyncSetOperationValueType value(bool(true));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_THROW_COROUTINE(
        patch->setLeakDetectionThresholdsPatch(value, &status, mockDev),
        std::exception);
}

// ============================================================================
// setLeakDetectionThresholdsPatch — patchRequestedValues->empty()
// → throws InvalidArgument
// ============================================================================

TEST_F(NsmLeakDetectionBranch7Test,
       SetThresholdsPatch_EmptyPatchVector_ThrowsInvalidArg)
{
    auto ld = makeSensor("empty_vec", {4});
    auto patch = makePatch(ld, 4);
    ASSERT_NE(patch, nullptr);

    AsyncSetOperationValueType value(PatchVector{}); // empty vector
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_THROW_COROUTINE(
        patch->setLeakDetectionThresholdsPatch(value, &status, mockDev),
        std::exception);
}

// ============================================================================
// setLeakDetectionThresholdsPatch — non-double value in tuple
// → throws InvalidArgument
// ============================================================================

TEST_F(NsmLeakDetectionBranch7Test,
       SetThresholdsPatch_NonDoubleValue_ThrowsInvalidArg)
{
    auto ld = makeSensor("non_double", {5});
    auto patch = makePatch(ld, 5);
    ASSERT_NE(patch, nullptr);

    // Vector with a uint32_t value (not double) → get_if<double> → nullptr
    PatchVector vec = {{"MinAllowableValue", uint32_t(100)}};
    AsyncSetOperationValueType value(vec);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_THROW_COROUTINE(
        patch->setLeakDetectionThresholdsPatch(value, &status, mockDev),
        std::exception);
}

// ============================================================================
// setLeakDetectionThresholdsPatch — unrecognized key → throws InvalidArgument
// ============================================================================

TEST_F(NsmLeakDetectionBranch7Test,
       SetThresholdsPatch_UnrecognizedKey_ThrowsInvalidArg)
{
    auto ld = makeSensor("bad_key", {6});
    auto patch = makePatch(ld, 6);
    ASSERT_NE(patch, nullptr);

    PatchVector vec = {{"UnknownThresholdKey", double(1.5)}};
    AsyncSetOperationValueType value(vec);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_THROW_COROUTINE(
        patch->setLeakDetectionThresholdsPatch(value, &status, mockDev),
        std::exception);
}

// ============================================================================
// setLeakDetectionThresholdsPatch — "MinAllowableValue" key with mock device
// Exercises the MinAllowableValue branch in the key dispatch loop.
// The coroutine proceeds to setLeakDetectionThresholdsOnDevice which uses
// mockDev to send the request.
// ============================================================================

TEST_F(NsmLeakDetectionBranch7Test,
       SetThresholdsPatch_MinAllowableValue_CallsDevice)
{
    auto ld = makeSensor("min_val", {7});
    auto patch = makePatch(ld, 7);
    ASSERT_NE(patch, nullptr);

    // Mock device returns success for the postPatchIO call
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    // Encode a successful response for set_leak_detection_thresholds
    uint8_t cc = NSM_SUCCESS;
    (void)cc;
    // Use a zeroed response that decode will treat as success
    memset(msg, 0, resp.size());

    EXPECT_CALL(*mockDev, postPatchIO).WillOnce(mockPostPatchIO(resp));

    PatchVector vec = {{"MinAllowableValue", double(1.0)}};
    AsyncSetOperationValueType value(vec);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_NO_THROW(
        patch->setLeakDetectionThresholdsPatch(value, &status, mockDev).data());
}

TEST_F(NsmLeakDetectionBranch7Test, SetThresholdsPatch_CriticalLow_CallsDevice)
{
    auto ld = makeSensor("crit_low", {8});
    auto patch = makePatch(ld, 8);
    ASSERT_NE(patch, nullptr);

    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    memset(resp.data(), 0, resp.size());
    EXPECT_CALL(*mockDev, postPatchIO).WillOnce(mockPostPatchIO(resp));

    PatchVector vec = {{"CriticalLow", double(0.5)}};
    AsyncSetOperationValueType value(vec);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_NO_THROW(
        patch->setLeakDetectionThresholdsPatch(value, &status, mockDev).data());
}

TEST_F(NsmLeakDetectionBranch7Test,
       SetThresholdsPatch_MaxAllowableValue_CallsDevice)
{
    auto ld = makeSensor("max_val", {9});
    auto patch = makePatch(ld, 9);
    ASSERT_NE(patch, nullptr);

    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    memset(resp.data(), 0, resp.size());
    EXPECT_CALL(*mockDev, postPatchIO).WillOnce(mockPostPatchIO(resp));

    PatchVector vec = {{"MaxAllowableValue", double(4.5)}};
    AsyncSetOperationValueType value(vec);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_NO_THROW(
        patch->setLeakDetectionThresholdsPatch(value, &status, mockDev).data());
}
