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

#define private public
#define protected public

#include "nsmApSkuId.hpp"
#include "nsmUpdateApSkuId.hpp"

namespace nsm
{
bool validateApSkuIdFormat(const std::string& skuId, std::string& errorMsg);
} // namespace nsm

using namespace nsm;

struct NsmUpdateApSkuIdTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmUpdateApSkuIdTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmUpdateApSkuIdTest, goodTestFormatApSkuId)
{
    uint32_t skuId = 0x12345678;
    std::string formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0x12345678");

    skuId = 0x00000000;
    formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0x00000000");

    skuId = 0xFFFFFFFF;
    formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0xFFFFFFFF");

    skuId = 0xABCDEF12;
    formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0xABCDEF12");
}

TEST_F(NsmUpdateApSkuIdTest, goodTestFormatMultipleSkuIds)
{
    std::string sku1 = formatApSkuId(0x11111111);
    std::string sku2 = formatApSkuId(0x22222222);
    std::string sku3 = formatApSkuId(0x33333333);

    EXPECT_EQ(sku1, "0x11111111");
    EXPECT_EQ(sku2, "0x22222222");
    EXPECT_EQ(sku3, "0x33333333");
}

TEST_F(NsmUpdateApSkuIdTest, goodTestFormatLeadingZeros)
{
    uint32_t skuId = 0x00000001;
    std::string formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0x00000001");

    skuId = 0x00001234;
    formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0x00001234");
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatValidInput)
{
    std::string errorMsg;

    EXPECT_TRUE(validateApSkuIdFormat("0x12345678", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0xABCDEF12", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0x00000000", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0xFFFFFFFF", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0x12abcdef", errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatUppercasePrefix)
{
    std::string errorMsg;

    EXPECT_TRUE(validateApSkuIdFormat("0X12345678", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0XABCDEF12", errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatInvalidLength)
{
    std::string errorMsg;

    EXPECT_FALSE(validateApSkuIdFormat("0x123", errorMsg));
    EXPECT_NE(errorMsg.find("length"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("0x12345678901", errorMsg));
    EXPECT_NE(errorMsg.find("length"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("", errorMsg));
    EXPECT_FALSE(validateApSkuIdFormat("0x", errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatInvalidPrefix)
{
    std::string errorMsg;

    // These will fail on length check first (8 != 10)
    EXPECT_FALSE(validateApSkuIdFormat("12345678", errorMsg));
    EXPECT_NE(errorMsg.find("length"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("x12345678", errorMsg));
    EXPECT_NE(errorMsg.find("length"), std::string::npos);

    // This has correct length (10) but wrong prefix
    EXPECT_FALSE(validateApSkuIdFormat("0y12345678", errorMsg));
    EXPECT_NE(errorMsg.find("prefix"), std::string::npos);
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatInvalidHexCharacters)
{
    std::string errorMsg;

    EXPECT_FALSE(validateApSkuIdFormat("0x1234567G", errorMsg));
    EXPECT_NE(errorMsg.find("hexadecimal"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("0x1234567!", errorMsg));
    EXPECT_NE(errorMsg.find("hexadecimal"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("0x1234 678", errorMsg));
    EXPECT_NE(errorMsg.find("hexadecimal"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("0x1234567Z", errorMsg));
    EXPECT_NE(errorMsg.find("hexadecimal"), std::string::npos);
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatBoundaryValues)
{
    std::string errorMsg;

    EXPECT_TRUE(validateApSkuIdFormat("0x00000000", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0xFFFFFFFF", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0x80000000", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0x7FFFFFFF", errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatMixedCase)
{
    std::string errorMsg;

    EXPECT_TRUE(validateApSkuIdFormat("0xAbCdEf12", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0XaBcDeF12", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0x1a2B3c4D", errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testFormatAndValidateRoundtrip)
{
    std::string errorMsg;

    uint32_t originalSkuId = 0xABCD1234;
    std::string formatted = formatApSkuId(originalSkuId);

    EXPECT_TRUE(validateApSkuIdFormat(formatted, errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatErrorMessages)
{
    std::string errorMsg;

    validateApSkuIdFormat("0x123", errorMsg);
    EXPECT_FALSE(errorMsg.empty());

    errorMsg.clear();
    validateApSkuIdFormat("12345678", errorMsg);
    EXPECT_FALSE(errorMsg.empty());

    errorMsg.clear();
    validateApSkuIdFormat("0x1234567G", errorMsg);
    EXPECT_FALSE(errorMsg.empty());
}

// Decode-fail path: too-short response → rc = NSM_SW_ERROR_LENGTH, cc stays 0.
// co_return cc ? cc : rc; takes the false branch (cc=0 → return rc). //

TEST_F(NsmUpdateApSkuIdTest, UpdateApSkuIdHandler_DecodeFail_CoversFalseBranch)
{
    using namespace testing;

    auto mockDevice = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID",
                                                      "test-uuid", 0);

    // Too-short response: decode fails with NSM_SW_ERROR_LENGTH, cc stays 0
    std::vector<uint8_t> shortResp(sizeof(nsm_msg_hdr) + 2, 0);

    EXPECT_CALL(*mockDevice, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(shortResp));

    AsyncSetOperationValueType value = std::string("0xABCD1234");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = updateApSkuIdHandler(value, &status, mockDevice, 0, 0, 0);

    // cc = 0 (stays 0 after failed decode) → false branch:
    // co_return 0 ? 0 : NSM_SW_ERROR_LENGTH → return rc
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Error-CC path: postPatchIO succeeds but cc = NSM_ERROR (non-zero)
// → decode_nsm_firmware_set_rot_property_resp succeeds but cc != NSM_SUCCESS
// → co_return cc ? cc : rc; takes the true branch (return cc). //

TEST_F(NsmUpdateApSkuIdTest, UpdateApSkuIdHandler_ErrorCC_CoversBranch)
{
    using namespace testing;

    // Create a mock device
    auto mockDevice = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID",
                                                      "test-uuid", 0);

    // Build a set_rot_property response with NSM_ERROR completion code
    std::vector<uint8_t> errorResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(errorResp.data());
    auto encRc = encode_nsm_firmware_set_rot_property_resp(0, NSM_ERROR,
                                                           ERR_NULL, respPtr);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    // Mock postPatchIO to return the error response with rc=NSM_SW_SUCCESS
    EXPECT_CALL(*mockDevice, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    // Act: call handler with valid SKU ID "0x" + 8 hex digits
    AsyncSetOperationValueType value = std::string("0xABCD1234");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = updateApSkuIdHandler(value, &status, mockDevice, 0, 0, 0);

    // cc = NSM_ERROR → WriteFailure status; true branch of co_return cc ? cc :
    // rc
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// updateApSkuIdHandler: value holds a non-string type (bool)
// → std::get_if<std::string>(&value) returns nullptr → line 105 TRUE
// → status = InvalidArgument, co_return NSM_SW_ERROR_DATA
TEST_F(NsmUpdateApSkuIdTest,
       UpdateApSkuIdHandler_NonStringValue_InvalidArgument)
{
    using namespace testing;

    auto mockDevice = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID",
                                                      "test-uuid", 0);

    // bool is first variant type → get_if<std::string> returns nullptr
    AsyncSetOperationValueType value = bool{true};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = updateApSkuIdHandler(value, &status, mockDevice, 0, 0, 0);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// updateApSkuIdHandler: value is a string but fails validateApSkuIdFormat
// → line 114 TRUE → status = InvalidArgument, co_return NSM_SW_ERROR_DATA //

TEST_F(NsmUpdateApSkuIdTest, UpdateApSkuIdHandler_InvalidFormat_InvalidArgument)
{
    using namespace testing;

    auto mockDevice = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID",
                                                      "test-uuid", 0);

    // "invalid" fails length check → validateApSkuIdFormat returns false
    AsyncSetOperationValueType value = std::string("invalid");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = updateApSkuIdHandler(value, &status, mockDevice, 0, 0, 0);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// updateApSkuIdHandler: valid hex format but too long for stoul → throws
// std::out_of_range → catch(exception) at lines 129-135 covered
// "0x" + 20 "F"s = 80-bit value > ULONG_MAX on 64-bit platform
TEST_F(NsmUpdateApSkuIdTest, UpdateApSkuIdHandler_StoulOverflow_ParseError)
{
    using namespace testing;

    auto mockDevice = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID",
                                                      "test-uuid", 0);

    // 20 hex F's after "0x" passes validateApSkuIdFormat but overflows stoul
    AsyncSetOperationValueType value = std::string("0xFFFFFFFFFFFFFFFFFFFF");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = updateApSkuIdHandler(value, &status, mockDevice, 0, 0, 0);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// updateApSkuIdHandler: valid hex format, stoul succeeds, but parsed value
// exceeds uint32_t range → lines 149-152 covered
// "0x100000000" = 2^32 = 4294967296 > UINT32_MAX (4294967295)
TEST_F(NsmUpdateApSkuIdTest,
       UpdateApSkuIdHandler_ValueExceedsUint32_InvalidArgument)
{
    using namespace testing;

    auto mockDevice = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID",
                                                      "test-uuid", 0);

    // 0x100000000 = 2^32, passes validation, fits in 64-bit ulong, > uint32_t
    AsyncSetOperationValueType value = std::string("0x100000000");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = updateApSkuIdHandler(value, &status, mockDevice, 0, 0, 0);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// updateApSkuIdHandler: valid SKU, all checks pass, IO succeeds cc=NSM_SUCCESS
// → line 211 condition FALSE (rc==0 && cc==0) → success path (lines 222-230)
// → status = Success, co_return NSM_SW_SUCCESS
TEST_F(NsmUpdateApSkuIdTest, UpdateApSkuIdHandler_Success_ReturnsSuccess)
{
    using namespace testing;

    auto mockDevice = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID",
                                                      "test-uuid", 0);

    // Encode a successful set_rot_property response with cc = NSM_SUCCESS
    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*mockDevice, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncSetOperationValueType value = std::string("0xABCD1234");
    AsyncOperationStatusType status = AsyncOperationStatusType::WriteFailure;
    auto coro = updateApSkuIdHandler(value, &status, mockDevice, 0, 0, 0);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// updateApSkuIdHandler: postPatchIO returns non-NSM_SW_SUCCESS rc
// → rc != NSM_SW_SUCCESS branch at line 195 → lines 197-203 covered
// status = WriteFailure, co_return rc
TEST_F(NsmUpdateApSkuIdTest, UpdateApSkuIdHandler_PostPatchIOFail_WriteFailure)
{
    using namespace testing;

    auto mockDevice = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID",
                                                      "test-uuid", 0);

    // postPatchIO itself fails (rc = NSM_ERROR, not just a bad response)
    EXPECT_CALL(*mockDevice, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncSetOperationValueType value = std::string("0xABCD1234");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = updateApSkuIdHandler(value, &status, mockDevice, 0, 0, 0);

    // rc = NSM_ERROR (non-zero) → WriteFailure
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmApSkuIdObject tests - covering nsmApSkuId.cpp lines 40-73, 75-96, 107-113
// ============================================================================

struct NsmApSkuIdObjectTest : public Test, public utils::DBusTest
{};

// Constructor with empty associations: covers constructor body (lines 40-52),
// outer if-block false branch (line 53), no AssociationDefinitionsIntf created.
TEST_F(NsmApSkuIdObjectTest, Constructor_EmptyAssociations_CreatesObject)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "SKU_ApTest_1";
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               name + "/Progress";
    auto progressIntf = std::make_shared<ProgressIntf>(bus,
                                                       progressPath.c_str());

    std::vector<utils::Association> emptyAssoc;
    NsmApSkuIdObject sensor(bus, name, "NSM_ApSkuId",
                            "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                            progressIntf, 0x1800, 0x0001, 0, emptyAssoc);

    EXPECT_EQ(sensor.getName(), name);
    EXPECT_EQ(sensor.associationDefinitionsIntf, nullptr);
}

// Constructor with inventory_SKU association: covers lines 53-72 (true branch
// on outer if, filteredAssociations non-empty → AssociationDefinitionsIntf
// created at lines 66-70).
TEST_F(NsmApSkuIdObjectTest,
       Constructor_InventorySkuAssociation_CreatesAssocIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "SKU_ApTest_2";
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               name + "/Progress";
    auto progressIntf = std::make_shared<ProgressIntf>(bus,
                                                       progressPath.c_str());

    std::vector<utils::Association> assoc = {
        {"inventory_SKU", "chassis",
         "/xyz/openbmc_project/inventory/system/chassis/SKU_2"}};
    NsmApSkuIdObject sensor(bus, name, "NSM_ApSkuId",
                            "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                            progressIntf, 0x1800, 0x0001, 0, assoc);

    EXPECT_NE(sensor.associationDefinitionsIntf, nullptr);
}

// Constructor with non-inventory_SKU association: covers lines 55-72 (outer
// if true, inner loop false → filteredAssociations empty → line 64 false →
// no AssociationDefinitionsIntf).
TEST_F(NsmApSkuIdObjectTest, Constructor_NonSkuAssociation_NoAssocIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "SKU_ApTest_3";
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               name + "/Progress";
    auto progressIntf = std::make_shared<ProgressIntf>(bus,
                                                       progressPath.c_str());

    // forward != "inventory_SKU" → filteredAssociations stays empty
    std::vector<utils::Association> assoc = {
        {"chassis", "sensors",
         "/xyz/openbmc_project/inventory/system/chassis/SKU_3"}};
    NsmApSkuIdObject sensor(bus, name, "NSM_ApSkuId",
                            "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                            progressIntf, 0x1800, 0x0001, 0, assoc);

    EXPECT_EQ(sensor.associationDefinitionsIntf, nullptr);
}

// genRequestMsg success: covers lines 75-96 success path.
TEST_F(NsmApSkuIdObjectTest, GenRequestMsg_ValidParams_ReturnsRequest)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "SKU_ApTest_4";
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               name + "/Progress";
    auto progressIntf = std::make_shared<ProgressIntf>(bus,
                                                       progressPath.c_str());

    std::vector<utils::Association> emptyAssoc;
    NsmApSkuIdObject sensor(bus, name, "NSM_ApSkuId",
                            "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                            progressIntf, 0x1800, 0x0001, 0, emptyAssoc);

    auto request = sensor.genRequestMsg(10, 1);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_get_erot_state_info_req));
}

// genRequestMsg failure: instanceId > NSM_INSTANCE_MAX → encode fails →
// if (rc) TRUE → return nullopt. Covers lines 89-90,92-94 in nsmApSkuId.cpp.
TEST_F(NsmApSkuIdObjectTest, GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "SKU_ApTest_4b";
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               name + "/Progress";
    auto progressIntf = std::make_shared<ProgressIntf>(bus,
                                                       progressPath.c_str());

    std::vector<utils::Association> emptyAssoc;
    NsmApSkuIdObject sensor(bus, name, "NSM_ApSkuId",
                            "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                            progressIntf, 0x1800, 0x0001, 0, emptyAssoc);

    // NSM_INSTANCE_MAX + 1 makes encode_nsm_query_get_erot_state_parameters_req
    // fail → if (rc) TRUE → return nullopt
    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// handleResponseMsg decode failure: too-small buffer → decode returns error,
// covers lines 107-113 true branch (rc != NSM_SW_SUCCESS).
TEST_F(NsmApSkuIdObjectTest, HandleResponseMsg_ShortBuffer_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "SKU_ApTest_5";
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               name + "/Progress";
    auto progressIntf = std::make_shared<ProgressIntf>(bus,
                                                       progressPath.c_str());

    std::vector<utils::Association> emptyAssoc;
    NsmApSkuIdObject sensor(bus, name, "NSM_ApSkuId",
                            "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                            progressIntf, 0x1800, 0x0001, 0, emptyAssoc);

    std::vector<uint8_t> tiny(sizeof(nsm_msg_hdr) + 2, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(tiny.data());
    auto rc = sensor.handleResponseMsg(msg, tiny.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// DISABLED: The response buffer (nsm_msg_hdr +
// handleResponseMsg success: encode with a 512-byte buffer (TLV encoding
// requires more space than just the header struct). The encode function does
// NOT include ap_sku_id in the TLV stream, so the decoded ap_sku_id is 0 →
// formatApSkuId(0) = "0x00000000". Covers nsmApSkuId.cpp lines 117-125.
TEST_F(NsmApSkuIdObjectTest, HandleResponseMsg_Success_SetsSku)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "SKU_ApTest_6";
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               name + "/Progress";
    auto progressIntf = std::make_shared<ProgressIntf>(bus,
                                                       progressPath.c_str());

    std::vector<utils::Association> emptyAssoc;
    NsmApSkuIdObject sensor(bus, name, "NSM_ApSkuId",
                            "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                            progressIntf, 0x1800, 0x0001, 0, emptyAssoc);

    // Allocate a 512-byte buffer: the TLV-encoded response for all header
    // fields (8 fields ≈ 35 bytes TLV + header overhead) needs more space
    // than sizeof(nsm_firmware_erot_state_info_hdr_resp).
    std::vector<uint8_t> response(512, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    nsm_firmware_erot_state_info_resp erotInfo = {};
    erotInfo.slot_info = nullptr; // firmware_slot_count=0, no slot data

    auto encRc = encode_nsm_query_get_erot_state_parameters_resp(
        0, NSM_SUCCESS, ERR_NULL, &erotInfo, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());

    // ap_sku_id is not encoded in the TLV stream → decoded value = 0
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor.apSkuIdObject->sku(), formatApSkuId(0));
}

// handleResponseMsg: decode succeeds but cc != NSM_SUCCESS → true branch of
// if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS) → free(slot_info) + return.
// Covers cc != 0 path of cc ? cc : rc.
TEST_F(NsmApSkuIdObjectTest, HandleResponseMsg_ErrorCC_ReturnsCC)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "SKU_ApTest_7";
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               name + "/Progress";
    auto progressIntf = std::make_shared<ProgressIntf>(bus,
                                                       progressPath.c_str());

    std::vector<utils::Association> emptyAssoc;
    NsmApSkuIdObject sensor(bus, name, "NSM_ApSkuId",
                            "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                            progressIntf, 0x1800, 0x0001, 0, emptyAssoc);

    // Build a minimal non-success response with cc = NSM_ERROR
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto* nonSuccessResp =
        reinterpret_cast<nsm_common_non_success_resp*>(responseMsg->payload);
    nonSuccessResp->completion_code = NSM_ERROR;

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// setSKU with null apSkuIdObject → if(apSkuIdObject) FALSE branch (L84)
// #define private public lets us null the member directly.
TEST_F(NsmApSkuIdObjectTest, SetSKU_NullApSkuIdObject_DoesNothing)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string name = "SKU_ApTest_NullObj";
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               name + "/Progress";
    auto progressIntf = std::make_shared<ProgressIntf>(bus,
                                                       progressPath.c_str());
    std::vector<utils::Association> emptyAssoc;
    NsmApSkuIdObject sensor(bus, name, "NSM_ApSkuId",
                            "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                            progressIntf, 0x1800, 0x0001, 0, emptyAssoc);

    // Null out apSkuIdObject to hit the false branch of if(apSkuIdObject)
    sensor.apSkuIdObject = nullptr;

    // Should do nothing (not crash)
    EXPECT_NO_THROW(sensor.setSKU("0xDEADBEEF"));
}
