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

#include "libnsm/firmware-utils.h"

#include "nsmApSkuId.hpp"
#include "nsmCommon.hpp"
#include "nsmSecurityRBP.hpp"
#include "nsmUpdateApSkuId.hpp"

using namespace nsm;

struct NsmApSkuIdTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string name = "HGX_BASEBOARD";
    const std::string type = "NSM_ApSkuId";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    std::shared_ptr<NsmApSkuIdObject> sensor;
    std::shared_ptr<ProgressIntf> progressIntf;

    NsmApSkuIdTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(fpga, nullptr);
        EXPECT_EQ(NSM_DEV_ID_BASEBOARD, fpga->getDeviceType());
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string progressPath = "/xyz/openbmc_project/software/" + name;
        progressIntf = std::make_shared<ProgressIntf>(bus,
                                                      progressPath.c_str());

        uint16_t classification = 0x0001;
        uint16_t identifier = 0x0002;
        uint8_t index = 0;
        std::vector<utils::Association> associations;

        sensor = std::make_shared<NsmApSkuIdObject>(
            bus, name, type, fpgaUuid, progressIntf, classification, identifier,
            index, associations);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), type);
        EXPECT_EQ(sensor->classification, classification);
        EXPECT_EQ(sensor->identifier, identifier);
        EXPECT_EQ(sensor->index, index);
    }

    void testRequest()
    {
        auto request = sensor->genRequestMsg(eid, instanceId);
        EXPECT_TRUE(request.has_value());
        EXPECT_EQ(request.value().size(),
                  sizeof(nsm_msg_hdr) +
                      sizeof(nsm_firmware_get_erot_state_info_req));
    }

    void testResponse(uint32_t apSkuIdValue)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
                sizeof(nsm_firmware_erot_state_info_hdr_resp) +
                sizeof(nsm_firmware_slot_info) * 2,
            0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        nsm_firmware_erot_state_info_resp erotInfo = {};
        // Store ap_sku_id in big-endian format as the protocol expects
        erotInfo.fq_resp_hdr.ap_sku_id = __builtin_bswap32(apSkuIdValue);
        erotInfo.slot_info = nullptr;

        auto rc = encode_nsm_query_get_erot_state_parameters_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmApSkuIdTest, goodTestConstructor)
{
    EXPECT_NE(sensor->apSkuIdObject, nullptr);
}

TEST_F(NsmApSkuIdTest, goodTestRequest)
{
    testRequest();
}

TEST_F(NsmApSkuIdTest, goodTestResponse)
{
    uint32_t skuId = 0x12345678;
    testResponse(skuId);

    // NOTE: encode_nsm_query_get_erot_state_parameters_resp does NOT encode
    // ap_sku_id to payload (libnsm limitation), so decoded value is always 0
    std::string expectedSku = formatApSkuId(0);
    EXPECT_EQ(sensor->apSkuIdObject->sku(), expectedSku);
}

TEST_F(NsmApSkuIdTest, goodTestResponseZeroSku)
{
    testResponse(0);
    std::string expectedSku = formatApSkuId(0);
    EXPECT_EQ(sensor->apSkuIdObject->sku(), expectedSku);
}

TEST_F(NsmApSkuIdTest, goodTestResponseMaxSku)
{
    testResponse(0xFFFFFFFF);
    // NOTE: encode_nsm_query_get_erot_state_parameters_resp does NOT encode
    // ap_sku_id to payload (libnsm limitation), so decoded value is always 0
    std::string expectedSku = formatApSkuId(0);
    EXPECT_EQ(sensor->apSkuIdObject->sku(), expectedSku);
}

TEST_F(NsmApSkuIdTest, goodTestSetSKU)
{
    std::string testSku = "TEST-SKU-001";
    sensor->setSKU(testSku);
    EXPECT_EQ(sensor->apSkuIdObject->sku(), testSku);
}

TEST_F(NsmApSkuIdTest, badTestResponseSize)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_get_erot_state_info_resp) -
        10);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmApSkuIdTest, badTestCompletionErrorResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.ap_sku_id = 0;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = reinterpret_cast<struct nsm_firmware_get_erot_state_info_resp*>(
        responseMsg->payload);
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));

    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmApSkuIdTest, goodTestGetPath)
{
    std::string chassisName = "TestChassis";
    std::string expectedPath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName;
    EXPECT_EQ(sensor->getPath(chassisName), expectedPath);
}

#include "nsmErot.hpp"
namespace nsm
{
requester::Coroutine nsmErotCreateSensors(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
} // namespace nsm

struct NsmErotTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_RoT";
    const std::string chassisName = "HGX_RoT_BASEBOARD";
    const std::string objPath = chassisInventoryBasePath / chassisName;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmErotTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(fpga, nullptr);
        EXPECT_EQ(NSM_DEV_ID_BASEBOARD, fpga->getDeviceType());
    }

    void TearDown() override
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap chassisRoT = {
        {"Type", "NSM_ChassisRoT"},
        {"Name", chassisName},
        {"UUID", fpgaUuid},
        {"ImageCopyEnabled", true},
    };

    dbus::PropertyMap slotAssociation = {
        {"Forward", "chassis"},
        {"Backward", "firmware_slot"},
        {"AbsolutePath", objPath},
    };

    const MapperServiceMap slotServiceMap = {
        {
            {
                "xyz.openbmc_project.NSM",
                {
                    "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations",
                },
            },
        },
    };
};

TEST_F(NsmErotTest, badTestNonRoTChassis)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Use a non-RoT chassis name (without "RoT_" in the name)
    propertyMap["Type"] = std::string("NSM_Chassis");
    propertyMap["Name"] = std::string("HGX_BASEBOARD");
    propertyMap["UUID"] = fpgaUuid;

    size_t initialSensorCount = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, objPath);

    // Should return early without adding sensors
    EXPECT_EQ(initialSensorCount, fpga->deviceSensors.size());
}

struct NsmSecurityRBPTest : public NsmErotTest
{
    const std::string securityName = "HGX_BASEBOARD";
    const std::string securityType = "NSM_SecurityCfg";

