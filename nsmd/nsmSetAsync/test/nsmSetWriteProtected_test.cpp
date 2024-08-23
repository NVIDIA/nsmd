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
    std::shared_ptr<NsmDevice> fpga = std::make_shared<NsmDevice>(fpgaUuid);
    NsmDeviceTable devices{{fpga}};

    NiceMock<MockSensorManager> mockManager{devices};
    std::unique_ptr<NsmSetWriteProtected> writeProtectedIntf;

    void init(NsmDeviceIdentification deviceType, uint8_t instanceNumber,
              bool retimer = false)
    {
        writeProtectedIntf = std::make_unique<NsmSetWriteProtected>(
            "TEST", mockManager, instanceNumber, deviceType,
            (firmwareInventoryBasePath / "HGX_FW_TEST_DEV").string(), retimer);
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
        value = NsmSetWriteProtected::getValue(
            data(), writeProtectedIntf->deviceType,
            writeProtectedIntf->instanceNumber, writeProtectedIntf->retimer);
        return value && result.await_resume() == NSM_SW_SUCCESS &&
               status == AsyncOperationStatusType::Success;
    }
};

TEST_F(NsmSetWriteProtectedTest, badTestBaseboardWrite)
{
    init(NSM_DEV_ID_BASEBOARD, 0);
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(1)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg, Response(),
                                     NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    EXPECT_THROW(
        writeProtected(true),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);
}

TEST_F(NsmSetWriteProtectedTest, goodTestBaseboardWrite)
{
    init(NSM_DEV_ID_BASEBOARD, 0);
    const Response enabled{0b00,       0x00, 0b00, 0x00,
                           0b00010000, 0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().hmc);
}

TEST_F(NsmSetWriteProtectedTest, goodTestRetimer1Write)
{
    init(NSM_DEV_ID_BASEBOARD, 0, true);
    const Response enabled{0b00000001, 0x00, 0b00000001, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer1);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer2Write)
{
    init(NSM_DEV_ID_BASEBOARD, 1, true);
    const Response enabled{0b00000001, 0x00, 0b00000010, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer2);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer3Write)
{
    init(NSM_DEV_ID_BASEBOARD, 2, true);
    const Response enabled{0b00000001, 0x00, 0b00000100, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer3);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer4Write)
{
    init(NSM_DEV_ID_BASEBOARD, 3, true);
    const Response enabled{0b00000001, 0x00, 0b00001000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer4);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer5Write)
{
    init(NSM_DEV_ID_BASEBOARD, 4, true);
    const Response enabled{0b00000001, 0x00, 0b00010000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer5);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer6Write)
{
    init(NSM_DEV_ID_BASEBOARD, 5, true);
    const Response enabled{0b00000001, 0x00, 0b00100000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer6);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer7Write)
{
    init(NSM_DEV_ID_BASEBOARD, 6, true);
    const Response enabled{0b00000001, 0x00, 0b01000000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer7);
}
TEST_F(NsmSetWriteProtectedTest, goodTestRetimer8Write)
{
    init(NSM_DEV_ID_BASEBOARD, 7, true);
    const Response enabled{0b00000001, 0x00, 0b10000000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().retimer);
    EXPECT_EQ(1, data().retimer8);
}

TEST_F(NsmSetWriteProtectedTest, goodTestNvSwitch1Write)
{
    init(NSM_DEV_ID_SWITCH, 0);
    const Response enabled{0b00001000, 0x00, 0x00, 0b0000001,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().nvSwitch);
    EXPECT_EQ(1, data().nvSwitch1);
}
TEST_F(NsmSetWriteProtectedTest, goodTestNvSwitch2Write)
{
    init(NSM_DEV_ID_SWITCH, 1);
    const Response enabled{0b00001000, 0x00, 0x00, 0b0000010,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().nvSwitch);
    EXPECT_EQ(1, data().nvSwitch2);
}
TEST_F(NsmSetWriteProtectedTest, goodTestNvLinkManagementWrite)
{
    init(NSM_DEV_ID_PCIE_BRIDGE, 0);
    const Response enabled{0b00000100, 0x00, 0x00, 0x00,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().pex);
}

