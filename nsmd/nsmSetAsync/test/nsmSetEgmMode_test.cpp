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
 * Branch coverage tests for nsmd/nsmSetAsync/nsmSetEgmMode.cpp
 *
 * Covers:
 *  - setEgmModeEnabled: non-bool variant value → throws InvalidArgument
 *  - setEgmModeOnDevice: postPatchIO transport failure → WriteFailure
 *  - setEgmModeOnDevice: error CC in response → WriteFailure
 *  - setEgmModeOnDevice: success path → Success
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

struct NsmSetEgmModeTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmSetEgmModeTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmSetEgmModeTest()
    {
        cleanupDeviceSensors(devices);
    }

    static std::vector<uint8_t> makeSuccessResp()
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_set_EGM_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);
        return buf;
    }

    static std::vector<uint8_t> makeErrorResp()
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_set_EGM_mode_resp(0, NSM_ERROR, 0x1234, msg);
        return buf;
    }
};

// ============================================================================
// setEgmModeEnabled: non-bool value → if (!egmMode) TRUE → throws
// ============================================================================

TEST_F(NsmSetEgmModeTest, SetEgmModeEnabled_NonBoolValue_ThrowsInvalidArgument)
{
    AsyncSetOperationValueType value = std::string("bad");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_THROW_COROUTINE(
        setEgmModeEnabled(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setEgmModeOnDevice via setEgmModeEnabled: postPatchIO transport failure
// → if (rc_) TRUE → status = WriteFailure, returns NSM_SW_ERROR_COMMAND_FAIL
// ============================================================================

TEST_F(NsmSetEgmModeTest, SetEgmModeOnDevice_PostPatchIOFails_SetsWriteFailure)
{
    AsyncSetOperationValueType value = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSuccessResp(), NSM_ERROR));

    setEgmModeEnabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setEgmModeOnDevice: error CC in response
// → if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS) FALSE → else branch
// → status = WriteFailure
// ============================================================================

TEST_F(NsmSetEgmModeTest, SetEgmModeOnDevice_ErrorCC_SetsWriteFailure)
{
    AsyncSetOperationValueType value = false;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeErrorResp()));

    setEgmModeEnabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setEgmModeOnDevice: success path
// → if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS) TRUE → status unchanged
// ============================================================================

TEST_F(NsmSetEgmModeTest, SetEgmModeOnDevice_Success_StatusUnchanged)
{
    AsyncSetOperationValueType value = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSuccessResp()));

    setEgmModeEnabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// setEgmModeOnDevice called directly: egmMode=false (tests both bool paths)
// ============================================================================

TEST_F(NsmSetEgmModeTest, SetEgmModeOnDevice_EgmModeFalse_Success)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSuccessResp()));

    nsm::setEgmModeOnDevice(false, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}
