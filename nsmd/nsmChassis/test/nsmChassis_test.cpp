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

#include "nsmAssetIntf.hpp"
#include "nsmChassis.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmPowerSupplyStatus.hpp"
#include "nsmProcessor/nsmOemResetStatistics.hpp"
#include "nsmWriteProtectedJumper.hpp"

namespace nsm
{
requester::Coroutine nsmChassisCreateSensors(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
};

using namespace nsm;

struct NsmChassisTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Chassis";
    const std::string name = "HGX_GPU_SXM_1";
    const std::string objPath = chassisInventoryBasePath / name;

    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    const uuid_t gpuDeviceUuid = "000b3ec1-0068-0045-0086-000009062aa8";
    const uuid_t fpgaUuid = "992b3ec1-e464-f145-8686-409009062aa8";

    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(gpuUuid)},
        {std::make_shared<NsmDevice>(fpgaUuid)},
    };
    NsmDevice& gpu = *devices[0];
    NsmDevice& fpga = *devices[1];

    NsmChassisTest() : SensorManagerTest(devices)
    {
        gpu.deviceType = NSM_DEV_ID_GPU;
        fpga.deviceType = NSM_DEV_ID_BASEBOARD;
    }

    const PropertyValuesCollection error = {
        {"Type", "NSM_GPU_cassis"},
    };
    const PropertyValuesCollection basic = {
        {"Name", name},
        {"Type", "NSM_Chassis"},
        {"UUID", gpuUuid},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"DEVICE_UUID", gpuDeviceUuid},
    };
    const PropertyValuesCollection fpgaProperties = {
        {"Name", name},
        {"Type", "NSM_Chassis"},
        {"UUID", fpgaUuid},
        {"DeviceType", uint64_t(NSM_DEV_ID_BASEBOARD)},
        {"DEVICE_UUID", fpgaUuid},
        {"INSTANCE_NUMBER", uint64_t(0)},
    };
    const PropertyValuesCollection fpgaAsset = {
        {"Type", "NSM_FPGA_Asset"},
        {"Manufacturer", "NVIDIA"},
    };
    const PropertyValuesCollection asset = {
        {"Type", "NSM_Asset"},
        {"Manufacturer", "NVIDIA"},
    };
    const PropertyValuesCollection chassisType = {
        {"Type", "NSM_ChassisType"},
        {"ChassisType",
         "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module"},
    };
    const PropertyValuesCollection dimension = {
        {"Type", "NSM_Dimension"},
    };
    const PropertyValuesCollection location = {
        {"Type", "NSM_Location"},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
    };
    const PropertyValuesCollection locationCode = {
        {"Type", "NSM_LocationCode"},
        {"LocationCode", "SXM2"},
    };
    const PropertyValuesCollection health = {
        {"Type", "NSM_Health"},
        {"Health", "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"},
    };
    const PropertyValuesCollection powerLimit = {
        {"Type", "NSM_PowerLimit"},
        {"Priority", false},
    };
    const PropertyValuesCollection powerState = {
        {"Type", "NSM_PowerState"},
        {"InstanceNumber", uint64_t(2)},
        {"InventoryObjPaths",
         std::vector<std::string>{
             objPath,
             objPath + "/PCIeDevices/Device1",
         }},
        {"Priority", false},
    };
    const PropertyValuesCollection writeProtect = {
        {"Type", "NSM_WriteProtect"},
    };
    const PropertyValuesCollection association = {
        {"Forward", "pciedevice"},
        {"Backward", "chassis"},
        {"AbsolutePath",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_1/PCIeDevices/GPU_SXM_1"},
    };
    const MapperServiceMap gpuServiceMap = {
        {
            {
                "xyz.openbmc_project.NSM",
                {
                    "xyz.openbmc_project.Configuration.NSM_Chassis.Associations0",
                },
            },
        },
    };
    const MapperServiceMap fpgaServiceMap;
};

