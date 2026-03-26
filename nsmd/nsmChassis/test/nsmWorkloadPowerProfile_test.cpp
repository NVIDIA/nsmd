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

#include "base.h"
#include "device-configuration.h"
#include "diagnostics.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmWorkloadPowerProfile.hpp"

#undef private
#undef protected

using namespace nsm;

static auto busWPP = sdbusplus::bus::new_default();
static std::string sNameWPP("dummy_sensor_wpp");
static std::string sTypeWPP("dummy_type_wpp");
static std::string
    invPathWPP("/xyz/openbmc_project/inventory/dummy_device_wpp");

// =============================================================================
// NsmWorkLoadProfileStatus tests
// =============================================================================

TEST(NsmWorkLoadProfileStatus, GenRequestMsg_Success)
{
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(busWPP, invPathWPP, nullptr);
    auto profileInfoAsync = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        busWPP, invPathWPP.c_str(), nullptr);

    NsmWorkLoadProfileStatus sensor(sNameWPP, sTypeWPP, invPathWPP,
                                    profileStatusInfo, profileInfoAsync);

    const uint8_t eid{10};
    const uint8_t instanceId{5};

    auto request = sensor.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_WORKLOAD_POWER_PROFILE_STATUS_INFO);
    EXPECT_EQ(command->data_size, 0);
}

TEST(NsmWorkLoadProfileStatus, HandleResponseMsg_Success)
{
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(busWPP, invPathWPP, nullptr);
    auto profileInfoAsync = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        busWPP, invPathWPP.c_str(), nullptr);

    NsmWorkLoadProfileStatus sensor(sNameWPP, sTypeWPP, invPathWPP,
                                    profileStatusInfo, profileInfoAsync);

    struct workload_power_profile_status data = {};
    data.supported_profile_mask.fields[0].byte = 0x01;
    data.requested_profile_maks.fields[0].byte = 0x02;
    data.enforced_profile_mask.fields[0].byte = 0x03;

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_workload_power_profile_status_resp(
        0, NSM_SUCCESS, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmWorkLoadProfileStatus, HandleResponseMsg_BadNull)
{
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(busWPP, invPathWPP, nullptr);
    auto profileInfoAsync = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        busWPP, invPathWPP.c_str(), nullptr);

    NsmWorkLoadProfileStatus sensor(sNameWPP, sTypeWPP, invPathWPP,
                                    profileStatusInfo, profileInfoAsync);

    struct workload_power_profile_status data = {};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_workload_power_profile_status_resp(
        0, NSM_SUCCESS, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    // cc initialized to NSM_ERROR(1); return cc?cc:rc returns cc not rc
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_ERROR);
    rc = sensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(NsmWorkLoadProfileStatus, HandleResponseMsg_ErrorCC)
{
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(busWPP, invPathWPP, nullptr);
    auto profileInfoAsync = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        busWPP, invPathWPP.c_str(), nullptr);

    NsmWorkLoadProfileStatus sensor(sNameWPP, sTypeWPP, invPathWPP,
                                    profileStatusInfo, profileInfoAsync);

    // Build a full-sized response with error CC
    struct workload_power_profile_status data = {};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t reason_code = ERR_NULL;

    // First encode a success response to get correct structure
    uint8_t rc = encode_get_workload_power_profile_status_resp(
        0, NSM_ERROR, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    // Should return NSM_ERROR since cc != NSM_SUCCESS
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmWorkLoadProfileStatus, UpdateReading_ValidData)
{
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(busWPP, invPathWPP, nullptr);
    auto profileInfoAsync = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        busWPP, invPathWPP.c_str(), nullptr);

    NsmWorkLoadProfileStatus sensor(sNameWPP, sTypeWPP, invPathWPP,
                                    profileStatusInfo, profileInfoAsync);

    struct workload_power_profile_status data = {};
    data.supported_profile_mask.fields[0].byte = 0xFF;
    data.requested_profile_maks.fields[0].byte = 0x0A;
    data.enforced_profile_mask.fields[0].byte = 0x05;

    sensor.updateReading(&data);

    // Verify the profile status info was updated
    auto supportedMask =
        profileStatusInfo->ProfileInfoIntf::supportedProfileMask();
    auto requestedMask =
        profileStatusInfo->ProfileInfoIntf::requestedProfileMask();
    auto enforcedMask =
        profileStatusInfo->ProfileInfoIntf::enforcedProfileMask();

    EXPECT_FALSE(supportedMask.empty());
    EXPECT_FALSE(requestedMask.empty());
    EXPECT_FALSE(enforcedMask.empty());
}

