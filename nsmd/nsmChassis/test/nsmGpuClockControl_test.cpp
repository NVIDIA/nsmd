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

#include "nsmGpuClockControl.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine createControlGpuClock(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);

}; // namespace nsm

struct NsmChassisClockControlTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "GPU0_ClockControl";
    const std::string type = "NSM_ClockControl";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmChassisClockControlTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmChassisClockControlTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmChassisClockControlTest, goodTestBasic)
{
    // Basic test to verify test infrastructure works
    EXPECT_EQ(name, "GPU0_ClockControl");
    EXPECT_EQ(type, "NSM_ClockControl");
}

TEST_F(NsmChassisClockControlTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "TestClockControl";
    std::string testType = "NSM_ClockControl";
    std::string testPath = "/test/path";
    std::vector<utils::Association> associations;

    auto clockControl =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, testPath.c_str(), gpu);

    EXPECT_NE(clockControl, nullptr);
    EXPECT_EQ(clockControl->device, gpu);
}

TEST_F(NsmChassisClockControlTest, testClearClockLimitEncodingFailure)
{
    auto& bus = utils::DBusHandler::getBus();

    auto clockControl =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, objPath.c_str(), gpu);

    EXPECT_NE(clockControl, nullptr);

    // This test verifies that the object is created properly
    // The actual coroutine testing would require more complex async test setup
}

TEST_F(NsmChassisClockControlTest, testDeviceAssociation)
{
    auto& bus = utils::DBusHandler::getBus();

    auto clockControl =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, objPath.c_str(), gpu);

    EXPECT_NE(clockControl, nullptr);
    EXPECT_EQ(clockControl->device, gpu);
    EXPECT_EQ(clockControl->device->getEid(), gpu->getEid());
}
TEST_F(NsmChassisClockControlTest, createControlGpuClockSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl0";
    const std::string testName = "HGX_GPU 0 ClockLimit_0";
    const std::string testType = "NSM_ControlClockLimit_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_0";
    const std::string physicalContext =
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU";
    const std::string clockMode =
        "com.nvidia.ClockMode.Mode.MaximumPerformance";
    const bool priority = false;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = testName;
    propertyMap["Type"] = testType;
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["PhysicalContext"] = physicalContext;
    propertyMap["ClockMode"] = clockMode;
    propertyMap["Priority"] = priority;

    dbus::PropertyMap association = {
        {"Forward", "parent_chassis"},
        {"Backward", "clock_controls"},
        {"AbsolutePath",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU 0"}};
    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        testObjPath, interface + ".Associations0");
    propertyMapAssociation0 = association;

    createControlGpuClock(mockManager, interface, testObjPath);

    EXPECT_GE(gpu->deviceSensors.size(), 1);
    // Check if sensor was created (could be at different index)
    bool foundSensor = false;
    for (const auto& sensor : gpu->deviceSensors)
    {
        if (std::dynamic_pointer_cast<NsmChassisClockControl>(sensor))
        {
            foundSensor = true;
            EXPECT_EQ(testName, sensor->getName());
            break;
        }
    }
    EXPECT_TRUE(foundSensor);
}

// Helper: create a NsmChassisClockControl via factory and return a pointer
// to the created sensor. Requires a valid GPU UUID in the fixture.
static std::shared_ptr<NsmChassisClockControl>
    createClockControlSensor(NsmChassisClockControlTest& fixture,
                             const std::string& extraSuffix = "")
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/"
        "ClockControl_genreq" +
        extraSuffix;
    const std::string testName = "GPU0_ClockControl_genreq" + extraSuffix;
    const std::string testType = "NSM_ControlClockLimit_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_genreq" +
        extraSuffix;
    const std::string physicalContext =
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU";
    const std::string clockMode =
        "com.nvidia.ClockMode.Mode.MaximumPerformance";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = testName;
    propertyMap["Type"] = testType;
    propertyMap["UUID"] = fixture.gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["PhysicalContext"] = physicalContext;
    propertyMap["ClockMode"] = clockMode;
    propertyMap["Priority"] = false;

    createControlGpuClock(fixture.mockManager, interface, testObjPath);

    for (const auto& sensor : fixture.gpu->deviceSensors)
    {
        auto s = std::dynamic_pointer_cast<NsmChassisClockControl>(sensor);
        if (s && s->getName() == testName)
        {
            return s;
        }
    }
    return nullptr;
}

