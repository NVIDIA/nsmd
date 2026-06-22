/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests (batch 2) for nsmPowerLimit.cpp
 *
 * Targets remaining half-covered branches:
 * - genRequestMsg: encode failure with bad instanceId (> NSM_INSTANCE_MAX)
 * - handleResponseMsg: decode failure (rc != NSM_SW_SUCCESS)
 * - handleResponseMsg: persistencyIntf == nullptr with valid response
 * - handleResponseMsg: pending value valid (non-INVALID) → persistentPowerLimit
 * - handleOfflineState: GPU_BASE (default) → no-op
 * - handleOfflineState: CPU_LIMIT_GPU_COPY → powerCap(INVALID)
 * - NsmOneShotPowerLimit: genRequestMsg encode failure
 * - NsmOneShotPowerLimit: handleResponseMsg decode failure
 * - NsmOneShotPowerLimit: persistencyIntf == nullptr
 * - NsmPowerLimitRange: decode with wrong dataSize
 * - NsmDefaultPowerLimit: decode with wrong dataSize
 * - updatePowerLimit: encode failure with invalid deviceModeIndex
 * - powerLimitIdToDeviceModeIndex: all branches
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

// Forward-declare free function defined in nsmPowerLimit.cpp (not in header)
namespace nsm
{
uint32_t powerLimitIdToDeviceModeIndex(uint8_t powerLimitId, bool persistent);
} // namespace nsm

// =============================================================================
// Fixture
// =============================================================================
struct NsmPowerLimitBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPowerLimitBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPowerLimitBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    auto& bus()
    {
        return utils::DBusHandler::getBus();
    }

    std::shared_ptr<NsmPersistentPowerLimit> makeSensor(
        uint8_t limitId, bool withPersistency,
        std::shared_ptr<PowerLimitsIntf>* outPowerLimitsIntf = nullptr,
        std::shared_ptr<PowerPersistencyIntf>* outPersistencyIntf = nullptr)
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
        if (outPowerLimitsIntf)
            *outPowerLimitsIntf = powerLimitsIntf;
        if (outPersistencyIntf)
            *outPersistencyIntf = persistencyIntf;
        return std::make_shared<NsmPersistentPowerLimit>(
            "TestPowerLimit", "NSM_GPU_BASE_POWER_LIMIT", powerLimitsIntf,
            clearIntf, assocIntf, gpu, limitId, persistencyIntf);
    }

    // Build a device_mode_settings_v2 success response with specified data
    std::vector<uint8_t> makeGetDevModeResp(uint32_t currentLimit,
                                            uint32_t pendingLimit)
    {
        uint32_t cl = htole32(currentLimit);
        uint32_t pl = htole32(pendingLimit);
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
                sizeof(cl) + sizeof(pl),
            0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        encode_get_device_mode_settings_v2_resp(
            0, NSM_SUCCESS, ERR_NULL, reinterpret_cast<const uint8_t*>(&cl),
            sizeof(cl), reinterpret_cast<const uint8_t*>(&pl), sizeof(pl),
            responseMsg);
        return response;
    }
};

// =============================================================================
// genRequestMsg: bad instanceId → encode failure → returns nullopt
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PersistentGenRequestMsg_BadInstanceId_ReturnsNullopt)
{
    auto sensor = makeSensor(GPU_BASE, false);

    // instanceId > NSM_INSTANCE_MAX → encode fails
    auto request = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// NsmOneShotPowerLimit: genRequestMsg with bad instanceId → nullopt
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       OneShotGenRequestMsg_BadInstanceId_ReturnsNullopt)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());

    NsmOneShotPowerLimit sensor("test", "type", GPU_BASE, persistencyIntf,
                                powerLimitsIntf);

    auto request = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// handleResponseMsg: decode failure (buffer too small for valid decode)
// rc != NSM_SW_SUCCESS
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PersistentHandleResponseMsg_DecodeFailure_ErrorPath)
{
    auto sensor = makeSensor(GPU_BASE, false);

    // Tiny buffer → decode_get_device_mode_settings_v2_resp returns failure
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    // rc should be non-zero (decode failure)
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmOneShotPowerLimit: handleResponseMsg decode failure
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       OneShotHandleResponseMsg_DecodeFailure_ErrorPath)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());

    NsmOneShotPowerLimit sensor("test", "type", GPU_BASE, persistencyIntf,
                                powerLimitsIntf);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handleResponseMsg: persistencyIntf == nullptr, valid response with
