/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for nsmd/nsmSetAsync/nsmSetEgmMode.cpp
 *
 * Covers:
 *  - setEgmModeOnDevice: decode_set_EGM_mode_resp fails (rc != 0, cc == 0)
 *    → enters the else branch → WriteFailure
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "device-configuration.h"

#include "nsmSetEgmMode.hpp"

using namespace nsm;

// Forward-declare the internal free function so it can be called directly
namespace nsm
{
requester::Coroutine setEgmModeOnDevice(bool egmMode,
                                        AsyncOperationStatusType* status,
                                        std::shared_ptr<NsmDevice> device);
} // namespace nsm

// ============================================================================
// Test fixture
// ============================================================================

struct NsmSetEgmModeBranchTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmSetEgmModeBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmSetEgmModeBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    /**
     * Build a response buffer where completion_code is NSM_SUCCESS (0)
     * but the buffer is too short for a full nsm_common_resp.
     * decode_set_EGM_mode_resp (→ decode_common_resp) will succeed at
     * decode_reason_code_and_cc (cc=0, returns 0) but then fail the
     * length check, returning NSM_SW_ERROR_LENGTH with cc still 0.
     *
     * This triggers the else branch:
     *   if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS) → FALSE
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
// setEgmModeOnDevice: decode fails (rc != 0, cc == 0) → else → WriteFailure
// Covers the "rc == NSM_SW_SUCCESS" FALSE sub-branch of the condition
// ============================================================================

TEST_F(NsmSetEgmModeBranchTest,
       SetEgmModeOnDevice_DecodeFailCcZero_SetsWriteFailure)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDecodeFailResp()));

    nsm::setEgmModeOnDevice(true, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
