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
#include "pci-links.h"

#include "nsmAERError.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmChassisPCIeDevice.hpp"
#include "nsmClockOutputEnableState.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmPCIeFunction.hpp"
#include "nsmPCIeLTSSMState.hpp"
#include "nsmPCIeLinkSpeed.hpp"
namespace nsm
{
requester::Coroutine
    nsmChassisPCIeDeviceCreateSensors(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NsmChassisPCIeDeviceTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice";
    const std::string chassisName = "HGX_GPU_SXM_1";
    const std::string name = "PCIeDevice1";
    const std::string objPath = chassisInventoryBasePath / chassisName / name;

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const uuid_t gpuDeviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmChassisPCIeDeviceTest() : SensorManagerTest(devices)
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

    dbus::PropertyMap error = {
        {"Type", "NSM_ChassispCIeDevice"},
    };
    dbus::PropertyMap basic = {
        {"ChassisName", chassisName},      {"Name", name},
        {"Type", "NSM_ChassisPCIeDevice"}, {"UUID", gpuUuid},
        {"DEVICE_UUID", gpuDeviceUuid},
    };
    dbus::PropertyMap chassisAttributes = {
        {"Type", "NSM_Chassis_Attributes"},
        {"Name", "HGX_GPU_SXM_1"},
    };
    dbus::PropertyMap associations[2] = {
        {
            {"Forward", "chassis"},
            {"Backward", "pciedevice"},
            {"AbsolutePath",
             "/xyz/openbmc_project/inventory/system/chassis/" + chassisName},
        },
        {
            {"Forward", "connected_port"},
            {"Backward", "connected_pciedevice"},
            {"AbsolutePath",
             "/xyz/openbmc_project/inventory/system/fabrics/HGX_PCIeRetimerTopology_0/Switches/PCIeRetimer_0/Ports/Down_0"},
        },
    };
    dbus::PropertyMap pcieDevice = {
        {"Type", "NSM_PCIeDevice"},
    };
    dbus::PropertyMap ltssmState = {
        {"Type", "NSM_LTSSMState"},
        {"DeviceIndex", uint64_t(1)},
        {"InventoryObjPath",
         "/xyz/openbmc_project/inventory/system/fabrics/HGX_PCIeRetimerTopology_0/Switches/PCIeRetimer_0/Ports/Down_0"},
    };
    dbus::PropertyMap clockOutputEnableState = {
        {"Type", "NSM_ClockOutputEnableState"},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"InstanceNumber", uint64_t(instanceId)},
    };
    const MapperServiceMap gpuServiceMap = {
        {
            {
                "xyz.openbmc_project.NSM",
                {
                    basicIntfName + ".Associations0",
                    basicIntfName + ".Associations1",
                },
            },
        },
    };
};

TEST_F(NsmChassisPCIeDeviceTest, badTestCreateDeviceSensors)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];

    // Set up interface-specific properties with invalid type
    propertyMap["Type"] = error["Type"];

    nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(2, devices.size());
    EXPECT_EQ(1, gpu->deviceSensors.size());     // Only msgTypes sensor
    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(1, gpu->roundRobinSensors.size()); // Only msgTypes sensor
}
TEST_F(NsmChassisPCIeDeviceTest, goodTestCreateDeviceSensors)
{
    auto& map = utils::MockDbusAsync::serviceMap();
    map = gpuServiceMap;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];

    // First call: NSM_ChassisPCIeDevice
    propertyMap["Type"] = basic["Type"];
    propertyMap["DEVICE_UUID"] = basic["DEVICE_UUID"];

    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".Associations0");
    propertyMapAssociation0 = associations[0];
    auto& propertyMapAssociation1 = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".Associations1");
    propertyMapAssociation1 = associations[1];

    nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName, objPath);

    EXPECT_EQ(2, devices.size());
    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(2, gpu->staticSensors.size());
    EXPECT_EQ(1, gpu->roundRobinSensors.size()); // msgTypes sensor
    EXPECT_EQ(3, gpu->deviceSensors.size());     // msgTypes + 2 others

    auto sensor = 1; // Skip msgTypes sensor at index 0
    auto uuidObject = dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
        gpu->deviceSensors[sensor++]);
    auto associationsObject =
        dynamic_pointer_cast<NsmInterfaceProvider<AssociationDefinitionsIntf>>(
            gpu->deviceSensors[sensor++]);

    EXPECT_NE(nullptr, uuidObject);
    EXPECT_NE(nullptr, associationsObject);

    EXPECT_EQ(gpuDeviceUuid, uuidObject->invoke(pdiMethod(uuid)));
    EXPECT_EQ(2, associationsObject->invoke(pdiMethod(associations)).size());
}

