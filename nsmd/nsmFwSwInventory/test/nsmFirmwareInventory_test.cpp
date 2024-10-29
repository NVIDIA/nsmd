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
        "xyz.openbmc_project.Configuration.NSM_WriteProtect";
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
        {"Type", "NSM_WriteProtect"},
        {"UUID", fpgaUuid},
        {"Manufacturer", "NVIDIA"},
        {"DataIndex",
         uint64_t(diagnostics_enable_disable_wp_data_index::RETIMER_EEPROM_3)},
        {"InstanceNumber", uint64_t(2)},
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
        {"Type", "NSM_WriteProtect"},
        {"UUID", fpgaUuid},
        {"DataIndex",
         uint64_t(diagnostics_enable_disable_wp_data_index::GPU_SPI_FLASH_2)},
    };
    const PropertyValuesCollection cpu = {
        {"Name", "HGX_FW_CPU_0"},
        {"Type", "NSM_WriteProtect"},
        {"UUID", fpgaUuid},
        {"Manufacturer", "NVIDIA"},
        {"DataIndex",
         uint64_t(diagnostics_enable_disable_wp_data_index::CPU_SPI_FLASH_3)},
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
    auto sensors = 0;
    auto& map = utils::MockDbusAsync::getServiceMap();
    auto& values = utils::MockDbusAsync::getValues();

    values.push(objPath, get(retimer, "Name"));
    values.push(objPath, get(retimerAsset, "Type"));
    values.push(objPath, get(retimer, "UUID"));
    values.push(objPath, get(retimerAsset, "Manufacturer"));
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName + ".Asset",
                                      objPath);

    auto retimerAsset =
        dynamic_pointer_cast<NsmFirmwareInventory<NsmAssetIntf>>(
            fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerAsset);
    EXPECT_EQ(get<std::string>(retimer, "Manufacturer"),
              retimerAsset->pdi().manufacturer());

    map = serviceMap;
    values.push(objPath, get(retimer, "Name"));
    values.push(objPath, get(retimer, "Type"));
    values.push(objPath, get(retimer, "UUID"));
    values.push(objPath, get(retimer, "DataIndex"));
    values.push(objPath, get(retimerAssociations[0], "Forward"));
    values.push(objPath, get(retimerAssociations[0], "Backward"));
    values.push(objPath, get(retimerAssociations[0], "AbsolutePath"));
    values.push(objPath, get(retimerAssociations[1], "Forward"));
    values.push(objPath, get(retimerAssociations[1], "Backward"));
    values.push(objPath, get(retimerAssociations[1], "AbsolutePath"));
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);

    auto retimerAssociation =
        dynamic_pointer_cast<NsmFirmwareInventory<AssociationDefinitionsIntf>>(
            fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerAssociation);
    EXPECT_EQ(2, retimerAssociation->pdi().associations().size());

    auto retimerSettings = dynamic_pointer_cast<NsmSetWriteProtected>(
        fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerSettings);
    EXPECT_EQ(get<uint64_t>(retimer, "DataIndex"), retimerSettings->dataIndex);

    auto retimerWriteProtectedSensor =
        dynamic_pointer_cast<NsmWriteProtectedControl>(
            fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerWriteProtectedSensor);

    values.push(objPath, get(retimer, "Name"));
    values.push(objPath, get(retimerVersion, "Type"));
    values.push(objPath, get(retimer, "UUID"));
    values.push(objPath, get(retimer, "InstanceNumber"));
    nsmFirmwareInventoryCreateSensors(
        mockManager, basicIntfName + ".FirmwareVersion", objPath);

    auto version = dynamic_pointer_cast<NsmInventoryProperty<VersionIntf>>(
        fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, version);
    EXPECT_EQ(PCIERETIMER_0_EEPROM_VERSION +
                  get<uint64_t>(retimer, "InstanceNumber"),
              version->property);

    map = emtpyServiceMap;
    values.push(objPath, get(gpu, "Name"));
    values.push(objPath, get(gpu, "Type"));
    values.push(objPath, get(gpu, "UUID"));
    values.push(objPath, get(gpu, "DataIndex"));
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);

    auto gpuSettings = dynamic_pointer_cast<NsmSetWriteProtected>(
        fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, gpuSettings);
    EXPECT_EQ(get<uint64_t>(gpu, "DataIndex"), gpuSettings->dataIndex);

    auto gpuWriteProtectedSensor =
        dynamic_pointer_cast<NsmWriteProtectedControl>(
            fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, gpuWriteProtectedSensor);

    EXPECT_EQ(5, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(sensors, fpga.deviceSensors.size());
}

TEST_F(NsmFirmwareInventoryTest, goodTestCreateCpuSensors)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(cpu, "Name"));
    values.push(objPath, get(cpu, "Type"));
    values.push(objPath, get(cpu, "UUID"));
    values.push(objPath, get(cpu, "DataIndex"));

    utils::MockDbusAsync::getServiceMap() = emtpyServiceMap;
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);

    EXPECT_EQ(1, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(2, fpga.deviceSensors.size());
    auto sensors = 0;
    auto cpuSettings = dynamic_pointer_cast<NsmSetWriteProtected>(
        fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, cpuSettings);
    EXPECT_EQ(get<uint64_t>(cpu, "DataIndex"), cpuSettings->dataIndex);

    auto cpuWriteProtectedSensor =
        dynamic_pointer_cast<NsmWriteProtectedControl>(
            fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, cpuWriteProtectedSensor);
}

TEST_F(NsmFirmwareInventoryTest, badTestNoDevideFound)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(retimer, "Name"));
    values.push(objPath, get(retimer, "Type"));
    values.push(objPath, get(error, "UUID"));
    values.push(objPath, get(retimer, "DataIndex"));

    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
}
