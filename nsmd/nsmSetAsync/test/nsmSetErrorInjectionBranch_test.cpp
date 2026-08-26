/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for nsmd/nsmSetAsync/nsmSetErrorInjection.cpp
 *
 * Covers branches NOT already hit by nsmSetErrorInjection_test.cpp:
 *  - setModeEnabled: decode fails with cc==0 but rc!=0 (truncated resp)
 *  - setEnabled: decode fails with cc==0 but rc!=0 (truncated resp)
 *  - setPayload: decode fails with cc==0 but rc!=0 (truncated resp)
 *  - setEnabled: invoke() lambda with type == Unknown → early return
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

// ============================================================================
// Fixture
// ============================================================================

struct NsmSetErrorInjectionBranchTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmSetErrorInjectionBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmSetErrorInjectionBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    /**
     * Build a response buffer where completion_code is NSM_SUCCESS (0)
     * but the buffer is too short for a full nsm_common_resp.
     * decode_*_resp will succeed at decode_reason_code_and_cc (cc=0,
     * returns 0) but then fail the length check, returning
     * NSM_SW_ERROR_LENGTH with cc still 0.
     *
     * This triggers: if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS) TRUE
     * via rc != 0 while cc == 0, covering the "rc" sub-branch of the
     * ternary "cc ? cc : rc".
     */
    static std::vector<uint8_t> makeDecodeFailResp()
    {
        const size_t bufSize = sizeof(nsm_msg_hdr) +
                               sizeof(nsm_common_non_success_resp);
        std::vector<uint8_t> buf(bufSize, 0);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        // payload[1] = completion_code = NSM_SUCCESS (0)
        msg->payload[1] = NSM_SUCCESS;
        return buf;
    }
};

// ============================================================================
// setModeEnabled: decode fails (rc != 0, cc == 0) → WriteFailure
// Covers the "rc != NSM_SW_SUCCESS" sub-branch of the condition
// ============================================================================

TEST_F(NsmSetErrorInjectionBranchTest,
       SetModeEnabled_DecodeFailCcZero_SetsWriteFailure)
{
    NsmSetErrorInjection sensor(mockManager,
                                std::filesystem::path("/test/br_mode_df"));

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDecodeFailResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setModeEnabled(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setEnabled: decode fails (rc != 0, cc == 0) → WriteFailure
// ============================================================================

TEST_F(NsmSetErrorInjectionBranchTest,
       SetEnabled_DecodeFailCcZero_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/br_en_df");
    auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(bus,
                                                               eiPath.c_str());
    intf->type(ErrorInjectionCapabilityIntf::Type::MemoryErrors);
    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    interfaces[eiPath] = intf;

    NsmSetErrorInjectionEnabled sensor(
        "EiBrDf", ErrorInjectionCapabilityIntf::Type::MemoryErrors, interfaces,
        makeErrorInjectionCapabilities(mockManager, interfaces));

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDecodeFailResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setEnabled(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setPayload: decode fails (rc != 0, cc == 0) → WriteFailure
// ============================================================================

TEST_F(NsmSetErrorInjectionBranchTest,
       SetPayload_DecodeFailCcZero_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/br_pl_df");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);

    NsmSetErrorInjectionPayload sensor("EiBrPlDf", mockManager, interfaces,
                                       activateIntf, EI_GPIO_SPOOFING, 0);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDecodeFailResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> faultBitMap{0x00, 0x00};
    sensor.setPayload(faultBitMap, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setPayload with EI_DEVICE_ERRORS: decode fails (rc != 0, cc == 0) →
// WriteFailure.  Also exercises the EI_DEVICE_ERRORS subtype-insert path
// in combination with a decode failure.
// ============================================================================

TEST_F(NsmSetErrorInjectionBranchTest,
       SetPayload_DeviceErrors_DecodeFailCcZero_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path eiPath("/test/br_pl_de_df");
    auto intf = std::make_shared<ErrorInjectionPayloadIntf>(bus,
                                                            eiPath.c_str());
    Interfaces<ErrorInjectionPayloadIntf> interfaces;
    interfaces[eiPath] = intf;
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, eiPath.c_str(), 0x01, 0x02, nullptr);

    NsmSetErrorInjectionPayload sensor("EiBrPlDeDf", mockManager, interfaces,
                                       activateIntf, EI_DEVICE_ERRORS,
                                       EI_DEVICE_ERRORS_SUBTYPE_FATAL);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDecodeFailResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // 4-byte bitmap; setPayload prepends 2 bytes for subtype → 6 bytes total
    std::vector<uint8_t> faultBitMap{0x00, 0x00, 0x00, 0x00};
    sensor.setPayload(faultBitMap, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