TEST_F(NsmSetWriteProtectedTest, goodTestGpu1Write)
{
    init(NSM_DEV_ID_GPU, 0);
    const Response enabled{0b10000000, 0x00, 0x00, 0b00010000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu1_4);
    EXPECT_EQ(1, data().gpu1);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu2Write)
{
    init(NSM_DEV_ID_GPU, 1);
    const Response enabled{0b10000000, 0x00, 0x00, 0b00100000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu1_4);
    EXPECT_EQ(1, data().gpu2);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu3Write)
{
    init(NSM_DEV_ID_GPU, 2);
    const Response enabled{0b10000000, 0x00, 0x00, 0b01000000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu1_4);
    EXPECT_EQ(1, data().gpu3);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu4Write)
{
    init(NSM_DEV_ID_GPU, 3);
    const Response enabled{0b10000000, 0x00, 0x00, 0b10000000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu1_4);
    EXPECT_EQ(1, data().gpu4);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu5Write)
{
    init(NSM_DEV_ID_GPU, 4);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00000001, 0x00,       0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu5_8);
    EXPECT_EQ(1, data().gpu5);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu6Write)
{
    init(NSM_DEV_ID_GPU, 5);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00000010, 0x00,       0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu5_8);
    EXPECT_EQ(1, data().gpu6);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu7Write)
{
    init(NSM_DEV_ID_GPU, 6);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00000100, 0x00,       0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu5_8);
    EXPECT_EQ(1, data().gpu7);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGpu8Write)
{
    init(NSM_DEV_ID_GPU, 7);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00001000, 0x00,       0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(enableDisableMsg));

    EXPECT_EQ(true, writeProtected(true, enabled));
    EXPECT_EQ(1, data().gpu5_8);
    EXPECT_EQ(1, data().gpu8);
}
TEST_F(NsmSetWriteProtectedTest, goodTestGetDataIndex)
{
    EXPECT_EQ(GPU_SPI_FLASH_1,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_GPU, 0));
    EXPECT_EQ(GPU_SPI_FLASH_2,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_GPU, 1));
    EXPECT_EQ(GPU_SPI_FLASH_3,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_GPU, 2));
    EXPECT_EQ(GPU_SPI_FLASH_4,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_GPU, 3));
    EXPECT_EQ(GPU_SPI_FLASH_5,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_GPU, 4));
    EXPECT_EQ(GPU_SPI_FLASH_6,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_GPU, 5));
    EXPECT_EQ(GPU_SPI_FLASH_7,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_GPU, 6));
    EXPECT_EQ(GPU_SPI_FLASH_8,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_GPU, 7));
    EXPECT_EQ(NVSW_EEPROM_1,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_SWITCH, 0));
    EXPECT_EQ(NVSW_EEPROM_2,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_SWITCH, 1));
    EXPECT_EQ(PEX_SW_EEPROM,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_PCIE_BRIDGE, 0));
    EXPECT_EQ(HMC_SPI_FLASH,
              NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_BASEBOARD, 0));
    EXPECT_EQ(RETIMER_EEPROM_1, NsmSetWriteProtected::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 0, true));
    EXPECT_EQ(RETIMER_EEPROM_2, NsmSetWriteProtected::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 1, true));
    EXPECT_EQ(RETIMER_EEPROM_3, NsmSetWriteProtected::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 2, true));
    EXPECT_EQ(RETIMER_EEPROM_4, NsmSetWriteProtected::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 3, true));
    EXPECT_EQ(RETIMER_EEPROM_5, NsmSetWriteProtected::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 4, true));
    EXPECT_EQ(RETIMER_EEPROM_6, NsmSetWriteProtected::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 5, true));
    EXPECT_EQ(RETIMER_EEPROM_7, NsmSetWriteProtected::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 6, true));
    EXPECT_EQ(RETIMER_EEPROM_8, NsmSetWriteProtected::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 7, true));
}

TEST_F(NsmSetWriteProtectedTest, badTestGetDataIndex)
{
    EXPECT_EQ(0, NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_GPU, 8));
    EXPECT_EQ(0, NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_GPU, 8));
    EXPECT_EQ(0, NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_SWITCH, 2));
    EXPECT_EQ(
        0, NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_BASEBOARD, 8, true));
    EXPECT_EQ(0, NsmSetWriteProtected::getDataIndex(NSM_DEV_ID_UNKNOWN, 0));
}