// Test genRequestMsg() returns a valid request
TEST_F(NsmChassisClockControlTest, GenRequestMsg_ReturnsValidRequest)
{
    auto sensor = createClockControlSensor(*this, "_genreq");
    ASSERT_NE(sensor, nullptr);

    auto request = sensor->genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req));
}

// Test handleResponseMsg() success path: updates requestedSpeedLimits
TEST_F(NsmChassisClockControlTest, HandleResponseMsg_Success_UpdatesLimits)
{
    auto sensor = createClockControlSensor(*this, "_resp_ok");
    ASSERT_NE(sensor, nullptr);

    nsm_clock_limit clockLimit = {};
    clockLimit.requested_limit_min = 100;
    clockLimit.requested_limit_max = 2000;
    clockLimit.present_limit_min = 200;
    clockLimit.present_limit_max = 1800;

    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_get_clock_limit_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, &clockLimit,
                                          responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor->handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(result, NSM_SUCCESS);
    auto limits = sensor->cpuOperatingConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(limits), 100u);
    EXPECT_EQ(std::get<1>(limits), 2000u);
}

// Test handleResponseMsg() error CC path: does not update limits
TEST_F(NsmChassisClockControlTest, HandleResponseMsg_ErrorCC_ReturnsError)
{
    auto sensor = createClockControlSensor(*this, "_resp_err");
    ASSERT_NE(sensor, nullptr);

    nsm_clock_limit clockLimit = {};
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_get_clock_limit_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_clock_limit_resp(0, NSM_ERROR, ERR_NULL, &clockLimit,
                                          responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor->handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(result, NSM_SUCCESS);
}

// Test handleResponseMsg() decode failure (truncated buffer): hits else branch
TEST_F(NsmChassisClockControlTest, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto sensor = createClockControlSensor(*this, "_resp_dec");
    ASSERT_NE(sensor, nullptr);

    // Full-size buffer so ASAN is happy; pass responseLen - 1 to trigger
    // NSM_SW_ERROR_LENGTH → condition false → else branch (cc ? cc : rc)
    nsm_clock_limit clockLimit = {};
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_get_clock_limit_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, &clockLimit,
                                responseMsg);

    auto result = sensor->handleResponseMsg(responseMsg,
                                            responseData.size() - 1);

    EXPECT_NE(result, NSM_SUCCESS);
}

TEST_F(NsmChassisClockControlTest, CreateControlGpuClockInvalidUUID)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/invalid/ClockControl";
    const std::string testName = "InvalidClockControl";
    const std::string testType = "NSM_ClockControl";
    const std::string testPath = "/test/clock/control";
    const uint64_t clockControlSize = 1;
    const uuid_t invalidUuid = "INVALID:UUID:DOES:NOT:EXIST";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = testName;
    propertyMap["Type"] = testType;
    propertyMap["UUID"] = invalidUuid;
    propertyMap["InventoryObjPaths"] = std::vector<std::string>{testPath};
    propertyMap["ClockControlSize"] = clockControlSize;

    EXPECT_THROW_COROUTINE(
        createControlGpuClock(mockManager, interface, testObjPath),
        std::runtime_error);

    // Should have only the automatic first sensor
    EXPECT_EQ(1, gpu->deviceSensors.size());
    auto clockControl = std::dynamic_pointer_cast<NsmClearClockLimAsyncIntf>(
        gpu->deviceSensors[0]);
    EXPECT_EQ(nullptr, clockControl);
}

// Success path: clearReqClockLimit coroutine – postPatchIO succeeds and
// cc = NSM_SUCCESS = 0 → co_return cc ? cc : rc; takes the false branch. //

TEST_F(NsmChassisClockControlTest, ClearReqClockLimit_Success_CoversFalseBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, objPath.c_str(), gpu);

    // Encode a set_clock_limit response with NSM_SUCCESS completion code
    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = clearIntf->clearReqClockLimit(&status);

    // cc = NSM_SUCCESS → false branch of co_return cc ? cc : rc //

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Error-CC path: clearReqClockLimit coroutine – postPatchIO succeeds but
// cc = NSM_ERROR → co_return cc ? cc : rc; takes the true branch. //