TEST_F(NsmChassisTest, badTestCreateDeviceSensors)
{
    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["Name"] = std::get<std::string>(get(basic, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(basic, "UUID").second);

    // Set up interface-specific properties with invalid type
    propertyMap["Type"] = std::get<std::string>(get(error, "Type").second);

    nsmChassisCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}
TEST_F(NsmChassisTest, goodTestCreateGpuChassis)
{
    auto& map = utils::MockDbusAsync::getServiceMap();
    map = gpuServiceMap;

    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["Name"] = std::get<std::string>(get(basic, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(basic, "UUID").second);

    // First call: NSM_Chassis
    propertyMap["Type"] = std::get<std::string>(get(basic, "Type").second);
    propertyMap["DeviceType"] =
        std::get<uint64_t>(get(basic, "DeviceType").second);
    propertyMap["DEVICE_UUID"] =
        std::get<uuid_t>(get(basic, "DEVICE_UUID").second);
    propertyMap["Forward"] =
        std::get<std::string>(get(association, "Forward").second);
    propertyMap["Backward"] =
        std::get<std::string>(get(association, "Backward").second);
    propertyMap["AbsolutePath"] =
        std::get<std::string>(get(association, "AbsolutePath").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Chassis", objPath);

    // Second call: NSM_ChassisType
    propertyMap["Type"] =
        std::get<std::string>(get(chassisType, "Type").second);
    propertyMap["ChassisType"] =
        std::get<std::string>(get(chassisType, "ChassisType").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".ChassisType",
                            objPath);

    // Third call: NSM_Health
    propertyMap["Type"] = std::get<std::string>(get(health, "Type").second);
    propertyMap["Health"] = std::get<std::string>(get(health, "Health").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Health", objPath);

    // Fourth call: NSM_Location
    propertyMap["Type"] = std::get<std::string>(get(location, "Type").second);
    propertyMap["LocationType"] =
        std::get<std::string>(get(location, "LocationType").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Location", objPath);

    // Fifth call: NSM_LocationCode
    propertyMap["Type"] =
        std::get<std::string>(get(locationCode, "Type").second);
    propertyMap["LocationCode"] =
        std::get<std::string>(get(locationCode, "LocationCode").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".LocationCode",
                            objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(7, gpu.staticSensors.size());
    EXPECT_EQ(7, gpu.deviceSensors.size());

    auto sensors = 0;
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
                           gpu.deviceSensors[sensors]));
    EXPECT_EQ(gpuDeviceUuid,
              dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
                  gpu.deviceSensors[sensors++])
                  ->invoke(pdiMethod(uuid)));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<MctpUuidIntf>>(
                           gpu.deviceSensors[sensors++]));
    EXPECT_NE(
        nullptr,
        dynamic_pointer_cast<NsmInterfaceProvider<AssociationDefinitionsInft>>(
            gpu.deviceSensors[sensors++]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<ChassisIntf>>(
                           gpu.deviceSensors[sensors++]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                           gpu.deviceSensors[sensors++]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                           gpu.deviceSensors[sensors++]));
    EXPECT_NE(nullptr,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationCodeIntf>>(
                  gpu.deviceSensors[sensors++]));

    sensors = 0;
    EXPECT_EQ(gpuDeviceUuid,
              dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
                  gpu.deviceSensors[sensors++])
                  ->invoke(pdiMethod(uuid)));
    EXPECT_EQ(gpuUuid, dynamic_pointer_cast<NsmInterfaceProvider<MctpUuidIntf>>(
                           gpu.deviceSensors[sensors++])
                           ->invoke(pdiMethod(uuid)));
    EXPECT_EQ(
        1,
        dynamic_pointer_cast<NsmInterfaceProvider<AssociationDefinitionsInft>>(
            gpu.deviceSensors[sensors++])
            ->invoke(pdiMethod(associations))
            .size());

    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<ChassisIntf>>(
                           gpu.deviceSensors[sensors]));
    EXPECT_EQ(ChassisIntf::ChassisType::Module,
              dynamic_pointer_cast<NsmInterfaceProvider<ChassisIntf>>(
                  gpu.deviceSensors[sensors++])
                  ->invoke(pdiMethod(type)));

    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                           gpu.deviceSensors[sensors]));
    EXPECT_EQ(HealthIntf::HealthType::OK,
              dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                  gpu.deviceSensors[sensors++])
                  ->invoke(pdiMethod(health)));

    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                           gpu.deviceSensors[sensors]));
    EXPECT_EQ(LocationIntf::LocationTypes::Embedded,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                  gpu.deviceSensors[sensors++])
                  ->invoke(pdiMethod(locationType)));

    EXPECT_NE(nullptr,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationCodeIntf>>(
                  gpu.deviceSensors[sensors]));
    EXPECT_EQ(get<std::string>(locationCode, "LocationCode"),
              dynamic_pointer_cast<NsmInterfaceProvider<LocationCodeIntf>>(
                  gpu.deviceSensors[sensors++])
                  ->invoke(pdiMethod(locationCode)));
}

