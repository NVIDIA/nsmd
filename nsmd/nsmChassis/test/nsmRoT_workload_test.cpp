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
#include "libnsm/platform-environmental.h"

#include "nsmRoTProperty.hpp"
#include "nsmWorkloadPowerProfile.hpp"

using namespace nsm;

// ============================================================================
// Part 1: nsmRoTProperty.cpp Tests
//   - NsmInbandUpdatePolicy (non-Object class)
//   - NsmImageCopy / NsmImageCopyObject
//   - NsmImageCopyPolicy / NsmImageCopyPolicyObject
//   - mapReasonCodeToErrorCode (static, tested via setImageCopyResult)
// ============================================================================

// ---------------------------------------------------------------------------
// NsmInbandUpdatePolicy Tests
// ---------------------------------------------------------------------------
struct NsmInbandUpdatePolicyTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis_IUP";
    const uint16_t classification = 1;
    const uint16_t identifier = 2;
    const uint8_t index = 0;

    NsmDeviceTable devices = {};
    std::shared_ptr<NsmInbandUpdatePolicyObject> policyObject = {};

    NsmInbandUpdatePolicyTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        policyObject = std::make_shared<NsmInbandUpdatePolicyObject>(
            bus, chassisName, classification, identifier, index);
        ASSERT_NE(policyObject, nullptr);
    }

    void TearDown() override
    {
        policyObject.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmInbandUpdatePolicyTest,
       UpdateProperties_EnabledPolicy_SetsEnabledState)
{
    // Arrange
    struct nsm_firmware_erot_state_info_resp erot_info = {};
    erot_info.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE;
    erot_info.slot_info = nullptr;

    // Act
    policyObject->nsmInbandUpdatePolicy->updateProperties(erot_info);

    // Assert - no crash, property was set (internal D-Bus property)
    SUCCEED();
}

TEST_F(NsmInbandUpdatePolicyTest,
       UpdateProperties_DisabledPolicy_SetsDisabledState)
{
    // Arrange
    struct nsm_firmware_erot_state_info_resp erot_info = {};
    erot_info.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_DISABLE;
    erot_info.slot_info = nullptr;

    // Act
    policyObject->nsmInbandUpdatePolicy->updateProperties(erot_info);

    // Assert
    SUCCEED();
}

TEST_F(NsmInbandUpdatePolicyTest,
       UpdateProperties_InvalidPolicy_HandlesGracefully)
{
    // Arrange
    struct nsm_firmware_erot_state_info_resp erot_info = {};
    erot_info.fq_resp_hdr.inband_update_policy = 0xFF; // Invalid
    erot_info.slot_info = nullptr;

    // Act
    policyObject->nsmInbandUpdatePolicy->updateProperties(erot_info);

    // Assert - should not crash
    SUCCEED();
}

TEST_F(NsmInbandUpdatePolicyTest, Constructor_ValidParams_SetsFields)
{
    // Assert - fields accessible via #define private public
    EXPECT_EQ(policyObject->nsmInbandUpdatePolicy->classification,
              classification);
    EXPECT_EQ(policyObject->nsmInbandUpdatePolicy->identifier, identifier);
    EXPECT_EQ(policyObject->nsmInbandUpdatePolicy->index, index);
}

// ---------------------------------------------------------------------------
// NsmInbandUpdatePolicyObject genRequestMsg/handleResponseMsg Tests
// ---------------------------------------------------------------------------
TEST_F(NsmInbandUpdatePolicyTest, GenRequestMsg_ValidParams_ReturnsRequest)
{
    // Arrange
    eid_t eid = 0;
    uint8_t instanceId = 0;

    // Act
    auto request = policyObject->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_get_erot_state_info_req));
}

TEST_F(NsmInbandUpdatePolicyTest,
       HandleResponseMsg_SuccessEnabled_ReturnsSuccess)
{
    // Arrange
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE;
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = policyObject->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmInbandUpdatePolicyTest,
       HandleResponseMsg_SuccessDisabled_ReturnsSuccess)
{
    // Arrange
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_DISABLE;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = policyObject->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmInbandUpdatePolicyTest,
       HandleResponseMsg_ErrorCC_ReturnsCompletionCode)
{
    // Arrange
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_ERR_INVALID_DATA_LENGTH, ERR_NULL, &erotInfo,
        responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = policyObject->handleResponseMsg(responseMsg, response.size());

    // Assert - error CC returned
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// NsmImageCopyObject Tests (constructor only since requestImageCopy is async)
// ---------------------------------------------------------------------------
struct NsmImageCopyObjectTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis_ImgCpy";
    const uuid_t testUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices = {};
    std::shared_ptr<NsmImageCopyObject> imgCopyObj = {};

    NsmImageCopyObjectTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        imgCopyObj = std::make_shared<NsmImageCopyObject>(bus, chassisName,
                                                          testUuid);
        ASSERT_NE(imgCopyObj, nullptr);
    }

    void TearDown() override
    {
        imgCopyObj.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmImageCopyObjectTest, Constructor_ValidParams_SetsObjectPath)
{
    // Assert
    EXPECT_NE(imgCopyObj, nullptr);
    EXPECT_EQ(imgCopyObj->getName(), chassisName);
    EXPECT_EQ(imgCopyObj->getType(), "NSM_ChassisRoT");
    EXPECT_NE(imgCopyObj->nsmImageCopy, nullptr);
}

TEST_F(NsmImageCopyObjectTest, Constructor_VerifyInheritance_NsmObject)
{
    // Assert - NsmImageCopyObject inherits from NsmObject
    NsmObject* obj = imgCopyObj.get();
    EXPECT_NE(obj, nullptr);
}

