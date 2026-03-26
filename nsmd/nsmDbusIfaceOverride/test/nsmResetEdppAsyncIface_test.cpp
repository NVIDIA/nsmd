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

#include "nsmResetEdppAsyncIface.hpp"

using namespace nsm;

struct NsmResetEdppAsyncIfaceTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string path = "/xyz/openbmc_project/control/edpp_reset";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmResetEdppAsyncIfaceTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmResetEdppAsyncIfaceTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmResetEdppAsyncIfaceTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetEdppIntf =
        std::make_shared<NsmResetEdppAsyncIntf>(bus, path.c_str(), gpu);

    EXPECT_NE(resetEdppIntf, nullptr);
    EXPECT_EQ(resetEdppIntf->device, gpu);
}

TEST_F(NsmResetEdppAsyncIfaceTest, testConstructorWithDifferentPaths)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetEdppIntf1 = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, "/xyz/openbmc_project/control/edpp_reset1", gpu);

    auto resetEdppIntf2 = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, "/xyz/openbmc_project/control/edpp_reset2", gpu);

    EXPECT_NE(resetEdppIntf1, nullptr);
    EXPECT_NE(resetEdppIntf2, nullptr);
}

TEST_F(NsmResetEdppAsyncIfaceTest, testDeviceStorage)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetEdppIntf =
        std::make_shared<NsmResetEdppAsyncIntf>(bus, path.c_str(), gpu);

    EXPECT_EQ(resetEdppIntf->device, gpu);
    EXPECT_NE(resetEdppIntf->device, nullptr);
}

TEST_F(NsmResetEdppAsyncIfaceTest, testResetEdppAsyncIntfInheritance)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetEdppIntf =
        std::make_shared<NsmResetEdppAsyncIntf>(bus, path.c_str(), gpu);

    ResetEdppAsyncIntf* baseIntf = resetEdppIntf.get();
    EXPECT_NE(baseIntf, nullptr);
}

TEST_F(NsmResetEdppAsyncIfaceTest, testStateChangeLoggerInheritance)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetEdppIntf =
        std::make_shared<NsmResetEdppAsyncIntf>(bus, path.c_str(), gpu);

    StateChangeLogger* logger = resetEdppIntf.get();
    EXPECT_NE(logger, nullptr);
}

TEST_F(NsmResetEdppAsyncIfaceTest, testMultipleInstances)
{
    auto& bus = utils::DBusHandler::getBus();

    const uuid_t gpu2Uuid = "STATIC:0:1:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto gpu2 = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(gpu2Uuid));
    EXPECT_NE(gpu2, nullptr);

    auto resetEdppIntf1 = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, "/xyz/openbmc_project/control/edpp_reset_gpu0", gpu);

    auto resetEdppIntf2 = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, "/xyz/openbmc_project/control/edpp_reset_gpu1", gpu2);

    EXPECT_NE(resetEdppIntf1, nullptr);
    EXPECT_NE(resetEdppIntf2, nullptr);
    EXPECT_NE(resetEdppIntf1->device, resetEdppIntf2->device);
}

TEST_F(NsmResetEdppAsyncIfaceTest, testConstructorWithNullDevice)
{
    auto& bus = utils::DBusHandler::getBus();

    std::shared_ptr<NsmDevice> nullDevice = nullptr;

    auto resetEdppIntf =
        std::make_shared<NsmResetEdppAsyncIntf>(bus, path.c_str(), nullDevice);

    EXPECT_NE(resetEdppIntf, nullptr);
    EXPECT_EQ(resetEdppIntf->device, nullptr);
}

TEST_F(NsmResetEdppAsyncIfaceTest, testPathStorage)
{
    auto& bus = utils::DBusHandler::getBus();

    std::string customPath = "/xyz/openbmc_project/test/custom_edpp";

    auto resetEdppIntf =
        std::make_shared<NsmResetEdppAsyncIntf>(bus, customPath.c_str(), gpu);

    EXPECT_NE(resetEdppIntf, nullptr);
}

TEST_F(NsmResetEdppAsyncIfaceTest, testConstructorInitialization)
{
    auto& bus = utils::DBusHandler::getBus();

    auto resetEdppIntf =
        std::make_shared<NsmResetEdppAsyncIntf>(bus, path.c_str(), gpu);

    EXPECT_NE(resetEdppIntf, nullptr);
    EXPECT_NE(resetEdppIntf->device, nullptr);
    EXPECT_EQ(resetEdppIntf->device->getEid(), gpu->getEid());
}