TEST_F(NsmChassisTest, goodTestCreateBaseboardChassis)
{
    auto& map = utils::MockDbusAsync::getServiceMap();
    map = fpgaServiceMap;

    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["Name"] =
        std::get<std::string>(get(fpgaProperties, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(fpgaProperties, "UUID").second);

    // First call: NSM_Chassis
    propertyMap["Type"] =
        std::get<std::string>(get(fpgaProperties, "Type").second);
    propertyMap["DeviceType"] =
        std::get<uint64_t>(get(fpgaProperties, "DeviceType").second);
    propertyMap["DEVICE_UUID"] =
        std::get<uuid_t>(get(fpgaProperties, "DEVICE_UUID").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Chassis", objPath);

    // Second call: NSM_Asset
    propertyMap["Type"] = std::get<std::string>(get(fpgaAsset, "Type").second);
    propertyMap["Manufacturer"] =
        std::get<std::string>(get(fpgaAsset, "Manufacturer").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Asset", objPath);

    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(3, fpga.staticSensors.size());
    EXPECT_EQ(4, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());

    auto sensors = 0;

    auto chassisUuid = dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
        fpga.deviceSensors[sensors++]);
    auto mctpUuid = dynamic_pointer_cast<NsmInterfaceProvider<MctpUuidIntf>>(
        fpga.deviceSensors[sensors++]);
    auto pcieRefClock =
        dynamic_pointer_cast<NsmInterfaceProvider<PCIeRefClockIntf>>(
            fpga.deviceSensors[sensors++]);
    auto chassisAsset = dynamic_pointer_cast<NsmChassis<NsmAssetIntf>>(
        fpga.deviceSensors[sensors++]);
    EXPECT_NE(nullptr, chassisUuid);
    EXPECT_NE(nullptr, mctpUuid);
    EXPECT_NE(nullptr, pcieRefClock);
    EXPECT_NE(nullptr, chassisAsset);
    EXPECT_EQ(get<std::string>(asset, "Manufacturer"),
              chassisAsset->invoke(pdiMethod(manufacturer)));

    EXPECT_EQ(fpgaUuid, chassisUuid->invoke(pdiMethod(uuid)));
}

TEST_F(NsmChassisTest, goodTestCreateStaticSensors)
{
    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["Name"] = std::get<std::string>(get(basic, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(basic, "UUID").second);

    // First call: NSM_Asset
    propertyMap["Type"] = std::get<std::string>(get(asset, "Type").second);
    propertyMap["Manufacturer"] =
        std::get<std::string>(get(asset, "Manufacturer").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Asset", objPath);

    // Second call: NSM_Dimension
    propertyMap["Type"] = std::get<std::string>(get(dimension, "Type").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Dimension", objPath);

    // Third call: NSM_WriteProtect (for FPGA)
    propertyMap["Name"] =
        std::get<std::string>(get(fpgaProperties, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(fpgaProperties, "UUID").second);
    propertyMap["Type"] =
        std::get<std::string>(get(writeProtect, "Type").second);
    propertyMap["DeviceType"] =
        std::get<uint64_t>(get(fpgaProperties, "DeviceType").second);
    propertyMap["INSTANCE_NUMBER"] =
        std::get<uint64_t>(get(fpgaProperties, "INSTANCE_NUMBER").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".WriteProtect",
                            objPath);

    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(6, gpu.staticSensors.size());
    EXPECT_EQ(6, gpu.deviceSensors.size());

    auto sensors = 0;
    auto partNumber = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        gpu.deviceSensors[sensors++]);
    auto serialNumber =
        dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
            gpu.deviceSensors[sensors++]);
    auto model = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        gpu.deviceSensors[sensors++]);
    auto depth = dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
        gpu.deviceSensors[sensors++]);
    auto width = dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
        gpu.deviceSensors[sensors++]);
    auto height = dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
        gpu.deviceSensors[sensors++]);
    EXPECT_EQ(sensors, gpu.deviceSensors.size());

    EXPECT_NE(nullptr, partNumber);
    EXPECT_NE(nullptr, serialNumber);
    EXPECT_NE(nullptr, model);
    EXPECT_NE(nullptr, depth);
    EXPECT_NE(nullptr, width);
    EXPECT_NE(nullptr, height);

    EXPECT_EQ(FRU_PART_NUMBER, partNumber->property);
    EXPECT_EQ(SERIAL_NUMBER, serialNumber->property);
    EXPECT_EQ(MARKETING_NAME, model->property);
    EXPECT_EQ(PRODUCT_LENGTH, depth->property);
    EXPECT_EQ(PRODUCT_WIDTH, width->property);
    EXPECT_EQ(PRODUCT_HEIGHT, height->property);
    EXPECT_EQ(get<std::string>(asset, "Manufacturer"),
              model->invoke(pdiMethod(manufacturer)));

    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(1, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.staticSensors.size());
    EXPECT_EQ(1, fpga.deviceSensors.size());

    sensors = 0;
    auto writeProtectedJumper = dynamic_pointer_cast<NsmWriteProtectedJumper>(
        fpga.deviceSensors[sensors++]);
    EXPECT_EQ(sensors, fpga.deviceSensors.size());

    EXPECT_NE(nullptr, writeProtectedJumper);
}

