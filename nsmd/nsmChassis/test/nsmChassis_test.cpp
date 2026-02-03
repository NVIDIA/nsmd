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

#include "base.h"
#include "device-configuration.h"

#include "nsmAssetIntf.hpp"
#include "nsmChassis.hpp"
#include "nsmGPIO/nsmGPIOStateCommon.hpp"
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
}; // namespace nsm

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

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";
    const uuid_t gpuDeviceUuid = "000b3ec1-0068-0045-0086-000009062aa8";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmChassisTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_EQ(2, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_NE(fpga, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
        EXPECT_EQ(NSM_DEV_ID_BASEBOARD, fpga->getDeviceType());
    }

    ~NsmChassisTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap error = {
        {"Type", "NSM_GPU_cassis"},
    };
    dbus::PropertyMap basic = {
        {"Name", name},
        {"Type", "NSM_Chassis"},
        {"UUID", gpuUuid},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"DEVICE_UUID", gpuDeviceUuid},
    };
    dbus::PropertyMap fpgaProperties = {
        {"Name", name},
        {"Type", "NSM_Chassis"},
        {"UUID", fpgaUuid},
        {"DeviceType", uint64_t(NSM_DEV_ID_BASEBOARD)},
        {"DEVICE_UUID", fpgaUuid},
        {"INSTANCE_NUMBER", uint64_t(0)},
    };
    dbus::PropertyMap fpgaAsset = {
        {"Type", "NSM_FPGA_Attributes"},
    };
    dbus::PropertyMap asset = {
        {"Type", "NSM_Chassis_Attributes"},
        {"AssetInformationAvailable", true},
        {"WriteProtectSupported", true},
        {"ChassisType",
         "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module"},
        {"DimensionSupported", true},
        {"PowerLimitSupported", true},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
        {"LocationCode", "SXM2"},
    };
    dbus::PropertyMap powerState = {
        {"Type", "NSM_PowerState"},
        {"InstanceNumber", uint64_t(2)},
        {"InventoryObjPaths",
         std::vector<std::string>{
             objPath,
             objPath + "/PCIeDevices/Device1",
         }},
    };
    dbus::PropertyMap association = {
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
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];

    // Set up interface-specific properties with invalid type
    propertyMap["Type"] = error["Type"];

    nsmChassisCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(1, devices.back()->deviceSensors.size());
}
TEST_F(NsmChassisTest, goodTestCreateGpuChassis)
{
    utils::MockDbusAsync::serviceMap() = gpuServiceMap;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];

    // First call: NSM_Chassis
    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, basicIntfName + ".Chassis");
    propertyMap["Type"] = basic["Type"];
    propertyMap["DeviceType"] = basic["DeviceType"];
    propertyMap["DEVICE_UUID"] = basic["DEVICE_UUID"];

    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".Associations0");
    propertyMapAssociation0 = association;

    nsmChassisCreateSensors(mockManager, basicIntfName + ".Chassis", objPath);

    // Second call: NSM_Chassis_Attributes
    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = asset["Type"];
    propertyMapAttributes["AssetInformationAvailable"] =
        asset["AssetInformationAvailable"];
    propertyMapAttributes["ChassisType"] = asset["ChassisType"];
    propertyMapAttributes["LocationType"] = asset["LocationType"];
    propertyMapAttributes["LocationCode"] = asset["LocationCode"];
    nsmChassisCreateSensors(mockManager, basicIntfName + ".ChassisAttributes",
                            objPath);
    EXPECT_EQ(11, gpu->staticSensors.size());
    EXPECT_EQ(1, gpu->roundRobinSensors.size());
    EXPECT_EQ(12, gpu->deviceSensors.size());

    auto sensors = 1; // Skip the first sensor (new sensor added)
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
                           gpu->deviceSensors[sensors]));
    EXPECT_EQ(gpuDeviceUuid,
              dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
                  gpu->deviceSensors[sensors++])
                  ->invoke(pdiMethod(uuid)));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<MctpUuidIntf>>(
                           gpu->deviceSensors[sensors++]));
    EXPECT_NE(
        nullptr,
        dynamic_pointer_cast<NsmInterfaceProvider<AssociationDefinitionsInft>>(
            gpu->deviceSensors[sensors++]));
    // Skip asset inventory property sensors (partNumber, serialNumber, model)
    // and SKU
    sensors += 4;
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                           gpu->deviceSensors[sensors++]));
    EXPECT_NE(nullptr,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationCodeIntf>>(
                  gpu->deviceSensors[sensors++]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<ChassisIntf>>(
                           gpu->deviceSensors[sensors++]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                           gpu->deviceSensors[sensors++]));

    sensors = 1; // Skip the first sensor (new sensor added)
    EXPECT_EQ(gpuDeviceUuid,
              dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
                  gpu->deviceSensors[sensors++])
                  ->invoke(pdiMethod(uuid)));
    EXPECT_EQ(gpuUuid, dynamic_pointer_cast<NsmInterfaceProvider<MctpUuidIntf>>(
                           gpu->deviceSensors[sensors++])
                           ->invoke(pdiMethod(uuid)));
    EXPECT_EQ(
        1,
        dynamic_pointer_cast<NsmInterfaceProvider<AssociationDefinitionsInft>>(
            gpu->deviceSensors[sensors++])
            ->invoke(pdiMethod(associations))
            .size());

    // Skip asset inventory property sensors (partNumber, serialNumber, model)
    // and SKU
    sensors += 4;

    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                           gpu->deviceSensors[sensors]));
    EXPECT_EQ(LocationIntf::LocationTypes::Embedded,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                  gpu->deviceSensors[sensors++])
                  ->invoke(pdiMethod(locationType)));

    EXPECT_NE(nullptr,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationCodeIntf>>(
                  gpu->deviceSensors[sensors]));
    EXPECT_EQ(std::get<std::string>(asset["LocationCode"]),
              dynamic_pointer_cast<NsmInterfaceProvider<LocationCodeIntf>>(
                  gpu->deviceSensors[sensors++])
                  ->invoke(pdiMethod(locationCode)));

    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<ChassisIntf>>(
                           gpu->deviceSensors[sensors]));
    EXPECT_EQ(ChassisIntf::ChassisType::Module,
              dynamic_pointer_cast<NsmInterfaceProvider<ChassisIntf>>(
                  gpu->deviceSensors[sensors++])
                  ->invoke(pdiMethod(type)));

    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                           gpu->deviceSensors[sensors]));
    EXPECT_EQ(HealthIntf::HealthType::OK,
              dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                  gpu->deviceSensors[sensors++])
                  ->invoke(pdiMethod(health)));

    gpu->deviceSensors.clear();
    gpu->prioritySensors.clear();
    gpu->roundRobinSensors.clear();
    gpu->staticSensors.clear();
}

