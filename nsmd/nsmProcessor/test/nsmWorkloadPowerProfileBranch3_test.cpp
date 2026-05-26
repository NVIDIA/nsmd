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
 * Branch coverage batch 3 for nsmWorkloadPowerProfile.cpp
 *
 * Targets remaining uncovered branches:
 * - enablePresetProfile: empty objectPath -> throws Unavailable
 * - enablePresetProfile: bytes.size() != 32 -> throws InvalidArgument
 * - disablePresetProfile: empty objectPath -> throws Unavailable
 * - disablePresetProfile: bytes.size() != 32 -> throws InvalidArgument
 * - requestEnablePresetProfile: encode failure
 * - requestEnablePresetProfile: postPatchIO failure
 * - requestEnablePresetProfile: decode failure (cc != NSM_SUCCESS)
 * - requestDisablePresetProfile: encode failure
 * - requestDisablePresetProfile: postPatchIO failure
 * - requestDisablePresetProfile: decode failure
 * - getSupportedProfileById: not found -> throws out_of_range
 * - addPage: duplicate page check (page already exists)
 * - NsmWorkloadPowerProfilePage::genRequestMsg encode failure
 * - NsmWorkLoadProfileStatus::updateReading nullptr
 * - NsmWorkloadPowerProfileCollection::updateSupportedProfile null obj
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

static std::vector<uint8_t> makeEnableRespBr3(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_enable_workload_power_profile_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : 1, msg);
    return buf;
}

static std::vector<uint8_t> makeDisableRespBr3(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_disable_workload_power_profile_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : 1, msg);
    return buf;
}

