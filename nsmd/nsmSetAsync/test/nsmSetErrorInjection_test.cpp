/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
 * Tests for nsmd/nsmSetAsync/nsmSetErrorInjection.cpp
 *
 *   - NsmSetErrorInjection constructor (name, type)
 *   - NsmSetErrorInjectionEnabled constructor (valid type)
 *   - NsmSetErrorInjectionEnabled constructor (Unknown type → throws)
 *   - NsmSetErrorInjectionPayload constructor
 *   - errorInjectionModeEnabled with non-bool value (throws InvalidArgument)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"

#include "nsmSetErrorInjection.hpp"

using namespace nsm;

namespace
{
/** Builds the mask owner that NsmSetErrorInjectionEnabled delegates its
 *  read-modify-write to. */
inline std::shared_ptr<NsmSetErrorInjectionCapabilities>
    makeErrorInjectionCapabilities(
        SensorManager& manager,
        const Interfaces<ErrorInjectionCapabilityIntf>& interfaces)
{
    // The mask owner reuses this sensor's update() as its post-write read-back.
    auto enabledSensor = std::make_shared<NsmErrorInjectionEnabled>(
        NsmInterfaceProvider<ErrorInjectionCapabilityIntf>(
            "ErrorInjectionCapability", "NSM_ErrorInjectionCapability",
            interfaces));
    return std::make_shared<NsmSetErrorInjectionCapabilities>(
        "ErrorInjectionCapabilities", manager, interfaces, enabledSensor);
}
} // namespace

static auto& testBus = utils::DBusHandler::getBus();

// =============================================================================
// NsmSetErrorInjection constructor test
// =============================================================================

TEST(NsmSetErrorInjection, Constructor_InitializesNameAndType)
{
    NsmDeviceTable devices;
    SensorManagerTest smTest(devices);

    std::filesystem::path objPath("/test/ei/mode");
    NsmSetErrorInjection sensor(smTest.mockManager, objPath);

    EXPECT_EQ(sensor.getName(), "ErrorInjection");
    EXPECT_EQ(sensor.getType(), "NSM_ErrorInjection");
}

// =============================================================================
// NsmSetErrorInjectionEnabled constructor tests
// =============================================================================

TEST(NsmSetErrorInjectionEnabled, Constructor_ValidType_Succeeds)
{
    NsmDeviceTable devices;
    SensorManagerTest smTest(devices);

    std::filesystem::path objPath("/test/ei/cap");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(testBus,
                                                               objPath.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::FatalErrors);

    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[objPath] = intf;

    EXPECT_NO_THROW(NsmSetErrorInjectionEnabled sensor(
        "ErrorInjectionCap", ErrorInjectionCapabilityIntf::Type::FatalErrors,
        interfaces,
        makeErrorInjectionCapabilities(smTest.mockManager, interfaces)));
}

TEST(NsmSetErrorInjectionEnabled, Constructor_UnknownType_Throws)
{
    NsmDeviceTable devices;
    SensorManagerTest smTest(devices);

    std::filesystem::path objPath("/test/ei/cap_unknown");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(testBus,
                                                               objPath.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::MemoryErrors);

    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[objPath] = intf;

    EXPECT_THROW(
        NsmSetErrorInjectionEnabled sensor(
            "ErrorInjectionCap", ErrorInjectionCapabilityIntf::Type::Unknown,
            interfaces,
            makeErrorInjectionCapabilities(smTest.mockManager, interfaces)),
        std::invalid_argument);
}

TEST(NsmSetErrorInjectionEnabled, Constructor_NameAndType)
{
    NsmDeviceTable devices;
    SensorManagerTest smTest(devices);

    std::filesystem::path objPath("/test/ei/cap2");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(testBus,
                                                               objPath.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::MemoryErrors);

    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[objPath] = intf;

    NsmSetErrorInjectionEnabled sensor(
        "EiCapSensor", ErrorInjectionCapabilityIntf::Type::MemoryErrors,
        interfaces,
        makeErrorInjectionCapabilities(smTest.mockManager, interfaces));

    EXPECT_EQ(sensor.getName(), "EiCapSensor");
    EXPECT_EQ(sensor.getType(), "NSM_ErrorInjectionCapability");
}

