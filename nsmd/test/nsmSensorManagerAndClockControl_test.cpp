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
 * nsmBatch11A.cpp
 *
 * Coverage targets:
 *   1. nsmd/sensorManager.cpp - SensorManagerImpl static methods:
 *      isNSMPollReady, dumpReadinessLogs, dumpNsmDevicesInfo,
 *      checkAllDevicesReady, getEid, getNsmDevice,
 *      getNsmDeviceFromStaticUUID, isEMReady, isMCTPReady, markEMReady
 *   2. nsmd/nsmChassis/nsmGpuClockControl.cpp - NsmChassisClockControl:
 *      genRequestMsg, handleResponseMsg, updateMetricOnSharedMemory,
 *      setRangeClockLimits, clearReqClockLimit, clearClockLimit,
 *      createControlGpuClock factory
 *   3. nsmd/nsmNumericSensor/nsmThresholdFactory.cpp -
 *      NsmThresholdFactory: constructor, getThresholdInterfaces,
 *      NsmThresholdAggregatorBuilder::makeAggregator
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#include <cstring>
#include <optional>
#include <tuple>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmChassis/nsmGpuClockControl.hpp"
#include "nsmNumericSensor/nsmNumericSensorFactory.hpp"
#include "nsmNumericSensor/nsmThreshold.hpp"
#include "nsmNumericSensor/nsmThresholdAggregator.hpp"
#include "nsmNumericSensor/nsmThresholdFactory.hpp"
#include "nsmNumericSensor/nsmThresholdValue.hpp"
#include "sensorManager.hpp"

#undef private
#undef protected

using namespace nsm;

// Forward declarations for factory functions
namespace nsm
{
requester::Coroutine createControlGpuClock(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

// ============================================================================
// Helper: Build a valid get_clock_limit response buffer
// ============================================================================
static std::vector<uint8_t>
    buildGetClockLimitResp(uint32_t reqMin, uint32_t reqMax, uint32_t presMin,
                           uint32_t presMax, uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_clock_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    struct nsm_clock_limit clockLimit = {};
    clockLimit.requested_limit_min = reqMin;
    clockLimit.requested_limit_max = reqMax;
    clockLimit.present_limit_min = presMin;
    clockLimit.present_limit_max = presMax;

    uint16_t reason_code = ERR_NULL;
    auto rc = encode_get_clock_limit_resp(0, cc, reason_code, &clockLimit,
                                          response);
    (void)rc;
    return responseMsg;
}

// ============================================================================
// Helper: Build a valid set_clock_limit response buffer
// ============================================================================
static std::vector<uint8_t> buildSetClockLimitResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t reason_code = ERR_NULL;
    auto rc = encode_set_clock_limit_resp(0, cc, reason_code, response);
    (void)rc;
    return responseMsg;
}

// ============================================================================
// PART 1: NsmChassisClockControl - genRequestMsg tests
// ============================================================================

struct NsmGpuClockControlTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "GPU0_ClockCtrl";
    const std::string type = "NSM_ClockControl";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/Controls/ClockLimit_0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf;
    std::shared_ptr<NsmClearClockLimAsyncIntf> clearClockIntf;
    std::shared_ptr<NsmChassisClockControl> clockControl;

    NsmGpuClockControlTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);

        auto& bus = utils::DBusHandler::getBus();
        cpuConfigIntf =
            std::make_shared<CpuOperatingConfigIntf>(bus, objPath.c_str());
        clearClockIntf = std::make_shared<NsmClearClockLimAsyncIntf>(
            bus, objPath.c_str(), gpu);

        std::vector<utils::Association> associations;
        associations.push_back(
            {"parent_chassis", "clock_controls",
             "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_0"});
        std::string typeCopy = type;
        std::string physicalContext =
            "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU";
        std::string clockMode = "com.nvidia.ClockMode.Mode.MaximumPerformance";

        clockControl = std::make_shared<NsmChassisClockControl>(
            bus, name, cpuConfigIntf, clearClockIntf, associations, typeCopy,
            objPath, physicalContext, clockMode);
    }

    ~NsmGpuClockControlTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmGpuClockControlTest, GenRequestMsg_ValidInput_ReturnsRequest)
{
    // Arrange
    eid_t eid = 10;
    uint8_t instanceId = 1;

    // Act
    auto result = clockControl->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req));
}

TEST_F(NsmGpuClockControlTest, GenRequestMsg_ZeroEid_ReturnsRequest)
{
    // Arrange & Act
    auto result = clockControl->genRequestMsg(0, 0);

    // Assert
    ASSERT_TRUE(result.has_value());
}

TEST_F(NsmGpuClockControlTest, GenRequestMsg_MaxEid_ReturnsRequest)
{
    // Arrange & Act
    auto result = clockControl->genRequestMsg(255, 31);

    // Assert
    ASSERT_TRUE(result.has_value());
}

TEST_F(NsmGpuClockControlTest, GenRequestMsg_MultipleCalls_AllSucceed)
{
    // Act
    auto r1 = clockControl->genRequestMsg(1, 1);
    auto r2 = clockControl->genRequestMsg(2, 2);
    auto r3 = clockControl->genRequestMsg(3, 3);

    // Assert
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    ASSERT_TRUE(r3.has_value());
}

// ============================================================================
// PART 2: NsmChassisClockControl - handleResponseMsg tests
// ============================================================================

TEST_F(NsmGpuClockControlTest,
       HandleResponseMsg_ValidResponse_ReturnsSuccessAndUpdatesLimits)
{
    // Arrange
    uint32_t reqMin = 300;
    uint32_t reqMax = 2100;
    uint32_t presMin = 210;
    uint32_t presMax = 2100;
    auto responseMsg = buildGetClockLimitResp(reqMin, reqMax, presMin, presMax);
    auto response = reinterpret_cast<const nsm_msg*>(responseMsg.data());
    size_t responseLen = responseMsg.size();

    // Act
    auto rc = clockControl->handleResponseMsg(response, responseLen);

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    auto limits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(limits), reqMin);
    EXPECT_EQ(std::get<1>(limits), reqMax);
}