TEST_F(NsmChassisPCIeDeviceTest, goodTestCreateSensors)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["ChassisName"] = basic["ChassisName"];
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];

    // First call: NSM_Chassis_Attributes
    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = chassisAttributes["Type"];
    nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", objPath);

    // Second call: NSM_PCIeDevice
    auto& propertyMapPCIe = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".PCIeDevice");
    propertyMapPCIe["Type"] = pcieDevice["Type"];
    nsmChassisPCIeDeviceCreateSensors(mockManager,
                                      basicIntfName + ".PCIeDevice", objPath);

    // Third call: NSM_LTSSMState
    auto& propertyMapLTSSM = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".LTSSMState");
    propertyMapLTSSM["Type"] = ltssmState["Type"];
    propertyMapLTSSM["DeviceIndex"] = ltssmState["DeviceIndex"];
    propertyMapLTSSM["InventoryObjPath"] = ltssmState["InventoryObjPath"];
    nsmChassisPCIeDeviceCreateSensors(mockManager,
                                      basicIntfName + ".LTSSMState", objPath);

    // Fourth call: NSM_ClockOutputEnableState
    auto& propertyMapClock = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".NSM_ClockOutputEnableState");
    propertyMapClock["Type"] = clockOutputEnableState["Type"];
    propertyMapClock["DeviceType"] = clockOutputEnableState["DeviceType"];
    propertyMapClock["InstanceNumber"] =
        clockOutputEnableState["InstanceNumber"];
    nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".NSM_ClockOutputEnableState", objPath);

    EXPECT_EQ(2, devices.size());
    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(5, gpu->staticSensors.size());
    EXPECT_EQ(6, gpu->roundRobinSensors.size()); // msgTypes + 5 others
    EXPECT_EQ(11, gpu->deviceSensors.size());    // msgTypes + 10 others

    auto sensors = 1; // Skip msgTypes sensor at index 0
    // Asset sensors (indices 1-3: partNumber, serialNumber, model)
    auto partNumber = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        gpu->deviceSensors[sensors++]);
    auto serialNumber =
        dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
            gpu->deviceSensors[sensors++]);
    auto model = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        gpu->deviceSensors[sensors++]);

    // Health sensor (index 4)
    auto healthObject = dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
        gpu->deviceSensors[sensors++]);

    // PCIeDevice sensor (index 5)
    auto pcieDeviceObject =
        dynamic_pointer_cast<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
            gpu->deviceSensors[sensors++]);

    // Function sensor (index 6)
    auto functionSensor =
        dynamic_pointer_cast<NsmPCIeFunction>(gpu->deviceSensors[sensors++]);
    sensors++;

    // LTSSMState sensor (index 8)
    auto ltssmStateSensor =
        dynamic_pointer_cast<NsmPCIeLTSSMState>(gpu->deviceSensors[sensors++]);

    // PCIeRefClock sensor (index 9)
    auto pcieRefClock =
        dynamic_pointer_cast<NsmClockOutputEnableState<PCIeRefClockIntf>>(
            gpu->deviceSensors[sensors++]);

    // NVLinkRefClock sensor (index 10)
    auto nvLinkRefClock =
        dynamic_pointer_cast<NsmClockOutputEnableState<NVLinkRefClockIntf>>(
            gpu->deviceSensors[sensors++]);
    EXPECT_EQ(sensors, gpu->deviceSensors.size());

    EXPECT_NE(nullptr, partNumber);
    EXPECT_NE(nullptr, serialNumber);
    EXPECT_NE(nullptr, model);
    EXPECT_NE(nullptr, healthObject);
    EXPECT_NE(nullptr, pcieDeviceObject);
    EXPECT_NE(nullptr, functionSensor);

    EXPECT_NE(nullptr, ltssmStateSensor);
    EXPECT_NE(nullptr, pcieRefClock);
    EXPECT_NE(nullptr, nvLinkRefClock);

    EXPECT_EQ(DEVICE_PART_NUMBER, partNumber->property);
    EXPECT_EQ(SERIAL_NUMBER, serialNumber->property);
    EXPECT_EQ(MARKETING_NAME, model->property);
    EXPECT_EQ(MANUFACTURER_NVIDIA, model->invoke(pdiMethod(manufacturer)));
    EXPECT_EQ(HealthIntf::HealthType::OK,
              healthObject->invoke(pdiMethod(health)));
    EXPECT_EQ(PCIE_DEVICE_TYPE_SINGLE_FUNCTION,
              PCIeDeviceIntf::convertDeviceTypesToString(
                  pcieDeviceObject->invoke(pdiMethod(deviceType))));
    EXPECT_EQ(std::get<uint64_t>(ltssmState["DeviceIndex"]),
              ltssmStateSensor->deviceIndex);
    EXPECT_EQ(PCIE_CLKBUF_INDEX, pcieRefClock->bufferIndex);
    EXPECT_EQ(NVHS_CLKBUF_INDEX, nvLinkRefClock->bufferIndex);
    testing::Mock::AllowLeak(gpu.get());
    EXPECT_CALL(*gpu, sensorIO).Times(sensors).WillRepeatedly(mockSensorIO());
    for (auto i = 0; i < sensors; i++)
    {
        gpu->deviceSensors[i]->update(gpu).detach();
    }
}

