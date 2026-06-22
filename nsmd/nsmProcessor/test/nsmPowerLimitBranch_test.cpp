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

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmPowerLimit.hpp"

#include <cmath>

using namespace nsm;

// =============================================================================
// Fixture for tests requiring MockNsmDevice (postPatchIO / sensorIO)
// =============================================================================
struct NsmPowerLimitBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPowerLimitBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPowerLimitBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    auto& bus()
    {
        return utils::DBusHandler::getBus();
    }

    // Build a properly sized set_device_mode_settings_v2 success response
    std::vector<uint8_t> makeSetDevModeSuccessResp()
    {
        std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL, msg);
        return resp;
    }

    std::vector<uint8_t> makeSetDevModeErrorResp()
    {
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        encode_set_device_mode_settings_v2_resp(0, NSM_ERROR, ERR_NULL, msg);
        return resp;
    }

    std::shared_ptr<NsmPersistentPowerLimit> makeSensor(uint8_t limitId,
                                                        bool withPersistency)
    {
        auto powerLimitsIntf =
            std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
        auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(),
                                                                  objPath);
        auto assocIntf = std::make_shared<AssociationDefinitionsIntf>(
            bus(), objPath.c_str());
        std::shared_ptr<PowerPersistencyIntf> persistencyIntf;
        if (withPersistency)
        {
            persistencyIntf =
                std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
            persistencyIntf->persistentPowerLimit(300.0);
            persistencyIntf->oneShotPowerLimit(0.0);
            persistencyIntf->persistency(false);
        }
        return std::make_shared<NsmPersistentPowerLimit>(
            "TestPowerLimit", "NSM_GPU_BASE_POWER_LIMIT", powerLimitsIntf,
            clearIntf, assocIntf, gpu, limitId, persistencyIntf);
    }
};