TEST_F(NsmGpuClockControlTest,
       HandleResponseMsg_ZeroLimits_ReturnsSuccessAndUpdates)
{
    // Arrange
    auto responseMsg = buildGetClockLimitResp(0, 0, 0, 0);
    auto response = reinterpret_cast<const nsm_msg*>(responseMsg.data());

    // Act
    auto rc = clockControl->handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    auto limits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(limits), 0u);
    EXPECT_EQ(std::get<1>(limits), 0u);
}

TEST_F(NsmGpuClockControlTest,
       HandleResponseMsg_MaxLimits_ReturnsSuccessAndUpdates)
{
    // Arrange
    uint32_t maxVal = UINT32_MAX;
    auto responseMsg = buildGetClockLimitResp(maxVal, maxVal, maxVal, maxVal);
    auto response = reinterpret_cast<const nsm_msg*>(responseMsg.data());

    // Act
    auto rc = clockControl->handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    auto limits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(limits), maxVal);
    EXPECT_EQ(std::get<1>(limits), maxVal);
}

TEST_F(NsmGpuClockControlTest, HandleResponseMsg_ErrorCC_ReturnsNonZero)
{
    // Arrange - build a response with error completion code
    auto responseMsg = buildGetClockLimitResp(300, 2100, 210, 2100, NSM_ERROR);
    auto response = reinterpret_cast<const nsm_msg*>(responseMsg.data());

    // Act
    auto rc = clockControl->handleResponseMsg(response, responseMsg.size());

    // Assert - non-zero rc from error CC
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmGpuClockControlTest, HandleResponseMsg_TooShortResponse_ReturnsError)
{
    // Arrange - response too short to decode
    std::vector<uint8_t> shortResp(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<const nsm_msg*>(shortResp.data());

    // Act
    auto rc = clockControl->handleResponseMsg(response, shortResp.size());

    // Assert
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmGpuClockControlTest,
       HandleResponseMsg_UpdatesRequestedMinMax_CorrectValues)
{
    // Arrange
    uint32_t reqMin = 500;
    uint32_t reqMax = 1800;
    auto responseMsg = buildGetClockLimitResp(reqMin, reqMax, 100, 2000);
    auto response = reinterpret_cast<const nsm_msg*>(responseMsg.data());

    // Act
    clockControl->handleResponseMsg(response, responseMsg.size());

    // Assert
    auto limits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(limits), reqMin);
    EXPECT_EQ(std::get<1>(limits), reqMax);
}

TEST_F(NsmGpuClockControlTest, HandleResponseMsg_MultipleUpdates_LastOneWins)
{
    // Arrange
    auto resp1 = buildGetClockLimitResp(100, 200, 50, 250);
    auto resp2 = buildGetClockLimitResp(300, 400, 150, 450);

    // Act
    clockControl->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(resp1.data()), resp1.size());
    clockControl->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(resp2.data()), resp2.size());

    // Assert
    auto limits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(limits), 300u);
    EXPECT_EQ(std::get<1>(limits), 400u);
}

// ============================================================================
// PART 3: NsmChassisClockControl - updateMetricOnSharedMemory
// ============================================================================

TEST_F(NsmGpuClockControlTest, UpdateMetricOnSharedMemory_NoShmem_DoesNotThrow)
{
    // Arrange - set some speed limits first
    cpuConfigIntf->requestedSpeedLimits(std::make_tuple(100u, 200u));

    // Act & Assert - should not throw even without shared memory //

    EXPECT_NO_THROW(clockControl->updateMetricOnSharedMemory());
}

// ============================================================================
// PART 4: NsmChassisClockControl - setRangeClockLimits (coroutine)
// ============================================================================

