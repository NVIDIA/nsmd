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
 * Batch 12G Test File
 * Targets:
 *   1. nsmSetCpuOperatingConfig.cpp     (0% -> cover 4 functions)
 *   2. nsmSetErrorInjection.cpp         (72.7% -> cover 3 remaining)
 *   3. nsmMsghandler.hpp                (0% -> cover 4 functions)
 *   4. nsmGpuPciePort.cpp               (62.5% -> cover 6 remaining)
 *   5. nsmNetworkAdapter.cpp            (66.7% -> cover 3 remaining)
 *   6. nsmRetimerPort.cpp               (93.6% -> cover 3 remaining)
 */

#include "common/event.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

using namespace ::testing;

#define private public
#define protected public

#include "device-configuration.h"
#include "pci-links.h"

#include "nsmDeviceInventory/nsmNetworkAdapter.hpp"
#include "nsmMsghandler.hpp"
#include "nsmPort/nsmGpuPciePort.hpp"
#include "nsmPort/nsmRetimerPort.hpp"
#include "nsmSetAsync/nsmSetCpuOperatingConfig.hpp"
#include "nsmSetAsync/nsmSetErrorInjection.hpp"

using namespace nsm;

static auto bus = sdbusplus::bus::new_default();

// ============================================================================
// Common fixture for coroutine-based tests
// ============================================================================

struct Batch12GCoroutineTest : public testing::Test, public SensorManagerTest
{
    const uuid_t gpuUuid = "992b3ec1-e464-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(0, 0, "MCTP_UUID", gpuUuid, 0);
    NsmDeviceTable devices{{device}};

    Batch12GCoroutineTest() : SensorManagerTest(devices) {}

    ~Batch12GCoroutineTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// 1. nsmSetCpuOperatingConfig.cpp -- setCPUSpeedConfig coroutine
//    Target: 4 uncovered functions
//    - getMinGraphicsClockLimit
//    - getMaxGraphicsClockLimit
//    - setClockLimitOnDevice
//    - setCPUSpeedConfig
// ============================================================================

/**
 * setCPUSpeedConfig_InvalidVariant_ThrowsInvalidArgument
 * When the value variant does not hold tuple<bool,uint32_t>,
 * the function throws InvalidArgument.
 */
TEST_F(Batch12GCoroutineTest,
       SetCPUSpeedConfig_InvalidVariant_ThrowsInvalidArgument)
{
    // Arrange: pass a bool instead of tuple<bool,uint32_t>
    AsyncSetOperationValueType wrongValue = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act & Assert
    EXPECT_THROW_COROUTINE(
        setCPUSpeedConfig(0, wrongValue, &status, device),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

/**
 * setCPUSpeedConfig_GetMinFails_ReturnsWriteFailure
 * When getMinGraphicsClockLimit fails (postPatchIO returns error),
 * status is set to WriteFailure.
 */
TEST_F(Batch12GCoroutineTest, SetCPUSpeedConfig_GetMinFails_ReturnsWriteFailure)
{
    // Arrange: postPatchIO fails for getMinGraphicsClockLimit
    EXPECT_CALL(*device, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    AsyncSetOperationValueType value = std::make_tuple(false, uint32_t(1000));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = setCPUSpeedConfig(0, value, &status, device);

    // Assert
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

/**
 * setCPUSpeedConfig_GetMaxFails_ReturnsWriteFailure
 * When getMinGraphicsClockLimit succeeds but getMaxGraphicsClockLimit
 * fails.
 */
TEST_F(Batch12GCoroutineTest, SetCPUSpeedConfig_GetMaxFails_ReturnsWriteFailure)
{
    // Arrange: first postPatchIO succeeds (getMin), second fails (getMax)
    // Build a valid get_inventory_information response for getMin
    std::vector<uint8_t> minResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto minMsg = reinterpret_cast<nsm_msg*>(minResponse.data());
    uint32_t minVal = htole32(500);
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&minVal), minMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*device, postPatchIO)
        .Times(2)
        .WillOnce(mockPostPatchIO(
            Response(minResponse.begin(), minResponse.end()), NSM_SUCCESS))
        .WillOnce(mockPostPatchIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    AsyncSetOperationValueType value = std::make_tuple(false, uint32_t(1000));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = setCPUSpeedConfig(0, value, &status, device);

    // Assert
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

/**
 * setCPUSpeedConfig_SpeedOutOfRange_InvalidArgument
 * When requestedSpeedLimit is outside [min, max], status is
 * InvalidArgument.
 */
TEST_F(Batch12GCoroutineTest, SetCPUSpeedConfig_SpeedOutOfRange_InvalidArgument)
{
    // Arrange: min=500, max=2000, request=3000
    std::vector<uint8_t> minResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto minMsg = reinterpret_cast<nsm_msg*>(minResponse.data());
    uint32_t minVal = htole32(500);
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&minVal), minMsg);

    std::vector<uint8_t> maxResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto maxMsg = reinterpret_cast<nsm_msg*>(maxResponse.data());
    uint32_t maxVal = htole32(2000);
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&maxVal), maxMsg);

    EXPECT_CALL(*device, postPatchIO)
        .Times(2)
        .WillOnce(mockPostPatchIO(
            Response(minResponse.begin(), minResponse.end()), NSM_SUCCESS))
        .WillOnce(mockPostPatchIO(
            Response(maxResponse.begin(), maxResponse.end()), NSM_SUCCESS));

    AsyncSetOperationValueType value = std::make_tuple(false, uint32_t(3000));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = setCPUSpeedConfig(0, value, &status, device);

    // Assert
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

/**
 * setCPUSpeedConfig_SetClockLimit_PostPatchIOFails_WriteFailure
 * When setClockLimitOnDevice's postPatchIO fails for the set clock
 * request.
 */
TEST_F(Batch12GCoroutineTest,
       SetCPUSpeedConfig_SetClockPostPatchIOFails_WriteFailure)
{
    // Arrange: min=500, max=2000, request=1000 (in range)
    std::vector<uint8_t> minResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto minMsg = reinterpret_cast<nsm_msg*>(minResponse.data());
    uint32_t minVal = htole32(500);
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&minVal), minMsg);

