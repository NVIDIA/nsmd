/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

// Tests for NVSwitch power profiles.

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "network-ports.h"

#include "nsmDeviceInventory/nsmSwitch.hpp"
#include "nsmDeviceInventory/nsmSwitchPowerProfile.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine createNsmSwitchDI(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

using namespace nsm;

namespace
{

Response encodeProfiles(uint8_t active,
                        const std::vector<std::pair<uint8_t, std::string>>& in)
{
    std::vector<nsm_power_profile_entry> entries;
    for (const auto& [id, name] : in)
    {
        nsm_power_profile_entry entry{};
        entry.profile_id = id;
        std::strncpy(entry.profile_name, name.c_str(),
                     sizeof(entry.profile_name) - 1);
        entries.push_back(entry);
    }
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + 2 +
                      entries.size() * sizeof(nsm_power_profile_entry));
    EXPECT_EQ(NSM_SW_SUCCESS, encode_get_power_profile_resp(
                                  0, NSM_SUCCESS, ERR_NULL, active,
                                  static_cast<uint8_t>(entries.size()),
                                  entries.empty() ? nullptr : entries.data(),
                                  reinterpret_cast<nsm_msg*>(response.data())));
    return response;
}

Response encodeSetProfile(uint8_t cc, uint16_t reasonCode = ERR_NULL)
{
    Response response(sizeof(nsm_msg_hdr) +
                      (cc == NSM_SUCCESS
                           ? sizeof(nsm_common_resp)
                           : sizeof(nsm_common_non_success_resp)));
    EXPECT_EQ(NSM_SW_SUCCESS, encode_set_power_profile_resp(
                                  0, cc, reasonCode,
                                  reinterpret_cast<nsm_msg*>(response.data())));
    return response;
}

} // namespace

class NsmSwitchPowerProfileTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const uuid_t switchUuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchPowerProfileTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchPowerProfileTest()
    {
        cleanupDeviceSensors(devices);
    }

    NsmSwitchPowerProfile makeProfileSensor()
    {
        return NsmSwitchPowerProfile("NVSwitch_0_PowerProfile", "NSM_NVSwitch",
                                     nullptr);
    }

    AsyncOperationStatusType setWithDeviceReply(uint8_t cc,
                                                uint16_t reasonCode = ERR_NULL)
    {
        auto sensor = makeProfileSensor();
        EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
            .WillOnce(mockPostPatchIO(encodeSetProfile(cc, reasonCode)));
        AsyncOperationStatusType status = AsyncOperationStatusType::Success;
        sensor.setActiveProfile(uint8_t{NSM_POWER_PROFILE_ID_HW_DEFAULT},
                                &status, nvswitch);
        return status;
    }

    size_t createSwitchAndCountProfiles(const std::string& path,
                                        const std::string& name,
                                        bool supportPowerCappingMode,
                                        bool supportPowerProfile)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(
            path, "xyz.openbmc_project.Configuration.NSM_NVSwitch");
        pm["Name"] = name;
        pm["UUID"] = switchUuid;
        pm["InventoryObjPath"] =
            std::string("/xyz/openbmc_project/inventory/system/fabr/nvswitch/");
        pm["Type"] = std::string("NSM_NVSwitch");
        pm["SupportL1PredictionMode"] = false;
        pm["SupportPowerCappingMode"] = supportPowerCappingMode;
        pm["SupportPowerProfile"] = supportPowerProfile;

        createNsmSwitchDI(mockManager,
                          "xyz.openbmc_project.Configuration.NSM_NVSwitch",
                          path);

        size_t profiles = 0;
        for (const auto& sensor : nvswitch->roundRobinSensors)
        {
            profiles += std::dynamic_pointer_cast<NsmSwitchPowerProfile>(sensor)
                            ? 1
                            : 0;
        }
        return profiles;
    }
};

// ============================================================================
// Reads
// ============================================================================
TEST_F(NsmSwitchPowerProfileTest, Profile_RequestIsGetPowerProfile)
{
    auto sensor = makeProfileSensor();
    auto request = sensor.genRequestMsg(0, 0);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(NSM_SW_SUCCESS,
              decode_get_power_profile_req(
                  reinterpret_cast<const nsm_msg*>(request->data()),
                  request->size()));
}

TEST_F(NsmSwitchPowerProfileTest, Profile_PlaceholderOnlyListsHardwareDefault)
{
    auto sensor = makeProfileSensor();
    auto response = encodeProfiles(0, {{0, ""}});
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<nsm_msg*>(response.data()), response.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
    EXPECT_EQ(NSM_POWER_PROFILE_ID_HW_DEFAULT, sensor.activeProfileId());
    ASSERT_EQ(1u, sensor.availableProfiles().size());
    EXPECT_EQ(NsmSwitchPowerProfile::ProfileEntry(
                  0, NsmSwitchPowerProfile::hardwareDefaultProfileName, true),
              sensor.availableProfiles()[0]);
}