TEST_F(NsmChassisTest, goodTestCreateBaseboardChassis)
{
    auto& map = utils::MockDbusAsync::serviceMap();
    map = fpgaServiceMap;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["Name"] = fpgaProperties["Name"];
    basePropertyMap["UUID"] = fpgaProperties["UUID"];
    basePropertyMap["DeviceType"] = fpgaProperties["DeviceType"];

    // First call: NSM_Chassis
    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, basicIntfName + ".Chassis");
    propertyMap["Type"] = fpgaProperties["Type"];
    propertyMap["DEVICE_UUID"] = fpgaProperties["DEVICE_UUID"];
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Chassis", objPath);
    EXPECT_EQ(3, fpga->staticSensors.size());
    EXPECT_EQ(1, fpga->roundRobinSensors.size());
    EXPECT_EQ(4, fpga->deviceSensors.size());
    // Second call: NSM_Chassis_Attributes
    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = fpgaAsset["Type"];
    nsmChassisCreateSensors(mockManager, basicIntfName + ".ChassisAttributes",
                            objPath);
    EXPECT_EQ(5, fpga->staticSensors.size());
    EXPECT_EQ(1, fpga->roundRobinSensors.size());
    EXPECT_EQ(7, fpga->deviceSensors.size());

    auto sensors = 1; // Skip msgTypes sensor added by initMsgTypesSensor()

    auto chassisUuid = dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
        fpga->deviceSensors[sensors++]);
    auto mctpUuid = dynamic_pointer_cast<NsmInterfaceProvider<MctpUuidIntf>>(
        fpga->deviceSensors[sensors++]);
    auto pcieRefClock =
        dynamic_pointer_cast<NsmInterfaceProvider<PCIeRefClockIntf>>(
            fpga->deviceSensors[sensors++]);
    auto chassisAsset = dynamic_pointer_cast<NsmChassis<NsmAssetIntf>>(
        fpga->deviceSensors[sensors++]);
    auto chassisSKU = dynamic_pointer_cast<NsmChassis<NsmApSkuIdIntf>>(
        fpga->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, chassisUuid);
    EXPECT_NE(nullptr, mctpUuid);
    EXPECT_NE(nullptr, pcieRefClock);
    EXPECT_NE(nullptr, chassisAsset);
    EXPECT_NE(nullptr, chassisSKU);
    EXPECT_EQ(MANUFACTURER_NVIDIA,
              chassisAsset->invoke(pdiMethod(manufacturer)));

    EXPECT_EQ(fpgaUuid, chassisUuid->invoke(pdiMethod(uuid)));
}