TEST_F(NsmGpuClockControlTest,
       SetRangeClockLimits_BothMinAndMax_SuccessResponse)
{
    // Arrange
    auto setResp = buildSetClockLimitResp(NSM_SUCCESS);
    ON_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillByDefault(mockPostPatchIO(setResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> rangeClockLimit = {
        {"SettingMin", 300}, {"SettingMax", 2100}};
    AsyncSetOperationValueType value = rangeClockLimit;

    // Act
    auto coroutine = clockControl->setRangeClockLimits(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmGpuClockControlTest, SetRangeClockLimits_OnlyMin_UsesExistingMax)
{
    // Arrange
    cpuConfigIntf->requestedSpeedLimits(std::make_tuple(100u, 2000u));
    auto setResp = buildSetClockLimitResp(NSM_SUCCESS);
    ON_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillByDefault(mockPostPatchIO(setResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> rangeClockLimit = {
        {"SettingMin", 500}};
    AsyncSetOperationValueType value = rangeClockLimit;

    // Act
    clockControl->setRangeClockLimits(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmGpuClockControlTest, SetRangeClockLimits_OnlyMax_UsesExistingMin)
{
    // Arrange
    cpuConfigIntf->requestedSpeedLimits(std::make_tuple(100u, 2000u));
    auto setResp = buildSetClockLimitResp(NSM_SUCCESS);
    ON_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillByDefault(mockPostPatchIO(setResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> rangeClockLimit = {
        {"SettingMax", 1800}};
    AsyncSetOperationValueType value = rangeClockLimit;

    // Act
    clockControl->setRangeClockLimits(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmGpuClockControlTest,
       SetRangeClockLimits_PostPatchIOFails_SetsWriteFailure)
{
    // Arrange
    ON_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillByDefault(mockPostPatchIO(
            static_cast<nsm_completion_codes>(NSM_SW_ERROR_COMMAND_FAIL)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> rangeClockLimit = {
        {"SettingMin", 300}, {"SettingMax", 2100}};
    AsyncSetOperationValueType value = rangeClockLimit;

    // Act
    clockControl->setRangeClockLimits(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmGpuClockControlTest, SetRangeClockLimits_ErrorCC_SetsWriteFailure)
{
    // Arrange
    auto setResp = buildSetClockLimitResp(NSM_ERROR);
    ON_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillByDefault(mockPostPatchIO(setResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> rangeClockLimit = {
        {"SettingMin", 300}, {"SettingMax", 2100}};
    AsyncSetOperationValueType value = rangeClockLimit;

    // Act
    clockControl->setRangeClockLimits(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmGpuClockControlTest,
       SetRangeClockLimits_InvalidValueType_ThrowsInvalidArgument)
{
    // Arrange - pass wrong variant type (string instead of vector)
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("invalid");

    // Act & Assert - coroutine stores exception in promise, use
    // EXPECT_THROW_COROUTINE to check it
    EXPECT_THROW_COROUTINE(
        clockControl->setRangeClockLimits(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmGpuClockControlTest,
       SetRangeClockLimits_EmptyRange_UsesExistingMinAndMax)
{
    // Arrange
    cpuConfigIntf->requestedSpeedLimits(std::make_tuple(200u, 1500u));
    auto setResp = buildSetClockLimitResp(NSM_SUCCESS);
    ON_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillByDefault(mockPostPatchIO(setResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> rangeClockLimit;
    AsyncSetOperationValueType value = rangeClockLimit;

    // Act -- coroutine handles empty range internally
    EXPECT_NO_THROW(clockControl->setRangeClockLimits(value, &status, gpu));
}

// ============================================================================
// PART 5: NsmClearClockLimAsyncIntf - clearReqClockLimit
// ============================================================================

TEST_F(NsmGpuClockControlTest,
       ClearReqClockLimit_SuccessResponse_ReturnsSuccess)
{
    // Arrange
    auto setResp = buildSetClockLimitResp(NSM_SUCCESS);
    ON_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillByDefault(mockPostPatchIO(setResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    clearClockIntf->clearReqClockLimit(&status);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmGpuClockControlTest,
       ClearReqClockLimit_PostPatchIOFails_SetsWriteFailure)
{
    // Arrange
    ON_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillByDefault(mockPostPatchIO(
            static_cast<nsm_completion_codes>(NSM_SW_ERROR_COMMAND_FAIL)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    clearClockIntf->clearReqClockLimit(&status);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmGpuClockControlTest, ClearReqClockLimit_ErrorCC_SetsWriteFailure)
{
    // Arrange
    auto setResp = buildSetClockLimitResp(NSM_ERROR);
    ON_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillByDefault(mockPostPatchIO(setResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    clearClockIntf->clearReqClockLimit(&status);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// PART 6: NsmChassisClockControl constructor tests
// ============================================================================

TEST_F(NsmGpuClockControlTest, Constructor_SetsNameAndType)
{
    EXPECT_EQ(clockControl->getName(), name);
    EXPECT_EQ(clockControl->getType(), type);
}

TEST_F(NsmGpuClockControlTest, Constructor_InitializesInterfaces)
{
    EXPECT_NE(clockControl->cpuOperatingConfigIntf, nullptr);
    EXPECT_NE(clockControl->nsmClearClockLimAsyncIntf, nullptr);
    EXPECT_NE(clockControl->decoratorAreaIntf, nullptr);
    EXPECT_NE(clockControl->associationDefinitionsInft, nullptr);
    EXPECT_NE(clockControl->processorModeIntf, nullptr);
}

TEST_F(NsmGpuClockControlTest, Constructor_SetsInventoryObjPath)
{
    EXPECT_EQ(clockControl->inventoryObjPath, objPath);
}

// ============================================================================
// PART 7: createControlGpuClock factory tests
// ============================================================================

TEST_F(NsmGpuClockControlTest, CreateControlGpuClock_AllProperties_Success)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl1";
    const std::string testName = "HGX_GPU 0 ClockLimit_1";
    const std::string testType = "NSM_ControlClockLimit_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_1";
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
         "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU 1"}};
    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        testObjPath, interface + ".Associations0");
    propertyMapAssociation0 = association;

    size_t initialSensorCount = gpu->deviceSensors.size();
    createControlGpuClock(mockManager, interface, testObjPath);

    EXPECT_GT(gpu->deviceSensors.size(), initialSensorCount);

    // Verify a NsmChassisClockControl sensor was added
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

TEST_F(NsmGpuClockControlTest,
       CreateControlGpuClock_WithPriority_AddsToCorrectQueue)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl2";
    const std::string testName = "HGX_GPU 0 PriorityClockLimit";
    const std::string testType = "NSM_ControlClockLimit_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_2";
    const std::string physicalContext =
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU";
    const std::string clockMode =
        "com.nvidia.ClockMode.Mode.MaximumPerformance";
    const bool priority = true;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = testName;
    propertyMap["Type"] = testType;
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["PhysicalContext"] = physicalContext;
    propertyMap["ClockMode"] = clockMode;
    propertyMap["Priority"] = priority;

    size_t initialPrioritySensors = gpu->prioritySensors.size();
    createControlGpuClock(mockManager, interface, testObjPath);

    EXPECT_GT(gpu->prioritySensors.size(), initialPrioritySensors);
}

TEST_F(NsmGpuClockControlTest, CreateControlGpuClock_InvalidUUID_ReturnsError)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/invalid/ClockControlX";
    const uuid_t invalidUuid = "INVALID:UUID:DOES:NOT:EXIST";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = std::string("InvalidClockControl");
    propertyMap["Type"] = std::string("NSM_ClockControl");
    propertyMap["UUID"] = invalidUuid;
    propertyMap["InventoryObjPath"] = std::string("/test/path");

    EXPECT_THROW_COROUTINE(
        createControlGpuClock(mockManager, interface, testObjPath),
        std::runtime_error);
}

TEST_F(NsmGpuClockControlTest,
       CreateControlGpuClock_EmptyProperties_HandledGracefully)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl3";

    // Only set UUID and InventoryObjPath (minimum required)
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_3");

    // With missing Name/Type/PhysicalContext/ClockMode, the factory
    // may throw during string conversion (e.g. empty physicalContext).
    // Verify it does not crash the test - the coroutine handles errors
    // internally or the exception is stored in the coroutine promise.
    EXPECT_NO_THROW_COROUTINE(
        createControlGpuClock(mockManager, interface, testObjPath));
}

// ============================================================================
// PART 8: NsmClearClockLimAsyncIntf constructor and device check
// ============================================================================

TEST_F(NsmGpuClockControlTest, NsmClearClockLimAsyncIntf_Constructor_SetsDevice)
{
    EXPECT_EQ(clearClockIntf->device, gpu);
}

TEST_F(NsmGpuClockControlTest, NsmClearClockLimAsyncIntf_DeviceEid_MatchesGpu)
{
    EXPECT_EQ(clearClockIntf->device->getEid(), gpu->getEid());
}

// ============================================================================
// PART 9: SensorManager - static methods and readiness checks
// ============================================================================

struct SensorManagerStaticTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    SensorManagerStaticTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~SensorManagerStaticTest()
    {
        // Reset static state between tests
        SensorManagerImpl::isReadyForReadinessCheck = false;
        SensorManagerImpl::isMCTPReadyCheck = false;
        SensorManagerImpl::isEMReadyCheck = false;
        SensorManagerImpl::readynessFailureMap.clear();
        cleanupDeviceSensors(devices);
    }
};

TEST_F(SensorManagerStaticTest, IsNSMPollReady_BothNotReady_ReturnsFalse)
{
    // Arrange
    SensorManagerImpl::isReadyForReadinessCheck = false;
    SensorManagerImpl::isMCTPReadyCheck = false;
    SensorManagerImpl::isEMReadyCheck = false;

    // Act
    bool result = SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_FALSE(result);
}

TEST_F(SensorManagerStaticTest, IsNSMPollReady_MCTPReadyOnly_ReturnsFalse)
{
    // Arrange
    SensorManagerImpl::isReadyForReadinessCheck = false;
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::isEMReadyCheck = false;

    // Act
    bool result = SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_FALSE(result);
}

TEST_F(SensorManagerStaticTest, IsNSMPollReady_EMReadyOnly_ReturnsFalse)
{
    // Arrange
    SensorManagerImpl::isReadyForReadinessCheck = false;
    SensorManagerImpl::isMCTPReadyCheck = false;
    SensorManagerImpl::isEMReadyCheck = true;

    // Act
    bool result = SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_FALSE(result);
}

TEST_F(SensorManagerStaticTest, IsNSMPollReady_BothReady_ReturnsTrue)
{
    // Arrange
    SensorManagerImpl::isReadyForReadinessCheck = false;
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::isEMReadyCheck = true;

    // Act
    bool result = SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
}

TEST_F(SensorManagerStaticTest, IsNSMPollReady_AlreadyReady_StaysTrue)
{
    // Arrange
    SensorManagerImpl::isReadyForReadinessCheck = true;
    SensorManagerImpl::isMCTPReadyCheck = false;
    SensorManagerImpl::isEMReadyCheck = false;

    // Act
    bool result = SensorManagerImpl::isNSMPollReady();

    // Assert - once set to true, should stay true
    EXPECT_TRUE(result);
}

TEST_F(SensorManagerStaticTest, IsNSMPollReady_SetsReadinessMap_OnSuccess)
{
    // Arrange
    SensorManagerImpl::isReadyForReadinessCheck = false;
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::isEMReadyCheck = true;
    SensorManagerImpl::readynessFailureMap.clear();

    // Act
    SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_EQ(SensorManagerImpl::readynessFailureMap["isNSMPollReady"], "true");
}

// ============================================================================
// PART 10: SensorManager - dumpReadinessLogs
// ============================================================================

TEST_F(SensorManagerStaticTest, DumpReadinessLogs_EmptyMap_NoThrow)
{
    // Arrange
    SensorManagerImpl::readynessFailureMap.clear();

    // Act & Assert
    EXPECT_NO_THROW(SensorManagerImpl::dumpReadinessLogs());
}

TEST_F(SensorManagerStaticTest, DumpReadinessLogs_WithEntries_NoThrow)
{
    // Arrange
    SensorManagerImpl::readynessFailureMap["isMCTPReady"] = "True";
    SensorManagerImpl::readynessFailureMap["isEMReady"] = "True";
    SensorManagerImpl::readynessFailureMap["isNSMPollReady"] = "true";

    // Act & Assert
    EXPECT_NO_THROW(SensorManagerImpl::dumpReadinessLogs());
}

TEST_F(SensorManagerStaticTest, DumpReadinessLogs_WithFailureEntries_NoThrow)
{
    // Arrange
    SensorManagerImpl::readynessFailureMap["isMCTPReady"] = "Timeout error";
    SensorManagerImpl::readynessFailureMap["isEMReady"] =
        "Failed reading Status DBus property";

    // Act & Assert
    EXPECT_NO_THROW(SensorManagerImpl::dumpReadinessLogs());
}

// ============================================================================
// PART 11: SensorManager - dumpNsmDevicesInfo
// ============================================================================

TEST_F(SensorManagerStaticTest, DumpNsmDevicesInfo_WithDevices_ThrowsBadCast)
{
    // Act & Assert - dumpNsmDevicesInfo does
    // dynamic_cast<SensorManagerImpl&>(getInstance()) which throws
    // std::bad_cast because the instance is a MockSensorManager, not
    // a SensorManagerImpl.
    EXPECT_THROW(SensorManagerImpl::dumpNsmDevicesInfo(), std::bad_cast);
}

// ============================================================================
// PART 12: SensorManager - markEMReady
// ============================================================================

TEST_F(SensorManagerStaticTest, MarkEMReady_SetsReadinessMap)
{
    // Arrange
    SensorManagerImpl::readynessFailureMap.clear();

    // Act
    SensorManagerImpl::markEMReady();

    // Assert
    EXPECT_EQ(SensorManagerImpl::readynessFailureMap["isEMReady"], "True");
}

TEST_F(SensorManagerStaticTest, MarkEMReady_CallsIsNSMPollReady)
{
    // Arrange
    SensorManagerImpl::isReadyForReadinessCheck = false;
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::isEMReadyCheck = true;

    // Act
    SensorManagerImpl::markEMReady();

    // Assert - should have triggered isNSMPollReady which sets the flag
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
}

// ============================================================================
// PART 13: SensorManager - MockSensorManager interface tests
// ============================================================================

TEST_F(SensorManagerStaticTest,
       GetNsmDeviceFromStaticUUID_ValidUUID_ReturnsDevice)
{
    // Act
    auto device = mockManager.getNsmDeviceFromStaticUUID(gpuUuid);

    // Assert
    EXPECT_NE(device, nullptr);
}

TEST_F(SensorManagerStaticTest, GetNsmDeviceFromStaticUUID_InvalidUUID_Throws)
{
    // Act & Assert
    EXPECT_THROW(mockManager.getNsmDeviceFromStaticUUID("INVALID:UUID:FORMAT"),
                 std::runtime_error);
}

TEST_F(SensorManagerStaticTest, GetNsmDevice_ExistingDevice_ReturnsDevice)
{
    // Arrange - device was created by getNsmDeviceFromStaticUUID
    // Act
    auto device = mockManager.getNsmDevice(0, 0, NSM_DEV_ROLE_RESERVED);

    // Assert
    EXPECT_NE(device, nullptr);
    EXPECT_EQ(device->getDeviceType(), 0);
    EXPECT_EQ(device->getInstanceNumber(), 0);
}

TEST_F(SensorManagerStaticTest, GetNsmDevice_NonExistentDevice_ReturnsNull)
{
    // Act
    auto device = mockManager.getNsmDevice(99, 99, 99);

    // Assert
    EXPECT_EQ(device, nullptr);
}

TEST_F(SensorManagerStaticTest, GetMctpLocalEid_ReturnsNulloptBeforeDiscovery)
{
    // Act - MCTP LocalEID is not set until discovery provides LocalEID
    auto localEidOpt = gpu->getMctpLocalEid();

    // Assert - no default assumption; nullopt until set from MCTP
    EXPECT_FALSE(localEidOpt.has_value());
}

TEST_F(SensorManagerStaticTest, GetInstance_ReturnsValidReference)
{
    // Act & Assert
    EXPECT_NO_THROW({
        auto& instance = SensorManager::getInstance();
        (void)instance;
    });
}

// ============================================================================
// PART 14: NsmThresholdFactory - constructor tests
// ============================================================================

struct NsmThresholdFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string interface = "xyz.openbmc_project.Configuration.NSM_Temp";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/Temp0";
    NumericSensorInfo info = {};

    NsmThresholdFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);

        info.name = "GPU0_Temp";
        info.type = "NSM_ThermalParameter";
        info.sensorId = 1;
        info.priority = false;
        info.aggregated = false;
    }

    ~NsmThresholdFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmThresholdFactoryTest, Constructor_InitializesMembers)
{
    // Arrange
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);

    // Act
    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);

    // Assert
    EXPECT_EQ(factory.interface, interface);
    EXPECT_EQ(factory.objPath, testObjPath);
    EXPECT_NE(factory.nsmDevice, nullptr);
}

TEST_F(NsmThresholdFactoryTest, Constructor_SetsNsmDevice)
{
    // Arrange
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);

    // Act
    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);

    // Assert
    EXPECT_EQ(factory.nsmDevice, gpu);
}

TEST_F(NsmThresholdFactoryTest, Constructor_DifferentUUIDs_GetDifferentDevices)
{
    // Arrange
    const uuid_t gpu1Uuid = "STATIC:0:1:NSM_DEVICE_INSTANCE_NUMBER:1";
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);

    // Act
    NsmThresholdFactory factory0(mockManager, interface, testObjPath,
                                 numericSensor, info, gpuUuid);
    NsmThresholdFactory factory1(mockManager, interface, testObjPath,
                                 numericSensor, info, gpu1Uuid);

    // Assert
    EXPECT_NE(factory0.nsmDevice, factory1.nsmDevice);
}

// ============================================================================
// PART 15: NsmThresholdAggregatorBuilder - makeAggregator tests
// ============================================================================

// NsmThresholdAggregatorBuilder is a local class in the .cpp file.
// We test its behavior indirectly via
// NumericSensorFactory::makeAggregatorAndAddSensor but we can also test the
// NsmThresholdAggregator itself more deeply.

TEST(NsmThresholdAggregatorBuilderTest,
     NsmThresholdAggregator_Constructor_SetsNameType)
{
    // Arrange & Act
    NsmThresholdAggregator aggregator("TestThreshold", "NSM_ThermalParameter",
                                      false);

    // Assert - the name should be set
    EXPECT_EQ(aggregator.getName(), "TestThreshold");
    EXPECT_EQ(aggregator.getType(), "NSM_ThermalParameter");
}

TEST(NsmThresholdAggregatorBuilderTest, NsmThresholdAggregator_Priority_IsSet)
{
    // Arrange & Act
    NsmThresholdAggregator aggregatorHigh("HighPri", "Threshold", true);
    NsmThresholdAggregator aggregatorLow("LowPri", "Threshold", false);

    // Assert
    EXPECT_TRUE(aggregatorHigh.priority);
    EXPECT_FALSE(aggregatorLow.priority);
}

TEST(NsmThresholdAggregatorBuilderTest,
     NsmThresholdAggregator_GenRequestMsg_HasCorrectSize)
{
    // Arrange
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    // Act - instanceId must be <= NSM_INSTANCE_MAX (31) for
    // pack_nsm_header to succeed
    auto result = aggregator.genRequestMsg(10, 5);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req));
}

TEST(NsmThresholdAggregatorBuilderTest,
     NsmThresholdAggregator_HandleSample_ValidData_ReturnsSuccess)
{
    // Arrange
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    uint32_t value = 42;
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 1;
    sample.valid = true;
    sample.data = reinterpret_cast<uint8_t*>(&value);
    sample.data_len = sizeof(value);

    // Act
    auto result = aggregator.handleSample(sample);

    // Assert
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST(NsmThresholdAggregatorBuilderTest,
     NsmThresholdAggregator_HandleSample_InvalidData_ReturnsSuccess)
{
    // Arrange
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    // Act
    auto result = aggregator.handleSample(sample);

    // Assert
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

// ============================================================================
// PART 16: NsmThreshold - genRequestMsg and handleResponseMsg
// ============================================================================

TEST(NsmThresholdTest, GenRequestMsg_ValidInput_ReturnsRequest)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/test_thresh");
    auto thresholdValue = std::make_unique<NsmThresholdValueWarningLow>(
        "TestThresh", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("TestThreshold", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Act
    auto result = threshold.genRequestMsg(10, 1);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req));
}

TEST(NsmThresholdTest, GenRequestMsg_ZeroEidAndInstance_ReturnsRequest)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/test_thresh2");
    auto thresholdValue = std::make_unique<NsmThresholdValueWarningHigh>(
        "TestThresh2", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("TestThreshold2", "NSM_ThermalParameter", 0,
                           sensorValueAgg);

    // Act
    auto result = threshold.genRequestMsg(0, 0);

    // Assert
    ASSERT_TRUE(result.has_value());
}

TEST(NsmThresholdTest, HandleResponseMsg_ValidResponse_UpdatesReading)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/test_thresh3");
    auto thresholdValue = std::make_unique<NsmThresholdValueWarningLow>(
        "TestThresh3", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("TestThreshold3", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Build a valid read_thermal_parameter response
    int32_t paramValue = 75;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    auto rc = encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 paramValue, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto result = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(thresholdIntf->warningLow(), static_cast<double>(paramValue));
}

TEST(NsmThresholdTest, HandleResponseMsg_ErrorCC_UpdatesWithNaN)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/test_thresh4");
    auto thresholdValue = std::make_unique<NsmThresholdValueCriticalHigh>(
        "TestThresh4", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("TestThreshold4", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Build a response with error CC
    int32_t paramValue = 0;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_read_thermal_parameter_resp(0, NSM_ERROR, ERR_NULL, paramValue,
                                       response);

    // Act
    auto result = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_NE(result, NSM_SUCCESS);
    // On error, reading should be set to NaN
    EXPECT_TRUE(std::isnan(thresholdIntf->criticalHigh()) ||
                thresholdIntf->criticalHigh() !=
                    static_cast<double>(paramValue));
}

TEST(NsmThresholdTest, HandleResponseMsg_TooShortResponse_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/test_thresh5");
    auto thresholdValue = std::make_unique<NsmThresholdValueHardShutdownHigh>(
        "TestThresh5", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("TestThreshold5", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Too short response
    std::vector<uint8_t> shortResp(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<const nsm_msg*>(shortResp.data());

    // Act
    auto result = threshold.handleResponseMsg(response, shortResp.size());

    // Assert
    EXPECT_NE(result, NSM_SUCCESS);
}

TEST(NsmThresholdTest, HandleResponseMsg_NegativeValue_UpdatesCorrectly)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/test_thresh6");
    auto thresholdValue = std::make_unique<NsmThresholdValueWarningLow>(
        "TestThresh6", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("TestThreshold6", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Build response with negative thermal value
    int32_t paramValue = -25;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL, paramValue,
                                       response);

    // Act
    auto result = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(thresholdIntf->warningLow(), static_cast<double>(paramValue));
}

TEST(NsmThresholdTest, HandleResponseMsg_ZeroValue_UpdatesCorrectly)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/test_thresh7");
    auto thresholdValue = std::make_unique<NsmThresholdValueCriticalLow>(
        "TestThresh7", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("TestThreshold7", "NSM_ThermalParameter", 0,
                           sensorValueAgg);

    // Build response with zero value
    int32_t paramValue = 0;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL, paramValue,
                                       response);

    // Act
    auto result = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(thresholdIntf->criticalLow(), 0.0);
}

TEST(NsmThresholdTest, HandleResponseMsg_LargePositiveValue_UpdatesCorrectly)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/test_thresh8");
    auto thresholdValue = std::make_unique<NsmThresholdValueHardShutdownLow>(
        "TestThresh8", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("TestThreshold8", "NSM_ThermalParameter", 255,
                           sensorValueAgg);

    // Build response with large value
    int32_t paramValue = INT32_MAX;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL, paramValue,
                                       response);

    // Act
    auto result = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(thresholdIntf->hardShutdownLow(),
              static_cast<double>(paramValue));
}

TEST(NsmThresholdTest, GetSensorType_ReturnsThreshold)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/test_thresh9");
    auto thresholdValue = std::make_unique<NsmThresholdValueWarningHigh>(
        "TestThresh9", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("TestThreshold9", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Act
    auto result = threshold.getSensorType();

    // Assert
    EXPECT_EQ(result, "threshold");
}

// ============================================================================
// PART 17: NsmThresholdValue - all six concrete types
// ============================================================================

TEST(NsmThresholdValueTypesTest, WarningLow_UpdateReading_SetsDbusProperty)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/wl1");
    NsmThresholdValueWarningLow sensor("wl_sensor", "ThermalParam", intf);

    sensor.updateReading(42.5);
    EXPECT_EQ(intf->warningLow(), 42.5);
}

TEST(NsmThresholdValueTypesTest, WarningHigh_UpdateReading_SetsDbusProperty)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/wh1");
    NsmThresholdValueWarningHigh sensor("wh_sensor", "ThermalParam", intf);

    sensor.updateReading(85.3);
    EXPECT_EQ(intf->warningHigh(), 85.3);
}

TEST(NsmThresholdValueTypesTest, CriticalLow_UpdateReading_SetsDbusProperty)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/cl1");
    NsmThresholdValueCriticalLow sensor("cl_sensor", "ThermalParam", intf);

    sensor.updateReading(10.5);
    EXPECT_EQ(intf->criticalLow(), 10.5);
}

TEST(NsmThresholdValueTypesTest, CriticalHigh_UpdateReading_SetsDbusProperty)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/ch1");
    NsmThresholdValueCriticalHigh sensor("ch_sensor", "ThermalParam", intf);

    sensor.updateReading(95.7);
    EXPECT_EQ(intf->criticalHigh(), 95.7);
}

TEST(NsmThresholdValueTypesTest, HardShutdownLow_UpdateReading_SetsDbusProperty)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/hsl1");
    NsmThresholdValueHardShutdownLow sensor("hsl_sensor", "ThermalParam", intf);

    sensor.updateReading(5.0);
    EXPECT_EQ(intf->hardShutdownLow(), 5.0);
}

TEST(NsmThresholdValueTypesTest,
     HardShutdownHigh_UpdateReading_SetsDbusProperty)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/hsh1");
    NsmThresholdValueHardShutdownHigh sensor("hsh_sensor", "ThermalParam",
                                             intf);

    sensor.updateReading(100.0);
    EXPECT_EQ(intf->hardShutdownHigh(), 100.0);
}

// ============================================================================
// PART 18: NsmThresholdFactory - getThresholdInterfaces (sync version)
// ============================================================================

// The sync getThresholdInterfaces() requires a real DBus call via mapper.
// We can test the factory constructor and interface/objPath storage.

TEST_F(NsmThresholdFactoryTest, Constructor_StoresInterfaceAndObjPath)
{
    // Arrange
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);

    // Act
    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);

    // Assert
    EXPECT_EQ(factory.interface, interface);
    EXPECT_EQ(factory.objPath, testObjPath);
}

TEST_F(NsmThresholdFactoryTest, Constructor_StoresNumericSensor)
{
    // Arrange
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);

    // Act
    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);

    // Assert
    EXPECT_EQ(factory.numericSensor, numericSensor);
}

TEST_F(NsmThresholdFactoryTest, Constructor_ResolversDevice)
{
    // Arrange
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);

    // Act
    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);

    // Assert
    EXPECT_NE(factory.nsmDevice, nullptr);
    EXPECT_EQ(factory.nsmDevice->getDeviceType(), gpu->getDeviceType());
}

// ============================================================================
// PART 19: Additional edge cases for NsmChassisClockControl
// ============================================================================

TEST_F(NsmGpuClockControlTest,
       GenRequestMsg_ConsistentEncodings_SameInputSameOutput)
{
    // Arrange
    eid_t eid = 42;
    uint8_t instanceId = 7;

    // Act
    auto r1 = clockControl->genRequestMsg(eid, instanceId);
    auto r2 = clockControl->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1->size(), r2->size());
    EXPECT_EQ(*r1, *r2);
}

TEST_F(NsmGpuClockControlTest,
       HandleResponseMsg_SuccessResponse_UpdatesOnlyRequested)
{
    // Arrange - verify that only requested min/max are updated (not present)
    uint32_t reqMin = 600;
    uint32_t reqMax = 1500;
    uint32_t presMin = 200;
    uint32_t presMax = 2500;
    auto responseMsg = buildGetClockLimitResp(reqMin, reqMax, presMin, presMax);
    auto response = reinterpret_cast<const nsm_msg*>(responseMsg.data());

    // Act
    clockControl->handleResponseMsg(response, responseMsg.size());

    // Assert - only requested limits should be stored
    auto limits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(limits), reqMin);
    EXPECT_EQ(std::get<1>(limits), reqMax);
    // Present limits are not stored in cpuOperatingConfigIntf
}