TEST_F(NsmChassisTest, goodTestCreateDynamicSensors)
{
    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["Name"] = std::get<std::string>(get(basic, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(basic, "UUID").second);

    // First call: NSM_PowerLimit
    propertyMap["Type"] = std::get<std::string>(get(powerLimit, "Type").second);
    propertyMap["Priority"] =
        std::get<bool>(get(powerLimit, "Priority").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".PowerLimit",
                            objPath);

    // Second call: NSM_PowerState (for FPGA)
    propertyMap["Name"] =
        std::get<std::string>(get(fpgaProperties, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(fpgaProperties, "UUID").second);
    propertyMap["Type"] = std::get<std::string>(get(powerState, "Type").second);
    propertyMap["DeviceType"] =
        std::get<uint64_t>(get(fpgaProperties, "DeviceType").second);
    propertyMap["InstanceNumber"] =
        std::get<uint64_t>(get(powerState, "InstanceNumber").second);
    propertyMap["InventoryObjPaths"] = std::get<std::vector<std::string>>(
        get(powerState, "InventoryObjPaths").second);
    propertyMap["Priority"] =
        std::get<bool>(get(powerState, "Priority").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".PowerState",
                            objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(1, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.staticSensors.size());
    EXPECT_EQ(1, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(2, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.staticSensors.size());
    EXPECT_EQ(2, gpu.deviceSensors.size());
}

TEST_F(NsmChassisTest, badTestCreateStaticSensors)
{
    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["Name"] =
        std::get<std::string>(get(fpgaProperties, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(fpgaProperties, "UUID").second);

    // Set up interface-specific properties with mismatched device type
    propertyMap["Type"] =
        std::get<std::string>(get(writeProtect, "Type").second);
    propertyMap["DeviceType"] =
        std::get<uint64_t>(get(basic, "DeviceType").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".WriteProtect",
                            objPath);
}

TEST_F(NsmChassisTest, badTestCreateDynamicSensors)
{
    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["Name"] =
        std::get<std::string>(get(fpgaProperties, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(fpgaProperties, "UUID").second);

    // Set up interface-specific properties with mismatched device type
    propertyMap["Type"] = std::get<std::string>(get(powerState, "Type").second);
    propertyMap["DeviceType"] =
        std::get<uint64_t>(get(basic, "DeviceType").second);
    nsmChassisCreateSensors(mockManager, basicIntfName + ".PowerState",
                            objPath);
}

struct NsmInventoryPropertyTest : public NsmChassisTest
{
    NsmChassis<NsmAssetIntf> chassisAsset{name};
    NsmChassis<DimensionIntf> chassisDimension{name};
    NsmChassis<PowerLimitIntf> chassisPowerLimit{name};
    void SetUp() override
    {
        EXPECT_EQ(chassisAsset.getName(), name);
        EXPECT_EQ(chassisAsset.getType(), "NSM_Chassis");
        EXPECT_EQ(chassisDimension.getName(), name);
        EXPECT_EQ(chassisDimension.getType(), "NSM_Chassis");
        EXPECT_EQ(chassisPowerLimit.getName(), name);
        EXPECT_EQ(chassisPowerLimit.getType(), "NSM_Chassis");
    }

    std::shared_ptr<NsmInventoryPropertyBase> sensor;
    void testRequest()
    {
        auto request = sensor->genRequestMsg(eid, instanceId);
        EXPECT_TRUE(request.has_value());
        EXPECT_EQ(request.value().size(),
                  sizeof(nsm_msg_hdr) +
                      sizeof(nsm_get_inventory_information_req));
        auto requestPtr =
            reinterpret_cast<struct nsm_msg*>(request.value().data());
        uint8_t decodedProperty = 0;
        auto rc = decode_get_inventory_information_req(
            requestPtr, request.value().size(), &decodedProperty);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        EXPECT_EQ(decodedProperty, (uint8_t)sensor->property);
    }
    void testResponse(uint8_t* data, size_t valueSize)
    {
        std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      NSM_RESPONSE_CONVENTION_LEN + valueSize);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_get_inventory_information_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, valueSize, data, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmInventoryPropertyTest, goodTestPartNumberRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestPartNumberResponse)
{
    std::string partNumber = "PN12345";
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    testResponse((uint8_t*)partNumber.c_str(), partNumber.size());
    EXPECT_EQ(chassisAsset.invoke(pdiMethod(partNumber)), partNumber);
}
TEST_F(NsmInventoryPropertyTest, goodTestSerialNumberRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, SERIAL_NUMBER);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestSerialNumberResponse)
{
    std::string serialNumber = "SN12345";
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, SERIAL_NUMBER);
    testResponse((uint8_t*)serialNumber.c_str(), serialNumber.size());
    EXPECT_EQ(chassisAsset.invoke(pdiMethod(serialNumber)), serialNumber);
}
TEST_F(NsmInventoryPropertyTest, goodTestModelRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, MARKETING_NAME);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestModelResponse)
{
    std::string model = "NV123";
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, MARKETING_NAME);
    testResponse((uint8_t*)model.c_str(), model.size());
    EXPECT_EQ(chassisAsset.invoke(pdiMethod(model)), model);
}
TEST_F(NsmInventoryPropertyTest, goodTestDepthRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_LENGTH);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestDepthResponse)
{
    uint32_t depth = 850;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_LENGTH);
    testResponse((uint8_t*)&depth, sizeof(depth));
    EXPECT_EQ(chassisDimension.invoke(pdiMethod(depth)), (double)depth);
}
TEST_F(NsmInventoryPropertyTest, goodTestHeightRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_HEIGHT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestHeightResponse)
{
    uint32_t height = 2100;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_HEIGHT);
    testResponse((uint8_t*)&height, sizeof(height));
    EXPECT_EQ(chassisDimension.invoke(pdiMethod(height)), (double)height);
}
TEST_F(NsmInventoryPropertyTest, goodTestWidthRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_WIDTH);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestWidthResponse)
{
    uint32_t width = 712;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_WIDTH);
    testResponse((uint8_t*)&width, sizeof(width));
    EXPECT_EQ(chassisDimension.invoke(pdiMethod(width)), (double)width);
}
TEST_F(NsmInventoryPropertyTest, goodTestMinPowerWattsRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestMinPowerWattsResponse)
{
    uint32_t minPowerMilliWatts = 20000;
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT);
    testResponse((uint8_t*)&minPowerMilliWatts, sizeof(minPowerMilliWatts));
    EXPECT_EQ(chassisPowerLimit.invoke(pdiMethod(minPowerWatts)),
              (size_t)minPowerMilliWatts / 1000);
}
TEST_F(NsmInventoryPropertyTest, goodTestMaxPowerWattsRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestMaxPowerWattsResponse)
{
    uint32_t maxPowerMilliWatts = 100000;
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT);
    testResponse((uint8_t*)&maxPowerMilliWatts, sizeof(maxPowerMilliWatts));
    EXPECT_EQ(chassisPowerLimit.invoke(pdiMethod(maxPowerWatts)),
              (size_t)maxPowerMilliWatts / 1000);
}
TEST_F(NsmInventoryPropertyTest, badTestRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmInventoryPropertyTest, badTestResponseSize)
{
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, 0, nullptr, responseMsg);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmInventoryPropertyTest, badTestCompletionErrorResponse)
{
    uint8_t value = 0;
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  NSM_RESPONSE_CONVENTION_LEN + sizeof(value));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, sizeof(value), &value, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    struct nsm_get_inventory_information_resp* resp =
        (struct nsm_get_inventory_information_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    auto cc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_ERROR);
}
TEST_F(NsmInventoryPropertyTest, badTestNotImplementedResponse)
{
    try
    {
        uint8_t value = 0;
        sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            chassisAsset, MEMORY_VENDOR);
        testResponse(&value, sizeof(value));
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& err)
    {
        EXPECT_EQ(err.what(), std::string("Not implemented PDI"));
    }
    catch (...)
    {
        FAIL() << "Expected std::runtime_error";
    }
}