struct NsmPCIeDeviceTest : public NsmChassisPCIeDeviceTest
{
  protected:
    uint8_t deviceIndex = 1;
    NsmChassisPCIeDevice<PCIeDeviceIntf> pcieDevice{"HGX_GPU_SXM_9", name};

  private:
    std::shared_ptr<NsmPCIeLinkSpeed<PCIeDeviceIntf>> sensor =
        std::make_shared<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(pcieDevice,
                                                           deviceIndex, false);

  protected:
    void SetUp() override
    {
        EXPECT_EQ(pcieDevice.getName(), name);
        EXPECT_EQ(pcieDevice.getType(), "NSM_ChassisPCIeDevice");
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->deviceIndex, deviceIndex);
    }
};

TEST_F(NsmPCIeDeviceTest, goodTestRequest)
{
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    uint8_t groupIndex = 0;
    uint8_t deviceIndex = 0;
    auto rc = decode_query_scalar_group_telemetry_v1_req(
        requestPtr, request.value().size(), &deviceIndex, &groupIndex);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(1, groupIndex);
    EXPECT_EQ(deviceIndex, deviceIndex);
}
TEST_F(NsmPCIeDeviceTest, badTestRequest)
{
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPCIeDeviceTest, goodTestResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_1 data{4, 1, 3, 5, 2, 0, 0, 0};
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(PCIeDeviceIntf::PCIeTypes::Gen4,
              sensor->invoke(pdiMethod(pcIeType)));
    EXPECT_EQ(PCIeSlotIntf::Generations::Gen4,
              sensor->invoke(pdiMethod(generationInUse)));
    EXPECT_EQ(PCIeDeviceIntf::PCIeTypes::Gen5,
              sensor->invoke(pdiMethod(maxPCIeType)));
    EXPECT_EQ(1, sensor->invoke(pdiMethod(lanesInUse)));
    EXPECT_EQ(2, sensor->invoke(pdiMethod(maxLanes)));
}
TEST_F(NsmPCIeDeviceTest, badTestResponseSize)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, nullptr, responseMsg);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmPCIeDeviceTest, badTestCompletionErrorResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_1 data{4, 1, 3, 5, 2, 0, 0, 0};
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    struct nsm_query_scalar_group_telemetry_v1_resp* resp =
        (struct nsm_query_scalar_group_telemetry_v1_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(PCIeDeviceIntf::PCIeTypes::Gen1,
              sensor->invoke(pdiMethod(pcIeType)));
    EXPECT_EQ(PCIeDeviceIntf::PCIeTypes::Gen1,
              sensor->invoke(pdiMethod(maxPCIeType)));
    EXPECT_EQ(PCIeSlotIntf::Generations::Gen1,
              sensor->invoke(pdiMethod(generationInUse)));
    EXPECT_EQ(0, sensor->invoke(pdiMethod(lanesInUse)));
    EXPECT_EQ(0, sensor->invoke(pdiMethod(maxLanes)));
}

struct NsmPCIeFunctionTest : public NsmPCIeDeviceTest
{
    std::shared_ptr<NsmPCIeFunction> sensor;
    void init(uint8_t functionId)
    {
        sensor = std::make_shared<NsmPCIeFunction>(pcieDevice, deviceIndex,
                                                   functionId);
        EXPECT_EQ(functionId, sensor->functionId);
    }
};