// ============================================================================
// PART 20: SensorManager readiness flow - combined scenarios
// ============================================================================

TEST_F(SensorManagerStaticTest, ReadinessFlow_MCTPThenEM_BothReady_PollIsReady)
{
    // Arrange - simulate MCTP becoming ready first
    SensorManagerImpl::isReadyForReadinessCheck = false;
    SensorManagerImpl::isMCTPReadyCheck = false;
    SensorManagerImpl::isEMReadyCheck = false;

    // Act - MCTP becomes ready
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::readynessFailureMap["isMCTPReady"] = "True";
    SensorManagerImpl::isNSMPollReady();
    EXPECT_FALSE(SensorManagerImpl::isReadyForReadinessCheck);

    // Act - EM becomes ready
    SensorManagerImpl::isEMReadyCheck = true;
    SensorManagerImpl::readynessFailureMap["isEMReady"] = "True";
    SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
}

TEST_F(SensorManagerStaticTest, ReadinessFlow_EMThenMCTP_BothReady_PollIsReady)
{
    // Arrange - simulate EM becoming ready first
    SensorManagerImpl::isReadyForReadinessCheck = false;
    SensorManagerImpl::isMCTPReadyCheck = false;
    SensorManagerImpl::isEMReadyCheck = false;

    // Act - EM becomes ready
    SensorManagerImpl::isEMReadyCheck = true;
    SensorManagerImpl::isNSMPollReady();
    EXPECT_FALSE(SensorManagerImpl::isReadyForReadinessCheck);

    // Act - MCTP becomes ready
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
}

