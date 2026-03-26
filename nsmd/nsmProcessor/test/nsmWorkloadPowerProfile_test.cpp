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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <xyz/openbmc_project/Common/error.hpp>

using namespace ::testing;

#define private public
#define protected public

#include "nsmDbusIfaceOverride/nsmWorkloadPowerProfileInfoIface.hpp"
#include "nsmWorkloadPowerProfile.hpp"

using namespace nsm;

static auto& getBus()
{
    static auto bus = sdbusplus::bus::new_default();
    return bus;
}

// =============================================================================
// NsmWorkLoadProfileEnum tests (existing, kept for completeness)
// =============================================================================

TEST(NsmWorkLoadProfileEnum, testConstructor)
{
    std::string name = "ProfileEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames = {"Performance", "Balanced",
                                             "PowerSaver"};

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    EXPECT_EQ(profileEnum.getName(), name);
    EXPECT_EQ(profileEnum.getType(), type);
}

TEST(NsmWorkLoadProfileEnum, testToString)
{
    std::string name = "ProfileEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames = {"Profile0", "Profile1",
                                             "Profile2"};

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    EXPECT_EQ(profileEnum.toString(0), "Profile0");
    EXPECT_EQ(profileEnum.toString(1), "Profile1");
    EXPECT_EQ(profileEnum.toString(2), "Profile2");
    EXPECT_EQ(profileEnum.toString(99), "Unknown");
}

TEST(NsmWorkLoadProfileEnum, testToEnum)
{
    std::string name = "ProfileEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames = {"High", "Medium", "Low"};

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    EXPECT_EQ(profileEnum.toEnum("High"), 0);
    EXPECT_EQ(profileEnum.toEnum("Medium"), 1);
    EXPECT_EQ(profileEnum.toEnum("Low"), 2);
    EXPECT_EQ(profileEnum.toEnum("Invalid"), -1);
}

TEST(NsmWorkLoadProfileEnum, testEmptyList)
{
    std::string name = "EmptyEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames; // Empty

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    EXPECT_EQ(profileEnum.toString(0), "Unknown");
    EXPECT_EQ(profileEnum.toEnum("Any"), -1);
}

TEST(NsmWorkLoadProfileEnum, testSingleProfile)
{
    std::string name = "SingleEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames = {"OnlyOne"};

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    EXPECT_EQ(profileEnum.toString(0), "OnlyOne");
    EXPECT_EQ(profileEnum.toEnum("OnlyOne"), 0);
    EXPECT_EQ(profileEnum.toString(1), "Unknown");
}

TEST(NsmWorkLoadProfileEnum, testManyProfiles)
{
    std::string name = "ManyEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames;
    for (int i = 0; i < 10; i++)
    {
        profileNames.push_back("Profile" + std::to_string(i));
    }

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    for (int i = 0; i < 10; i++)
    {
        EXPECT_EQ(profileEnum.toString(i), "Profile" + std::to_string(i));
        EXPECT_EQ(profileEnum.toEnum("Profile" + std::to_string(i)), i);
    }
}

TEST(NsmWorkLoadProfileEnum, testDuplicateStrings)
{
    std::string name = "DuplicateEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames = {"Same", "Same", "Different"};

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    // Last duplicate wins
    EXPECT_EQ(profileEnum.toEnum("Same"), 1);
    EXPECT_EQ(profileEnum.toEnum("Different"), 2);
}

TEST(NsmWorkLoadProfileEnum, testMultipleInstances)
{
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames1 = {"A", "B", "C"};
    std::vector<std::string> profileNames2 = {"X", "Y", "Z"};

    std::string name1 = "Enum1";
    std::string name2 = "Enum2";

    NsmWorkLoadProfileEnum enum1(name1, type, profileNames1);
    NsmWorkLoadProfileEnum enum2(name2, type, profileNames2);

    EXPECT_EQ(enum1.toString(0), "A");
    EXPECT_EQ(enum2.toString(0), "X");
    EXPECT_EQ(enum1.toEnum("A"), 0);
    EXPECT_EQ(enum2.toEnum("X"), 0);
}

// =============================================================================
// Fixture for tests using MockNsmDevice (coroutines / device I/O)
// =============================================================================

