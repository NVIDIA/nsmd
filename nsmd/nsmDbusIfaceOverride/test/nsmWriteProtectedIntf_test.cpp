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

#include "device-configuration.h"
#include "diagnostics.h"

#include "nsmWriteProtectedIntf.hpp"

struct NsmWriteProtectedIntfTest : public testing::Test
{
    const uuid_t fpgaUuid = "992b3ec1-e464-f145-8686-409009062aa8";

    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(fpgaUuid)},
    };
    NsmDevice& fpga = *devices[0];

    NiceMock<MockSensorManager> mockManager{devices};
    std::unique_ptr<NsmWriteProtectedIntf> writeProtectedIntf;

    void init(NsmDeviceIdentification deviceType, uint8_t instanceNumber,
              bool retimer = false)
    {
        writeProtectedIntf = std::make_unique<NsmWriteProtectedIntf>(
            mockManager, devices[0], instanceNumber, deviceType,
            (firmwareInventoryBasePath / "HGX_FW_TEST_DEV").string().c_str(),
            retimer);
    }

    const Response fpgaDiagnosticMsgHeader{
        0x10,
        0xDE,                              // PCI VID: NVIDIA 0x10DE
        0x00,                              // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                              // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_DEVICE_CONFIGURATION,     // NVIDIA_MSG_TYPE
        NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
        0,                                 // completion code
        0,
        0,
        8,
        0 // data size
    };
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

    std::vector<std::shared_ptr<Response>> responses;
    auto data() const
    {
        const auto headerSize = fpgaDiagnosticMsgHeader.size();
        EXPECT_FALSE(responses.empty());
        EXPECT_EQ(headerSize + sizeof(nsm_fpga_diagnostics_settings_wp),
                  responses.back()->size());
        return *reinterpret_cast<nsm_fpga_diagnostics_settings_wp*>(
            responses.back()->data() + headerSize);
    }
    auto mockSendRecvNsmMsgSync(const Response& responseMsg,
                                const Response& data = Response(),
                                nsm_completion_codes code = NSM_SUCCESS)
    {
        auto response = std::make_shared<Response>();
        response->insert(response->end(), responseMsg.begin(),
                         responseMsg.end());
        response->insert(response->end(), data.begin(), data.end());
        responses.emplace_back(response);
        return [response, code](eid_t, Request&,
                                [[maybe_unused]] const nsm_msg** responseMsg,
                                [[maybe_unused]] size_t* responseLen) {
            *responseMsg = reinterpret_cast<nsm_msg*>(response->data());
            *responseLen = response->size();
            return code;
        };
    }
};

TEST_F(NsmWriteProtectedIntfTest, badTestBaseboardWrite)
{
    init(NSM_DEV_ID_BASEBOARD, 0);
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(1)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg, Response(),
                                         NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    EXPECT_THROW(
        writeProtectedIntf->setWriteProtected(true),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);
}