TEST_F(NsmPCIeFunctionTest, goodTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    uint8_t groupIndex = 0;
    uint8_t deviceIndex = 0;
    auto rc = decode_query_scalar_group_telemetry_v1_req(
        requestPtr, request.value().size(), &deviceIndex, &groupIndex);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(0, groupIndex);
    EXPECT_EQ(deviceIndex, deviceIndex);
}
TEST_F(NsmPCIeFunctionTest, badTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPCIeFunctionTest, goodTestResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_0 data{10, 3, 0x10, 0xFB0C};
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
#define EXPECT_EQ_PcieFunction(X)                                              \
    sensor->invoke([](auto& pdi) {                                             \
        EXPECT_EQ("0x000A", pdi.function##X##VendorId());                      \
        EXPECT_EQ("0x0003", pdi.function##X##DeviceId());                      \
        EXPECT_EQ("0x0010", pdi.function##X##SubsystemVendorId());             \
        EXPECT_EQ("0xFB0C", pdi.function##X##SubsystemId());                   \
    });
        switch (i)
        {
            case 0:
                EXPECT_EQ_PcieFunction(0);
                break;
            case 1:
                EXPECT_EQ_PcieFunction(1);
                break;
            case 2:
                EXPECT_EQ_PcieFunction(2);
                break;
            case 3:
                EXPECT_EQ_PcieFunction(3);
                break;
            case 4:
                EXPECT_EQ_PcieFunction(4);
                break;
            case 5:
                EXPECT_EQ_PcieFunction(5);
                break;
            case 6:
                EXPECT_EQ_PcieFunction(6);
                break;
            case 7:
                EXPECT_EQ_PcieFunction(7);
                break;
        }
    }
}
TEST_F(NsmPCIeFunctionTest, badTestResponseSize)
{
    init(0);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, nullptr, responseMsg);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmPCIeFunctionTest, badTestCompletionErrorResponse)
{
    init(0);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_0 data{3, 3, 3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    struct nsm_query_scalar_group_telemetry_v1_resp* resp =
        (struct nsm_query_scalar_group_telemetry_v1_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ("", sensor->invoke(pdiMethod(function0VendorId)));
    EXPECT_EQ("", sensor->invoke(pdiMethod(function0DeviceId)));
    EXPECT_EQ("", sensor->invoke(pdiMethod(function0SubsystemVendorId)));
    EXPECT_EQ("", sensor->invoke(pdiMethod(function0SubsystemId)));
}
TEST(NsmPCIeLinkSpeedTest, testGenerationTypeConvertion)
{
    using GenType = PCIeSlotIntf::Generations;
    EXPECT_EQ(GenType::Unknown, NsmPCIeLinkSpeedBase::generation(0));
    EXPECT_EQ(GenType::Gen1, NsmPCIeLinkSpeedBase::generation(1));
    EXPECT_EQ(GenType::Gen2, NsmPCIeLinkSpeedBase::generation(2));
    EXPECT_EQ(GenType::Gen3, NsmPCIeLinkSpeedBase::generation(3));
    EXPECT_EQ(GenType::Gen4, NsmPCIeLinkSpeedBase::generation(4));
    EXPECT_EQ(GenType::Gen5, NsmPCIeLinkSpeedBase::generation(5));
    EXPECT_EQ(GenType::Gen6, NsmPCIeLinkSpeedBase::generation(6));
    EXPECT_EQ(GenType::Unknown, NsmPCIeLinkSpeedBase::generation(7));
}
TEST(NsmPCIeLinkSpeedTest, testPcieTypeConvertion)
{
    using PCIeType = PCIeDeviceIntf::PCIeTypes;
    EXPECT_EQ(PCIeType::Unknown, NsmPCIeLinkSpeedBase::pcieType(0));
    EXPECT_EQ(PCIeType::Gen1, NsmPCIeLinkSpeedBase::pcieType(1));
    EXPECT_EQ(PCIeType::Gen2, NsmPCIeLinkSpeedBase::pcieType(2));
    EXPECT_EQ(PCIeType::Gen3, NsmPCIeLinkSpeedBase::pcieType(3));
    EXPECT_EQ(PCIeType::Gen4, NsmPCIeLinkSpeedBase::pcieType(4));
    EXPECT_EQ(PCIeType::Gen5, NsmPCIeLinkSpeedBase::pcieType(5));
    EXPECT_EQ(PCIeType::Gen6, NsmPCIeLinkSpeedBase::pcieType(6));
    EXPECT_EQ(PCIeType::Unknown, NsmPCIeLinkSpeedBase::pcieType(7));
}

struct NsmPCIeLTSSMStateTest : public NsmPCIeDeviceTest
{
    NsmChassisPCIeDevice<LTSSMStateIntf> ltssmDevice{chassisName, name};
    std::shared_ptr<NsmPCIeLTSSMState> sensor =
        std::make_shared<NsmPCIeLTSSMState>(ltssmDevice, deviceIndex);
    void testResponse(uint32_t ltssmState)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        nsm_query_scalar_group_telemetry_group_6 data{ltssmState, 0};
        auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmPCIeLTSSMStateTest, goodTestRequest)
{
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    uint8_t groupIndex = 0;
    uint8_t deviceIndex = 0;
    auto rc = decode_query_scalar_group_telemetry_v1_req(
        requestPtr, request.value().size(), &deviceIndex, &groupIndex);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(6, groupIndex);
    EXPECT_EQ(deviceIndex, deviceIndex);
}
TEST_F(NsmPCIeLTSSMStateTest, badTestRequest)
{
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPCIeLTSSMStateTest, goodTestResponse)
{
    for (uint32_t state = 0x0; state < 0x12; state++)
    {
        testResponse(state);
        EXPECT_EQ(LTSSMStateIntf::State(state),
                  sensor->invoke(pdiMethod(ltssmState)));
    }
    testResponse(0xFF);
    EXPECT_EQ(LTSSMStateIntf::State::IllegalState,
              sensor->invoke(pdiMethod(ltssmState)));
}
TEST_F(NsmPCIeLTSSMStateTest, badTestResponseSize)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, nullptr, responseMsg);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
    EXPECT_EQ(LTSSMStateIntf::State::NA, sensor->invoke(pdiMethod(ltssmState)));
}
TEST_F(NsmPCIeLTSSMStateTest, badTestCompletionErrorResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_6 data{3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    struct nsm_query_scalar_group_telemetry_v1_resp* resp =
        (struct nsm_query_scalar_group_telemetry_v1_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(LTSSMStateIntf::State::NA, sensor->invoke(pdiMethod(ltssmState)));
}

#ifdef ENABLE_PCIE_AER_ERROR

struct NsmAERErrorTest : public NsmPCIeDeviceTest
{
    std::string aerObjPath;
    std::shared_ptr<NsmAERErrorStatusIntf> aerIntf;
    std::shared_ptr<NsmPCIeAERErrorStatus> sensor;

    void SetUp() override
    {
        // Use unique path for each test to avoid D-Bus conflicts
        static int testCounter = 0;
        aerObjPath = std::string(chassisInventoryBasePath / chassisName /
                                 "PCIeDevices" / name) +
                     "_aer_test_" + std::to_string(testCounter++);
        aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
            utils::DBusHandler::getBus(), aerObjPath.c_str(), 0, gpu);
        sensor = std::make_shared<NsmPCIeAERErrorStatus>(
            name, "PCIeAerErrorStatus", aerIntf, 0);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), "PCIeAerErrorStatus");
        EXPECT_EQ(sensor->deviceIndex, 0);
    }

    void TearDown() override
    {
        sensor.reset();
        aerIntf.reset();
    }

    void testRequest()
    {
        auto request = sensor->genRequestMsg(eid, instanceId);
        EXPECT_TRUE(request.has_value());
        EXPECT_EQ(request.value().size(),
                  sizeof(nsm_msg_hdr) +
                      sizeof(nsm_query_scalar_group_telemetry_v1_req));
        auto requestPtr =
            reinterpret_cast<struct nsm_msg*>(request.value().data());
        uint8_t deviceId = 0;
        uint8_t groupId = 0;
        auto rc = decode_query_scalar_group_telemetry_v1_req(
            requestPtr, request.value().size(), &deviceId, &groupId);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        EXPECT_EQ(deviceId, 0);
        EXPECT_EQ(groupId, GROUP_ID_9);
    }

    void testResponse(uint32_t uncorrectable, uint32_t correctable)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        nsm_query_scalar_group_telemetry_group_9 data{uncorrectable,
                                                      correctable};
        auto rc = encode_query_scalar_group_telemetry_v1_group9_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmAERErrorTest, goodTestConstructor)
{
    EXPECT_EQ(sensor->deviceIndex, 0);
}

TEST_F(NsmAERErrorTest, goodTestRequest)
{
    testRequest();
}

TEST_F(NsmAERErrorTest, goodTestResponse)
{
    uint32_t uncorrectable = 0x12345678;
    uint32_t correctable = 0x9ABCDEF0;
    testResponse(uncorrectable, correctable);

    EXPECT_STREQ(aerIntf->aerUncorrectableErrorStatus().c_str(), "0x12345678");
    EXPECT_STREQ(aerIntf->aerCorrectableErrorStatus().c_str(), "0x9ABCDEF0");
}

TEST_F(NsmAERErrorTest, goodTestResponseZeroValues)
{
    testResponse(0, 0);
    EXPECT_STREQ(aerIntf->aerUncorrectableErrorStatus().c_str(), "0x00000000");
    EXPECT_STREQ(aerIntf->aerCorrectableErrorStatus().c_str(), "0x00000000");
}

TEST_F(NsmAERErrorTest, goodTestResponseMaxValues)
{
    testResponse(0xFFFFFFFF, 0xFFFFFFFF);
    EXPECT_STREQ(aerIntf->aerUncorrectableErrorStatus().c_str(), "0xFFFFFFFF");
    EXPECT_STREQ(aerIntf->aerCorrectableErrorStatus().c_str(), "0xFFFFFFFF");
}

TEST_F(NsmAERErrorTest, badTestResponseSize)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_group_9) -
        1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmAERErrorTest, badTestCompletionErrorResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_9 data{0, 0};
    auto rc = encode_query_scalar_group_telemetry_v1_group9_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = reinterpret_cast<
        struct nsm_query_scalar_group_telemetry_v1_group_9_resp*>(
        responseMsg->payload);
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmAERErrorTest, goodTestLinkAerStatusSensor)
{
    aerIntf->linkAerStatusSensor(sensor);
    EXPECT_EQ(aerIntf->aerStatusSensor, sensor);
}