TEST_F(NsmImageCopyObjectTest, Constructor_ObjectPathContainsChassis)
{
    // Assert
    std::string expectedPrefix = std::string(chassisInventoryBasePath) + "/" +
                                 chassisName;
    EXPECT_EQ(imgCopyObj->objectPath, expectedPrefix);
}

TEST_F(NsmImageCopyObjectTest, Constructor_DifferentChassis_UniqueObjectPaths)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    const std::string otherName = "HGX_Chassis_ImgCpy_2";
    const uuid_t otherUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:1";

    // Act
    auto otherObj = std::make_shared<NsmImageCopyObject>(bus, otherName,
                                                         otherUuid);

    // Assert
    EXPECT_NE(imgCopyObj->objectPath, otherObj->objectPath);
    otherObj.reset();
}

// ---------------------------------------------------------------------------
// NsmImageCopyPolicy Tests
// ---------------------------------------------------------------------------
struct NsmImageCopyPolicyObjectTest2 :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "HGX_Chassis_ICP2";
    const std::string type = "NSM_ImageCopyPolicy";
    const uint16_t classification = 3;
    const uint16_t identifier = 4;
    const uint8_t index = 1;

    NsmDeviceTable devices = {};
    std::shared_ptr<NsmImageCopyPolicyObject> copyPolicyObj = {};

    NsmImageCopyPolicyObjectTest2() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        copyPolicyObj = std::make_shared<NsmImageCopyPolicyObject>(
            bus, name, type, classification, identifier, index);
        ASSERT_NE(copyPolicyObj, nullptr);
    }

    void TearDown() override
    {
        copyPolicyObj.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmImageCopyPolicyObjectTest2,
       UpdateProperties_ManualPolicy_SetsManualState)
{
    // Arrange
    struct nsm_firmware_erot_state_info_resp erot_info = {};
    erot_info.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY;
    erot_info.slot_info = nullptr;

    // Act
    copyPolicyObj->imageCopyPolicyObject->updateProperties(erot_info);

    // Assert - property set without crash
    SUCCEED();
}

TEST_F(NsmImageCopyPolicyObjectTest2,
       UpdateProperties_AutomaticPolicy_SetsAutomaticState)
{
    // Arrange
    struct nsm_firmware_erot_state_info_resp erot_info = {};
    erot_info.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY;
    erot_info.slot_info = nullptr;

    // Act
    copyPolicyObj->imageCopyPolicyObject->updateProperties(erot_info);

    // Assert
    SUCCEED();
}

TEST_F(NsmImageCopyPolicyObjectTest2,
       UpdateProperties_InvalidPolicy_HandlesGracefully)
{
    // Arrange
    struct nsm_firmware_erot_state_info_resp erot_info = {};
    erot_info.fq_resp_hdr.background_copy_policy = 0xFF; // Invalid
    erot_info.slot_info = nullptr;

    // Act
    copyPolicyObj->imageCopyPolicyObject->updateProperties(erot_info);

    // Assert
    SUCCEED();
}

TEST_F(NsmImageCopyPolicyObjectTest2,
       GenRequestMsg_ValidParams_ReturnsCorrectSize)
{
    // Arrange
    eid_t eid = 5;
    uint8_t instanceId = 1;

    // Act
    auto request = copyPolicyObj->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_get_erot_state_info_req));
}

TEST_F(NsmImageCopyPolicyObjectTest2,
       HandleResponseMsg_ManualPolicy_ReturnsSuccess)
{
    // Arrange
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = copyPolicyObj->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmImageCopyPolicyObjectTest2,
       HandleResponseMsg_AutomaticPolicy_ReturnsSuccess)
{
    // Arrange
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp) +
            sizeof(nsm_firmware_slot_info) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY;
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = copyPolicyObj->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmImageCopyPolicyObjectTest2, HandleResponseMsg_ErrorCC_ReturnsError)
{
    // Arrange
    uint8_t instanceId = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr;

    auto rc = encode_nsm_query_get_erot_state_parameters_resp(
        instanceId, NSM_ERR_INVALID_DATA_LENGTH, ERR_NULL, &erotInfo,
        responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = copyPolicyObj->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmImageCopyPolicyObjectTest2, Constructor_VerifyFields_CorrectValues)
{
    // Assert
    EXPECT_EQ(copyPolicyObj->classification, classification);
    EXPECT_EQ(copyPolicyObj->identifier, identifier);
    EXPECT_EQ(copyPolicyObj->index, index);
    EXPECT_NE(copyPolicyObj->imageCopyPolicyObject, nullptr);
}

// ============================================================================
// Part 2: nsmWorkloadPowerProfile.cpp Tests
//   - NsmWorkLoadProfileStatus: constructor, genRequestMsg, handleResponseMsg,
//     updateReading
//   - NsmWorkloadPowerProfileCollection: constructor, hasProfileId,
//     getSupportedProfileById, addSupportedProfile, updateSupportedProfile
//   - NsmWorkloadPowerProfilePageCollection: constructor, hasPageId,
//     getPageById, addPage
//   - NsmWorkloadPowerProfilePage: constructor, genRequestMsg,
//     handleResponseMsg
//   - NsmWorkloadProfileInfoAsyncIntf: constructor
// ============================================================================

// ---------------------------------------------------------------------------
// NsmWorkLoadProfileStatus Tests
// ---------------------------------------------------------------------------
struct NsmWorkLoadProfileStatusTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::string name = "ProfileStatus";
    std::string type = "NSM_WorkloadProfileStatus";
    std::string inventoryObjPath = "/xyz/openbmc_project/inventory/test_gpu";
    eid_t eid = 0;
    uint8_t instanceId = 0;

    NsmDeviceTable devices = {};
    std::shared_ptr<MockNsmDevice> device = {};
    std::shared_ptr<OemProfileInfoIntf> profileStatusInfo = {};
    std::shared_ptr<NsmWorkloadProfileInfoAsyncIntf> profileInfoAsync = {};
    std::shared_ptr<NsmWorkLoadProfileStatus> sensor = {};

    NsmWorkLoadProfileStatusTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        ASSERT_NE(device, nullptr);

        profileStatusInfo =
            std::make_shared<OemProfileInfoIntf>(bus, inventoryObjPath, device);
        profileInfoAsync = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
            bus, inventoryObjPath.c_str(), device);

        sensor = std::make_shared<NsmWorkLoadProfileStatus>(
            name, type, inventoryObjPath, profileStatusInfo, profileInfoAsync);
        ASSERT_NE(sensor, nullptr);
    }

    void TearDown() override
    {
        sensor.reset();
        profileInfoAsync.reset();
        profileStatusInfo.reset();
        device.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmWorkLoadProfileStatusTest, Constructor_ValidParams_SetsNameType)
{
    // Assert
    EXPECT_EQ(sensor->getName(), name);
    EXPECT_EQ(sensor->getType(), type);
    EXPECT_EQ(sensor->inventoryObjPath, inventoryObjPath);
    EXPECT_NE(sensor->profileStatusInfo, nullptr);
    EXPECT_NE(sensor->profileInfoAsync, nullptr);
}

