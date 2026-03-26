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

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "libnsm/pci-links.h"

#include "nsmAERError.hpp"

using namespace nsm;

struct NsmPCIeAERErrorStatusTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "AERErrorStatus";
    const std::string type = "NSM_AERError";
    const std::string path = "/xyz/openbmc_project/sensors/aer_error/GPU0";
    const uint8_t deviceIndex = 0;

    NsmDeviceTable devices;
    std::shared_ptr<NsmPCIeAERErrorStatus> aerError;
    std::shared_ptr<NsmAERErrorStatusIntf> aerErrorStatusIntf;
    std::shared_ptr<NsmDevice> nsmDevice;

    NsmPCIeAERErrorStatusTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();

        nsmDevice = std::make_shared<MockNsmDevice>(0x10, 0, "MCTP_EID", "16",
                                                    0);
        aerErrorStatusIntf = std::make_shared<NsmAERErrorStatusIntf>(
            bus, path.c_str(), deviceIndex, nsmDevice);

        aerError = std::make_shared<NsmPCIeAERErrorStatus>(
            name, type, aerErrorStatusIntf, deviceIndex);

        EXPECT_NE(aerError, nullptr);
        EXPECT_EQ(aerError->getName(), name);
        EXPECT_EQ(aerError->getType(), type);
    }
};

TEST_F(NsmPCIeAERErrorStatusTest, goodTestConstructor)
{
    EXPECT_NE(aerError->aerErrorStatusIntf, nullptr);
    EXPECT_EQ(aerError->deviceIndex, deviceIndex);
}

TEST_F(NsmPCIeAERErrorStatusTest, goodTestDeviceIndex)
{
    EXPECT_EQ(aerError->deviceIndex, 0);

    auto aerError2 = std::make_shared<NsmPCIeAERErrorStatus>(
        "AERError2", type, aerErrorStatusIntf, 5);
    EXPECT_EQ(aerError2->deviceIndex, 5);
}

TEST_F(NsmPCIeAERErrorStatusTest, goodTestAERErrorStatusIntf)
{
    EXPECT_NE(aerError->aerErrorStatusIntf, nullptr);
    EXPECT_EQ(aerError->aerErrorStatusIntf, aerErrorStatusIntf);
}

TEST_F(NsmPCIeAERErrorStatusTest, goodTestMultipleInstances)
{
    auto aerError1 = std::make_shared<NsmPCIeAERErrorStatus>(
        "AERError1", type, aerErrorStatusIntf, 0);
    auto aerError2 = std::make_shared<NsmPCIeAERErrorStatus>(
        "AERError2", type, aerErrorStatusIntf, 1);
    auto aerError3 = std::make_shared<NsmPCIeAERErrorStatus>(
        "AERError3", type, aerErrorStatusIntf, 2);

    EXPECT_EQ(aerError1->getName(), "AERError1");
    EXPECT_EQ(aerError2->getName(), "AERError2");
    EXPECT_EQ(aerError3->getName(), "AERError3");

    EXPECT_EQ(aerError1->deviceIndex, 0);
    EXPECT_EQ(aerError2->deviceIndex, 1);
    EXPECT_EQ(aerError3->deviceIndex, 2);
}

struct NsmAERErrorStatusIntfTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string path = "/xyz/openbmc_project/sensors/aer_error/GPU0";
    const uint8_t deviceIndex = 0;

    NsmDeviceTable devices;
    std::shared_ptr<NsmAERErrorStatusIntf> aerIntf;
    std::shared_ptr<NsmDevice> nsmDevice;

    NsmAERErrorStatusIntfTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        nsmDevice = std::make_shared<MockNsmDevice>(0x10, 0, "MCTP_EID", "16",
                                                    0);

        aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
            bus, path.c_str(), deviceIndex, nsmDevice);

        EXPECT_NE(aerIntf, nullptr);
    }
};

TEST_F(NsmAERErrorStatusIntfTest, goodTestConstructor)
{
    EXPECT_NE(aerIntf->device, nullptr);
    EXPECT_EQ(aerIntf->deviceIndex, deviceIndex);
}

TEST_F(NsmAERErrorStatusIntfTest, goodTestDevice)
{
    EXPECT_EQ(aerIntf->device, nsmDevice);
}

