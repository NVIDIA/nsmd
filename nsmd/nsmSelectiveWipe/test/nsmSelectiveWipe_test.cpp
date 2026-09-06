/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "test/mockDBusHandler.hpp"

#define private public
#define protected public

#include "diagnostics.h"

#include "nsmSelectiveWipe.hpp"
#include "test/mockSensorManager.hpp"

#include <xyz/openbmc_project/Common/error.hpp>

using namespace nsm;
using namespace ::testing;

NsmDeviceTable devices;
std::shared_ptr<MockNsmDevice> mockDevice;

class NsmSelectiveWipeTest : public Test, public SensorManagerTest
{
  protected:
    const uuid_t testUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::unique_ptr<NsmSelectiveWipeObject> wipeObject;

    NsmSelectiveWipeTest() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(testUuid));
        wipeObject = std::make_unique<NsmSelectiveWipeObject>(
            "SelectiveWipe", "NSM_SelectiveWipe", testUuid);
    }

    Response successResponse()
    {
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_selective_data_wipe_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_selective_data_wipe_resp(0, NSM_SUCCESS, ERR_NULL,
                                                  msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response errorResponse(uint8_t cc, uint16_t reasonCode = 0x1234)
    {
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_selective_data_wipe_resp(0, cc, reasonCode, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    auto runWipeOnDevice(uint16_t mask)
    {
        wipeObject->wipeStatus = WipeOperationStatus::InProgress;
        auto rc = wipeObject->wipeOnDevice(mask).await_resume();
        return std::make_tuple(rc, wipeObject->wipeStatus);
    }
};

TEST_F(NsmSelectiveWipeTest, WipeRejectsInProgress)
{
    wipeObject->wipeStatus = WipeOperationStatus::InProgress;
    EXPECT_THROW(wipeObject->wipe({WipeDataClass::ScratchData}),
                 sdbusplus::error::xyz::openbmc_project::common::Unavailable);
}

TEST_F(NsmSelectiveWipeTest, WipeRejectsEmptyTargets)
{
    EXPECT_THROW(
        wipeObject->wipe({}),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmSelectiveWipeTest, GoodWipeLogicalInvalidation)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(successResponse()));

    const auto [rc, status] = runWipeOnDevice(
        NSM_SELECTIVE_DATA_WIPE_TARGET_BIT(NSM_WIPE_TARGET_SCRATCH_DATA) |
        NSM_SELECTIVE_DATA_WIPE_TARGET_BIT(NSM_WIPE_TARGET_USER_DATA));
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(status, WipeOperationStatus::Success);
}

TEST_F(NsmSelectiveWipeTest, UnsupportedCommandMapsToUnsupportedRequest)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(successResponse(),
                                  NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    const auto [rc, status] = runWipeOnDevice(
        NSM_SELECTIVE_DATA_WIPE_TARGET_BIT(NSM_WIPE_TARGET_LOG_DATA));
    EXPECT_EQ(rc, NSM_ERR_UNSUPPORTED_COMMAND_CODE);
    EXPECT_EQ(status, WipeOperationStatus::UnsupportedRequest);
}

TEST_F(NsmSelectiveWipeTest, PostPatchIoFailureIsWriteFailure)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(successResponse(), NSM_ERROR));

    const auto [rc, status] = runWipeOnDevice(
        NSM_SELECTIVE_DATA_WIPE_TARGET_BIT(NSM_WIPE_TARGET_DIAGNOSTIC_DATA));
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(status, WipeOperationStatus::WriteFailure);
}

TEST_F(NsmSelectiveWipeTest, InvalidDataCcIsInvalidArgument)
{
    testing::Mock::AllowLeak(mockDevice.get());
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(errorResponse(NSM_ERR_INVALID_DATA)));

    const auto [rc, status] = runWipeOnDevice(
        NSM_SELECTIVE_DATA_WIPE_TARGET_BIT(NSM_WIPE_TARGET_USER_DATA));
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(status, WipeOperationStatus::InvalidArgument);
}