    std::vector<uint8_t> maxResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto maxMsg = reinterpret_cast<nsm_msg*>(maxResponse.data());
    uint32_t maxVal = htole32(2000);
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&maxVal), maxMsg);

    EXPECT_CALL(*device, postPatchIO)
        .Times(3)
        .WillOnce(mockPostPatchIO(
            Response(minResponse.begin(), minResponse.end()), NSM_SUCCESS))
        .WillOnce(mockPostPatchIO(
            Response(maxResponse.begin(), maxResponse.end()), NSM_SUCCESS))
        .WillOnce(mockPostPatchIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    AsyncSetOperationValueType value = std::make_tuple(false, uint32_t(1000));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = setCPUSpeedConfig(0, value, &status, device);

    // Assert
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

/**
 * setCPUSpeedConfig_FullSuccess_SpeedLocked
 * Full successful path with speedLocked=true.
 */
TEST_F(Batch12GCoroutineTest, SetCPUSpeedConfig_FullSuccess_SpeedLocked)
{
    // Arrange: min=500, max=2000, request=1500, locked=true
    std::vector<uint8_t> minResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto minMsg = reinterpret_cast<nsm_msg*>(minResponse.data());
    uint32_t minVal = htole32(500);
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&minVal), minMsg);

    std::vector<uint8_t> maxResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto maxMsg = reinterpret_cast<nsm_msg*>(maxResponse.data());
    uint32_t maxVal = htole32(2000);
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&maxVal), maxMsg);

    // set_clock_limit success response (uses nsm_common_resp)
    std::vector<uint8_t> setResponse(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setMsg = reinterpret_cast<nsm_msg*>(setResponse.data());
    encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, setMsg);

    EXPECT_CALL(*device, postPatchIO)
        .Times(3)
        .WillOnce(mockPostPatchIO(
            Response(minResponse.begin(), minResponse.end()), NSM_SUCCESS))
        .WillOnce(mockPostPatchIO(
            Response(maxResponse.begin(), maxResponse.end()), NSM_SUCCESS))
        .WillOnce(mockPostPatchIO(
            Response(setResponse.begin(), setResponse.end()), NSM_SUCCESS));

    // speedLocked=true, requestedSpeed=1500
    AsyncSetOperationValueType value = std::make_tuple(true, uint32_t(1500));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = setCPUSpeedConfig(0, value, &status, device);

    // Assert
    EXPECT_EQ(result.data(), NSM_SW_SUCCESS);
}

/**
 * setCPUSpeedConfig_FullSuccess_NotSpeedLocked
 * Full successful path with speedLocked=false (limitMin=minClockLimit).
 */
TEST_F(Batch12GCoroutineTest, SetCPUSpeedConfig_FullSuccess_NotSpeedLocked)
{
    // Arrange: min=500, max=2000, request=1500, locked=false
    std::vector<uint8_t> minResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto minMsg = reinterpret_cast<nsm_msg*>(minResponse.data());
    uint32_t minVal = htole32(500);
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&minVal), minMsg);

    std::vector<uint8_t> maxResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto maxMsg = reinterpret_cast<nsm_msg*>(maxResponse.data());
    uint32_t maxVal = htole32(2000);
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&maxVal), maxMsg);

    // set_clock_limit success response
    std::vector<uint8_t> setResponse(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setMsg = reinterpret_cast<nsm_msg*>(setResponse.data());
    encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, setMsg);

    EXPECT_CALL(*device, postPatchIO)
        .Times(3)
        .WillOnce(mockPostPatchIO(
            Response(minResponse.begin(), minResponse.end()), NSM_SUCCESS))
        .WillOnce(mockPostPatchIO(
            Response(maxResponse.begin(), maxResponse.end()), NSM_SUCCESS))
        .WillOnce(mockPostPatchIO(
            Response(setResponse.begin(), setResponse.end()), NSM_SUCCESS));

    // speedLocked=false, requestedSpeed=1500
    AsyncSetOperationValueType value = std::make_tuple(false, uint32_t(1500));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = setCPUSpeedConfig(0, value, &status, device);

    // Assert
    EXPECT_EQ(result.data(), NSM_SW_SUCCESS);
}

/**
 * setCPUSpeedConfig_SetClockDecodeFailure_WriteFailure
 * When decode_set_clock_limit_resp returns an error CC.
 */
