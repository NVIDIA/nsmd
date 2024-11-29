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

#include "pci-links.h"

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
}

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

    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    const uuid_t fpgaUuid = "992b3ec1-e464-f145-8686-409009062aa8";
    const uuid_t gpuDeviceUuid = "592b3ec1-e464-f145-8686-409009062aa8";

    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(gpuUuid)},
        {std::make_shared<NsmDevice>(fpgaUuid)},
    };
    NsmDevice& gpu = *devices[0];
    NsmDevice& fpga = *devices[1];

    NsmChassisPCIeDeviceTest() : SensorManagerTest(devices) {}

    const PropertyValuesCollection error = {
        {"Type", "NSM_ChassispCIeDevice"},
    };
    const PropertyValuesCollection basic = {
        {"ChassisName", chassisName},      {"Name", name},
        {"Type", "NSM_ChassisPCIeDevice"}, {"UUID", gpuUuid},
        {"DEVICE_UUID", gpuDeviceUuid},
    };
    const PropertyValuesCollection asset = {
        {"Type", "NSM_Asset"},
        {"Name", "HGX_GPU_SXM_1"},
        {"Manufacturer", "NVIDIA"},
    };
    const PropertyValuesCollection associations[2] = {
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
    const PropertyValuesCollection health = {
        {"Type", "NSM_Health"},
        {"Health", "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"},
    };
    const PropertyValuesCollection pcieDevice = {
        {"Type", "NSM_PCIeDevice"},
        {"DeviceType", "GPU"},
        {"Functions", std::vector<uint64_t>{0}},
    };
    const PropertyValuesCollection ltssmState = {
        {"Type", "NSM_LTSSMState"},
        {"DeviceIndex", uint64_t(1)},
        {"Priority", false},
        {"InventoryObjPath",
         "/xyz/openbmc_project/inventory/system/fabrics/HGX_PCIeRetimerTopology_0/Switches/PCIeRetimer_0/Ports/Down_0"},
    };
    const PropertyValuesCollection clockOutputEnableState = {
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
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(error, "Type"));
    values.push(objPath, get(basic, "UUID"));

    nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}
TEST_F(NsmChassisPCIeDeviceTest, goodTestCreateDeviceSensors)
{
    auto& map = utils::MockDbusAsync::getServiceMap();
    map = gpuServiceMap;

    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(basic, "Type"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(basic, "DEVICE_UUID"));
    values.push(objPath, get(associations[0], "Forward"));
    values.push(objPath, get(associations[0], "Backward"));
    values.push(objPath, get(associations[0], "AbsolutePath"));
    values.push(objPath, get(associations[1], "Forward"));
    values.push(objPath, get(associations[1], "Backward"));
    values.push(objPath, get(associations[1], "AbsolutePath"));
    nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName, objPath);
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(health, "Type"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(health, "Health"));
    nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName + ".Health",
                                      objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(3, gpu.roundRobinSensors.size());
    EXPECT_EQ(3, gpu.deviceSensors.size());

    auto sensor = 0;
    auto uuidObject = dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
        gpu.deviceSensors[sensor++]);
    auto associationsObject =
        dynamic_pointer_cast<NsmInterfaceProvider<AssociationDefinitionsIntf>>(
            gpu.deviceSensors[sensor++]);
    auto healthObject = dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
        gpu.deviceSensors[sensor++]);

    EXPECT_NE(nullptr, uuidObject);
    EXPECT_NE(nullptr, associationsObject);
    EXPECT_NE(nullptr, healthObject);

    EXPECT_EQ(gpuDeviceUuid, uuidObject->pdi().uuid());
    EXPECT_EQ(2, associationsObject->pdi().associations().size());
    EXPECT_EQ(HealthIntf::HealthType::OK, healthObject->pdi().health());
}