struct NsmPowerSupplyStatusTest : public NsmChassisTest
{
    NsmInterfaceProvider<PowerStateIntf> chassisPowerState{
        name, "NSM_PowerState", "/xyz/openbmc_project/dummy"};

    void SetUp() override
    {
        EXPECT_EQ(chassisPowerState.getName(), name);
        EXPECT_EQ(chassisPowerState.getType(), "NSM_PowerState");
    }
    std::shared_ptr<NsmPowerSupplyStatus> sensor;
    void init(uint8_t gpuInstanceId)
    {
        eid = 12;
        sensor = std::make_shared<NsmPowerSupplyStatus>(chassisPowerState,
                                                        gpuInstanceId);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), "NSM_PowerState");
        EXPECT_EQ(sensor->gpuInstanceId, gpuInstanceId);
    }
    void testResponse(uint8_t status)
    {
        std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_get_power_supply_status_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_get_power_supply_status_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, status, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmPowerSupplyStatusTest, goodTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    fpga_diagnostics_settings_data_index data_index;
    auto rc = decode_get_fpga_diagnostics_settings_req(
        requestPtr, request.value().size(), &data_index);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(data_index, GET_POWER_SUPPLY_STATUS);
}
TEST_F(NsmPowerSupplyStatusTest, goodTestResponse)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using PowerState =
            sdbusplus::xyz::openbmc_project::State::server::Chassis::PowerState;
        for (auto state : {PowerState::Off, PowerState::On})
        {
            uint8_t status = (state == PowerState::On) << i;
            testResponse(status);
            EXPECT_EQ(chassisPowerState.invoke(pdiMethod(currentPowerState)),
                      state);
        }
    }
}
TEST_F(NsmPowerSupplyStatusTest, badTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPowerSupplyStatusTest, badTestResponseSize)
{
    init(0);
    uint8_t status = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_power_supply_status_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_power_supply_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, status, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size() - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmPowerSupplyStatusTest, badTestCompletionErrorResponse)
{
    init(0);
    uint8_t status = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_power_supply_status_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_power_supply_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, status, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    struct nsm_get_power_supply_status_resp* resp =
        (struct nsm_get_power_supply_status_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    auto cc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

struct NsmGpuPresenceAndPowerStatusTest : public NsmChassisTest
{
    const PropertyValuesCollection operationalStatus = {
        {"Name", "HGX_GPU_SXM_1"},
        {"Type", "NSM_OperationalStatus"},
        {"UUID", fpgaUuid},
        {"DeviceType", uint64_t(NSM_DEV_ID_BASEBOARD)},
        {"InstanceNumber", uint64_t(0)},
        {"InventoryObjPaths",
         std::vector<std::string>{objPath,
                                  processorsInventoryBasePath / "GPU_SXM_1"}},
        {"Priority", true},
    };
    std::shared_ptr<NsmGpuPresenceAndPowerStatus> sensor,
        chassisOperationalStatus;
    void SetUp() override
    {
        auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
        propertyMap.clear();

        // Set up base properties that coGetCachedBaseProperties needs
        propertyMap["Name"] =
            std::get<std::string>(get(operationalStatus, "Name").second);
        propertyMap["UUID"] =
            std::get<uuid_t>(get(operationalStatus, "UUID").second);

        // Set up interface-specific properties for OperationalStatus
        propertyMap["Type"] =
            std::get<std::string>(get(operationalStatus, "Type").second);
        propertyMap["DeviceType"] =
            std::get<uint64_t>(get(operationalStatus, "DeviceType").second);
        propertyMap["InstanceNumber"] =
            std::get<uint64_t>(get(operationalStatus, "InstanceNumber").second);
        propertyMap["InventoryObjPaths"] = std::get<std::vector<std::string>>(
            get(operationalStatus, "InventoryObjPaths").second);
        propertyMap["Priority"] =
            std::get<bool>(get(operationalStatus, "Priority").second);
        nsmChassisCreateSensors(mockManager,
                                basicIntfName + ".OperationalStatus", objPath);

        EXPECT_EQ(1, fpga.deviceSensors.size());
        EXPECT_EQ(1, fpga.prioritySensors.size());
        chassisOperationalStatus =
            std::dynamic_pointer_cast<NsmGpuPresenceAndPowerStatus>(
                fpga.deviceSensors[0]);
        EXPECT_NE(chassisOperationalStatus, nullptr);
        EXPECT_EQ(0, chassisOperationalStatus->sensors.size());

        for (size_t i = 1; i < 8; i++)
        {
            auto gpuName = ("HGX_GPU_SXM_" + std::to_string(i + 1));
            auto gpuPath = chassisInventoryBasePath / gpuName;
            auto gpuOperationalStatus = operationalStatus;
            gpuOperationalStatus[0].second = gpuName.c_str();
            gpuOperationalStatus[4].second = uint64_t(i);
            gpuOperationalStatus[5].second = std::vector<std::string>{
                gpuPath, processorsInventoryBasePath /
                             ("GPU_SXM_" + std::to_string(i + 1))};

            // Update propertyMap for each GPU sensor
            propertyMap["Name"] =
                std::get<std::string>(get(gpuOperationalStatus, "Name").second);
            propertyMap["Type"] =
                std::get<std::string>(get(gpuOperationalStatus, "Type").second);
            propertyMap["UUID"] =
                std::get<uuid_t>(get(gpuOperationalStatus, "UUID").second);
            propertyMap["DeviceType"] = std::get<uint64_t>(
                get(gpuOperationalStatus, "DeviceType").second);
            propertyMap["InstanceNumber"] = std::get<uint64_t>(
                get(gpuOperationalStatus, "InstanceNumber").second);
            propertyMap["InventoryObjPaths"] =
                std::get<std::vector<std::string>>(
                    get(gpuOperationalStatus, "InventoryObjPaths").second);
            propertyMap["Priority"] =
                std::get<bool>(get(gpuOperationalStatus, "Priority").second);
            nsmChassisCreateSensors(
                mockManager, basicIntfName + ".OperationalStatus", gpuPath);
        }
        // Sensors shall be added as sub sensor
        EXPECT_EQ(1, fpga.deviceSensors.size());
        EXPECT_EQ(1, fpga.prioritySensors.size());
        EXPECT_EQ(7, chassisOperationalStatus->sensors.size());
    }
    void init(uint8_t gpuInstanceId)
    {
        if (gpuInstanceId == 0)
        {
            sensor = chassisOperationalStatus;
        }
        else
        {
            sensor = std::dynamic_pointer_cast<NsmGpuPresenceAndPowerStatus>(
                chassisOperationalStatus->sensors[gpuInstanceId - 1]);
        }
    }

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
    void testResponse(uint8_t presence, uint8_t power_status)
    {
        const Response presenceMsg{presence};
        const Response powerStatusMsg{power_status};
        EXPECT_CALL(mockManager, SendRecvNsmMsg)
            .Times(2)
            .WillOnce(mockSendRecvNsmMsg(diagHeader, presenceMsg))
            .WillOnce(mockSendRecvNsmMsg(diagHeader, powerStatusMsg));
        sensor->update(mockManager, eid);
    }
};
TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestRequest)
{
    init(0);
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPresence;

    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    fpga_diagnostics_settings_data_index data_index;
    auto rc = decode_get_fpga_diagnostics_settings_req(
        requestPtr, request.value().size(), &data_index);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(data_index, GET_GPU_PRESENCE);
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;

    request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
    requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    rc = decode_get_fpga_diagnostics_settings_req(
        requestPtr, request.value().size(), &data_index);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(data_index, GET_GPU_POWER_STATUS);
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestGpuStatusEnabledResponse)
{
    // "State": "Enabled" if presence=active, power=active
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0x01 << i, 0x01 << i);
        EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Enabled);
    }
}
TEST_F(NsmGpuPresenceAndPowerStatusTest,
       goodTestGpuStatusUnavailableOfflineResponse)
{
    //"State": "UnavailableOffline" if presence=active,
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0x01 << i, 0x00);
        EXPECT_EQ(sensor->invoke(pdiMethod(state)),
                  StateType::UnavailableOffline);
    }
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestGpuStatusFaultResponse)
{
    init(0);
    using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
        server::OperationalStatus::StateType;
    const Response presenceMsg{0};
    const Response powerStatusMsg{0};
    auto ccError = diagHeader;
    ccError[6] = NSM_ERROR;

    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsg(diagHeader, presenceMsg))
        .WillOnce(mockSendRecvNsmMsg(diagHeader, powerStatusMsg, NSM_ERROR));
    sensor->update(mockManager, eid);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);

    sensor->invoke(pdiMethod(state), StateType::None);
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(diagHeader, presenceMsg, NSM_ERROR));
    sensor->update(mockManager, eid);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);

    sensor->invoke(pdiMethod(state), StateType::None);
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(ccError, presenceMsg));
    sensor->update(mockManager, eid);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);

    sensor->invoke(pdiMethod(state), StateType::None);
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsg(diagHeader, presenceMsg))
        .WillOnce(mockSendRecvNsmMsg(ccError, powerStatusMsg));
    sensor->update(mockManager, eid);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestGpuStatusAbsentResponse)
{
    // power=inactive "State": "Absent" if presence=inactive
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0, 0);
        EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Absent);
    }
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestRequest)
{
    init(0);
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestResponseSize)
{
    init(0);
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    uint8_t presence = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpu_presence_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_gpu_presence_resp(instanceId, NSM_SUCCESS, ERR_NULL,
                                           presence, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size() - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestCompletionErrorResponse)
{
    init(0);
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    uint8_t power_status = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpu_power_status_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_gpu_power_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, power_status, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = reinterpret_cast<struct nsm_get_gpu_power_status_resp*>(
        responseMsg->payload);
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}
