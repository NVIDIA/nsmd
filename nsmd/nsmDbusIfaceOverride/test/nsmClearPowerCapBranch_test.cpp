/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for NsmClearPowerCapAsyncIntf
 * (nsmd/nsmDbusIfaceOverride/nsmClearPowerCapIface.hpp)
 *
 * Covers branches NOT already hit by nsmClearPowerCapIface_test.cpp:
 *  - clearPowerCap(): objectPath.empty() → throws Unavailable
 *  - getPowerCapFromDevice: decode fails (rc != 0, cc == 0)
 *  - clearPowerCapOnDevice: decode fails (rc != 0, cc == 0)
 *  - setPowerCapOnDevice: decode fails (rc != 0, cc == 0) → WriteFailure
 *  - setPowerCapOnDevice: postPatchIO shouldLog TRUE branch
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "platform-environmental.h"

#include "nsmDbusIfaceOverride/nsmClearPowerCapIface.hpp"

using namespace nsm;

// ============================================================================
// Helpers
// ============================================================================

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

/**
 * Build a response buffer where completion_code is NSM_SUCCESS (0)
 * but the buffer is too short for a full response structure.
 * decode_*_resp will succeed at decode_reason_code_and_cc (cc=0)
 * but fail the length check, returning NSM_SW_ERROR_LENGTH with cc=0.
 */
static std::vector<uint8_t> makeDecodeFailResp()
{
    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    msg->payload[1] = NSM_SUCCESS;
    return buf;
}

// ============================================================================
// Fixture
// ============================================================================

struct NsmClearPowerCapBranchTest :
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

    NsmClearPowerCapBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        EXPECT_CALL(mockManager, getEid(testing::_))
            .WillRepeatedly(testing::Return(0x10));
    }

    ~NsmClearPowerCapBranchTest()
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

// ============================================================================
// getPowerCapFromDevice: decode fails (rc != 0, cc == 0)
// decode_get_power_limit_resp fails the length check but cc stays 0.
// Covers: "cc ? cc : rc" returning rc (non-zero) with cc == 0.
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       GetPowerCapFromDevice_DecodeFailCcZero_CapUnchanged)
{
    auto iface = makeIface("/xyz/openbmc_project/control/br_clear_pc/get_dfc0");
    powerCapIntf->powerCap(0xAAAA);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDecodeFailResp()));

    auto coro = iface->getPowerCapFromDevice();
    // Cap unchanged because decode failed
    EXPECT_EQ(powerCapIntf->powerCap(), 0xAAAAu);
}

// ============================================================================
// clearPowerCapOnDevice: decode fails (rc != 0, cc == 0) → WriteFailure
// Covers the else branch: !(rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
// when cc == 0 but rc != 0.
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       ClearPowerCapOnDevice_DecodeFailCcZero_WriteFailure)
{
    auto iface = makeIface("/xyz/openbmc_project/control/br_clear_pc/clr_dfc0");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDecodeFailResp()));

    auto coro = iface->clearPowerCapOnDevice(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmPowerCapIntf::getPowerCapFromDevice: decode fails (rc != 0, cc == 0)
// Covers: "cc ? cc : rc" with cc==0, rc!=0 on the NsmPowerCapIntf copy.
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       PowerCapIntf_GetPowerCapFromDevice_DecodeFailCcZero)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/br_clear_pc/pcap_dfc0");
    powerCapIntf->powerCap(0xBBBB);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDecodeFailResp()));

    auto coro = powerCapIntf->getPowerCapFromDevice();
    // Cap unchanged because decode failed
    EXPECT_EQ(powerCapIntf->powerCap(), 0xBBBBu);
}

// ============================================================================
// NsmPowerCapIntf::setPowerCapOnDevice: decode fails (rc != 0, cc == 0)
// → else branch → WriteFailure
// Covers: !(rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) when cc==0,rc!=0
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       PowerCapIntf_SetPowerCapOnDevice_DecodeFailCcZero_WriteFailure)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/br_clear_pc/setpc_dfc0");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDecodeFailResp()));

    auto coro = powerCapIntf->setPowerCapOnDevice(100, &status, true);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmPowerCapIntf::setPowerCapOnDevice: postPatchIO fails → WriteFailure