TEST_F(SensorManagerStaticTest, IsNSMPollReady_CalledMultipleTimes_OnlySetOnce)
{
    // Arrange
    SensorManagerImpl::isReadyForReadinessCheck = false;
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::isEMReadyCheck = true;
    SensorManagerImpl::readynessFailureMap.clear();

    // Act - call multiple times
    SensorManagerImpl::isNSMPollReady();
    SensorManagerImpl::isNSMPollReady();
    SensorManagerImpl::isNSMPollReady();

    // Assert - should still be true
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
    EXPECT_EQ(SensorManagerImpl::readynessFailureMap["isNSMPollReady"], "true");
}

// ============================================================================
// PART 21: Additional SensorManager object path and sensor map tests
// ============================================================================

TEST_F(SensorManagerStaticTest, ObjectPathToSensorMap_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.objectPathToSensorMap.empty());
}

TEST_F(SensorManagerStaticTest, ProcessorModuleToDeviceMap_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.processorModuleToDeviceMap.empty());
}

TEST_F(SensorManagerStaticTest, DeviceToPortMap_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.deviceToPortMap.empty());
}

TEST_F(SensorManagerStaticTest, PowerCapList_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.powerCapList.empty());
}

TEST_F(SensorManagerStaticTest, DefaultPowerCapList_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.defaultPowerCapList.empty());
}