#endif // ENABLE_PCIE_AER_ERROR

struct NsmPCIeLTSSMStateStandaloneTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string name = "PCIeDevice1";
    const std::string type = "NSM_PCIeLTSSMState";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/chassis/"
                                "HGX_GPU_SXM_1/PCIeDevices/PCIeDevice1";

    NsmDeviceTable devices;
    std::shared_ptr<NsmPCIeLTSSMState> sensor;

    NsmPCIeLTSSMStateStandaloneTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        uint8_t deviceIndex = 0;
        NsmInterfaceProvider<LTSSMStateIntf> provider(name, type, objPath);
        sensor = std::make_shared<NsmPCIeLTSSMState>(provider, deviceIndex);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), type);
        EXPECT_EQ(sensor->deviceIndex, deviceIndex);
    }

    void testRequest()
    {
        auto request = sensor->genRequestMsg(eid, instanceId);
        EXPECT_TRUE(request.has_value());
        EXPECT_EQ(request.value().size(),
                  sizeof(nsm_msg_hdr) +
                      sizeof(nsm_query_scalar_group_telemetry_v1_req));
        auto requestPtr =
            reinterpret_cast<struct nsm_msg*>(request.value().data());
        uint8_t deviceIndex;
        uint8_t groupId;
        auto rc = decode_query_scalar_group_telemetry_v1_req(
            requestPtr, request.value().size(), &deviceIndex, &groupId);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        EXPECT_EQ(groupId, GROUP_ID_6);
    }

    void testResponse(uint8_t ltssmState)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        nsm_query_scalar_group_telemetry_group_6 data = {};
        data.ltssm_state = ltssmState;

        auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestRequest)
{
    testRequest();
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseDetect)
{
    testResponse(0x0);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::Detect);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponsePolling)
{
    testResponse(0x1);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::Polling);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseConfiguration)
{
    testResponse(0x2);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::Configuration);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseRecovery)
{
    testResponse(0x3);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::Recovery);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseRecoveryEQ)
{
    testResponse(0x4);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::RecoveryEQ);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseL0)
{
    testResponse(0x5);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)), LTSSMStateIntf::State::L0);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseL0s)
{
    testResponse(0x6);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::L0s);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseL1)
{
    testResponse(0x7);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)), LTSSMStateIntf::State::L1);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseL1_PLL_PD)
{
    testResponse(0x8);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::L1_PLL_PD);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseL2)
{
    testResponse(0x9);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)), LTSSMStateIntf::State::L2);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseL1_CPM)
{
    testResponse(0xA);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::L1_CPM);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseL1_1)
{
    testResponse(0xB);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::L1_1);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseL1_2)
{
    testResponse(0xC);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::L1_2);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseHotReset)
{
    testResponse(0xD);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::HotReset);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseLoopback)
{
    testResponse(0xE);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::Loopback);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseDisabled)
{
    testResponse(0xF);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::Disabled);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseLinkDown)
{
    testResponse(0x10);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::LinkDown);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseLinkReady)
{
    testResponse(0x11);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::LinkReady);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseLanesInSleep)
{
    testResponse(0x12);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::LanesInSleep);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestResponseIllegalState)
{
    testResponse(0xFF);
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)),
              LTSSMStateIntf::State::IllegalState);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, badTestResponseSize)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, badTestCompletionErrorResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_6 data = {};
    data.ltssm_state = 0x5; // L0

    auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = reinterpret_cast<
        struct nsm_query_scalar_group_telemetry_v1_group_6_resp*>(
        responseMsg->payload);
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
    // When error, state should be NA
    EXPECT_EQ(sensor->invoke(pdiMethod(ltssmState)), LTSSMStateIntf::State::NA);
}