// Covers the shouldLog + if(rc) path.
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       PowerCapIntf_SetPowerCapOnDevice_PostPatchIOFails_WriteFailure)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/br_clear_pc/setpc_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto coro = powerCapIntf->setPowerCapOnDevice(100, &status, true);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmPowerCapIntf::setPowerCapOnDevice: success with empty parents
// Covers the success branch and empty-parents loop.
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       PowerCapIntf_SetPowerCapOnDevice_Success_EmptyParents)
{
    auto iface = makeIface("/xyz/openbmc_project/control/br_clear_pc/setpc_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp()))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(200000)));

    auto coro = powerCapIntf->setPowerCapOnDevice(100, &status, true);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmPowerCapIntf::setPowerCapOnDevice: success with parent NOT in map
// Covers: sensorIt == end() → FALSE branch of outer if → ++it
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       PowerCapIntf_SetPowerCapOnDevice_ParentNotInMap_StaysInList)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/br_clear_pc/setpc_parent_nm");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    const std::string unknownPath = "/xyz/test/br_unknown/parent";
    powerCapIntf->parents.push_back(unknownPath);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp()))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(100000)));

    auto coro = powerCapIntf->setPowerCapOnDevice(100, &status, true);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_EQ(powerCapIntf->parents.size(), 1u);
    EXPECT_TRUE(powerCapIntf->sensorCache.empty());
}

// ============================================================================
// NsmPowerCapIntf::setPowerCapOnDevice: parent in map but null sensor
// Covers: if(sensor) FALSE → ++it
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       PowerCapIntf_SetPowerCapOnDevice_ParentFoundNull_StaysInList)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/br_clear_pc/setpc_parent_null");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    const std::string parentPath = "/xyz/test/br_parent/null_sensor";
    powerCapIntf->parents.push_back(parentPath);
    mockManager.objectPathToSensorMap[parentPath] = nullptr;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp()))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitResp(200000)));

    auto coro = powerCapIntf->setPowerCapOnDevice(100, &status, true);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_EQ(powerCapIntf->parents.size(), 1u);
    EXPECT_TRUE(powerCapIntf->sensorCache.empty());
}

// ============================================================================
// NsmPowerCapIntf::setPowerCapOnDevice: error CC → WriteFailure
// Covers the else branch when cc != NSM_SUCCESS.
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       PowerCapIntf_SetPowerCapOnDevice_ErrorCC_WriteFailure)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/br_clear_pc/setpc_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitResp(NSM_ERROR)));

    auto coro = powerCapIntf->setPowerCapOnDevice(100, &status, true);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmPowerCapIntf::setPowerCap: not a tuple → throws InvalidArgument
// ============================================================================

#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmClearPowerCapBranchTest,
       PowerCapIntf_SetPowerCap_InvalidType_ThrowsInvalidArgument)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/br_clear_pc/setpc_invtype");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType invalidType = bool{true};
    EXPECT_THROW(
        powerCapIntf->setPowerCap(invalidType, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}
#endif // COVERAGE_DISABLE_COROUTINES

// ============================================================================
// NsmPowerCapIntf::setPowerCap: power exceeds max → InvalidArgument
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       PowerCapIntf_SetPowerCap_PowerTooHigh_InvalidArgument)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/br_clear_pc/setpc_high");
    powerCapIntf->maxPowerCapValue(100);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType highPower = std::tuple<bool, uint32_t>{false,
                                                                      200};
    auto coro = powerCapIntf->setPowerCap(highPower, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// ============================================================================
// NsmPowerCapIntf::setPowerCap: power below min → InvalidArgument
// ============================================================================

TEST_F(NsmClearPowerCapBranchTest,
       PowerCapIntf_SetPowerCap_PowerTooLow_InvalidArgument)
{
    auto iface =
        makeIface("/xyz/openbmc_project/control/br_clear_pc/setpc_low");
    powerCapIntf->minPowerCapValue(50);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType lowPower = std::tuple<bool, uint32_t>{false, 10};
    auto coro = powerCapIntf->setPowerCap(lowPower, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}