// =============================================================================
// NsmSetErrorInjectionPayload constructor test
// =============================================================================

TEST(NsmSetErrorInjectionPayload, Constructor_InitializesFields)
{
    NsmDeviceTable devices;
    SensorManagerTest smTest(devices);

    std::filesystem::path objPath("/test/ei/payload");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(testBus,
                                                            objPath.c_str());

    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[objPath] = intf;

    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        testBus, objPath.c_str(), 0x01, 0x02, nullptr);

    NsmSetErrorInjectionPayload sensor("EiPayload", smTest.mockManager,
                                       interfaces, activateIntf, 0x01, 0x02);

    EXPECT_EQ(sensor.getName(), "EiPayload");
    EXPECT_EQ(sensor.getType(), "NSM_ErrorInjectionPayload");
    EXPECT_EQ(sensor.errorInjectionType, 0x01);
    EXPECT_EQ(sensor.errorInjectionSubtype, 0x02);
}

// =============================================================================
// Coroutine tests: cover both branches of co_return cc ? cc : rc for //
// setModeEnabled, setEnabled and setPayload.
//
// cc is initialized to NSM_ERROR (non-zero) in all three coroutines:
//   FALSE branch (cc==0): covered by success-response test (decode sets cc=0)
//   TRUE  branch (cc!=0): covered by error-CC-response test (decode reads
//                          NSM_ERROR from the encoded response)
// =============================================================================

struct NsmSetErrorInjectionCoroutineTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmSetErrorInjectionCoroutineTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        ASSERT_NE(gpu, nullptr);
    }

    ~NsmSetErrorInjectionCoroutineTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// setModeEnabled – success response → FALSE branch of co_return cc ? cc : rc //

TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetModeEnabled_Success_CoversFalseBranch)
{
    NsmSetErrorInjection sensor(mockManager,
                                std::filesystem::path("/test/mode_succ"));

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(
        encode_set_error_injection_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL, msg),
        NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setModeEnabled(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setModeEnabled – error-CC response → TRUE branch of co_return cc ? cc : rc //

TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetModeEnabled_ErrorCC_CoversTrueBranch)
{
    NsmSetErrorInjection sensor(mockManager,
                                std::filesystem::path("/test/mode_err"));

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(
        encode_set_error_injection_mode_v1_resp(0, NSM_ERROR, ERR_NULL, msg),
        NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setModeEnabled(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setEnabled – success response → FALSE branch of co_return cc ? cc : rc //

TEST_F(NsmSetErrorInjectionCoroutineTest, SetEnabled_Success_CoversFalseBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_enabled_succ");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               eiPath.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::MemoryErrors);
    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[eiPath] = intf;

    NsmSetErrorInjectionEnabled sensor(
        "EiEnabled", ErrorInjectionCapabilityIntf::Type::MemoryErrors,
        interfaces, makeErrorInjectionCapabilities(mockManager, interfaces));

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(encode_set_current_error_injection_types_v1_resp(0, NSM_SUCCESS,
                                                               ERR_NULL, msg),
              NSM_SW_SUCCESS);
    // The write is followed by the in-lock read-back, which reuses the polling
    // sensor's update() and therefore goes through sensorIO, not postPatchIO.
    std::vector<uint8_t> readBackResp(256, 0);
    auto readBackMsg = reinterpret_cast<nsm_msg*>(readBackResp.data());
    nsm_error_injection_types_mask readBackMask{};
    ASSERT_EQ(encode_get_current_error_injection_types_v1_resp(
                  0, NSM_SUCCESS, ERR_NULL, &readBackMask, readBackMsg),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(readBackResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setEnabled(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setEnabled – error-CC response → TRUE branch of co_return cc ? cc : rc //

TEST_F(NsmSetErrorInjectionCoroutineTest, SetEnabled_ErrorCC_CoversTrueBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_enabled_err");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               eiPath.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::MemoryErrors);
    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[eiPath] = intf;

    NsmSetErrorInjectionEnabled sensor(
        "EiEnabled", ErrorInjectionCapabilityIntf::Type::MemoryErrors,
        interfaces, makeErrorInjectionCapabilities(mockManager, interfaces));

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(encode_set_current_error_injection_types_v1_resp(0, NSM_ERROR,
                                                               ERR_NULL, msg),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setEnabled(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setPayload – success response → FALSE branch of co_return cc ? cc : rc //

TEST_F(NsmSetErrorInjectionCoroutineTest, SetPayload_Success_CoversFalseBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_payload_succ");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);

    NsmSetErrorInjectionPayload sensor("EiPayloadTest", mockManager, interfaces,
                                       activateIntf, EI_GPIO_SPOOFING, 0);

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(
        encode_set_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL, msg),
        NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> faultBitMap(2,
                                     0); // count_of_gpio=0 for EI_GPIO_SPOOFING
    sensor.setPayload(faultBitMap, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setPayload – error-CC response → TRUE branch of co_return cc ? cc : rc //

TEST_F(NsmSetErrorInjectionCoroutineTest, SetPayload_ErrorCC_CoversTrueBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_payload_err");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);

    NsmSetErrorInjectionPayload sensor("EiPayloadErrTest", mockManager,
                                       interfaces, activateIntf,
                                       EI_GPIO_SPOOFING, 0);

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(
        encode_set_error_injection_payload_resp(0, NSM_ERROR, ERR_NULL, msg),
        NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> faultBitMap(2,
                                     0); // count_of_gpio=0 for EI_GPIO_SPOOFING
    sensor.setPayload(faultBitMap, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// errorInjectionModeEnabled – valid bool → calls setModeEnabled (lines 36-47)
// =============================================================================

// errorInjectionModeEnabled: value is bool → !enabledValue FALSE →
// setModeEnabled
TEST_F(NsmSetErrorInjectionCoroutineTest,
       ErrorInjectionModeEnabled_ValidBool_CallsSetModeEnabled)
{
    NsmSetErrorInjection sensor(mockManager,
                                std::filesystem::path("/test/mode_via_emi"));

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(
        encode_set_error_injection_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL, msg),
        NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};
    sensor.errorInjectionModeEnabled(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// errorInjectionModeEnabled: value is not bool → throws InvalidArgument
// Only runs in coverage build: in real-coroutine mode the throw is stored in //
// the coroutine's promise and not propagated to the caller.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmSetErrorInjectionCoroutineTest,
       ErrorInjectionModeEnabled_NonBool_ThrowsInvalidArgument)
{
    NsmSetErrorInjection sensor(mockManager,
                                std::filesystem::path("/test/mode_nonbool"));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{42};
    EXPECT_THROW(
        sensor.errorInjectionModeEnabled(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}
#endif // COVERAGE_DISABLE_COROUTINES

// =============================================================================
// enabled – valid bool → calls setEnabled
// =============================================================================

TEST_F(NsmSetErrorInjectionCoroutineTest, Enabled_ValidBool_CallsSetEnabled)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_en_via_enabled");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               eiPath.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::MemoryErrors);
    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[eiPath] = intf;

    NsmSetErrorInjectionEnabled sensor(
        "EiEnabledViaEnabled", ErrorInjectionCapabilityIntf::Type::MemoryErrors,
        interfaces, makeErrorInjectionCapabilities(mockManager, interfaces));

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(encode_set_current_error_injection_types_v1_resp(0, NSM_SUCCESS,
                                                               ERR_NULL, msg),
              NSM_SW_SUCCESS);
    // The write is followed by the in-lock read-back, which reuses the polling
    // sensor's update() and therefore goes through sensorIO, not postPatchIO.
    std::vector<uint8_t> readBackResp(256, 0);
    auto readBackMsg = reinterpret_cast<nsm_msg*>(readBackResp.data());
    nsm_error_injection_types_mask readBackMask{};
    ASSERT_EQ(encode_get_current_error_injection_types_v1_resp(
                  0, NSM_SUCCESS, ERR_NULL, &readBackMask, readBackMsg),
              NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(readBackResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};
    sensor.enabled(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// enabled: value is not bool → throws InvalidArgument
// Only runs in coverage build: same reason as ErrorInjectionModeEnabled above.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmSetErrorInjectionCoroutineTest, Enabled_NonBool_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_en_nonbool");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               eiPath.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::MemoryErrors);
    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[eiPath] = intf;

    NsmSetErrorInjectionEnabled sensor(
        "EiEnabledNonBool", ErrorInjectionCapabilityIntf::Type::MemoryErrors,
        interfaces, makeErrorInjectionCapabilities(mockManager, interfaces));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{42};
    EXPECT_THROW(
        sensor.enabled(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}
#endif // COVERAGE_DISABLE_COROUTINES

// =============================================================================
// setModeEnabled – postPatchIO failure branches
// =============================================================================

// postPatchIO fails with non-unsupported code → inner if (rc != NSM_ERR...)
// TRUE
TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetModeEnabled_PostPatchIOFails_NonUnsupported_WritesFailure)
{
    NsmSetErrorInjection sensor(mockManager,
                                std::filesystem::path("/test/mode_sio_fail"));

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setModeEnabled(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// postPatchIO fails with NSM_ERR_UNSUPPORTED_COMMAND_CODE → inner if FALSE
TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetModeEnabled_PostPatchIOUnsupported_WritesFailure)
{
    NsmSetErrorInjection sensor(
        mockManager, std::filesystem::path("/test/mode_unsupported"));

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setModeEnabled(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// setEnabled – postPatchIO failure branches
// =============================================================================

TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetEnabled_PostPatchIOFails_NonUnsupported_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_sio_fail");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               eiPath.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::MemoryErrors);
    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[eiPath] = intf;

    NsmSetErrorInjectionEnabled sensor(
        "EiSioFail", ErrorInjectionCapabilityIntf::Type::MemoryErrors,
        interfaces, makeErrorInjectionCapabilities(mockManager, interfaces));

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setEnabled(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetEnabled_PostPatchIOUnsupported_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_unsup");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               eiPath.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::MemoryErrors);
    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[eiPath] = intf;

    NsmSetErrorInjectionEnabled sensor(
        "EiUnsupported", ErrorInjectionCapabilityIntf::Type::MemoryErrors,
        interfaces, makeErrorInjectionCapabilities(mockManager, interfaces));

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setEnabled(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// setErrorInjectionPayload – branch coverage
// =============================================================================

// Non-matching variant type → payloadValue == nullptr → throws
// Only runs in coverage build: same reason as ErrorInjectionModeEnabled above.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetErrorInjectionPayload_NonMatchingType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_pl_nontype");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);
    NsmSetErrorInjectionPayload sensor("EiPlNonType", mockManager, interfaces,
                                       activateIntf, EI_GPIO_SPOOFING, 0);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        uint32_t{42}; // not the vector-of-tuples type
    EXPECT_THROW(
        sensor.setErrorInjectionPayload(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}
#endif // COVERAGE_DISABLE_COROUTINES

// Empty payload vector → throws
// Only runs in coverage build: same reason as ErrorInjectionModeEnabled above.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetErrorInjectionPayload_EmptyPayload_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_pl_empty");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);
    NsmSetErrorInjectionPayload sensor("EiPlEmpty", mockManager, interfaces,
                                       activateIntf, EI_GPIO_SPOOFING, 0);

    using TupleVec = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = TupleVec{};
    EXPECT_THROW(
        sensor.setErrorInjectionPayload(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}
#endif // COVERAGE_DISABLE_COROUTINES

// Unrecognized key → throws
// Only runs in coverage build: same reason as ErrorInjectionModeEnabled above.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetErrorInjectionPayload_UnrecognizedKey_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_pl_badkey");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);
    NsmSetErrorInjectionPayload sensor("EiPlBadKey", mockManager, interfaces,
                                       activateIntf, EI_GPIO_SPOOFING, 0);

    using TupleVec = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    TupleVec payload;
    payload.emplace_back("NotFaultBitMap", std::vector<uint8_t>{0x01, 0x02});
    AsyncSetOperationValueType value = payload;
    EXPECT_THROW(
        sensor.setErrorInjectionPayload(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}
#endif // COVERAGE_DISABLE_COROUTINES

// FaultBitMap key but non-vector<uint8_t> value → throws
// Only runs in coverage build: same reason as ErrorInjectionModeEnabled above.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetErrorInjectionPayload_FaultBitMapNonVectorType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_pl_badval");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);
    NsmSetErrorInjectionPayload sensor("EiPlBadVal", mockManager, interfaces,
                                       activateIntf, EI_GPIO_SPOOFING, 0);

    using TupleVec = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    TupleVec payload;
    payload.emplace_back("FaultBitMap", uint32_t{42}); // wrong type
    AsyncSetOperationValueType value = payload;
    EXPECT_THROW(
        sensor.setErrorInjectionPayload(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}
#endif // COVERAGE_DISABLE_COROUTINES

// =============================================================================
// setPayload – EI_DEVICE_ERRORS type inserts subtype bytes
// =============================================================================

TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetPayload_EI_DEVICE_ERRORS_InsertsSubtype)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_devtype");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);

    NsmSetErrorInjectionPayload sensor("EiDevType", mockManager, interfaces,
                                       activateIntf, EI_DEVICE_ERRORS,
                                       EI_DEVICE_ERRORS_SUBTYPE_FATAL);

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(
        encode_set_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL, msg),
        NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // 4 bytes for payload_bitmap; setPayload prepends 2 bytes for subtype
    // → 6 bytes total = sizeof(nsm_error_injection_fatal_payload)
    std::vector<uint8_t> faultBitMap{0x00, 0x00, 0x00, 0x00};
    sensor.setPayload(faultBitMap, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setErrorInjectionPayload – valid FaultBitMap vector<uint8_t> → success path
// Covers lines 240 (FALSE branch of !faultBitMapValue), 247 (assignment), 258
// (co_return co_await setPayload)
TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetErrorInjectionPayload_ValidFaultBitMap_CallsSetPayload)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_pl_valid");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);
    NsmSetErrorInjectionPayload sensor("EiPlValid", mockManager, interfaces,
                                       activateIntf, EI_GPIO_SPOOFING, 0);

    std::vector<uint8_t> resp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    ASSERT_EQ(
        encode_set_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL, msg),
        NSM_SW_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    using TupleVec = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    TupleVec payload;
    // EI_GPIO_SPOOFING expects 2-byte bitmap: use zeros to pass encode
    payload.emplace_back("FaultBitMap", std::vector<uint8_t>{0x00, 0x00});
    AsyncSetOperationValueType value = payload;

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setErrorInjectionPayload(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setPayload – postPatchIO fails → if (rc) TRUE → WriteFailure
TEST_F(NsmSetErrorInjectionCoroutineTest,
       SetPayload_PostPatchIOFails_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/ei_sio_pl_fail");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);

    NsmSetErrorInjectionPayload sensor("EiPayloadSioFail", mockManager,
                                       interfaces, activateIntf,
                                       EI_GPIO_SPOOFING, 0);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> faultBitMap{0x00, 0x00};
    sensor.setPayload(faultBitMap, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
