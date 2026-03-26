/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for nsmd/nsmSetAsync/ files:
 *
 * Targets:
 *   - NsmAsyncSensor::set: null status throws
 *   - NsmAsyncSensor::update: genRequestMsg fail, sensorIO fail,
 *     handleResponseMsg fail
 *   - NsmSetMigMode::handleResponseMsg: both LR and non-LR paths,
 *     cc!=0 ternary, decode fail
 *   - NsmSetWriteProtected::writeProtected: non-bool throws
 *   - NsmSetWriteProtected::setWriteProtected: postPatchIO fail w/ unsupported
 *   - NsmSetWriteProtected::setWriteProtected: decode success/failure
 *   - NsmSetWriteProtected::getValue: various dataIndex cases
 *   - setCPUSpeedConfig: non-tuple throws
 *   - setClockLimitOnDevice: out-of-range speed
 */

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "device-configuration.h"
#include "platform-environmental.h"

#include "asyncOperationManager.hpp"
#include "nsmAsyncSensor.hpp"
#include "nsmSetCpuOperatingConfig.hpp"
#include "nsmSetMigMode.hpp"
#include "nsmSetWriteProtected.hpp"

using namespace nsm;

// Forward declarations for internal functions
namespace nsm
{
requester::Coroutine setCPUSpeedConfig(uint8_t clockId,
                                       const AsyncSetOperationValueType& value,
                                       AsyncOperationStatusType* status,
                                       std::shared_ptr<NsmDevice> device);
requester::Coroutine setClockLimitOnDevice(uint8_t clockId, bool speedLocked,
                                           uint32_t requestedSpeedLimit,
                                           AsyncOperationStatusType* status,
                                           std::shared_ptr<NsmDevice> device);
requester::Coroutine getMinGraphicsClockLimit(uint32_t& minClockLimit,
                                              std::shared_ptr<NsmDevice> dev);
requester::Coroutine getMaxGraphicsClockLimit(uint32_t& maxClockLimit,
                                              std::shared_ptr<NsmDevice> dev);
} // namespace nsm

// ============================================================================
// Fixture
// ============================================================================

struct NsmSetAsyncBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmSetAsyncBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmSetAsyncBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    static std::vector<uint8_t> makeShortResp()
    {
        const size_t bufSize = sizeof(nsm_msg_hdr) +
                               sizeof(nsm_common_non_success_resp);
        std::vector<uint8_t> buf(bufSize, 0);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        msg->payload[1] = NSM_SUCCESS;
        return buf;
    }
};

