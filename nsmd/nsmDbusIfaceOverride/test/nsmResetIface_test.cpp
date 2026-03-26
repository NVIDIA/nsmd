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

#include "nsmResetIface.hpp"

using namespace nsm;

struct NsmResetIntfTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string resetPath =
        "/xyz/openbmc_project/control/processor/reset0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmResetIntfTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmResetIntfTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmResetIntfTest, testNsmResetIntfConstructor)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetIntf = std::make_shared<NsmResetIntf>(bus, resetPath.c_str());

    EXPECT_NE(resetIntf, nullptr);
}

TEST_F(NsmResetIntfTest, testNsmResetIntfReset)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetIntf = std::make_shared<NsmResetIntf>(bus, resetPath.c_str());

    int result = resetIntf->reset();
    EXPECT_EQ(result, 0);
}

TEST_F(NsmResetIntfTest, testNsmResetAsyncIntfConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t deviceIndex = 0;

    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, resetPath.c_str(), gpu, deviceIndex);

    EXPECT_NE(resetAsyncIntf, nullptr);
    EXPECT_EQ(resetAsyncIntf->device, gpu);
    EXPECT_EQ(resetAsyncIntf->deviceIndex, deviceIndex);
}

TEST_F(NsmResetIntfTest, testNsmResetDeviceIntfConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string deviceResetPath = "/xyz/openbmc_project/control/reset0";

    auto resetDeviceIntf =
        std::make_shared<NsmResetDeviceIntf>(bus, deviceResetPath.c_str());

    EXPECT_NE(resetDeviceIntf, nullptr);
}

TEST_F(NsmResetIntfTest, testNsmResetDeviceIntfReset)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string deviceResetPath = "/xyz/openbmc_project/control/reset0";

    auto resetDeviceIntf =
        std::make_shared<NsmResetDeviceIntf>(bus, deviceResetPath.c_str());

    EXPECT_NO_THROW(resetDeviceIntf->reset());
}

TEST_F(NsmResetIntfTest, testNsmNetworkDeviceResetAsyncIntfConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string networkResetPath =
        "/xyz/openbmc_project/control/network_reset0";

    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, networkResetPath.c_str(), gpu);

    EXPECT_NE(networkResetIntf, nullptr);
    EXPECT_EQ(networkResetIntf->device, gpu);
}

TEST_F(NsmResetIntfTest, testMultipleResetInterfaces)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetIntf1 = std::make_shared<NsmResetIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset1");
    auto resetIntf2 = std::make_shared<NsmResetIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset2");

    EXPECT_NE(resetIntf1, nullptr);
    EXPECT_NE(resetIntf2, nullptr);
    EXPECT_NE(resetIntf1, resetIntf2);
}

TEST_F(NsmResetIntfTest, testResetAsyncWithDifferentDeviceIndex)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetAsync1 = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset1", gpu, 0);
    auto resetAsync2 = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset2", gpu, 1);

    EXPECT_EQ(resetAsync1->deviceIndex, 0);
    EXPECT_EQ(resetAsync2->deviceIndex, 1);
}

// assertFundamentalReset – success response → FALSE branch of
// co_return cc ? cc : rc (cc = NSM_SUCCESS = 0 after decode). //

TEST_F(NsmResetIntfTest, AssertFundamentalReset_Success_CoversFalseBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset_frt_succ", gpu, 0);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_assert_pcie_fundamental_reset_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    auto coro = resetAsyncIntf->assertFundamentalReset(RESET);
    // cc = NSM_SUCCESS → false branch of co_return cc ? cc : rc //
}