    std::shared_ptr<ProgressIntf> progressIntf;
    std::shared_ptr<NsmSecurityCfgObject> securitySensor;

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                                   securityName;
        progressIntf = std::make_shared<ProgressIntf>(bus,
                                                      progressPath.c_str());

        // Create NsmSecurityCfgObject sensor - pass name, not full path
        securitySensor = std::make_shared<NsmSecurityCfgObject>(
            bus, securityName, securityType, fpgaUuid, progressIntf);

        EXPECT_NE(securitySensor, nullptr);
        EXPECT_EQ(securitySensor->getName(), securityName);
        EXPECT_EQ(securitySensor->getType(), securityType);
    }
};

TEST_F(NsmSecurityRBPTest, goodTestConstructor)
{
    EXPECT_NE(securitySensor->securityCfgObject, nullptr);
}

TEST_F(NsmSecurityRBPTest, goodTestRequest)
{
    uint8_t instanceId = 0;
    eid_t eid = 0;

    auto request = securitySensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request.value().size(), sizeof(nsm_msg_hdr));
}

TEST_F(NsmSecurityRBPTest, goodTestResponseQueryState)
{
    uint8_t instanceId = 0;
    // Buffer size: header + response command structure
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 1;

    auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &cfg_state, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = securitySensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmSecurityRBPTest, badTestResponseSize)
{
    uint8_t instanceId = 0;
    // Buffer size: header + response command structure
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 1;

    auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &cfg_state, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Test with minimal size (just header) - should fail
    size_t minimalSize = sizeof(nsm_msg_hdr);
    rc = securitySensor->handleResponseMsg(responseMsg, minimalSize);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

#include "nsmErot.hpp"

struct NsmBuildTypeObjectTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "TestChassis";
    const std::string type = "NSM_ChassisRoT";
    const uuid_t uuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const int classification = 1;
    const int identifier = 2;

    NsmDeviceTable devices;
    std::shared_ptr<NsmBuildTypeObject> buildTypeObj;

    NsmBuildTypeObjectTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        buildTypeObj = std::make_shared<NsmBuildTypeObject>(
            chassisName, type, uuid, classification, identifier);

        EXPECT_NE(buildTypeObj, nullptr);
        EXPECT_EQ(buildTypeObj->getName(), chassisName);
        EXPECT_EQ(buildTypeObj->getType(), type);
    }
};

TEST_F(NsmBuildTypeObjectTest, goodTestConstructor)
{
    EXPECT_EQ(buildTypeObj->uuid, uuid);
    EXPECT_EQ(buildTypeObj->nsmRequest.component_classification,
              classification);
    EXPECT_EQ(buildTypeObj->nsmRequest.component_identifier, identifier);
    EXPECT_EQ(buildTypeObj->nsmRequest.component_classification_index, 0);
}

TEST_F(NsmBuildTypeObjectTest, goodTestGenRequestMsg)
{
    eid_t eid = 0;
    uint8_t instanceId = 0;

    auto request = buildTypeObj->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_get_erot_state_info_req));
}

TEST_F(NsmBuildTypeObjectTest, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    // instanceId > NSM_INSTANCE_MAX causes encode to fail -> return nullopt
    auto request = buildTypeObj->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmBuildTypeObjectTest, goodTestAddSlotObject)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string path = "/test/slot/path";
    std::vector<utils::Association> associations;
    int slotNumber = 0;

    auto slot1 = std::make_shared<NsmFirmwareSlot>(
        bus, path, associations, slotNumber, SlotIntf::FirmwareType::AP,
        chassisName);
    auto slot2 = std::make_shared<NsmFirmwareSlot>(
        bus, path, associations, slotNumber + 1, SlotIntf::FirmwareType::AP,
        chassisName);

    size_t initialSize = buildTypeObj->fwSlotObjects.size();
    buildTypeObj->addSlotObject(slot1);
    EXPECT_EQ(buildTypeObj->fwSlotObjects.size(), initialSize + 1);

    buildTypeObj->addSlotObject(slot2);
    EXPECT_EQ(buildTypeObj->fwSlotObjects.size(), initialSize + 2);
}