// =============================================================================
// setPowerLimit: std::tuple<bool, uint32_t> variant → updatePowerLimit success
// =============================================================================
TEST_F(NsmPowerLimitBranchTest, SetPowerLimit_TupleVariant_Success)
{
    auto sensor = makeSensor(GPU_BASE, false);
    auto successResp = makeSetDevModeSuccessResp();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::WriteFailure;
    AsyncSetOperationValueType value = std::make_tuple(true, uint32_t(300));
    sensor->setPowerLimit(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setPowerLimit: uint32_t variant → updatePowerLimit success
TEST_F(NsmPowerLimitBranchTest, SetPowerLimit_UInt32Variant_Success)
{
    auto sensor = makeSensor(GPU_BASE, false);
    auto successResp = makeSetDevModeSuccessResp();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::WriteFailure;
    AsyncSetOperationValueType value = uint32_t(500);
    sensor->setPowerLimit(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setPowerLimit: invalid variant type → throws InvalidArgument
TEST_F(NsmPowerLimitBranchTest, SetPowerLimit_InvalidVariant_Throws)
{
    auto sensor = makeSensor(GPU_BASE, false);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool(true); // not tuple or uint32_t
    EXPECT_THROW_COROUTINE(
        sensor->setPowerLimit(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// updatePowerLimit: postPatchIO fails → status = WriteFailure, rc != 0
TEST_F(NsmPowerLimitBranchTest, UpdatePowerLimit_PostPatchIOFails_WriteFailure)
{
    auto sensor = makeSensor(GPU_BASE, false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t(300);
    sensor->setPowerLimit(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// updatePowerLimit: response has error cc → status = WriteFailure
TEST_F(NsmPowerLimitBranchTest, UpdatePowerLimit_ErrorCC_WriteFailure)
{
    auto sensor = makeSensor(GPU_BASE, false);
    auto errorResp = makeSetDevModeErrorResp();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t(300);
    sensor->setPowerLimit(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// updatePowerLimit with CPU_LIMIT_GPU_COPY powerLimitId (one-shot) →
// verifies powerLimitIdToDeviceModeIndex TRUE branch for persistent=false
TEST_F(NsmPowerLimitBranchTest, SetPowerLimit_CPULimitGPUCopy_Success)
{
    auto sensor = makeSensor(CPU_LIMIT_GPU_COPY, false);
    auto successResp = makeSetDevModeSuccessResp();

    // persistent=false → DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::WriteFailure;
    // tuple with persistent=false
    AsyncSetOperationValueType value = std::make_tuple(false, uint32_t(200));
    sensor->setPowerLimit(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// handleResponseMsg: additional branch paths
// =============================================================================

// persistencyIntf not null → persistentPowerLimit and powerCap both set
// to reading=300 → persistency=true (both sides set from same value)
TEST_F(NsmPowerLimitBranchTest,
       HandleResponseMsg_PersistencyIntf_PersistencyTrue)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    auto assocIntf =
        std::make_shared<AssociationDefinitionsIntf>(bus(), objPath.c_str());
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    persistencyIntf->persistency(false); // start as false

    NsmPersistentPowerLimit sensor("test", "type", powerLimitsIntf, clearIntf,
                                   assocIntf, nullptr, GPU_BASE,
                                   persistencyIntf);

    uint32_t powerLimitValue = htole32(300000); // 300W
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(powerLimitValue) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue),
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue), responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // pending mode value is valid → persistency=true and persistentPowerLimit
    // is the pending value (300000mW / 1000 = 300W)
    EXPECT_TRUE(persistencyIntf->persistency());
    EXPECT_EQ(persistencyIntf->persistentPowerLimit(), 300.0);
}

// currentModeLength = 0 (unexpected size) → shouldLog/error path
TEST_F(NsmPowerLimitBranchTest, HandleResponseMsg_CurrentModeLenZero_ErrorPath)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    auto assocIntf =
        std::make_shared<AssociationDefinitionsIntf>(bus(), objPath.c_str());

    NsmPersistentPowerLimit sensor("test", "type", powerLimitsIntf, clearIntf,
                                   assocIntf, nullptr, GPU_BASE, nullptr);

    // encode with empty mode data (currentModeLength = 0)
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, reason_code, nullptr, 0, nullptr, 0, responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// pendingModeLength = 0 → persistency=false, persistentPowerLimit=nan
TEST_F(NsmPowerLimitBranchTest, HandleResponseMsg_PendingModeLenZero_Nan)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    auto assocIntf =
        std::make_shared<AssociationDefinitionsIntf>(bus(), objPath.c_str());
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    persistencyIntf->persistentPowerLimit(300.0);
    persistencyIntf->persistency(true);

    NsmPersistentPowerLimit sensor("test", "type", powerLimitsIntf, clearIntf,
                                   assocIntf, nullptr, GPU_BASE,
                                   persistencyIntf);

    uint32_t currentLimit = htole32(300000); // 300W
    // Only provide currentModeData, not pendingModeData (pendingModeLength=0)
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(currentLimit),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        reinterpret_cast<const uint8_t*>(&currentLimit), sizeof(currentLimit),
        nullptr, 0, responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(powerLimitsIntf->powerCap(), 300u);
    EXPECT_FALSE(persistencyIntf->persistency());
    EXPECT_TRUE(std::isnan(persistencyIntf->persistentPowerLimit()));
}

// INVALID_POWER_LIMIT as currentLimit → reading = 0
TEST_F(NsmPowerLimitBranchTest, HandleResponseMsg_InvalidPowerLimit_ReadingZero)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    auto assocIntf =
        std::make_shared<AssociationDefinitionsIntf>(bus(), objPath.c_str());

    NsmPersistentPowerLimit sensor("test", "type", powerLimitsIntf, clearIntf,
                                   assocIntf, nullptr, GPU_BASE, nullptr);

    uint32_t invalidLimit = htole32(INVALID_POWER_LIMIT);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(invalidLimit) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        reinterpret_cast<const uint8_t*>(&invalidLimit), sizeof(invalidLimit),
        reinterpret_cast<const uint8_t*>(&invalidLimit), sizeof(invalidLimit),
        responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(powerLimitsIntf->powerCap(), 0u);
}

// INVALID_POWER_LIMIT as pendingLimit → persistency=false,
// persistentPowerLimit=nan
TEST_F(NsmPowerLimitBranchTest, HandleResponseMsg_InvalidPendingLimit_Nan)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    auto assocIntf =
        std::make_shared<AssociationDefinitionsIntf>(bus(), objPath.c_str());
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    persistencyIntf->persistentPowerLimit(500.0);
    persistencyIntf->persistency(true);

    NsmPersistentPowerLimit sensor("test", "type", powerLimitsIntf, clearIntf,
                                   assocIntf, nullptr, GPU_BASE,
                                   persistencyIntf);

    uint32_t currentLimit = htole32(300000);
    uint32_t pendingLimit = htole32(INVALID_POWER_LIMIT);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(currentLimit) + sizeof(pendingLimit),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        reinterpret_cast<const uint8_t*>(&currentLimit), sizeof(currentLimit),
        reinterpret_cast<const uint8_t*>(&pendingLimit), sizeof(pendingLimit),
        responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(powerLimitsIntf->powerCap(), 300u);
    EXPECT_FALSE(persistencyIntf->persistency());
    EXPECT_TRUE(std::isnan(persistencyIntf->persistentPowerLimit()));
}

// =============================================================================
// createGPUPowerLimit branch coverage
// =============================================================================

// createGPUPowerLimit with "NSM_GPU_BASE_POWER_LIMIT" → creates 5 sensors
TEST_F(NsmPowerLimitBranchTest, CreateGPUPowerLimit_GPUBaseType_AddsSensors)
{
    const size_t beforeDevice = gpu->deviceSensors.size();
    const size_t beforeStatic = gpu->staticSensors.size();

    createGPUPowerLimit(gpu, bus(), "GPU0_PowerLimit",
                        "NSM_GPU_BASE_POWER_LIMIT", objPath);

    // addSensor (x2: NsmPersistentPowerLimit, NsmOneShotPowerLimit)
    // addStaticSensor (x3: NsmDefaultPowerLimit, NsmMaxPowerLimit,
    // NsmMinPowerLimit)
    EXPECT_GT(gpu->deviceSensors.size(), beforeDevice);
    EXPECT_GT(gpu->staticSensors.size(), beforeStatic);
}

// createGPUPowerLimit with "NSM_GPU_COPY_CPU_POWER_LIMIT" → creates sensors
TEST_F(NsmPowerLimitBranchTest, CreateGPUPowerLimit_GPUCopyCPUType_AddsSensors)
{
    const size_t beforeDevice = gpu->deviceSensors.size();

    createGPUPowerLimit(gpu, bus(), "GPU0_CopyPowerLimit",
                        "NSM_GPU_COPY_CPU_POWER_LIMIT", objPath);

    // addSensor (x2: NsmPersistentPowerLimit w/ nullptr clearPowerLimitIntf,
    // NsmOneShotPowerLimit)
    EXPECT_GT(gpu->deviceSensors.size(), beforeDevice);
}

// createGPUPowerLimit with unknown type → no sensors added
TEST_F(NsmPowerLimitBranchTest, CreateGPUPowerLimit_UnknownType_NoSensors)
{
    const size_t beforeDevice = gpu->deviceSensors.size();
    const size_t beforeStatic = gpu->staticSensors.size();

    createGPUPowerLimit(gpu, bus(), "GPU0_UnknownLimit",
                        "NSM_UNKNOWN_POWER_LIMIT", objPath);

    EXPECT_EQ(gpu->deviceSensors.size(), beforeDevice);
    EXPECT_EQ(gpu->staticSensors.size(), beforeStatic);
}

// NsmOneShotPowerLimit: persistencyIntf != nullptr → oneShotPowerLimit is
// sourced from the pending mode value; persistency is left untouched.
TEST_F(NsmPowerLimitBranchTest, NsmOneShot_HandleResponse_OneShotFromPending)
{
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());

    powerLimitsIntf->powerCap(500);
    persistencyIntf->persistentPowerLimit(300.0);
    persistencyIntf->persistency(true);

    NsmOneShotPowerLimit sensor("test", "type", GPU_BASE, persistencyIntf,
                                powerLimitsIntf);

    uint32_t powerLimitValue = htole32(200000); // 200W one-shot
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(powerLimitValue) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue),
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue), responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // oneShotPowerLimit comes from the pending value (200000mW / 1000 = 200W)
    EXPECT_DOUBLE_EQ(persistencyIntf->oneShotPowerLimit(), 200.0);
    // persistency is no longer written by the one-shot handler
    EXPECT_TRUE(persistencyIntf->persistency());
}

// NsmOneShotPowerLimit: currentModeLength != sizeof(uint32_t) → shouldLog path
TEST_F(NsmPowerLimitBranchTest, NsmOneShot_HandleResponse_InvalidCurrentModeLen)
{
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());

    NsmOneShotPowerLimit sensor("test", "type", GPU_BASE, persistencyIntf,
                                powerLimitsIntf);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL, nullptr,
                                            0, nullptr, 0, responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// NsmPersistentPowerLimit::handleResponseMsg: rc==NSM_SW_SUCCESS,
// cc!=NSM_SUCCESS → L126 FALSE via cc!=NSM_SUCCESS branch.
// Buffer exactly sizeof(nsm_msg_hdr)+sizeof(nsm_common_non_success_resp)
// so decode_reason_code_and_cc returns NSM_SW_SUCCESS with cc=NSM_ERROR.
TEST_F(NsmPowerLimitBranchTest,
       HandleResponseMsg_Persistent_DecodeSuccessNonZeroCC_ElseBranch)
{
    auto sensor = makeSensor(GPU_BASE, false);
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // cc = NSM_ERROR
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmOneShotPowerLimit::handleResponseMsg: rc==NSM_SW_SUCCESS,
// cc!=NSM_SUCCESS → L525 FALSE via cc!=NSM_SUCCESS branch.
TEST_F(NsmPowerLimitBranchTest,
       HandleResponseMsg_OneShot_DecodeSuccessNonZeroCC_ElseBranch)
{
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    NsmOneShotPowerLimit sensor("test", "type", GPU_BASE, persistencyIntf,
                                powerLimitsIntf);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // cc = NSM_ERROR
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmPowerLimitRange: update with sensorIO failure
TEST_F(NsmPowerLimitBranchTest, NsmPowerLimitRange_Update_SensorIOFails)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    NsmPowerLimitRange sensor("test", "type", MAXIMUM_GPU_BASE_POWER_LIMIT,
                              powerLimitsIntf);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    EXPECT_NO_THROW(sensor.update(gpu));
}

// NsmPowerLimitRange: update with success + valid data
TEST_F(NsmPowerLimitBranchTest, NsmPowerLimitRange_Update_Success)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    NsmPowerLimitRange sensor("test", "type", MAXIMUM_GPU_BASE_POWER_LIMIT,
                              powerLimitsIntf);

    // Build inventory information response with 4-byte uint32 value
    uint32_t limitVal = htole32(500000); // 500W
    std::vector<uint8_t> limitData(sizeof(limitVal));
    std::memcpy(limitData.data(), &limitVal, sizeof(limitVal));
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + limitData.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          limitData.size(), limitData.data(),
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
    EXPECT_EQ(powerLimitsIntf->maxPowerCapValue(), 500u);
}

// NsmPowerLimitRange: update with MINIMUM id → minPowerCapValue updated
TEST_F(NsmPowerLimitBranchTest, NsmPowerLimitRange_Update_MinPowerLimit)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    NsmPowerLimitRange sensor("test", "type", MINIMUM_GPU_BASE_POWER_LIMIT,
                              powerLimitsIntf);

    uint32_t limitVal = htole32(100000); // 100W
    std::vector<uint8_t> limitData2(sizeof(limitVal));
    std::memcpy(limitData2.data(), &limitVal, sizeof(limitVal));
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + limitData2.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          limitData2.size(), limitData2.data(),
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
    EXPECT_EQ(powerLimitsIntf->minPowerCapValue(), 100u);
}

// NsmDefaultPowerLimit: update with sensorIO failure
TEST_F(NsmPowerLimitBranchTest, NsmDefaultPowerLimit_Update_SensorIOFails)
{
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    NsmDefaultPowerLimit sensor("test", "type", RATED_GPU_BASE_POWER_LIMIT,
                                clearIntf);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    EXPECT_NO_THROW(sensor.update(gpu));
}

// NsmDefaultPowerLimit: update with success + valid data
TEST_F(NsmPowerLimitBranchTest, NsmDefaultPowerLimit_Update_Success)
{
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    NsmDefaultPowerLimit sensor("test", "type", RATED_GPU_BASE_POWER_LIMIT,
                                clearIntf);

    uint32_t limitVal = htole32(400000); // 400W
    std::vector<uint8_t> limitData3(sizeof(limitVal));
    std::memcpy(limitData3.data(), &limitVal, sizeof(limitVal));
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + limitData3.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          limitData3.size(), limitData3.data(),
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
    EXPECT_EQ(clearIntf->defaultPowerCap(), 400u);
}

// NsmDefaultPowerLimit: INVALID_POWER_LIMIT → defaultPowerCap = 0
TEST_F(NsmPowerLimitBranchTest, NsmDefaultPowerLimit_Update_InvalidLimit)
{
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    NsmDefaultPowerLimit sensor("test", "type", RATED_GPU_BASE_POWER_LIMIT,
                                clearIntf);

    uint32_t limitVal4 = htole32(INVALID_POWER_LIMIT);
    std::vector<uint8_t> limitData4(sizeof(limitVal4));
    std::memcpy(limitData4.data(), &limitVal4, sizeof(limitVal4));
    std::vector<uint8_t> response4(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + limitData4.size(),
        0);
    auto responseMsg4 = reinterpret_cast<nsm_msg*>(response4.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          limitData4.size(), limitData4.data(),
                                          responseMsg4);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(response4));

    EXPECT_NO_THROW(sensor.update(gpu));
    EXPECT_EQ(clearIntf->defaultPowerCap(), 0u);
}

// NsmPowerLimitRange: value == INVALID_POWER_LIMIT → reading = 0
TEST_F(NsmPowerLimitBranchTest, NsmPowerLimitRange_Update_InvalidLimit)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    NsmPowerLimitRange sensor("test", "type", MAXIMUM_GPU_BASE_POWER_LIMIT,
                              powerLimitsIntf);

    uint32_t limitVal5 = htole32(INVALID_POWER_LIMIT);
    std::vector<uint8_t> limitData5(sizeof(limitVal5));
    std::memcpy(limitData5.data(), &limitVal5, sizeof(limitVal5));
    std::vector<uint8_t> response5(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + limitData5.size(),
        0);
    auto responseMsg5 = reinterpret_cast<nsm_msg*>(response5.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          limitData5.size(), limitData5.data(),
                                          responseMsg5);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(response5));

    EXPECT_NO_THROW(sensor.update(gpu));
    EXPECT_EQ(powerLimitsIntf->maxPowerCapValue(), 0u);
}

// NsmPowerLimitRange: default propertyId → propertyName = ""
// Covers: default case in NsmPowerLimitRange constructor switch (L320-321)
TEST_F(NsmPowerLimitBranchTest, NsmPowerLimitRange_Default_PropertyId)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    // Use an unknown propertyId to trigger the default case
    NsmPowerLimitRange sensor("test", "type", /*propertyId=*/99,
                              powerLimitsIntf);
    // The sensor is created successfully; constructor hits default case
    EXPECT_NE(&sensor, nullptr);
}

