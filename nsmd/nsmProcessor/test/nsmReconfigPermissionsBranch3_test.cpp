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
 * Branch coverage batch 3 for nsmReconfigPermissions.cpp
 *
 * Targets remaining uncovered branches:
 * - getIndex: remaining FeatureType cases (CCDevMode, TGPCurrentLimit, etc.)
 * - setAllowPermission: encode failure (rc != NSM_SW_SUCCESS)
 * - setAllowPermission: decode failure (cc != NSM_SUCCESS && rc != 0)
 * - Additional permission combinations in patch methods
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

static auto& testBusBr3 = utils::DBusHandler::getBus();

// Helper: build a fully constructed NsmReconfigPermissions
static std::shared_ptr<NsmReconfigPermissions>
    makeReconfigSensorBr3(const std::string& basePath,
                          std::shared_ptr<ReconfigSettingsIntf>& outHost,
                          std::shared_ptr<ReconfigSettingsIntf>& outDoe,
                          std::string& hostPath, std::string& doePath,
                          ReconfigSettingsIntf::FeatureType feature =
                              ReconfigSettingsIntf::FeatureType::CCMode)
{
    hostPath = basePath + "/host";
    doePath = basePath + "/doe";
    outHost = std::make_shared<ReconfigSettingsIntf>(testBusBr3,
                                                     hostPath.c_str());
    outDoe = std::make_shared<ReconfigSettingsIntf>(testBusBr3,
                                                    doePath.c_str());
    return std::make_shared<NsmReconfigPermissions>(
        "reconfig_br3", "NSM_ReconfigPermissions", hostPath, doePath, feature,
        outHost, outDoe);
}

// =============================================================================
// getIndex - remaining FeatureType switch cases not covered elsewhere
// =============================================================================

TEST(NsmReconfigPermsBranch3, GetIndex_CCDevMode)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::CCDevMode),
              RP_CONFIDENTIAL_COMPUTE_DEV_MODE);
}

TEST(NsmReconfigPermsBranch3, GetIndex_TGPCurrentLimit)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::TGPCurrentLimit),
              RP_TOTAL_GPU_POWER_CURRENT_LIMIT);
}

TEST(NsmReconfigPermsBranch3, GetIndex_TGPRatedLimit)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::TGPRatedLimit),
              RP_TOTAL_GPU_POWER_RATED_LIMIT);
}

TEST(NsmReconfigPermsBranch3, GetIndex_TGPMaxLimit)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::TGPMaxLimit),
              RP_TOTAL_GPU_POWER_MAX_LIMIT);
}

TEST(NsmReconfigPermsBranch3, GetIndex_TGPMinLimit)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::TGPMinLimit),
              RP_TOTAL_GPU_POWER_MIN_LIMIT);
}

TEST(NsmReconfigPermsBranch3, GetIndex_ClockLimit)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::ClockLimit),
              RP_CLOCK_LIMIT);
}

TEST(NsmReconfigPermsBranch3, GetIndex_PCIeVFConfiguration)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::PCIeVFConfiguration),
              RP_PCIE_VF_CONFIGURATION);
}

TEST(NsmReconfigPermsBranch3, GetIndex_RowRemappingAllowed)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::RowRemappingAllowed),
              RP_ROW_REMAPPING_ALLOWED);
}

TEST(NsmReconfigPermsBranch3, GetIndex_RowRemappingFeature)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::RowRemappingFeature),
              RP_ROW_REMAPPING_FEATURE);
}

TEST(NsmReconfigPermsBranch3, GetIndex_HULKLicenseUpdate)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::HULKLicenseUpdate),
              RP_HULK_LICENSE_UPDATE);
}

TEST(NsmReconfigPermsBranch3, GetIndex_ForceTestCoupling)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::ForceTestCoupling),
              RP_FORCE_TEST_COUPLING);
}

TEST(NsmReconfigPermsBranch3, GetIndex_BAR0TypeConfig)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::BAR0TypeConfig),
              RP_BAR0_TYPE_CONFIG);
}