TEST_F(NsmSwitchPowerProfileTest, Profile_HardwareDefaultListedWhenNotReported)
{
    auto sensor = makeProfileSensor();
    auto response = encodeProfiles(1, {{1, "compute"}});
    ASSERT_EQ(NSM_SW_SUCCESS, sensor.handleResponseMsg(
                                  reinterpret_cast<nsm_msg*>(response.data()),
                                  response.size()));
    const auto& profiles = sensor.availableProfiles();
    ASSERT_EQ(2u, profiles.size());
    EXPECT_EQ(NsmSwitchPowerProfile::ProfileEntry(
                  0, NsmSwitchPowerProfile::hardwareDefaultProfileName, false),
              profiles[0]);
    EXPECT_EQ(NsmSwitchPowerProfile::ProfileEntry(1, "compute", true),
              profiles[1]);
}

TEST_F(NsmSwitchPowerProfileTest, Profile_DeviceNameForHardwareDefaultIsKept)
{
    auto sensor = makeProfileSensor();
    auto response = encodeProfiles(0, {{0, "ini-default"}, {1, "compute"}});
    ASSERT_EQ(NSM_SW_SUCCESS, sensor.handleResponseMsg(
                                  reinterpret_cast<nsm_msg*>(response.data()),
                                  response.size()));
    const auto& profiles = sensor.availableProfiles();
    ASSERT_EQ(2u, profiles.size());
    EXPECT_EQ(NsmSwitchPowerProfile::ProfileEntry(0, "ini-default", true),
              profiles[0]);
}

TEST_F(NsmSwitchPowerProfileTest, Profile_PublishesListAndActive)
{
    auto sensor = makeProfileSensor();
    auto response = encodeProfiles(
        2, {{1, "compute"}, {2, "networking"}, {3, "reduced-bandwidth"}});
    ASSERT_EQ(NSM_SW_SUCCESS, sensor.handleResponseMsg(
                                  reinterpret_cast<nsm_msg*>(response.data()),
                                  response.size()));

    EXPECT_EQ(2, sensor.activeProfileId());
    const auto& profiles = sensor.availableProfiles();
    ASSERT_EQ(4u, profiles.size());
    EXPECT_EQ(NsmSwitchPowerProfile::ProfileEntry(1, "compute", false),
              profiles[1]);
    EXPECT_EQ(NsmSwitchPowerProfile::ProfileEntry(2, "networking", true),
              profiles[2]);
}

TEST_F(NsmSwitchPowerProfileTest, Profile_PublishesToDbus)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<PowerProfileIntf>(
        testBus, "/test/switchprofile/publish");
    NsmSwitchPowerProfile sensor("NVSwitch_0_PowerProfile", "NSM_NVSwitch",
                                 intf);

    auto response = encodeProfiles(1, {{1, "compute"}, {2, "networking"}});
    ASSERT_EQ(NSM_SW_SUCCESS, sensor.handleResponseMsg(
                                  reinterpret_cast<nsm_msg*>(response.data()),
                                  response.size()));
    EXPECT_EQ(1, intf->activeProfileId());
    EXPECT_EQ("compute", intf->activeProfileName());
    ASSERT_EQ(3u, intf->availableProfiles().size());
    EXPECT_EQ(NsmSwitchPowerProfile::ProfileEntry(2, "networking", false),
              intf->availableProfiles()[2]);

    response = encodeProfiles(0, {{1, "compute"}, {2, "networking"}});
    ASSERT_EQ(NSM_SW_SUCCESS, sensor.handleResponseMsg(
                                  reinterpret_cast<nsm_msg*>(response.data()),
                                  response.size()));
    EXPECT_EQ(0, intf->activeProfileId());
    EXPECT_EQ(NsmSwitchPowerProfile::hardwareDefaultProfileName,
              intf->activeProfileName());
}

TEST_F(NsmSwitchPowerProfileTest, Profile_ErrorReturnsCompletionCode)
{
    auto sensor = makeProfileSensor();
    Response response(sizeof(nsm_msg_hdr) +
                      sizeof(nsm_common_non_success_resp));
    encode_get_power_profile_resp(0, NSM_ERR_NOT_READY, ERR_NULL, 0, 0, nullptr,
                                  reinterpret_cast<nsm_msg*>(response.data()));
    EXPECT_EQ(
        NSM_ERR_NOT_READY,
        sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(response.data()),
                                 response.size()));
}

