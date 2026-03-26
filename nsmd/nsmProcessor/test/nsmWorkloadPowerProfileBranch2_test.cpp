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
 * Additional branch coverage tests for nsmWorkloadPowerProfile.cpp
 *
 * Covers remaining branches not hit by nsmWorkloadPowerProfile_branch_test:
 * - NsmWorkloadPowerProfilePage::genRequestMsg encode fail (instanceId=255)
 * - NsmWorkloadPowerProfileCollection::updateSupportedProfile with valid obj
 * - NsmWorkLoadProfileStatus::handleResponseMsg cc ? cc : rc FALSE branch
 * - NsmWorkLoadProfileStatus::updateReading with various mask patterns
 * - NsmWorkloadPowerProfilePage::handleResponseMsg error CC
 * - NsmWorkloadPowerProfilePageCollection::getPageById found
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "nsmWorkloadPowerProfile.hpp"
#include "nsmWorkloadPowerProfileInfoIface.hpp"

using namespace nsm;

// ============================================================================
// Helpers
// ============================================================================

static std::vector<uint8_t> makeWorkloadStatusResp2(uint8_t cc = NSM_SUCCESS,
                                                    uint8_t val = 0x0A)
{
    struct workload_power_profile_status profileData{};
    for (int i = 0; i < 8; i++)
    {
        profileData.supported_profile_mask.fields[i].byte = val;
        profileData.requested_profile_maks.fields[i].byte = val;
        profileData.enforced_profile_mask.fields[i].byte = val;
    }
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    [[maybe_unused]] auto rc = encode_get_workload_power_profile_status_resp(
        0, cc, ERR_NULL, &profileData, msg);
    return buf;
}

static std::vector<uint8_t> makeWorkloadInfoResp2(uint8_t numProfiles,
                                                  uint16_t nextId,
                                                  uint16_t startId = 0)
{
    struct nsm_all_workload_power_profile_meta_data meta{};
    meta.number_of_profiles = numProfiles;
    meta.next_identifier = nextId;

    std::vector<nsm_workload_power_profile_data> profiles(numProfiles);
    for (uint8_t i = 0; i < numProfiles; i++)
    {
        profiles[i].profile_id = startId + i;
        profiles[i].priority = i + 1;
    }

    uint16_t dataSize = sizeof(meta) +
                        numProfiles * sizeof(nsm_workload_power_profile_data);
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + dataSize, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    [[maybe_unused]] auto rc = encode_get_workload_power_profile_info_resp(
        0, NSM_SUCCESS, ERR_NULL, &meta,
        numProfiles > 0 ? profiles.data() : nullptr, numProfiles, msg);
    return buf;
}

// ============================================================================
// Fixture
// ============================================================================

struct WPPBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    const std::string invPath = "/xyz/openbmc_project/inventory/wppb2_test";

    std::string sensorName = "wppB2Status";
    std::string sensorType = "NSM_WPP_B2";

    WPPBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        EXPECT_CALL(mockManager, getEid(testing::_))
            .WillRepeatedly(testing::Return(0x20));
    }

    ~WPPBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmWorkLoadProfileStatus>
        makeStatusSensor(const std::string& path)
    {
        auto& bus = utils::DBusHandler::getBus();
        auto profileInfo = std::make_shared<OemProfileInfoIntf>(bus, path, gpu);
        return std::make_shared<NsmWorkLoadProfileStatus>(
            sensorName, sensorType, const_cast<std::string&>(invPath),
            profileInfo, nullptr);
    }
};

// ============================================================================
// NsmWorkloadPowerProfilePage::genRequestMsg encode fail
// ============================================================================