struct WorkloadProfileTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "992b3ec1-e464-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> device =
        std::make_shared<MockNsmDevice>(0, 0, "MCTP_UUID", gpuUuid, 0);
    NsmDeviceTable devices{{device}};

    // Unique D-Bus paths per test instance to avoid registration conflicts
    static int pathCounter;
    const std::string invPath;
    std::string dbusPath;

    WorkloadProfileTest() :
        SensorManagerTest(devices),
        invPath("/xyz/openbmc_project/inventory/system/processor/GPU" +
                std::to_string(pathCounter)),
        dbusPath("/xyz/openbmc_project/processor/GPU" +
                 std::to_string(pathCounter++) + "/workload")
    {
        // device may be held by asyncIntf (D-Bus object) past test teardown
        testing::Mock::AllowLeak(device.get());
    }

    ~WorkloadProfileTest()
    {
        cleanupDeviceSensors(devices);
    }

    // Build a valid enable/disable workload profile response message
    static std::vector<uint8_t> makeEnableResp(uint8_t cc = NSM_SUCCESS)
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_enable_workload_power_profile_resp(0, cc, ERR_NULL, msg);
        return buf;
    }

    static std::vector<uint8_t> makeDisableResp(uint8_t cc = NSM_SUCCESS)
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_disable_workload_power_profile_resp(0, cc, ERR_NULL, msg);
        return buf;
    }

    // Build a valid GetWorkloadPowerProfileStatus response
    static std::vector<uint8_t> makeStatusResp(uint8_t cc = NSM_SUCCESS)
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) +
                sizeof(nsm_get_workload_power_profile_status_info_resp),
            0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        workload_power_profile_status data{};
        data.supported_profile_mask.fields[0].byte = 0x01;
        encode_get_workload_power_profile_status_resp(0, cc, ERR_NULL, &data,
                                                      msg);
        return buf;
    }

    // Build a valid GetWorkloadPowerProfileInfo response (0 profiles, no next)
    static std::vector<uint8_t> makePageInfoResp(uint8_t numberOfProfiles = 0,
                                                 uint16_t nextIdentifier = 0,
                                                 uint8_t cc = NSM_SUCCESS)
    {
        nsm_all_workload_power_profile_meta_data meta{};
        meta.next_identifier = nextIdentifier;
        meta.number_of_profiles = numberOfProfiles;
        nsm_workload_power_profile_data profileData{};
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                sizeof(nsm_all_workload_power_profile_resp_meta_data) +
                numberOfProfiles * sizeof(nsm_workload_power_profile_data),
            0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_get_workload_power_profile_info_resp(
            0, cc, ERR_NULL, &meta, &profileData, numberOfProfiles, msg);
        return buf;
    }
};
int WorkloadProfileTest::pathCounter = 0;

// =============================================================================
// NsmWorkLoadProfileStatus tests
// =============================================================================

TEST_F(WorkloadProfileTest, ProfileStatus_Constructor)
{
    std::string name = "WorkloadStatus";
    std::string type = "NSM_WorkloadStatus";
    std::string invPathCopy = invPath;

    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    NsmWorkLoadProfileStatus sensor(name, type, invPathCopy, profileStatusInfo,
                                    asyncIntf);

    EXPECT_EQ(sensor.getName(), name);
    EXPECT_EQ(sensor.getType(), type);
}

// genRequestMsg: valid instanceId → returns non-nullopt
TEST_F(WorkloadProfileTest, ProfileStatus_GenRequestMsg_Success)
{
    std::string name = "WorkloadStatus";
    std::string type = "NSM_WorkloadStatus";
    std::string invPathCopy = invPath;

    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    NsmWorkLoadProfileStatus sensor(name, type, invPathCopy, profileStatusInfo,
                                    asyncIntf);

    auto result = sensor.genRequestMsg(5, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result->empty());
}

// genRequestMsg: instanceId > NSM_INSTANCE_MAX → encode fails → nullopt
TEST_F(WorkloadProfileTest, ProfileStatus_GenRequestMsg_EncodeFailure)
{
    std::string name = "WorkloadStatus";
    std::string type = "NSM_WorkloadStatus";
    std::string invPathCopy = invPath;

    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    NsmWorkLoadProfileStatus sensor(name, type, invPathCopy, profileStatusInfo,
                                    asyncIntf);

    // instanceId > NSM_INSTANCE_MAX (31) causes encode to fail
    auto result = sensor.genRequestMsg(5, 32);
    EXPECT_FALSE(result.has_value());
}

