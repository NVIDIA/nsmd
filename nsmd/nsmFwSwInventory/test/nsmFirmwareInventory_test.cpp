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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "device-configuration.h"

#include "nsmFirmwareInventory.hpp"
#include "nsmSoftwareSettings.hpp"

namespace nsm
{
void nsmFirmwareInventoryCreateSensors(SensorManager&, const std::string&,
                                       const std::string&);
}

struct NsmFirmwareInventoryTest : public testing::Test, public utils::DBusTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_FirmwareInventory";
    const std::string name = "HGX_FW_FPGA_0";
    const std::string objPath = firmwareInventoryBasePath / name;

    const uuid_t fpgaUuid = "992b3ec1-e464-f145-8686-409009062aa8";

    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(fpgaUuid)},
    };
    NsmDevice& fpga = *devices[0];

    NiceMock<MockSensorManager> mockManager{devices};

    const PropertyValuesCollection error = {
        {"Type", "NSM_FirmwreInventory"},
        {"UUID", "a3b0bdf6-8661-4d8e-8268-0e59415f2076"},
    };
    const PropertyValuesCollection basic = {
        {"Name", name},
        {"Type", "NSM_FirmwareInventory"},
        {"UUID", fpgaUuid},
        {"Manufacturer", "NVIDIA"},
        {"DeviceType", uint64_t(NSM_DEV_ID_BASEBOARD)},
        {"InstanceNumber", uint64_t(0)},
    };
};

TEST_F(NsmFirmwareInventoryTest, goodTestCreateSensors)
{
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(1)
        .WillRepeatedly(
            [](eid_t, Request&, const nsm_msg**,
               size_t*) -> requester::Coroutine { co_return NSM_SUCCESS; });
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(basic, "Manufacturer")))
        .WillOnce(Return(get(basic, "DeviceType")))
        .WillOnce(Return(get(basic, "InstanceNumber")));
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(2, fpga.deviceSensors.size());

    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmFirmwareInventory<AssetIntf>>(
                           fpga.deviceSensors[0]));
    EXPECT_EQ(get<std::string>(basic, "Manufacturer"),
              dynamic_pointer_cast<NsmFirmwareInventory<AssetIntf>>(
                  fpga.deviceSensors[0])
                  ->pdi()
                  .manufacturer());

    auto settings =
        dynamic_pointer_cast<NsmSoftwareSettings>(fpga.deviceSensors[1]);
    EXPECT_NE(nullptr, settings);
    EXPECT_EQ(get<uint64_t>(basic, "DeviceType"), settings->deviceType);
    EXPECT_EQ(get<uint64_t>(basic, "InstanceNumber"), settings->instanceId);
}

TEST_F(NsmFirmwareInventoryTest, badTestNoDevideFound)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(error, "UUID")))
        .WillOnce(Return(get(basic, "Manufacturer")))
        .WillOnce(Return(get(basic, "DeviceType")))
        .WillOnce(Return(get(basic, "InstanceNumber")));
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
}