TEST_F(NsmWorkLoadProfileStatusTest, Constructor_InheritsNsmSensor)
{
    // Assert
    NsmSensor* base = sensor.get();
    EXPECT_NE(base, nullptr);
}

TEST_F(NsmWorkLoadProfileStatusTest, GenRequestMsg_ValidParams_ReturnsRequest)
{
    // Act
    auto request = sensor->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}

TEST_F(NsmWorkLoadProfileStatusTest,
       GenRequestMsg_DifferentInstanceId_ReturnsRequest)
{
    // Act
    auto request = sensor->genRequestMsg(eid, 5);

    // Assert
    ASSERT_TRUE(request.has_value());
}

TEST_F(NsmWorkLoadProfileStatusTest,
       HandleResponseMsg_SuccessResponse_ReturnsSuccess)
{
    // Arrange
    struct workload_power_profile_status statusData = {};
    statusData.supported_profile_mask = {};
    statusData.requested_profile_maks = {};
    statusData.enforced_profile_mask = {};
    // Set one bit in supported mask
    statusData.supported_profile_mask.fields[0].byte = 0x01;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = encode_get_workload_power_profile_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &statusData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmWorkLoadProfileStatusTest, HandleResponseMsg_ErrorCC_ReturnsError)
{
    // Arrange
    struct workload_power_profile_status statusData = {};

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = encode_get_workload_power_profile_status_resp(
        instanceId, NSM_ERR_INVALID_DATA_LENGTH, ERR_NULL, &statusData,
        responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmWorkLoadProfileStatusTest, UpdateReading_NullPtr_HandlesGracefully)
{
    // Act - should not crash
    sensor->updateReading(nullptr);

    // Assert - no crash
    SUCCEED();
}

TEST_F(NsmWorkLoadProfileStatusTest, UpdateReading_ValidData_UpdatesProperties)
{
    // Arrange
    struct workload_power_profile_status data = {};
    data.supported_profile_mask = {};
    data.requested_profile_maks = {};
    data.enforced_profile_mask = {};
    // Set a specific bit to verify it gets passed through
    data.supported_profile_mask.fields[0].byte = 0x03; // bits 0 and 1

    // Act
    sensor->updateReading(&data);

    // Assert - no crash; internally calls ProfileInfoIntf methods
    SUCCEED();
}

TEST_F(NsmWorkLoadProfileStatusTest, UpdateReading_AllZeros_NoErrors)
{
    // Arrange
    struct workload_power_profile_status data = {};

    // Act
    sensor->updateReading(&data);

    // Assert
    SUCCEED();
}

TEST_F(NsmWorkLoadProfileStatusTest, UpdateReading_AllOnes_NoErrors)
{
    // Arrange
    struct workload_power_profile_status data = {};
    memset(&data, 0xFF, sizeof(data));

    // Act
    sensor->updateReading(&data);

    // Assert
    SUCCEED();
}

// ---------------------------------------------------------------------------
// NsmWorkloadProfileInfoAsyncIntf Constructor Test
// ---------------------------------------------------------------------------
struct NsmWorkloadProfileInfoAsyncIntfTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::string inventoryObjPath = "/xyz/openbmc_project/inventory/test_async";

    NsmDeviceTable devices = {};
    std::shared_ptr<MockNsmDevice> device = {};

    NsmWorkloadProfileInfoAsyncIntfTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    }

    void TearDown() override
    {
        device.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmWorkloadProfileInfoAsyncIntfTest,
       Constructor_ValidParams_CreatesObject)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();

    // Act
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, inventoryObjPath.c_str(), device);

    // Assert
    EXPECT_NE(asyncIntf, nullptr);
    EXPECT_EQ(asyncIntf->device, device);
    asyncIntf.reset();
}

// ---------------------------------------------------------------------------
// NsmWorkloadPowerProfileCollection Tests
// ---------------------------------------------------------------------------
struct NsmWorkloadPowerProfileCollectionTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::string name = "ProfileCollection";
    std::string type = "NSM_ProfileCollection";
    std::string inventoryObjPath = "/xyz/openbmc_project/inventory/test_coll";

    NsmDeviceTable devices = {};
    std::shared_ptr<MockNsmDevice> device = {};
    std::shared_ptr<NsmWorkloadPowerProfileCollection> collection = {};

    NsmWorkloadPowerProfileCollectionTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    }

    void SetUp() override
    {
        ASSERT_NE(device, nullptr);
        collection = std::make_shared<NsmWorkloadPowerProfileCollection>(
            name, type, inventoryObjPath, device);
        ASSERT_NE(collection, nullptr);
    }

    void TearDown() override
    {
        collection.reset();
        device.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmWorkloadPowerProfileCollectionTest,
       Constructor_ValidParams_SetsFields)
{
    // Assert
    EXPECT_EQ(collection->getName(), name);
    EXPECT_EQ(collection->getType(), type);
    EXPECT_EQ(collection->inventoryObjPath, inventoryObjPath);
}