// handleResponseMsg: valid success response → updateReading called → cc=0
TEST_F(WorkloadProfileTest, ProfileStatus_HandleResponseMsg_Success)
{
    std::string name = "WorkloadStatus";
    std::string type = "NSM_WorkloadStatus";
    std::string invPathCopy = invPath;

    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    NsmWorkLoadProfileStatus sensor(name, type, invPathCopy, profileStatusInfo,
                                    asyncIntf);

    auto buf = makeStatusResp(NSM_SUCCESS);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    uint8_t rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// handleResponseMsg: bad CC → non-zero return, updateReading NOT called
TEST_F(WorkloadProfileTest, ProfileStatus_HandleResponseMsg_BadCC)
{
    std::string name = "WorkloadStatus";
    std::string type = "NSM_WorkloadStatus";
    std::string invPathCopy = invPath;

    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    NsmWorkLoadProfileStatus sensor(name, type, invPathCopy, profileStatusInfo,
                                    asyncIntf);

    // NSM_ERROR CC in properly-sized buffer → decode fails with non-success
    std::vector<uint8_t> badBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    badBuf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code
    auto* msg = reinterpret_cast<const nsm_msg*>(badBuf.data());
    uint8_t rc = sensor.handleResponseMsg(msg, badBuf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// updateReading: null data → early return (no crash)
TEST_F(WorkloadProfileTest, ProfileStatus_UpdateReading_NullData)
{
    std::string name = "WorkloadStatus";
    std::string type = "NSM_WorkloadStatus";
    std::string invPathCopy = invPath;

    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    NsmWorkLoadProfileStatus sensor(name, type, invPathCopy, profileStatusInfo,
                                    asyncIntf);

    EXPECT_NO_THROW(sensor.updateReading(nullptr));
}

// updateReading: valid data → fields updated
TEST_F(WorkloadProfileTest, ProfileStatus_UpdateReading_ValidData)
{
    std::string name = "WorkloadStatus";
    std::string type = "NSM_WorkloadStatus";
    std::string invPathCopy = invPath;

    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    NsmWorkLoadProfileStatus sensor(name, type, invPathCopy, profileStatusInfo,
                                    asyncIntf);

    workload_power_profile_status data{};
    data.supported_profile_mask.fields[0].byte = 0xFF;
    data.requested_profile_maks.fields[0].byte = 0x0F;
    data.enforced_profile_mask.fields[0].byte = 0x03;
    EXPECT_NO_THROW(sensor.updateReading(&data));
}

// =============================================================================
// NsmWorkloadPowerProfileCollection tests
// =============================================================================

TEST_F(WorkloadProfileTest, ProfileCollection_Constructor)
{
    std::string name = "ProfileCollection";
    std::string type = "NSM_ProfileCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfileCollection collection(name, type, invPathCopy,
                                                 device);
    EXPECT_EQ(collection.getName(), name);
    EXPECT_EQ(collection.getType(), type);
}

TEST_F(WorkloadProfileTest, ProfileCollection_HasProfileId_False)
{
    std::string name = "ProfileCollection";
    std::string type = "NSM_ProfileCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfileCollection collection(name, type, invPathCopy,
                                                 device);
    EXPECT_FALSE(collection.hasProfileId(42));
}

TEST_F(WorkloadProfileTest, ProfileCollection_HasProfileId_True)
{
    std::string name = "ProfileCollection";
    std::string type = "NSM_ProfileCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfileCollection collection(name, type, invPathCopy,
                                                 device);

    std::string profileName = "TestProfile";
    auto intf = std::make_shared<OemWorkLoadPowerProfileIntf>(
        getBus(), invPath, 42, profileName, device);
    collection.addSupportedProfile(42, intf);

    EXPECT_TRUE(collection.hasProfileId(42));
    EXPECT_FALSE(collection.hasProfileId(99));
}

TEST_F(WorkloadProfileTest, ProfileCollection_GetSupportedProfileById_Found)
{
    std::string name = "ProfileCollection";
    std::string type = "NSM_ProfileCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfileCollection collection(name, type, invPathCopy,
                                                 device);

    std::string profileName = "TestProfile";
    auto intf = std::make_shared<OemWorkLoadPowerProfileIntf>(
        getBus(), invPath, 7, profileName, device);
    collection.addSupportedProfile(7, intf);

    auto result = collection.getSupportedProfileById(7);
    EXPECT_EQ(result, intf);
}

