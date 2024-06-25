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

#include "nsmChassisPCIeDevice.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmPCIeFunction.hpp"
#include "nsmPCIeLTSSMState.hpp"
#include "nsmPCIeLinkSpeed.hpp"

namespace nsm
{
void nsmChassisPCIeDeviceCreateSensors(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
}

using namespace nsm;

struct NsmChassisPCIeDeviceTest : public testing::Test, public utils::DBusTest
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

    NiceMock<MockSensorManager> mockManager{devices};

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
    const PropertyValuesCollection association = {
        {"Forward", "chassis"},
        {"Backward", "pciedevice"},
        {"AbsolutePath",
         "/xyz/openbmc_project/inventory/system/chassis/" + chassisName},
    };
    const PropertyValuesCollection health = {
        {"Type", "NSM_Health"},
        {"Health", "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"},
    };
    const PropertyValuesCollection pcieDevice = {
        {"Type", "NSM_PCIeDevice"},
        {"DeviceType", "GPU"},
        {"DeviceIndex", uint64_t(1)},
        {"Priority", false},
        {"Functions", std::vector<uint64_t>{0, 1}},
    };
    const PropertyValuesCollection ltssmState = {
        {"Type", "NSM_LTSSMState"},
        {"DeviceIndex", uint64_t(1)},
        {"Priority", false},
    };
    const MapperServiceMap gpuServiceMap = {
        {
            {
                "xyz.openbmc_project.NSM",
                {
                    basicIntfName + ".Associations0",
                },
            },
        },
    };
};

TEST_F(NsmChassisPCIeDeviceTest, badTestCreateDeviceSensors)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(error, "Type")))
        .WillOnce(Return(get(basic, "UUID")));
    EXPECT_NO_THROW(
        nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName, objPath));
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}
TEST_F(NsmChassisPCIeDeviceTest, goodTestCreateDeviceSensors)
{
    EXPECT_CALL(mockDBus, getServiceMap).WillOnce(Return(gpuServiceMap));
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(basic, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(basic, "DEVICE_UUID")))
        .WillOnce(Return(get(association, "Forward")))
        .WillOnce(Return(get(association, "Backward")))
        .WillOnce(Return(get(association, "AbsolutePath")));
    nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(health, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(health, "Health")));
    nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName + ".Health",
                                      objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(3, gpu.deviceSensors.size());
    EXPECT_NE(
        nullptr,
        dynamic_pointer_cast<NsmInterfaceProvider<AssociationDefinitionsInft>>(
            gpu.deviceSensors[0]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
                           gpu.deviceSensors[1]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                           gpu.deviceSensors[2]));

    EXPECT_EQ(
        1,
        dynamic_pointer_cast<NsmInterfaceProvider<AssociationDefinitionsInft>>(
            gpu.deviceSensors[0])
            ->pdi()
            .associations()
            .size());
    EXPECT_EQ(gpuDeviceUuid,
              dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
                  gpu.deviceSensors[1])
                  ->pdi()
                  .uuid());
    EXPECT_EQ(HealthIntf::HealthType::OK,
              dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                  gpu.deviceSensors[2])
                  ->pdi()
                  .health());
}

TEST_F(NsmChassisPCIeDeviceTest, goodTestCreateSensors)
{
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(5)
        .WillRepeatedly(
            [](eid_t, Request&, std::shared_ptr<const nsm_msg>&,
               size_t&) -> requester::Coroutine { co_return NSM_SUCCESS; });
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(asset, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(asset, "Manufacturer")));
    nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName + ".Asset",
                                      objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(pcieDevice, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(pcieDevice, "DeviceType")))
        .WillOnce(Return(get(pcieDevice, "DeviceIndex")))
        .WillOnce(Return(get(pcieDevice, "Functions")))
        .WillOnce(Return(get(pcieDevice, "Priority")));
    nsmChassisPCIeDeviceCreateSensors(mockManager,
                                      basicIntfName + ".PCIeDevice", objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(ltssmState, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(ltssmState, "DeviceIndex")))
        .WillOnce(Return(get(ltssmState, "Priority")));
    nsmChassisPCIeDeviceCreateSensors(mockManager,
                                      basicIntfName + ".LTSSMState", objPath);
                                      

    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(2, gpu.roundRobinSensors.size());
    EXPECT_EQ(7, gpu.deviceSensors.size());

    auto sensors = 0;
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<AssetIntf>>(
                           gpu.deviceSensors[sensors++]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<AssetIntf>>(
                           gpu.deviceSensors[sensors++]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<AssetIntf>>(
                           gpu.deviceSensors[sensors]));
    EXPECT_EQ(get<std::string>(asset, "Manufacturer"),
              dynamic_pointer_cast<NsmInventoryProperty<AssetIntf>>(
                  gpu.deviceSensors[sensors++])
                  ->pdi()
                  .manufacturer());
                  
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
                           gpu.deviceSensors[sensors]));
    EXPECT_EQ(get<uint64_t>(pcieDevice, "DeviceIndex"),
              dynamic_pointer_cast<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
                  gpu.deviceSensors[sensors])
                  ->deviceIndex);
    EXPECT_EQ(get<std::string>(pcieDevice, "DeviceType"),
              dynamic_pointer_cast<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
                  gpu.deviceSensors[sensors++])
                  ->pdi()
                  .deviceType());

    for (int i = 0; i < 2; i++)
    {
        auto& sensor = gpu.deviceSensors[sensors++];
        auto functionSensor = dynamic_pointer_cast<NsmPCIeFunction>(sensor);
        EXPECT_NE(nullptr, functionSensor);
    }
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmPCIeLTSSMState>(
                           gpu.deviceSensors[sensors]));
    EXPECT_EQ(get<uint64_t>(ltssmState, "DeviceIndex"),
              dynamic_pointer_cast<NsmPCIeLTSSMState>(gpu.deviceSensors[sensors++])
                  ->deviceIndex);
}

struct NsmPCIeDeviceTest : public NsmChassisPCIeDeviceTest
{
  protected:
    uint8_t deviceIndex = 1;
    NsmChassisPCIeDevice<PCIeDeviceIntf> pcieDevice{chassisName, name};

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
    nsm_query_scalar_group_telemetry_group_1 data{3, 3, 3, 3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
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
    nsm_query_scalar_group_telemetry_group_1 data{3, 3, 3, 3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    struct nsm_query_scalar_group_telemetry_v1_resp* resp =
        (struct nsm_query_scalar_group_telemetry_v1_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
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
    nsm_query_scalar_group_telemetry_group_0 data{3, 3, 3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
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
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

struct NsmPCIeLTSSMStateTest : public NsmPCIeDeviceTest
{
    NsmChassisPCIeDevice<LTSSMStateIntf> ltssmDevice{chassisName, name};
    std::shared_ptr<NsmPCIeLTSSMState> sensor =
        std::make_shared<NsmPCIeLTSSMState>(ltssmDevice, deviceIndex);
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
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_6 data{3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
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
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