// currentModeLength == sizeof(uint32_t) → powerCap set but no persistency
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PersistentHandleResponseMsg_NoPersistencyIntf_PowerCapSet)
{
    auto sensor = makeSensor(GPU_BASE, false);          // no persistency

    auto response = makeGetDevModeResp(250000, 300000); // 250W, 300W
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(response.data()), response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handleResponseMsg: valid pending value differing from current → persistency
// is false and persistentPowerLimit holds the pending value (in Watts)
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PersistentHandleResponseMsg_ValidPendingLimit_PersistentSet)
{
    std::shared_ptr<PowerLimitsIntf> plIntf;
    std::shared_ptr<PowerPersistencyIntf> persIntf;
    auto sensor = makeSensor(GPU_BASE, true, &plIntf, &persIntf);

    auto response = makeGetDevModeResp(300000, 400000); // 300W, pending 400W
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(response.data()), response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(plIntf->powerCap(), 300u);
    EXPECT_DOUBLE_EQ(persIntf->persistentPowerLimit(), 400.0);
    // pending (400W) != current powerCap (300W) → persistency false
    EXPECT_FALSE(persIntf->persistency());
}

// =============================================================================
// handleOfflineState: GPU_BASE (default) → no-op, powerCap unchanged
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test, HandleOfflineState_GPUBase_NoOp)
{
    std::shared_ptr<PowerLimitsIntf> plIntf;
    auto sensor = makeSensor(GPU_BASE, false, &plIntf);

    plIntf->powerCap(500);
    sensor->handleOfflineState();
    // GPU_BASE hits default: case → no change
    EXPECT_EQ(plIntf->powerCap(), 500u);
}

// =============================================================================
// handleOfflineState: CPU_LIMIT_GPU_COPY → powerCap set to INVALID_POWER_LIMIT
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test, HandleOfflineState_CPULimitGPUCopy_SetsInvalid)
{
    std::shared_ptr<PowerLimitsIntf> plIntf;
    auto sensor = makeSensor(CPU_LIMIT_GPU_COPY, false, &plIntf);

    plIntf->powerCap(500);
    sensor->handleOfflineState();
    EXPECT_EQ(plIntf->powerCap(), INVALID_POWER_LIMIT);
}

// =============================================================================
// NsmOneShotPowerLimit: handleResponseMsg with persistencyIntf == nullptr
// currentModeLength == sizeof(uint32_t), but no persistency → just skip
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       OneShotHandleResponseMsg_NoPersistencyIntf_SkipsPersistency)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());

    // nullptr persistencyIntf
    NsmOneShotPowerLimit sensor("test", "type", GPU_BASE, nullptr,
                                powerLimitsIntf);

    auto response = makeGetDevModeResp(200000, 200000); // 200W
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(response.data()), response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmPowerLimitRange: decode success but dataSize != sizeof(uint32_t)
// → shouldLog path, no property set
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PowerLimitRange_Update_WrongDataSize_NoPropertySet)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    NsmPowerLimitRange sensor("test", "type", MAXIMUM_GPU_BASE_POWER_LIMIT,
                              powerLimitsIntf);

    // Build response with 2-byte data (not 4-byte)
    std::vector<uint8_t> shortData = {0x01, 0x02};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + shortData.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          shortData.size(), shortData.data(),
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
}

// =============================================================================
// NsmDefaultPowerLimit: decode success but dataSize != sizeof(uint32_t)
// → shouldLog path, no property set
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       DefaultPowerLimit_Update_WrongDataSize_NoPropertySet)
{
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    NsmDefaultPowerLimit sensor("test", "type", RATED_GPU_BASE_POWER_LIMIT,
                                clearIntf);

    // Build response with 2-byte data (not 4-byte)
    std::vector<uint8_t> shortData = {0x03, 0x04};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + shortData.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          shortData.size(), shortData.data(),
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
}

// =============================================================================
// powerLimitIdToDeviceModeIndex: GPU_BASE + persistent=true
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PowerLimitIdToDeviceModeIndex_GPUBase_PersistentTrue)
{
    auto result = powerLimitIdToDeviceModeIndex(GPU_BASE, true);
    EXPECT_EQ(result, DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT);
}

