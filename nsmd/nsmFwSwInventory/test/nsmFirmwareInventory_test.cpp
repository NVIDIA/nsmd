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
} // namespace nsm

struct NsmFirmwareInventoryTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_WriteProtect";
    const std::string name = "HGX_FW_PCIeRetimer_5";
    const std::string objPath = firmwareInventoryBasePath / name;

    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    NsmFirmwareInventoryTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(fpga, nullptr);
        EXPECT_EQ(NSM_DEV_ID_BASEBOARD, fpga->getDeviceType());
    }

    dbus::PropertyMap error = {
        {"Type", "NSM_FirmwreInventory"},
        {"UUID", "a3b0bdf6-8661-4d8e-8268-0e59415f2076"},
    };
    dbus::PropertyMap retimer = {
        {"Name", name},
        {"Type", "NSM_WriteProtect"},
        {"UUID", fpgaUuid},
        {"Manufacturer", "NVIDIA"},
        {"DataIndex",
         uint64_t(diagnostics_enable_disable_wp_data_index::RETIMER_EEPROM_3)},
        {"InstanceNumber", uint64_t(2)},
    };
    dbus::PropertyMap retimerAsset = {
        {"Type", "NSM_Asset"},
        {"Manufacturer", "NVIDIA"},
    };
    dbus::PropertyMap retimerAssociations[2] = {
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
    dbus::PropertyMap retimerVersion = {
        {"Type", "NSM_FirmwareVersion"},
    };
    dbus::PropertyMap gpu = {
        {"Name", "HGX_FW_GPU_SXM_2"},
        {"Type", "NSM_WriteProtect"},
        {"UUID", fpgaUuid},
        {"DataIndex",
         uint64_t(diagnostics_enable_disable_wp_data_index::GPU_SPI_FLASH_2)},
    };
    dbus::PropertyMap cpu = {
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

TEST_F(NsmFirmwareInventoryTest, badTestNoDevideFound)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties with INVALID UUID that doesn't match any device
    const uuid_t invalidUuid =
        "a3b0bdf6-8661-4d8e-8268-0e59415f2076"; // From error collection
    propertyMap["Name"] = retimer["Name"];
    propertyMap["UUID"] = invalidUuid;          // Invalid UUID as uuid_t type

    // Set up interface-specific properties
    propertyMap["Type"] = retimer["Type"];
    propertyMap["DataIndex"] = retimer["DataIndex"];

    EXPECT_THROW_COROUTINE(
        nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmFirmwareInventoryTest, goodTestCreateSensors)
{
    auto sensors = 1; // Skip msgTypes sensor added by initMsgTypesSensor()
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["Name"] = retimer["Name"];
    basePropertyMap["UUID"] = retimer["UUID"];

    // Set up interface-specific properties for NSM_Asset
    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, basicIntfName + ".Asset");
    propertyMap["Type"] = retimerAsset["Type"];
    propertyMap["Manufacturer"] = retimerAsset["Manufacturer"];
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName + ".Asset",
                                      objPath);
    auto retimerAsset =
        dynamic_pointer_cast<NsmFirmwareInventory<NsmAssetIntf>>(
            fpga->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerAsset);
    EXPECT_EQ(std::get<std::string>(retimer["Manufacturer"]),
              retimerAsset->invoke(pdiMethod(manufacturer)));

    utils::MockDbusAsync::serviceMap() = serviceMap;
    // Update properties for NSM_WriteProtect
    auto& propertyMapWP = utils::MockDbusAsync::propertyMap(objPath,
                                                            basicIntfName);
    propertyMapWP["Name"] = retimer["Name"];
    propertyMapWP["UUID"] = retimer["UUID"];
    propertyMapWP["Type"] = retimer["Type"];
    propertyMapWP["DataIndex"] = retimer["DataIndex"];

    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".Associations0");
    propertyMapAssociation0 = retimerAssociations[0];
    auto& propertyMapAssociation1 = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".Associations1");
    propertyMapAssociation1 = retimerAssociations[1];

    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);

    auto retimerAssociation =
        dynamic_pointer_cast<NsmFirmwareInventory<AssociationDefinitionsIntf>>(
            fpga->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerAssociation);
    EXPECT_EQ(2, retimerAssociation->invoke(pdiMethod(associations)).size());

    auto retimerSettings = dynamic_pointer_cast<NsmSetWriteProtected>(
        fpga->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, retimerSettings);
    EXPECT_EQ(std::get<uint64_t>(retimer["DataIndex"]),
              retimerSettings->dataIndex);

    auto writeProtectedSensor = dynamic_pointer_cast<NsmWriteProtectedControl>(
        fpga->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, writeProtectedSensor);
    EXPECT_TRUE(writeProtectedSensor->sensors.empty());

    // Update properties for NSM_FirmwareVersion
    auto& propertyMapFW = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".FirmwareVersion");
    propertyMapFW["Type"] = retimerVersion["Type"];
    propertyMapFW["InstanceNumber"] = retimer["InstanceNumber"];
    nsmFirmwareInventoryCreateSensors(
        mockManager, basicIntfName + ".FirmwareVersion", objPath);

    auto version = dynamic_pointer_cast<NsmInventoryProperty<VersionIntf>>(
        fpga->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, version);
    EXPECT_EQ(PCIERETIMER_0_EEPROM_VERSION +
                  std::get<uint64_t>(retimer["InstanceNumber"]),
              version->property);

    utils::MockDbusAsync::serviceMap() = emtpyServiceMap;
    // Update properties for GPU NSM_WriteProtect
    auto& propertyMapGPU = utils::MockDbusAsync::propertyMap(objPath,
                                                             basicIntfName);
    propertyMapGPU["Name"] = gpu["Name"];
    propertyMapGPU["UUID"] = retimer["UUID"];
    propertyMapGPU["Type"] = gpu["Type"];
    propertyMapGPU["DataIndex"] = gpu["DataIndex"];
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);

    auto gpuSettings = dynamic_pointer_cast<NsmSetWriteProtected>(
        fpga->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, gpuSettings);
    EXPECT_EQ(std::get<uint64_t>(gpu["DataIndex"]), gpuSettings->dataIndex);

    EXPECT_EQ(1, writeProtectedSensor->sensors.size());
    EXPECT_EQ(3, fpga->staticSensors.size());
    EXPECT_EQ(2, fpga->roundRobinSensors.size());
    EXPECT_EQ(0, fpga->prioritySensors.size());
    EXPECT_EQ(sensors, fpga->deviceSensors.size());
}

TEST_F(NsmFirmwareInventoryTest, goodTestCreateCpuSensors)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["Name"] = cpu["Name"];
    propertyMap["UUID"] = cpu["UUID"];

    // Set up interface-specific properties for NSM_WriteProtect
    propertyMap["Type"] = cpu["Type"];
    propertyMap["DataIndex"] = cpu["DataIndex"];

    utils::MockDbusAsync::serviceMap() = emtpyServiceMap;
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(2, fpga->roundRobinSensors.size()); // msgTypes + 1 other
    EXPECT_EQ(0, fpga->staticSensors.size());
    EXPECT_EQ(0, fpga->prioritySensors.size());
    EXPECT_EQ(3, fpga->deviceSensors.size()); // msgTypes + 2 others
    auto sensors = 1;                         // Skip msgTypes sensor at index 0
    auto cpuSettings = dynamic_pointer_cast<NsmSetWriteProtected>(
        fpga->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, cpuSettings);
    EXPECT_EQ(std::get<uint64_t>(cpu["DataIndex"]), cpuSettings->dataIndex);

    auto cpuWriteProtectedSensor =
        dynamic_pointer_cast<NsmWriteProtectedControl>(
            fpga->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, cpuWriteProtectedSensor);
}