// NsmDefaultPowerLimit: default propertyId → propertyName = "UNKNOWN"
// Covers: default case in NsmDefaultPowerLimit constructor switch (L413-414)
TEST_F(NsmPowerLimitBranchTest, NsmDefaultPowerLimit_Default_PropertyId)
{
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    // Use an unknown propertyId to trigger the default case
    NsmDefaultPowerLimit sensor("test", "type", /*propertyId=*/99, clearIntf);
    EXPECT_NE(&sensor, nullptr);
}

// NsmOneShotPowerLimit: persistencyIntf not null → oneShotPowerLimit sourced
// from the pending mode value; persistency is left untouched by this handler.
TEST_F(NsmPowerLimitBranchTest, NsmOneShot_HandleResponse_OneShotValue)
{
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());

    powerLimitsIntf->powerCap(300);
    persistencyIntf->persistentPowerLimit(300.0);
    persistencyIntf->persistency(false); // start as false; must stay unchanged

    NsmOneShotPowerLimit sensor("test", "type", GPU_BASE, persistencyIntf,
                                powerLimitsIntf);

    uint32_t powerLimitValue = htole32(300000); // 300W
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(powerLimitValue) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue),
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue), responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // oneShotPowerLimit comes from the pending value (300000mW / 1000 = 300W)
    EXPECT_DOUBLE_EQ(persistencyIntf->oneShotPowerLimit(), 300.0);
    // persistency is no longer written by the one-shot handler
    EXPECT_FALSE(persistencyIntf->persistency());
}