TEST_F(NsmPCIeLTSSMStateStandaloneTest, goodTestMultipleDeviceIndices)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        // Use unique path for each sensor to avoid D-Bus conflicts
        std::filesystem::path uniquePath = objPath + "_dev" + std::to_string(i);
        NsmInterfaceProvider<LTSSMStateIntf> provider(name, type, uniquePath);
        auto testSensor = std::make_shared<NsmPCIeLTSSMState>(provider, i);
        EXPECT_EQ(testSensor->deviceIndex, i);
    }
}

struct NsmClockOutputEnableStateStandaloneTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string name = "GPU_SXM_1";
    const std::string type = "NSM_ClockOutputEnableState";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/chassis/"
                                "HGX_GPU_SXM_1";

    NsmDeviceTable devices;

    NsmClockOutputEnableStateStandaloneTest() : SensorManagerTest(devices) {}

    void testRequest(std::shared_ptr<NsmClockOutputEnableStateBase> sensor,
                     uint8_t expectedIndex)
    {
        auto request = sensor->genRequestMsg(eid, instanceId);
        EXPECT_TRUE(request.has_value());
        EXPECT_EQ(request.value().size(),
                  sizeof(nsm_msg_hdr) +
                      sizeof(nsm_get_clock_output_enabled_state_req));
        auto requestPtr =
            reinterpret_cast<struct nsm_msg*>(request.value().data());
        uint8_t bufferIndex;
        auto rc = decode_get_clock_output_enable_state_req(
            requestPtr, request.value().size(), &bufferIndex);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        EXPECT_EQ(bufferIndex, expectedIndex);
    }

    template <typename DataType>
    void testResponse(std::shared_ptr<NsmClockOutputEnableStateBase> sensor,
                      const DataType& data)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_clock_output_enabled_state_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        uint32_t rawData;
        memcpy(&rawData, &data, sizeof(uint32_t));

        auto rc = encode_get_clock_output_enable_state_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, rawData, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmClockOutputEnableStateStandaloneTest, goodTestPCIeRefClockGpuRequest)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        NsmInterfaceProvider<PCIeRefClockIntf> provider(name, type, objPath);
        auto sensor =
            std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
                provider, PCIE_CLKBUF_INDEX, NSM_DEV_ID_GPU, i, false);
        EXPECT_NE(sensor, nullptr);
        testRequest(sensor, PCIE_CLKBUF_INDEX);
    }
}

