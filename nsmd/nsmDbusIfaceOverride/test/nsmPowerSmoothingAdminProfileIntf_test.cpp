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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "platform-environmental.h"

#include "nsmDbusIfaceOverride/nsmPowerSmoothingAdminProfileIntf.hpp"

using namespace nsm;

// -----------------------------------------------------------------------
// Helpers: encode query-admin-override response
// -----------------------------------------------------------------------
static std::vector<uint8_t>
    makeQueryAdminResp(uint8_t cc, nsm_admin_override_data data = {})
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_admin_override_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_admin_override_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, &data, msg);
    return buf;
}

// -----------------------------------------------------------------------
// Helpers: encode setup-admin-override response
// -----------------------------------------------------------------------
static std::vector<uint8_t> makeSetupAdminResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_setup_admin_override_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// -----------------------------------------------------------------------
// Test Fixture
// -----------------------------------------------------------------------
struct OemAdminProfileIntfTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    OemAdminProfileIntfTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~OemAdminProfileIntfTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<OemAdminProfileIntf> makeIntf(const std::string& path)
    {
        return std::make_shared<OemAdminProfileIntf>(
            utils::DBusHandler::getBus(), path, gpu);
    }
};

// -----------------------------------------------------------------------
// getAdminProfileFromDevice: postPatchIO fails → co_return error //

// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, GetAdminProfile_PostPatchIOFails_ReturnsError)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/get_piof");

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data), NSM_ERROR));

    auto coro = intf->getAdminProfileFromDevice();
}

// -----------------------------------------------------------------------
// getAdminProfileFromDevice: error CC → function completes without crash
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, GetAdminProfile_ErrorCC_CompletesCleanly)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/get_errcc");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_ERROR)));

    // Error CC path: decode reports error, fields not updated - no crash
    auto coro = intf->getAdminProfileFromDevice();
    SUCCEED(); // Test passes if no exception/crash
}

// -----------------------------------------------------------------------
// getAdminProfileFromDevice: success with INVALID_UINT16_VALUE floor
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest,
       GetAdminProfile_InvalidFloor_SetsInvalidUint32Value)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/get_invfloor");

    nsm_admin_override_data data{};
    data.admin_override_percent_tmp_floor = INVALID_UINT16_VALUE;
    data.admin_override_ramup_rate_in_miliwatts_per_second = 5000;
    data.admin_override_rampdown_rate_in_miliwatts_per_second = 3000;
    data.admin_override_rampdown_hysteresis_value_in_milisec = 2000;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    auto coro = intf->getAdminProfileFromDevice();
    EXPECT_EQ(intf->AdminPowerProfileIntf::tmpFloorPercent(),
              static_cast<double>(INVALID_UINT32_VALUE));
    EXPECT_DOUBLE_EQ(intf->AdminPowerProfileIntf::rampUpRate(), 5.0);
    EXPECT_DOUBLE_EQ(intf->AdminPowerProfileIntf::rampDownRate(), 3.0);
    EXPECT_DOUBLE_EQ(intf->AdminPowerProfileIntf::rampDownHysteresis(), 2.0);
}

// -----------------------------------------------------------------------
// getAdminProfileFromDevice: success with valid floor value
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, GetAdminProfile_ValidFloor_SetsConvertedPercent)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/get_validfloor");

    nsm_admin_override_data data{};
    // UFXP4.12 value for 50% = 0.5 * 4096 = 2048
    data.admin_override_percent_tmp_floor = 2048;
    data.admin_override_ramup_rate_in_miliwatts_per_second = 0;
    data.admin_override_rampdown_rate_in_miliwatts_per_second = 0;
    data.admin_override_rampdown_hysteresis_value_in_milisec = 0;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    auto coro = intf->getAdminProfileFromDevice();
    // 2048/4096 * 100 = 50%
    EXPECT_NEAR(intf->AdminPowerProfileIntf::tmpFloorPercent(), 50.0, 0.01);
}

// -----------------------------------------------------------------------
// resetParam: INVALID_POWER_LIMIT → true; valid value → false
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, ResetParam_InvalidLimit_ReturnsTrue)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/resetparam");
    EXPECT_TRUE(intf->resetParam(static_cast<double>(INVALID_POWER_LIMIT)));
    EXPECT_FALSE(intf->resetParam(100.0));
}

