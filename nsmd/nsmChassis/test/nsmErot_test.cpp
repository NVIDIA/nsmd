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

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmErot.hpp"
#include "nsmRoTProperty.hpp"

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

    /** @brief One RoT slot as the NSM_Chassis RoT feature params describe it */
    struct SlotSpec
    {
        std::string fwType = "AP";
        std::string chassisName;
        std::string type = "Active";
        uint64_t classification = 1;
        uint64_t identifier = 1;
        uint64_t index = 0;
        bool isRoT = false;
        bool reportSkuWithNsm = false;
        bool updateSku = false;
    };

    /** @brief Slot arrays of the RoT feature params; element i is slot i + 1 */
    static dbus::PropertyMap slotParams(const std::vector<SlotSpec>& slots)
    {
        std::vector<std::string> chassisNames, fwTypes, types;
        std::vector<uint64_t> classes, ids, indexes, isRoT, reportSku,
            updateSku, softwareAssoc;
        for (const auto& slot : slots)
        {
            chassisNames.push_back(slot.chassisName);
            fwTypes.push_back(slot.fwType);
            types.push_back(slot.type);
            classes.push_back(slot.classification);
            ids.push_back(slot.identifier);
            indexes.push_back(slot.index);
            isRoT.push_back(slot.isRoT);
            reportSku.push_back(slot.reportSkuWithNsm);
            updateSku.push_back(slot.updateSku);
            softwareAssoc.push_back(0);
        }
        return {{"SlotChassisName", chassisNames},
                {"SlotFirmwareType", fwTypes},
                {"SlotType", types},
                {"SlotComponentClassification", classes},
                {"SlotComponentIdentifier", ids},
                {"SlotComponentIndex", indexes},
                {"SlotIsRoT", isRoT},
                {"SlotReportSkuWithNsm", reportSku},
                {"SlotUpdateSkuEnabled", updateSku},
                {"SlotSoftwareAssociationsEnabled", softwareAssoc}};
    }

    /** @brief Run the RoT feature of the device `name` on the mock NsmDevice */
    requester::Coroutine createRoT(const std::string& name,
                                   dbus::PropertyMap params,
                                   bool withDevice = true)
    {
        nsm::features::DeviceContext dev;
        dev.name = name;
        dev.kind = "erot";
        dev.staticUuid = fpgaUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = fpga;
        }
        params["Name"] = name;
        return nsm::features::createRoTFeature(mockManager, dev, params);
    }
};

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

// =============================================================================
// createRoTFeature: RoT sensors from the slot arrays of the feature params
// =============================================================================

TEST_F(NsmErotTest, goodTestCreateRoTAPAndECSlots)
{
    const std::string name = "HGX_RoT_BASEBOARD_ApEc";
    auto params = slotParams(
        {{.fwType = "AP", .chassisName = name + "_AP", .identifier = 1},
         {.fwType = "EC",
          .chassisName = name + "_EC",
          .type = "InActive",
          .classification = 2,
          .identifier = 2,
          .index = 1}});
    params["ImageCopyEnabled"] = true;

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

TEST_F(NsmErotTest, goodTestCreateRoTImageCopyEnabled)
{
    const std::string name = "HGX_RoT_BASEBOARD_ImageCopy";
    auto params = slotParams({{.chassisName = name + "_AP"}});
    params["ImageCopyEnabled"] = true;

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // min sec version, firmware type, key management, image copy, security cfg
    EXPECT_GE(fpga->deviceSensors.size(), before + 5);
}

TEST_F(NsmErotTest, goodTestCreateRoTUpdateSkuSlot)
{
    const std::string name = "HGX_RoT_BASEBOARD_UpdateSKU";
    auto params =
        slotParams({{.chassisName = name + "_AP", .updateSku = true}});

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

TEST_F(NsmErotTest, goodTestCreateRoTECSlot)
{
    const std::string name = "HGX_RoT_BASEBOARD_Ec";
    auto params = slotParams({{.fwType = "EC",
                               .chassisName = name,
                               .classification = 2,
                               .identifier = 2}});

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // EC min sec version, firmware type, key management, security cfg
    EXPECT_GE(fpga->deviceSensors.size(), before + 4);
}

TEST_F(NsmErotTest, goodTestCreateRoTNoSlots)
{
    // Empty slot arrays: only the security config sensor is created
    const std::string name = "HGX_RoT_BASEBOARD_NoSlots";

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, slotParams({}));

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(fpga->deviceSensors.size(), before + 1);
}

TEST_F(NsmErotTest, goodTestCreateRoTNoSlotParamsAtAll)
{
    // A list that sets no slot arrays has zero slots
    const std::string name = "HGX_RoT_BASEBOARD_NoParams";

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, {});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(fpga->deviceSensors.size(), before + 1);
}