TEST(NsmWorkLoadProfileStatus, UpdateReading_NullData)
{
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(busWPP, invPathWPP, nullptr);
    auto profileInfoAsync = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        busWPP, invPathWPP.c_str(), nullptr);

    NsmWorkLoadProfileStatus sensor(sNameWPP, sTypeWPP, invPathWPP,
                                    profileStatusInfo, profileInfoAsync);

    // Should not crash with null data
    sensor.updateReading(nullptr);
}

// =============================================================================
// NsmWorkloadPowerProfileCollection tests
// =============================================================================

TEST(NsmWorkloadPowerProfileCollection, HasProfileId_NotFound)
{
    NsmWorkloadPowerProfileCollection collection(sNameWPP, sTypeWPP, invPathWPP,
                                                 nullptr);
    EXPECT_FALSE(collection.hasProfileId(0));
    EXPECT_FALSE(collection.hasProfileId(999));
}

TEST(NsmWorkloadPowerProfileCollection, AddAndHasProfileId)
{
    NsmWorkloadPowerProfileCollection collection(sNameWPP, sTypeWPP, invPathWPP,
                                                 nullptr);

    std::string profileName = "TestProfile";
    auto profile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        busWPP, invPathWPP, 1, profileName, nullptr);

    collection.addSupportedProfile(1, profile);
    EXPECT_TRUE(collection.hasProfileId(1));
    EXPECT_FALSE(collection.hasProfileId(2));
}

TEST(NsmWorkloadPowerProfileCollection, GetSupportedProfileById_Found)
{
    NsmWorkloadPowerProfileCollection collection(sNameWPP, sTypeWPP, invPathWPP,
                                                 nullptr);

    std::string profileName = "TestProfile";
    auto profile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        busWPP, invPathWPP, 42, profileName, nullptr);

    collection.addSupportedProfile(42, profile);
    auto result = collection.getSupportedProfileById(42);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result.get(), profile.get());
}

TEST(NsmWorkloadPowerProfileCollection, GetSupportedProfileById_NotFound)
{
    NsmWorkloadPowerProfileCollection collection(sNameWPP, sTypeWPP, invPathWPP,
                                                 nullptr);
    EXPECT_THROW(collection.getSupportedProfileById(999), std::out_of_range);
}

TEST(NsmWorkloadPowerProfileCollection, UpdateSupportedProfile_Valid)
{
    NsmWorkloadPowerProfileCollection collection(sNameWPP, sTypeWPP, invPathWPP,
                                                 nullptr);

    std::string profileName = "TestProfile";
    auto profile = std::make_shared<OemWorkLoadPowerProfileIntf>(
        busWPP, invPathWPP, 5, profileName, nullptr);

    collection.addSupportedProfile(5, profile);

    nsm_workload_power_profile_data data = {};
    data.profile_id = 5;
    data.priority = 10;
    data.conflict_mask.fields[0].byte = 0x0F;

    collection.updateSupportedProfile(profile, &data);

    EXPECT_EQ(profile->WorkLoadPowerProfileIntf::profileId(), 5);
    EXPECT_EQ(profile->WorkLoadPowerProfileIntf::priority(), 10);
}

TEST(NsmWorkloadPowerProfileCollection, UpdateSupportedProfile_NullObj)
{
    NsmWorkloadPowerProfileCollection collection(sNameWPP, sTypeWPP, invPathWPP,
                                                 nullptr);
    nsm_workload_power_profile_data data = {};
    // Should not crash with null obj
    collection.updateSupportedProfile(nullptr, &data);
}

// =============================================================================
// NsmWorkloadPowerProfilePageCollection tests
// =============================================================================

TEST(NsmWorkloadPowerProfilePageCollection, HasPageId_NotFound)
{
    NsmWorkloadPowerProfilePageCollection pageCollection(sNameWPP, sTypeWPP,
                                                         invPathWPP, nullptr);
    EXPECT_FALSE(pageCollection.hasPageId(0));
    EXPECT_FALSE(pageCollection.hasPageId(999));
}

TEST(NsmWorkloadPowerProfilePageCollection, GetPageById_NotFound)
{
    NsmWorkloadPowerProfilePageCollection pageCollection(sNameWPP, sTypeWPP,
                                                         invPathWPP, nullptr);
    auto result = pageCollection.getPageById(0);
    EXPECT_EQ(result, nullptr);
}