TEST_F(WPPBranch2Test, PageGenRequestMsg_EncodeFail_ReturnsNullopt)
{
    std::string colName = "col", colType = "NSM_WPP_COL";
    std::string inv = "/xyz/test/pgr_enc_fail";
    auto profileCol = std::make_shared<NsmWorkloadPowerProfileCollection>(
        colName, colType, inv, gpu);

    std::string pageColName = "pagecol", pageColType = "NSM_WPP_PCOL";
    auto pageCol = std::make_shared<NsmWorkloadPowerProfilePageCollection>(
        pageColName, pageColType, inv, gpu);

    std::vector<std::string> profileNames = {"P0"};
    std::string enumName = "mapper", enumType = "NSM_MAPPER";
    auto mapper = std::make_shared<NsmWorkLoadProfileEnum>(enumName, enumType,
                                                           profileNames);

    std::string pageName = "page0", pageType = "NSM_WPP_PAGE";
    NsmWorkloadPowerProfilePage page(pageName, pageType, inv, gpu, profileCol,
                                     pageCol, mapper, 0);

    // instanceId > NSM_INSTANCE_MAX triggers encode failure
    auto result = page.genRequestMsg(0x01, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// NsmWorkLoadProfileStatus::handleResponseMsg - cc==0, rc==0 FALSE branch
// (cc ? cc : rc when cc==NSM_SUCCESS returns rc==0)
// ============================================================================

TEST_F(WPPBranch2Test, HandleResponseMsg_Success_ReturnsCcFalseBranch)
{
    auto sensor =
        makeStatusSensor("/xyz/openbmc_project/inventory/b2_handle_ok");
    auto resp = makeWorkloadStatusResp2(NSM_SUCCESS, 0xFF);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    // cc==0 → cc ? cc : rc → returns rc (0). FALSE branch of ternary.
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmWorkLoadProfileStatus::updateReading - various data patterns
// ============================================================================

TEST_F(WPPBranch2Test, UpdateReading_AllZeros_UpdatesInterface)
{
    auto sensor =
        makeStatusSensor("/xyz/openbmc_project/inventory/b2_upd_zero");
    struct workload_power_profile_status data{};
    // All zeros
    EXPECT_NO_THROW(sensor->updateReading(&data));
}

TEST_F(WPPBranch2Test, UpdateReading_AllOnes_UpdatesInterface)
{
    auto sensor =
        makeStatusSensor("/xyz/openbmc_project/inventory/b2_upd_ones");
    struct workload_power_profile_status data{};
    for (int i = 0; i < 8; i++)
    {
        data.supported_profile_mask.fields[i].byte = 0xFF;
        data.requested_profile_maks.fields[i].byte = 0xFF;
        data.enforced_profile_mask.fields[i].byte = 0xFF;
    }
    EXPECT_NO_THROW(sensor->updateReading(&data));
}

TEST_F(WPPBranch2Test, UpdateReading_MixedPatterns_UpdatesInterface)
{
    auto sensor =
        makeStatusSensor("/xyz/openbmc_project/inventory/b2_upd_mixed");
    struct workload_power_profile_status data{};
    data.supported_profile_mask.fields[0].byte = 0x55;
    data.supported_profile_mask.fields[3].byte = 0xAA;
    data.requested_profile_maks.fields[1].byte = 0x0F;
    data.enforced_profile_mask.fields[7].byte = 0xF0;
    EXPECT_NO_THROW(sensor->updateReading(&data));
}

// ============================================================================
// NsmWorkloadPowerProfileCollection::updateSupportedProfile with valid obj
// ============================================================================

TEST_F(WPPBranch2Test, UpdateSupportedProfile_ValidObj_SetsProperties)
{
    std::string colName = "col_upd", colType = "NSM_WPP_COL";
    std::string inv = "/xyz/test/upd_valid_obj";
    auto profileCol = std::make_shared<NsmWorkloadPowerProfileCollection>(
        colName, colType, inv, gpu);

    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "TestProfile";
    auto profile = std::make_shared<OemWorkLoadPowerProfileIntf>(bus, inv, 42,
                                                                 pName, gpu);

    struct nsm_workload_power_profile_data data{};
    data.profile_id = 42;
    data.priority = 7;
    // Exercise the TRUE branch of if (obj)
    EXPECT_NO_THROW(profileCol->updateSupportedProfile(profile, &data));
}

// ============================================================================
// NsmWorkloadPowerProfilePage::handleResponseMsg - error CC (TRUE branch)
// ============================================================================

TEST_F(WPPBranch2Test, PageHandleResponseMsg_ErrorCC_ReturnsCc)
{
    std::string colName = "col", colType = "NSM_WPP_COL";
    std::string inv = "/xyz/test/page_err_cc";
    auto profileCol = std::make_shared<NsmWorkloadPowerProfileCollection>(
        colName, colType, inv, gpu);

    std::string pageColName = "pagecol", pageColType = "NSM_WPP_PCOL";
    auto pageCol = std::make_shared<NsmWorkloadPowerProfilePageCollection>(
        pageColName, pageColType, inv, gpu);

    std::vector<std::string> profileNames = {"P0"};
    std::string enumName = "mapper", enumType = "NSM_MAPPER";
    auto mapper = std::make_shared<NsmWorkLoadProfileEnum>(enumName, enumType,
                                                           profileNames);

    std::string pageName = "page0", pageType = "NSM_WPP_PAGE";
    auto page = std::make_shared<NsmWorkloadPowerProfilePage>(
        pageName, pageType, inv, gpu, profileCol, pageCol, mapper, 0);

    // Build non-success response with valid buffer size
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = page->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);

    pageCol->supportedPages.clear();
}

// ============================================================================
// NsmWorkloadPowerProfilePage::handleResponseMsg -
// nextIdentifier == 0 (no next page) with profiles
// ============================================================================

TEST_F(WPPBranch2Test, PageHandleResponseMsg_NoNextPage_SkipsPageCreation)
{
    std::string colName = "col", colType = "NSM_WPP_COL";
    std::string inv = "/xyz/test/page_no_next";
    auto profileCol = std::make_shared<NsmWorkloadPowerProfileCollection>(
        colName, colType, inv, gpu);

    std::string pageColName = "pagecol", pageColType = "NSM_WPP_PCOL";
    auto pageCol = std::make_shared<NsmWorkloadPowerProfilePageCollection>(
        pageColName, pageColType, inv, gpu);

    std::vector<std::string> profileNames;
    for (int i = 0; i < 20; i++)
        profileNames.push_back("Profile" + std::to_string(i));
    std::string enumName = "mapper", enumType = "NSM_MAPPER";
    auto mapper = std::make_shared<NsmWorkLoadProfileEnum>(enumName, enumType,
                                                           profileNames);

    std::string pageName = "page0", pageType = "NSM_WPP_PAGE";
    auto page = std::make_shared<NsmWorkloadPowerProfilePage>(
        pageName, pageType, inv, gpu, profileCol, pageCol, mapper, 0);

    // 3 profiles, nextId=0 → no next page
    auto resp = makeWorkloadInfoResp2(3, 0, 10);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = page->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    // nextIdentifier == 0, so no page 1 should exist
    EXPECT_FALSE(pageCol->hasPageId(1));

    pageCol->supportedPages.clear();
}

// ============================================================================
// NsmWorkLoadProfileEnum - edge cases
// ============================================================================

TEST(NsmWorkLoadProfileEnumBranch2, ToStringBoundary)
{
    std::string name = "BoundaryEnum";
    std::string type = "NSM_PE";
    std::vector<std::string> names = {"A", "B"};
    NsmWorkLoadProfileEnum pe(name, type, names);

    // Exact boundary
    EXPECT_EQ(pe.toString(1), "B");
    // Just past boundary
    EXPECT_EQ(pe.toString(2), "Unknown");
}

// ============================================================================
// NsmWorkloadPowerProfileCollection - getSupportedProfileById found with
// valid (non-null) entry
// ============================================================================

TEST_F(WPPBranch2Test, GetSupportedProfileById_ValidObj_ReturnsObj)
{
    std::string colName = "col_get", colType = "NSM_WPP_COL";
    std::string inv = "/xyz/test/get_valid";
    auto profileCol = std::make_shared<NsmWorkloadPowerProfileCollection>(
        colName, colType, inv, gpu);

    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "TestProfile";
    auto profile = std::make_shared<OemWorkLoadPowerProfileIntf>(bus, inv, 10,
                                                                 pName, gpu);
    profileCol->addSupportedProfile(10, profile);

    auto retrieved = profileCol->getSupportedProfileById(10);
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved, profile);
}

// ============================================================================
// NsmWorkloadPowerProfilePageCollection - getPageById found
// ============================================================================

TEST_F(WPPBranch2Test, GetPageById_Found_ReturnsPage)
{
    std::string colName = "col", colType = "NSM_WPP_COL";
    std::string inv = "/xyz/test/get_page_found";
    auto profileCol = std::make_shared<NsmWorkloadPowerProfileCollection>(
        colName, colType, inv, gpu);

    std::string pageColName = "pagecol", pageColType = "NSM_WPP_PCOL";
    auto pageCol = std::make_shared<NsmWorkloadPowerProfilePageCollection>(
        pageColName, pageColType, inv, gpu);

    std::vector<std::string> profileNames = {"P0"};
    std::string enumName = "mapper", enumType = "NSM_MAPPER";
    auto mapper = std::make_shared<NsmWorkLoadProfileEnum>(enumName, enumType,
                                                           profileNames);

    std::string pageName = "page5", pageType = "NSM_WPP_PAGE";
    auto page = std::make_shared<NsmWorkloadPowerProfilePage>(
        pageName, pageType, inv, gpu, profileCol, pageCol, mapper, 5);

    pageCol->addPage(5, page);

    auto retrieved = pageCol->getPageById(5);
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved, page);

    pageCol->supportedPages.clear();
}