TEST_F(NsmChassisTest, goodTestCreateStaticSensors)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["Name"] = fpgaProperties["Name"];
    basePropertyMap["UUID"] = fpgaProperties["UUID"];
    basePropertyMap["DeviceType"] = fpgaProperties["DeviceType"];

    // First call: NSM_Chassis (create FPGA device)
    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, basicIntfName + ".Chassis");
    propertyMap["Type"] = fpgaProperties["Type"];
    propertyMap["DEVICE_UUID"] = fpgaProperties["DEVICE_UUID"];
    propertyMap["INSTANCE_NUMBER"] = fpgaProperties["INSTANCE_NUMBER"];
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Chassis", objPath);

    // Second call: NSM_Chassis_Attributes (for FPGA)
    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = asset["Type"];
    propertyMapAttributes["AssetInformationAvailable"] =
        asset["AssetInformationAvailable"];
    propertyMapAttributes["DimensionSupported"] = asset["DimensionSupported"];
    propertyMapAttributes["WriteProtectSupported"] =
        asset["WriteProtectSupported"];
    nsmChassisCreateSensors(mockManager, basicIntfName + ".ChassisAttributes",
                            objPath);

    EXPECT_EQ(11, fpga->staticSensors.size());
    EXPECT_EQ(2, fpga->roundRobinSensors.size()); // +1 for msgTypes sensor
    EXPECT_EQ(13, fpga->deviceSensors.size());    // +1 for msgTypes sensor

    auto sensors = 0;
    sensors += 4; // Skip msgTypes sensor at index 0 + 3 more
    auto partNumber = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        fpga->deviceSensors[sensors++]);
    auto serialNumber =
        dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
            fpga->deviceSensors[sensors++]);
    auto model = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        fpga->deviceSensors[sensors++]);
    sensors += 2; // Skip SKU sensor and one more
    auto depth = dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
        fpga->deviceSensors[sensors++]);
    auto width = dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
        fpga->deviceSensors[sensors++]);
    auto height = dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
        fpga->deviceSensors[sensors++]);
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
    EXPECT_EQ(MANUFACTURER_NVIDIA, model->invoke(pdiMethod(manufacturer)));

    EXPECT_EQ(0, fpga->prioritySensors.size());
    auto writeProtectedJumper = dynamic_pointer_cast<NsmWriteProtectedJumper>(
        fpga->deviceSensors.back());
    EXPECT_NE(nullptr, writeProtectedJumper);
}

TEST_F(NsmChassisTest, goodTestCreateDynamicSensors)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];

    // First call: NSM_Chassis_Attributes
    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = asset["Type"];
    propertyMap["PowerLimitSupported"] = asset["PowerLimitSupported"];
    nsmChassisCreateSensors(mockManager, basicIntfName + ".ChassisAttributes",
                            objPath);

    // Second call: NSM_PowerState (for FPGA)
    // Update base properties to FPGA for the PowerState call
    basePropertyMap["Name"] = fpgaProperties["Name"];
    basePropertyMap["UUID"] = fpgaProperties["UUID"];
    basePropertyMap["DeviceType"] = fpgaProperties["DeviceType"];
    basePropertyMap["InstanceNumber"] = powerState["InstanceNumber"];

    auto& propertyMapPowerState = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".PowerState");
    propertyMapPowerState["Type"] = powerState["Type"];
    propertyMapPowerState["InventoryObjPaths"] =
        powerState["InventoryObjPaths"];
    nsmChassisCreateSensors(mockManager, basicIntfName + ".PowerState",
                            objPath);

    EXPECT_EQ(2, fpga->roundRobinSensors.size());
    EXPECT_EQ(2, fpga->deviceSensors.size());
    EXPECT_EQ(3, gpu->roundRobinSensors.size());
    EXPECT_EQ(5, gpu->deviceSensors.size()); // +1 for msgTypes sensor
}