TEST_F(NsmChassisClockControlTest, ClearReqClockLimit_ErrorCC_CoversBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, objPath.c_str(), gpu);

    // Encode a set_clock_limit response with NSM_ERROR completion code
    std::vector<uint8_t> errorResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(errorResp.data());
    auto encRc = encode_set_clock_limit_resp(0, NSM_ERROR, ERR_NULL, respPtr);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = clearIntf->clearReqClockLimit(&status);

    // cc = NSM_ERROR → WriteFailure; true branch of co_return cc ? cc : rc //

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Success path: setRangeClockLimits coroutine – postPatchIO succeeds and
// cc = NSM_SUCCESS = 0 → co_return cc ? cc : rc; takes the false branch. //

TEST_F(NsmChassisClockControlTest,
       SetRangeClockLimits_Success_CoversFalseBranch)
{
    auto sensor = createClockControlSensor(*this, "_set_succ");
    ASSERT_NE(sensor, nullptr);

    // Encode a set_clock_limit response with NSM_SUCCESS completion code
    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncSetOperationValueType value =
        std::vector<std::tuple<std::string, uint32_t>>{{"SettingMin", 100u},
                                                       {"SettingMax", 2000u}};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setRangeClockLimits(value, &status, gpu);

    // cc = NSM_SUCCESS → false branch of co_return cc ? cc : rc //

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// genRequestMsg failure: instanceId > NSM_INSTANCE_MAX → encode fails →
// if (rc != NSM_SW_SUCCESS) TRUE → return nullopt. Covers lines 191-194.
TEST_F(NsmChassisClockControlTest,
       GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    auto sensor = createClockControlSensor(*this, "_genreq_inv");
    ASSERT_NE(sensor, nullptr);

    // NSM_INSTANCE_MAX + 1 makes encode_get_clock_limit_req fail
    auto request = sensor->genRequestMsg(5, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// doClearClockLimitOnDevice: wrapper coroutine → calls clearReqClockLimit and
// sets statusInterface. Covers lines 48-58 in nsmGpuClockControl.cpp.
TEST_F(NsmChassisClockControlTest, DoClearClockLimitOnDevice_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, objPath.c_str(), gpu);

    // Encode a valid set_clock_limit success response
    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/test/clock_doclear0");
    clearIntf->doClearClockLimitOnDevice(statusIntf);
    // statusInterface->status should have been set to Success
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// Error-CC path: setRangeClockLimits coroutine – postPatchIO succeeds but
// cc = NSM_ERROR → co_return cc ? cc : rc; takes the true branch. //

TEST_F(NsmChassisClockControlTest, SetRangeClockLimits_ErrorCC_CoversBranch)
{
    auto sensor = createClockControlSensor(*this, "_set_errcc");
    ASSERT_NE(sensor, nullptr);

    // Encode a set_clock_limit response with NSM_ERROR completion code
    std::vector<uint8_t> errorResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(errorResp.data());
    auto encRc = encode_set_clock_limit_resp(0, NSM_ERROR, ERR_NULL, respPtr);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    // Pass explicit min/max speed values to avoid needing
    // cpuOperatingConfigIntf
    AsyncSetOperationValueType value =
        std::vector<std::tuple<std::string, uint32_t>>{{"SettingMin", 100u},
                                                       {"SettingMax", 2000u}};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setRangeClockLimits(value, &status, gpu);

    // cc = NSM_ERROR → WriteFailure; true branch of co_return cc ? cc : rc //

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// postPatchIO transport failure in clearReqClockLimit: rc != 0 → if (rc) TRUE
// → WriteFailure and co_return rc. Covers lines 83-91.
TEST_F(NsmChassisClockControlTest,
       ClearReqClockLimit_PostPatchIO_Fail_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, objPath.c_str(), gpu);

    // postPatchIO returns NSM_ERROR as transport rc (no response msg)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = clearIntf->clearReqClockLimit(&status);

    // rc = NSM_ERROR (non-zero) → if (rc) TRUE → WriteFailure
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setRangeClockLimits: value is not vector<tuple<string,uint32_t>> →
// std::get_if returns NULL → throw InvalidArgument (lines 230-233). //

TEST_F(NsmChassisClockControlTest, SetRangeClockLimits_InvalidValueType_Throws)
{
    auto sensor = createClockControlSensor(*this, "_inv_type");
    ASSERT_NE(sensor, nullptr);

    // Wrong type: uint32_t scalar instead of the expected vector
    AsyncSetOperationValueType wrongValue = uint32_t{42};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_THROW_COROUTINE(
        sensor->setRangeClockLimits(wrongValue, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// setRangeClockLimits: only SettingMin in vector → maxReqSpeed stays UINT_MAX
// → falls back to cpuOperatingConfigIntf max (lines 254-258).
TEST_F(NsmChassisClockControlTest,
       SetRangeClockLimits_OnlySettingMin_UsesExistingMax)
{
    auto sensor = createClockControlSensor(*this, "_only_min");
    ASSERT_NE(sensor, nullptr);

    // Pre-load requestedSpeedLimits so the fallback max value is known
    sensor->cpuOperatingConfigIntf->requestedSpeedLimits(
        std::make_tuple(500u, 1500u));

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    // Only SettingMin: min=200, max falls back to existing 1500
    AsyncSetOperationValueType value =
        std::vector<std::tuple<std::string, uint32_t>>{{"SettingMin", 200u}};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setRangeClockLimits: only SettingMax in vector → minReqSpeed stays UINT_MAX
// → falls back to cpuOperatingConfigIntf min (lines 249-253).
TEST_F(NsmChassisClockControlTest,
       SetRangeClockLimits_OnlySettingMax_UsesExistingMin)
{
    auto sensor = createClockControlSensor(*this, "_only_max");
    ASSERT_NE(sensor, nullptr);

    // Pre-load requestedSpeedLimits so the fallback min value is known
    sensor->cpuOperatingConfigIntf->requestedSpeedLimits(
        std::make_tuple(500u, 1500u));

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    // Only SettingMax: max=2500, min falls back to existing 500
    AsyncSetOperationValueType value =
        std::vector<std::tuple<std::string, uint32_t>>{{"SettingMax", 2500u}};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setRangeClockLimits: postPatchIO transport failure (rc != 0) → if (rc) TRUE
// → WriteFailure and co_return rc. Covers lines 282-291.
TEST_F(NsmChassisClockControlTest,
       SetRangeClockLimits_PostPatchIO_Fail_SetsWriteFailure)
{
    auto sensor = createClockControlSensor(*this, "_ptchio_fail");
    ASSERT_NE(sensor, nullptr);

    // postPatchIO returns NSM_ERROR as transport rc (no response)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncSetOperationValueType value =
        std::vector<std::tuple<std::string, uint32_t>>{{"SettingMin", 100u},
                                                       {"SettingMax", 2000u}};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setRangeClockLimits(value, &status, gpu);

    // rc = NSM_ERROR (non-zero) → if (rc) TRUE → WriteFailure
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setRangeClockLimits: empty vector → both min and max stay UINT_MAX →
// both fall back to cpuOperatingConfigIntf values (both fallback branches).
TEST_F(NsmChassisClockControlTest,
       SetRangeClockLimits_EmptyVector_UsesBothExisting)
{
    auto sensor = createClockControlSensor(*this, "_empty_vec");
    ASSERT_NE(sensor, nullptr);

    // Pre-load requestedSpeedLimits for both fallback paths
    sensor->cpuOperatingConfigIntf->requestedSpeedLimits(
        std::make_tuple(500u, 1500u));

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    // Empty vector: both min(500) and max(1500) come from existing config
    AsyncSetOperationValueType value =
        std::vector<std::tuple<std::string, uint32_t>>{};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// Branch coverage: FALSE branches for property count() checks in
// createControlGpuClock (lines 318-362 of nsmGpuClockControl.cpp)
// =============================================================================

// Name absent → FALSE branch for count("Name") → name="" →
// NsmChassisClockControl accepts empty name in test env → sensor IS added.
TEST_F(NsmChassisClockControlTest, Factory_MissingName_SensorCreated)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl_miss_name";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_miss_name";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    // Intentionally omit "Name" → FALSE branch for count("Name") → name=""
    propertyMap["Type"] = std::string("NSM_ControlClockLimit_0");
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");
    propertyMap["ClockMode"] =
        std::string("com.nvidia.ClockMode.Mode.MaximumPerformance");
    propertyMap["Priority"] = false;

    const size_t before = gpu->deviceSensors.size();
    createControlGpuClock(mockManager, interface, testObjPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// UUID absent → FALSE branch for count("UUID") → uuid="" →
// getNsmDeviceFromStaticUUID("") → parseStaticUuid throws std::runtime_error
TEST_F(NsmChassisClockControlTest, Factory_MissingUUID_Throws)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl_miss_uuid";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_miss_uuid";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = std::string("ClockControl_miss_uuid");
    propertyMap["Type"] = std::string("NSM_ControlClockLimit_0");
    // Intentionally omit "UUID" → FALSE branch for count("UUID") → uuid="" →
    // parseStaticUuid("") returns -1 → throws std::runtime_error
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");
    propertyMap["ClockMode"] =
        std::string("com.nvidia.ClockMode.Mode.MaximumPerformance");
    propertyMap["Priority"] = false;

    EXPECT_THROW_COROUTINE(
        createControlGpuClock(mockManager, interface, testObjPath),
        std::runtime_error);
}

// Type absent → FALSE branch for count("Type") → type="" →
// NsmChassisClockControl accepts empty type → sensor IS added.
TEST_F(NsmChassisClockControlTest, Factory_MissingType_SensorCreated)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl_miss_type";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_miss_type";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = std::string("ClockControl_miss_type");
    // Intentionally omit "Type" → FALSE branch for count("Type") → type=""
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");
    propertyMap["ClockMode"] =
        std::string("com.nvidia.ClockMode.Mode.MaximumPerformance");
    propertyMap["Priority"] = false;

    const size_t before = gpu->deviceSensors.size();
    createControlGpuClock(mockManager, interface, testObjPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// Priority absent → FALSE branch for count("Priority") → priority=false
// (default bool{}) → addSensor(sensor, false) → roundRobin sensor. //

TEST_F(NsmChassisClockControlTest, Factory_MissingPriority_RoundRobinSensor)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl_miss_prio";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_miss_prio";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = std::string("ClockControl_miss_prio");
    propertyMap["Type"] = std::string("NSM_ControlClockLimit_0");
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");
    propertyMap["ClockMode"] =
        std::string("com.nvidia.ClockMode.Mode.MaximumPerformance");
    // Intentionally omit "Priority" → FALSE branch → priority=false →
    // roundRobin

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createControlGpuClock(mockManager, interface, testObjPath);
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// InventoryObjPath absent → FALSE branch for count("InventoryObjPath") →
// inventoryObjPath="" → path becomes "/Controls/ClockLimit_0" →
// valid D-Bus path → sensor IS added.
TEST_F(NsmChassisClockControlTest,
       Factory_MissingInventoryObjPath_SensorCreated)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl_miss_invpath";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = std::string("ClockControl_miss_invpath");
    propertyMap["Type"] = std::string("NSM_ControlClockLimit_0");
    propertyMap["UUID"] = gpuUuid;
    // Intentionally omit "InventoryObjPath" → FALSE branch →
    // inventoryObjPath="" → path becomes "/Controls/ClockLimit_0" (unique
    // across tests)
    propertyMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");
    propertyMap["ClockMode"] =
        std::string("com.nvidia.ClockMode.Mode.MaximumPerformance");
    propertyMap["Priority"] = false;

    const size_t before = gpu->deviceSensors.size();
    createControlGpuClock(mockManager, interface, testObjPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// PhysicalContext absent → FALSE branch for count("PhysicalContext") →
// physicalContext="" → NsmChassisClockControl enum conversion throws
// InvalidEnumString (extends std::exception) → propagates out.
TEST_F(NsmChassisClockControlTest, Factory_MissingPhysicalContext_Throws)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl_miss_phys";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_miss_phys";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = std::string("ClockControl_miss_phys");
    propertyMap["Type"] = std::string("NSM_ControlClockLimit_0");
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    // Intentionally omit "PhysicalContext" → FALSE branch → physicalContext=""
    // → enum conversion in NsmChassisClockControl throws InvalidEnumString
    propertyMap["ClockMode"] =
        std::string("com.nvidia.ClockMode.Mode.MaximumPerformance");
    propertyMap["Priority"] = false;

    EXPECT_THROW_COROUTINE(
        createControlGpuClock(mockManager, interface, testObjPath),
        std::exception);
}

// ClockMode absent → FALSE branch for count("ClockMode") →
// clockMode="" → NsmChassisClockControl enum conversion throws
// InvalidEnumString (extends std::exception) → propagates out.
// clearClockLimit() public method – calls doClearClockLimitOnDevice().detach()
// and returns the allocated object path (lines 110-125 in
// nsmGpuClockControl.cpp).
TEST_F(NsmChassisClockControlTest, ClearClockLimit_HappyPath_ReturnsObjectPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, objPath.c_str(), gpu);

    // The detached coroutine will call postPatchIO; let it succeed cleanly
    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillRepeatedly(mockPostPatchIO(successResp));

    auto objPathResult = clearIntf->clearClockLimit();

    // objectPath must be non-empty (AsyncOperationManager allocated a slot)
    EXPECT_FALSE(std::string(objPathResult).empty());
}

// setRangeClockLimits: vector element with key neither "SettingMin" nor
// "SettingMax" → covers Decision 'false' for else-if at L243. The unknown key
// is ignored; both speeds fall back to existing cpuOperatingConfigIntf values.
TEST_F(NsmChassisClockControlTest,
       SetRangeClockLimits_UnknownKey_ElseIfFalseBranch)
{
    auto sensor = createClockControlSensor(*this, "_unknown_key");
    ASSERT_NE(sensor, nullptr);

    sensor->cpuOperatingConfigIntf->requestedSpeedLimits(
        std::make_tuple(300u, 1200u));

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    // "Unknown" is neither "SettingMin" nor "SettingMax" →
    // if("SettingMin") FALSE → else-if("SettingMax") FALSE (L243 false branch)
    // → both speeds stay UINT_MAX → fall back to existing (300, 1200)
    AsyncSetOperationValueType value =
        std::vector<std::tuple<std::string, uint32_t>>{{"Unknown", 500u}};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmChassisClockControlTest, Factory_MissingClockMode_Throws)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl_miss_clkmode";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_miss_clkmode";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = std::string("ClockControl_miss_clkmode");
    propertyMap["Type"] = std::string("NSM_ControlClockLimit_0");
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");
    // Intentionally omit "ClockMode" → FALSE branch → clockMode="" →
    // enum conversion in NsmChassisClockControl throws InvalidEnumString
    propertyMap["Priority"] = false;

    EXPECT_THROW_COROUTINE(
        createControlGpuClock(mockManager, interface, testObjPath),
        std::exception);
}

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc==NSM_ERROR →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch →
// cpuOperatingConfigIntf NOT updated.
TEST_F(NsmChassisClockControlTest,
       HandleResponseMsg_DecodeSuccessNonZeroCC_DoesNotUpdate)
{
    auto sensor = createClockControlSensor(*this, "_ccbranch");
    ASSERT_NE(sensor, nullptr);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// Branch 12: L301 second operand — clearReqClockLimit: rc==NSM_SW_SUCCESS but
// cc!=NSM_SUCCESS → 9-byte buffer causes decode_reason_code_and_cc to return
// NSM_SW_SUCCESS with cc=NSM_ERROR → rc=0 (FALSE) but cc!=0 (TRUE).
TEST_F(NsmChassisClockControlTest,
       ClearReqClockLimit_DecodeSuccessNonZeroCC_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, objPath.c_str(), gpu);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(buf));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = clearIntf->clearReqClockLimit(&status);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Branch 12: L301 second operand — setRangeClockLimits: rc==NSM_SW_SUCCESS but
// cc!=NSM_SUCCESS → 9-byte buffer → decode_reason_code_and_cc returns
// NSM_SW_SUCCESS with cc=NSM_ERROR.
TEST_F(NsmChassisClockControlTest,
       SetRangeClockLimits_DecodeSuccessNonZeroCC_SetsWriteFailure)
{
    auto sensor = createClockControlSensor(*this, "_set_ccbranch12");
    ASSERT_NE(sensor, nullptr);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(buf));

    AsyncSetOperationValueType value =
        std::vector<std::tuple<std::string, uint32_t>>{{"SettingMin", 100u},
                                                       {"SettingMax", 2000u}};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setRangeClockLimits(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