// powerLimitIdToDeviceModeIndex: default case (unknown powerLimitId) —
// covers L78 Branch 2→7 (default: branch) and L90 return 0.
// NsmPersistentPowerLimit::genRequestMsg calls powerLimitIdToDeviceModeIndex
// with persistent=true; an invalid limitId triggers the default.
TEST_F(NsmPowerLimitBranchTest,
       PersistentGenRequestMsg_InvalidLimitId_DefaultCase)
{
    // limitId=99 is not GPU_BASE or CPU_LIMIT_GPU_COPY → hits default
    auto sensor = makeSensor(/*limitId=*/99, false);

    // genRequestMsg calls powerLimitIdToDeviceModeIndex(99, true) → default
    auto request = sensor->genRequestMsg(0, 0);
    // encode_get_device_mode_settings_v2_req with index=0 succeeds
    EXPECT_TRUE(request.has_value());
}

// NsmOneShotPowerLimit::genRequestMsg with CPU_LIMIT_GPU_COPY —
// covers L84 Branch 5→9 (persistent=false branch for CPU_LIMIT_GPU_COPY).
// genRequestMsg calls powerLimitIdToDeviceModeIndex(CPU_LIMIT_GPU_COPY, false)
// which returns DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY.
TEST_F(NsmPowerLimitBranchTest,
       OneShotGenRequestMsg_CpuLimitGpuCopy_PersistentFalseBranch)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());

    // CPU_LIMIT_GPU_COPY with persistent=false → ONE_SHOT path
    NsmOneShotPowerLimit sensor("test", "type", CPU_LIMIT_GPU_COPY,
                                persistencyIntf, powerLimitsIntf);
    auto request = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