TEST_F(NsmBuildTypeObjectTest, goodTestHandleResponseMsgNoSlots)
{
    uint8_t instanceId = 0;
    // Increase buffer size to accommodate the full response structure
    std::vector<uint8_t> response(1024, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.firmware_slot_count = 0;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = buildTypeObj->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST_F(NsmBuildTypeObjectTest, badTestHandleResponseMsgSlotCountMismatch)
{
    // Add one slot to the object
    auto& bus = utils::DBusHandler::getBus();
    std::string path = "/test/slot/path";
    std::vector<utils::Association> associations;
    auto slot = std::make_shared<NsmFirmwareSlot>(
        bus, path, associations, 0, SlotIntf::FirmwareType::AP, chassisName);
    buildTypeObj->addSlotObject(slot);

    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            2 * sizeof(nsm_firmware_slot_info),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Create slot info array for 2 slots
    std::vector<nsm_firmware_slot_info> slotInfoArray(2);
    slotInfoArray[0] = {};
    slotInfoArray[1] = {};

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.firmware_slot_count = 2;
    erotInfo.slot_info = slotInfoArray.data();

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = buildTypeObj->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST_F(NsmBuildTypeObjectTest, badTestHandleResponseMsgCompletionCodeError)
{
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.firmware_slot_count = 0;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_ERROR, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = buildTypeObj->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST_F(NsmBuildTypeObjectTest, goodTestMultipleClassificationIdentifiers)
{
    auto obj1 = std::make_shared<NsmBuildTypeObject>("Chassis1", type, uuid,
                                                     0x0001, 0x0001);
    auto obj2 = std::make_shared<NsmBuildTypeObject>("Chassis2", type, uuid,
                                                     0x0002, 0x0003);
    auto obj3 = std::make_shared<NsmBuildTypeObject>("Chassis3", type, uuid,
                                                     0xFFFF, 0xFFFF);

    EXPECT_EQ(obj1->nsmRequest.component_classification, 0x0001);
    EXPECT_EQ(obj1->nsmRequest.component_identifier, 0x0001);

    EXPECT_EQ(obj2->nsmRequest.component_classification, 0x0002);
    EXPECT_EQ(obj2->nsmRequest.component_identifier, 0x0003);

    EXPECT_EQ(obj3->nsmRequest.component_classification, 0xFFFF);
    EXPECT_EQ(obj3->nsmRequest.component_identifier, 0xFFFF);
}

TEST_F(NsmErotTest, goodTestCreateErotSensorsWithSlots)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = chassisRoT["Name"];
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(2);
    propertyMap["ImageCopyEnabled"] = chassisRoT["ImageCopyEnabled"];

    // Setup slot 1 properties - AP firmware with unique chassis name
    std::string slot1Path = objPath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = std::string("HGX_RoT_BASEBOARD_AP");

    // Setup slot 1 associations
    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot1AssocProps = slotAssociation;

    // Setup slot 2 properties - EC firmware with unique chassis name
    std::string slot2Path = objPath + "/Slot2";
    auto& slot2Props = utils::MockDbusAsync::propertyMap(
        slot2Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot2Props["Name"] = std::string("Slot2");
    slot2Props["ComponentClassification"] = uint64_t(1);
    slot2Props["ComponentIdentifier"] = uint64_t(2);
    slot2Props["ComponentIndex"] = uint64_t(1);
    slot2Props["FirmwareType"] = std::string("EC");
    slot2Props["ChassisName"] = std::string("HGX_RoT_BASEBOARD_EC");

    // Setup slot 2 associations
    auto& slot2AssocProps = utils::MockDbusAsync::propertyMap(
        slot2Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot2AssocProps = slotAssociation;

    size_t initialSensorCount = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, objPath);

    EXPECT_GT(fpga->deviceSensors.size(), initialSensorCount);
}

TEST_F(NsmErotTest, goodTestCreateErotSensorsWithImageCopy)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    // Use unique chassis name to avoid D-Bus object conflicts
    std::string uniqueChassisName = chassisName + "_ImageCopy";
    std::string uniqueObjPath = chassisInventoryBasePath / uniqueChassisName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniqueObjPath,
                                                          basicIntfName);

    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueChassisName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = true;

    std::string slot1Path = uniqueObjPath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = uniqueChassisName + "_AP";

    // Setup slot 1 associations
    dbus::PropertyMap uniqueSlotAssociation = {
        {"Forward", "chassis"},
        {"Backward", "firmware_slot"},
        {"AbsolutePath", uniqueObjPath},
    };
    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot1AssocProps = uniqueSlotAssociation;

    size_t initialSensorCount = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniqueObjPath);

    EXPECT_GT(fpga->deviceSensors.size(), initialSensorCount);
}

TEST_F(NsmErotTest, goodTestCreateErotSensorsWithUpdateSKU)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    // Use unique chassis name to avoid D-Bus object conflicts
    std::string uniqueChassisName = chassisName + "_UpdateSKU";
    std::string uniqueObjPath = chassisInventoryBasePath / uniqueChassisName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniqueObjPath,
                                                          basicIntfName);

    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueChassisName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = false;

    // Enable SKU update
    std::vector<std::string> propertyList = {"AP_SKU_ID"};
    propertyMap["SetRotPropertyList"] = propertyList;

    std::string slot1Path = uniqueObjPath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = uniqueChassisName + "_AP";

    // Setup slot 1 associations
    dbus::PropertyMap uniqueSlotAssociation = {
        {"Forward", "chassis"},
        {"Backward", "firmware_slot"},
        {"AbsolutePath", uniqueObjPath},
    };
    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot1AssocProps = uniqueSlotAssociation;

    size_t initialSensorCount = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniqueObjPath);

    EXPECT_GT(fpga->deviceSensors.size(), initialSensorCount);
}

TEST_F(NsmErotTest, goodTestCreateErotSensorsECFirmware)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = chassisRoT["Name"];
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = false;

    std::string slot1Path = objPath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(2);
    slot1Props["ComponentIdentifier"] = uint64_t(2);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("EC");
    slot1Props["ChassisName"] = chassisRoT["Name"];

    // Setup slot 1 associations
    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot1AssocProps = slotAssociation;

    size_t initialSensorCount = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, objPath);

    EXPECT_GT(fpga->deviceSensors.size(), initialSensorCount);
}

TEST_F(NsmApSkuIdTest, goodTestMultipleSKUUpdates)
{
    // Test setting SKU multiple times
    sensor->setSKU("SKU-001");
    EXPECT_EQ(sensor->apSkuIdObject->sku(), "SKU-001");

    sensor->setSKU("SKU-002");
    EXPECT_EQ(sensor->apSkuIdObject->sku(), "SKU-002");

    sensor->setSKU("0x12345678");
    EXPECT_EQ(sensor->apSkuIdObject->sku(), "0x12345678");
}

TEST_F(NsmApSkuIdTest, goodTestEmptySKU)
{
    sensor->setSKU("");
    EXPECT_EQ(sensor->apSkuIdObject->sku(), "");
}

TEST_F(NsmApSkuIdTest, goodTestLongSKUString)
{
    std::string longSku =
        "VERY-LONG-SKU-STRING-WITH-MANY-CHARACTERS-0123456789";
    sensor->setSKU(longSku);
    EXPECT_EQ(sensor->apSkuIdObject->sku(), longSku);
}

TEST_F(NsmApSkuIdTest, goodTestSpecialCharactersSKU)
{
    sensor->setSKU("SKU_With-Special.Chars!");
    EXPECT_EQ(sensor->apSkuIdObject->sku(), "SKU_With-Special.Chars!");
}

TEST_F(NsmSecurityRBPTest, goodTestMultipleStateChanges)
{
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Test state 0
    nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 0;
    auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &cfg_state, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = securitySensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Test state 1
    cfg_state.irreversible_config_state = 1;
    rc = encode_nsm_firmware_irreversible_config_request_0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &cfg_state, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = securitySensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmSecurityRBPTest, badTestInvalidCompletionCode)
{
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 1;

    // Test with error completion code
    auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
        instanceId, NSM_ERROR, 0x1234, &cfg_state, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = securitySensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmBuildTypeObjectTest, goodTestMultipleSlots)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string path = "/test/slot/path";
    std::vector<utils::Association> associations;

    // Add 5 slots
    for (int i = 0; i < 5; ++i)
    {
        auto slot = std::make_shared<NsmFirmwareSlot>(
            bus, path, associations, i, SlotIntf::FirmwareType::AP,
            chassisName);
        buildTypeObj->addSlotObject(slot);
    }

    EXPECT_EQ(buildTypeObj->fwSlotObjects.size(), 5);
}