// Error-CC path: clearSetPoint coroutine – postPatchIO succeeds but
// cc = NSM_ERROR → co_return cc ? cc : rc; takes the true branch. //

TEST_F(NsmResetEdppAsyncIfaceTest, ClearSetPoint_ErrorCC_CoversBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetEdppIntf =
        std::make_shared<NsmResetEdppAsyncIntf>(bus, path.c_str(), gpu);

    // Encode a set_programmable_EDPp_scaling_factor response with NSM_ERROR
    // completion code so decode succeeds but cc != 0, covering the true
    // branch of co_return cc ? cc : rc.
    std::vector<uint8_t> errorResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(errorResp.data());
    auto encRc = encode_set_programmable_EDPp_scaling_factor_resp(
        0, NSM_ERROR, ERR_NULL, respPtr);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = resetEdppIntf->clearSetPoint(&status);

    // cc = NSM_ERROR → WriteFailure; true branch of co_return cc ? cc : rc //

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// ClearSetPoint: postPatchIO transport fails → if(rc_) TRUE → WriteFailure
// -----------------------------------------------------------------------
TEST_F(NsmResetEdppAsyncIfaceTest, ClearSetPoint_PostPatchIOFails_WriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string p = path + "_ppfail";
    auto resetEdppIntf = std::make_shared<NsmResetEdppAsyncIntf>(bus, p.c_str(),
                                                                 gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = resetEdppIntf->clearSetPoint(&status);

    // rc_ = NSM_ERROR (non-zero) → if(rc_) TRUE → WriteFailure
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// ClearSetPoint: success (cc=NSM_SUCCESS, rc=NSM_SW_SUCCESS) →
// if(rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS) FALSE branch; status
// stays Success; co_return cc ? cc : rc takes the FALSE branch (both 0). //

// -----------------------------------------------------------------------
TEST_F(NsmResetEdppAsyncIfaceTest, ClearSetPoint_Success_StatusSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string p = path + "_succ";
    auto resetEdppIntf = std::make_shared<NsmResetEdppAsyncIntf>(bus, p.c_str(),
                                                                 gpu);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_programmable_EDPp_scaling_factor_resp(
                  0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = resetEdppIntf->clearSetPoint(&status);

    // cc=0, rc=0 → FALSE branches → status stays Success
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// -----------------------------------------------------------------------
// DoResetEdppOnDevice: success → statusInterface status = Success
// -----------------------------------------------------------------------
TEST_F(NsmResetEdppAsyncIfaceTest, DoResetEdppOnDevice_Success_StatusSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string p = path + "_do_ok";
    auto resetEdppIntf = std::make_shared<NsmResetEdppAsyncIntf>(bus, p.c_str(),
                                                                 gpu);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_programmable_EDPp_scaling_factor_resp(
                  0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/edpp_do_ok");
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto coro = resetEdppIntf->doResetEdppOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// -----------------------------------------------------------------------
// DoResetEdppOnDevice: failure → statusInterface status = WriteFailure
// -----------------------------------------------------------------------
TEST_F(NsmResetEdppAsyncIfaceTest, DoResetEdppOnDevice_Failure_WriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string p = path + "_do_fail";
    auto resetEdppIntf = std::make_shared<NsmResetEdppAsyncIntf>(bus, p.c_str(),
                                                                 gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/edpp_do_fail");
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto coro = resetEdppIntf->doResetEdppOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// Reset: objectPath not empty → success; detached coroutine runs
// synchronously in coverage mode (consumes the postPatchIO mock).
// -----------------------------------------------------------------------
TEST_F(NsmResetEdppAsyncIfaceTest, Reset_Success_ReturnsNonEmptyPath)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string p = path + "_reset_ok";
    auto resetEdppIntf = std::make_shared<NsmResetEdppAsyncIntf>(bus, p.c_str(),
                                                                 gpu);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_programmable_EDPp_scaling_factor_resp(
                  0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    auto objectPath = resetEdppIntf->reset();
    EXPECT_FALSE(std::string(objectPath).empty());
}