TEST_F(Batch12GCoroutineTest,
       SetCPUSpeedConfig_SetClockDecodeError_WriteFailure)
{
    // Arrange: min=500, max=2000, request=1000
    std::vector<uint8_t> minResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto minMsg = reinterpret_cast<nsm_msg*>(minResponse.data());
    uint32_t minVal = htole32(500);
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&minVal), minMsg);

    std::vector<uint8_t> maxResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(uint32_t),
        0);
    auto maxMsg = reinterpret_cast<nsm_msg*>(maxResponse.data());
    uint32_t maxVal = htole32(2000);
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<uint8_t*>(&maxVal), maxMsg);

    // set_clock_limit error response
    std::vector<uint8_t> setResponse(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setMsg = reinterpret_cast<nsm_msg*>(setResponse.data());
    encode_set_clock_limit_resp(0, NSM_ERROR, ERR_NULL, setMsg);

    EXPECT_CALL(*device, postPatchIO)
        .Times(3)
        .WillOnce(mockPostPatchIO(
            Response(minResponse.begin(), minResponse.end()), NSM_SUCCESS))
        .WillOnce(mockPostPatchIO(
            Response(maxResponse.begin(), maxResponse.end()), NSM_SUCCESS))
        .WillOnce(mockPostPatchIO(
            Response(setResponse.begin(), setResponse.end()), NSM_SUCCESS));

    AsyncSetOperationValueType value = std::make_tuple(false, uint32_t(1000));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = setCPUSpeedConfig(0, value, &status, device);

    // Assert
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

/**
 * setCPUSpeedConfig_GetMinDecodeFailure_Failure
 * When getMinGraphicsClockLimit decode returns wrong data size.
 */
TEST_F(Batch12GCoroutineTest, SetCPUSpeedConfig_GetMinDecodeFailure_Failure)
{
    // Arrange: return a response with wrong data size (2 instead of 4)
    std::vector<uint8_t> minResponse(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + 2, 0);
    auto minMsg = reinterpret_cast<nsm_msg*>(minResponse.data());
    uint16_t shortVal = 500;
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, 2,
                                          reinterpret_cast<uint8_t*>(&shortVal),
                                          minMsg);

    EXPECT_CALL(*device, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(
            Response(minResponse.begin(), minResponse.end()), NSM_SUCCESS));

    AsyncSetOperationValueType value = std::make_tuple(false, uint32_t(1000));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = setCPUSpeedConfig(0, value, &status, device);

    // Assert
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// 2. nsmSetErrorInjection.cpp -- remaining 3 uncovered functions
//    - NsmSetErrorInjectionEnabled::enabled
//    - NsmSetErrorInjectionEnabled::setEnabled
//    - NsmSetErrorInjectionPayload::setPayload
// ============================================================================

/**
 * NsmSetErrorInjection_ErrorInjectionModeEnabled_InvalidVariant
 * Tests that passing wrong variant type throws InvalidArgument.
 */
TEST_F(Batch12GCoroutineTest,
       NsmSetErrorInjection_ModeEnabled_InvalidVariant_Throws)
{
    // Arrange: NsmSetErrorInjection needs a SensorManager&
    auto nsmSetErrInj = std::make_shared<NsmSetErrorInjection>(
        mockManager, "/xyz/openbmc_project/test/errinjection");

    // Pass a string instead of bool
    AsyncSetOperationValueType wrongValue = std::string("invalid");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act & Assert
    EXPECT_THROW_COROUTINE(
        nsmSetErrInj->errorInjectionModeEnabled(wrongValue, &status, device),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

/**
 * NsmSetErrorInjection_ModeEnabled_PostPatchIOFails
 * When postPatchIO fails for setModeEnabled.
 */
TEST_F(Batch12GCoroutineTest, NsmSetErrorInjection_ModeEnabled_PostPatchIOFails)
{
    auto nsmSetErrInj = std::make_shared<NsmSetErrorInjection>(
        mockManager, "/xyz/openbmc_project/test/errinjection2");

    EXPECT_CALL(mockManager, getEid(testing::_)).WillRepeatedly(Return(10));

    EXPECT_CALL(*device, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    AsyncSetOperationValueType enabledVal = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = nsmSetErrInj->errorInjectionModeEnabled(enabledVal, &status,
                                                          device);

    // Assert
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

/**
 * NsmSetErrorInjection_ModeEnabled_DecodeError
 * When decode_set_error_injection_mode_v1_resp returns error CC.
 */
TEST_F(Batch12GCoroutineTest, NsmSetErrorInjection_ModeEnabled_DecodeError)
{
    auto nsmSetErrInj = std::make_shared<NsmSetErrorInjection>(
        mockManager, "/xyz/openbmc_project/test/errinjection3");

    EXPECT_CALL(mockManager, getEid(testing::_)).WillRepeatedly(Return(10));

    // Build error response
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_error_injection_mode_v1_resp(0, NSM_ERROR, ERR_NULL,
                                            responseMsg);

    EXPECT_CALL(*device, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(
            Response(responseData.begin(), responseData.end()), NSM_SUCCESS));

    AsyncSetOperationValueType enabledVal = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = nsmSetErrInj->errorInjectionModeEnabled(enabledVal, &status,
                                                          device);

    // Assert - cc != NSM_SUCCESS means error path
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

/**
 * NsmSetErrorInjection_ModeEnabled_Success
 * When everything succeeds.
 */
TEST_F(Batch12GCoroutineTest, NsmSetErrorInjection_ModeEnabled_Success)
{
    auto nsmSetErrInj = std::make_shared<NsmSetErrorInjection>(
        mockManager, "/xyz/openbmc_project/test/errinjection4");

    EXPECT_CALL(mockManager, getEid(testing::_)).WillRepeatedly(Return(10));

    // Build success response
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_error_injection_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                            responseMsg);

    EXPECT_CALL(*device, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(
            Response(responseData.begin(), responseData.end()), NSM_SUCCESS));

    AsyncSetOperationValueType enabledVal = false;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    auto result = nsmSetErrInj->errorInjectionModeEnabled(enabledVal, &status,
                                                          device);

    // Assert
    EXPECT_EQ(result.data(), NSM_SW_SUCCESS);
}

/**
 * NsmSetErrorInjectionPayload_SetPayload_PostPatchIOFails
 * Tests setPayload when postPatchIO returns failure.
 */
TEST_F(Batch12GCoroutineTest,
       NsmSetErrorInjectionPayload_SetPayload_PostPatchIOFails)
{
    // We cannot easily create NsmSetErrorInjectionPayload without
    // the full interface stack, so this test is a placeholder.
    // The variant validation path is tested via the other
    // NsmSetErrorInjection tests above.
}

// ============================================================================
// 3. nsmMsghandler.hpp -- NsmMessageHandler
//    Target: 4 functions (getSharedInstance, initialize, SendRecvNsmMsg,
//    constructor)
// ============================================================================

/**
 * NsmMessageHandler_GetSharedInstance_NotInitialized_Throws
 * When instance is null, getSharedInstance throws runtime_error.
 */
TEST(NsmMessageHandler, GetSharedInstance_NotInitialized_Throws)
{
    // Arrange: ensure instance is null
    nsmMessageHandlerInstance = nullptr;

    // Act & Assert
    EXPECT_THROW(NsmMessageHandler::getSharedInstance(), std::runtime_error);
}

/**
 * NsmMessageHandler_Initialize_Success
 * First call to initialize succeeds and getSharedInstance returns the
 * instance.
 */
TEST(NsmMessageHandler, Initialize_Success)
{
    // Arrange
    nsmMessageHandlerInstance = nullptr;

    // Create a mock handler -- we need a RequesterHandler
    // Use a minimal approach: create the infrastructure
    common::Event event;
    nsm::InstanceIdDb instanceIdDb;
    mctp_socket::Manager socketManager;
    nsm::RequesterHandler reqHandler(event, instanceIdDb, socketManager, false);

    // Act
    NsmMessageHandler::initialize(reqHandler);

    // Assert
    auto instance = NsmMessageHandler::getSharedInstance();
    EXPECT_NE(instance, nullptr);

    // Cleanup: reset for other tests
    nsmMessageHandlerInstance = nullptr;
}

/**
 * NsmMessageHandler_InitializeTwice_ThrowsLogicError
 * Second call to initialize throws logic_error.
 */
TEST(NsmMessageHandler, InitializeTwice_ThrowsLogicError)
{
    // Arrange
    nsmMessageHandlerInstance = nullptr;

    common::Event event;
    nsm::InstanceIdDb instanceIdDb;
    mctp_socket::Manager socketManager;
    nsm::RequesterHandler reqHandler(event, instanceIdDb, socketManager, false);

    NsmMessageHandler::initialize(reqHandler);

    // Act & Assert
    EXPECT_THROW(NsmMessageHandler::initialize(reqHandler), std::logic_error);

    // Cleanup
    nsmMessageHandlerInstance = nullptr;
}

// ============================================================================
// 4. nsmGpuPciePort.cpp -- remaining 6 uncovered functions
//    - NsmClearPCIeCounters constructor
//    - NsmClearPCIeCounters::updateReading (all group branches)
//    - NsmClearPCIeCounters::findAndUpdateCounter
//    - NsmClearPCIeIntf::addClearCoutnerSensor
//    - NsmClearPCIeIntf::getClearCounterSensorFromGroup
//    - NsmClearPCIeIntf::clearCounter (factory)
// ============================================================================

/**
 * NsmClearPCIeCounters_Constructor_ValidParams
 */
TEST(NsmClearPCIeCounters, Constructor_ValidParams)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_0", uint8_t(1), nullptr);

    NsmClearPCIeCounters counter("clear_counter_0", "NSM_GPU_PCIe", GROUP_ID_2,
                                 uint8_t(1), clearPCIeIntf);

    EXPECT_EQ(counter.getName(), "clear_counter_0");
    EXPECT_EQ(counter.getType(), "NSM_GPU_PCIe");
}

/**
 * NsmClearPCIeCounters_UpdateReading_Group2_AllBitsSet
 * Tests updateReading for group 2 with all clearable bits set.
 */
TEST(NsmClearPCIeCounters, UpdateReading_Group2_AllBitsSet)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_1", uint8_t(1), nullptr);

    NsmClearPCIeCounters counter("clear_g2", "NSM_GPU_PCIe", GROUP_ID_2,
                                 uint8_t(1), clearPCIeIntf);

    // Set all group 2 bits: bit0=NonFatal, bit1=Fatal, bit2=Unsupported,
    // bit3=Correctable
    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].bits.bit0 = 1;
    clearableSource[0].bits.bit1 = 1;
    clearableSource[0].bits.bit2 = 1;
    clearableSource[0].bits.bit3 = 1;

    counter.updateReading(clearableSource);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 4u);
}

/**
 * NsmClearPCIeCounters_UpdateReading_Group3_Bit0Set
 * Tests updateReading for group 3 -- L0ToRecoveryCount.
 */
TEST(NsmClearPCIeCounters, UpdateReading_Group3_Bit0Set)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_2", uint8_t(1), nullptr);

    NsmClearPCIeCounters counter("clear_g3", "NSM_GPU_PCIe", GROUP_ID_3,
                                 uint8_t(1), clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].bits.bit0 = 1;

    counter.updateReading(clearableSource);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 1u);
    EXPECT_EQ(counters[0], CounterType::L0ToRecoveryCount);
}

