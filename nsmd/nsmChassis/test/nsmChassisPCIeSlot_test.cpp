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

#include "nsmChassisPCIeSlot.hpp"
#include "nsmPCIeLinkSpeed.hpp"

namespace nsm
{
void nsmChassisPCIeSlotCreateSensors(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath);
}

using namespace nsm;

struct NsmChassisPCIeSlotTest : public testing::Test, public utils::DBusTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisPCIeSlot";
    const std::string chassisName = "HGX_Chassis_0";
    const std::string name = "PCIeSlot1";
    const std::string objPath = chassisInventoryBasePath / chassisName / name;

    const uuid_t baseboardUuid = "992b3ec1-e468-f145-8686-409009062aa8";

    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(baseboardUuid)},
    };
    NsmDevice& baseboard = *devices[0];

    NiceMock<MockSensorManager> mockManager{devices};

    const PropertyValuesCollection error = {
        {"UUID", "99sb3ec1-e468-f145-8686-409009062aa8"},
    };
    const PropertyValuesCollection basic = {
        {"ChassisName", chassisName},
        {"Name", name},
        {"Type", "NSM_ChassisPCIeSlot"},
        {"UUID", baseboardUuid},
        {"DeviceIndex", uint64_t(0)},
        {"SlotType",
         "xyz.openbmc_project.Inventory.Item.PCIeSlot.SlotTypes.OEM"},
        {"Priority", false},
    };
};

TEST_F(NsmChassisPCIeSlotTest, goodTestCreateSensors)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(basic, "DeviceIndex")))
        .WillOnce(Return(get(basic, "SlotType")))
        .WillOnce(Return(get(basic, "Priority")));
    nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, baseboard.prioritySensors.size());
    EXPECT_EQ(1, baseboard.roundRobinSensors.size());
    EXPECT_EQ(0, baseboard.deviceSensors.size());
    auto sensor = dynamic_pointer_cast<NsmPCIeLinkSpeed<PCIeSlotIntf>>(
        baseboard.roundRobinSensors[0]);
    EXPECT_NE(nullptr, sensor);
    EXPECT_EQ(PCIeSlotIntf::convertSlotTypesFromString(
                  get<std::string>(basic, "SlotType")),
              sensor->pdi().slotType());
}
TEST_F(NsmChassisPCIeSlotTest, badTestNoDeviceFound)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(error, "UUID")))
        .WillOnce(Return(get(basic, "DeviceIndex")))
        .WillOnce(Return(get(basic, "SlotType")))
        .WillOnce(Return(get(basic, "Priority")));
    EXPECT_THROW(
        nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath),
        std::runtime_error);
    EXPECT_EQ(0, baseboard.prioritySensors.size());
    EXPECT_EQ(0, baseboard.roundRobinSensors.size());
    EXPECT_EQ(0, baseboard.deviceSensors.size());
}

struct NsmPCIeSlotTest : public NsmChassisPCIeSlotTest
{
  protected:
    uint8_t deviceIndex = 1;
    NsmChassisPCIeSlot pcieDevice{chassisName, name};

  private:
    std::shared_ptr<NsmPCIeLinkSpeed<PCIeSlotIntf>> sensor =
        std::make_shared<NsmPCIeLinkSpeed<PCIeSlotIntf>>(pcieDevice,
                                                         deviceIndex);

  protected:
    void SetUp() override
    {
        EXPECT_EQ(pcieDevice.getName(), name);
        EXPECT_EQ(pcieDevice.getType(), "NSM_ChassisPCIeSlot");
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->deviceIndex, deviceIndex);
    }
};

TEST_F(NsmPCIeSlotTest, goodTestRequest)
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
TEST_F(NsmPCIeSlotTest, badTestRequest)
{
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPCIeSlotTest, goodTestResponse)
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
TEST_F(NsmPCIeSlotTest, badTestResponseSize)
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
TEST_F(NsmPCIeSlotTest, badTestCompletionErrorResponse)
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
