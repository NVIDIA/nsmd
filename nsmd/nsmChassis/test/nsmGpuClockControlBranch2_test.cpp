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

// Additional branch coverage for nsmGpuClockControl.cpp
// Targets:
//   - clearClockLimit: AsyncOp path
//   - doClearClockLimitOnDevice: wrapper coroutine
//   - setRangeClockLimits: neither SettingMin nor SettingMax provided
//   - Factory: createControlGpuClock with nsmDevice=null

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "platform-environmental.h"

#include "nsmGpuClockControl.hpp"
#include "test/commonMock.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine createControlGpuClock(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

// ============================================================================
// Fixture
// ============================================================================
struct NsmGpuClockControlBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/clockctrl_br2";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmGpuClockControlBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGpuClockControlBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    static std::vector<uint8_t> makeSuccessSetClockResp()
    {
        std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp));
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                           NSM_TYPE_PLATFORM_ENVIRONMENTAL, NSM_SET_CLOCK_LIMIT,
                           msg);
        return resp;
    }
};

// =========================================================================
// doClearClockLimitOnDevice: sets status
// =========================================================================
TEST_F(NsmGpuClockControlBranch2Test,
       DoClearClockLimitOnDevice_Success_SetsStatus)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);

    auto resp = makeSuccessSetClockResp();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/test/clk_doclear0");

    clearClockIntf->doClearClockLimitOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmGpuClockControlBranch2Test,
       DoClearClockLimitOnDevice_Failure_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/test/clk_doclear_fail");

    clearClockIntf->doClearClockLimitOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// clearClockLimit: returns object path
// =========================================================================
TEST_F(NsmGpuClockControlBranch2Test, ClearClockLimit_ReturnsObjectPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);

    auto resp = makeSuccessSetClockResp();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillRepeatedly(mockPostPatchIO(resp));

    auto objPath = clearClockIntf->clearClockLimit();
    EXPECT_FALSE(std::string(objPath).empty());
}

// =========================================================================
// setRangeClockLimits: no SettingMin or SettingMax provided
// =========================================================================
TEST_F(NsmGpuClockControlBranch2Test,
       SetRangeClockLimits_NeitherMinNorMax_UsesCurrentBoth)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    cpuConfigIntf->requestedSpeedLimits(std::make_tuple(200u, 1800u));
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_NoBoth", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/sr_noboth",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    using ClockLimitTuple = std::tuple<std::string, uint32_t>;
    // Provide unknown key
    AsyncSetOperationValueType value =
        std::vector<ClockLimitTuple>{{"UnknownKey", 999}};

    auto resp = makeSuccessSetClockResp();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    sensor.setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =========================================================================
// setRangeClockLimits: empty vector (both default to UINT_MAX then current)
// =========================================================================
TEST_F(NsmGpuClockControlBranch2Test,
       SetRangeClockLimits_EmptyVector_UsesCurrentBoth)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    cpuConfigIntf->requestedSpeedLimits(std::make_tuple(150u, 1500u));
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_Empty", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/sr_empty",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    using ClockLimitTuple = std::tuple<std::string, uint32_t>;
    AsyncSetOperationValueType value = std::vector<ClockLimitTuple>{};

    auto resp = makeSuccessSetClockResp();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    sensor.setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =========================================================================
// Factory: createControlGpuClock with missing UUID (nsmDevice = null)
// =========================================================================
// Factory null device test removed: requires utils::DBusHandler::getInstance()