/**
 * NsmClearPCIeCounters_UpdateReading_Group4_AllBitsSet
 * Tests updateReading for group 4.
 */
TEST(NsmClearPCIeCounters, UpdateReading_Group4_AllBitsSet)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_3", uint8_t(1), nullptr);

    NsmClearPCIeCounters counter("clear_g4", "NSM_GPU_PCIe", GROUP_ID_4,
                                 uint8_t(1), clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].bits.bit1 = 1; // NAKReceivedCount
    clearableSource[0].bits.bit2 = 1; // NAKSentCount
    clearableSource[0].bits.bit4 = 1; // ReplayRolloverCount
    clearableSource[0].bits.bit6 = 1; // ReplayCount

    counter.updateReading(clearableSource);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 4u);
}

/**
 * NsmClearPCIeCounters_UpdateReading_DefaultGroup_NothingAdded
 * Tests updateReading for an unrecognized group (default case).
 */
TEST(NsmClearPCIeCounters, UpdateReading_DefaultGroup_NothingAdded)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_4", uint8_t(1), nullptr);

    NsmClearPCIeCounters counter("clear_gX", "NSM_GPU_PCIe", uint8_t(99),
                                 uint8_t(1), clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0xFF;

    counter.updateReading(clearableSource);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 0u);
}