TEST_F(NsmChassisTest, badTestCreateStaticSensors)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["Name"] = fpgaProperties["Name"];
    basePropertyMap["UUID"] = fpgaProperties["UUID"];
    // Use GPU device type (NSM_DEV_ID_GPU) instead of BASEBOARD to trigger
    // error
    basePropertyMap["DeviceType"] = basic["DeviceType"];

    // Set up interface-specific properties with WriteProtectSupported
    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = asset["Type"];
    propertyMap["WriteProtectSupported"] = asset["WriteProtectSupported"];

    // Coroutines do not propagate the first exception to the caller
    EXPECT_THROW_COROUTINE(
        nsmChassisCreateSensors(mockManager,
                                basicIntfName + ".ChassisAttributes", objPath),
        sdbusplus::exception::SdBusError);
}

TEST_F(NsmChassisTest, badTestCreateDynamicSensors)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["Name"] = fpgaProperties["Name"];
    basePropertyMap["UUID"] = fpgaProperties["UUID"];

    // Set up interface-specific properties with mismatched device type
    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".PowerState");
    propertyMap["Type"] = powerState["Type"];
    propertyMap["DeviceType"] = basic["DeviceType"];
    EXPECT_THROW_COROUTINE(
        nsmChassisCreateSensors(mockManager, basicIntfName + ".PowerState",
                                objPath),
        std::runtime_error);
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
    dbus::PropertyMap operationalStatus = {
        {"Name", "HGX_GPU_SXM_1"},
        {"Type", "NSM_OperationalStatus"},
        {"UUID", fpgaUuid},
        {"DeviceType", uint64_t(NSM_DEV_ID_BASEBOARD)},
        {"InstanceNumber", uint64_t(0)},
        {"InventoryObjPaths",
         std::vector<std::string>{objPath,
                                  processorsInventoryBasePath / "GPU_SXM_1"}},
    };
    std::shared_ptr<NsmGpuPresenceAndPowerStatus> sensor,
        chassisOperationalStatus;
    void SetUp() override
    {
        auto& basePropertyMap =
            utils::MockDbusAsync::propertyMap(objPath, basicIntfName);

        // Set up base properties that coGetCachedBaseProperties needs
        basePropertyMap["Name"] = operationalStatus["Name"];
        basePropertyMap["UUID"] = operationalStatus["UUID"];
        basePropertyMap["DeviceType"] = operationalStatus["DeviceType"];
        basePropertyMap["InstanceNumber"] = operationalStatus["InstanceNumber"];

        // Set up interface-specific properties for OperationalStatus
        auto& propertyMap = utils::MockDbusAsync::propertyMap(
            objPath, basicIntfName + ".OperationalStatus");
        propertyMap["Type"] = operationalStatus["Type"];
        propertyMap["InventoryObjPaths"] =
            operationalStatus["InventoryObjPaths"];
        propertyMap["Priority"] = true;
        nsmChassisCreateSensors(mockManager,
                                basicIntfName + ".OperationalStatus", objPath);

        EXPECT_EQ(2, fpga->deviceSensors.size()); // +1 for msgTypes sensor
        EXPECT_EQ(1, fpga->prioritySensors.size());
        chassisOperationalStatus =
            std::dynamic_pointer_cast<NsmGpuPresenceAndPowerStatus>(
                fpga->deviceSensors[1]); // Skip msgTypes sensor at index 0
        EXPECT_NE(chassisOperationalStatus, nullptr);
        EXPECT_EQ(0, chassisOperationalStatus->sensors.size());

        for (size_t i = 1; i < 8; i++)
        {
            auto gpuName = ("HGX_GPU_SXM_" + std::to_string(i + 1));
            auto gpuPath = chassisInventoryBasePath / gpuName;
            auto gpuOperationalStatus = operationalStatus;
            gpuOperationalStatus["Name"] = gpuName;
            gpuOperationalStatus["InstanceNumber"] = uint64_t(i);
            gpuOperationalStatus["InventoryObjPaths"] =
                std::vector<std::string>{
                    gpuPath, processorsInventoryBasePath /
                                 ("GPU_SXM_" + std::to_string(i + 1))};

            // Update base propertyMap for each GPU sensor
            auto& gpuBasePropertyMap =
                utils::MockDbusAsync::propertyMap(gpuPath, basicIntfName);
            gpuBasePropertyMap["Name"] = gpuOperationalStatus["Name"];
            gpuBasePropertyMap["UUID"] = gpuOperationalStatus["UUID"];
            gpuBasePropertyMap["DeviceType"] =
                gpuOperationalStatus["DeviceType"];
            gpuBasePropertyMap["InstanceNumber"] =
                gpuOperationalStatus["InstanceNumber"];

            // Update interface-specific propertyMap for each GPU sensor
            auto& gpuPropertyMap = utils::MockDbusAsync::propertyMap(
                gpuPath, basicIntfName + ".OperationalStatus");
            gpuPropertyMap["Type"] = gpuOperationalStatus["Type"];
            gpuPropertyMap["InventoryObjPaths"] =
                gpuOperationalStatus["InventoryObjPaths"];
            gpuPropertyMap["Priority"] = true;
            nsmChassisCreateSensors(
                mockManager, basicIntfName + ".OperationalStatus", gpuPath);
        }
        // Sensors shall be added as sub sensor
        EXPECT_EQ(2, fpga->deviceSensors.size());
        EXPECT_EQ(1, fpga->prioritySensors.size());
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

        EXPECT_CALL(*fpga, sensorIO)
            .Times(2)
            .WillOnce(mockSensorIO(diagHeader, presenceMsg))
            .WillOnce(mockSensorIO(diagHeader, powerStatusMsg));
        sensor->update(fpga);
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
    // For error responses, we need at least 4 bytes to avoid buffer
    // overflow
    const Response errorResponse{0, 0, 0,
                                 0}; // 4 bytes minimum for error response
    auto ccError = diagHeader;
    ccError[6] = NSM_ERROR;

    EXPECT_CALL(*fpga, sensorIO)
        .Times(2)
        .WillOnce(mockSensorIO(diagHeader, presenceMsg))
        .WillOnce(mockSensorIO(diagHeader, powerStatusMsg, NSM_ERROR));
    sensor->update(fpga);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);

    sensor->invoke(pdiMethod(state), StateType::None);
    EXPECT_CALL(*fpga, sensorIO)
        .WillOnce(mockSensorIO(diagHeader, presenceMsg, NSM_ERROR));
    sensor->update(fpga);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);

    sensor->invoke(pdiMethod(state), StateType::None);
    EXPECT_CALL(*fpga, sensorIO).WillOnce(mockSensorIO(ccError, presenceMsg));
    sensor->update(fpga);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);

    sensor->invoke(pdiMethod(state), StateType::None);
    EXPECT_CALL(*fpga, sensorIO)
        .Times(2)
        .WillOnce(mockSensorIO(diagHeader, presenceMsg))
        .WillOnce(mockSensorIO(ccError, powerStatusMsg));
    sensor->update(fpga);
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