TEST_F(NsmClockOutputEnableStateStandaloneTest, goodTestPCIeRefClockGpuResponse)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        NsmInterfaceProvider<PCIeRefClockIntf> provider(name, type, objPath);
        auto sensor =
            std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
                provider, PCIE_CLKBUF_INDEX, NSM_DEV_ID_GPU, i, false);

        nsm_pcie_clock_buffer_data data = {};
        // Set the corresponding GPU clock buffer bit
        switch (i)
        {
            case 0:
                data.clk_buf_gpu1 = 1;
                break;
            case 1:
                data.clk_buf_gpu2 = 1;
                break;
            case 2:
                data.clk_buf_gpu3 = 1;
                break;
            case 3:
                data.clk_buf_gpu4 = 1;
                break;
            case 4:
                data.clk_buf_gpu5 = 1;
                break;
            case 5:
                data.clk_buf_gpu6 = 1;
                break;
            case 6:
                data.clk_buf_gpu7 = 1;
                break;
            case 7:
                data.clk_buf_gpu8 = 1;
                break;
        }

        testResponse(sensor, data);
        EXPECT_TRUE(sensor->invoke(pdiMethod(pcIeReferenceClockEnabled)));
    }
}

TEST_F(NsmClockOutputEnableStateStandaloneTest,
       goodTestPCIeRefClockSwitchResponse)
{
    for (uint8_t i = 0; i < 2; i++)
    {
        NsmInterfaceProvider<PCIeRefClockIntf> provider(name, type, objPath);
        auto sensor =
            std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
                provider, PCIE_CLKBUF_INDEX, NSM_DEV_ID_SWITCH, i, false);

        nsm_pcie_clock_buffer_data data = {};
        if (i == 0)
        {
            data.clk_buf_nvSwitch_1 = 1;
        }
        else
        {
            data.clk_buf_nvSwitch_2 = 1;
        }

        testResponse(sensor, data);
        EXPECT_TRUE(sensor->invoke(pdiMethod(pcIeReferenceClockEnabled)));
    }
}