// assertFundamentalReset – error-CC response → TRUE branch of
// co_return cc ? cc : rc (cc = NSM_ERROR after decode).
TEST_F(NsmResetIntfTest, AssertFundamentalReset_ErrorCC_CoversTrueBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset_frt_err", gpu, 0);

    std::vector<uint8_t> errorResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(errorResp.data());
    ASSERT_EQ(encode_assert_pcie_fundamental_reset_resp(0, NSM_ERROR, ERR_NULL,
                                                        respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    auto coro = resetAsyncIntf->assertFundamentalReset(RESET);
    // cc = NSM_ERROR → true branch of co_return cc ? cc : rc //
}

// resetOnDevice – success response → FALSE branch of co_return cc ? cc : rc //
// (cc = NSM_SUCCESS = 0 stays 0 after decode).
TEST_F(NsmResetIntfTest, ResetOnDevice_Success_CoversFalseBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string networkResetPath =
        "/xyz/openbmc_project/control/network_reset_succ";
    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, networkResetPath.c_str(), gpu);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    encode_reset_network_device_resp(0, ERR_NULL, respPtr);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = networkResetIntf->resetOnDevice(&status);

    // cc = NSM_SUCCESS = 0 → false branch of co_return cc ? cc : rc //

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Error-CC path: resetOnDevice coroutine – postPatchIO succeeds but
// cc = NSM_ERROR → co_return cc ? cc : rc; takes the true branch. //

TEST_F(NsmResetIntfTest, ResetOnDevice_ErrorCC_CoversBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string networkResetPath =
        "/xyz/openbmc_project/control/network_reset_errcc";
    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, networkResetPath.c_str(), gpu);

    // encode_reason_code produces a proper nsm_common_non_success_resp so that
    // decode_reason_code_and_cc reads cc = NSM_ERROR and returns
    // NSM_SW_SUCCESS, making decode_reset_network_device_resp return early with
    // cc != 0. This exercises the true branch of co_return cc ? cc : rc. //

    std::vector<uint8_t> errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_reason_code(NSM_ERROR, ERR_NULL, NSM_RESET_NETWORK_DEVICE, respPtr);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = networkResetIntf->resetOnDevice(&status);

    // cc = NSM_ERROR → WriteFailure; true branch of co_return cc ? cc : rc //

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// assertFundamentalReset: postPatchIO transport fails → if(rc_) TRUE branch
// -----------------------------------------------------------------------
TEST_F(NsmResetIntfTest, AssertFundamentalReset_PostPatchIOFails_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset_pp_fail", gpu, 0);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    // rc_ = NSM_ERROR (non-zero) → if(rc_) TRUE → co_return rc_ //

    auto coro = resetAsyncIntf->assertFundamentalReset(RESET);
}

// -----------------------------------------------------------------------
// doResetOnDevice: RESET fails → if(rc != 0) TRUE → status = InternalFailure
// -----------------------------------------------------------------------
TEST_F(NsmResetIntfTest, DoResetOnDevice_ResetFails_InternalFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset_dro_fail", gpu, 0);

    // postPatchIO returns error → assertFundamentalReset(RESET) returns error
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/async/status/dro_fail");
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto coro = resetAsyncIntf->doResetOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// -----------------------------------------------------------------------
// doResetOnDevice: RESET succeeds, NOT_RESET fails → else branch taken;
// if(rc_ != 0) TRUE → status = InternalFailure
// -----------------------------------------------------------------------
TEST_F(NsmResetIntfTest,
       DoResetOnDevice_ResetSucceeds_NotResetFails_InternalFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset_dro_notresetfail",
        gpu, 0);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_assert_pcie_fundamental_reset_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    // First call (RESET) → success; second call (NOT_RESET) → transport error
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp)) // RESET succeeds
        .WillOnce(mockPostPatchIO(NSM_ERROR));  // NOT_RESET fails

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/async/status/dro_notresetfail");
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto coro = resetAsyncIntf->doResetOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// -----------------------------------------------------------------------
// doResetOnDevice: both RESET and NOT_RESET succeed → status = Success
// -----------------------------------------------------------------------
TEST_F(NsmResetIntfTest, DoResetOnDevice_BothSucceed_StatusSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset_dro_both_ok", gpu,
        0);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_assert_pcie_fundamental_reset_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp))  // RESET → success
        .WillOnce(mockPostPatchIO(successResp)); // NOT_RESET → success

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/async/status/dro_both_ok");
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto coro = resetAsyncIntf->doResetOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// -----------------------------------------------------------------------
// NsmResetAsyncIntf::reset: objectPath not empty → success; detached
// coroutine runs synchronously in coverage mode.
// -----------------------------------------------------------------------
TEST_F(NsmResetIntfTest, Reset_NsmResetAsyncIntf_ReturnsNonEmptyPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/processor/reset_async_ok", gpu, 0);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_assert_pcie_fundamental_reset_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp))  // RESET
        .WillOnce(mockPostPatchIO(successResp)); // NOT_RESET

    auto objectPath = resetAsyncIntf->reset();
    EXPECT_FALSE(std::string(objectPath).empty());
}

// -----------------------------------------------------------------------
// NsmNetworkDeviceResetAsyncIntf::resetOnDevice: postPatchIO fails →
// if(rc_) TRUE branch → WriteFailure
// -----------------------------------------------------------------------
TEST_F(NsmResetIntfTest, ResetOnDevice_PostPatchIOFails_WriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/network_reset_ppf", gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = networkResetIntf->resetOnDevice(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// NsmNetworkDeviceResetAsyncIntf::doResetOnDevice: success → status=Success
// -----------------------------------------------------------------------
TEST_F(NsmResetIntfTest, NetworkDoResetOnDevice_Success_StatusSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/network_reset_do_ok", gpu);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    encode_reset_network_device_resp(0, ERR_NULL, respPtr);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/async/status/net_do_ok");
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto coro = networkResetIntf->doResetOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// -----------------------------------------------------------------------
// NsmNetworkDeviceResetAsyncIntf::reset: success → returns non-empty path
// -----------------------------------------------------------------------
TEST_F(NsmResetIntfTest, NetworkReset_Success_ReturnsNonEmptyPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, "/xyz/openbmc_project/control/network_reset_ok", gpu);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    encode_reset_network_device_resp(0, ERR_NULL, respPtr);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    auto objectPath = networkResetIntf->reset();
    EXPECT_FALSE(std::string(objectPath).empty());
}