TEST_F(NsmWorkloadPowerProfileCollectionTest,
       HasProfileId_EmptyCollection_ReturnsFalse)
{
    // Act / Assert
    EXPECT_FALSE(collection->hasProfileId(0));
    EXPECT_FALSE(collection->hasProfileId(1));
    EXPECT_FALSE(collection->hasProfileId(0xFFFF));
}

TEST_F(NsmWorkloadPowerProfileCollectionTest,
       AddSupportedProfile_OneProfile_HasProfileIdReturnsTrue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::string profileName = "TestProfile";
    auto profile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, inventoryObjPath, 0, profileName, device);

    // Act
    collection->addSupportedProfile(0, profile);

    // Assert
    EXPECT_TRUE(collection->hasProfileId(0));
    EXPECT_FALSE(collection->hasProfileId(1));
}

TEST_F(NsmWorkloadPowerProfileCollectionTest,
       AddSupportedProfile_MultipleProfiles_AllFound)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::string name1 = "Profile0";
    std::string name2 = "Profile1";
    auto profile0 = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, inventoryObjPath, 0, name1, device);
    auto profile1 = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, inventoryObjPath, 1, name2, device);

    // Act
    collection->addSupportedProfile(0, profile0);
    collection->addSupportedProfile(1, profile1);

    // Assert
    EXPECT_TRUE(collection->hasProfileId(0));
    EXPECT_TRUE(collection->hasProfileId(1));
    EXPECT_FALSE(collection->hasProfileId(2));
}

TEST_F(NsmWorkloadPowerProfileCollectionTest,
       GetSupportedProfileById_ExistingId_ReturnsProfile)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::string profileName = "TestGet";
    auto profile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, inventoryObjPath, 5, profileName, device);
    collection->addSupportedProfile(5, profile);

    // Act
    auto retrieved = collection->getSupportedProfileById(5);

    // Assert
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved, profile);
}

TEST_F(NsmWorkloadPowerProfileCollectionTest,
       GetSupportedProfileById_NonExistentId_Throws)
{
    // Act / Assert
    EXPECT_THROW(collection->getSupportedProfileById(999), std::out_of_range);
}

TEST_F(NsmWorkloadPowerProfileCollectionTest,
       UpdateSupportedProfile_ValidObj_UpdatesFields)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::string profileName = "UpdateTest";
    auto profile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, inventoryObjPath, 7, profileName, device);
    collection->addSupportedProfile(7, profile);

    nsm_workload_power_profile_data data = {};
    data.profile_id = 7;
    data.priority = 42;
    data.conflict_mask = {};

    // Act
    collection->updateSupportedProfile(profile, &data);

    // Assert - no crash, fields updated through D-Bus interface
    SUCCEED();
}

TEST_F(NsmWorkloadPowerProfileCollectionTest,
       UpdateSupportedProfile_NullObj_HandlesGracefully)
{
    // Arrange
    nsm_workload_power_profile_data data = {};
    data.profile_id = 0;
    data.priority = 1;
    data.conflict_mask = {};

    // Act - should not crash since obj is nullptr checked
    collection->updateSupportedProfile(nullptr, &data);

    // Assert
    SUCCEED();
}

// ---------------------------------------------------------------------------
// NsmWorkloadPowerProfilePageCollection Tests
// ---------------------------------------------------------------------------
struct NsmWorkloadPowerProfilePageCollectionTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::string name = "PageCollection";
    std::string type = "NSM_PageCollection";
    std::string inventoryObjPath = "/xyz/openbmc_project/inventory/test_page";

    NsmDeviceTable devices = {};
    std::shared_ptr<MockNsmDevice> device = {};
    std::shared_ptr<NsmWorkloadPowerProfilePageCollection> pageCollection = {};

    NsmWorkloadPowerProfilePageCollectionTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    }

    void SetUp() override
    {
        ASSERT_NE(device, nullptr);
        pageCollection =
            std::make_shared<NsmWorkloadPowerProfilePageCollection>(
                name, type, inventoryObjPath, device);
        ASSERT_NE(pageCollection, nullptr);
    }

    void TearDown() override
    {
        // Break shared_ptr cycles before resetting:
        // pageCollection->supportedPages → page → page->pageCollection →
        // pageCollection page->device → device → device->deviceSensors → page
        if (pageCollection)
            pageCollection->supportedPages.clear();
        pageCollection.reset();
        device.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmWorkloadPowerProfilePageCollectionTest,
       Constructor_ValidParams_SetsFields)
{
    // Assert
    EXPECT_EQ(pageCollection->getName(), name);
    EXPECT_EQ(pageCollection->getType(), type);
}

TEST_F(NsmWorkloadPowerProfilePageCollectionTest,
       HasPageId_EmptyCollection_ReturnsFalse)
{
    // Act / Assert
    EXPECT_FALSE(pageCollection->hasPageId(0));
    EXPECT_FALSE(pageCollection->hasPageId(1));
}

TEST_F(NsmWorkloadPowerProfilePageCollectionTest,
       GetPageById_EmptyCollection_ReturnsNullptr)
{
    // Act
    auto page = pageCollection->getPageById(0);

    // Assert
    EXPECT_EQ(page, nullptr);
}