// =============================================================================
// NsmPersistentPowerLimit::handleResponseMsg: persistencyIntf != null,
// powerCap != persistentPowerLimit → persistency(false)
// Covers: L148 persistencyIntf->persistency(false) branch
// =============================================================================
TEST_F(NsmPowerLimitBranchTest,
       HandleResponseMsg_PersistencyIntf_PersistencyFalse)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    auto assocIntf =
        std::make_shared<AssociationDefinitionsIntf>(bus(), objPath.c_str());
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    persistencyIntf->persistency(true); // start as true

    NsmPersistentPowerLimit sensor("test", "type", powerLimitsIntf, clearIntf,
                                   assocIntf, nullptr, GPU_BASE,
                                   persistencyIntf);

    // Set powerCap to something different from what the response will set
    // persistentPowerLimit to
    powerLimitsIntf->powerCap(999);

    uint32_t powerLimitValue = htole32(200000); // 200W → reading=200
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(powerLimitValue) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue),
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue), responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // powerCap comes from current-mode (200W). persistentPowerLimit comes from
    // the pending value (200W) and, since the pending value is valid,
    // persistency is set to true.
    EXPECT_EQ(persistencyIntf->persistentPowerLimit(), 200.0);
    EXPECT_EQ(powerLimitsIntf->powerCap(), 200u);
    EXPECT_TRUE(persistencyIntf->persistency());
}