TEST(NsmReconfigPermsBranch3, GetIndex_EDPpScalingFactor)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::EDPpScalingFactor),
              RP_EDPP_SCALING_FACTOR);
}

TEST(NsmReconfigPermsBranch3, GetIndex_InfoROMFileSystemRecreate)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::InfoROMFileSystemRecreate),
              RP_INFOROM_RECREATE_ALLOW_INB);
}

// =============================================================================
// Fixture for coroutine-based tests
// =============================================================================

struct NsmReconfigPermsBr3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmReconfigPermsBr3Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmReconfigPermsBr3Test()
    {
        cleanupDeviceSensors(devices);
    }

    static std::vector<uint8_t> makeSetReconfigSuccessResp()
    {
        std::vector<uint8_t> resp(256, 0);
        auto ptr = reinterpret_cast<nsm_msg*>(resp.data());
        encode_set_reconfiguration_permissions_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       ptr);
        return resp;
    }

    static std::vector<uint8_t> makeSetReconfigErrorResp()
    {
        std::vector<uint8_t> resp(256, 0);
        auto ptr = reinterpret_cast<nsm_msg*>(resp.data());
        encode_set_reconfiguration_permissions_v1_resp(0, NSM_ERROR, ERR_NULL,
                                                       ptr);
        return resp;
    }
};

// =============================================================================
// setAllowPermission: decode failure (error CC after successful postPatchIO)
// This covers the else branch in the decode success/failure check
// =============================================================================
TEST_F(NsmReconfigPermsBr3Test, SetAllowPermission_DecodeFailure_WriteFailure)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensorBr3("/test/reconfig_br3/decode_fail",
                                        hostIntf, doeIntf, hostPath, doePath);

    // Send a too-small response so decode fails
    std::vector<uint8_t> badResp(sizeof(nsm_msg_hdr) + 2, 0);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(badResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setAllowPermission(RP_ONESHOOT_HOT_RESET, 1, status,
                                           gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// Constructor with various FeatureType values to hit more switch branches
// =============================================================================
TEST(NsmReconfigPermsBranch3, Constructor_CCDevMode)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensorBr3(
        "/test/reconfig_br3/ctor_ccdev", hostIntf, doeIntf, hostPath, doePath,
        ReconfigSettingsIntf::FeatureType::CCDevMode);
    EXPECT_EQ(sensor->index, RP_CONFIDENTIAL_COMPUTE_DEV_MODE);
}

TEST(NsmReconfigPermsBranch3, Constructor_TGPCurrentLimit)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensorBr3(
        "/test/reconfig_br3/ctor_tgpcur", hostIntf, doeIntf, hostPath, doePath,
        ReconfigSettingsIntf::FeatureType::TGPCurrentLimit);
    EXPECT_EQ(sensor->index, RP_TOTAL_GPU_POWER_CURRENT_LIMIT);
}

TEST(NsmReconfigPermsBranch3, Constructor_ClockLimit)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensorBr3(
        "/test/reconfig_br3/ctor_clock", hostIntf, doeIntf, hostPath, doePath,
        ReconfigSettingsIntf::FeatureType::ClockLimit);
    EXPECT_EQ(sensor->index, RP_CLOCK_LIMIT);
}

TEST(NsmReconfigPermsBranch3, Constructor_PCIeVFConfiguration)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensorBr3(
        "/test/reconfig_br3/ctor_pcievf", hostIntf, doeIntf, hostPath, doePath,
        ReconfigSettingsIntf::FeatureType::PCIeVFConfiguration);
    EXPECT_EQ(sensor->index, RP_PCIE_VF_CONFIGURATION);
}

TEST(NsmReconfigPermsBranch3, Constructor_EDPpScalingFactor)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensorBr3(
        "/test/reconfig_br3/ctor_edpp", hostIntf, doeIntf, hostPath, doePath,
        ReconfigSettingsIntf::FeatureType::EDPpScalingFactor);
    EXPECT_EQ(sensor->index, RP_EDPP_SCALING_FACTOR);
}