TEST_F(WorkloadProfileTest,
       ProfileCollection_GetSupportedProfileById_NotFound_Throws)
{
    std::string name = "ProfileCollection";
    std::string type = "NSM_ProfileCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfileCollection collection(name, type, invPathCopy,
                                                 device);

    EXPECT_THROW(collection.getSupportedProfileById(999), std::out_of_range);
}

TEST_F(WorkloadProfileTest, ProfileCollection_UpdateSupportedProfile_ValidObj)
{
    std::string name = "ProfileCollection";
    std::string type = "NSM_ProfileCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfileCollection collection(name, type, invPathCopy,
                                                 device);

    std::string profileName = "TestProfile";
    auto intf = std::make_shared<OemWorkLoadPowerProfileIntf>(
        getBus(), invPath, 3, profileName, device);
    collection.addSupportedProfile(3, intf);

    nsm_workload_power_profile_data data{};
    data.profile_id = 3;
    data.priority = 10;
    data.conflict_mask.fields[0].byte = 0x01;

    // Should update fields without throwing
    EXPECT_NO_THROW(collection.updateSupportedProfile(intf, &data));
    EXPECT_EQ(intf->WorkLoadPowerProfileIntf::profileId(), 3);
    EXPECT_EQ(intf->WorkLoadPowerProfileIntf::priority(), 10);
}

TEST_F(WorkloadProfileTest, ProfileCollection_UpdateSupportedProfile_NullObj)
{
    std::string name = "ProfileCollection";
    std::string type = "NSM_ProfileCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfileCollection collection(name, type, invPathCopy,
                                                 device);

    nsm_workload_power_profile_data data{};
    data.profile_id = 5;

    // null obj → if (obj) block skipped → no crash
    EXPECT_NO_THROW(collection.updateSupportedProfile(nullptr, &data));
}

// =============================================================================
// NsmWorkloadPowerProfilePageCollection tests
// =============================================================================

TEST_F(WorkloadProfileTest, PageCollection_Constructor)
{
    std::string name = "PageCollection";
    std::string type = "NSM_PageCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfilePageCollection collection(name, type, invPathCopy,
                                                     device);
    EXPECT_EQ(collection.getName(), name);
}

TEST_F(WorkloadProfileTest, PageCollection_HasPageId_False)
{
    std::string name = "PageCollection";
    std::string type = "NSM_PageCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfilePageCollection collection(name, type, invPathCopy,
                                                     device);
    EXPECT_FALSE(collection.hasPageId(0));
}

TEST_F(WorkloadProfileTest, PageCollection_GetPageById_NotFound_ReturnsNull)
{
    std::string name = "PageCollection";
    std::string type = "NSM_PageCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfilePageCollection collection(name, type, invPathCopy,
                                                     device);
    auto result = collection.getPageById(0);
    EXPECT_EQ(result, nullptr);
}

TEST_F(WorkloadProfileTest, PageCollection_AddPage_New_ThenFound)
{
    std::string name = "PageCollection";
    std::string type = "NSM_PageCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfilePageCollection collection(name, type, invPathCopy,
                                                     device);

    // Build supporting objects for page
    std::string colName = "ProfCol";
    std::string colType = "T";
    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(
            colName, colType, invPathCopy, device);

    std::string mapName = "Mapper";
    std::string mapType = "T";
    std::vector<std::string> names = {"Prof0"};
    auto profileMapper =
        std::make_shared<NsmWorkLoadProfileEnum>(mapName, mapType, names);

    std::string pgName = "Page0";
    std::string pgType = "T";
    auto pagePtr = std::make_shared<NsmWorkloadPowerProfilePage>(
        pgName, pgType, invPathCopy, device, profileCollection,
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            name, type, invPathCopy, device),
        profileMapper, 0);

    EXPECT_FALSE(collection.hasPageId(0));
    collection.addPage(0, pagePtr);
    EXPECT_TRUE(collection.hasPageId(0));

    auto found = collection.getPageById(0);
    EXPECT_EQ(found, pagePtr);
}