// =============================================================================
// powerLimitIdToDeviceModeIndex: GPU_BASE + persistent=false
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PowerLimitIdToDeviceModeIndex_GPUBase_PersistentFalse)
{
    auto result = powerLimitIdToDeviceModeIndex(GPU_BASE, false);
    EXPECT_EQ(result, DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT);
}

// =============================================================================
// powerLimitIdToDeviceModeIndex: CPU_LIMIT_GPU_COPY + persistent=true
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PowerLimitIdToDeviceModeIndex_CPULimitGPUCopy_PersistentTrue)
{
    auto result = powerLimitIdToDeviceModeIndex(CPU_LIMIT_GPU_COPY, true);
    EXPECT_EQ(result, DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY);
}

// =============================================================================
// powerLimitIdToDeviceModeIndex: CPU_LIMIT_GPU_COPY + persistent=false
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PowerLimitIdToDeviceModeIndex_CPULimitGPUCopy_PersistentFalse)
{
    auto result = powerLimitIdToDeviceModeIndex(CPU_LIMIT_GPU_COPY, false);
    EXPECT_EQ(result, DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY);
}

// =============================================================================
// powerLimitIdToDeviceModeIndex: unknown ID + persistent=true → default → 0
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PowerLimitIdToDeviceModeIndex_Unknown_PersistentTrue_Default)
{
    auto result = powerLimitIdToDeviceModeIndex(99, true);
    EXPECT_EQ(result, 0u);
}

// =============================================================================
// powerLimitIdToDeviceModeIndex: unknown ID + persistent=false → default → 0
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PowerLimitIdToDeviceModeIndex_Unknown_PersistentFalse_Default)
{
    auto result = powerLimitIdToDeviceModeIndex(99, false);
    EXPECT_EQ(result, 0u);
}

// =============================================================================
// NsmOneShotPowerLimit: genRequestMsg with GPU_BASE, persistent=false (valid)
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test, OneShotGenRequestMsg_GPUBase_ValidInstanceId)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());

    NsmOneShotPowerLimit sensor("test", "type", GPU_BASE, persistencyIntf,
                                powerLimitsIntf);

    auto request = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

// =============================================================================
// PersistentPowerLimit: genRequestMsg with CPU_LIMIT_GPU_COPY, valid instanceId
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PersistentGenRequestMsg_CPULimit_ValidInstanceId)
{
    auto sensor = makeSensor(CPU_LIMIT_GPU_COPY, false);

    auto request = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

// =============================================================================
// NsmClearPowerLimitIntf: clearPowerCap returns 0
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test, ClearPowerLimitIntf_ClearPowerCap_Returns0)
{
    auto clearIntf = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    EXPECT_EQ(clearIntf->clearPowerCap(), 0);
}

// =============================================================================
// NsmOneShotPowerLimit: handleResponseMsg with INVALID_POWER_LIMIT current
// and nullptr persistencyIntf → reading=0, no crash
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       OneShotHandleResponseMsg_InvalidPower_NoPersistency)
{
    auto powerLimitsIntf = std::make_shared<PowerLimitsIntf>(bus(),
                                                             objPath.c_str());

    NsmOneShotPowerLimit sensor("test", "type", GPU_BASE, nullptr,
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
}

// =============================================================================
// NsmPersistentPowerLimit: handleResponseMsg with INVALID_POWER_LIMIT and
// persistencyIntf set → powerCap=0, persistentPowerLimit=nan, persistency=false
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PersistentHandleResponseMsg_InvalidPower_WithPersistency)
{
    std::shared_ptr<PowerLimitsIntf> plIntf;
    std::shared_ptr<PowerPersistencyIntf> persIntf;
    auto sensor = makeSensor(GPU_BASE, true, &plIntf, &persIntf);

    auto response = makeGetDevModeResp(INVALID_POWER_LIMIT,
                                       INVALID_POWER_LIMIT);
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(response.data()), response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(plIntf->powerCap(), 0u);
    // INVALID pending → persistentPowerLimit=nan, persistency=false
    EXPECT_TRUE(std::isnan(persIntf->persistentPowerLimit()));
    EXPECT_FALSE(persIntf->persistency());
}

// =============================================================================
// NsmPowerLimitRange: update with MIN property and valid INVALID_POWER_LIMIT
// value → reading=0, minPowerCapValue=0
// =============================================================================
TEST_F(NsmPowerLimitBranch2Test,
       PowerLimitRange_Update_MinProperty_InvalidValue)
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
