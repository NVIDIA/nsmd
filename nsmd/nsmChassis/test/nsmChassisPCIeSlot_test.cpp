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
requester::Coroutine
    nsmChassisPCIeSlotCreateSensors(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NsmChassisPCIeSlotTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisPCIeSlot";
    const std::string chassisName = "HGX_Chassis_0";
    const std::string name = "PCIeSlot1";
    const std::string objPath = chassisInventoryBasePath / chassisName / name;

    const uuid_t baseboardUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:255";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> baseboard;

    NsmChassisPCIeSlotTest() : SensorManagerTest(devices)
    {
        baseboard = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(baseboardUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(baseboard, nullptr);
        EXPECT_EQ(NSM_DEV_ID_BASEBOARD, baseboard->getDeviceType());
    }

    dbus::PropertyMap error = {
        {"UUID", "99sb3ec1-e468-f145-8686-409009062aa8"},
    };
    dbus::PropertyMap basic = {
        {"ChassisName", chassisName},
        {"Name", name},
        {"Type", "NSM_ChassisPCIeSlot"},
        {"UUID", baseboardUuid},
        {"DeviceIndex", uint64_t(0)},
        {"SlotType",
         "xyz.openbmc_project.Inventory.Item.PCIeSlot.SlotTypes.OEM"},
        {"Priority", false},
    };
    const MapperServiceMap serviceMap;
};

TEST_F(NsmChassisPCIeSlotTest, goodTestCreateSensors)
{
    auto& map = utils::MockDbusAsync::serviceMap();
    map = serviceMap;
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];

    // Set up interface-specific properties
    propertyMap["Type"] = basic["Type"];
    propertyMap["DeviceIndex"] = basic["DeviceIndex"];
    propertyMap["SlotType"] = basic["SlotType"];
    propertyMap["Priority"] = basic["Priority"];
    nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath);

    EXPECT_EQ(1, devices.size());
    baseboard = dynamic_pointer_cast<MockNsmDevice>(devices[0]);
    EXPECT_EQ(0, baseboard->prioritySensors.size());
    EXPECT_EQ(1, baseboard->staticSensors.size());
    EXPECT_EQ(2, baseboard->roundRobinSensors.size());
    EXPECT_EQ(3, baseboard->deviceSensors.size());

    auto sensors = 1; // Skip msgTypes sensor added by initMsgTypesSensor()
    auto sensor = dynamic_pointer_cast<NsmPCIeLinkSpeed<PCIeSlotIntf>>(
        baseboard->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, sensor);
    EXPECT_EQ(PCIeSlotIntf::convertSlotTypesFromString(
                  std::get<std::string>(basic["SlotType"])),
              sensor->invoke(pdiMethod(slotType)));
    auto associations =
        dynamic_pointer_cast<NsmChassisPCIeSlot<AssociationDefinitionsIntf>>(
            baseboard->deviceSensors[sensors++]);
    EXPECT_NE(nullptr, associations);
    EXPECT_EQ(0, associations->invoke(pdiMethod(associations)).size());
}
TEST_F(NsmChassisPCIeSlotTest, badTestNoDeviceFound)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties with INVALID UUID that doesn't match any
    // device
    const uuid_t invalidUuid =
        "99sb3ec1-e468-f145-8686-409009062aa8"; // From error collection
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = invalidUuid; // Invalid UUID as uuid_t type

    // Set up interface-specific properties
    propertyMap["Type"] = basic["Type"];
    propertyMap["DeviceIndex"] = basic["DeviceIndex"];
    propertyMap["SlotType"] = basic["SlotType"];
    propertyMap["Priority"] = basic["Priority"];

    EXPECT_THROW_COROUTINE(
        nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

struct NsmPCIeSlotTest : public NsmChassisPCIeSlotTest
{
  protected:
    uint8_t deviceIndex = 1;
    NsmChassisPCIeSlot<PCIeSlotIntf> pcieDevice{chassisName, name};

  private:
    std::shared_ptr<NsmPCIeLinkSpeed<PCIeSlotIntf>> sensor =
        std::make_shared<NsmPCIeLinkSpeed<PCIeSlotIntf>>(pcieDevice,
                                                         deviceIndex, false);

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
    nsm_query_scalar_group_telemetry_group_1 data{3, 3, 3, 3, 3, 0, 0, 0};
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
    nsm_query_scalar_group_telemetry_group_1 data{3, 3, 3, 3, 3, 0, 0, 0};
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    struct nsm_query_scalar_group_telemetry_v1_resp* resp =
        (struct nsm_query_scalar_group_telemetry_v1_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// Branch coverage: factory property presence/absence branches
// =============================================================================

// ChassisName absent → FALSE branch → chassisName="" → sensor still created
TEST_F(NsmChassisPCIeSlotTest, Factory_MissingChassisName_SensorCreated)
{
    auto& map = utils::MockDbusAsync::serviceMap();
    map = serviceMap;
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    // Intentionally omit "ChassisName" → FALSE branch for count("ChassisName")
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];
    propertyMap["DeviceIndex"] = basic["DeviceIndex"];
    propertyMap["SlotType"] = basic["SlotType"];
    propertyMap["Priority"] = basic["Priority"];

    const size_t before = baseboard->staticSensors.size();
    nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath);
    // AssociationDefinitionsIntf static sensor added regardless of chassisName
    EXPECT_GT(baseboard->staticSensors.size(), before);
}

// Name absent → FALSE branch → name="" → D-Bus path invalid → throws
TEST_F(NsmChassisPCIeSlotTest, Factory_MissingName_Throws)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    // Intentionally omit "Name" → FALSE branch for count("Name") → name=""
    // NsmChassisPCIeSlot constructor registers path ending with "/" → invalid
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["UUID"] = basic["UUID"];
    propertyMap["DeviceIndex"] = basic["DeviceIndex"];
    propertyMap["SlotType"] = basic["SlotType"];
    propertyMap["Priority"] = basic["Priority"];

    EXPECT_THROW_COROUTINE(
        nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath),
        std::exception);
}