TEST_F(NsmWriteProtectedIntfTest, goodTestBaseboardWrite)
{
    init(NSM_DEV_ID_BASEBOARD, 0);
    const Response enabled{0b00000010, 0x00, 0x00, 0x00,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
}

TEST_F(NsmWriteProtectedIntfTest, goodTestRetimer1Write)
{
    init(NSM_DEV_ID_BASEBOARD, 0, true);
    const Response enabled{0b00000001, 0x00, 0b00000001, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().retimer);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestRetimer2Write)
{
    init(NSM_DEV_ID_BASEBOARD, 1, true);
    const Response enabled{0b00000001, 0x00, 0b00000010, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().retimer);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestRetimer3Write)
{
    init(NSM_DEV_ID_BASEBOARD, 2, true);
    const Response enabled{0b00000001, 0x00, 0b00000100, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().retimer);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestRetimer4Write)
{
    init(NSM_DEV_ID_BASEBOARD, 3, true);
    const Response enabled{0b00000001, 0x00, 0b00001000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().retimer);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestRetimer5Write)
{
    init(NSM_DEV_ID_BASEBOARD, 4, true);
    const Response enabled{0b00000001, 0x00, 0b00010000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().retimer);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestRetimer6Write)
{
    init(NSM_DEV_ID_BASEBOARD, 5, true);
    const Response enabled{0b00000001, 0x00, 0b00100000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().retimer);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestRetimer7Write)
{
    init(NSM_DEV_ID_BASEBOARD, 6, true);
    const Response enabled{0b00000001, 0x00, 0b01000000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().retimer);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestRetimer8Write)
{
    init(NSM_DEV_ID_BASEBOARD, 7, true);
    const Response enabled{0b00000001, 0x00, 0b10000000, 0x00,
                           0x00,       0x00, 0x00,       0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().retimer);
}

TEST_F(NsmWriteProtectedIntfTest, goodTestNvSwitch1Write)
{
    init(NSM_DEV_ID_SWITCH, 0);
    const Response enabled{0b00001000, 0x00, 0x00, 0b0000001,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().nvSwitch);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestNvSwitch2Write)
{
    init(NSM_DEV_ID_SWITCH, 1);
    const Response enabled{0b00001000, 0x00, 0x00, 0b0000010,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().nvSwitch);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestNvLinkManagementWrite)
{
    init(NSM_DEV_ID_PCIE_BRIDGE, 0);
    const Response enabled{0b00000100, 0x00, 0x00, 0x00,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
}

TEST_F(NsmWriteProtectedIntfTest, goodTestGpu1Write)
{
    init(NSM_DEV_ID_GPU, 0);
    const Response enabled{0b10000000, 0x00, 0x00, 0b00010000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().gpu1_4);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestGpu2Write)
{
    init(NSM_DEV_ID_GPU, 1);
    const Response enabled{0b10000000, 0x00, 0x00, 0b00100000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().gpu1_4);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestGpu3Write)
{
    init(NSM_DEV_ID_GPU, 2);
    const Response enabled{0b10000000, 0x00, 0x00, 0b01000000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().gpu1_4);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestGpu4Write)
{
    init(NSM_DEV_ID_GPU, 3);
    const Response enabled{0b10000000, 0x00, 0x00, 0b10000000,
                           0x00,       0x00, 0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().gpu1_4);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestGpu5Write)
{
    init(NSM_DEV_ID_GPU, 4);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00000001, 0x00,       0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().gpu5_8);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestGpu6Write)
{
    init(NSM_DEV_ID_GPU, 5);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00000010, 0x00,       0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().gpu5_8);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestGpu7Write)
{
    init(NSM_DEV_ID_GPU, 6);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00000100, 0x00,       0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().gpu5_8);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestGpu8Write)
{
    init(NSM_DEV_ID_GPU, 7);
    const Response enabled{0x00,       0b00000001, 0x00, 0x00,
                           0b00001000, 0x00,       0x00, 0x00};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableMsg))
        .WillOnce(mockSendRecvNsmMsgSync(fpgaDiagnosticMsgHeader, enabled));

    EXPECT_EQ(true, writeProtectedIntf->setWriteProtected(true));
    EXPECT_EQ(1, data().gpu5_8);
}
TEST_F(NsmWriteProtectedIntfTest, goodTestGetDataIndex)
{
    EXPECT_EQ(GPU_SPI_FLASH_1,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_GPU, 0));
    EXPECT_EQ(GPU_SPI_FLASH_2,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_GPU, 1));
    EXPECT_EQ(GPU_SPI_FLASH_3,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_GPU, 2));
    EXPECT_EQ(GPU_SPI_FLASH_4,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_GPU, 3));
    EXPECT_EQ(GPU_SPI_FLASH_5,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_GPU, 4));
    EXPECT_EQ(GPU_SPI_FLASH_6,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_GPU, 5));
    EXPECT_EQ(GPU_SPI_FLASH_7,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_GPU, 6));
    EXPECT_EQ(GPU_SPI_FLASH_8,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_GPU, 7));
    EXPECT_EQ(NVSW_EEPROM_1,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_SWITCH, 0));
    EXPECT_EQ(NVSW_EEPROM_2,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_SWITCH, 1));
    EXPECT_EQ(PEX_SW_EEPROM,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_PCIE_BRIDGE, 0));
    EXPECT_EQ(BASEBOARD_FRU_EEPROM,
              NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_BASEBOARD, 0));
    EXPECT_EQ(RETIMER_EEPROM_1, NsmWriteProtectedIntf::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 0, true));
    EXPECT_EQ(RETIMER_EEPROM_2, NsmWriteProtectedIntf::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 1, true));
    EXPECT_EQ(RETIMER_EEPROM_3, NsmWriteProtectedIntf::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 2, true));
    EXPECT_EQ(RETIMER_EEPROM_4, NsmWriteProtectedIntf::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 3, true));
    EXPECT_EQ(RETIMER_EEPROM_5, NsmWriteProtectedIntf::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 4, true));
    EXPECT_EQ(RETIMER_EEPROM_6, NsmWriteProtectedIntf::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 5, true));
    EXPECT_EQ(RETIMER_EEPROM_7, NsmWriteProtectedIntf::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 6, true));
    EXPECT_EQ(RETIMER_EEPROM_8, NsmWriteProtectedIntf::getDataIndex(
                                    NSM_DEV_ID_BASEBOARD, 7, true));
}

TEST_F(NsmWriteProtectedIntfTest, badTestGetDataIndex)
{
    EXPECT_EQ(0, NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_GPU, 8));
    EXPECT_EQ(0, NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_GPU, 8));
    EXPECT_EQ(0, NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_SWITCH, 2));
    EXPECT_EQ(
        0, NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_BASEBOARD, 8, true));
    EXPECT_EQ(0, NsmWriteProtectedIntf::getDataIndex(NSM_DEV_ID_UNKNOWN, 0));
}
