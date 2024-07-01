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

#include "nsmModeIntf.hpp"

struct NsmModeIntfTest : public testing::Test, public SensorManagerTest
{
    const uuid_t fpgaUuid = "992b3ec1-e464-f145-8686-409009062aa8";

    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(fpgaUuid)},
    };
    NsmDevice& fpga = *devices[0];

    NiceMock<MockSensorManager> mockManager{devices};
    NsmModeIntf modeIntf{mockManager, devices[0]};

    const Response enableDisableResp{
        0x10,
        0xDE,                            // PCI VID: NVIDIA 0x10DE
        0x00,                            // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                            // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
        NSM_ENABLE_DISABLE_GPU_IST_MODE, // command
        0,                               // completion code
        0,
        0,
        0,
        0 // data size
    };
    const Response diagHeader{
        0x10,
        0xDE,                              // PCI VID: NVIDIA 0x10DE
        0x00,                              // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                              // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_DEVICE_CONFIGURATION,     // NVIDIA_MSG_TYPE
        NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
        0,                                 // completion code
        0,
        0,
        1,
        0 // data size
    };

    uint8_t& data()
    {
        return SensorManagerTest::data<uint8_t>(diagHeader.size());
    }
};

TEST_F(NsmModeIntfTest, testGoodSet)
{
    const Response enabled{0xFF};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableResp))
        .WillOnce(mockSendRecvNsmMsgSync(diagHeader, enabled));

    EXPECT_EQ(ModeIntf::StateOfISTMode::Enabled,
              modeIntf.setIstMode(ModeIntf::StateOfISTMode::Enabled));
}

TEST_F(NsmModeIntfTest, testBadSetUnsupportedCommandCode)
{
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(1)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableResp, Response(),
                                         NSM_ERR_NOT_READY));

    EXPECT_THROW(
        modeIntf.setIstMode(ModeIntf::StateOfISTMode::Enabled),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);
}
TEST_F(NsmModeIntfTest, testBadSetDecodeError)
{
    const Response enableDisableBadResp{
        0x10,
        0xDE,                            // PCI VID: NVIDIA 0x10DE
        0x00,                            // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                            // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
        NSM_ENABLE_DISABLE_GPU_IST_MODE, // command
        0,                               // completion code
        0,
        0,
        1,
        0 // data size
    };
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(1)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableBadResp));

    EXPECT_THROW(
        modeIntf.setIstMode(ModeIntf::StateOfISTMode::Enabled),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);
}

TEST_F(NsmModeIntfTest, testBadGetUnsupportedCommandCode)
{
    const Response enabled{0xFF};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableResp))
        .WillOnce(
            mockSendRecvNsmMsgSync(diagHeader, enabled, NSM_ERR_NOT_READY));

    EXPECT_THROW(
        modeIntf.setIstMode(ModeIntf::StateOfISTMode::Enabled),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);
}
TEST_F(NsmModeIntfTest, testBadGetDecodeError)
{
    const Response diagBadHeader{
        0x10,
        0xDE,                              // PCI VID: NVIDIA 0x10DE
        0x00,                              // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                              // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_DEVICE_CONFIGURATION,     // NVIDIA_MSG_TYPE
        NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
        0,                                 // completion code
        0,
        0,
        0,
        0 // data size
    };
    const Response enabled{0xFF};
    EXPECT_CALL(mockManager, SendRecvNsmMsgSync)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsgSync(enableDisableResp))
        .WillOnce(mockSendRecvNsmMsgSync(diagBadHeader, enabled));

    EXPECT_THROW(
        modeIntf.setIstMode(ModeIntf::StateOfISTMode::Enabled),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);
}