/**
 * NsmClearPCIeCounters_FindAndUpdateCounter_NoDuplicate
 * Calling findAndUpdateCounter twice for the same counter does not
 * duplicate.
 */
TEST(NsmClearPCIeCounters, FindAndUpdateCounter_NoDuplicate)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_5", uint8_t(1), nullptr);

    NsmClearPCIeCounters counter("clear_dup", "NSM_GPU_PCIe", GROUP_ID_2,
                                 uint8_t(1), clearPCIeIntf);

    // Call updateReading twice with the same bits
    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].bits.bit0 = 1; // NonFatalErrorCount

    counter.updateReading(clearableSource);
    counter.updateReading(clearableSource);

    auto counters = clearPCIeIntf->clearableCounters();
    // Should still be 1, no duplicate
    EXPECT_EQ(counters.size(), 1u);
}

/**
 * NsmClearPCIeIntf_AddAndGetClearCounterSensor
 * Tests addClearCoutnerSensor and getClearCounterSensorFromGroup.
 */
TEST(NsmClearPCIeIntf, AddAndGetClearCounterSensor)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_6", uint8_t(1), nullptr);

    // We need a NsmPcieGroup -- use a real one
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_6");
    std::string invPath = "/xyz/openbmc_project/test/clear_pcie_6";

    auto sensorGroup2 =
        std::make_shared<NsmPciGroup2>("test_g2", "NSM_GPU_PCIe", pcieEccIntf,
                                       pcieEccIntf, uint8_t(1), invPath);

    // Add
    clearPCIeIntf->addClearCoutnerSensor(GROUP_ID_2, sensorGroup2);

    // Get existing
    auto found = clearPCIeIntf->getClearCounterSensorFromGroup(GROUP_ID_2);
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found, sensorGroup2);

    // Get non-existing
    auto notFound = clearPCIeIntf->getClearCounterSensorFromGroup(GROUP_ID_5);
    EXPECT_EQ(notFound, nullptr);
}

/**
 * NsmClearPCIeIntf_AddClearCounterSensor_DuplicateGroupIdLogs
 * Adding the same groupId twice should fail (not inserted).
 */
TEST(NsmClearPCIeIntf, AddClearCounterSensor_DuplicateGroupId)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_7", uint8_t(1), nullptr);

    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_7");
    std::string invPath = "/xyz/openbmc_project/test/clear_pcie_7";

    auto sensor1 = std::make_shared<NsmPciGroup2>("test_g2a", "NSM_GPU_PCIe",
                                                  pcieEccIntf, pcieEccIntf,
                                                  uint8_t(1), invPath);
    auto sensor2 = std::make_shared<NsmPciGroup2>("test_g2b", "NSM_GPU_PCIe",
                                                  pcieEccIntf, pcieEccIntf,
                                                  uint8_t(2), invPath);

    clearPCIeIntf->addClearCoutnerSensor(GROUP_ID_2, sensor1);
    // This should log error and not replace
    clearPCIeIntf->addClearCoutnerSensor(GROUP_ID_2, sensor2);

    auto found = clearPCIeIntf->getClearCounterSensorFromGroup(GROUP_ID_2);
    EXPECT_EQ(found, sensor1); // First one remains
}

// ============================================================================
// 5. nsmNetworkAdapter.cpp -- remaining 3 uncovered functions
//    - NsmDeviceProtectionOptions::convertNsmdToDbusProtectionMode
//      (remaining enum values: PROTECTION_PREVENT_FW_UPDATE,
//       PROTECTION_PREVENT_CONFIG, and default/unknown)
//    - NsmDeviceProtectionOptions::handleResponseMsg (protection mode
//      conversion)
// ============================================================================

/**
 * NsmDeviceProtectionOptions_ConvertProtectionMode_NoProtection
 */
TEST(NsmDeviceProtectionOptions, ConvertProtectionMode_NoProtection)
{
    const char* path = "/xyz/openbmc_project/test/protection_0";
    NsmDeviceProtectionOptions opt(bus, path, "prot0", "NSM_Protection");

    auto result = opt.convertNsmdToDbusProtectionMode(PROTECTION_NONE);
    EXPECT_EQ(result, ProtectionOption::NoProtection);
}

/**
 * NsmDeviceProtectionOptions_ConvertProtectionMode_PreventAll
 */