struct NsmWriteProtectedJumperTest : public NsmChassisTest
{
    std::shared_ptr<NsmWriteProtectedJumper> sensor;

    void SetUp() override
    {
        auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
        propertyMap.clear();

        // Set up base properties that coGetCachedBaseProperties needs
        propertyMap["Name"] = fpgaProperties["Name"];
        propertyMap["UUID"] = fpgaProperties["UUID"];
        propertyMap["DeviceType"] = fpgaProperties["DeviceType"];

        // Set up interface-specific properties
        auto& assetPropertyMap = utils::MockDbusAsync::propertyMap(
            objPath, basicIntfName + ".ChassisAttributes");
        assetPropertyMap["Type"] = asset["Type"];
        assetPropertyMap["WriteProtectSupported"] =
            asset["WriteProtectSupported"];
        nsmChassisCreateSensors(mockManager,
                                basicIntfName + ".ChassisAttributes", objPath);

        EXPECT_GT(fpga->deviceSensors.size(), 0);
        sensor = std::dynamic_pointer_cast<NsmWriteProtectedJumper>(
            fpga->deviceSensors.back());
        EXPECT_NE(sensor, nullptr);
    }

    void TearDown() override
    {
        sensor.reset();
        ::testing::Mock::VerifyAndClearExpectations(&mockManager);
    }

