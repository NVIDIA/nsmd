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

// ===================================================================
// Branch Coverage Tests
// ===================================================================

// base interface not registered → coGetCachedBaseProperties returns error
// → co_return rc immediately (lines 45-48)
TEST_F(NsmFirmwareInventoryTest, BasePropertiesFail_ReturnsEarly)
{
    const std::string uniquePath = "/xyz/test/fw/base_fail_unique";
    // Do NOT register basicIntfName for uniquePath — findPropertyMap returns
    // nullptr → coGetCachedBaseProperties returns NSM_SW_ERROR → early return.
    auto& other = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    basicIntfName + ".Other");
    other["Type"] = std::string("NSM_Asset");

    const size_t before = fpga->deviceSensors.size();
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, uniquePath);
    // No sensors added; function exited via co_return rc
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// NSM_WriteProtect: DataIndex has no matching case → default: throw
// std::out_of_range (lines 132-133)
TEST_F(NsmFirmwareInventoryTest, WriteProtect_InvalidDataIndex_Throws)
{
    const std::string uniquePath = "/xyz/test/fw/inv_idx_unique";
    auto& propMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    propMap["Name"] = std::string("TestSensor");
    propMap["UUID"] = fpgaUuid;
    propMap["Type"] = std::string("NSM_WriteProtect");
    propMap["DataIndex"] = uint64_t(0xFFFF); // out-of-range enum value

    utils::MockDbusAsync::serviceMap() = emtpyServiceMap;
    EXPECT_THROW_COROUTINE(nsmFirmwareInventoryCreateSensors(
                               mockManager, basicIntfName, uniquePath),
                           std::out_of_range);
}

// NSM_Asset: "Manufacturer" key absent → manufacturer defaults to empty string
// (line 157 false-branch)
TEST_F(NsmFirmwareInventoryTest, Asset_MissingManufacturer_EmptyManufacturer)
{
    const std::string uniquePath = "/xyz/test/fw/no_mfg_unique";
    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    baseMap["Name"] = std::string("TestFW");
    baseMap["UUID"] = fpgaUuid;

    auto& assetMap =
        utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName + ".Asset");
    assetMap["Type"] = std::string("NSM_Asset");
    // No "Manufacturer" key

    const size_t before = fpga->staticSensors.size();
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName + ".Asset",
                                      uniquePath);
    EXPECT_EQ(before + 1, fpga->staticSensors.size());
    auto asset = dynamic_pointer_cast<NsmFirmwareInventory<NsmAssetIntf>>(
        fpga->staticSensors.back());
    ASSERT_NE(nullptr, asset);
    EXPECT_EQ(std::string(""), asset->invoke(pdiMethod(manufacturer)));
}

// NSM_FirmwareVersion: "InstanceNumber" key absent → instanceNumber defaults
// to 0 (line 170 false-branch)
TEST_F(NsmFirmwareInventoryTest,
       FirmwareVersion_MissingInstanceNumber_DefaultsZero)
{
    const std::string uniquePath = "/xyz/test/fw/no_instnum_unique";
    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    baseMap["Name"] = std::string("TestVersionFW");
    baseMap["UUID"] = fpgaUuid;

    auto& fwMap = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".FirmwareVersion");
    fwMap["Type"] = std::string("NSM_FirmwareVersion");
    // No "InstanceNumber" key → instanceNumber = 0

    const size_t before = fpga->staticSensors.size();
    nsmFirmwareInventoryCreateSensors(
        mockManager, basicIntfName + ".FirmwareVersion", uniquePath);
    EXPECT_EQ(before + 1, fpga->staticSensors.size());
    auto version = dynamic_pointer_cast<NsmInventoryProperty<VersionIntf>>(
        fpga->staticSensors.back());
    ASSERT_NE(nullptr, version);
    // instanceNumber=0 → property = PCIERETIMER_0_EEPROM_VERSION + 0
    EXPECT_EQ(nsm_inventory_property_identifiers(PCIERETIMER_0_EEPROM_VERSION),
              version->property);
}