// UUID absent → FALSE branch → uuid="" → getNsmDeviceFromStaticUUID throws
TEST_F(NsmChassisPCIeSlotTest, Factory_MissingUUID_Throws)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    // Intentionally omit "UUID" → FALSE branch for count("UUID") → uuid=""
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["DeviceIndex"] = basic["DeviceIndex"];
    propertyMap["SlotType"] = basic["SlotType"];
    propertyMap["Priority"] = basic["Priority"];

    EXPECT_THROW_COROUTINE(
        nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

// DeviceIndex absent → FALSE branch → deviceIndex=0 → sensor still created
TEST_F(NsmChassisPCIeSlotTest, Factory_MissingDeviceIndex_SensorCreated)
{
    auto& map = utils::MockDbusAsync::serviceMap();
    map = serviceMap;
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    // Intentionally omit "DeviceIndex" → FALSE branch for count("DeviceIndex")
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];
    propertyMap["SlotType"] = basic["SlotType"];
    propertyMap["Priority"] = basic["Priority"];

    const size_t before = baseboard->staticSensors.size();
    nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_GT(baseboard->staticSensors.size(), before);
}

// SlotType absent → FALSE branch → slotType="" → convertSlotTypesFromString
// throws
TEST_F(NsmChassisPCIeSlotTest, Factory_MissingSlotType_Throws)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    // Intentionally omit "SlotType" → FALSE branch for count("SlotType")
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];
    propertyMap["DeviceIndex"] = basic["DeviceIndex"];
    propertyMap["Priority"] = basic["Priority"];

    EXPECT_THROW_COROUTINE(
        nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath),
        std::exception);
}

// Priority absent → FALSE branch → priority=false (default) → roundRobinSensors
TEST_F(NsmChassisPCIeSlotTest, Factory_MissingPriority_RoundRobinSensor)
{
    auto& map = utils::MockDbusAsync::serviceMap();
    map = serviceMap;
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    // Intentionally omit "Priority" → FALSE branch for count("Priority")
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];
    propertyMap["DeviceIndex"] = basic["DeviceIndex"];
    propertyMap["SlotType"] = basic["SlotType"];

    const size_t roundRobinBefore = baseboard->roundRobinSensors.size();
    nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath);
    // priority defaults to false → sensor goes to roundRobinSensors
    EXPECT_GT(baseboard->roundRobinSensors.size(), roundRobinBefore);
    EXPECT_EQ(0u, baseboard->prioritySensors.size());
}

// Priority=true → TRUE branch → addSensor(sensor, true) → prioritySensors //

TEST_F(NsmChassisPCIeSlotTest, Factory_PriorityTrue_AddsToPrioritySensors)
{
    auto& map = utils::MockDbusAsync::serviceMap();
    map = serviceMap;
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];
    propertyMap["DeviceIndex"] = basic["DeviceIndex"];
    propertyMap["SlotType"] = basic["SlotType"];
    propertyMap["Priority"] = true; // TRUE → addSensor(sensor, true)

    const size_t priorityBefore = baseboard->prioritySensors.size();
    const size_t roundRobinBefore = baseboard->roundRobinSensors.size();
    nsmChassisPCIeSlotCreateSensors(mockManager, basicIntfName, objPath);
    // priority=true → sensor added to prioritySensors, not roundRobinSensors
    EXPECT_GT(baseboard->prioritySensors.size(), priorityBefore);
    EXPECT_EQ(roundRobinBefore, baseboard->roundRobinSensors.size());
}