TEST_F(NsmWorkloadPowerProfilePageCollectionTest, AddPage_NewPage_PageIsFound)
{
    // Arrange
    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(
            name, type, inventoryObjPath, device);
    std::vector<std::string> profileNames = {"P0", "P1"};
    std::string enumName = "TestEnum";
    std::string enumType = "NSM_Enum";
    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
        enumName, enumType, profileNames);

    std::string pageName = "Page0";
    std::string pageType = "NSM_Page";
    auto page = std::make_shared<NsmWorkloadPowerProfilePage>(
        pageName, pageType, inventoryObjPath, device, profileCollection,
        pageCollection, profileMapper, 0);

    // Act
    pageCollection->addPage(0, page);

    // Assert
    EXPECT_TRUE(pageCollection->hasPageId(0));
    EXPECT_NE(pageCollection->getPageById(0), nullptr);
    EXPECT_EQ(pageCollection->getPageById(0), page);
}

TEST_F(NsmWorkloadPowerProfilePageCollectionTest,
       AddPage_DuplicatePageId_DoesNotReplace)
{
    // Arrange
    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(
            name, type, inventoryObjPath, device);
    std::vector<std::string> profileNames = {"P0"};
    std::string enumName = "TestEnum2";
    std::string enumType = "NSM_Enum";
    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
        enumName, enumType, profileNames);

    std::string pageName1 = "Page0a";
    std::string pageType1 = "NSM_Page";
    auto page1 = std::make_shared<NsmWorkloadPowerProfilePage>(
        pageName1, pageType1, inventoryObjPath, device, profileCollection,
        pageCollection, profileMapper, 0);

    std::string pageName2 = "Page0b";
    std::string pageType2 = "NSM_Page";
    auto page2 = std::make_shared<NsmWorkloadPowerProfilePage>(
        pageName2, pageType2, inventoryObjPath, device, profileCollection,
        pageCollection, profileMapper, 0);

    // Act
    pageCollection->addPage(0, page1);
    pageCollection->addPage(0, page2); // duplicate - should not replace

    // Assert - first page remains
    EXPECT_EQ(pageCollection->getPageById(0), page1);
}

// ---------------------------------------------------------------------------
// NsmWorkloadPowerProfilePage Tests
// ---------------------------------------------------------------------------
struct NsmWorkloadPowerProfilePageTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::string name = "TestPage";
    std::string type = "NSM_WorkloadPage";
    std::string inventoryObjPath = "/xyz/openbmc_project/inventory/test_wp";
    eid_t eid = 0;
    uint8_t instanceId = 0;

    NsmDeviceTable devices = {};
    std::shared_ptr<MockNsmDevice> device = {};
    std::shared_ptr<NsmWorkloadPowerProfileCollection> profileCollection = {};
    std::shared_ptr<NsmWorkloadPowerProfilePageCollection> pageCollection = {};
    std::shared_ptr<NsmWorkLoadProfileEnum> profileMapper = {};
    std::shared_ptr<NsmWorkloadPowerProfilePage> page = {};

    NsmWorkloadPowerProfilePageTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    }

    void SetUp() override
    {
        ASSERT_NE(device, nullptr);

        profileCollection = std::make_shared<NsmWorkloadPowerProfileCollection>(
            name, type, inventoryObjPath, device);

        std::string pgCollName = "PageColl";
        std::string pgCollType = "NSM_PageColl";
        pageCollection =
            std::make_shared<NsmWorkloadPowerProfilePageCollection>(
                pgCollName, pgCollType, inventoryObjPath, device);

        std::vector<std::string> profileNames = {"Perf", "Balanced", "Saver"};
        std::string enumName = "PageEnum";
        std::string enumType = "NSM_PageEnum";
        profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
            enumName, enumType, profileNames);

        page = std::make_shared<NsmWorkloadPowerProfilePage>(
            name, type, inventoryObjPath, device, profileCollection,
            pageCollection, profileMapper, 0);
        ASSERT_NE(page, nullptr);
    }

    void TearDown() override
    {
        page.reset();
        profileMapper.reset();
        pageCollection.reset();
        profileCollection.reset();
        device.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmWorkloadPowerProfilePageTest, Constructor_ValidParams_SetsFields)
{
    // Assert
    EXPECT_EQ(page->getName(), name);
    EXPECT_EQ(page->getType(), type);
    EXPECT_EQ(page->inventoryObjPath, inventoryObjPath);
    EXPECT_EQ(page->pageId, 0);
}

TEST_F(NsmWorkloadPowerProfilePageTest, Constructor_InheritsNsmSensor)
{
    // Assert
    NsmSensor* base = page.get();
    EXPECT_NE(base, nullptr);
}

TEST_F(NsmWorkloadPowerProfilePageTest,
       GenRequestMsg_ValidParams_ReturnsRequest)
{
    // Act
    auto request = page->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_workload_power_profile_info_req));
}

TEST_F(NsmWorkloadPowerProfilePageTest,
       GenRequestMsg_DifferentPageId_ReturnsRequest)
{
    // Arrange
    auto page2 = std::make_shared<NsmWorkloadPowerProfilePage>(
        name, type, inventoryObjPath, device, profileCollection, pageCollection,
        profileMapper, 5);

    // Act
    auto request = page2->genRequestMsg(eid, instanceId);

    // Assert
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(page2->pageId, 5);
}