TEST_F(NsmErotTest, badTestCreateRoTNoNsmDevice)
{
    const std::string name = "HGX_RoT_BASEBOARD_NoDevice";

    auto rc = createRoT(name, slotParams({{.chassisName = name + "_AP"}}),
                        false);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_FALSE(rc.exception());
}

TEST_F(NsmErotTest, badTestCreateRoTSlotArraysDifferInLength)
{
    const std::string name = "HGX_RoT_BASEBOARD_BadLen";
    auto params = slotParams({{.chassisName = name + "_AP"},
                              {.fwType = "EC", .chassisName = name + "_EC"}});
    // One entry fewer than the two slots
    params["SlotComponentIndex"] = std::vector<uint64_t>{0};

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(fpga->deviceSensors.size(), before);
}

TEST_F(NsmErotTest, badTestCreateRoTMissingSoftwareAssociationFlags)
{
    // Without SlotSoftwareName / SlotSoftwareNameFromChassis the association
    // flags must be given for every slot
    const std::string name = "HGX_RoT_BASEBOARD_NoAssocFlags";
    auto params = slotParams({{.chassisName = name + "_AP"}});
    params.erase("SlotSoftwareAssociationsEnabled");

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(fpga->deviceSensors.size(), before);
}

TEST_F(NsmErotTest, goodTestCreateRoTSlotExtendedProperties)
{
    const std::string name = "HGX_RoT_BASEBOARD_ExtSlot";
    auto params = slotParams({{.chassisName = name + "_AP",
                               .isRoT = true,
                               .reportSkuWithNsm = true,
                               .updateSku = true}});

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

TEST_F(NsmErotTest, goodTestCreateRoTPolicyFlags)
{
    const std::string name = "HGX_RoT_BASEBOARD_Policy";
    auto params = slotParams({{.chassisName = name + "_AP"}});
    params["InbandUpdatePolicyEnabled"] = true;
    params["ImageCopyPolicyEnabled"] = true;
    params["FailoverPolicyEnabled"] = true;

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // inband update, image copy and failover policy sensors, on top of the
    // sensors of the slot and the security config
    EXPECT_GE(fpga->deviceSensors.size(), before + 3 + 4);
}

TEST_F(NsmErotTest, goodTestCreateRoTPolicyFlagsOffByDefault)
{
    const std::string name = "HGX_RoT_BASEBOARD_NoPolicy";
    auto params = slotParams({{.chassisName = name + "_AP"}});

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);
    const size_t withoutPolicies = fpga->deviceSensors.size() - before;

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // min sec version, firmware type, key management and security cfg
    EXPECT_EQ(withoutPolicies, 4u);
}