// Unknown type: falls through all if/else-if chains without creating sensors
// → co_return NSM_SUCCESS (lines 184-186)
TEST_F(NsmFirmwareInventoryTest, UnknownType_FallsThrough_NoSensorsCreated)
{
    const std::string uniquePath = "/xyz/test/fw/unknown_type_unique";
    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    baseMap["Name"] = std::string("TestSensor");
    baseMap["UUID"] = fpgaUuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".Unknown");
    currMap["Type"] = std::string("NSM_UnknownType");

    const size_t beforeDevice = fpga->deviceSensors.size();
    const size_t beforeStatic = fpga->staticSensors.size();
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName + ".Unknown",
                                      uniquePath);
    EXPECT_EQ(beforeDevice, fpga->deviceSensors.size());
    EXPECT_EQ(beforeStatic, fpga->staticSensors.size());
}

// =============================================================================
// FALSE-branch coverage: count() checks in nsmFirmwareInventoryCreateSensors
// =============================================================================

// Type absent → count("Type") FALSE → type="" → no if/else-if branch matches
// → co_return NSM_SUCCESS (same fall-through as unknown type but via FALSE //
// branch)
TEST_F(NsmFirmwareInventoryTest, Factory_MissingType_NoSensorCreated)
{
    const std::string uniquePath = "/xyz/test/fw/missing_type_false";
    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    baseMap["Name"] = std::string("TestSensor");
    baseMap["UUID"] = fpgaUuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".NoType");
    (void)currMap; // intentionally empty → count("Type") = 0 → type=""

    const size_t beforeDevice = fpga->deviceSensors.size();
    const size_t beforeStatic = fpga->staticSensors.size();
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName + ".NoType",
                                      uniquePath);
    EXPECT_EQ(beforeDevice, fpga->deviceSensors.size());
    EXPECT_EQ(beforeStatic, fpga->staticSensors.size());
}

// UUID absent from base → count("UUID") FALSE → uuid="" →
// parseStaticUuid("") throws inside getNsmDeviceFromStaticUUID
TEST_F(NsmFirmwareInventoryTest, Factory_MissingUUID_Throws)
{
    const std::string uniquePath = "/xyz/test/fw/missing_uuid_false";
    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    baseMap["Name"] = std::string("TestSensor");
    // "UUID" intentionally omitted → uuid="" → parseStaticUuid throws

    auto& currMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName + ".Asset");
    currMap["Type"] = std::string("NSM_Asset");

    EXPECT_THROW_COROUTINE(
        nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName + ".Asset",
                                          uniquePath),
        std::exception);
}

// Name absent from base → count("Name") FALSE → name="" → NSM_Asset branch:
// NsmFirmwareInventory<NsmAssetIntf>("") tries to register D-Bus object at
// firmwareInventoryBasePath/"" → invalid D-Bus path → sd_bus throws
TEST_F(NsmFirmwareInventoryTest, Factory_MissingName_Throws)
{
    const std::string uniquePath = "/xyz/test/fw/missing_name_false";
    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    // "Name" intentionally omitted → name="" → FALSE branch at line 53
    baseMap["UUID"] = fpgaUuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName + ".Asset");
    currMap["Type"] = std::string("NSM_Asset");

    // name="" → path = firmwareInventoryBasePath/"" → invalid → sd_bus throws
    EXPECT_THROW_COROUTINE(
        nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName + ".Asset",
                                          uniquePath),
        std::exception);
}

// type=="NSM_WriteProtect", DataIndex absent → count("DataIndex") FALSE →
// dataIndex=0 → not in enum range (RETIMER_EEPROM=128) → default: throw
TEST_F(NsmFirmwareInventoryTest, WriteProtect_MissingDataIndex_ThrowsOutOfRange)
{
    const std::string uniquePath = "/xyz/test/fw/missing_dataidx_false";
    auto& propMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    propMap["Name"] = std::string("TestSensor");
    propMap["UUID"] = fpgaUuid;
    propMap["Type"] = std::string("NSM_WriteProtect");
    // "DataIndex" intentionally omitted → dataIndex = 0 (default) →
    // switch hits default → throw std::out_of_range

    utils::MockDbusAsync::serviceMap() = emtpyServiceMap;
    EXPECT_THROW_COROUTINE(nsmFirmwareInventoryCreateSensors(
                               mockManager, basicIntfName, uniquePath),
                           std::out_of_range);
}