TEST(NsmWorkloadPowerProfilePageCollection, AddPage_NewPage)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);

    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(sNameWPP, sTypeWPP,
                                                            invPathWPP, device);
    NsmWorkloadPowerProfilePageCollection pageCollection(sNameWPP, sTypeWPP,
                                                         invPathWPP, device);

    std::vector<std::string> profileNames = {"Profile0", "Profile1"};
    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
        sNameWPP, sTypeWPP, profileNames);

    auto page = std::make_shared<NsmWorkloadPowerProfilePage>(
        sNameWPP, sTypeWPP, invPathWPP, device, profileCollection,
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            sNameWPP, sTypeWPP, invPathWPP, device),
        profileMapper, 0);

    pageCollection.addPage(0, page);
    EXPECT_TRUE(pageCollection.hasPageId(0));
    EXPECT_NE(pageCollection.getPageById(0), nullptr);
    // Break shared_ptr cycles: device->deviceSensors and device->sensors
    // queues both hold page which holds device
    pageCollection.supportedPages.clear();
    device->deviceSensors.clear();
    for (auto& [type, queue] : device->sensors)
        queue.clear();
}

TEST(NsmWorkloadPowerProfilePageCollection, AddPage_DuplicatePage)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);

    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(sNameWPP, sTypeWPP,
                                                            invPathWPP, device);
    NsmWorkloadPowerProfilePageCollection pageCollection(sNameWPP, sTypeWPP,
                                                         invPathWPP, device);

    std::vector<std::string> profileNames = {"P0"};
    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
        sNameWPP, sTypeWPP, profileNames);

    auto page1 = std::make_shared<NsmWorkloadPowerProfilePage>(
        sNameWPP, sTypeWPP, invPathWPP, device, profileCollection,
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            sNameWPP, sTypeWPP, invPathWPP, device),
        profileMapper, 0);

    auto page2 = std::make_shared<NsmWorkloadPowerProfilePage>(
        sNameWPP, sTypeWPP, invPathWPP, device, profileCollection,
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            sNameWPP, sTypeWPP, invPathWPP, device),
        profileMapper, 0);

    pageCollection.addPage(0, page1);
    // Adding duplicate should not replace
    pageCollection.addPage(0, page2);
    EXPECT_EQ(pageCollection.getPageById(0).get(), page1.get());
    // Break shared_ptr cycles: device->deviceSensors and device->sensors
    // queues both hold page which holds device
    pageCollection.supportedPages.clear();
    device->deviceSensors.clear();
    for (auto& [type, queue] : device->sensors)
        queue.clear();
}

// =============================================================================
// NsmWorkloadPowerProfilePage tests
// =============================================================================

TEST(NsmWorkloadPowerProfilePage, GenRequestMsg_Success)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);

    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(sNameWPP, sTypeWPP,
                                                            invPathWPP, device);
    auto pageCollection =
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            sNameWPP, sTypeWPP, invPathWPP, device);

    std::vector<std::string> profileNames = {"Profile0", "Profile1"};
    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
        sNameWPP, sTypeWPP, profileNames);

    uint16_t pageId = 0;
    NsmWorkloadPowerProfilePage sensor(sNameWPP, sTypeWPP, invPathWPP, device,
                                       profileCollection, pageCollection,
                                       profileMapper, pageId);

    const uint8_t eid{10};
    const uint8_t instanceId{5};

    auto request = sensor.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_get_workload_power_profile_info_req*>(
            msg->payload);
    EXPECT_EQ(command->hdr.command, NSM_GET_WORKLOAD_POWER_PROFILE_INFO);
    EXPECT_EQ(command->identifier, pageId);
}

TEST(NsmWorkloadPowerProfilePage, HandleResponseMsg_Success)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);

    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(sNameWPP, sTypeWPP,
                                                            invPathWPP, device);
    auto pageCollection =
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            sNameWPP, sTypeWPP, invPathWPP, device);

    std::vector<std::string> profileNames = {"Profile0", "Profile1",
                                             "Profile2"};
    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
        sNameWPP, sTypeWPP, profileNames);

    uint16_t pageId = 0;
    NsmWorkloadPowerProfilePage sensor(sNameWPP, sTypeWPP, invPathWPP, device,
                                       profileCollection, pageCollection,
                                       profileMapper, pageId);

    // Build response with 1 profile, next_identifier = 0 (no more pages)
    struct nsm_all_workload_power_profile_meta_data metaData = {};
    metaData.next_identifier = 0;
    metaData.number_of_profiles = 1;

    struct nsm_workload_power_profile_data profileData = {};
    profileData.profile_id = 0;
    profileData.priority = 5;
    profileData.conflict_mask.fields[0].byte = 0x00;

    uint8_t numProfiles = 1;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
            sizeof(nsm_all_workload_power_profile_resp_meta_data) - 1 +
            numProfiles * sizeof(nsm_workload_power_profile_data),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_workload_power_profile_info_resp(
        0, NSM_SUCCESS, reason_code, &metaData, &profileData, numProfiles,
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Verify profile was added to collection
    EXPECT_TRUE(profileCollection->hasProfileId(0));
}

