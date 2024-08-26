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

#include "test/mockSensorManager.hpp"

#include <xyz/openbmc_project/Common/Device/error.hpp>
using namespace ::testing;

#define private public
#define protected public

#include "device-configuration.h"

#include "nsmSetReconfigSettings.hpp"

struct nsmReconfigSettingsTest : public testing::Test, public SensorManagerTest
{
    const uuid_t gpuUuid = "992b3ec1-e464-f145-8686-409009062aa8";

    std::shared_ptr<NsmDevice> gpu = std::make_shared<NsmDevice>(gpuUuid);
    NsmDeviceTable devices{{gpu}};

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    NiceMock<MockSensorManager> mockManager{devices};
    std::unique_ptr<NsmSetReconfigSettings> reconfigSettings;

    void init(
        reconfiguration_permissions_v1_index settingIndex = RP_IN_SYSTEM_TEST)
    {
        status = AsyncOperationStatusType::Success;
        reconfigSettings.reset();
        reconfigSettings = std::make_unique<NsmSetReconfigSettings>(
            std::to_string(settingIndex), mockManager,
            (processorsInventoryBasePath / "HGX_GPU_TEST_DEV").string(),
            settingIndex);
    }

    Response setReconfigPermissionsMsg{
        0x10,
        0xDE,                          // PCI VID: NVIDIA 0x10DE
        0x00,                          // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                          // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
        NSM_SET_RECONFIGURATION_PERMISSIONS_V1, // command
        0,                                      // completion code
        0,
        0,
        0,
        0 // data size
    };

    bool testSetReconfigSettings(
        reconfiguration_permissions_v1_index settingIndex,
        reconfiguration_permissions_v1_setting configuration, bool permission)
    {
        init(settingIndex);
        nsm_reconfiguration_permissions_v1 expectedData = {0, 0, 0, 0};
        switch (configuration)
        {
            case RP_ONESHOOT_HOT_RESET:
                expectedData.oneshot = permission;
                break;
            case RP_PERSISTENT:
                expectedData.persistent = permission;
                break;
            case RP_ONESHOT_FLR:
                expectedData.flr_persistent = permission;
                break;
        }
        Response getResponseData = {0x00};
        memcpy(getResponseData.data(), &expectedData, sizeof(expectedData));
        EXPECT_CALL(mockManager, SendRecvNsmMsg)
            .WillOnce(mockSendRecvNsmMsg(setReconfigPermissionsMsg));

        uint8_t result = 0;
        switch (configuration)
        {
            case RP_ONESHOOT_HOT_RESET:

                result = reconfigSettings
                             ->allowOneShotConfig(permission, &status, gpu)
                             .await_resume();
                break;
            case RP_PERSISTENT:
                result = reconfigSettings
                             ->allowPersistentConfig(permission, &status, gpu)
                             .await_resume();
                break;
            case RP_ONESHOT_FLR:
                result = reconfigSettings
                             ->allowFLRPersistentConfig(permission, &status,
                                                        gpu)
                             .await_resume();
                break;
            default:
                result = reconfigSettings
                             ->setAllowPermission(configuration, permission,
                                                  status, gpu)
                             .await_resume();
                break;
        }
        EXPECT_EQ(AsyncOperationStatusType::Success, status);
        EXPECT_EQ(NSM_SW_SUCCESS, result);
        return permission;
    }
};

TEST_F(nsmReconfigSettingsTest, badTestSetReconfigSettings)
{
    init();
    uint32_t badValue = 0;
    reconfigSettings->allowOneShotConfig(badValue, &status, gpu);
    reconfigSettings->allowPersistentConfig(badValue, &status, gpu);
    reconfigSettings->allowFLRPersistentConfig(badValue, &status, gpu);

    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(setReconfigPermissionsMsg, Response(),
                                     NSM_ERR_UNSUPPORTED_COMMAND_CODE));
    EXPECT_NO_THROW(
        reconfigSettings
            ->setAllowPermission(RP_ONESHOOT_HOT_RESET, true, status, gpu)
            .await_resume());
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);

    Response setReconfigPermissionsErrorMsg{
        0x10,
        0xDE,                          // PCI VID: NVIDIA 0x10DE
        0x00,                          // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                          // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
        NSM_SET_RECONFIGURATION_PERMISSIONS_V1, // command
        0,                                      // completion code
        0,
        0,
        1, // incorrect data size
        0, // data size
        0, // invalid data byte
    };

    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(setReconfigPermissionsErrorMsg));
    EXPECT_NO_THROW(
        reconfigSettings
            ->setAllowPermission(RP_ONESHOOT_HOT_RESET, false, status, gpu)
            .await_resume());
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(nsmReconfigSettingsTest, goodTestSetReconfigSettings)
{
    for (auto fi = 0; fi <= int(RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2); fi++)
    {
        for (auto ci = 0; ci <= int(RP_ONESHOT_FLR); ci++)
        {
            auto settingIndex = reconfiguration_permissions_v1_index(fi);
            auto configuration = reconfiguration_permissions_v1_setting(ci);
            EXPECT_EQ(true, testSetReconfigSettings(settingIndex, configuration,
                                                    true));
            EXPECT_EQ(false, testSetReconfigSettings(settingIndex,
                                                     configuration, false));
        }
    }
}