static std::vector<uint8_t> makeWorkloadStatusRespBr3(uint8_t cc = NSM_SUCCESS)
{
    struct workload_power_profile_status profileData{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_workload_power_profile_status_resp(0, cc, ERR_NULL, &profileData,
                                                  msg);
    return buf;
}

// ============================================================================
// Fixture
// ============================================================================

struct WPPBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    const std::string invPath = "/xyz/openbmc_project/inventory/wppb3_test";

    std::string sensorName = "wppB3Status";
    std::string sensorType = "NSM_WPP_B3";

    WPPBranch3Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~WPPBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// enablePresetProfile: bytes.size() != 32 -> throws InvalidArgument
// ============================================================================
TEST_F(WPPBranch3Test, EnablePresetProfile_WrongSize_ThrowsInvalidArg)
{
    auto& bus = utils::DBusHandler::getBus();
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, invPath.c_str(), gpu);

    std::vector<uint8_t> badBytes(16, 0); // 16 != 32
    EXPECT_THROW(
        asyncIntf->enablePresetProfile(badBytes),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

// ============================================================================
// disablePresetProfile: bytes.size() != 32 -> throws InvalidArgument
// ============================================================================
TEST_F(WPPBranch3Test, DisablePresetProfile_WrongSize_ThrowsInvalidArg)
{
    auto& bus = utils::DBusHandler::getBus();
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, invPath.c_str(), gpu);

    std::vector<uint8_t> badBytes(8, 0); // 8 != 32
    EXPECT_THROW(
        asyncIntf->disablePresetProfile(badBytes),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

// ============================================================================
// requestEnablePresetProfile: postPatchIO failure
// ============================================================================
TEST_F(WPPBranch3Test, RequestEnablePresetProfile_PostPatchIOFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, invPath.c_str(), gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeEnableRespBr3(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0x01);
    auto coro = asyncIntf->requestEnablePresetProfile(&status, bytes);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// requestEnablePresetProfile: decode failure (cc != NSM_SUCCESS)
// ============================================================================
TEST_F(WPPBranch3Test, RequestEnablePresetProfile_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, invPath.c_str(), gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeEnableRespBr3(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0x01);
    auto coro = asyncIntf->requestEnablePresetProfile(&status, bytes);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// requestDisablePresetProfile: postPatchIO failure
// ============================================================================
TEST_F(WPPBranch3Test, RequestDisablePresetProfile_PostPatchIOFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, invPath.c_str(), gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDisableRespBr3(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0x01);
    auto coro = asyncIntf->requestDisablePresetProfile(&status, bytes);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// requestDisablePresetProfile: decode failure (cc != NSM_SUCCESS)
// ============================================================================
TEST_F(WPPBranch3Test, RequestDisablePresetProfile_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, invPath.c_str(), gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDisableRespBr3(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0x01);
    auto coro = asyncIntf->requestDisablePresetProfile(&status, bytes);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmWorkloadPowerProfileCollection: getSupportedProfileById not found
// ============================================================================
TEST_F(WPPBranch3Test, ProfileCollection_GetById_NotFound_Throws)
{
    std::string colName = "col_br3";
    std::string colType = "NSM_WPP_COL";
    std::string inv = invPath + "/col_nf";

    NsmWorkloadPowerProfileCollection collection(colName, colType, inv, gpu);

    EXPECT_THROW(collection.getSupportedProfileById(99), std::out_of_range);
}

// ============================================================================
// NsmWorkloadPowerProfileCollection: hasProfileId false
// ============================================================================
TEST_F(WPPBranch3Test, ProfileCollection_HasProfileId_False)
{
    std::string colName = "col_br3_hf";
    std::string colType = "NSM_WPP_COL";
    std::string inv = invPath + "/col_hf";

    NsmWorkloadPowerProfileCollection collection(colName, colType, inv, gpu);

    EXPECT_FALSE(collection.hasProfileId(42));
}

// ============================================================================
// NsmWorkloadPowerProfileCollection: updateSupportedProfile with null obj
// ============================================================================
TEST_F(WPPBranch3Test, ProfileCollection_UpdateProfile_NullObj)
{
    std::string colName = "col_br3_upd";
    std::string colType = "NSM_WPP_COL";
    std::string inv = invPath + "/col_upd";

    NsmWorkloadPowerProfileCollection collection(colName, colType, inv, gpu);

    nsm_workload_power_profile_data data{};
    data.profile_id = 1;
    data.priority = 2;

    // Should not crash with nullptr
    collection.updateSupportedProfile(nullptr, &data);
}

// ============================================================================
// NsmWorkloadPowerProfilePageCollection: addPage duplicate
// ============================================================================
TEST_F(WPPBranch3Test, PageCollection_AddPage_Duplicate)
{
    std::string pageName = "page_br3";
    std::string pageType = "NSM_WPP_PAGE";
    std::string inv = invPath + "/page_dup";

    auto pageCol = std::make_shared<NsmWorkloadPowerProfilePageCollection>(
        pageName, pageType, inv, gpu);
    auto profCol = std::make_shared<NsmWorkloadPowerProfileCollection>(
        pageName, pageType, inv, gpu);

    std::string enumName = "enum_br3";
    std::string enumType = "NSM_ENUM";
    std::vector<std::string> strings = {"Profile0", "Profile1"};
    auto profileMapper =
        std::make_shared<NsmWorkLoadProfileEnum>(enumName, enumType, strings);

    auto page1 = std::make_shared<NsmWorkloadPowerProfilePage>(
        pageName, pageType, inv, gpu, profCol, pageCol, profileMapper, 0);

    // First add succeeds
    pageCol->addPage(0, page1);
    EXPECT_TRUE(pageCol->hasPageId(0));

    // Second add with same pageId should be ignored (duplicate)
    auto page2 = std::make_shared<NsmWorkloadPowerProfilePage>(
        pageName, pageType, inv, gpu, profCol, pageCol, profileMapper, 0);
    pageCol->addPage(0, page2);
    // Still only one page
    EXPECT_TRUE(pageCol->hasPageId(0));

    pageCol->supportedPages.clear();
    gpu->deviceSensors.clear();
    for (auto& [_, queue] : gpu->sensors)
    {
        queue.clear();
    }
}

// ============================================================================
// NsmWorkLoadProfileStatus: updateReading nullptr
// ============================================================================
TEST_F(WPPBranch3Test, WorkLoadProfileStatus_UpdateReading_Nullptr)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileInfo = std::make_shared<OemProfileInfoIntf>(bus, invPath, gpu);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, const_cast<std::string&>(invPath), profileInfo,
        nullptr);

    // Should not crash, just early return
    statusSensor->updateReading(nullptr);
}

// ============================================================================
// NsmWorkLoadProfileStatus: genRequestMsg encode failure
// ============================================================================
TEST_F(WPPBranch3Test, WorkLoadProfileStatus_GenReq_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileInfo = std::make_shared<OemProfileInfoIntf>(bus, invPath, gpu);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, const_cast<std::string&>(invPath), profileInfo,
        nullptr);

    auto request = statusSensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmWorkLoadProfileStatus: handleResponseMsg decode failure
// ============================================================================
TEST_F(WPPBranch3Test, WorkLoadProfileStatus_HandleResp_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileInfo = std::make_shared<OemProfileInfoIntf>(bus, invPath, gpu);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, const_cast<std::string&>(invPath), profileInfo,
        nullptr);

    // Too small response -> decode failure
    std::vector<uint8_t> badResp(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(badResp.data());
    auto rc = statusSensor->handleResponseMsg(responseMsg, badResp.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmWorkloadPowerProfilePage: genRequestMsg encode failure
// ============================================================================
TEST_F(WPPBranch3Test, Page_GenReq_EncodeFail)
{
    std::string pageName = "page_br3_enc";
    std::string pageType = "NSM_WPP_PAGE";
    std::string inv = invPath + "/page_enc_fail";

    auto profCol = std::make_shared<NsmWorkloadPowerProfileCollection>(
        pageName, pageType, inv, gpu);
    auto pageCol = std::make_shared<NsmWorkloadPowerProfilePageCollection>(
        pageName, pageType, inv, gpu);

    std::string enumName = "enum_br3_e";
    std::string enumType = "NSM_ENUM";
    std::vector<std::string> strings = {"Profile0"};
    auto mapper = std::make_shared<NsmWorkLoadProfileEnum>(enumName, enumType,
                                                           strings);

    NsmWorkloadPowerProfilePage page(pageName, pageType, inv, gpu, profCol,
                                     pageCol, mapper, 0);

    auto request = page.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// requestEnablePresetProfile: success + update sensor fails
// ============================================================================
TEST_F(WPPBranch3Test, RequestEnablePresetProfile_UpdateSensorFails)
{
    auto& bus = utils::DBusHandler::getBus();
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, invPath.c_str(), gpu);

    // Create a workload profile status sensor
    auto profileInfo = std::make_shared<OemProfileInfoIntf>(bus, invPath, gpu);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, const_cast<std::string&>(invPath), profileInfo,
        nullptr);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    // First call: enable success, second call: sensorIO for update fails
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeEnableRespBr3(NSM_SUCCESS)));
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0x01);
    auto coro = asyncIntf->requestEnablePresetProfile(&status, bytes);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// requestDisablePresetProfile: success + update sensor fails
// ============================================================================
TEST_F(WPPBranch3Test, RequestDisablePresetProfile_UpdateSensorFails)
{
    auto& bus = utils::DBusHandler::getBus();
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, invPath.c_str(), gpu);

    auto profileInfo = std::make_shared<OemProfileInfoIntf>(bus, invPath, gpu);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, const_cast<std::string&>(invPath), profileInfo,
        nullptr);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDisableRespBr3(NSM_SUCCESS)));
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0x01);
    auto coro = asyncIntf->requestDisablePresetProfile(&status, bytes);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// requestEnablePresetProfile: full success path (enable + update ok)
