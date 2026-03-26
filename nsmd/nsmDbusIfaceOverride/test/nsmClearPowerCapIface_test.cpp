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

#include "nsmDbusIfaceOverride/nsmClearPowerCapIface.hpp"

using namespace nsm;

static std::vector<uint8_t> makeGetPowerLimitResp(uint32_t enforcedLimitMw)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0, enforcedLimitMw,
                                msg);
    return buf;
}

static std::vector<uint8_t> makeSetPowerLimitResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_power_limit_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

struct NsmClearPowerCapAsyncIfaceTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::string pcName = "ClearPowerCap";
    std::vector<std::string> parents;
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf;
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf;

    NsmClearPowerCapAsyncIfaceTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        EXPECT_CALL(mockManager, getEid(testing::_))
            .WillRepeatedly(testing::Return(0x10));
    }

    ~NsmClearPowerCapAsyncIfaceTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmClearPowerCapAsyncIntf> makeIface(const char* path)
    {
        auto& bus = utils::DBusHandler::getBus();
        powerCapIntf = std::make_shared<NsmPowerCapIntf>(bus, path, pcName,
                                                         parents, gpu);
        clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(bus, path);
        return std::make_shared<NsmClearPowerCapAsyncIntf>(
            bus, path, gpu, powerCapIntf, clearPowerCapIntf);
    }
};

// NsmClearPowerCapIntf: clearPowerCap() always returns 0
TEST(NsmClearPowerCapIntfTest, ClearPowerCap_ReturnsZero)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmClearPowerCapIntf intfObj(bus, "/xyz/test/clear_pc_simple");
    EXPECT_EQ(intfObj.NsmClearPowerCapIntf::clearPowerCap(), 0);
}

// getPowerCapFromDevice: postPatchIO fails → returns error, cap unchanged
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       GetPowerCapFromDevice_PostPatchIOFails_CapUnchanged)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/get_piof");
    powerCapIntf->powerCap(0xDEAD);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(100), NSM_ERROR));

    auto coro = iface->getPowerCapFromDevice();
    EXPECT_EQ(powerCapIntf->powerCap(), 0xDEAD);
}

// getPowerCapFromDevice: success with valid limit → sets powerCap in watts
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       GetPowerCapFromDevice_Success_SetsPowerCap)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/get_ok");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(5000)));

    auto coro = iface->getPowerCapFromDevice();
    EXPECT_EQ(powerCapIntf->powerCap(), 5u); // 5000 mW → 5 W
}

// getPowerCapFromDevice: INVALID_POWER_LIMIT → propagated as-is
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       GetPowerCapFromDevice_InvalidLimit_SetsInvalidMarker)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/get_inv");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(INVALID_POWER_LIMIT)));

    auto coro = iface->getPowerCapFromDevice();
    EXPECT_EQ(powerCapIntf->powerCap(), INVALID_POWER_LIMIT);
}

// getPowerCapFromDevice: error CC in decode → cap unchanged
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       GetPowerCapFromDevice_ErrorCC_CapUnchanged)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/get_errcc");
    powerCapIntf->powerCap(0xBEEF);

    std::vector<uint8_t> errResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(errResp.data());
    encode_get_power_limit_resp(0, NSM_ERROR, ERR_INVALID_RQD, 0, 0, 0, msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errResp));

    auto coro = iface->getPowerCapFromDevice();
    EXPECT_EQ(powerCapIntf->powerCap(), 0xBEEF);
}

// clearPowerCapOnDevice: postPatchIO fails → WriteFailure
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       ClearPowerCapOnDevice_PostPatchIOFails_WriteFailure)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/clear_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp(), NSM_ERROR));

    auto coro = iface->clearPowerCapOnDevice(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// clearPowerCapOnDevice: error CC → WriteFailure
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       ClearPowerCapOnDevice_ErrorCC_WriteFailure)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/clear_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp(NSM_ERROR)));

    auto coro = iface->clearPowerCapOnDevice(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// clearPowerCapOnDevice: success with empty parents → not WriteFailure
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       ClearPowerCapOnDevice_Success_EmptyParents)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/clear_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp()))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(200000)));

    auto coro = iface->clearPowerCapOnDevice(&status);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// doClearPowerCapOnDevice: success → statusInterface = Success
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       DoClearPowerCapOnDevice_Success_StatusSuccess)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/do_ok");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp()))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(100000)));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/async/status/clearpc_ok");
    statusIntf->status(AsyncOperationStatusType::InProgress);

    auto coro = iface->doClearPowerCapOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// doClearPowerCapOnDevice: failure → statusInterface = WriteFailure
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       DoClearPowerCapOnDevice_Failure_WriteFailure)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/do_fail");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp(), NSM_ERROR));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/async/status/clearpc_fail");

    auto coro = iface->doClearPowerCapOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmPowerCapIntf::setPowerCap() branch tests