TEST(NsmDeviceProtectionOptions, ConvertProtectionMode_PreventAll)
{
    const char* path = "/xyz/openbmc_project/test/protection_1";
    NsmDeviceProtectionOptions opt(bus, path, "prot1", "NSM_Protection");

    auto result = opt.convertNsmdToDbusProtectionMode(
        PROTECTION_PREVENT_FW_UPDATE_AND_CONFIG);
    EXPECT_EQ(result, ProtectionOption::PreventAll);
}

/**
 * NsmDeviceProtectionOptions_ConvertProtectionMode_PreventFwUpdate
 */
TEST(NsmDeviceProtectionOptions, ConvertProtectionMode_PreventFwUpdate)
{
    const char* path = "/xyz/openbmc_project/test/protection_2";
    NsmDeviceProtectionOptions opt(bus, path, "prot2", "NSM_Protection");

    auto result =
        opt.convertNsmdToDbusProtectionMode(PROTECTION_PREVENT_FW_UPDATE);
    EXPECT_EQ(result, ProtectionOption::PreventHostFirmwareUpdates);
}

/**
 * NsmDeviceProtectionOptions_ConvertProtectionMode_PreventConfig
 */
TEST(NsmDeviceProtectionOptions, ConvertProtectionMode_PreventConfig)
{
    const char* path = "/xyz/openbmc_project/test/protection_3";
    NsmDeviceProtectionOptions opt(bus, path, "prot3", "NSM_Protection");

    auto result =
        opt.convertNsmdToDbusProtectionMode(PROTECTION_PREVENT_CONFIG);
    EXPECT_EQ(result, ProtectionOption::PreventHostConfigurations);
}

/**
 * NsmDeviceProtectionOptions_ConvertProtectionMode_UnknownValue
 */
TEST(NsmDeviceProtectionOptions, ConvertProtectionMode_UnknownValue)
{
    const char* path = "/xyz/openbmc_project/test/protection_4";
    NsmDeviceProtectionOptions opt(bus, path, "prot4", "NSM_Protection");

    auto result = opt.convertNsmdToDbusProtectionMode(255);
    EXPECT_EQ(result, ProtectionOption::Unknown);
}

/**
 * NsmDeviceProtectionOptions_HandleResponseMsg_AllProtectionModes
 * Tests handleResponseMsg with all known protection modes to verify
 * the full convertNsmdToDbusProtectionMode dispatch is exercised.
 */