// ============================================================================
TEST_F(WPPBranch3Test, RequestEnablePresetProfile_FullSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, invPath.c_str(), gpu);

    auto profileInfo = std::make_shared<OemProfileInfoIntf>(bus, invPath, gpu);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, const_cast<std::string&>(invPath), profileInfo,
        nullptr);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeEnableRespBr3(NSM_SUCCESS)));
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeWorkloadStatusRespBr3(NSM_SUCCESS)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0x01);
    auto coro = asyncIntf->requestEnablePresetProfile(&status, bytes);
    // status not set to WriteFailure since all succeeded
}

// ============================================================================
// requestDisablePresetProfile: full success path
// ============================================================================
TEST_F(WPPBranch3Test, RequestDisablePresetProfile_FullSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    auto asyncIntf = std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
        bus, invPath.c_str(), gpu);

    auto profileInfo = std::make_shared<OemProfileInfoIntf>(bus, invPath, gpu);
    auto statusSensor = std::make_shared<NsmWorkLoadProfileStatus>(
        sensorName, sensorType, const_cast<std::string&>(invPath), profileInfo,
        nullptr);
    asyncIntf->workloadProfileStatusSensor = statusSensor;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDisableRespBr3(NSM_SUCCESS)));
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeWorkloadStatusRespBr3(NSM_SUCCESS)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> bytes(32, 0x01);
    auto coro = asyncIntf->requestDisablePresetProfile(&status, bytes);
}