TEST_F(WorkloadProfileTest, PageCollection_AddPage_Existing_Ignored)
{
    std::string name = "PageCollection";
    std::string type = "NSM_PageCollection";
    std::string invPathCopy = invPath;

    NsmWorkloadPowerProfilePageCollection collection(name, type, invPathCopy,
                                                     device);

    std::string colName = "ProfCol";
    std::string colType = "T";
    auto profileCollection =
        std::make_shared<NsmWorkloadPowerProfileCollection>(
            colName, colType, invPathCopy, device);

    std::string mapName = "Mapper";
    std::string mapType = "T";
    std::vector<std::string> names = {"Prof0"};
    auto profileMapper =
        std::make_shared<NsmWorkLoadProfileEnum>(mapName, mapType, names);

    auto makeInnerCollection = [&]() {
        return std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            name, type, invPathCopy, device);
    };

    std::string pgName = "Page0";
    std::string pgType = "T";
    auto page1 = std::make_shared<NsmWorkloadPowerProfilePage>(
        pgName, pgType, invPathCopy, device, profileCollection,
        makeInnerCollection(), profileMapper, 0);
    auto page2 = std::make_shared<NsmWorkloadPowerProfilePage>(
        pgName, pgType, invPathCopy, device, profileCollection,
        makeInnerCollection(), profileMapper, 0);

    collection.addPage(0, page1);
    // Adding again with same ID → else branch (already exists log)
    collection.addPage(0, page2);
    // First page is still stored
    EXPECT_EQ(collection.getPageById(0), page1);
}

// =============================================================================
// NsmWorkloadPowerProfilePage tests
// =============================================================================

struct WorkloadPageTest : public WorkloadProfileTest
{
    std::string colName = "ProfCol";
    std::string colType = "T";
    std::string mapName = "Mapper";
    std::string mapType = "T";
    std::string pgName = "Page0";
    std::string pgType = "T";
    std::string invPathCopy = invPath;

    std::shared_ptr<NsmWorkloadPowerProfileCollection> profileCollection;
    std::shared_ptr<NsmWorkloadPowerProfilePageCollection> pageCollection;
    std::shared_ptr<NsmWorkLoadProfileEnum> profileMapper;
    std::shared_ptr<NsmWorkloadPowerProfilePage> page;

    WorkloadPageTest()
    {
        profileCollection = std::make_shared<NsmWorkloadPowerProfileCollection>(
            colName, colType, invPathCopy, device);
        pageCollection =
            std::make_shared<NsmWorkloadPowerProfilePageCollection>(
                pgName, pgType, invPathCopy, device);
        std::vector<std::string> names = {"Prof0", "Prof1"};
        profileMapper =
            std::make_shared<NsmWorkLoadProfileEnum>(mapName, mapType, names);
        page = std::make_shared<NsmWorkloadPowerProfilePage>(
            pgName, pgType, invPathCopy, device, profileCollection,
            pageCollection, profileMapper, 0);
    }

    ~WorkloadPageTest()
    {
        // Break circular reference: pageCollection → added pages →
        // pageCollection
        pageCollection->supportedPages.clear();
    }
};

TEST_F(WorkloadPageTest, Page_Constructor)
{
    EXPECT_EQ(page->getName(), pgName);
}

// genRequestMsg: valid instanceId → success
TEST_F(WorkloadPageTest, Page_GenRequestMsg_Success)
{
    auto result = page->genRequestMsg(5, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result->empty());
}

// genRequestMsg: instanceId > NSM_INSTANCE_MAX → encode fails → nullopt
TEST_F(WorkloadPageTest, Page_GenRequestMsg_EncodeFailure)
{
    auto result = page->genRequestMsg(5, 32);
    EXPECT_FALSE(result.has_value());
}