TEST_F(NsmChassisPCIeDeviceTest, goodTestCreateSensors)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(asset, "Type"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(asset, "Manufacturer"));
    nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName + ".Asset",
                                      objPath);
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(pcieDevice, "Type"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(pcieDevice, "DeviceType"));
    values.push(objPath, get(pcieDevice, "Functions"));
    nsmChassisPCIeDeviceCreateSensors(mockManager,
                                      basicIntfName + ".PCIeDevice", objPath);
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(ltssmState, "Type"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(ltssmState, "DeviceIndex"));
    values.push(objPath, get(ltssmState, "Priority"));
    values.push(objPath, get(ltssmState, "InventoryObjPath"));
    nsmChassisPCIeDeviceCreateSensors(mockManager,
                                      basicIntfName + ".LTSSMState", objPath);
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(clockOutputEnableState, "Type"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(clockOutputEnableState, "DeviceType"));
    values.push(objPath, get(clockOutputEnableState, "InstanceNumber"));
    nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".NSM_ClockOutputEnableState", objPath);

    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(9, gpu.roundRobinSensors.size());
    EXPECT_EQ(9, gpu.deviceSensors.size());

    auto sensors = 0;
    auto partNumber = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        gpu.deviceSensors[sensors++]);
    auto serialNumber =
        dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
            gpu.deviceSensors[sensors++]);
    auto model = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        gpu.deviceSensors[sensors++]);
    auto pcieDeviceObject =
        dynamic_pointer_cast<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
            gpu.deviceSensors[sensors++]);
    auto functionSensor =
        dynamic_pointer_cast<NsmPCIeFunction>(gpu.deviceSensors[sensors++]);
    sensors++;
    auto ltssmStateSensor =
        dynamic_pointer_cast<NsmPCIeLTSSMState>(gpu.deviceSensors[sensors++]);
    auto pcieRefClock =
        dynamic_pointer_cast<NsmClockOutputEnableState<PCIeRefClockIntf>>(
            gpu.deviceSensors[sensors++]);
    auto nvLinkRefClock =
        dynamic_pointer_cast<NsmClockOutputEnableState<NVLinkRefClockIntf>>(
            gpu.deviceSensors[sensors++]);
    EXPECT_EQ(sensors, gpu.deviceSensors.size());

    EXPECT_NE(nullptr, partNumber);
    EXPECT_NE(nullptr, serialNumber);
    EXPECT_NE(nullptr, model);
    EXPECT_NE(nullptr, pcieDeviceObject);
    EXPECT_NE(nullptr, functionSensor);

    EXPECT_NE(nullptr, ltssmStateSensor);
    EXPECT_NE(nullptr, pcieRefClock);
    EXPECT_NE(nullptr, nvLinkRefClock);

    EXPECT_EQ(DEVICE_PART_NUMBER, partNumber->property);
    EXPECT_EQ(SERIAL_NUMBER, serialNumber->property);
    EXPECT_EQ(MARKETING_NAME, model->property);
    EXPECT_EQ(get<std::string>(asset, "Manufacturer"),
              model->pdi().manufacturer());
    EXPECT_EQ(get<std::string>(pcieDevice, "DeviceType"),
              pcieDeviceObject->pdi().deviceType());
    EXPECT_EQ(get<uint64_t>(ltssmState, "DeviceIndex"),
              ltssmStateSensor->deviceIndex);
    EXPECT_EQ(PCIE_CLKBUF_INDEX, pcieRefClock->bufferIndex);
    EXPECT_EQ(NVHS_CLKBUF_INDEX, nvLinkRefClock->bufferIndex);

    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(sensors)
        .WillRepeatedly(mockSendRecvNsmMsg());
    for (auto i = 0; i < sensors; i++)
    {
        gpu.deviceSensors[i]->update(mockManager, eid).detach();
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
                                                           deviceIndex);

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
    nsm_query_scalar_group_telemetry_group_1 data{4, 1, 3, 5, 2};
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(PCIeDeviceIntf::PCIeTypes::Gen4, sensor->pdi().pcIeType());
    EXPECT_EQ(PCIeSlotIntf::Generations::Gen4, sensor->pdi().generationInUse());
    EXPECT_EQ(PCIeDeviceIntf::PCIeTypes::Gen5, sensor->pdi().maxPCIeType());
    EXPECT_EQ(1, sensor->pdi().lanesInUse());
    EXPECT_EQ(2, sensor->pdi().maxLanes());
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
    nsm_query_scalar_group_telemetry_group_1 data{4, 1, 3, 5, 2};
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    struct nsm_query_scalar_group_telemetry_v1_resp* resp =
        (struct nsm_query_scalar_group_telemetry_v1_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(PCIeDeviceIntf::PCIeTypes::Gen1, sensor->pdi().pcIeType());
    EXPECT_EQ(PCIeDeviceIntf::PCIeTypes::Gen1, sensor->pdi().maxPCIeType());
    EXPECT_EQ(PCIeSlotIntf::Generations::Gen1, sensor->pdi().generationInUse());
    EXPECT_EQ(0, sensor->pdi().lanesInUse());
    EXPECT_EQ(0, sensor->pdi().maxLanes());
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
    EXPECT_EQ("0x000A", sensor->pdi().function##X##VendorId());                \
    EXPECT_EQ("0x0003", sensor->pdi().function##X##DeviceId());                \
    EXPECT_EQ("0x0010", sensor->pdi().function##X##SubsystemVendorId());       \
    EXPECT_EQ("0xFB0C", sensor->pdi().function##X##SubsystemId());
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
    EXPECT_EQ("", sensor->pdi().function0VendorId());
    EXPECT_EQ("", sensor->pdi().function0DeviceId());
    EXPECT_EQ("", sensor->pdi().function0SubsystemVendorId());
    EXPECT_EQ("", sensor->pdi().function0SubsystemId());
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
        EXPECT_EQ(LTSSMStateIntf::State(state), sensor->pdi().ltssmState());
    }
    testResponse(0xFF);
    EXPECT_EQ(LTSSMStateIntf::State::IllegalState, sensor->pdi().ltssmState());
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
    EXPECT_EQ(LTSSMStateIntf::State::NA, sensor->pdi().ltssmState());
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
    EXPECT_EQ(LTSSMStateIntf::State::NA, sensor->pdi().ltssmState());
}