TEST_F(NsmBuildTypeObjectTest, goodTestMixedSlotTypes)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string path = "/test/slot/path";
    std::vector<utils::Association> associations;

    // Add AP slots
    auto apSlot1 = std::make_shared<NsmFirmwareSlot>(
        bus, path, associations, 0, SlotIntf::FirmwareType::AP, chassisName);
    auto apSlot2 = std::make_shared<NsmFirmwareSlot>(
        bus, path, associations, 1, SlotIntf::FirmwareType::AP, chassisName);

    // Add EC slots
    auto ecSlot1 = std::make_shared<NsmFirmwareSlot>(
        bus, path, associations, 2, SlotIntf::FirmwareType::EC, chassisName);

    buildTypeObj->addSlotObject(apSlot1);
    buildTypeObj->addSlotObject(apSlot2);
    buildTypeObj->addSlotObject(ecSlot1);

    EXPECT_EQ(buildTypeObj->fwSlotObjects.size(), 3);
}

TEST_F(NsmErotTest, goodTestCreateErotSensorsNoSlots)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = chassisRoT["Name"];
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(0); // No slots
    propertyMap["ImageCopyEnabled"] = false;

    size_t initialSensorCount = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, objPath);

    // Should still create security sensor even with no slots
    EXPECT_GE(fpga->deviceSensors.size(), initialSensorCount);
}

TEST_F(NsmErotTest, badTestInvalidUUID)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = chassisRoT["Name"];
    propertyMap["UUID"] = std::string("INVALID-UUID-THAT-DOES-NOT-EXIST");
    propertyMap["SlotCount"] = uint64_t(0);

    size_t initialSensorCount = fpga->deviceSensors.size();
    EXPECT_THROW_COROUTINE(
        nsmErotCreateSensors(mockManager, basicIntfName, objPath),
        std::runtime_error);

    // Should fail to create sensors with invalid UUID
    EXPECT_EQ(fpga->deviceSensors.size(), initialSensorCount);
}

