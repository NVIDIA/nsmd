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
#include "nsmWriteProtectedControl.hpp"
#include "nsmWriteProtectedIntf.hpp"

namespace nsm
{
void nsmFirmwareInventoryCreateSensors(SensorManager&, const std::string&,
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
    const PropertyValuesCollection retimerVersion = {
        {"Type", "NSM_FirmwareVersion"},
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
    const PropertyValuesCollection gpu = {
        {"Name", "HGX_FW_GPU_SXM_2"},
        {"Type", "NSM_FirmwareInventory"},
        {"UUID", fpgaUuid},
        {"Manufacturer", "NVIDIA"},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"InstanceNumber", uint64_t(1)},
    };
    const PropertyValuesCollection gpuAssociations[2] = {
        {
            {"Forward", "inventory"},
            {"Backward", "activation"},
            {"AbsolutePath",
             "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_2"},
        },
        {
            {"Forward", "software_version"},
            {"Backward", "updateable"},
            {"AbsolutePath", "/xyz/openbmc_project/software"},
        },
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
};

TEST_F(NsmFirmwareInventoryTest, goodTestCreateSensors)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(retimer, "Name")))
        .WillOnce(Return(get(retimer, "Type")))
        .WillOnce(Return(get(retimer, "UUID")))
        .WillOnce(Return(get(retimer, "DeviceType")))
        .WillOnce(Return(get(retimer, "InstanceNumber")))
        .WillOnce(Return(get(retimer, "Manufacturer")))
        .WillOnce(Return(get(retimerAssociations[0], "Forward")))
        .WillOnce(Return(get(retimerAssociations[0], "Backward")))
        .WillOnce(Return(get(retimerAssociations[0], "AbsolutePath")))
        .WillOnce(Return(get(retimerAssociations[1], "Forward")))
        .WillOnce(Return(get(retimerAssociations[1], "Backward")))
        .WillOnce(Return(get(retimerAssociations[1], "AbsolutePath")))
        .WillOnce(Return(get(retimer, "IsRetimer")));
    EXPECT_CALL(mockDBus, getServiceMap).WillOnce(Return(serviceMap));
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(retimer, "Name")))
        .WillOnce(Return(get(retimerVersion, "Type")))
        .WillOnce(Return(get(retimer, "UUID")))
        .WillOnce(Return(get(retimer, "DeviceType")))
        .WillOnce(Return(get(retimer, "InstanceNumber")));
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(
            [](eid_t, Request&, const nsm_msg**,
               size_t*) -> requester::Coroutine { co_return NSM_SUCCESS; });
    nsmFirmwareInventoryCreateSensors(
        mockManager, basicIntfName + ".FirmwareVersion", objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(gpu, "Name")))
        .WillOnce(Return(get(gpu, "Type")))
        .WillOnce(Return(get(gpu, "UUID")))
        .WillOnce(Return(get(gpu, "DeviceType")))
        .WillOnce(Return(get(gpu, "InstanceNumber")))
        .WillOnce(Return(get(gpu, "Manufacturer")))
        .WillOnce(Return(get(gpuAssociations[0], "Forward")))
        .WillOnce(Return(get(gpuAssociations[0], "Backward")))
        .WillOnce(Return(get(gpuAssociations[0], "AbsolutePath")))
        .WillOnce(Return(get(gpuAssociations[1], "Forward")))
        .WillOnce(Return(get(gpuAssociations[1], "Backward")))
        .WillOnce(Return(get(gpuAssociations[1], "AbsolutePath")))
        .WillOnce(Throw(utils::SdBusTestError(
            int(sdbusplus::UnpackErrorReason::missingProperty))));
    EXPECT_CALL(mockDBus, getServiceMap).WillOnce(Return(serviceMap));
    nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath);

    auto staticSensors = 0;
    auto dynamicSensors = 0;
    auto prioritySensors = 0;
    auto retimerAsset = dynamic_pointer_cast<NsmFirmwareInventory<AssetIntf>>(
        fpga.deviceSensors[staticSensors++]);
    EXPECT_NE(nullptr, retimerAsset);
    EXPECT_EQ(get<std::string>(retimer, "Manufacturer"),
              retimerAsset->pdi().manufacturer());

    auto retimerAssociation =
        dynamic_pointer_cast<NsmFirmwareInventory<AssociationDefinitionsIntf>>(
            fpga.deviceSensors[staticSensors++]);
    EXPECT_NE(nullptr, retimerAssociation);
    EXPECT_EQ(2, retimerAssociation->pdi().associations().size());

    auto retimerSettings =
        dynamic_pointer_cast<NsmFirmwareInventory<SettingsIntf>>(
            fpga.deviceSensors[staticSensors++]);
    EXPECT_NE(nullptr, retimerSettings);
    EXPECT_NE(nullptr,
              dynamic_cast<NsmWriteProtectedIntf*>(&retimerSettings->pdi()));

    auto retimerWriteProtectedSensor = dynamic_pointer_cast<NsmWriteProtectedControl>(
        fpga.roundRobinSensors[dynamicSensors++]);
    EXPECT_NE(nullptr, retimerWriteProtectedSensor);

    auto version = dynamic_pointer_cast<NsmInventoryProperty<VersionIntf>>(
        fpga.deviceSensors[staticSensors++]);
    EXPECT_NE(nullptr, version);
    EXPECT_EQ(PCIERETIMER_0_EEPROM_VERSION +
                  get<uint64_t>(retimer, "InstanceNumber"),
              version->property);

    auto gpuAsset = dynamic_pointer_cast<NsmFirmwareInventory<AssetIntf>>(
        fpga.deviceSensors[staticSensors++]);
    EXPECT_NE(nullptr, gpuAsset);
    EXPECT_EQ(get<std::string>(gpu, "Manufacturer"),
              gpuAsset->pdi().manufacturer());

    auto gpuAssociation =
        dynamic_pointer_cast<NsmFirmwareInventory<AssociationDefinitionsIntf>>(
            fpga.deviceSensors[staticSensors++]);
    EXPECT_NE(nullptr, gpuAssociation);
    EXPECT_EQ(2, gpuAssociation->pdi().associations().size());

    auto gpuSettings = dynamic_pointer_cast<NsmFirmwareInventory<SettingsIntf>>(
        fpga.deviceSensors[staticSensors++]);
    EXPECT_NE(nullptr, gpuSettings);
    EXPECT_NE(nullptr, dynamic_cast<NsmWriteProtectedIntf*>(&gpuSettings->pdi()));

    auto gpuWriteProtectedSensor = dynamic_pointer_cast<NsmWriteProtectedControl>(
        fpga.roundRobinSensors[dynamicSensors++]);
    EXPECT_NE(nullptr, gpuWriteProtectedSensor);

    EXPECT_EQ(staticSensors, fpga.deviceSensors.size());
    EXPECT_EQ(dynamicSensors, fpga.roundRobinSensors.size());
    EXPECT_EQ(prioritySensors, fpga.prioritySensors.size());
}

TEST_F(NsmFirmwareInventoryTest, badTestNoDevideFound)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(retimer, "Name")))
        .WillOnce(Return(get(retimer, "Type")))
        .WillOnce(Return(get(error, "UUID")))
        .WillOnce(Return(get(retimer, "DeviceType")))
        .WillOnce(Return(get(retimer, "InstanceNumber")));
    EXPECT_THROW(
        nsmFirmwareInventoryCreateSensors(mockManager, basicIntfName, objPath),
        std::runtime_error);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
}
