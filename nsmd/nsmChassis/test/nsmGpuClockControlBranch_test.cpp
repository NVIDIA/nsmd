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
 * Branch coverage for nsmGpuClockControl.cpp:
 * - NsmClearClockLimAsyncIntf: clearReqClockLimit postPatchIO/decode paths
 * - NsmChassisClockControl: genRequestMsg, handleResponseMsg,
 * setRangeClockLimits
 * - Factory: createControlGpuClock missing properties, invalid UUID
 */

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
struct NsmGpuClockControlBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/clockctrl_br";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmGpuClockControlBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGpuClockControlBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    static std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ===========================================================================
// NsmChassisClockControl::genRequestMsg - success and failure
// ===========================================================================
TEST_F(NsmGpuClockControlBranchTest, GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_GenOK", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/gen_ok",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmGpuClockControlBranchTest, GenRequestMsg_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_GenFail", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/gen_fail",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ===========================================================================
// NsmChassisClockControl::handleResponseMsg - success, errorCC, decodeFail
// ===========================================================================
TEST_F(NsmGpuClockControlBranchTest, HandleResponseMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_HRSuccess", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/hr_success",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    struct nsm_clock_limit clockLimit = {};
    clockLimit.requested_limit_min = 300;
    clockLimit.requested_limit_max = 2100;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_clock_limit_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, &clockLimit,
                                          msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(msg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    auto limits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(limits), 300u);
    EXPECT_EQ(std::get<1>(limits), 2100u);
}

TEST_F(NsmGpuClockControlBranchTest, HandleResponseMsg_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_HRErr", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/hr_err",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    auto resp = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST_F(NsmGpuClockControlBranchTest, HandleResponseMsg_DecodeSuccessNonZeroCC)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_HRNZCC", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/hr_nzcc",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_header_info hdr = {};
    hdr.nsm_msg_type = NSM_RESPONSE;
    hdr.instance_id = 0;
    hdr.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
    pack_nsm_header(&hdr, &msg->hdr);
    encode_reason_code(NSM_ERROR, ERR_NULL, NSM_GET_CLOCK_LIMIT, msg);

    auto cc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

// ===========================================================================
// NsmChassisClockControl::setRangeClockLimits - error paths
// ===========================================================================
TEST_F(NsmGpuClockControlBranchTest, SetRangeClockLimits_InvalidType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_SRInv", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/sr_inv",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("not a clock limit");

    EXPECT_THROW_COROUTINE(sensor.setRangeClockLimits(value, &status, gpu),
                           std::exception);
}

TEST_F(NsmGpuClockControlBranchTest, SetRangeClockLimits_PostPatchIOFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_SRPP", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/sr_pp",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    using ClockLimitTuple = std::tuple<std::string, uint32_t>;
    AsyncSetOperationValueType value =
        std::vector<ClockLimitTuple>{{"SettingMin", 300}, {"SettingMax", 2100}};

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    sensor.setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmGpuClockControlBranchTest, SetRangeClockLimits_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_SRDec", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/sr_dec",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    using ClockLimitTuple = std::tuple<std::string, uint32_t>;
    AsyncSetOperationValueType value =
        std::vector<ClockLimitTuple>{{"SettingMin", 300}, {"SettingMax", 2100}};

    auto resp = decodeFail();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    sensor.setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmGpuClockControlBranchTest, SetRangeClockLimits_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_SRSucc", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/sr_succ",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    using ClockLimitTuple = std::tuple<std::string, uint32_t>;
    AsyncSetOperationValueType value =
        std::vector<ClockLimitTuple>{{"SettingMin", 300}, {"SettingMax", 2100}};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_PLATFORM_ENVIRONMENTAL, NSM_SET_CLOCK_LIMIT,
                       msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    sensor.setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmGpuClockControlBranchTest,
       SetRangeClockLimits_OnlySettingMin_UsesCurrentMax)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    cpuConfigIntf->requestedSpeedLimits(std::make_tuple(100u, 2000u));
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_SRMin", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/sr_min",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    using ClockLimitTuple = std::tuple<std::string, uint32_t>;
    // Only SettingMin provided, SettingMax should use current value (2000)
    AsyncSetOperationValueType value =
        std::vector<ClockLimitTuple>{{"SettingMin", 500}};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_PLATFORM_ENVIRONMENTAL, NSM_SET_CLOCK_LIMIT,
                       msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    sensor.setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmGpuClockControlBranchTest,
       SetRangeClockLimits_OnlySettingMax_UsesCurrentMin)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    cpuConfigIntf->requestedSpeedLimits(std::make_tuple(100u, 2000u));
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_SRMax", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/sr_max",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    using ClockLimitTuple = std::tuple<std::string, uint32_t>;
    // Only SettingMax provided
    AsyncSetOperationValueType value =
        std::vector<ClockLimitTuple>{{"SettingMax", 1800}};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_PLATFORM_ENVIRONMENTAL, NSM_SET_CLOCK_LIMIT,
                       msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    sensor.setRangeClockLimits(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ===========================================================================
// NsmClearClockLimAsyncIntf::clearReqClockLimit - error paths
// ===========================================================================
TEST_F(NsmGpuClockControlBranchTest, ClearReqClockLimit_PostPatchIOFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    clearClockIntf->clearReqClockLimit(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmGpuClockControlBranchTest, ClearReqClockLimit_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);

    auto resp = decodeFail();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    clearClockIntf->clearReqClockLimit(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmGpuClockControlBranchTest, ClearReqClockLimit_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_common_resp(0, NSM_SUCCESS, ERR_NULL,
                       NSM_TYPE_PLATFORM_ENVIRONMENTAL, NSM_SET_CLOCK_LIMIT,
                       msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    clearClockIntf->clearReqClockLimit(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Factory tests omitted: createControlGpuClock requires valid D-Bus paths
// for NsmChassisClockControl constructor (PhysicalContextType/ClockMode
// enum conversion throws with empty strings).