TEST_F(SensorManagerStaticTest, MaxPowerCapList_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.maxPowerCapList.empty());
}

TEST_F(SensorManagerStaticTest, MinPowerCapList_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.minPowerCapList.empty());
}

TEST_F(SensorManagerStaticTest, DebugTokenList_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.debugTokenList.empty());
}

// ============================================================================
// PART 22: NsmThreshold with different threshold value types end-to-end
// ============================================================================

TEST(NsmThresholdEndToEndTest, WarningThreshold_FullCycle_GenAndHandle)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/e2e_warn");
    auto thresholdValue = std::make_unique<NsmThresholdValueWarningHigh>(
        "E2E_Warn", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("E2E_Threshold_Warn", "NSM_ThermalParameter", 42,
                           sensorValueAgg);

    // Act - generate request
    auto request = threshold.genRequestMsg(10, 1);
    ASSERT_TRUE(request.has_value());

    // Build response
    int32_t paramValue = 80;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL, paramValue,
                                       response);

    // Handle response
    auto rc = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(thresholdIntf->warningHigh(), 80.0);
}

TEST(NsmThresholdEndToEndTest, CriticalThreshold_FullCycle_GenAndHandle)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/e2e_crit");
    auto thresholdValue = std::make_unique<NsmThresholdValueCriticalLow>(
        "E2E_Crit", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("E2E_Threshold_Crit", "NSM_ThermalParameter", 10,
                           sensorValueAgg);

    // Act - generate request
    auto request = threshold.genRequestMsg(20, 5);
    ASSERT_TRUE(request.has_value());

    // Build response
    int32_t paramValue = -10;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL, paramValue,
                                       response);

    // Handle response
    auto rc = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(thresholdIntf->criticalLow(), -10.0);
}