TEST(NsmWorkloadPowerProfilePage, HandleResponseMsg_BadNull)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);

    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(sNameWPP, sTypeWPP,
                                                            invPathWPP, device);
    auto pageCollection =
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            sNameWPP, sTypeWPP, invPathWPP, device);

    std::vector<std::string> profileNames = {"Profile0"};
    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
        sNameWPP, sTypeWPP, profileNames);

    NsmWorkloadPowerProfilePage sensor(sNameWPP, sTypeWPP, invPathWPP, device,
                                       profileCollection, pageCollection,
                                       profileMapper, 0);

    size_t msg_len = 128;
    // cc initialized to NSM_ERROR(1); return cc?cc:rc returns cc not rc
    uint8_t rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmWorkloadPowerProfilePage, HandleResponseMsg_ErrorCC)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);

    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(sNameWPP, sTypeWPP,
                                                            invPathWPP, device);
    auto pageCollection =
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            sNameWPP, sTypeWPP, invPathWPP, device);

    std::vector<std::string> profileNames = {"Profile0"};
    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
        sNameWPP, sTypeWPP, profileNames);

    NsmWorkloadPowerProfilePage sensor(sNameWPP, sTypeWPP, invPathWPP, device,
                                       profileCollection, pageCollection,
                                       profileMapper, 0);

    // Build a valid-sized response with error CC
    struct nsm_all_workload_power_profile_meta_data metaData = {};
    metaData.next_identifier = 0;
    metaData.number_of_profiles = 1;

    struct nsm_workload_power_profile_data profileData = {};
    profileData.profile_id = 0;

    uint8_t numProfiles = 1;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
            sizeof(nsm_all_workload_power_profile_resp_meta_data) - 1 +
            numProfiles * sizeof(nsm_workload_power_profile_data),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_workload_power_profile_info_resp(
        0, NSM_ERROR, reason_code, &metaData, &profileData, numProfiles,
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmWorkLoadProfileStatus, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(busWPP, invPathWPP, nullptr);
    auto profileInfoAsync = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        busWPP, invPathWPP.c_str(), nullptr);

    NsmWorkLoadProfileStatus sensor(sNameWPP, sTypeWPP, invPathWPP,
                                    profileStatusInfo, profileInfoAsync);

    // 7-byte buffer: safely reads cc=0 at payload[1] but too short for
    // nsm_get_workload_power_profile_status_info_resp; decode returns error
    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = sensor.handleResponseMsg(response, responseMsg.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmWorkloadPowerProfilePage, HandleResponseMsg_DecodeFail_ReturnsError)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);

    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(sNameWPP, sTypeWPP,
                                                            invPathWPP, device);
    auto pageCollection =
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            sNameWPP, sTypeWPP, invPathWPP, device);

    std::vector<std::string> profileNames = {"Profile0"};
    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
        sNameWPP, sTypeWPP, profileNames);

    NsmWorkloadPowerProfilePage sensor(sNameWPP, sTypeWPP, invPathWPP, device,
                                       profileCollection, pageCollection,
                                       profileMapper, 0);

    // 7-byte buffer: safely reads cc=0 at payload[1] but too short for
    // nsm_get_workload_power_profile_info_resp; decode returns error
    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = sensor.handleResponseMsg(response, responseMsg.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}

// instanceId > NSM_INSTANCE_MAX → encode_get_workload_power_profile_info_req
// fails → FALSE branch (encode failure path) taken → returns nullopt
TEST(NsmWorkloadPowerProfilePage,
     GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa9";
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);

    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(sNameWPP, sTypeWPP,
                                                            invPathWPP, device);
    auto pageCollection =
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            sNameWPP, sTypeWPP, invPathWPP, device);

    std::vector<std::string> profileNames = {"Profile0"};
    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(
        sNameWPP, sTypeWPP, profileNames);

    NsmWorkloadPowerProfilePage sensor(sNameWPP, sTypeWPP, invPathWPP, device,
                                       profileCollection, pageCollection,
                                       profileMapper, 0);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