TEST_F(NsmSwitchPowerProfileTest, ProfileUpdate_PollsAndPublishes)
{
    auto sensor = makeProfileSensor();
    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(encodeProfiles(1, {{1, "compute"}})));
    sensor.update(nvswitch);

    EXPECT_EQ(1, sensor.activeProfileId());
}

// ============================================================================
// setActiveProfile
// ============================================================================
TEST_F(NsmSwitchPowerProfileTest, SetProfile_UnknownIdRejectedWithoutIO)
{
    auto sensor = makeProfileSensor();
    auto list = encodeProfiles(1, {{1, "compute"}});
    sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(list.data()),
                             list.size());

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _)).Times(0);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setActiveProfile(uint8_t{9}, &status, nvswitch);

    EXPECT_EQ(AsyncOperationStatusType::InvalidArgument, status);
}

TEST_F(NsmSwitchPowerProfileTest, SetProfile_Accepted)
{
    auto sensor = makeProfileSensor();
    auto list = encodeProfiles(1, {{1, "compute"}, {2, "networking"}});
    sensor.handleResponseMsg(reinterpret_cast<nsm_msg*>(list.data()),
                             list.size());

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(encodeSetProfile(NSM_SUCCESS)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setActiveProfile(uint8_t{2}, &status, nvswitch);

    EXPECT_EQ(AsyncOperationStatusType::Success, status);
}

TEST_F(NsmSwitchPowerProfileTest, SetProfile_HardwareDefaultAlwaysAllowed)
{
    auto sensor = makeProfileSensor();
    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(encodeSetProfile(NSM_SUCCESS)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setActiveProfile(uint8_t{NSM_POWER_PROFILE_ID_HW_DEFAULT}, &status,
                            nvswitch);
    EXPECT_EQ(AsyncOperationStatusType::Success, status);
}

TEST_F(NsmSwitchPowerProfileTest, SetProfile_DeviceRejectsId)
{
    EXPECT_EQ(AsyncOperationStatusType::InvalidArgument,
              setWithDeviceReply(NSM_ERR_INVALID_DATA));
}

TEST_F(NsmSwitchPowerProfileTest, SetProfile_UnsupportedCommand)
{
    EXPECT_EQ(AsyncOperationStatusType::UnsupportedRequest,
              setWithDeviceReply(NSM_ERR_UNSUPPORTED_COMMAND_CODE));
}

TEST_F(NsmSwitchPowerProfileTest, SetProfile_UnsupportedReasonCode)
{
    EXPECT_EQ(AsyncOperationStatusType::UnsupportedRequest,
              setWithDeviceReply(NSM_ERROR, ERR_NOT_SUPPORTED));
}

TEST_F(NsmSwitchPowerProfileTest, SetProfile_DeviceNotReady)
{
    EXPECT_EQ(AsyncOperationStatusType::Unavailable,
              setWithDeviceReply(NSM_ERR_NOT_READY));
}

TEST_F(NsmSwitchPowerProfileTest, SetProfile_GenericDeviceError)
{
    EXPECT_EQ(AsyncOperationStatusType::WriteFailure,
              setWithDeviceReply(NSM_ERROR));
}

TEST_F(NsmSwitchPowerProfileTest, SetProfile_DeviceOffline)
{
    auto sensor = makeProfileSensor();
    nvswitch->isDeviceActive = false;
    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _)).Times(0);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setActiveProfile(uint8_t{NSM_POWER_PROFILE_ID_HW_DEFAULT}, &status,
                            nvswitch);
    nvswitch->isDeviceActive = true;

    EXPECT_EQ(AsyncOperationStatusType::Unavailable, status);
}

TEST_F(NsmSwitchPowerProfileTest, SetProfile_TransportErrorOnSet)
{
    auto sensor = makeProfileSensor();
    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor.setActiveProfile(uint8_t{NSM_POWER_PROFILE_ID_HW_DEFAULT}, &status,
                            nvswitch);

    EXPECT_EQ(AsyncOperationStatusType::WriteFailure, status);
}

// ============================================================================
// Factory: SupportPowerProfile alone decides whether profiles are exposed
// ============================================================================
TEST_F(NsmSwitchPowerProfileTest, Factory_SupportPowerProfileCreatesSensor)
{
    EXPECT_EQ(1u, createSwitchAndCountProfiles("/test/switchprofile/only",
                                               "NVSwitch_ProfileOnly", false,
                                               true));
}

TEST_F(NsmSwitchPowerProfileTest, Factory_PowerCappingModeAloneHasNoProfiles)
{
    EXPECT_EQ(0u,
              createSwitchAndCountProfiles("/test/switchprofile/mode",
                                           "NVSwitch_ModeOnly", true, false));
}