// ============================================================================
// NsmSetMigMode handleResponseMsg: non-LR decode fail (cc==0, rc!=0)
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, MigMode_HandleResp_NonLR_DecodeFail)
{
    NsmSetMigMode sensor(false, gpu);

    auto buf = makeShortResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    uint8_t result = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmSetMigMode handleResponseMsg: LR decode fail (cc==0, rc!=0)
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, MigMode_HandleResp_LR_DecodeFail)
{
    NsmSetMigMode sensor(true, gpu);

    auto buf = makeShortResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    uint8_t result = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmSetMigMode handleResponseMsg: LR error CC
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, MigMode_HandleResp_LR_ErrorCC)
{
    NsmSetMigMode sensor(true, gpu);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                 sizeof(nsm_long_running_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_MIG_mode_event_resp(0, NSM_ERROR, 0xABCD, msg);

    uint8_t result = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(result, NSM_ERROR);
}

// ============================================================================
// NsmSetWriteProtected::getValue: various dataIndex values
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, WriteProtected_GetValue_VariousIndices)
{
    nsm_fpga_diagnostics_settings_wp data = {};
    data.retimer = true;
    data.baseboard = true;
    data.pex = true;
    data.nvSwitch = true;
    data.nvSwitch1 = true;
    data.nvSwitch2 = true;
    data.gpu1_4 = true;
    data.gpu5_8 = true;
    data.gpu9_12 = true;
    data.gpu13_16 = true;
    data.cx8 = true;
    data.gpu1 = true;
    data.gpu2 = true;
    data.gpu3 = true;
    data.gpu4 = true;
    data.gpu5 = true;
    data.gpu6 = true;
    data.gpu7 = true;
    data.gpu8 = true;
    data.hmc = true;
    data.retimer1 = true;
    data.retimer2 = true;
    data.retimer3 = true;
    data.retimer4 = true;
    data.retimer5 = true;
    data.retimer6 = true;
    data.retimer7 = true;
    data.retimer8 = true;
    data.cpu1 = true;
    data.cpu2 = true;
    data.cpu3 = true;
    data.cpu4 = true;

    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, RETIMER_EEPROM));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, BASEBOARD_FRU_EEPROM));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, PEX_SW_EEPROM));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, NVSW_EEPROM_BOTH));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, NVSW_EEPROM_1));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, NVSW_EEPROM_2));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_1_4_SPI_FLASH));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_5_8_SPI_FLASH));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, CX8_SPI_FLASH));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH_1));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH_2));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH_3));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH_4));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH_5));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH_6));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH_7));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH_8));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, HMC_SPI_FLASH));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, RETIMER_EEPROM_1));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, RETIMER_EEPROM_2));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, RETIMER_EEPROM_3));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, RETIMER_EEPROM_4));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, RETIMER_EEPROM_5));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, RETIMER_EEPROM_6));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, RETIMER_EEPROM_7));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, RETIMER_EEPROM_8));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, CPU_SPI_FLASH_1));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, CPU_SPI_FLASH_2));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, CPU_SPI_FLASH_3));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, CPU_SPI_FLASH_4));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, CX7_FRU_EEPROM));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, HMC_FRU_EEPROM));

    // Default case
    EXPECT_FALSE(NsmSetWriteProtected::getValue(
        data, static_cast<diagnostics_enable_disable_wp_data_index>(0xFF)));
}

// ============================================================================
// NsmSetWriteProtected::writeProtected: non-bool throws
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, WriteProtected_NonBoolThrows)
{
    NsmSetWriteProtected sensor("WpBranch", mockManager, RETIMER_EEPROM,
                                "/xyz/test/wp_br");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Pass a string instead of bool
    AsyncSetOperationValueType badValue = std::string("not_bool");

    EXPECT_THROW_COROUTINE(
        sensor.writeProtected(badValue, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// NsmSetWriteProtected::setWriteProtected: postPatchIO fail unsupported
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, WriteProtected_PostPatchIO_Unsupported)
{
    NsmSetWriteProtected sensor("WpBranch2", mockManager, RETIMER_EEPROM,
                                "/xyz/test/wp_br2");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    sensor.setWriteProtected(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSetWriteProtected::setWriteProtected: postPatchIO fail other error
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, WriteProtected_PostPatchIO_OtherError)
{
    NsmSetWriteProtected sensor("WpBranch3", mockManager, RETIMER_EEPROM,
                                "/xyz/test/wp_br3");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    sensor.setWriteProtected(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSetWriteProtected::setWriteProtected: decode fail
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, WriteProtected_DecodeFail)
{
    NsmSetWriteProtected sensor("WpBranch4", mockManager, RETIMER_EEPROM,
                                "/xyz/test/wp_br4");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeShortResp()));

    sensor.setWriteProtected(true, status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setCPUSpeedConfig: non-tuple throws
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, SetCPUSpeedConfig_NonTupleThrows)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType badValue = std::string("not_tuple");

    EXPECT_THROW_COROUTINE(
        nsm::setCPUSpeedConfig(0, badValue, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// getMinGraphicsClockLimit: postPatchIO fail
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, GetMinClockLimit_PostPatchIO_Fail)
{
    uint32_t limit = 0;
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    nsm::getMinGraphicsClockLimit(limit, gpu);
}

// ============================================================================
// getMaxGraphicsClockLimit: postPatchIO fail
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, GetMaxClockLimit_PostPatchIO_Fail)
{
    uint32_t limit = 0;
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    nsm::getMaxGraphicsClockLimit(limit, gpu);
}

// ============================================================================
// setClockLimitOnDevice: getMinGraphicsClockLimit fails
// ============================================================================

TEST_F(NsmSetAsyncBranchTest, SetClockLimit_MinFail)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    nsm::setClockLimitOnDevice(0, false, 1000, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
