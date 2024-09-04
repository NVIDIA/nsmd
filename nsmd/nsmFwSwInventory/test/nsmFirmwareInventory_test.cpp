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
#include "nsmInventoryProperty.hpp"
#include "nsmSetWriteProtected.hpp"
#include "nsmWriteProtectedControl.hpp"

namespace nsm
{
requester::Coroutine nsmFirmwareInventoryCreateSensors(SensorManager&,
                                                       const std::string&,
                                                       const std::string&);
}

struct NsmFirmwareInventoryTest : public testing::Test, public utils::DBusTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_FirmwareInventory";
    const std::string name = "HGX_FW_PCIeRetimer_5";
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
    const PropertyValuesCollection retimer = {
        {"Name", name},
        {"Type", "NSM_FirmwareInventory"},
        {"UUID", fpgaUuid},
        {"Manufacturer", "NVIDIA"},
        {"DeviceType", uint64_t(NSM_DEV_ID_BASEBOARD)},
        {"IsRetimer", true},
        {"InstanceNumber", uint64_t(5)},
    };
    const PropertyValuesCollection retimerAsset = {
        {"Type", "NSM_Asset"},
        {"Manufacturer", "NVIDIA"},
    };
    const PropertyValuesCollection retimerAssociations[2] = {
        {
            {"Forward", "inventory"},
            {"Backward", "activation"},
            {"AbsolutePath",
             "/xyz/openbmc_project/inventory/system/chassis/HGX_PCIeRetimer_5"},
        },
        {
            {"Forward", "software_version"},
            {"Backward", "updateable"},
            {"AbsolutePath", "/xyz/openbmc_project/software"},
        },
    };
    const PropertyValuesCollection retimerVersion = {
        {"Type", "NSM_FirmwareVersion"},
    };
    const PropertyValuesCollection gpu = {
        {"Name", "HGX_FW_GPU_SXM_2"},
        {"Type", "NSM_FirmwareInventory"},
        {"UUID", fpgaUuid},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"InstanceNumber", uint64_t(1)},
    };
    const MapperServiceMap serviceMap = {
        {
            "xyz.openbmc_project.NSM",
            {
                basicIntfName + ".Associations0",
                basicIntfName + ".Associations1",
            },
        },
    };
    const MapperServiceMap emtpyServiceMap;
};

TEST_F(NsmFirmwareInventoryTest, goodTestCreateSensors)
{
    auto& values = utils::MockDbusAsync::getValues();
    values = std::queue<PropertyValue>();
    values.push(get(retimer, "Name"));
    values.push(get(retimerAsset, "Type"));
    values.push(get(retimer, "UUID"));
    values.push(get(retimer, "DeviceType"));
    values.push(get(retimer, "InstanceNumber"));
    values.push(get(retimerAsset, "Manufacturer"));
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName + ".Asset",
                                      objPath);

    values = std::queue<PropertyValue>();
    values.push(get(retimer, "Name"));
    values.push(get(retimer, "Type"));
    values.push(get(retimer, "UUID"));
    values.push(get(retimer, "DeviceType"));
    values.push(get(retimer, "InstanceNumber"));
    values.push(get(retimerAssociations[0], "Forward"));
    values.push(get(retimerAssociations[0], "Backward"));
    values.push(get(retimerAssociations[0], "AbsolutePath"));
    values.push(get(retimerAssociations[1], "Forward"));
    values.push(get(retimerAssociations[1], "Backward"));
    values.push(get(retimerAssociations[1], "AbsolutePath"));
    values.push(get(retimer, "IsRetimer"));

    auto& map = utils::MockDbusAsync::getServiceMap();
    map = serviceMap;

    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);

    values = std::queue<PropertyValue>();
    values.push(get(retimer, "Name"));
    values.push(get(retimerVersion, "Type"));
    values.push(get(retimer, "UUID"));
    values.push(get(retimer, "DeviceType"));
    values.push(get(retimer, "InstanceNumber"));
    nsmFirmwareInventoryCreateSensors(
        mockManager, basicIntfName + ".FirmwareVersion", objPath);

    values = std::queue<PropertyValue>();
    values.push(get(gpu, "Name"));
    values.push(get(gpu, "Type"));
    values.push(get(gpu, "UUID"));
    values.push(get(gpu, "DeviceType"));
    values.push(get(gpu, "InstanceNumber"));

    map = emtpyServiceMap;
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);

    EXPECT_EQ(5, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(7, fpga.deviceSensors.size());
    auto sensors = 0;
    auto retimerAsset =
        dynamic_pointer_cast<NsmFirmwareInventory<NsmAssetIntf>>(
            fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerAsset);
    EXPECT_EQ(get<std::string>(retimer, "Manufacturer"),
              retimerAsset->pdi().manufacturer());

    auto retimerAssociation =
        dynamic_pointer_cast<NsmFirmwareInventory<AssociationDefinitionsIntf>>(
            fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerAssociation);
    EXPECT_EQ(2, retimerAssociation->pdi().associations().size());

    auto retimerSettings = dynamic_pointer_cast<NsmSetWriteProtected>(
        fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerSettings);

    auto retimerWriteProtectedSensor =
        dynamic_pointer_cast<NsmWriteProtectedControl>(
            fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerWriteProtectedSensor);

    auto version = dynamic_pointer_cast<NsmInventoryProperty<VersionIntf>>(
        fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, version);
    EXPECT_EQ(PCIERETIMER_0_EEPROM_VERSION +
                  get<uint64_t>(retimer, "InstanceNumber"),
              version->property);

    auto gpuSettings = dynamic_pointer_cast<NsmSetWriteProtected>(
        fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, gpuSettings);

    auto gpuWriteProtectedSensor =
        dynamic_pointer_cast<NsmWriteProtectedControl>(
            fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, gpuWriteProtectedSensor);
}

TEST_F(NsmFirmwareInventoryTest, badTestNoDevideFound)
{
    auto& values = utils::MockDbusAsync::getValues();
    values = std::queue<PropertyValue>();
    values.push(get(retimer, "Name"));
    values.push(get(retimer, "Type"));
    values.push(get(error, "UUID"));
    values.push(get(retimer, "DeviceType"));
    values.push(get(retimer, "InstanceNumber"));

    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
}