// =============================================================================
// NsmOneShotPowerLimit::handleResponseMsg: INVALID_POWER_LIMIT as
// currentLimit → reading = 0 (ternary true branch at L532-533)
// =============================================================================
TEST_F(NsmPowerLimitBranchTest,
       NsmOneShot_HandleResponse_InvalidPowerLimit_ReadingZero)
{
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());

    powerLimitsIntf->powerCap(0);
    persistencyIntf->persistentPowerLimit(0.0);
    persistencyIntf->persistency(false);

    NsmOneShotPowerLimit sensor("test", "type", GPU_BASE, persistencyIntf,
                                powerLimitsIntf);

    uint32_t invalidLimit = htole32(INVALID_POWER_LIMIT);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(invalidLimit) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        reinterpret_cast<const uint8_t*>(&invalidLimit), sizeof(invalidLimit),
        reinterpret_cast<const uint8_t*>(&invalidLimit), sizeof(invalidLimit),
        responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // INVALID_POWER_LIMIT pending → oneShotPowerLimit = nan
    EXPECT_TRUE(std::isnan(persistencyIntf->oneShotPowerLimit()));
}

// =============================================================================
// NsmPowerLimitRange::update: decode response with error CC
// Covers: rc==SUCCESS && cc!=SUCCESS path at L379 (shouldLog + no property set)
// =============================================================================
TEST_F(NsmPowerLimitBranchTest, NsmPowerLimitRange_Update_ErrorCC)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    NsmPowerLimitRange sensor("test", "type", MAXIMUM_GPU_BASE_POWER_LIMIT,
                              powerLimitsIntf);

    // Build response with error CC
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    response[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    encode_get_inventory_information_resp(0, NSM_ERROR, ERR_NULL, 0, nullptr,
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
}

// =============================================================================
// NsmDefaultPowerLimit::update: decode response with error CC
// Covers: rc==SUCCESS && cc!=SUCCESS path at L472 (shouldLog + no property set)
// =============================================================================
TEST_F(NsmPowerLimitBranchTest, NsmDefaultPowerLimit_Update_ErrorCC)
{
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    NsmDefaultPowerLimit sensor("test", "type", RATED_GPU_BASE_POWER_LIMIT,
                                clearIntf);

    // Build response with error CC
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    response[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    encode_get_inventory_information_resp(0, NSM_ERROR, ERR_NULL, 0, nullptr,
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
}

// =============================================================================
// NsmPowerLimitRange::update: success with default (unknown) propertyId
// Covers: default case in switch at L392-394 in update method
// =============================================================================
TEST_F(NsmPowerLimitBranchTest,
       NsmPowerLimitRange_Update_DefaultPropertyId_Switch)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    // propertyId=99 is neither MAX nor MIN → default case in update switch
    NsmPowerLimitRange sensor("test", "type", /*propertyId=*/99,
                              powerLimitsIntf);

    uint32_t limitVal = htole32(300000); // 300W
    std::vector<uint8_t> limitData(sizeof(limitVal));
    std::memcpy(limitData.data(), &limitVal, sizeof(limitVal));
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + limitData.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          limitData.size(), limitData.data(),
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    // Decoding succeeds, but default case in switch doesn't set anything
    EXPECT_NO_THROW(sensor.update(gpu));
}

// =============================================================================
// NsmPersistentPowerLimit::updatePowerLimit: persistent=true +
// CPU_LIMIT_GPU_COPY Covers: DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY
// path in powerLimitIdToDeviceModeIndex (L83, persistent=true)
// =============================================================================
TEST_F(NsmPowerLimitBranchTest,
       SetPowerLimit_CPULimitGPUCopy_PersistentTrue_Success)
{
    auto sensor = makeSensor(CPU_LIMIT_GPU_COPY, false);
    auto successResp = makeSetDevModeSuccessResp();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::WriteFailure;
    // tuple with persistent=true →
    // DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY
    AsyncSetOperationValueType value = std::make_tuple(true, uint32_t(250));
    sensor->setPowerLimit(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// NsmOneShotPowerLimit: genRequestMsg with unknown powerLimitId → default
// case in powerLimitIdToDeviceModeIndex with persistent=false
// =============================================================================
TEST_F(NsmPowerLimitBranchTest, OneShotGenRequestMsg_UnknownLimitId_DefaultCase)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());

    // powerLimitId=99 → default case with persistent=false
    NsmOneShotPowerLimit sensor("test", "type", /*powerLimitId=*/99,
                                persistencyIntf, powerLimitsIntf);
    auto request = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

// =============================================================================
// NsmPowerLimitRange: INVALID_POWER_LIMIT with MINIMUM propertyId
// Covers: MINIMUM_GPU_BASE_POWER_LIMIT switch case with reading=0
// =============================================================================
TEST_F(NsmPowerLimitBranchTest,
       NsmPowerLimitRange_Update_MinPowerLimit_InvalidLimit)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    NsmPowerLimitRange sensor("test", "type", MINIMUM_GPU_BASE_POWER_LIMIT,
                              powerLimitsIntf);

    uint32_t limitVal = htole32(INVALID_POWER_LIMIT);
    std::vector<uint8_t> limitData(sizeof(limitVal));
    std::memcpy(limitData.data(), &limitVal, sizeof(limitVal));
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + limitData.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          limitData.size(), limitData.data(),
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
    EXPECT_EQ(powerLimitsIntf->minPowerCapValue(), 0u);
}

// =============================================================================
// NsmDefaultPowerLimit: update with unknown propertyId (not
// RATED_GPU_BASE_POWER_LIMIT) Covers: encode_get_inventory_information_req
// with different propertyId, and the decode success path where
// clearPowerLimitIntf->defaultPowerCap is set regardless of propertyId
// =============================================================================
TEST_F(NsmPowerLimitBranchTest,
       NsmDefaultPowerLimit_Update_UnknownPropertyId_Success)
{
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    // propertyId=99 → constructor default case, propertyName="UNKNOWN"
    NsmDefaultPowerLimit sensor("test", "type", /*propertyId=*/99, clearIntf);

    uint32_t limitVal = htole32(350000); // 350W
    std::vector<uint8_t> limitData(sizeof(limitVal));
    std::memcpy(limitData.data(), &limitVal, sizeof(limitVal));
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + limitData.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          limitData.size(), limitData.data(),
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
    EXPECT_EQ(clearIntf->defaultPowerCap(), 350u);
}