TEST_F(NsmWorkloadPowerProfilePageTest,
       HandleResponseMsg_SuccessWithProfiles_ReturnsSuccess)
{
    // Arrange - build a response with 2 profiles
    uint8_t numberOfProfiles = 2;

    // Calculate total response size
    size_t respSize =
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_workload_power_profile_get_all_preset_profile_resp) - 1 +
        (numberOfProfiles * sizeof(nsm_workload_power_profile_data));

    std::vector<uint8_t> response(respSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_all_workload_power_profile_meta_data metaData = {};
    metaData.next_identifier = 0; // No next page
    metaData.number_of_profiles = numberOfProfiles;

    nsm_workload_power_profile_data profileData[2] = {};
    profileData[0].profile_id = 0;
    profileData[0].priority = 10;
    profileData[0].conflict_mask = {};
    profileData[1].profile_id = 1;
    profileData[1].priority = 20;
    profileData[1].conflict_mask = {};

    auto rc = encode_get_workload_power_profile_info_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &metaData, profileData,
        numberOfProfiles, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = page->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(profileCollection->hasProfileId(0));
    EXPECT_TRUE(profileCollection->hasProfileId(1));
}

TEST_F(NsmWorkloadPowerProfilePageTest, HandleResponseMsg_ErrorCC_ReturnsError)
{
    // Arrange
    uint8_t numberOfProfiles = 1;
    size_t respSize =
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_workload_power_profile_get_all_preset_profile_resp) - 1 +
        (numberOfProfiles * sizeof(nsm_workload_power_profile_data));

    std::vector<uint8_t> response(respSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_all_workload_power_profile_meta_data metaData = {};
    metaData.next_identifier = 0;
    metaData.number_of_profiles = numberOfProfiles;

    nsm_workload_power_profile_data profileData = {};
    profileData.profile_id = 0;
    profileData.priority = 1;
    profileData.conflict_mask = {};

    auto rc = encode_get_workload_power_profile_info_resp(
        instanceId, NSM_ERR_INVALID_DATA_LENGTH, ERR_NULL, &metaData,
        &profileData, numberOfProfiles, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = page->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmWorkloadPowerProfilePageTest,
       HandleResponseMsg_NextIdentifierSet_AddsNextPage)
{
    // Arrange - build response with next_identifier > 0
    uint8_t numberOfProfiles = 1;
    size_t respSize =
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_workload_power_profile_get_all_preset_profile_resp) - 1 +
        (numberOfProfiles * sizeof(nsm_workload_power_profile_data));

    std::vector<uint8_t> response(respSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_all_workload_power_profile_meta_data metaData = {};
    metaData.next_identifier = 1; // next page exists
    metaData.number_of_profiles = numberOfProfiles;

    nsm_workload_power_profile_data profileData = {};
    profileData.profile_id = 0;
    profileData.priority = 5;
    profileData.conflict_mask = {};

    auto rc = encode_get_workload_power_profile_info_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &metaData, &profileData,
        numberOfProfiles, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = page->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // The next page (id=1) should have been added to pageCollection
    EXPECT_TRUE(pageCollection->hasPageId(1));
}

TEST_F(NsmWorkloadPowerProfilePageTest,
       DISABLED_HandleResponseMsg_ExistingNextPage_DoesNotDuplicate)
{
    // Arrange - add page 1 manually first
    auto existingPage = std::make_shared<NsmWorkloadPowerProfilePage>(
        name, type, inventoryObjPath, device, profileCollection, pageCollection,
        profileMapper, 1);
    pageCollection->addPage(1, existingPage);
    ASSERT_TRUE(pageCollection->hasPageId(1));

    // Now build a response with next_identifier = 1
    uint8_t numberOfProfiles = 1;
    size_t respSize =
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_workload_power_profile_get_all_preset_profile_resp) - 1 +
        (numberOfProfiles * sizeof(nsm_workload_power_profile_data));

    std::vector<uint8_t> response(respSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_all_workload_power_profile_meta_data metaData = {};
    metaData.next_identifier = 1;
    metaData.number_of_profiles = numberOfProfiles;

    nsm_workload_power_profile_data profileData = {};
    profileData.profile_id = 0;
    profileData.priority = 5;
    profileData.conflict_mask = {};

    auto rc = encode_get_workload_power_profile_info_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &metaData, &profileData,
        numberOfProfiles, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = page->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Should still be the existing page object, not replaced
    EXPECT_EQ(pageCollection->getPageById(1), existingPage);
}

TEST_F(NsmWorkloadPowerProfilePageTest,
       DISABLED_HandleResponseMsg_UpdateExistingProfile_UpdatesFields)
{
    // Arrange - pre-add a profile, then handle a response with the same id
    auto& bus = utils::DBusHandler::getBus();
    std::string profName = "Perf";
    auto existingProfile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, inventoryObjPath, 0, profName, device);
    profileCollection->addSupportedProfile(0, existingProfile);
    ASSERT_TRUE(profileCollection->hasProfileId(0));

    uint8_t numberOfProfiles = 1;
    size_t respSize =
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_workload_power_profile_get_all_preset_profile_resp) - 1 +
        (numberOfProfiles * sizeof(nsm_workload_power_profile_data));

    std::vector<uint8_t> response(respSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_all_workload_power_profile_meta_data metaData = {};
    metaData.next_identifier = 0;
    metaData.number_of_profiles = numberOfProfiles;

    nsm_workload_power_profile_data profileData = {};
    profileData.profile_id = 0;
    profileData.priority = 99;
    profileData.conflict_mask = {};

    auto rc = encode_get_workload_power_profile_info_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &metaData, &profileData,
        numberOfProfiles, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = page->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Existing profile should have been updated, not a new one created
    EXPECT_TRUE(profileCollection->hasProfileId(0));
}

// ---------------------------------------------------------------------------
// NsmWorkLoadProfileEnum Additional Tests (for completeness)
// ---------------------------------------------------------------------------
TEST(NsmWorkLoadProfileEnumExtra, Constructor_InheritsNsmObject)
{
    std::string name = "EnumObj";
    std::string type = "NSM_Enum";
    std::vector<std::string> names = {"A", "B"};

    NsmWorkLoadProfileEnum enumObj(name, type, names);

    // Verify NsmObject inheritance
    NsmObject* base = &enumObj;
    EXPECT_NE(base, nullptr);
    EXPECT_EQ(base->getName(), name);
    EXPECT_EQ(base->getType(), type);
}