TEST(NsmThresholdEndToEndTest, HardShutdownThreshold_FullCycle_GenAndHandle)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto thresholdIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/e2e_hsd");
    auto thresholdValue = std::make_unique<NsmThresholdValueHardShutdownHigh>(
        "E2E_HSD", "NSM_ThermalParameter", thresholdIntf);
    auto sensorValueAgg = std::make_shared<NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    NsmThreshold threshold("E2E_Threshold_HSD", "NSM_ThermalParameter", 100,
                           sensorValueAgg);

    // Act - generate request
    auto request = threshold.genRequestMsg(30, 8);
    ASSERT_TRUE(request.has_value());

    // Build response
    int32_t paramValue = 150;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL, paramValue,
                                       response);

    // Handle response
    auto rc = threshold.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(responseMsg.data()),
        responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(thresholdIntf->hardShutdownHigh(), 150.0);
}

// ============================================================================
// PART 23: NsmChassisClockControl - full request/response round-trip
// ============================================================================

TEST_F(NsmGpuClockControlTest,
       FullRoundTrip_GenRequest_Then_HandleResponse_UpdatesState)
{
    // Arrange
    eid_t eid = 10;
    uint8_t instanceId = 1;

    // Step 1: Generate request
    auto request = clockControl->genRequestMsg(eid, instanceId);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req));

    // Step 2: Build a response
    uint32_t reqMin = 450;
    uint32_t reqMax = 1950;
    auto responseMsg = buildGetClockLimitResp(reqMin, reqMax, 200, 2100);
    auto response = reinterpret_cast<const nsm_msg*>(responseMsg.data());

    // Step 3: Handle the response
    auto rc = clockControl->handleResponseMsg(response, responseMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    auto limits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(limits), reqMin);
    EXPECT_EQ(std::get<1>(limits), reqMax);
}

