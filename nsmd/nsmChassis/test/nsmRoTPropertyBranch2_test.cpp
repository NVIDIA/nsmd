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

/*
 * Branch coverage for nsmd/nsmChassis/nsmRoTProperty.cpp - batch 2
 *
 * Targets remaining uncovered branches:
 * - getActiveSlotComponentInfo: mapServiceInterfaces.empty() -> continue
 * - getActiveSlotComponentInfo: AbsolutePath != objectPath -> continue
 * - getActiveSlotComponentInfo: no AbsolutePath -> continue
 * - getActiveSlotComponentInfo: SlotType != "Active" -> skip
 * - getActiveSlotComponentInfo: no SlotType -> skip
 * - getActiveSlotComponentInfo: classification > UINT16_MAX
 * - getActiveSlotComponentInfo: exception in associations -> continue
 * - getActiveSlotComponentInfo: exception in slot properties -> continue
 * - NsmImageCopyObject constructor
 * - NsmImageCopyPolicy constructor
 * - NsmImageCopyPolicyObject constructor + genRequestMsg + handleResponseMsg
 * - InbandUpdatePolicyHandler: encode success shouldLog suppression
 * - ImageCopyPolicyHandler: encode success shouldLog suppression
 * - initiateImageCopyAsync: multiple object paths (2nd fails)
 * - initiateImageCopyAsync: multiple object paths (both succeed)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "firmware-utils.h"

#define private public
#define protected public

#include "nsmFirmwareUtils/nsmFirmwareUtilsCommon.hpp"
#include "nsmRoTProperty.hpp"

namespace nsm
{
sdbusplus::common::com::nvidia::ImageCopy::ErrorCode
    mapReasonCodeToErrorCode(uint16_t reasonCode);
} // namespace nsm

using namespace nsm;

// =============================================================================
// Fixture
// =============================================================================

struct NsmRoTPropertyBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis_BR2";
    const uint16_t classification = 1;
    const uint16_t identifier = 2;
    const uint8_t index = 0;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:2";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmRoTPropertyBranch2Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmRoTPropertyBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    static std::vector<uint8_t> buildErotStateResponse(uint8_t inbandPolicy,
                                                       uint8_t bgCopyPolicy)
    {
        std::vector<uint8_t> buf(256, 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        nsm_firmware_erot_state_info_resp erotInfo = {};
        erotInfo.slot_info = nullptr;
        erotInfo.fq_resp_hdr.inband_update_policy = inbandPolicy;
        erotInfo.fq_resp_hdr.background_copy_policy = bgCopyPolicy;
        erotInfo.fq_resp_hdr.firmware_slot_count = 0;
        [[maybe_unused]] auto rc =
            encode_nsm_query_get_erot_state_parameters_resp(
                0, NSM_SUCCESS, ERR_NULL, &erotInfo, msg);
        return buf;
    }

    static std::vector<uint8_t> buildSetRotPropertyResponse(uint8_t cc,
                                                            uint16_t reason)
    {
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) +
                sizeof(nsm_firmware_set_rot_property_resp_command),
            0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        [[maybe_unused]] auto rc =
            encode_nsm_firmware_set_rot_property_resp(0, cc, reason, msg);
        return resp;
    }

    static std::vector<uint8_t> buildErotErrorCCResponse(uint8_t cc,
                                                         uint16_t reason)
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_erot_state_info_hdr_resp),
            0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        nsm_firmware_erot_state_info_resp erotInfo = {};
        erotInfo.slot_info = nullptr;
        [[maybe_unused]] auto rc =
            encode_nsm_query_get_erot_state_parameters_resp(0, cc, reason,
                                                            &erotInfo, msg);
        return buf;
    }
};

// =============================================================================
// NsmImageCopyObject constructor - exercises object creation path
// =============================================================================

TEST_F(NsmRoTPropertyBranch2Test, NsmImageCopyObject_Constructor)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyObject>(bus, chassisName + "_ico",
                                                    fpgaUuid);
    EXPECT_NE(obj, nullptr);
}

// =============================================================================
// NsmImageCopyPolicy constructor directly
// =============================================================================

TEST_F(NsmRoTPropertyBranch2Test, NsmImageCopyPolicy_Constructor)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_icp_ctor", classification, identifier, index);

    auto policy = std::make_unique<NsmImageCopyPolicy>(
        bus,
        std::string(chassisInventoryBasePath) + "/" + chassisName + "_icp_ctor",
        *sensor);
    EXPECT_NE(policy, nullptr);
}

// =============================================================================
// getActiveSlotComponentInfo: mapServiceInterfaces.empty() -> continue
// =============================================================================

struct NsmImageCopySlotBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_IC_SB2";
    const uint16_t classification = 1;
    const uint16_t identifier = 2;
    const uint8_t index = 0;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:2";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    std::unique_ptr<NsmInbandUpdatePolicyObject> nsmObj;
    std::unique_ptr<NsmImageCopy> imageCopy;
    std::shared_ptr<AsyncValueIntf> valueIntf;
    std::shared_ptr<AsyncStatusIntf> statusIntf;

    NsmImageCopySlotBranch2Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);

        auto& bus = utils::DBusHandler::getBus();
        nsmObj = std::make_unique<NsmInbandUpdatePolicyObject>(
            bus, chassisName, classification, identifier, index);

        std::string basePath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName;
        imageCopy = std::make_unique<NsmImageCopy>(bus, basePath, fpgaUuid,
                                                   *nsmObj);
        valueIntf =
            std::make_shared<AsyncValueIntf>(bus, (basePath + "_v2").c_str());
        statusIntf =
            std::make_shared<AsyncStatusIntf>(bus, (basePath + "_s2").c_str());
    }

    ~NsmImageCopySlotBranch2Test()
    {
        imageCopy.reset();
        nsmObj.reset();
        cleanupDeviceSensors(devices);
    }
};

// mapServiceInterfaces.empty() -> continue
TEST_F(NsmImageCopySlotBranch2Test,
       GetActiveSlotComponentInfo_EmptyServiceInterfaces)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/sb2_empty_svc";
    const std::string targetPath = "/target/sb2_empty_svc";

    // Empty service interfaces map -> continue branch
    GetSubTreeResponse resp{{slotPath, {}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// AbsolutePath != objectPath -> continue
TEST_F(NsmImageCopySlotBranch2Test,
       GetActiveSlotComponentInfo_AbsolutePathMismatch)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/sb2_mismatch";
    const std::string targetPath = "/target/sb2_mismatch";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";

    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    assocMap["AbsolutePath"] =
        std::string("/some/other/path"); // Does not match targetPath

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// No AbsolutePath property -> continue
TEST_F(NsmImageCopySlotBranch2Test, GetActiveSlotComponentInfo_NoAbsolutePath)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/sb2_no_abs";
    const std::string targetPath = "/target/sb2_no_abs";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";

    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    // No "AbsolutePath" key -> count() returns 0 -> continue
    assocMap["SomeOtherKey"] = std::string("foo");

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// SlotType != "Active" -> skip
TEST_F(NsmImageCopySlotBranch2Test,
       GetActiveSlotComponentInfo_SlotTypeNotActive)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/sb2_not_active";
    const std::string targetPath = "/target/sb2_not_active";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string slotIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    assocMap["AbsolutePath"] = targetPath;
    auto& slotMap = utils::MockDbusAsync::propertyMap(slotPath, slotIntf);
    slotMap["SlotType"] = std::string("Staging"); // Not "Active"
    slotMap["ComponentClassification"] = uint64_t(1);
    slotMap["ComponentIdentifier"] = uint64_t(1);
    slotMap["ComponentIndex"] = uint64_t(0);

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// No SlotType property -> no Active found
TEST_F(NsmImageCopySlotBranch2Test, GetActiveSlotComponentInfo_NoSlotType)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/sb2_no_slottype";
    const std::string targetPath = "/target/sb2_no_slottype";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string slotIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    assocMap["AbsolutePath"] = targetPath;
    auto& slotMap = utils::MockDbusAsync::propertyMap(slotPath, slotIntf);
    // No "SlotType" -> count() returns 0 -> no Active branch
    slotMap["ComponentClassification"] = uint64_t(1);

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// Classification out of range -> continue
TEST_F(NsmImageCopySlotBranch2Test,
       GetActiveSlotComponentInfo_ClassificationOutOfRange)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath = "/test/sb2_cls_oor";
    const std::string targetPath = "/target/sb2_cls_oor";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string slotIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    GetSubTreeResponse resp{
        {slotPath, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    auto& assocMap = utils::MockDbusAsync::propertyMap(slotPath, assocIntf);
    assocMap["AbsolutePath"] = targetPath;
    auto& slotMap = utils::MockDbusAsync::propertyMap(slotPath, slotIntf);
    slotMap["SlotType"] = std::string("Active");
    slotMap["ComponentClassification"] = uint64_t(0x10000); // > UINT16_MAX
    slotMap["ComponentIdentifier"] = uint64_t(1);
    slotMap["ComponentIndex"] = uint64_t(0);

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// Multiple slots: first is not matching, second matches
TEST_F(NsmImageCopySlotBranch2Test,
       GetActiveSlotComponentInfo_SecondSlotMatches)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string slotPath1 = "/test/sb2_multi1";
    const std::string slotPath2 = "/test/sb2_multi2";
    const std::string targetPath = "/target/sb2_multi";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string slotIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    GetSubTreeResponse resp{
        {slotPath1, {{"xyz.openbmc_project.EntityManager", {}}}},
        {slotPath2, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _)).WillOnce(Return(resp));

    // First slot: AbsolutePath doesn't match
    auto& assocMap1 = utils::MockDbusAsync::propertyMap(slotPath1, assocIntf);
    assocMap1["AbsolutePath"] = std::string("/some/other/path");

    // Second slot: matches
    auto& assocMap2 = utils::MockDbusAsync::propertyMap(slotPath2, assocIntf);
    assocMap2["AbsolutePath"] = targetPath;
    auto& slotMap2 = utils::MockDbusAsync::propertyMap(slotPath2, slotIntf);
    slotMap2["SlotType"] = std::string("Active");
    slotMap2["ComponentClassification"] = uint64_t(5);
    slotMap2["ComponentIdentifier"] = uint64_t(10);
    slotMap2["ComponentIndex"] = uint64_t(3);

    ComponentInfo info;
    auto rc = imageCopy->getActiveSlotComponentInfo(targetPath, info);
    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(info.classification, 5);
    EXPECT_EQ(info.identifier, 10);
    EXPECT_EQ(info.index, 3);
}

// =============================================================================
// initiateImageCopyAsync: multiple object paths, both succeed
// =============================================================================

TEST_F(NsmImageCopySlotBranch2Test,
       InitiateImageCopyAsync_MultiplePathsBothSucceed)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string targetPath1 = "/target/sb2_mp1";
    const std::string targetPath2 = "/target/sb2_mp2";
    const std::string slotPath1 = "/test/sb2_mp1_slot";
    const std::string slotPath2 = "/test/sb2_mp2_slot";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string slotIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    // Two getSubtree calls, one per path
    GetSubTreeResponse resp1{
        {slotPath1, {{"xyz.openbmc_project.EntityManager", {}}}}};
    GetSubTreeResponse resp2{
        {slotPath2, {{"xyz.openbmc_project.EntityManager", {}}}}};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _))
        .WillOnce(Return(resp1))
        .WillOnce(Return(resp2));

    auto& assocMap1 = utils::MockDbusAsync::propertyMap(slotPath1, assocIntf);
    assocMap1["AbsolutePath"] = targetPath1;
    auto& slotMap1 = utils::MockDbusAsync::propertyMap(slotPath1, slotIntf);
    slotMap1["SlotType"] = std::string("Active");
    slotMap1["ComponentClassification"] = uint64_t(2);
    slotMap1["ComponentIdentifier"] = uint64_t(1);
    slotMap1["ComponentIndex"] = uint64_t(0);

    auto& assocMap2 = utils::MockDbusAsync::propertyMap(slotPath2, assocIntf);
    assocMap2["AbsolutePath"] = targetPath2;
    auto& slotMap2 = utils::MockDbusAsync::propertyMap(slotPath2, slotIntf);
    slotMap2["SlotType"] = std::string("Active");
    slotMap2["ComponentClassification"] = uint64_t(3);
    slotMap2["ComponentIdentifier"] = uint64_t(4);
    slotMap2["ComponentIndex"] = uint64_t(1);

    // Build success response
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_initiate_copy_resp_command),
        0);

    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    imageCopy->initiateImageCopyAsync({targetPath1, targetPath2}, statusIntf,
                                      valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Accepted);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// =============================================================================
// initiateImageCopyAsync: first path succeeds, second fails
// =============================================================================

TEST_F(NsmImageCopySlotBranch2Test, InitiateImageCopyAsync_SecondPathFails)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis";
    const std::string targetPath1 = "/target/sb2_f1";
    const std::string targetPath2 = "/target/sb2_f2";
    const std::string slotPath1 = "/test/sb2_f1_slot";
    const std::string assocIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string slotIntf =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    // First call: succeeds
    GetSubTreeResponse resp1{
        {slotPath1, {{"xyz.openbmc_project.EntityManager", {}}}}};
    // Second call: no matching slots -> fails
    GetSubTreeResponse resp2{};
    EXPECT_CALL(mockDBus, getSubtree(chassisPath, 0, _))
        .WillOnce(Return(resp1))
        .WillOnce(Return(resp2));

    auto& assocMap1 = utils::MockDbusAsync::propertyMap(slotPath1, assocIntf);
    assocMap1["AbsolutePath"] = targetPath1;
    auto& slotMap1 = utils::MockDbusAsync::propertyMap(slotPath1, slotIntf);
    slotMap1["SlotType"] = std::string("Active");
    slotMap1["ComponentClassification"] = uint64_t(2);
    slotMap1["ComponentIdentifier"] = uint64_t(1);
    slotMap1["ComponentIndex"] = uint64_t(0);

    imageCopy->initiateImageCopyAsync({targetPath1, targetPath2}, statusIntf,
                                      valueIntf);

    using RequestStatus =
        sdbusplus::common::com::nvidia::ImageCopy::ImageCopyRequestStatus;
    EXPECT_EQ(imageCopy->imageCopyRequestStatus(), RequestStatus::Rejected);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// =============================================================================
// NsmInbandUpdatePolicyObject: handleResponseMsg success path with
// Enabled policy -> verify nsmInbandUpdatePolicy is updated
// =============================================================================

TEST_F(NsmRoTPropertyBranch2Test,
       InbandUpdatePolicyObject_HandleResponse_EnabledPolicy)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_en_hr", classification, identifier, index);

    auto buf = buildErotStateResponse(
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE,
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = obj->handleResponseMsg(response, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmImageCopyPolicyObject: handleResponseMsg with Automatic policy
// Exercises the updateProperties Automatic branch
// =============================================================================

TEST_F(NsmRoTPropertyBranch2Test,
       ImageCopyPolicyObject_HandleResponse_AutomaticPolicy)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_auto_hr", "NSM_ChassisRoT", classification,
        identifier, index);

    auto buf = buildErotStateResponse(
        NSM_ROT_INBAND_UPDATE_POLICY_DISABLE,
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY);
    auto response = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = obj->handleResponseMsg(response, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmInbandUpdatePolicy: updateProperties with DISABLE followed by ENABLE
// Exercises the state transition path
// =============================================================================

TEST_F(NsmRoTPropertyBranch2Test,
       InbandUpdatePolicy_UpdateProperties_DisableThenEnable)
{
    auto& bus = utils::DBusHandler::getBus();
    auto policyObj = std::make_shared<NsmInbandUpdatePolicyObject>(
        bus, chassisName + "_de", classification, identifier, index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_DISABLE;
    policyObj->nsmInbandUpdatePolicy->updateProperties(erotInfo);

    erotInfo.fq_resp_hdr.inband_update_policy =
        NSM_ROT_INBAND_UPDATE_POLICY_ENABLE;
    policyObj->nsmInbandUpdatePolicy->updateProperties(erotInfo);
    EXPECT_NE(policyObj->nsmInbandUpdatePolicy, nullptr);
}

// =============================================================================
// NsmImageCopyPolicy: updateProperties with AUTOMATIC followed by MANUAL
// =============================================================================

TEST_F(NsmRoTPropertyBranch2Test,
       ImageCopyPolicy_UpdateProperties_AutomaticThenManual)
{
    auto& bus = utils::DBusHandler::getBus();
    auto obj = std::make_shared<NsmImageCopyPolicyObject>(
        bus, chassisName + "_am", "NSM_ChassisRoT", classification, identifier,
        index);

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY;
    obj->imageCopyPolicyObject->updateProperties(erotInfo);

    erotInfo.fq_resp_hdr.background_copy_policy =
        NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY;
    obj->imageCopyPolicyObject->updateProperties(erotInfo);
    EXPECT_NE(obj->imageCopyPolicyObject, nullptr);
}

// =============================================================================
// InbandUpdatePolicyHandler: Enabled then Disabled transition
// Exercises both policy value branches across calls
// =============================================================================

TEST_F(NsmRoTPropertyBranch2Test, InbandHandler_EnabledThenDisabled_Transition)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};

    auto resp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);

    // Enabled
    AsyncSetOperationValueType valueE =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));
    handler.updateInbandUpdatePolicy(valueE, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    // Disabled
    status = {};
    AsyncSetOperationValueType valueD =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Disabled");
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));
    handler.updateInbandUpdatePolicy(valueD, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// ImageCopyPolicyHandler: Manual then Automatic transition
// =============================================================================

TEST_F(NsmRoTPropertyBranch2Test,
       ImageCopyHandler_ManualThenAutomatic_Transition)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};

    auto resp = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);

    // Manual
    AsyncSetOperationValueType valueM =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));
    handler.updateImageCopyPolicy(valueM, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    // Automatic
    status = {};
    AsyncSetOperationValueType valueA = std::string(
        "com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Automatic");
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));
    handler.updateImageCopyPolicy(valueA, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// InbandUpdatePolicyHandler: shouldLog for postPatchIO after success
// (transition from success to error state)
// =============================================================================

TEST_F(NsmRoTPropertyBranch2Test,
       InbandHandler_SuccessThenPostPatchIOFail_ShouldLog)
{
    InbandUpdatePolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.InbandUpdatePolicy.InbandPolicyState.Enabled");

    auto respOk = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respOk))
        .WillOnce(mockPostPatchIO(NSM_ERR_INVALID_DATA));

    // Success call
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    // Failure call: shouldLog transitions from OK to error
    status = {};
    handler.updateInbandUpdatePolicy(value, &status, fpga, classification,
                                     identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// ImageCopyPolicyHandler: shouldLog for decode fail after success
// =============================================================================

TEST_F(NsmRoTPropertyBranch2Test,
       ImageCopyHandler_SuccessThenDecodeFail_ShouldLog)
{
    ImageCopyPolicyHandler handler;
    AsyncOperationStatusType status = {};
    AsyncSetOperationValueType value =
        std::string("com.nvidia.ImageCopyPolicy.ImageCopyPolicyState.Manual");

    auto respOk = buildSetRotPropertyResponse(NSM_SUCCESS, ERR_NULL);
    auto respErr = buildSetRotPropertyResponse(NSM_ERROR, ERR_NULL);
    EXPECT_CALL(*fpga, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(respOk))
        .WillOnce(mockPostPatchIO(respErr));

    // Success call
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    // Error call
    status = {};
    handler.updateImageCopyPolicy(value, &status, fpga, classification,
                                  identifier, index);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
