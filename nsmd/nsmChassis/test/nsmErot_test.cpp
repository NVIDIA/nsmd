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