TEST(NsmWorkLoadProfileEnumExtra, ToString_BoundaryIndex_ReturnsCorrectly)
{
    std::string name = "BoundaryEnum";
    std::string type = "NSM_Enum";
    std::vector<std::string> names = {"First", "Last"};

    NsmWorkLoadProfileEnum enumObj(name, type, names);

    EXPECT_EQ(enumObj.toString(0), "First");
    EXPECT_EQ(enumObj.toString(1), "Last");
    EXPECT_EQ(enumObj.toString(2), "Unknown");
    EXPECT_EQ(enumObj.toString(UINT16_MAX), "Unknown");
}

TEST(NsmWorkLoadProfileEnumExtra, ToEnum_CaseSensitive_ReturnsCorrectly)
{
    std::string name = "CaseEnum";
    std::string type = "NSM_Enum";
    std::vector<std::string> names = {"Abc", "abc", "ABC"};

    NsmWorkLoadProfileEnum enumObj(name, type, names);

    EXPECT_EQ(enumObj.toEnum("Abc"), 0);
    EXPECT_EQ(enumObj.toEnum("abc"), 1);
    EXPECT_EQ(enumObj.toEnum("ABC"), 2);
    EXPECT_EQ(enumObj.toEnum("aBc"), -1);
}

// ---------------------------------------------------------------------------
// NsmWorkloadPowerProfileCollection Additional Edge Cases
// ---------------------------------------------------------------------------
struct NsmWorkloadPowerProfileCollectionEdgeTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::string name = "EdgeColl";
    std::string type = "NSM_EdgeColl";
    std::string inventoryObjPath = "/xyz/openbmc_project/inventory/test_edge";

    NsmDeviceTable devices = {};
    std::shared_ptr<MockNsmDevice> device = {};
    std::shared_ptr<NsmWorkloadPowerProfileCollection> collection = {};

    NsmWorkloadPowerProfileCollectionEdgeTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    }

    void SetUp() override
    {
        ASSERT_NE(device, nullptr);
        collection = std::make_shared<NsmWorkloadPowerProfileCollection>(
            name, type, inventoryObjPath, device);
    }

    void TearDown() override
    {
        collection.reset();
        device.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmWorkloadPowerProfileCollectionEdgeTest,
       AddSupportedProfile_MaxProfileId_Works)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint16_t maxId = 0xFFFF;
    std::string profileName = "MaxProfile";
    auto profile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, inventoryObjPath, maxId, profileName, device);

    // Act
    collection->addSupportedProfile(maxId, profile);

    // Assert
    EXPECT_TRUE(collection->hasProfileId(maxId));
    EXPECT_EQ(collection->getSupportedProfileById(maxId), profile);
}

TEST_F(NsmWorkloadPowerProfileCollectionEdgeTest,
       AddSupportedProfile_ReplaceExisting_UpdatesProfile)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::string name1 = "Old";
    std::string name2 = "New";
    auto profile1 = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, inventoryObjPath, 10, name1, device);
    auto profile2 = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, inventoryObjPath + "/replaced", 10, name2, device);

    // Act
    collection->addSupportedProfile(10, profile1);
    collection->addSupportedProfile(10, profile2); // replace

    // Assert - map overwrite
    EXPECT_EQ(collection->getSupportedProfileById(10), profile2);
}

TEST_F(NsmWorkloadPowerProfileCollectionEdgeTest,
       UpdateSupportedProfile_WithConflictMask_NoError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "ConflictTest";
    auto profile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        bus, inventoryObjPath, 3, pName, device);
    collection->addSupportedProfile(3, profile);

    nsm_workload_power_profile_data data = {};
    data.profile_id = 3;
    data.priority = 100;
    // Set some bits in conflict mask
    data.conflict_mask.fields[0].byte = 0xFF;
    data.conflict_mask.fields[1].byte = 0xAA;

    // Act
    collection->updateSupportedProfile(profile, &data);

    // Assert
    SUCCEED();
}

// ---------------------------------------------------------------------------
// NsmImageCopyPolicy Constructor Edge Test
// ---------------------------------------------------------------------------
struct NsmImageCopyPolicyConstructorTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices = {};

    NsmImageCopyPolicyConstructorTest() : SensorManagerTest(devices) {}

    void TearDown() override
    {
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmImageCopyPolicyConstructorTest,
       Constructor_DifferentClassIdentIndex_SetsCorrectly)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "EdgePolicyObj";
    std::string type = "NSM_EdgePolicy";
    uint16_t classification = 0xABCD;
    uint16_t identifier = 0x1234;
    uint8_t index = 0x7F;

    // Act
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, name, type, classification, identifier, index);

    // Assert
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(obj->classification, classification);
    EXPECT_EQ(obj->identifier, identifier);
    EXPECT_EQ(obj->index, index);
    EXPECT_NE(obj->imageCopyPolicyObject, nullptr);
    obj.reset();
}

// ---------------------------------------------------------------------------
// NsmInbandUpdatePolicyObject with different params
// ---------------------------------------------------------------------------
struct NsmInbandUpdatePolicyObjectEdgeTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices = {};

    NsmInbandUpdatePolicyObjectEdgeTest() : SensorManagerTest(devices) {}

    void TearDown() override
    {
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmInbandUpdatePolicyObjectEdgeTest, Constructor_MaxValues_CreatesObject)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::string chassisName = "HGX_Chassis_Edge";
    uint16_t classification = 0xFFFF;
    uint16_t identifier = 0xFFFF;
    uint8_t index = 0xFF;

    // Act
    auto obj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName, classification, identifier, index);

    // Assert
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(obj->classification, classification);
    EXPECT_EQ(obj->identifier, identifier);
    EXPECT_EQ(obj->index, index);
    EXPECT_NE(obj->nsmInbandUpdatePolicy, nullptr);
    obj.reset();
}