TEST_F(NsmClockOutputEnableStateStandaloneTest,
       goodTestPCIeRefClockRetimerResponse)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        NsmInterfaceProvider<PCIeRefClockIntf> provider(name, type, objPath);
        auto sensor =
            std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
                provider, PCIE_CLKBUF_INDEX, NSM_DEV_ID_BASEBOARD, i, true);

        nsm_pcie_clock_buffer_data data = {};
        switch (i)
        {
            case 0:
                data.clk_buf_retimer1 = 1;
                break;
            case 1:
                data.clk_buf_retimer2 = 1;
                break;
            case 2:
                data.clk_buf_retimer3 = 1;
                break;
            case 3:
                data.clk_buf_retimer4 = 1;
                break;
            case 4:
                data.clk_buf_retimer5 = 1;
                break;
            case 5:
                data.clk_buf_retimer6 = 1;
                break;
            case 6:
                data.clk_buf_retimer7 = 1;
                break;
            case 7:
                data.clk_buf_retimer8 = 1;
                break;
        }

        testResponse(sensor, data);
        EXPECT_TRUE(sensor->invoke(pdiMethod(pcIeReferenceClockEnabled)));
    }
}

TEST_F(NsmClockOutputEnableStateStandaloneTest,
       goodTestNVLinkRefClockGpuResponse)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        NsmInterfaceProvider<NVLinkRefClockIntf> provider(name, type, objPath);
        auto sensor =
            std::make_shared<NsmClockOutputEnableState<NVLinkRefClockIntf>>(
                provider, NVHS_CLKBUF_INDEX, NSM_DEV_ID_GPU, i, false);

        nsm_nvhs_clock_buffer_data data = {};
        switch (i)
        {
            case 0:
                data.clk_buf_sxm_nvhs1 = 1;
                break;
            case 1:
                data.clk_buf_sxm_nvhs2 = 1;
                break;
            case 2:
                data.clk_buf_sxm_nvhs3 = 1;
                break;
            case 3:
                data.clk_buf_sxm_nvhs4 = 1;
                break;
            case 4:
                data.clk_buf_sxm_nvhs5 = 1;
                break;
            case 5:
                data.clk_buf_sxm_nvhs6 = 1;
                break;
            case 6:
                data.clk_buf_sxm_nvhs7 = 1;
                break;
            case 7:
                data.clk_buf_sxm_nvhs8 = 1;
                break;
        }

        testResponse(sensor, data);
        EXPECT_TRUE(sensor->invoke(pdiMethod(nvLinkReferenceClockEnabled)));
    }
}

TEST_F(NsmClockOutputEnableStateStandaloneTest,
       goodTestNVLinkRefClockSwitchResponse)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        NsmInterfaceProvider<NVLinkRefClockIntf> provider(name, type, objPath);
        auto sensor =
            std::make_shared<NsmClockOutputEnableState<NVLinkRefClockIntf>>(
                provider, NVHS_CLKBUF_INDEX, NSM_DEV_ID_SWITCH, i, false);

        nsm_nvhs_clock_buffer_data data = {};
        switch (i)
        {
            case 0:
                data.clk_buf_nvSwitch_nvhs1 = 1;
                break;
            case 1:
                data.clk_buf_nvSwitch_nvhs2 = 1;
                break;
            case 2:
                data.clk_buf_nvSwitch_nvhs3 = 1;
                break;
            case 3:
                data.clk_buf_nvSwitch_nvhs4 = 1;
                break;
        }

        testResponse(sensor, data);
        EXPECT_TRUE(sensor->invoke(pdiMethod(nvLinkReferenceClockEnabled)));
    }
}

TEST_F(NsmClockOutputEnableStateStandaloneTest, badTestResponseSize)
{
    NsmInterfaceProvider<PCIeRefClockIntf> provider(name, type, objPath);
    auto sensor = std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
        provider, PCIE_CLKBUF_INDEX, NSM_DEV_ID_GPU, 0, false);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_resp) -
        1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmClockOutputEnableStateStandaloneTest, badTestCompletionErrorResponse)
{
    NsmInterfaceProvider<PCIeRefClockIntf> provider(name, type, objPath);
    auto sensor = std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
        provider, PCIE_CLKBUF_INDEX, NSM_DEV_ID_GPU, 0, false);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint32_t data = 0;

    auto rc = encode_get_clock_output_enable_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto resp =
        reinterpret_cast<struct nsm_get_clock_output_enabled_state_resp*>(
            responseMsg->payload);
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmClockOutputEnableStateStandaloneTest, goodTestDisabledClockResponse)
{
    NsmInterfaceProvider<PCIeRefClockIntf> provider(name, type, objPath);
    auto sensor = std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
        provider, PCIE_CLKBUF_INDEX, NSM_DEV_ID_GPU, 0, false);

    nsm_pcie_clock_buffer_data data = {};
    data.clk_buf_gpu1 = 0; // Disabled

    testResponse(sensor, data);
    EXPECT_FALSE(sensor->invoke(pdiMethod(pcIeReferenceClockEnabled)));
}