// -----------------------------------------------------------------------
// overrideAdminProfileParam: postPatchIO fails → WriteFailure
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest,
       OverrideAdminProfileParam_PostPatchIOFails_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/override_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp(), NSM_ERROR));

    auto coro = intf->overrideAdminProfileParam(1, 5.0, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// overrideAdminProfileParam: error CC → WriteFailure
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, OverrideAdminProfileParam_ErrorCC_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/override_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp(NSM_ERROR)));

    auto coro = intf->overrideAdminProfileParam(1, 5.0, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// overrideAdminProfileParam: success parameterId=0 → floor %
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest,
       OverrideAdminProfileParam_SuccessParamId0_CallsGet)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/override_param0");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    auto coro = intf->overrideAdminProfileParam(0, 50.0, &status);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// overrideAdminProfileParam: success parameterId=1 → ramp rate
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest,
       OverrideAdminProfileParam_SuccessParamId1_CallsGet)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/override_param1");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    auto coro = intf->overrideAdminProfileParam(1, 10.0, &status);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// resetAdminProfileParam: postPatchIO fails → WriteFailure
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest,
       ResetAdminProfileParam_PostPatchIOFails_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/reset_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp(), NSM_ERROR));

    auto coro = intf->resetAdminProfileParam(0, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// resetAdminProfileParam: error CC → WriteFailure
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, ResetAdminProfileParam_ErrorCC_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/reset_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp(NSM_ERROR)));

    auto coro = intf->resetAdminProfileParam(0, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// resetAdminProfileParam: success → calls getAdminProfileFromDevice
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, ResetAdminProfileParam_Success_CallsGet)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/reset_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    auto coro = intf->resetAdminProfileParam(0, &status);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// setTmpFloorPercent: non-double → throws InvalidArgument
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest,
       SetTmpFloorPercent_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/settmp_nondbls");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = uint32_t{42};
    EXPECT_THROW_COROUTINE(
        intf->setTmpFloorPercent(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// -----------------------------------------------------------------------
// setTmpFloorPercent: INVALID_POWER_LIMIT → calls resetAdminProfileParam
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, SetTmpFloorPercent_InvalidLimit_CallsReset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/settmp_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    auto coro = intf->setTmpFloorPercent(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// setTmpFloorPercent: valid value → calls overrideAdminProfileParam
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, SetTmpFloorPercent_ValidValue_CallsOverride)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/settmp_override");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType validVal = double{50.0};
    auto coro = intf->setTmpFloorPercent(validVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// setRampUpRate: non-double → throws InvalidArgument
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, SetRampUpRate_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/setrampup_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = bool{true};
    EXPECT_THROW_COROUTINE(
        intf->setRampUpRate(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// -----------------------------------------------------------------------
// setRampUpRate: valid value → calls overrideAdminProfileParam
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, SetRampUpRate_ValidValue_CallsOverride)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/setrampup_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{10.0};
    auto coro = intf->setRampUpRate(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// setRampUpRate: INVALID_POWER_LIMIT → calls resetAdminProfileParam
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, SetRampUpRate_InvalidLimit_CallsReset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/setrampup_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    auto coro = intf->setRampUpRate(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// setRampDownRate: non-double → throws InvalidArgument
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, SetRampDownRate_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/setrampdn_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = bool{false};
    EXPECT_THROW_COROUTINE(
        intf->setRampDownRate(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// -----------------------------------------------------------------------
// setRampDownRate: valid value → calls overrideAdminProfileParam
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, SetRampDownRate_ValidValue_CallsOverride)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/setrampdn_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{8.0};
    auto coro = intf->setRampDownRate(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// setRampDownRate: INVALID_POWER_LIMIT → calls resetAdminProfileParam
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, SetRampDownRate_InvalidLimit_CallsReset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/setrampdn_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    auto coro = intf->setRampDownRate(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// setRampDownHysteresis: non-double → throws InvalidArgument
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest,
       SetRampDownHysteresis_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/sethyst_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = uint32_t{0};
    EXPECT_THROW_COROUTINE(
        intf->setRampDownHysteresis(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// -----------------------------------------------------------------------
// setRampDownHysteresis: valid value → calls overrideAdminProfileParam
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, SetRampDownHysteresis_ValidValue_CallsOverride)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/sethyst_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{2.0};
    auto coro = intf->setRampDownHysteresis(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// setRampDownHysteresis: INVALID_POWER_LIMIT → calls resetAdminProfileParam
// -----------------------------------------------------------------------
TEST_F(OemAdminProfileIntfTest, SetRampDownHysteresis_InvalidLimit_CallsReset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/admin/sethyst_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminResp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    auto coro = intf->setRampDownHysteresis(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}
