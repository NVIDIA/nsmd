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

#include "test/mockSensorManager.hpp"

#include <xyz/openbmc_project/Common/Device/error.hpp>
using namespace ::testing;

#define private public
#define protected public

#include "diagnostics.h"

#include "nsmSetWriteProtected.hpp"

struct NsmSetWriteProtectedTest : public testing::Test, public SensorManagerTest
{
    const uuid_t fpgaUuid = "992b3ec1-e464-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> fpga =
        std::make_shared<MockNsmDevice>(0, 0, "MCTP_UUID", fpgaUuid, 0);
    NsmDeviceTable devices{{fpga}};

    std::unique_ptr<NsmSetWriteProtected> writeProtectedIntf;
    NsmSetWriteProtectedTest() : SensorManagerTest(devices) {}

    ~NsmSetWriteProtectedTest()
    {
        cleanupDeviceSensors(devices);
    }

    void init(const diagnostics_enable_disable_wp_data_index dataIndex)
    {
        writeProtectedIntf = std::make_unique<NsmSetWriteProtected>(
            "TEST", mockManager, dataIndex,
            (firmwareInventoryBasePath / "HGX_FW_TEST_DEV").string());
    }

    const Response enableDisableMsg{
        0x10,
        0xDE,                  // PCI VID: NVIDIA 0x10DE
        0x00,                  // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                  // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_DIAGNOSTIC,   // NVIDIA_MSG_TYPE
        NSM_ENABLE_DISABLE_WP, // command
        0,                     // completion code
        0,
        0,
        0,
        0 // data size
    };

    auto& data()
    {
        return SensorManagerTest::data<nsm_fpga_diagnostics_settings_wp>(0);
    }
    bool writeProtected(bool value, const Response& resp = Response{})
    {
        AsyncOperationStatusType status = AsyncOperationStatusType::Success;
        auto result = writeProtectedIntf->writeProtected(value, &status, fpga);
        if (status == AsyncOperationStatusType::WriteFailure)
        {
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure{};
        }
        lastResponse = resp;
        value = NsmSetWriteProtected::getValue(data(),
                                               writeProtectedIntf->dataIndex);
        return value && result.data() == NSM_SW_SUCCESS &&
               status == AsyncOperationStatusType::Success;
    }
};

TEST_F(NsmSetWriteProtectedTest, badTestBaseboardWrite)
{
    init(HMC_SPI_FLASH);
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg,
                                  NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    EXPECT_THROW(
        writeProtected(true),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);
}

TEST_F(NsmSetWriteProtectedTest, goodTestBaseboardWrite)
{
    init(HMC_SPI_FLASH);
    const Response enabled{0b00,       0x00, 0b00, 0x00,
                           0b00010000, 0x00, 0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().hmc);
}

TEST_F(NsmSetWriteProtectedTest, goodTestRetimer1Write)
{
    init(RETIMER_EEPROM_1);
    const Response enabled{0b00000001, 0x00, 0b00000001, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer1);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer2Write)
{
    init(RETIMER_EEPROM_2);
    const Response enabled{0b00000001, 0x00, 0b00000010, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer2);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer3Write)
{
    init(RETIMER_EEPROM_3);
    const Response enabled{0b00000001, 0x00, 0b00000100, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer3);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer4Write)
{
    init(RETIMER_EEPROM_4);
    const Response enabled{0b00000001, 0x00, 0b00001000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer4);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer5Write)
{
    init(RETIMER_EEPROM_5);
    const Response enabled{0b00000001, 0x00, 0b00010000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer5);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer6Write)
{
    init(RETIMER_EEPROM_6);
    const Response enabled{0b00000001, 0x00, 0b00100000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer6);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer7Write)
{
    init(RETIMER_EEPROM_7);
    const Response enabled{0b00000001, 0x00, 0b01000000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer7);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer8Write)
{
    init(RETIMER_EEPROM_8);
    const Response enabled{0b00000001, 0x00, 0b10000000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer8);
}
TEST_F(NsmSetWriteProtectedTest, goodTestCpu1Write)
{
    init(CPU_SPI_FLASH_1);
    const Response enabled{0x00,       0b00000010, 0x00, 0x00,
                           0b00100000, 0x00,       0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().cpu1_4);
    EXPECT_EQ(1, data().cpu1);
}
TEST_F(NsmSetWriteProtectedTest, goodTestCpu2Write)
{
    init(CPU_SPI_FLASH_2);
    const Response enabled{0x00,       0b00000010, 0x00, 0x00,
                           0b01000000, 0x00,       0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().cpu1_4);
    EXPECT_EQ(1, data().cpu2);
}
TEST_F(NsmSetWriteProtectedTest, goodTestCpu3Write)
{
    init(CPU_SPI_FLASH_3);
    const Response enabled{0x00,       0b00000010, 0x00, 0x00,
                           0b10000000, 0x00,       0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().cpu1_4);
    EXPECT_EQ(1, data().cpu3);
}
TEST_F(NsmSetWriteProtectedTest, goodTestCpu4Write)
{
    init(CPU_SPI_FLASH_4);
    const Response enabled{0x00, 0b00000010, 0x00, 0x00,
                           0x00, 0b00000001, 0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().cpu1_4);
    EXPECT_EQ(1, data().cpu4);
}
TEST_F(NsmSetWriteProtectedTest, goodTestNvSwitch1Write)
{
    init(NVSW_EEPROM_1);
    const Response enabled{0b00001000, 0x00, 0x00, 0b0000001,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().nvSwitch);
    EXPECT_EQ(1, data().nvSwitch1);
}
TEST_F(NsmSetWriteProtectedTest, goodTestNvSwitch2Write)
{
    init(NVSW_EEPROM_2);
    const Response enabled{0b00001000, 0x00, 0x00, 0b0000010,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().nvSwitch);
    EXPECT_EQ(1, data().nvSwitch2);
}
TEST_F(NsmSetWriteProtectedTest, goodTestNvLinkManagementWrite)
{
    init(PEX_SW_EEPROM);
    const Response enabled{0b00000100, 0x00, 0x00, 0x00,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().pex);
}

