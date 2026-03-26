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
 * Remaining getIndex switch case coverage for nsmReconfigPermissions.cpp
 * Covers all 10 cases not tested in nsmReconfigPermissionsBranch_test.cpp:
 * InSystemTest, FusingMode, CCMode, BAR0Firewall, NVLinkDisable,
 * ECCEnable, HBMFrequencyChange, PowerSmoothingPrivilegeLevel1,
 * PowerSmoothingPrivilegeLevel2, EGMMode
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#include "base.h"
#include "device-configuration.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmReconfigPermissions.hpp"

using namespace nsm;

static auto& testBus3 = utils::DBusHandler::getBus();

static std::shared_ptr<NsmReconfigPermissions>
    makeReconfigSensor3(const std::string& basePath,
                        ReconfigSettingsIntf::FeatureType feature)
{
    std::string hostPath = basePath + "/host";
    std::string doePath = basePath + "/doe";
    auto hostIntf = std::make_shared<ReconfigSettingsIntf>(testBus3,
                                                           hostPath.c_str());
    auto doeIntf = std::make_shared<ReconfigSettingsIntf>(testBus3,
                                                          doePath.c_str());

    return std::make_shared<NsmReconfigPermissions>(
        "Reconfig_Br2", "NSM_Reconfig", hostPath, doePath, feature, hostIntf,
        doeIntf);
}

// ============================================================================
// getIndex - remaining FeatureType switch cases
// ============================================================================

TEST(NsmReconfigPermsBranch2, GetIndex_InSystemTest)
{
    auto s = makeReconfigSensor3(
        "/xyz/openbmc_project/inventory/reconfig_br2/insystemtest",
        ReconfigSettingsIntf::FeatureType::InSystemTest);
    EXPECT_EQ(s->index, RP_IN_SYSTEM_TEST);
}

TEST(NsmReconfigPermsBranch2, GetIndex_FusingMode)
{
    auto s = makeReconfigSensor3(
        "/xyz/openbmc_project/inventory/reconfig_br2/fusingmode",
        ReconfigSettingsIntf::FeatureType::FusingMode);
    EXPECT_EQ(s->index, RP_FUSING_MODE);
}

TEST(NsmReconfigPermsBranch2, GetIndex_CCMode)
{
    auto s = makeReconfigSensor3(
        "/xyz/openbmc_project/inventory/reconfig_br2/ccmode",
        ReconfigSettingsIntf::FeatureType::CCMode);
    EXPECT_EQ(s->index, RP_CONFIDENTIAL_COMPUTE);
}

TEST(NsmReconfigPermsBranch2, GetIndex_BAR0Firewall)
{
    auto s = makeReconfigSensor3(
        "/xyz/openbmc_project/inventory/reconfig_br2/bar0firewall",
        ReconfigSettingsIntf::FeatureType::BAR0Firewall);
    EXPECT_EQ(s->index, RP_BAR0_FIREWALL);
}

TEST(NsmReconfigPermsBranch2, GetIndex_NVLinkDisable)
{
    auto s = makeReconfigSensor3(
        "/xyz/openbmc_project/inventory/reconfig_br2/nvlinkdisable",
        ReconfigSettingsIntf::FeatureType::NVLinkDisable);
    EXPECT_EQ(s->index, RP_NVLINK_DISABLE);
}

TEST(NsmReconfigPermsBranch2, GetIndex_ECCEnable)
{
    auto s = makeReconfigSensor3(
        "/xyz/openbmc_project/inventory/reconfig_br2/eccenable",
        ReconfigSettingsIntf::FeatureType::ECCEnable);
    EXPECT_EQ(s->index, RP_ECC_ENABLE);
}

TEST(NsmReconfigPermsBranch2, GetIndex_HBMFrequencyChange)
{
    auto s = makeReconfigSensor3(
        "/xyz/openbmc_project/inventory/reconfig_br2/hbmfreq",
        ReconfigSettingsIntf::FeatureType::HBMFrequencyChange);
    EXPECT_EQ(s->index, RP_HBM_FREQUENCY_CHANGE);
}

TEST(NsmReconfigPermsBranch2, GetIndex_PowerSmoothingPrivilegeLevel1)
{
    auto s = makeReconfigSensor3(
        "/xyz/openbmc_project/inventory/reconfig_br2/pspl1",
        ReconfigSettingsIntf::FeatureType::PowerSmoothingPrivilegeLevel1);
    EXPECT_EQ(s->index, RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_1);
}

TEST(NsmReconfigPermsBranch2, GetIndex_PowerSmoothingPrivilegeLevel2)
{
    auto s = makeReconfigSensor3(
        "/xyz/openbmc_project/inventory/reconfig_br2/pspl2",
        ReconfigSettingsIntf::FeatureType::PowerSmoothingPrivilegeLevel2);
    EXPECT_EQ(s->index, RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2);
}

TEST(NsmReconfigPermsBranch2, GetIndex_EGMMode)
{
    auto s = makeReconfigSensor3(
        "/xyz/openbmc_project/inventory/reconfig_br2/egmmode",
        ReconfigSettingsIntf::FeatureType::EGMMode);
    EXPECT_EQ(s->index, RP_EGM_MODE);
}