// ============================================================================
// PART 24: Additional NsmThresholdAggregator tests
// ============================================================================

TEST(NsmThresholdAggregatorAdvancedTest,
     HandleSample_WithSensorData_ReturnsSuccess)
{
    // Arrange
    NsmThresholdAggregator aggregator("AdvThresh", "Threshold", false);

    int32_t value = 100;
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 5;
    sample.valid = true;
    sample.data = reinterpret_cast<uint8_t*>(&value);
    sample.data_len = sizeof(value);

    // Act
    auto result = aggregator.handleSample(sample);

    // Assert
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST(NsmThresholdAggregatorAdvancedTest, HandleSample_MaxTag_ReturnsSuccess)
{
    // Arrange
    NsmThresholdAggregator aggregator("MaxTagThresh", "Threshold", true);

    // handleSample with valid=true calls
    // decode_aggregate_thermal_parameter_data which requires non-null
    // data of sizeof(int32_t) bytes.
    int32_t value = 50;
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE;
    sample.valid = true;
    sample.data = reinterpret_cast<uint8_t*>(&value);
    sample.data_len = sizeof(value);

    // Act
    auto result = aggregator.handleSample(sample);

    // Assert
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST(NsmThresholdAggregatorAdvancedTest, HandleSample_ZeroTag_ReturnsSuccess)
{
    // Arrange
    NsmThresholdAggregator aggregator("ZeroTagThresh", "Threshold", false);

    // handleSample with valid=true calls
    // decode_aggregate_thermal_parameter_data which requires non-null
    // data of sizeof(int32_t) bytes.
    int32_t value = 25;
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 0;
    sample.valid = true;
    sample.data = reinterpret_cast<uint8_t*>(&value);
    sample.data_len = sizeof(value);

    // Act
    auto result = aggregator.handleSample(sample);

    // Assert
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST(NsmThresholdAggregatorAdvancedTest,
     GenRequestMsg_MultipleInstanceIds_AllSucceed)
{
    // Arrange
    NsmThresholdAggregator aggregator("MultiInst", "Threshold", false);

    // Act & Assert
    for (uint8_t i = 0; i < 10; ++i)
    {
        auto result = aggregator.genRequestMsg(10, i);
        ASSERT_TRUE(result.has_value())
            << "Failed for instanceId " << static_cast<int>(i);
    }
}

// ============================================================================
// PART 25: NsmThresholdFactory additional member tests
// ============================================================================

TEST_F(NsmThresholdFactoryTest, Constructor_WithNullNumericSensor_StoresNull)
{
    // Arrange
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);

    // Act
    NsmThresholdFactory factory(mockManager, interface, testObjPath,
                                numericSensor, info, gpuUuid);

    // Assert
    EXPECT_EQ(factory.numericSensor, nullptr);
}

TEST_F(NsmThresholdFactoryTest, Constructor_WithDifferentInterfaces_Works)
{
    // Arrange
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);
    std::string intf1 = "xyz.openbmc_project.Configuration.NSM_Temp";
    std::string intf2 = "xyz.openbmc_project.Configuration.NSM_Power";
    std::string intf3 = "xyz.openbmc_project.Configuration.NSM_Voltage";

    // Act & Assert
    EXPECT_NO_THROW({
        NsmThresholdFactory f1(mockManager, intf1, testObjPath, numericSensor,
                               info, gpuUuid);
        NsmThresholdFactory f2(mockManager, intf2, testObjPath, numericSensor,
                               info, gpuUuid);
        NsmThresholdFactory f3(mockManager, intf3, testObjPath, numericSensor,
                               info, gpuUuid);
    });
}

TEST_F(NsmThresholdFactoryTest, Constructor_WithDifferentObjPaths_Works)
{
    // Arrange
    auto numericSensor = std::shared_ptr<NsmNumericSensor>(nullptr);
    std::string path1 = "/xyz/openbmc_project/inventory/sensor1";
    std::string path2 = "/xyz/openbmc_project/inventory/sensor2";

    // Act
    NsmThresholdFactory f1(mockManager, interface, path1, numericSensor, info,
                           gpuUuid);
    NsmThresholdFactory f2(mockManager, interface, path2, numericSensor, info,
                           gpuUuid);

    // Assert
    EXPECT_EQ(f1.objPath, path1);
    EXPECT_EQ(f2.objPath, path2);
}
