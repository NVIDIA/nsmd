/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
 * Tests for nsmd/nsmProcessor/nsmReconfigPermissions.cpp
 *
 *   - NsmReconfigPermissions::NsmReconfigPermissions (constructor)
 *   - NsmReconfigPermissions::getIndex (all feature→index mappings,
 *     plus invalid feature throws)
 *   - NsmReconfigPermissions::genRequestMsg (success)
 *   - NsmReconfigPermissions::handleResponseMsg (success, all 6 booleans)
 *   - NsmReconfigPermissions::handleResponseMsg (error CC)
 */

#include "test/mockDBusHandler.hpp"

using namespace ::testing;

#include "base.h"
#include "device-configuration.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmReconfigPermissions.hpp"

using namespace nsm;

static auto& testBus = utils::DBusHandler::getBus();

// Helper: build a fully constructed NsmReconfigPermissions
static std::shared_ptr<NsmReconfigPermissions>
    makeReconfigSensor(const std::string& basePath,
                       std::shared_ptr<ReconfigSettingsIntf>& outHost,
                       std::shared_ptr<ReconfigSettingsIntf>& outDoe,
                       std::string& hostPath, std::string& doePath,
                       ReconfigSettingsIntf::FeatureType feature =
                           ReconfigSettingsIntf::FeatureType::CCMode)
{
    hostPath = basePath + "/host";
    doePath = basePath + "/doe";
    outHost = std::make_shared<ReconfigSettingsIntf>(testBus, hostPath.c_str());
    outDoe = std::make_shared<ReconfigSettingsIntf>(testBus, doePath.c_str());
    return std::make_shared<NsmReconfigPermissions>(
        "reconfig_test", "NSM_ReconfigPermissions", hostPath, doePath, feature,
        outHost, outDoe);
}

// =============================================================================
// Constructor
// =============================================================================

TEST(NsmReconfigPermissions, Constructor_CreatesObject)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;

    auto sensor = makeReconfigSensor("/test/reconfig/ctor", hostIntf, doeIntf,
                                     hostPath, doePath);

    EXPECT_EQ(sensor->getName(), "reconfig_test");
    EXPECT_EQ(sensor->getType(), "NSM_ReconfigPermissions");
    EXPECT_EQ(sensor->feature, ReconfigSettingsIntf::FeatureType::CCMode);
    EXPECT_EQ(sensor->index, RP_CONFIDENTIAL_COMPUTE);
}

// =============================================================================
// getIndex – mapping each FeatureType to the correct index
// =============================================================================

TEST(NsmReconfigPermissions, GetIndex_InSystemTest_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::InSystemTest),
              RP_IN_SYSTEM_TEST);
}

TEST(NsmReconfigPermissions, GetIndex_FusingMode_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::FusingMode),
              RP_FUSING_MODE);
}

TEST(NsmReconfigPermissions, GetIndex_CCMode_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::CCMode),
              RP_CONFIDENTIAL_COMPUTE);
}

TEST(NsmReconfigPermissions, GetIndex_BAR0Firewall_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::BAR0Firewall),
              RP_BAR0_FIREWALL);
}

TEST(NsmReconfigPermissions, GetIndex_NVLinkDisable_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::NVLinkDisable),
              RP_NVLINK_DISABLE);
}

TEST(NsmReconfigPermissions, GetIndex_ECCEnable_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::ECCEnable),
              RP_ECC_ENABLE);
}

TEST(NsmReconfigPermissions, GetIndex_HBMFrequencyChange_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::HBMFrequencyChange),
              RP_HBM_FREQUENCY_CHANGE);
}

TEST(NsmReconfigPermissions, GetIndex_PowerSmoothingLevel1_ReturnsCorrectIndex)
{
    EXPECT_EQ(
        NsmReconfigPermissions::getIndex(
            ReconfigSettingsIntf::FeatureType::PowerSmoothingPrivilegeLevel1),
        RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_1);
}

TEST(NsmReconfigPermissions, GetIndex_PowerSmoothingLevel2_ReturnsCorrectIndex)
{
    EXPECT_EQ(
        NsmReconfigPermissions::getIndex(
            ReconfigSettingsIntf::FeatureType::PowerSmoothingPrivilegeLevel2),
        RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2);
}

TEST(NsmReconfigPermissions, GetIndex_EGMMode_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::EGMMode),
              RP_EGM_MODE);
}

TEST(NsmReconfigPermissions, GetIndex_InvalidFeature_Throws)
{
    // Cast an out-of-range int to FeatureType to trigger the default case
    auto invalidFeature = static_cast<ReconfigSettingsIntf::FeatureType>(0xFF);
    EXPECT_THROW(NsmReconfigPermissions::getIndex(invalidFeature),
                 std::invalid_argument);
}

// =============================================================================
// genRequestMsg
// =============================================================================

TEST(NsmReconfigPermissions, GenRequestMsg_ReturnsValidRequest)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/genreq", hostIntf, doeIntf,
                                     hostPath, doePath);

    auto request = sensor->genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_reconfiguration_permissions_v1_req));
}

// =============================================================================
// handleResponseMsg – success: all six booleans updated
// =============================================================================

TEST(NsmReconfigPermissions, HandleResponseMsg_Success_UpdatesAllPermissions)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/handle_ok", hostIntf,
                                     doeIntf, hostPath, doePath);

    nsm_reconfiguration_permissions_v1 data = {};
    data.host_oneshot = 1;
    data.host_persistent = 0;
    data.host_flr_persistent = 1;
    data.DOE_oneshot = 0;
    data.DOE_persistent = 1;
    data.DOE_flr_persistent = 0;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_reconfiguration_permissions_v1_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_reconfiguration_permissions_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor->handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(hostIntf->allowOneShotConfig(), true);
    EXPECT_EQ(hostIntf->allowPersistentConfig(), false);
    EXPECT_EQ(hostIntf->allowFLRPersistentConfig(), true);
    EXPECT_EQ(doeIntf->allowOneShotConfig(), false);
    EXPECT_EQ(doeIntf->allowPersistentConfig(), true);
    EXPECT_EQ(doeIntf->allowFLRPersistentConfig(), false);
}

// =============================================================================
// handleResponseMsg – error CC: returns error
// =============================================================================

TEST(NsmReconfigPermissions, HandleResponseMsg_ErrorCC_ReturnsError)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/handle_err", hostIntf,
                                     doeIntf, hostPath, doePath);

    nsm_reconfiguration_permissions_v1 data = {};
    data.host_oneshot = 1;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_reconfiguration_permissions_v1_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_reconfiguration_permissions_v1_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor->handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}