TEST_F(NsmErotTest, goodTestCreateRoTTwoAPSlotsSameChassis)
{
    // The second slot reuses the objects of the first one
    const std::string name = "HGX_RoT_BASEBOARD_TwoAP";
    const std::string shared = name + "_AP";
    auto params = slotParams(
        {{.chassisName = shared, .reportSkuWithNsm = true, .updateSku = true},
         {.chassisName = shared,
          .identifier = 2,
          .index = 1,
          .reportSkuWithNsm = true,
          .updateSku = true}});
    params["ImageCopyEnabled"] = true;
    params["InbandUpdatePolicyEnabled"] = true;
    params["ImageCopyPolicyEnabled"] = true;

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

TEST_F(NsmErotTest, goodTestCreateRoTTwoECSlots)
{
    const std::string name = "HGX_RoT_BASEBOARD_TwoEC";
    auto params = slotParams({{.fwType = "EC",
                               .chassisName = name,
                               .classification = 2,
                               .identifier = 1},
                              {.fwType = "EC",
                               .chassisName = name,
                               .classification = 2,
                               .identifier = 2,
                               .index = 1}});

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // One EC set (min sec version, firmware type, key management) and the
    // security config, however many EC slots there are
    EXPECT_EQ(fpga->deviceSensors.size(), before + 4);
}

TEST_F(NsmErotTest, badTestCreateRoTAPSlotWithoutChassisName)
{
    // An empty chassis name makes an invalid D-Bus path
    const std::string name = "HGX_RoT_BASEBOARD_NoChassisName";
    auto params = slotParams({{.chassisName = ""}});

    EXPECT_THROW_COROUTINE(createRoT(name, params), std::exception);
}

// SlotSoftwareName: named software targets; an empty entry gives no target
TEST_F(NsmErotTest, goodTestCreateRoTNamedSoftwareTargetRegistersComponent)
{
    const std::string name = "HGX_RoT_BASEBOARD_SwNamed";
    auto params = slotParams({{.chassisName = name + "_AP",
                               .classification = 7,
                               .identifier = 0x1234,
                               .index = 3}});
    params.erase("SlotSoftwareAssociationsEnabled");
    params["SlotSoftwareName"] = std::vector<std::string>{"FW_RoTTest_SwNamed"};

    auto rc = createRoT(name, params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    auto info = findActiveSlotComponent(
        (firmwareInventoryBasePath / "FW_RoTTest_SwNamed").string());
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->classification, 7);
    EXPECT_EQ(info->identifier, 0x1234);
    EXPECT_EQ(info->index, 3);
}

TEST_F(NsmErotTest, goodTestCreateRoTEmptySoftwareNameHasNoTarget)
{
    const std::string name = "HGX_RoT_BASEBOARD_SwEmpty";
    auto params =
        slotParams({{.chassisName = name + "_AP", .identifier = 0x4321}});
    params.erase("SlotSoftwareAssociationsEnabled");
    params["SlotSoftwareName"] = std::vector<std::string>{""};

    auto rc = createRoT(name, params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // A registration under an empty software name would be keyed by the bare
    // software base path
    EXPECT_FALSE(
        findActiveSlotComponent((firmwareInventoryBasePath / "").string())
            .has_value());
}

// SlotSoftwareNameFromChassis: <PlatformPrefix>FW_<chassis without prefix>;
// the prefix falls back to the one of the device record
TEST_F(NsmErotTest, goodTestCreateRoTSoftwareNameFromChassisDevicePrefix)
{
    const std::string name = "HGX_RoT_BASEBOARD_SwChassis";
    auto params = slotParams({{.chassisName = name + "_AP",
                               .classification = 9,
                               .identifier = 0x55,
                               .index = 1}});
    params["SlotSoftwareNameFromChassis"] = true;

    auto rc = createRoT(name, params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // dev.platformPrefix is "HGX_": HGX_RoT_BASEBOARD_SwChassis_AP ->
    // HGX_FW_RoT_BASEBOARD_SwChassis_AP
    auto info = findActiveSlotComponent(
        (firmwareInventoryBasePath / "HGX_FW_RoT_BASEBOARD_SwChassis_AP")
            .string());
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->classification, 9);
    EXPECT_EQ(info->identifier, 0x55);
    EXPECT_EQ(info->index, 1);
}

TEST_F(NsmErotTest, goodTestCreateRoTSoftwareNameFromChassisParamPrefix)
{
    // The PlatformPrefix param wins over the prefix of the device record
    const std::string name = "MGX_RoT_BASEBOARD_SwPrefix";
    auto params = slotParams({{.chassisName = name + "_AP",
                               .classification = 4,
                               .identifier = 0x66,
                               .index = 2}});
    params["SlotSoftwareNameFromChassis"] = true;
    params["PlatformPrefix"] = std::string("MGX_");

    auto rc = createRoT(name, params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    auto info = findActiveSlotComponent(
        (firmwareInventoryBasePath / "MGX_FW_RoT_BASEBOARD_SwPrefix_AP")
            .string());
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->identifier, 0x66);
}

TEST_F(NsmErotTest, goodTestCreateRoTInactiveSlotRegistersNoComponent)
{
    const std::string name = "HGX_RoT_BASEBOARD_SwInactive";
    auto params = slotParams({{.chassisName = name + "_AP",
                               .type = "InActive",
                               .identifier = 0x77}});
    params["SlotSoftwareNameFromChassis"] = true;

    auto rc = createRoT(name, params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_FALSE(findActiveSlotComponent((firmwareInventoryBasePath /
                                          "HGX_FW_RoT_BASEBOARD_SwInactive_AP")
                                             .string())
                     .has_value());
}

TEST_F(NsmErotTest, goodTestCreateRoTInventorySkuAssociation)
{
    const std::string name = "HGX_RoT_BASEBOARD_SkuAssoc";
    auto params =
        slotParams({{.chassisName = name + "_AP", .reportSkuWithNsm = true}});
    params["SlotSkuInventoryPath"] =
        std::vector<std::string>{"board/HGX_BASEBOARD"};

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

TEST_F(NsmErotTest, badTestCreateRoTSkuInventoryPathLengthMismatch)
{
    const std::string name = "HGX_RoT_BASEBOARD_SkuBadLen";
    auto params = slotParams({{.chassisName = name + "_AP"}});
    params["SlotSkuInventoryPath"] = std::vector<std::string>{"board/A",
                                                              "board/B"};

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(fpga->deviceSensors.size(), before);
}

TEST_F(NsmErotTest, goodTestCreateFWComponentTypeCreatesNothing)
{
    nsm::features::DeviceContext dev;
    dev.name = "HGX_RoT_BASEBOARD_FwType";
    dev.kind = "erot";
    dev.staticUuid = fpgaUuid;
    dev.device = fpga;

    const size_t sensorsBefore = fpga->deviceSensors.size();
    const size_t roundRobinBefore = fpga->roundRobinSensors.size();
    auto rc = nsm::features::createFWComponentTypeFeature(
        mockManager, dev,
        {{"Name", std::string("AP")}, {"ComponentType", std::string("AP")}});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(fpga->deviceSensors.size(), sensorsBefore);
    EXPECT_EQ(fpga->roundRobinSensors.size(), roundRobinBefore);
}

TEST_F(NsmErotTest, goodTestCreateFWComponentTypeWithoutParamsStillSucceeds)
{
    nsm::features::DeviceContext dev;
    dev.name = "HGX_RoT_BASEBOARD_FwTypeNoParams";
    dev.kind = "erot";

    auto rc = nsm::features::createFWComponentTypeFeature(mockManager, dev, {});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
}

TEST_F(NsmErotTest, goodTestCreateRoTImageCopyStateEnabledOnECSlot)
{
    const std::string name = "HGX_RoT_BASEBOARD_ImageCopyState";
    auto params = slotParams({{.fwType = "EC",
                               .chassisName = name,
                               .classification = 2,
                               .identifier = 2}});
    params["ImageCopyStateEnabled"] = true;

    const size_t before = fpga->deviceSensors.size();
    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // EC sensors plus the image copy state sensor
    EXPECT_GE(fpga->deviceSensors.size(), before + 5);
    EXPECT_FALSE(fpga->imageCopyStateSensor.expired());
}

TEST_F(NsmErotTest, goodTestCreateRoTImageCopyStateDisabledByDefault)
{
    const std::string name = "HGX_RoT_BASEBOARD_ImageCopyStateOff";
    auto params = slotParams({{.fwType = "EC",
                               .chassisName = name,
                               .classification = 2,
                               .identifier = 2}});

    auto rc = createRoT(name, params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_TRUE(fpga->imageCopyStateSensor.expired());
}