// =============================================================================

// setPowerCap: value is not tuple<bool,uint32_t> → throws InvalidArgument
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       SetPowerCap_InvalidType_ThrowsInvalidArgument)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/setpc_type");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Pass a bool instead of tuple<bool,uint32_t> – reqPowerLimit will be null
    AsyncSetOperationValueType invalidType = bool{true};
    EXPECT_THROW_COROUTINE(
        powerCapIntf->setPowerCap(invalidType, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// setPowerCap: powerLimit exceeds maxPowerCapValue → status = InvalidArgument
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       SetPowerCap_PowerTooHigh_ReturnsInvalidArgument)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/clear_pc/setpc_toohigh");
    // Set the max to 100 so that 200 exceeds it
    powerCapIntf->maxPowerCapValue(100);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType highPower = std::tuple<bool, uint32_t>{false,
                                                                      200};
    auto coro = powerCapIntf->setPowerCap(highPower, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// setPowerCap: powerLimit below minPowerCapValue → status = InvalidArgument
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       SetPowerCap_PowerTooLow_ReturnsInvalidArgument)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/clear_pc/setpc_toolow");
    // Raise the min to 50 so that 10 is below it
    powerCapIntf->minPowerCapValue(50);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType lowPower = std::tuple<bool, uint32_t>{false, 10};
    auto coro = powerCapIntf->setPowerCap(lowPower, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// setPowerCap: valid value in range → calls setPowerCapOnDevice (success path)
// setPowerCapOnDevice calls postPatchIO twice: once to set, once in
// getPowerCapFromDevice() to verify the new value.
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       SetPowerCap_ValidValue_CallsSetPowerCapOnDevice)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/setpc_ok");
    powerCapIntf->minPowerCapValue(0);
    powerCapIntf->maxPowerCapValue(1000);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp()))
        .WillOnce(
            mockPostPatchIO(makeGetPowerLimitResp(100000))); // 100W=100000mW

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType validPower = std::tuple<bool, uint32_t>{true,
                                                                       100};
    auto coro = powerCapIntf->setPowerCap(validPower, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::InvalidArgument);
}

// clearPowerCapOnDevice: parent path NOT in objectPathToSensorMap →
// sensorIt == end() → FALSE branch of outer if → ++it, parent stays in list.
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       ClearPowerCapOnDevice_ParentNotInMap_ParentStaysInList)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/clear_pc/parent_notfound");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    const std::string unknownPath = "/xyz/test/unknown/parent";
    powerCapIntf->parents.push_back(unknownPath);

    // postPatchIO x2: clearPowerCapOnDevice (set) + getPowerCapFromDevice (get)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp()))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(300000)));

    auto coro = iface->clearPowerCapOnDevice(&status);

    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
    // Parent not found → stays in parents, never moved to sensorCache
    EXPECT_EQ(powerCapIntf->parents.size(), 1u);
    EXPECT_EQ(powerCapIntf->parents[0], unknownPath);
    EXPECT_TRUE(powerCapIntf->sensorCache.empty());
}

// clearPowerCapOnDevice: parent path IS in objectPathToSensorMap but the stored
// shared_ptr is null → `if (sensor)` FALSE branch → ++it, parent stays.
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       ClearPowerCapOnDevice_ParentFoundNullSensor_ParentStaysInList)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/parent_null");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    const std::string parentPath = "/xyz/test/parent/null_sensor";
    powerCapIntf->parents.push_back(parentPath);
    // Register null sensor: sensorIt != end() TRUE, but sensor == nullptr
    mockManager.objectPathToSensorMap[parentPath] = nullptr;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp()))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(200000)));

    auto coro = iface->clearPowerCapOnDevice(&status);

    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
    // Null sensor → not moved to sensorCache, parent stays in list
    EXPECT_EQ(powerCapIntf->parents.size(), 1u);
    EXPECT_TRUE(powerCapIntf->sensorCache.empty());
}

// clearPowerCap() D-Bus override: AsyncOperationManager returns a valid path →
// doClearPowerCapOnDevice executes synchronously (coverage mode); result is a
// non-empty object path.
TEST_F(NsmClearPowerCapAsyncIfaceTest,
       ClearPowerCap_Override_ReturnsNonEmptyPath)
{
    auto iface = makeIface("/xyz/openbmc_project/control/clear_pc/override_ok");

    // clearPowerCapOnDevice (set) + getPowerCapFromDevice (verify)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp()))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(150000)));

    auto objectPath = iface->clearPowerCap();
    EXPECT_FALSE(std::string(objectPath).empty());
}