// handleResponseMsg: truncated buffer → decode fails → non-zero cc
TEST_F(WorkloadPageTest, Page_HandleResponseMsg_DecodeFailure)
{
    std::vector<uint8_t> badBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    badBuf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<const nsm_msg*>(badBuf.data());
    uint8_t rc = page->handleResponseMsg(msg, badBuf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// handleResponseMsg: success response with 0 profiles, nextIdentifier=0
// → loop runs 0 times, next-page branch NOT taken
TEST_F(WorkloadPageTest, Page_HandleResponseMsg_ZeroProfiles_NoNextPage)
{
    auto buf = makePageInfoResp(0, 0, NSM_SUCCESS);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    uint8_t rc = page->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// handleResponseMsg: success with nextIdentifier > 0 → new page added
TEST_F(WorkloadPageTest, Page_HandleResponseMsg_NextIdentifier_NewPage)
{
    // Response with next_identifier=1 (nextPageId=1 not yet known)
    auto buf = makePageInfoResp(0, 1, NSM_SUCCESS);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    uint8_t rc = page->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    // New page 1 should have been added to pageCollection
    EXPECT_TRUE(pageCollection->hasPageId(1));
}

// handleResponseMsg: nextIdentifier > 0 but page already exists → else branch
TEST_F(WorkloadPageTest, Page_HandleResponseMsg_NextIdentifier_ExistingPage)
{
    // Pre-add page 1 to pageCollection
    std::string n = pgName;
    std::string t = pgType;
    std::string p = invPathCopy;
    auto existingPage = std::make_shared<NsmWorkloadPowerProfilePage>(
        n, t, p, device, profileCollection, pageCollection, profileMapper, 1);
    pageCollection->addPage(1, existingPage);

    auto buf = makePageInfoResp(0, 1, NSM_SUCCESS);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    // nextIdentifier=1 but page 1 already exists → hasPageId(1)=true → else
    uint8_t rc = page->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    // Still only the original page
    EXPECT_EQ(pageCollection->getPageById(1), existingPage);
}

// =============================================================================
// NsmWorkloadProfileInfoAsyncIntf tests
// =============================================================================

TEST_F(WorkloadProfileTest, AsyncIntf_Constructor)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);
    EXPECT_NE(asyncIntf, nullptr);
}

// enablePresetProfile: bytes.size() != 32 → throws InvalidArgument
TEST_F(WorkloadProfileTest, AsyncIntf_EnablePresetProfile_WrongSize_Throws)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    std::vector<uint8_t> bytes(16, 0); // wrong size
    EXPECT_THROW(
        asyncIntf->enablePresetProfile(bytes),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

// enablePresetProfile: bytes.size() == 32 → succeeds (returns object path)
TEST_F(WorkloadProfileTest, AsyncIntf_EnablePresetProfile_ValidSize_ReturnsPath)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    // Attach a workloadProfileStatusSensor so the coroutine won't crash
    std::string sensorName = "Status";
    std::string sensorType = "T";
    std::string invPathCopy = invPath;
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, invPathCopy, profileStatusInfo, asyncIntf);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    // postPatchIO returns a valid enable response (success CC)
    auto respBuf = makeEnableResp(NSM_SUCCESS);
    EXPECT_CALL(*device, postPatchIO).WillRepeatedly(mockPostPatchIO(respBuf));

    // sensorIO returns a valid status response for the update()
    auto statusBuf = makeStatusResp(NSM_SUCCESS);
    EXPECT_CALL(*device, sensorIO).WillRepeatedly(mockSensorIO(statusBuf));

    std::vector<uint8_t> bytes(32, 0);
    std::string path;
    EXPECT_NO_THROW(path = asyncIntf->enablePresetProfile(bytes));
    EXPECT_FALSE(path.empty());
}

// disablePresetProfile: bytes.size() != 32 → throws InvalidArgument
TEST_F(WorkloadProfileTest, AsyncIntf_DisablePresetProfile_WrongSize_Throws)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    std::vector<uint8_t> bytes(1, 0); // wrong size
    EXPECT_THROW(
        asyncIntf->disablePresetProfile(bytes),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

// disablePresetProfile: bytes.size() == 32 → returns path
TEST_F(WorkloadProfileTest,
       AsyncIntf_DisablePresetProfile_ValidSize_ReturnsPath)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    std::string sensorName = "Status";
    std::string sensorType = "T";
    std::string invPathCopy = invPath;
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, invPathCopy, profileStatusInfo, asyncIntf);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    auto respBuf = makeDisableResp(NSM_SUCCESS);
    EXPECT_CALL(*device, postPatchIO).WillRepeatedly(mockPostPatchIO(respBuf));

    auto statusBuf = makeStatusResp(NSM_SUCCESS);
    EXPECT_CALL(*device, sensorIO).WillRepeatedly(mockSensorIO(statusBuf));

    std::vector<uint8_t> bytes(32, 0);
    std::string path;
    EXPECT_NO_THROW(path = asyncIntf->disablePresetProfile(bytes));
    EXPECT_FALSE(path.empty());
}