TEST_F(NsmSetWriteProtectedTest, goodTestGpu1Write)
{
    init(GPU_SPI_FLASH_1);
    const Response enabled{0b10000000, 0x00, 0x00, 0b00010000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu1_4);
    EXPECT_EQ(1, data().gpu1);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu2Write)
{
    init(GPU_SPI_FLASH_2);
    const Response enabled{0b10000000, 0x00, 0x00, 0b00100000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu1_4);
    EXPECT_EQ(1, data().gpu2);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu3Write)
{
    init(GPU_SPI_FLASH_3);
    const Response enabled{0b10000000, 0x00, 0x00, 0b01000000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu1_4);
    EXPECT_EQ(1, data().gpu3);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu4Write)
{
    init(GPU_SPI_FLASH_4);
    const Response enabled{0b10000000, 0x00, 0x00, 0b10000000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu1_4);
    EXPECT_EQ(1, data().gpu4);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu5Write)
{
    init(GPU_SPI_FLASH_5);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00000001, 0x00,       0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu5_8);
    EXPECT_EQ(1, data().gpu5);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu6Write)
{
    init(GPU_SPI_FLASH_6);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00000010, 0x00,       0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu5_8);
    EXPECT_EQ(1, data().gpu6);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu7Write)
{
    init(GPU_SPI_FLASH_7);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00000100, 0x00,       0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu5_8);
    EXPECT_EQ(1, data().gpu7);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu8Write)
{
    init(GPU_SPI_FLASH_8);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00001000, 0x00,       0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu5_8);
    EXPECT_EQ(1, data().gpu8);
}

TEST_F(NsmSetWriteProtectedTest, goodTestGpus9_16Write)
{
    init(GPU_SPI_FLASH);
    const Response enabled{0b10000000, 0b00001101, 0x00,       0b11110000,
                           0b00001111, 0x00,       0b11111111, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu1_4);
    EXPECT_EQ(1, data().gpu5_8);
    EXPECT_EQ(1, data().gpu9_12);
    EXPECT_EQ(1, data().gpu13_16);
    EXPECT_EQ(1, data().gpu1);
    EXPECT_EQ(1, data().gpu2);
    EXPECT_EQ(1, data().gpu3);
    EXPECT_EQ(1, data().gpu4);
    EXPECT_EQ(1, data().gpu5);
    EXPECT_EQ(1, data().gpu6);
    EXPECT_EQ(1, data().gpu7);
    EXPECT_EQ(1, data().gpu8);
    EXPECT_EQ(1, data().gpu9);
    EXPECT_EQ(1, data().gpu10);
    EXPECT_EQ(1, data().gpu11);
    EXPECT_EQ(1, data().gpu12);
    EXPECT_EQ(1, data().gpu13);
    EXPECT_EQ(1, data().gpu14);
    EXPECT_EQ(1, data().gpu15);
    EXPECT_EQ(1, data().gpu16);
}

TEST_F(NsmSetWriteProtectedTest, goodTestCX8Write)
{
    init(CX8_SPI_FLASH);
    const Response enabled{0b00010000, 0x00, 0x00, 0x00,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_SUCCESS));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().cx8);
}

// ---- getValue() direct tests for untested switch cases ----

// default: branch – invalid dataIndex value → returns false
TEST(NsmSetWriteProtectedGetValue, DefaultCase_ReturnsFalse)
{
    nsm_fpga_diagnostics_settings_wp data{};
    // Set every bit so that any valid case would return true
    std::memset(&data, 0xFF, sizeof(data));

    // Cast an out-of-range integer so the switch hits the default branch
    auto invalidIndex =
        static_cast<diagnostics_enable_disable_wp_data_index>(0);
    EXPECT_FALSE(NsmSetWriteProtected::getValue(data, invalidIndex));
}

// BASEBOARD_FRU_EEPROM / CX7_FRU_EEPROM / HMC_FRU_EEPROM → data.baseboard
TEST(NsmSetWriteProtectedGetValue, BaseboardFruEeprom_ReturnsBaseboard)
{
    nsm_fpga_diagnostics_settings_wp data{};
    data.baseboard = 1;
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, BASEBOARD_FRU_EEPROM));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, CX7_FRU_EEPROM));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, HMC_FRU_EEPROM));
}