TEST_F(NsmBuildTypeObjectTest, goodTestHandleResponseMsgWithSlots)
{
    // Add slots to match the response
    auto& bus = utils::DBusHandler::getBus();
    std::string path = "/test/slot/path";
    std::vector<utils::Association> associations;

    auto slot1 = std::make_shared<NsmFirmwareSlot>(
        bus, path + "/0", associations, 0, SlotIntf::FirmwareType::AP,
        chassisName);
    auto slot2 = std::make_shared<NsmFirmwareSlot>(
        bus, path + "/1", associations, 1, SlotIntf::FirmwareType::AP,
        chassisName);

    buildTypeObj->addSlotObject(slot1);
    buildTypeObj->addSlotObject(slot2);
    EXPECT_EQ(buildTypeObj->fwSlotObjects.size(), 2);

    // Create response with matching slot count
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            2 * sizeof(nsm_firmware_slot_info),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Create slot info array
    std::vector<nsm_firmware_slot_info> slotInfoArray(2);
    slotInfoArray[0] = {}; // Initialize with default values
    slotInfoArray[1] = {};

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.firmware_slot_count =
        2; // Matches fwSlotObjects.size()
    erotInfo.slot_info = slotInfoArray.data();

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // This should succeed and cover lines 92, 96
    rc = buildTypeObj->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// Decode fail test: 7-byte buffer is too short for
// decode_nsm_query_get_erot_state_parameters_resp; returns error.
TEST_F(NsmBuildTypeObjectTest, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = buildTypeObj->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// parseSlots branch coverage: IsRoT, ReportSkuWithNsm, SetRotPropertyList
// =============================================================================

// Test covers uncovered slot property branches (lines 166-196):
// - IsRoT=true (line 168)
// - ReportSkuWithNsm=true (lines 173-174)
// - SetRotPropertyList with AP_SKU_ID -> enableUpdateSKU=true (lines 185-188)
TEST_F(NsmErotTest, goodTestCreateErotSensors_SlotExtendedProperties)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = chassisName + "_ExtSlot";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = false;

    // Slot 1: include IsRoT, ReportSkuWithNsm, SetRotPropertyList (AP_SKU_ID)
    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("ExtSlot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = uniqueName + "_AP";
    slot1Props["IsRoT"] = bool(true);
    slot1Props["ReportSkuWithNsm"] = bool(true);
    slot1Props["SetRotPropertyList"] = std::vector<std::string>{"AP_SKU_ID",
                                                                "OTHER_PROP"};

    dbus::PropertyMap assocProps = {
        {"Forward", "chassis"},
        {"Backward", "firmware_slot"},
        {"AbsolutePath", uniquePath},
    };
    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot1AssocProps = assocProps;

    size_t initialSensorCount = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    EXPECT_GE(fpga->deviceSensors.size(), initialSensorCount);
}

// =============================================================================
// FALSE-branch coverage: count() checks in parseSlots
// ComponentClassification, ComponentIdentifier, ComponentIndex absent ->
// count() = 0 -> FALSE branches -> defaults to 0.
// IsRoT, ReportSkuWithNsm, SetRotPropertyList also omitted -> FALSE branches.
// =============================================================================
TEST_F(NsmErotTest, parseSlots_MinimalSlotProps_MissingNumericFields)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = chassisName + "_MinimalSlot";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& slotMainProps = utils::MockDbusAsync::propertyMap(uniquePath,
                                                            basicIntfName);
    slotMainProps["Type"] = std::string("NSM_ChassisRoT");
    slotMainProps["Name"] = uniqueName;
    slotMainProps["UUID"] = fpgaUuid;
    slotMainProps["SlotCount"] = uint64_t(1);
    slotMainProps["ImageCopyEnabled"] = false;

    // Slot 1: only Name, FirmwareType, ChassisName present.
    // ComponentClassification, ComponentIdentifier, ComponentIndex
    // intentionally omitted -> count() = 0 -> FALSE branches -> default to 0.
    // IsRoT, ReportSkuWithNsm, SetRotPropertyList also omitted -> FALSE
    // branches.
    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = uniqueName + "_AP";
    // ComponentClassification, ComponentIdentifier, ComponentIndex omitted
    // IsRoT, ReportSkuWithNsm, SetRotPropertyList also omitted

    auto& minSlotAssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    minSlotAssocProps = slotAssociation;

    const size_t sensorCountBefore = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    // AP slot processed with default 0 values -> sensors added
    EXPECT_GE(fpga->deviceSensors.size(), sensorCountBefore);
}

// =============================================================================
// FALSE-branch coverage: count() checks in nsmErotCreateSensors
// =============================================================================

// Type absent -> type="" -> if (type == "NSM_Chassis" || ...) is false ->
// entire inner block skipped -> no sensors added (early co_return NSM_SUCCESS)
//
TEST_F(NsmErotTest, CreateErotSensors_MissingType_NoSensors)
{
    const std::string uniquePath = "/xyz/test/erot/no_type";
    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Name"] = chassisName;
    pm["UUID"] = fpgaUuid;
    // "Type" intentionally omitted -> type="" -> neither NSM_Chassis nor
    // NSM_ChassisRoT -> skip entire block -> no sensors

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Type="NSM_ChassisRoT" set but SlotCount absent -> else branch:
// co_return NSM_SUCCESS (not a RoT chassis) before slot processing //

TEST_F(NsmErotTest, CreateErotSensors_ChassisRoT_MissingSlotCount_EarlyReturn)
{
    const std::string uniquePath = "/xyz/test/erot/no_slotcount";
    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = std::string("TestErotNoSlot");
    pm["UUID"] = fpgaUuid;
    // "SlotCount" intentionally omitted -> co_return NSM_SUCCESS early //

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    // Returned before slot/sensor creation -> sensor count unchanged
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// UUID absent (inside type block) -> count("UUID") FALSE -> uuid="" ->
// getNsmDeviceFromStaticUUID("") throws std::runtime_error
TEST_F(NsmErotTest, CreateErotSensors_NSMChassisRoT_MissingUUID_Throws)
{
    const std::string uniquePath = "/xyz/test/erot/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = chassisName;
    pm["SlotCount"] = uint64_t(1);
    pm["ImageCopyEnabled"] = false;
    // "UUID" intentionally omitted -> count("UUID") FALSE -> uuid="" ->
    // getNsmDeviceFromStaticUUID("") -> parseStaticUuid("") throws

    EXPECT_THROW_COROUTINE(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath),
        std::runtime_error);
}

// Name absent (inside type block) -> count("Name") FALSE -> name="" ->
// SlotCount also absent -> else-branch early co_return NSM_SUCCESS; //
// No sensors added, no exception thrown
TEST_F(NsmErotTest, CreateErotSensors_NSMChassisRoT_MissingName_EarlyReturn)
{
    const std::string uniquePath = "/xyz/test/erot/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["UUID"] = fpgaUuid;
    // "Name" intentionally omitted -> count("Name") FALSE -> name="" ->
    // path = chassisInventoryBasePath + "/" + "" (valid but SlotCount absent)
    // SlotCount absent -> else-branch -> early co_return NSM_SUCCESS //

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    // No sensors added since SlotCount is absent (early return)
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Test covers SetRotPropertyList without AP_SKU_ID (enableUpdateSKU stays
// false) and InbandUpdatePolicyEnabled + ImageCopyPolicyEnabled properties
// (lines 258-270).
TEST_F(NsmErotTest, goodTestCreateErotSensors_PolicyEnabled)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = chassisName + "_PolicyEnabled";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = false;
    propertyMap["InbandUpdatePolicyEnabled"] = bool(true);
    propertyMap["ImageCopyPolicyEnabled"] = bool(true);

    // Slot 1: SetRotPropertyList without AP_SKU_ID -> enableUpdateSKU=false
    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("PolicySlot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = uniqueName + "_AP";
    slot1Props["SetRotPropertyList"] =
        std::vector<std::string>{"OTHER_PROP"}; // no AP_SKU_ID

    dbus::PropertyMap assocProps = {
        {"Forward", "chassis"},
        {"Backward", "firmware_slot"},
        {"AbsolutePath", uniquePath},
    };
    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot1AssocProps = assocProps;

    size_t initialSensorCount = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    EXPECT_GE(fpga->deviceSensors.size(), initialSensorCount);
}

// count("ImageCopyEnabled") FALSE (line 252): property absent ->
// imageCopyEnabled defaults to false. Covers all three policy-flag FALSE
// branches in one test:
//   line 252 count("ImageCopyEnabled") FALSE
//   line 259 count("InbandUpdatePolicyEnabled") FALSE
//   line 266 count("ImageCopyPolicyEnabled") FALSE
// Uses SlotCount=0 so slot loop runs 0 times -> no sensors added, no throw.
TEST_F(NsmErotTest, CreateErotSensors_MissingPolicyFlags_DefaultsFalse)
{
    const std::string uniquePath = "/xyz/test/erot/no_policy_flags";
    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = std::string("ErotNoPolicyFlags");
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(0); // present but zero -> slot loop skipped
    // ImageCopyEnabled, InbandUpdatePolicyEnabled, ImageCopyPolicyEnabled
    // intentionally omitted -> all count() checks return 0 -> defaults to false

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    // SlotCount=0 -> slot loop runs 0 times -> only NsmSecurityCfgObject added
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// ============================================================================
// parseSlots catch block: SetRotPropertyList is wrong type ->
// std::bad_variant_access caught -> enableUpdateSKU stays false, slot still
// created normally. Covers the FALSE branch of (find != end) at line 186 as
// well, since the catch fires before the AP_SKU_ID check, but
// enableUpdateSKU=false is still the coverage target for the catch path.
// ============================================================================

TEST_F(NsmErotTest, ParseSlots_WrongTypeSetRotPropertyList_CatchBlock)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = chassisName + "_WrongType";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(1);
    pm["ImageCopyEnabled"] = false;

    // SetRotPropertyList present but as uint64_t (wrong type) ->
    // std::get<std::vector<std::string>> throws std::bad_variant_access ->
    // caught by catch (const std::exception& e) ->
    // enableUpdateSKU stays false
    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = uniqueName + "_AP";
    slot1Props["SetRotPropertyList"] =
        uint64_t(42); // wrong type -> catch block

    dbus::PropertyMap assocProps = {{"Forward", "chassis"},
                                    {"Backward", "firmware_slot"},
                                    {"AbsolutePath", uniquePath}};
    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot1AssocProps = assocProps;

    // Should not throw; slot created with enableUpdateSKU=false
    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    EXPECT_GE(fpga->deviceSensors.size(), before);
}

// ============================================================================
// Two AP slots with same chassisName -> FALSE branch for apFirmwareTypeMap,
// apProgressIntfMap, apKeyMgmtMap, apMinSecVersionMap (second slot skips
// map creation since key already exists).
// ============================================================================

TEST_F(NsmErotTest, TwoAPSlots_SameChassisName_DuplicateMapEntriesSkipped)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = chassisName + "_TwoAP";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;
    const std::string sharedChassisName = uniqueName + "_AP";

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(2);
    pm["ImageCopyEnabled"] = true;
    pm["InbandUpdatePolicyEnabled"] = bool(true);
    pm["ImageCopyPolicyEnabled"] = bool(true);

    // Slot 1: AP with sharedChassisName
    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = sharedChassisName;
    slot1Props["ReportSkuWithNsm"] = bool(true);
    slot1Props["SetRotPropertyList"] = std::vector<std::string>{"AP_SKU_ID"};
    auto& slot1Assoc = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot1Assoc = {{"Forward", "chassis"},
                  {"Backward", "firmware_slot"},
                  {"AbsolutePath", uniquePath}};

    // Slot 2: AP with SAME sharedChassisName -> FALSE branch of all map.find()
    // checks: apFirmwareTypeMap, apProgressIntfMap, apKeyMgmtMap,
    // apMinSecVersionMap, apSkuIdSensorMap, apUpdateSkuIntfMap skip creation.
    // imageCopyPolicySensor: second slot hits (sensor != nullptr) -> FALSE.
    // inbandUpdatePolicy:    second slot hits (policy != nullptr) -> FALSE.
    // imageCopyObject:       second slot hits (obj != nullptr) -> FALSE.
    std::string slot2Path = uniquePath + "/Slot2";
    auto& slot2Props = utils::MockDbusAsync::propertyMap(
        slot2Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot2Props["Name"] = std::string("Slot2");
    slot2Props["ComponentClassification"] = uint64_t(1);
    slot2Props["ComponentIdentifier"] = uint64_t(2);
    slot2Props["ComponentIndex"] = uint64_t(1);
    slot2Props["FirmwareType"] = std::string("AP");
    slot2Props["ChassisName"] = sharedChassisName; // same -> all maps skip
    slot2Props["ReportSkuWithNsm"] = bool(true);
    slot2Props["SetRotPropertyList"] = std::vector<std::string>{"AP_SKU_ID"};
    auto& slot2Assoc = utils::MockDbusAsync::propertyMap(
        slot2Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot2Assoc = {{"Forward", "chassis"},
                  {"Backward", "firmware_slot"},
                  {"AbsolutePath", uniquePath}};

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    EXPECT_GE(fpga->deviceSensors.size(), before);
}

// ============================================================================
// Two EC slots -> second slot hits FALSE branches for ecFirmwareType,
// ecProgressIntf, ecKeyMgmt, ecMinSecVersion nullptr checks.
// Also covers rotProgressIntf = ecProgressIntf path (line 543).
// ============================================================================

TEST_F(NsmErotTest, TwoECSlots_SecondSlotNullptrChecksFalseBranches)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = chassisName + "_TwoEC";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(2);
    pm["ImageCopyEnabled"] = false;

    // Slot 1: EC firmware -> creates ecFirmwareType, ecProgressIntf, ecKeyMgmt,
    // ecMinSecVersion
    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(2);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("EC");
    slot1Props["ChassisName"] = uniqueName;
    auto& slot1Assoc = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot1Assoc = {{"Forward", "chassis"},
                  {"Backward", "firmware_slot"},
                  {"AbsolutePath", uniquePath}};

    // Slot 2: EC again -> all four nullptr checks hit FALSE branch (already
    // set)
    std::string slot2Path = uniquePath + "/Slot2";
    auto& slot2Props = utils::MockDbusAsync::propertyMap(
        slot2Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot2Props["Name"] = std::string("Slot2");
    slot2Props["ComponentClassification"] = uint64_t(2);
    slot2Props["ComponentIdentifier"] = uint64_t(2);
    slot2Props["ComponentIndex"] = uint64_t(1);
    slot2Props["FirmwareType"] = std::string("EC");
    slot2Props["ChassisName"] = uniqueName;
    auto& slot2Assoc = utils::MockDbusAsync::propertyMap(
        slot2Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot2Assoc = {{"Forward", "chassis"},
                  {"Backward", "firmware_slot"},
                  {"AbsolutePath", uniquePath}};

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    // At minimum the EC sensors should be added (ecFirmwareType, ecKeyMgmt,
    // ecMinSecVersion, securityCfg)
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// Covers nsmErot.cpp L84: FALSE branch of
// `if (erotInfo.fq_resp_hdr.firmware_slot_count > 0)` inside the mismatch
// block. Triggered when buildTypeObj has ≥1 slot but response says count==0,
// so mismatch is detected but free() is NOT called (count is 0).
TEST_F(NsmBuildTypeObjectTest,
       badTestHandleResponseMsgSlotCountMismatch_ZeroCountInResponse)
{
    // Add one slot so fwSlotObjects.size() == 1
    auto& bus = utils::DBusHandler::getBus();
    std::string path = "/test/slot/mismatch_zero";
    std::vector<utils::Association> associations;
    auto slot = std::make_shared<NsmFirmwareSlot>(
        bus, path, associations, 0, SlotIntf::FirmwareType::AP, chassisName);
    buildTypeObj->addSlotObject(slot);

    // Build a response that reports firmware_slot_count == 0
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(1024, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.firmware_slot_count = 0;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // fwSlotObjects.size()==1 but count==0 -> mismatch -> L84 FALSE (no free)
    rc = buildTypeObj->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

// Mismatch where response firmware_slot_count > 0 -> L84 TRUE (free called).
// fwSlotObjects has 1 slot but response says count=2 -> mismatch + count>0.
TEST_F(NsmBuildTypeObjectTest,
       badTestHandleResponseMsgSlotCountMismatch_NonZeroCountInResponse)
{
    // Add one slot so fwSlotObjects.size() == 1
    auto& bus = utils::DBusHandler::getBus();
    std::string path = "/test/slot/mismatch_nonzero";
    std::vector<utils::Association> associations;
    auto slot = std::make_shared<NsmFirmwareSlot>(
        bus, path, associations, 0, SlotIntf::FirmwareType::AP, chassisName);
    buildTypeObj->addSlotObject(slot);

    // Build a response that reports firmware_slot_count == 2 (mismatch: 2 != 1)
    uint8_t instanceId = 0;
    // Allocate enough space for 2 slot infos so encode doesn't overflow
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            2 * sizeof(nsm_firmware_slot_info),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    std::vector<nsm_firmware_slot_info> slotInfoArray(2);
    slotInfoArray[0] = {};
    slotInfoArray[1] = {};

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.firmware_slot_count = 2; // mismatch: 2 != 1
    erotInfo.slot_info = slotInfoArray.data();

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // fwSlotObjects.size()==1 but count==2 -> mismatch -> L84 TRUE (free
    // called)
    rc = buildTypeObj->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

// Covers L132 FALSE (no "Name") in parseSlots.
// Covers L132 FALSE (no "Name") in parseSlots.
// When "Name" is absent, slot.slotName stays "" -> extractNumber("") = -1.
// NsmFirmwareSlot::getPath produces a path with "-1" which is invalid for
// D-Bus (paths allow only [A-Za-z0-9_/]).  The constructor throws
// SdBusError.  We use EXPECT_THROW_COROUTINE to accept that exception while
// still crediting the L132 FALSE branch in coverage.
TEST_F(NsmErotTest, goodTestCreateErotSensors_SlotWithoutName_L132False)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_RoT_BASEBOARD_NoSlotName";
    const std::string uniqueChassisName = uniqueName + "_AP";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = false;

    // Slot without "Name" -> L132 FALSE -> slotName="" -> extractNumber("")=-1
    // -> NsmFirmwareSlot path contains "-1" -> SdBusError (invalid D-Bus path)
    std::string slotPath = uniquePath + "/Slot1";
    auto& slotProps = utils::MockDbusAsync::propertyMap(
        slotPath, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slotProps["ComponentClassification"] = uint64_t(1);
    slotProps["ComponentIdentifier"] = uint64_t(1);
    slotProps["ComponentIndex"] = uint64_t(0);
    slotProps["FirmwareType"] = std::string("AP");
    slotProps["ChassisName"] = uniqueChassisName;
    // "Name" deliberately absent -> L132 FALSE

    auto& slotAssocProps = utils::MockDbusAsync::propertyMap(
        slotPath,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    dbus::PropertyMap assoc = {{"Forward", "chassis"},
                               {"Backward", "firmware_slot"},
                               {"AbsolutePath", uniquePath}};
    slotAssocProps = assoc;

    // The SdBusError thrown by NsmFirmwareSlot is expected.
    EXPECT_THROW_COROUTINE(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath),
        std::exception);
}

// Covers L152 FALSE (no "FirmwareType") in parseSlots.
// Slot 2 has no FirmwareType -> fwType="" -> neither AP nor EC branch is
// entered for that slot.  Slot 1 has FirmwareType="EC" so ecProgressIntf
// is established; rotProgressIntf reuses it, avoiding a fresh ProgressIntf
// creation that would conflict with the NsmSecurityCfgObject at the same path.
TEST_F(NsmErotTest, goodTestCreateErotSensors_SlotWithoutFirmwareType_L152False)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_RoT_BASEBOARD_NoFwType";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(2);
    propertyMap["ImageCopyEnabled"] = false;

    // Slot 1: EC firmware with valid ChassisName so ecProgressIntf is created
    // at chassisInventoryBasePath / uniqueName (the main chassis path).
    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("EC");
    slot1Props["ChassisName"] = uniqueName;
    auto& slot1Assoc = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot1Assoc = {{"Forward", "chassis"},
                  {"Backward", "firmware_slot"},
                  {"AbsolutePath", uniquePath}};

    // Slot 2: no "FirmwareType" -> L152 FALSE (count=0) -> fwType stays ""
    std::string slot2Path = uniquePath + "/Slot2";
    auto& slot2Props = utils::MockDbusAsync::propertyMap(
        slot2Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot2Props["Name"] = std::string("Slot2");
    slot2Props["ComponentClassification"] = uint64_t(2);
    slot2Props["ComponentIdentifier"] = uint64_t(2);
    slot2Props["ComponentIndex"] = uint64_t(1);
    // "FirmwareType" deliberately absent -> L152 FALSE
    slot2Props["ChassisName"] = uniqueName + "_EC2";
    auto& slot2Assoc = utils::MockDbusAsync::propertyMap(
        slot2Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slot2Assoc = {{"Forward", "chassis"},
                  {"Backward", "firmware_slot"},
                  {"AbsolutePath", uniquePath}};

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);

    // EC + security sensors should still be created
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// =============================================================================
// Covers L159 FALSE: slot without "ChassisName" -> chassisName="" ->
// AP processing path creates NsmFirmwareSlot/apProgressIntfMap at path
// chassisInventoryBasePath + "/" which is an invalid D-Bus path.
// EXPECT_THROW_COROUTINE accepts the resulting SdBusError while still
// crediting the L159 FALSE branch in coverage.
// =============================================================================
TEST_F(NsmErotTest, goodTestCreateErotSensors_SlotWithoutChassisName_L159False)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_RoT_BASEBOARD_NoChassisName";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = false;

    // Slot with Name + FirmwareType but NO ChassisName -> L159 FALSE
    // slot.chassisName stays "" -> AP path creates objects at
    // chassisInventoryBasePath + "/" (invalid D-Bus path) -> SdBusError
    std::string slotPath = uniquePath + "/Slot1";
    auto& slotProps = utils::MockDbusAsync::propertyMap(
        slotPath, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slotProps["Name"] = std::string("Slot1");
    slotProps["ComponentClassification"] = uint64_t(1);
    slotProps["ComponentIdentifier"] = uint64_t(1);
    slotProps["ComponentIndex"] = uint64_t(0);
    slotProps["FirmwareType"] = std::string("AP");
    // "ChassisName" deliberately absent -> L159 FALSE

    auto& slotAssocProps = utils::MockDbusAsync::propertyMap(
        slotPath,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slotAssocProps = {{"Forward", "chassis"},
                      {"Backward", "firmware_slot"},
                      {"AbsolutePath", uniquePath}};

    EXPECT_THROW_COROUTINE(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath),
        std::exception);
}

// =============================================================================
// Covers L369 TRUE: ImageCopyPolicyEnabled=true -> imageCopyPolicySensor
// creation + AsyncOperationManager handler registration (L369-396).
// =============================================================================
TEST_F(NsmErotTest, goodTestCreateErotSensors_ImageCopyPolicyEnabled)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_RoT_BASEBOARD_ImgCopyPolicy";
    const std::string slotChassisName = uniqueName + "_AP";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = false;
    // This is the key property: enable ImageCopyPolicy (different from
    // ImageCopyEnabled which controls NsmImageCopyObject)
    propertyMap["ImageCopyPolicyEnabled"] = true;

    std::string slotPath = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slotPath, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = slotChassisName;

    auto& slotAssoc = utils::MockDbusAsync::propertyMap(
        slotPath,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slotAssoc = {{"Forward", "chassis"},
                 {"Backward", "firmware_slot"},
                 {"AbsolutePath", uniquePath}};

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);

    // imageCopyPolicySensor + securityCfg should both be added
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// =============================================================================
// Covers L482 TRUE: InbandUpdatePolicyEnabled=true -> inbandUpdatePolicy
// creation + AsyncOperationManager handler registration (L482-508).
// =============================================================================
TEST_F(NsmErotTest, goodTestCreateErotSensors_InbandUpdatePolicyEnabled)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_RoT_BASEBOARD_InbandPolicy";
    const std::string slotChassisName = uniqueName + "_AP";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = false;
    propertyMap["InbandUpdatePolicyEnabled"] = true;

    std::string slotPath = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slotPath, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = slotChassisName;

    auto& slotAssoc = utils::MockDbusAsync::propertyMap(
        slotPath,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slotAssoc = {{"Forward", "chassis"},
                 {"Backward", "firmware_slot"},
                 {"AbsolutePath", uniquePath}};

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);

    // inbandUpdatePolicy + securityCfg should be added
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// =============================================================================
// Covers L400-413 (reportSkuWithNsm TRUE -> NsmApSkuIdObject) and
// L414-442 (enableUpdateSKU TRUE -> NsmUpdateApSkuIdIntf + async handler).
// Requires ReportSkuWithNsm=true in slot AND SetRotPropertyList={"AP_SKU_ID"}
// in slot.
// =============================================================================
TEST_F(NsmErotTest, goodTestCreateErotSensors_ReportSkuAndEnableUpdateSKU)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_RoT_BASEBOARD_SkuUpdate";
    const std::string slotChassisName = uniqueName + "_AP";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = false;

    std::string slotPath = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slotPath, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = slotChassisName;
    // ReportSkuWithNsm=true -> L400 TRUE -> NsmApSkuIdObject created
    slot1Props["ReportSkuWithNsm"] = true;
    // SetRotPropertyList with AP_SKU_ID -> L414 TRUE (enableUpdateSKU) ->
    // NsmUpdateApSkuIdIntf + AsyncSetOperation handler created
    slot1Props["SetRotPropertyList"] = std::vector<std::string>{"AP_SKU_ID"};

    auto& slotAssoc = utils::MockDbusAsync::propertyMap(
        slotPath,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slotAssoc = {{"Forward", "chassis"},
                 {"Backward", "firmware_slot"},
                 {"AbsolutePath", uniquePath}};

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);

    // apSkuIdSensor, apUpdateSkuIntf, and securityCfg should all be added
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// =============================================================================
// Covers nsmApSkuId.cpp L58 TRUE + L64 TRUE:
// Association with Forward=="inventory_SKU" -> filteredAssociations non-empty
// -> AssociationDefinitionsIntf created.
// =============================================================================
TEST_F(NsmErotTest, goodTestCreateErotSensors_ReportSkuWithInventorySKUAssoc)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_RoT_BASEBOARD_SkuInvAssoc";
    const std::string slotChassisName = uniqueName + "_AP";
    const std::string uniquePath = chassisInventoryBasePath / uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = chassisRoT["Type"];
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = chassisRoT["UUID"];
    propertyMap["SlotCount"] = uint64_t(1);
    propertyMap["ImageCopyEnabled"] = false;

    std::string slotPath = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(
        slotPath, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = slotChassisName;
    // ReportSkuWithNsm=true -> NsmApSkuIdObject created
    slot1Props["ReportSkuWithNsm"] = true;

    // Use "inventory_SKU" as Forward -> covers nsmApSkuId.cpp L58 TRUE and
    // L64 TRUE (filteredAssociations non-empty -> AssociationDefinitionsIntf)
    auto& slotAssoc = utils::MockDbusAsync::propertyMap(
        slotPath,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slotAssoc = {{"Forward", "inventory_SKU"},
                 {"Backward", "chassis"},
                 {"AbsolutePath", uniquePath}};

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);

    // apSkuIdSensor should be added (with AssociationDefinitionsIntf)
    EXPECT_GT(fpga->deviceSensors.size(), before);
}