// requestEnablePresetProfile: postPatchIO fails → WriteFailure
TEST_F(WorkloadProfileTest, RequestEnable_PostPatchIOFails_WriteFailure)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    std::string sensorName = "Status";
    std::string sensorType = "T";
    std::string invPathCopy = invPath;
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, invPathCopy, profileStatusInfo, asyncIntf);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    EXPECT_CALL(*device, postPatchIO)
        .WillOnce(mockPostPatchIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0);
    auto result = asyncIntf->requestEnablePresetProfile(&status, bytes);
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// requestEnablePresetProfile: postPatchIO succeeds but bad CC → WriteFailure
TEST_F(WorkloadProfileTest, RequestEnable_BadCC_WriteFailure)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    std::string sensorName = "Status";
    std::string sensorType = "T";
    std::string invPathCopy = invPath;
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, invPathCopy, profileStatusInfo, asyncIntf);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    // Return a response with error CC → decode succeeds but cc != NSM_SUCCESS
    auto respBuf = makeEnableResp(NSM_ERROR);
    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(respBuf));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0);
    auto result = asyncIntf->requestEnablePresetProfile(&status, bytes);
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// requestDisablePresetProfile: postPatchIO fails → WriteFailure
TEST_F(WorkloadProfileTest, RequestDisable_PostPatchIOFails_WriteFailure)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    std::string sensorName = "Status";
    std::string sensorType = "T";
    std::string invPathCopy = invPath;
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, invPathCopy, profileStatusInfo, asyncIntf);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    EXPECT_CALL(*device, postPatchIO)
        .WillOnce(mockPostPatchIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0);
    auto result = asyncIntf->requestDisablePresetProfile(&status, bytes);
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// requestDisablePresetProfile: postPatchIO succeeds but bad CC → WriteFailure
TEST_F(WorkloadProfileTest, RequestDisable_BadCC_WriteFailure)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    std::string sensorName = "Status";
    std::string sensorType = "T";
    std::string invPathCopy = invPath;
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, invPathCopy, profileStatusInfo, asyncIntf);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    auto respBuf = makeDisableResp(NSM_ERROR);
    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(respBuf));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0);
    auto result = asyncIntf->requestDisablePresetProfile(&status, bytes);
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// requestEnablePresetProfile: success path then statusSensor update fails
TEST_F(WorkloadProfileTest, RequestEnable_StatusUpdateFails_WriteFailure)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    std::string sensorName = "Status";
    std::string sensorType = "T";
    std::string invPathCopy = invPath;
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, invPathCopy, profileStatusInfo, asyncIntf);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    // postPatchIO returns success enable response
    auto enableBuf = makeEnableResp(NSM_SUCCESS);
    // sendRecvNsmMsg (for update()) returns truncated buffer → decode fails
    // → handleResponseMsg returns non-zero → update returns NSM_SW_ERROR
    std::vector<uint8_t> badStatusBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    badStatusBuf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(enableBuf));
    EXPECT_CALL(*device, sensorIO)
        .WillOnce(mockSensorIO(badStatusBuf, NSM_SUCCESS));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0);
    auto result = asyncIntf->requestEnablePresetProfile(&status, bytes);
    // update failed → WriteFailure
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// requestDisablePresetProfile: success path then statusSensor update fails
TEST_F(WorkloadProfileTest, RequestDisable_StatusUpdateFails_WriteFailure)
{
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        getBus(), dbusPath.c_str(), device);

    std::string sensorName = "Status";
    std::string sensorType = "T";
    std::string invPathCopy = invPath;
    auto profileStatusInfo =
        std::make_shared<OemProfileInfoIntf>(getBus(), invPath, device);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, invPathCopy, profileStatusInfo, asyncIntf);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    auto disableBuf = makeDisableResp(NSM_SUCCESS);
    std::vector<uint8_t> badStatusBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    badStatusBuf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(disableBuf));
    EXPECT_CALL(*device, sensorIO)
        .WillOnce(mockSensorIO(badStatusBuf, NSM_SUCCESS));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0);
    auto result = asyncIntf->requestDisablePresetProfile(&status, bytes);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