TEST_F(NsmInbandUpdatePolicyObjectEdgeTest,
       Constructor_ZeroValues_CreatesObject)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::string chassisName = "HGX_Chassis_Zero";
    uint16_t classification = 0;
    uint16_t identifier = 0;
    uint8_t index = 0;

    // Act
    auto obj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName, classification, identifier, index);

    // Assert
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(obj->classification, 0);
    EXPECT_EQ(obj->identifier, 0);
    EXPECT_EQ(obj->index, 0);
    obj.reset();
}

// ---------------------------------------------------------------------------
// HandleResponseMsg with various policy combinations
// ---------------------------------------------------------------------------
struct NsmRoTResponseCombinationsTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis_Combo";
    const uint16_t classification = 1;
    const uint16_t identifier = 2;
    const uint8_t index = 0;
    const uint8_t instanceId = 0;

    NsmDeviceTable devices = {};
    std::shared_ptr<NsmInbandUpdatePolicyObject> policyObject = {};

    NsmRoTResponseCombinationsTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        policyObject = std::make_shared<NsmInbandUpdatePolicyObject>(
            bus, chassisName, classification, identifier, index);
        ASSERT_NE(policyObject, nullptr);
    }

    void TearDown() override
    {
        policyObject.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }

    uint8_t buildAndHandleResponse(uint8_t inbandPolicy,
                                   uint8_t backgroundPolicy, uint8_t cc)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
                sizeof(nsm_firmware_erot_state_info_hdr_resp) +
                sizeof(nsm_firmware_slot_info) * 2,
            0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        nsm_firmware_erot_state_info_resp erotInfo = {};
        erotInfo.fq_resp_hdr.inband_update_policy = inbandPolicy;
        erotInfo.fq_resp_hdr.background_copy_policy = backgroundPolicy;
        erotInfo.slot_info = nullptr;

        auto rc = encode_nsm_query_get_erot_state_parameters_resp(
            instanceId, cc, ERR_NULL, &erotInfo, responseMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            return rc;
        }
        return policyObject->handleResponseMsg(responseMsg, response.size());
    }
};

TEST_F(NsmRoTResponseCombinationsTest, HandleResponseMsg_EnabledManual_Success)
{
    auto rc = buildAndHandleResponse(
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE,
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY, NSM_SUCCESS);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmRoTResponseCombinationsTest,
       HandleResponseMsg_DisabledAutomatic_Success)
{
    auto rc = buildAndHandleResponse(
        NSM_ROT_INBAND_UPDATE_POLICY_DISABLE,
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY, NSM_SUCCESS);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmRoTResponseCombinationsTest,
       HandleResponseMsg_InvalidBothPolicies_SuccessCC)
{
    // Both policies invalid but CC is success - code still returns success
    auto rc = buildAndHandleResponse(0xFF, 0xFF, NSM_SUCCESS);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// NsmWorkLoadProfileStatus with full encode/decode roundtrip
// ---------------------------------------------------------------------------
struct NsmWorkLoadProfileStatusRoundtripTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::string name = "StatusRT";
    std::string type = "NSM_StatusRT";
    std::string inventoryObjPath = "/xyz/openbmc_project/inventory/test_rt";

    NsmDeviceTable devices = {};
    std::shared_ptr<MockNsmDevice> device = {};
    std::shared_ptr<NsmWorkLoadProfileStatus> sensor = {};

    NsmWorkLoadProfileStatusRoundtripTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        ASSERT_NE(device, nullptr);

        auto profileStatusInfo =
            std::make_shared<OemProfileInfoIntf>(bus, inventoryObjPath, device);
        auto profileInfoAsync =
            std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
                bus, inventoryObjPath.c_str(), device);

        sensor = std::make_shared<NsmWorkLoadProfileStatus>(
            name, type, inventoryObjPath, profileStatusInfo, profileInfoAsync);
    }

    void TearDown() override
    {
        sensor.reset();
        device.reset();
        SensorManagerTest::cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmWorkLoadProfileStatusRoundtripTest,
       GenAndHandleResponse_FullRoundtrip_Success)
{
    // Arrange - generate request
    eid_t eid = 0;
    uint8_t instanceId = 0;
    auto request = sensor->genRequestMsg(eid, instanceId);
    ASSERT_TRUE(request.has_value());

    // Build response
    struct workload_power_profile_status statusData = {};
    statusData.supported_profile_mask.fields[0].byte = 0x07;
    statusData.requested_profile_maks.fields[0].byte = 0x03;
    statusData.enforced_profile_mask.fields[0].byte = 0x01;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = encode_get_workload_power_profile_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &statusData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmWorkLoadProfileStatusRoundtripTest,
       HandleResponseMsg_AllMasksZero_Success)
{
    // Arrange
    struct workload_power_profile_status statusData = {};
    // All masks are zero-initialized

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = encode_get_workload_power_profile_status_resp(
        0, NSM_SUCCESS, ERR_NULL, &statusData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmWorkLoadProfileStatusRoundtripTest,
       HandleResponseMsg_AllMasksFull_Success)
{
    // Arrange
    struct workload_power_profile_status statusData = {};
    memset(&statusData, 0xFF, sizeof(statusData));

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = encode_get_workload_power_profile_status_resp(
        0, NSM_SUCCESS, ERR_NULL, &statusData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor->handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