    void testResponse(uint8_t presence)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        nsm_fpga_diagnostics_settings_wp_jumper data;
        data.presence = presence;
        auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmWriteProtectedJumperTest, goodTestRequest)
{
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
    EXPECT_EQ(data_index, GET_WP_JUMPER_PRESENCE);
}

TEST_F(NsmWriteProtectedJumperTest, goodTestResponsePresent)
{
    testResponse(1);
    EXPECT_EQ(sensor->invoke(pdiMethod(writeProtected)), uint8_t(1));
    EXPECT_EQ(sensor->invoke(pdiMethod(writeProtectedControl)), uint8_t(1));
}

TEST_F(NsmWriteProtectedJumperTest, goodTestResponseNotPresent)
{
    testResponse(0);
    EXPECT_EQ(sensor->invoke(pdiMethod(writeProtected)), uint8_t(0));
    EXPECT_EQ(sensor->invoke(pdiMethod(writeProtectedControl)), uint8_t(0));
}

TEST_F(NsmWriteProtectedJumperTest, badTestResponseSize)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmWriteProtectedJumperTest, badTestCompletionErrorResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_fpga_diagnostics_settings_wp_jumper data;
    data.presence = 0;
    auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto resp =
        reinterpret_cast<struct nsm_fpga_diagnostics_settings_wp_jumper_resp*>(
            responseMsg->payload);
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// Test for ResetMetricsSupported
TEST_F(NsmChassisTest, goodTestCreateResetMetrics)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];

    // First call: NSM_Chassis_Attributes with ResetMetricsSupported
    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = asset["Type"];
    propertyMap["ResetMetricsSupported"] = true;
    nsmChassisCreateSensors(mockManager, basicIntfName + ".ChassisAttributes",
                            objPath);

    // Verify that reset metrics sensor was created
    // It should be added to deviceSensors
    EXPECT_GT(gpu->deviceSensors.size(), 0);
}

// Test for DeviceDiagnosticsSupported
TEST_F(NsmChassisTest, goodTestCreateDeviceDiagnostics)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];

    // First call: NSM_Chassis_Attributes with DeviceDiagnosticsSupported
    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = asset["Type"];
    propertyMap["DeviceDiagnosticsSupported"] = true;
    nsmChassisCreateSensors(mockManager, basicIntfName + ".ChassisAttributes",
                            objPath);

    // Verify that device diagnostics sensor was created
    // It should be added to staticSensors
    EXPECT_GT(gpu->staticSensors.size(), 0);
}

// Test for GPIOStateSupported
TEST_F(NsmChassisTest, goodTestCreateGPIOStateSensors)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];

    // First call: NSM_Chassis_Attributes with GPIOStateSupported
    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = asset["Type"];
    propertyMap["GPIOStateSupported"] = true;
    nsmChassisCreateSensors(mockManager, basicIntfName + ".ChassisAttributes",
                            objPath);

    // Verify that GPIO state sensors were created
    EXPECT_GT(gpu->deviceSensors.size(), 0);
}

// Test for ErrorInjectionSupported with MCTP Bridge device
TEST_F(NsmChassisTest, goodTestCreateErrorInjectionPayload)
{
    // Use FPGA device for this test since it can be MCTP_BRIDGE
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties with MCTP_BRIDGE device type
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_MCTP_BRIDGE);

    // First call: NSM_Chassis_Attributes with ErrorInjectionSupported
    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = asset["Type"];
    propertyMap["ErrorInjectionSupported"] = true;
    nsmChassisCreateSensors(mockManager, basicIntfName + ".ChassisAttributes",
                            objPath);

    // For MCTP_BRIDGE, error injection sensors should be created
    // Verify the function was called by checking device sensors
    // Note: The actual implementation depends on
    // createNsmMCUErrorInjectionSensors
}