// RETIMER_EEPROM → data.retimer
TEST(NsmSetWriteProtectedGetValue, RetimerEeprom_ReturnsRetimer)
{
    nsm_fpga_diagnostics_settings_wp data{};
    data.retimer = 1;
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, RETIMER_EEPROM));
}

// NVSW_EEPROM_BOTH → data.nvSwitch
TEST(NsmSetWriteProtectedGetValue, NvswEepromBoth_ReturnsNvSwitch)
{
    nsm_fpga_diagnostics_settings_wp data{};
    data.nvSwitch = 1;
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, NVSW_EEPROM_BOTH));
}

// GPU_1_4_SPI_FLASH → data.gpu1_4; GPU_5_8_SPI_FLASH → data.gpu5_8
TEST(NsmSetWriteProtectedGetValue, Gpu14And58SpiFlash)
{
    nsm_fpga_diagnostics_settings_wp data{};
    data.gpu1_4 = 1;
    data.gpu5_8 = 1;
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_1_4_SPI_FLASH));
    EXPECT_TRUE(NsmSetWriteProtected::getValue(data, GPU_5_8_SPI_FLASH));
}

// GPU_SPI_FLASH compound && FALSE branch: any bit zero → returns false
// Line 62: return data.gpu1_4 && data.gpu5_8 && data.gpu9_12 && data.gpu13_16;
// Branch 11→15 (FALSE path) taken when gpu1_4 == 0
TEST(NsmSetWriteProtectedGetValue, GpuSpiFlash_PartialBits_ReturnsFalse)
{
    nsm_fpga_diagnostics_settings_wp data{};
    // All bits zero → compound && evaluates to false at the first operand
    EXPECT_FALSE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH));
    // One bit set but others zero → still false (short-circuits at gpu5_8=0)
    data.gpu1_4 = 1;
    EXPECT_FALSE(NsmSetWriteProtected::getValue(data, GPU_SPI_FLASH));
}

// Error-CC path: setWriteProtected coroutine – postPatchIO succeeds but
// cc = NSM_ERROR → co_return cc ? cc : rc; takes the true branch. //

TEST_F(NsmSetWriteProtectedTest, SetWriteProtected_ErrorCC_CoversTrueBranch)
{
    init(HMC_SPI_FLASH);

    // Construct response with cc = NSM_ERROR (byte 6 holds completion code)
    Response errorMsg = enableDisableMsg;
    errorMsg[6] = NSM_ERROR;

    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(errorMsg));

    // cc = NSM_ERROR → status = WriteFailure → throw
    EXPECT_THROW(
        writeProtected(true),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);
}

// setWriteProtected(): postPatchIO returns a generic (non-UNSUPPORTED) error →
// enters the inner if(rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE) true branch →
// triggers lg2::error logging at lines 153-156 of nsmSetWriteProtected.cpp. //

TEST_F(NsmSetWriteProtectedTest,
       WriteProtected_PostPatchIOGenericError_LogsErrorAndThrows)
{
    init(HMC_SPI_FLASH);
    // NSM_SW_ERROR is non-zero and ≠ NSM_ERR_UNSUPPORTED_COMMAND_CODE, so the
    // inner logging branch (lines 153-156) is taken before the WriteFailure
    // throw.
    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(enableDisableMsg, NSM_ERROR));
    EXPECT_THROW(
        writeProtected(true),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);
}

// setWriteProtected(): decode returns NSM_SW_SUCCESS but cc != NSM_SUCCESS
// (L168 FALSE: rc==NSM_SW_SUCCESS && cc!=NSM_SUCCESS branch not taken).
// Use 9-byte buffer so decode_reason_code_and_cc returns NSM_SW_SUCCESS.
TEST_F(NsmSetWriteProtectedTest,
       SetWriteProtected_DecodeSuccessNonZeroCC_WriteFailure)
{
    init(HMC_SPI_FLASH);

    // sizeof(nsm_msg_hdr)+sizeof(nsm_common_non_success_resp)=9 bytes:
    // decode_reason_code_and_cc returns NSM_SW_SUCCESS with cc=NSM_ERROR
    Response ccErrResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    ccErrResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // payload[1]=cc

    EXPECT_CALL(*fpga, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(ccErrResp));

    EXPECT_THROW(
        writeProtected(true),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);
}

// writeProtected() receives a non-bool variant value →
// std::get_if<bool>(&value) returns nullptr →
// throws sdbusplus InvalidArgument (line 120 in nsmSetWriteProtected.cpp).
TEST_F(NsmSetWriteProtectedTest,
       WriteProtected_NonBoolValue_ThrowsInvalidArgument)
{
    init(CPU_SPI_FLASH_1);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType nonBool = uint32_t(42);
    EXPECT_THROW_COROUTINE(
        writeProtectedIntf->writeProtected(nonBool, &status, fpga),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}