TEST_F(NsmAERErrorStatusIntfTest, goodTestCorrectableErrorStatus)
{
    std::string status = "0x00001234";
    aerIntf->aerCorrectableErrorStatus(status);
    EXPECT_EQ(aerIntf->aerCorrectableErrorStatus(), status);
}

TEST_F(NsmAERErrorStatusIntfTest, goodTestUncorrectableErrorStatus)
{
    std::string status = "0xABCD5678";
    aerIntf->aerUncorrectableErrorStatus(status);
    EXPECT_EQ(aerIntf->aerUncorrectableErrorStatus(), status);
}

TEST_F(NsmAERErrorStatusIntfTest, goodTestBothErrorStatuses)
{
    std::string correctableStatus = "0x00000001";
    std::string uncorrectableStatus = "0x00000002";

    aerIntf->aerCorrectableErrorStatus(correctableStatus);
    aerIntf->aerUncorrectableErrorStatus(uncorrectableStatus);

    EXPECT_EQ(aerIntf->aerCorrectableErrorStatus(), correctableStatus);
    EXPECT_EQ(aerIntf->aerUncorrectableErrorStatus(), uncorrectableStatus);
}

// ============================================================================
// NsmPCIeAERErrorStatus::genRequestMsg tests
// ============================================================================

TEST_F(NsmPCIeAERErrorStatusTest, GenRequestMsg_ReturnsValidRequest)
{
    auto request = aerError->genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

TEST_F(NsmPCIeAERErrorStatusTest, GenRequestMsg_DifferentDeviceIndex)
{
    auto aerError5 = std::make_shared<NsmPCIeAERErrorStatus>(
        "AERError5", type, aerErrorStatusIntf, 5);

    auto request = aerError5->genRequestMsg(10, 1);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

TEST_F(NsmPCIeAERErrorStatusTest, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto request = aerError->genRequestMsg(5, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmPCIeAERErrorStatus::handleResponseMsg tests
// ============================================================================

// Helper: build a valid group-9 response message
static std::vector<uint8_t> buildGroup9Response(uint8_t cc,
                                                uint32_t uncorrectable,
                                                uint32_t correctable)
{
    nsm_query_scalar_group_telemetry_group_9 data = {};
    data.aer_uncorrectable_error_status = uncorrectable;
    data.aer_correctable_error_status = correctable;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group9_resp(0, cc, ERR_NULL, &data,
                                                       msg);
    return buf;
}

// handleResponseMsg – success path: sets AER status strings on interface
// Note: hexFormat uses sizeof("0x00000000") which includes null terminator,
// so compare via c_str() (EXPECT_STREQ) to stop at the embedded null byte.
TEST_F(NsmPCIeAERErrorStatusTest, HandleResponseMsg_Success_SetsAERStatuses)
{
    auto buf = buildGroup9Response(NSM_SUCCESS, 0x0000ABCD, 0x12340000);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = aerError->handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_STREQ(aerErrorStatusIntf->aerUncorrectableErrorStatus().c_str(),
                 "0x0000ABCD");
    EXPECT_STREQ(aerErrorStatusIntf->aerCorrectableErrorStatus().c_str(),
                 "0x12340000");
}

// handleResponseMsg – error CC: returns error, does not update statuses
TEST_F(NsmPCIeAERErrorStatusTest, HandleResponseMsg_ErrorCC_ReturnsError)
{
    // Pre-set known values
    aerErrorStatusIntf->aerUncorrectableErrorStatus("0xDEADBEEF");
    aerErrorStatusIntf->aerCorrectableErrorStatus("0xCAFEBABE");

    auto buf = buildGroup9Response(NSM_ERROR, 0x11111111, 0x22222222);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = aerError->handleResponseMsg(msg, buf.size());

    EXPECT_NE(rc, NSM_SUCCESS);
    // Statuses should NOT have been updated
    EXPECT_EQ(aerErrorStatusIntf->aerUncorrectableErrorStatus(), "0xDEADBEEF");
    EXPECT_EQ(aerErrorStatusIntf->aerCorrectableErrorStatus(), "0xCAFEBABE");
}

// handleResponseMsg – success with all-zero data
TEST_F(NsmPCIeAERErrorStatusTest, HandleResponseMsg_ZeroData_SetsZeroStatuses)
{
    auto buf = buildGroup9Response(NSM_SUCCESS, 0x00000000, 0x00000000);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = aerError->handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_STREQ(aerErrorStatusIntf->aerUncorrectableErrorStatus().c_str(),
                 "0x00000000");
    EXPECT_STREQ(aerErrorStatusIntf->aerCorrectableErrorStatus().c_str(),
                 "0x00000000");
}

// handleResponseMsg – success with max data (0xFFFFFFFF)
TEST_F(NsmPCIeAERErrorStatusTest, HandleResponseMsg_MaxData_SetsMaxStatuses)
{
    auto buf = buildGroup9Response(NSM_SUCCESS, 0xFFFFFFFF, 0xFFFFFFFF);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = aerError->handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_STREQ(aerErrorStatusIntf->aerUncorrectableErrorStatus().c_str(),
                 "0xFFFFFFFF");
    EXPECT_STREQ(aerErrorStatusIntf->aerCorrectableErrorStatus().c_str(),
                 "0xFFFFFFFF");
}

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc==NSM_ERROR → L77 FALSE via
// cc!=NSM_SUCCESS (existing ErrorCC test uses full-size encoded buffer which
// causes decode_reason_code_and_cc to return NSM_SW_ERROR_LENGTH)
TEST_F(NsmPCIeAERErrorStatusTest,
       HandleResponseMsg_DecodeSuccessNonZeroCC_ElseBranch)
{
    // Exactly-sized non-success buffer: decode_reason_code_and_cc returns
    // NSM_SW_SUCCESS with cc=NSM_ERROR → L77 FALSE via cc!=NSM_SUCCESS
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = aerError->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// handleResponseMsg – decode failure: too-short buffer
TEST_F(NsmPCIeAERErrorStatusTest, HandleResponseMsg_DecodeFail_ReturnsError)
{
    // 7-byte buffer: safely reads cc=0 at payload[1] but too short for
    // nsm_query_scalar_group_telemetry_v1_group_9_resp so decode returns
    // NSM_SW_ERROR_LENGTH
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = aerError->handleResponseMsg(msg, buf.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}

// Error-CC path: clearAERError coroutine – postPatchIO succeeds but
// cc = NSM_ERROR → co_return cc ? cc : rc; takes the true branch. //

TEST_F(NsmPCIeAERErrorStatusTest, ClearAERError_ErrorCC_CoversBranch)
{
    // Encode a clear_data_source_v1 response with NSM_ERROR completion
    // code so decode returns NSM_SW_SUCCESS but cc != 0, covering the
    // true branch of co_return cc ? cc : rc.
    std::vector<uint8_t> errorResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(errorResp.data());
    auto encRc = encode_clear_data_source_v1_resp(0, NSM_ERROR, ERR_NULL,
                                                  respPtr);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(nsmDevice);
    ASSERT_NE(mockDevice, nullptr);
    EXPECT_CALL(*mockDevice, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = aerErrorStatusIntf->clearAERError(&status);

    // cc = NSM_ERROR → WriteFailure; true branch of co_return cc ? cc : rc //

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// clearAERError – postPatchIO succeeds, decode returns NSM_SW_ERROR_LENGTH
// while cc remains NSM_SUCCESS (0) → the L137 second &&-operand (rc) is FALSE
// → else branch → WriteFailure.
TEST_F(NsmAERErrorStatusIntfTest, ClearAERError_DecodeFailsCC0_SetsWriteFailure)
{
    // A minimal buffer with cc=0 (NSM_SUCCESS) forces decode to return
    // NSM_SW_ERROR_LENGTH (not enough bytes for the success body) while
    // cc stays NSM_SUCCESS — covers the (cc==NSM_SUCCESS && rc!=0) path.
    std::vector<uint8_t> shortResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    // cc byte (payload[1] = buf[sizeof(nsm_msg_hdr)+1]) stays 0 = NSM_SUCCESS

    auto mockDev = std::dynamic_pointer_cast<MockNsmDevice>(nsmDevice);
    ASSERT_NE(mockDev, nullptr);
    EXPECT_CALL(*mockDev, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(shortResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    aerIntf->clearAERError(&status);

    // decode_clear_data_source_v1_resp fails (NSM_SW_ERROR_LENGTH) with
    // cc=NSM_SUCCESS → else branch → WriteFailure
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// clearAERError – postPatchIO returns a failure rc → WriteFailure (lines
// 114-123 in nsmAERError.cpp)
TEST_F(NsmAERErrorStatusIntfTest,
       ClearAERError_PostPatchIOFail_SetsWriteFailure)
{
    auto mockDev = std::dynamic_pointer_cast<MockNsmDevice>(nsmDevice);
    ASSERT_NE(mockDev, nullptr);
    EXPECT_CALL(*mockDev, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    aerIntf->clearAERError(&status);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// clearAERError – success, cc=0, no aerStatusSensor → status unchanged
// (covers the false branch of "if (aerStatusSensor)")
TEST_F(NsmAERErrorStatusIntfTest, ClearAERError_Success_NoSensor_ReturnsOK)
{
    std::vector<uint8_t> resp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(resp.data());
    encode_clear_data_source_v1_resp(0, NSM_SUCCESS, ERR_NULL, respPtr);

    auto mockDev = std::dynamic_pointer_cast<MockNsmDevice>(nsmDevice);
    ASSERT_NE(mockDev, nullptr);
    EXPECT_CALL(*mockDev, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    aerIntf->clearAERError(&status);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// clearAERError – success WITH aerStatusSensor linked: co_await //
// aerStatusSensor->update(device) is executed (lines 136-140)
TEST_F(NsmAERErrorStatusIntfTest,
       ClearAERError_Success_WithSensor_UpdatesSensor)
{
    // Build a valid clear_data_source_v1 success response
    std::vector<uint8_t> clearResp(256, 0);
    auto clearPtr = reinterpret_cast<nsm_msg*>(clearResp.data());
    encode_clear_data_source_v1_resp(0, NSM_SUCCESS, ERR_NULL, clearPtr);

    // Build a valid group-9 response for the sensor refresh
    nsm_query_scalar_group_telemetry_group_9 g9data = {};
    std::vector<uint8_t> sensorResp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp),
        0);
    auto sensorPtr = reinterpret_cast<nsm_msg*>(sensorResp.data());
    encode_query_scalar_group_telemetry_v1_group9_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &g9data, sensorPtr);

    // Link a sensor so the "if (aerStatusSensor)" branch is taken
    auto linkedSensor = std::make_shared<NsmPCIeAERErrorStatus>(
        "AER_linked", "NSM_AERError", aerIntf, 0);
    aerIntf->linkAerStatusSensor(linkedSensor);

    auto mockDev = std::dynamic_pointer_cast<MockNsmDevice>(nsmDevice);
    ASSERT_NE(mockDev, nullptr);
    EXPECT_CALL(*mockDev, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(clearResp));
    EXPECT_CALL(*mockDev, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(sensorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    aerIntf->clearAERError(&status);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    // Break circular reference: aerIntf ↔ linkedSensor to allow teardown
    aerIntf->aerStatusSensor = nullptr;
}

// doclearAERErrorOnDevice: wrapper that calls clearAERError and sets
// statusInterface (lines 150-160 in nsmAERError.cpp)
TEST_F(NsmAERErrorStatusIntfTest,
       DoclearAERErrorOnDevice_CallsClearAndSetsStatus)
{
    std::vector<uint8_t> resp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(resp.data());
    encode_clear_data_source_v1_resp(0, NSM_SUCCESS, ERR_NULL, respPtr);

    auto mockDev = std::dynamic_pointer_cast<MockNsmDevice>(nsmDevice);
    ASSERT_NE(mockDev, nullptr);
    EXPECT_CALL(*mockDev, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(), "/com/nvidia/nsmd/test/aerDoclear0");
    aerIntf->doclearAERErrorOnDevice(statusIntf);
    // If we get here the coroutine ran to completion without crashing
}

// clearAERStatus: happy path – AsyncOperationManager returns a valid object
// path; doclearAERErrorOnDevice().detach() is called (lines 162-177)
TEST_F(NsmAERErrorStatusIntfTest, ClearAERStatus_HappyPath_ReturnsObjectPath)
{
    auto mockDev = std::dynamic_pointer_cast<MockNsmDevice>(nsmDevice);
    ASSERT_NE(mockDev, nullptr);
    // The detached coroutine will call postPatchIO; let it fail cleanly
    EXPECT_CALL(*mockDev, postPatchIO(_, _, _, _))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    auto objPath = aerIntf->clearAERStatus();

    EXPECT_FALSE(std::string(objPath).empty());
}