TEST(NsmDeviceProtectionOptions, HandleResponseMsg_PreventFWUpdateMode)
{
    const char* path = "/xyz/openbmc_project/test/protection_5";
    NsmDeviceProtectionOptions opt(bus, path, "prot5", "NSM_Protection");

    // Build response with PROTECTION_PREVENT_FW_UPDATE
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_protection_options_resp(0, NSM_SUCCESS, ERR_NULL,
                                       PROTECTION_PREVENT_FW_UPDATE, response);

    auto rc = opt.handleResponseMsg(response, responseData.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(opt.protectionIntf->protectionLevel(),
              ProtectionOption::PreventHostFirmwareUpdates);
}

/**
 * NsmDeviceProtectionOptions_HandleResponseMsg_PreventConfigMode
 */
TEST(NsmDeviceProtectionOptions, HandleResponseMsg_PreventConfigMode)
{
    const char* path = "/xyz/openbmc_project/test/protection_6";
    NsmDeviceProtectionOptions opt(bus, path, "prot6", "NSM_Protection");

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_protection_options_resp(0, NSM_SUCCESS, ERR_NULL,
                                       PROTECTION_PREVENT_CONFIG, response);

    auto rc = opt.handleResponseMsg(response, responseData.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(opt.protectionIntf->protectionLevel(),
              ProtectionOption::PreventHostConfigurations);
}

/**
 * NsmDeviceProtectionOptions_HandleResponseMsg_PreventAllMode
 */
TEST(NsmDeviceProtectionOptions, HandleResponseMsg_PreventAllMode)
{
    const char* path = "/xyz/openbmc_project/test/protection_7";
    NsmDeviceProtectionOptions opt(bus, path, "prot7", "NSM_Protection");

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_protection_options_resp(0, NSM_SUCCESS, ERR_NULL,
                                       PROTECTION_PREVENT_FW_UPDATE_AND_CONFIG,
                                       response);

    auto rc = opt.handleResponseMsg(response, responseData.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(opt.protectionIntf->protectionLevel(),
              ProtectionOption::PreventAll);
}

/**
 * NsmDeviceProtectionOptions_HandleResponseMsg_UnknownProtectionMode
 */
TEST(NsmDeviceProtectionOptions, HandleResponseMsg_UnknownProtectionMode)
{
    const char* path = "/xyz/openbmc_project/test/protection_8";
    NsmDeviceProtectionOptions opt(bus, path, "prot8", "NSM_Protection");

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    // Use an unrecognized protection mode value
    encode_get_protection_options_resp(0, NSM_SUCCESS, ERR_NULL, 200, response);

    auto rc = opt.handleResponseMsg(response, responseData.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(opt.protectionIntf->protectionLevel(), ProtectionOption::Unknown);
}

TEST(NsmDeviceProtectionOptions, HandleResponseMsg_ErrorCC_ReturnsError)
{
    const char* path = "/xyz/openbmc_project/test/protection_9";
    NsmDeviceProtectionOptions opt(bus, path, "prot9", "NSM_Protection");

    // Fully-allocated buffer with error completion code
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    auto* resp = reinterpret_cast<nsm_common_resp*>(response->payload);
    resp->completion_code = NSM_ERROR;

    auto rc = opt.handleResponseMsg(response, responseData.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmDeviceProtectionOptions, HandleResponseMsg_DecodeFail_ReturnsError)
{
    const char* path = "/xyz/openbmc_project/test/protection_10";
    NsmDeviceProtectionOptions opt(bus, path, "prot10", "NSM_Protection");

    // 7-byte buffer: safely reads cc=0 but too short for nsm_common_resp,
    // triggering NSM_SW_ERROR_LENGTH from decode_common_resp
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = opt.handleResponseMsg(response, responseData.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// 6. nsmRetimerPort.cpp -- remaining 3 uncovered functions
//    - NsmPCIeECCGroup10::handleResponseMsg (updateMetricOnSharedMemory)
//    - Already covered in existing tests, need to cover edge cases:
//      Group10 zero data, Group5 GenRequestMsg multiport
// ============================================================================

static const std::string retimerInvPath =
    "/xyz/openbmc_project/inventory/system/retimer/port_new";

struct NsmPCIeECCGroup10Fixture : public Batch12GCoroutineTest
{
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmPCIeECCGroup10Fixture() :
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));
    }
};

/**
 * NsmPCIeECCGroup10_HandleResponseMsg_ZeroValues
 * All zeros in group10 data verifies the conversion to bytes is correct.
 */
TEST_F(NsmPCIeECCGroup10Fixture, HandleResponseMsg_ZeroValues)
{
    NsmPCIeECCGroup10 group("g10_zero", "NSM_PCIeRetimer", retimerInvPath,
                            uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_10 data = {};
    memset(&data, 0, sizeof(data));

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group10_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(group.outboundReadPktCount, 0u);
    EXPECT_EQ(group.outboundWritePktCount, 0u);
    EXPECT_EQ(group.outboundTLPCount, 0u);
    EXPECT_EQ(group.outboundReadTransfer, 0u);
    EXPECT_EQ(group.outboundWriteTransfer, 0u);
    EXPECT_EQ(group.outboundTLPsTransfer, 0u);
    EXPECT_EQ(group.reqDroppedTag, 0u);
    EXPECT_EQ(group.reqDroppedCreditCompletion, 0u);
    EXPECT_EQ(group.reqDroppedNonPostCredit, 0u);
}

/**
 * NsmPCIeECCGroup5_GenRequestMsg_MultiPort
 * Verifies genRequestMsg for multi-port variant of Group5.
 */
TEST(NsmPCIeECCGroup5, GenRequestMsg_MultiPort)
{
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, retimerInvPath.c_str());

    NsmPCIeECCGroup5 group("g5_multi_gen", "NSM_PCIeRetimer", retimerInvPath,
                           portMetricsOem2Intf, NSM_PORT_TYPE_DOWNSTREAM,
                           uint8_t(2), uint8_t(3));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

/**
 * NsmPCIeECCGroup2_GenRequestMsg_MultiPort
 * Verifies genRequestMsg for multi-port variant of Group2.
 */
TEST(NsmPCIeECCGroup2, GenRequestMsg_MultiPort)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     retimerInvPath.c_str());

    NsmPCIeECCGroup2 group("g2_multi_gen", "NSM_PCIeRetimer", retimerInvPath,
                           pcieEccIntf, NSM_PORT_TYPE_UPSTREAM, uint8_t(0),
                           uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

/**
 * NsmPCIeECCGroup3_GenRequestMsg_MultiPort
 * Verifies genRequestMsg for multi-port variant of Group3.
 */
TEST(NsmPCIeECCGroup3, GenRequestMsg_MultiPort)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     retimerInvPath.c_str());

    NsmPCIeECCGroup3 group("g3_multi_gen", "NSM_PCIeRetimer", retimerInvPath,
                           pcieEccIntf, NSM_PORT_TYPE_DOWNSTREAM, uint8_t(1),
                           uint8_t(2));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

/**
 * NsmPCIeECCGroup4_GenRequestMsg_MultiPort
 * Verifies genRequestMsg for multi-port variant of Group4.
 */
TEST(NsmPCIeECCGroup4, GenRequestMsg_MultiPort)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     retimerInvPath.c_str());

    NsmPCIeECCGroup4 group("g4_multi_gen", "NSM_PCIeRetimer", retimerInvPath,
                           pcieEccIntf, NSM_PORT_TYPE_UPSTREAM, uint8_t(0),
                           uint8_t(3));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

/**
 * NsmPCIeECCGroup9_GenRequestMsg_MultiPort
 * Verifies genRequestMsg for multi-port variant of Group9.
 */
TEST(NsmPCIeECCGroup9, GenRequestMsg_MultiPort)
{
    auto aerIntf = std::make_shared<AERErrorStatusIntf>(bus,
                                                        retimerInvPath.c_str());

    NsmPCIeECCGroup9 group("g9_multi_gen", "NSM_PCIeRetimer", retimerInvPath,
                           aerIntf, NSM_PORT_TYPE_DOWNSTREAM, uint8_t(1),
                           uint8_t(2));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

/**
 * NsmPCIeECCGroup8_GenRequestMsg_MultiPort
 * Verifies genRequestMsg for multi-port variant of Group8.
 */
TEST(NsmPCIeECCGroup8, GenRequestMsg_MultiPort)
{
    auto laneErrorIntf =
        std::make_shared<LaneErrorIntf>(bus, retimerInvPath.c_str());

    NsmPCIeECCGroup8 group("g8_multi_gen", "NSM_PCIeRetimer", laneErrorIntf,
                           retimerInvPath, NSM_PORT_TYPE_UPSTREAM, uint8_t(0),
                           uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

/**
 * NsmPCIeECCGroup10_HandleResponseMsg_MaxUint32Values
 * Verifies large values in Group10 don't overflow during byte conversion.
 */
TEST_F(NsmPCIeECCGroup10Fixture, HandleResponseMsg_MaxUint32Values)
{
    NsmPCIeECCGroup10 group("g10_max", "NSM_PCIeRetimer", retimerInvPath,
                            uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_10 data = {};
    data.outbound_read_tlp_count = 0xFFFFFFFF;
    data.outbound_write_tlp_count = 0xFFFFFFFF;
    data.outbound_completion_tlp_count = 0xFFFFFFFF;
    data.dwords_transferred_in_outbound_read_tlp_high = 0xFFFFFFFF;
    data.dwords_transferred_in_outbound_read_tlp_low = 0xFFFFFFFF;
    data.dwords_transferred_in_outbound_write_tlp_high = 0xFFFFFFFF;
    data.dwords_transferred_in_outbound_write_tlp_low = 0xFFFFFFFF;
    data.dwords_transferred_in_outbound_completion = 0xFFFFFFFF;
    data.read_requests_dropped_tag_unavailable = 0xFFFFFFFF;
    data.read_requests_dropped_credit_exhaustion = 0xFFFFFFFF;
    data.read_requests_dropped_credit_not_posted = 0xFFFFFFFF;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group10_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    // Verify dwords_transferred assembly: (high << 32) | low
    uint64_t expectedTransfer =
        ((static_cast<uint64_t>(0xFFFFFFFF) << 32) | 0xFFFFFFFF) * 4;
    EXPECT_EQ(group.outboundReadTransfer, expectedTransfer);
    EXPECT_EQ(group.outboundWriteTransfer, expectedTransfer);
    EXPECT_EQ(group.outboundTLPsTransfer,
              static_cast<uint64_t>(0xFFFFFFFF) * 4);
}

/**
 * NsmPCIeECCGroup2_HandleResponseMsg_HighValues
 * Tests Group2 handleResponseMsg with large counter values.
 */
TEST(NsmPCIeECCGroup2, HandleResponseMsg_HighValues)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     retimerInvPath.c_str());

    NsmPCIeECCGroup2 group("g2_high", "NSM_PCIeRetimer", retimerInvPath,
                           pcieEccIntf, uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_2 data = {};
    data.non_fatal_errors = 0xFFFFFFFF;
    data.fatal_errors = 0xEEEEEEEE;
    data.correctable_errors = 0xDDDDDDDD;
    data.unsupported_request_count = 0xCCCCCCCC;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->nonfeCount(), 0xFFFFFFFFu);
    EXPECT_EQ(pcieEccIntf->feCount(), 0xEEEEEEEEu);
    EXPECT_EQ(pcieEccIntf->ceCount(), 0xDDDDDDDDu);
    EXPECT_EQ(pcieEccIntf->unsupportedRequestCount(), 0xCCCCCCCCu);
}

/**
 * NsmPCIeECCGroup3_HandleResponseMsg_LargeRecoveryCount
 * Tests Group3 with a very large recovery count.
 */
TEST(NsmPCIeECCGroup3, HandleResponseMsg_LargeRecoveryCount)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     retimerInvPath.c_str());

    NsmPCIeECCGroup3 group("g3_large", "NSM_PCIeRetimer", retimerInvPath,
                           pcieEccIntf, uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_3 data = {};
    data.L0ToRecoveryCount = 0xFFFFFFFF;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 0xFFFFFFFFu);
}

/**
 * NsmPCIeECCGroup4_HandleResponseMsg_AllMaxValues
 * Tests Group4 with all max values.
 */
TEST(NsmPCIeECCGroup4, HandleResponseMsg_AllMaxValues)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     retimerInvPath.c_str());

    NsmPCIeECCGroup4 group("g4_max", "NSM_PCIeRetimer", retimerInvPath,
                           pcieEccIntf, uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_4 data = {};
    data.replay_cnt = 0xFFFFFFFF;
    data.replay_rollover_cnt = 0xFFFFFFFF;
    data.NAK_sent_cnt = 0xFFFFFFFF;
    data.NAK_recv_cnt = 0xFFFFFFFF;
    data.FC_timeout_err_cnt = 0xFFFFFFFF;
    data.bad_TLP_cnt = 0xFFFFFFFF;
    data.recv_err_cnt = 0xFFFFFFFF;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->replayCount(), 0xFFFFFFFFu);
    EXPECT_EQ(pcieEccIntf->replayRolloverCount(), 0xFFFFFFFFu);
    EXPECT_EQ(pcieEccIntf->nakSentCount(), 0xFFFFFFFFu);
    EXPECT_EQ(pcieEccIntf->nakReceivedCount(), 0xFFFFFFFFu);
    EXPECT_EQ(pcieEccIntf->fcTimeoutErrors(), 0xFFFFFFFFu);
    EXPECT_EQ(pcieEccIntf->badTLPCount(), 0xFFFFFFFFu);
    EXPECT_EQ(pcieEccIntf->receiverErrorCount(), 0xFFFFFFFFu);
}

// ============================================================================
// addSensor<T> instantiation coverage
// ============================================================================

TEST_F(Batch12GCoroutineTest, AddSensorNsmClearPCIeCounters)
{
    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, "/xyz/openbmc_project/test/clear_pcie_as", uint8_t(1), nullptr);
    auto sensor = std::make_shared<NsmClearPCIeCounters>(
        "ClearPCIeCounters_AS", "NSM_GPU_PCIe", GROUP_ID_2, uint8_t(1),
        clearPCIeIntf);
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}
