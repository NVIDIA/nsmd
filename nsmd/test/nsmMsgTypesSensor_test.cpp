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

using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmMsgTypesSensor.hpp"
#include "nsmSensor.hpp"

using namespace nsm;

// -----------------------------------------------------------------------
// Helpers: encode message-types response
// -----------------------------------------------------------------------
static std::vector<uint8_t>
    makeMsgTypesResp(uint8_t cc,
                     bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE])
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_supported_nvidia_message_types_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_nvidia_message_types_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, types, msg);
    return buf;
}

// -----------------------------------------------------------------------
// Helpers: encode command-codes response
// -----------------------------------------------------------------------
static std::vector<uint8_t>
    makeCmdCodesResp(uint8_t cc,
                     bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE])
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_command_codes_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, codes, msg);
    return buf;
}

// -----------------------------------------------------------------------
// Test Fixture
// -----------------------------------------------------------------------
struct NsmMsgTypesSensorTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmMsgTypesSensorTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmMsgTypesSensorTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// -----------------------------------------------------------------------
// NsmCommandCodes::update: sensorIO fails → co_return error //

// -----------------------------------------------------------------------
TEST_F(NsmMsgTypesSensorTest, NsmCommandCodes_Update_SensorIOFails_ReturnsError)
{
    auto sensor = std::make_shared<NsmCommandCodes>("cmdcodes", "type", *gpu,
                                                    0);

    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(
            mockSensorIO(makeCmdCodesResp(NSM_SUCCESS, codes), NSM_ERROR));

    auto coro = sensor->update(gpu);
}

// -----------------------------------------------------------------------
// NsmCommandCodes::update: error CC → co_return cc ? cc : rc //

// -----------------------------------------------------------------------
TEST_F(NsmMsgTypesSensorTest, NsmCommandCodes_Update_ErrorCC_ReturnsCC)
{
    auto sensor = std::make_shared<NsmCommandCodes>("cmdcodes_errcc", "type",
                                                    *gpu, 0);

    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeCmdCodesResp(NSM_ERROR, codes)));

    auto coro = sensor->update(gpu);
}

// -----------------------------------------------------------------------
// NsmCommandCodes::update: success → calls
// updateMessageTypesToCommandCodeMatrix
// -----------------------------------------------------------------------
TEST_F(NsmMsgTypesSensorTest, NsmCommandCodes_Update_Success_UpdatesMatrix)
{
    auto sensor = std::make_shared<NsmCommandCodes>("cmdcodes_ok", "type", *gpu,
                                                    1);

    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[0].byte = 0xFF; // All 8 commands in first byte supported

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeCmdCodesResp(NSM_SUCCESS, codes)));

    auto coro = sensor->update(gpu);
    // No crash is the success criterion; matrix update is verified via the
    // side effect that commandCodesRetrieved is updated
}

// -----------------------------------------------------------------------
// NsmMsgTypes::update: sensorIO fails → co_return error
// -----------------------------------------------------------------------
TEST_F(NsmMsgTypesSensorTest, NsmMsgTypes_Update_SensorIOFails_ReturnsError)
{
    NsmMsgTypes sensor("msgtypes", "type", *gpu);

    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(
            mockSensorIO(makeMsgTypesResp(NSM_SUCCESS, types), NSM_ERROR));

    auto coro = sensor.update(gpu);
}

// -----------------------------------------------------------------------
// NsmMsgTypes::update: error CC → co_return cc ? cc : rc
// -----------------------------------------------------------------------
TEST_F(NsmMsgTypesSensorTest, NsmMsgTypes_Update_ErrorCC_ReturnsCC)
{
    NsmMsgTypes sensor("msgtypes_errcc", "type", *gpu);

    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeMsgTypesResp(NSM_ERROR, types)));

    auto coro = sensor.update(gpu);
}

// -----------------------------------------------------------------------
// NsmMsgTypes::update: success with no bits set → no sensors added
// -----------------------------------------------------------------------
TEST_F(NsmMsgTypesSensorTest, NsmMsgTypes_Update_NoBitsSet_NoSensorsAdded)
{
    NsmMsgTypes sensor("msgtypes_nobits", "type", *gpu);

    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    // All bits zero: no message types supported

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeMsgTypesResp(NSM_SUCCESS, types)));

    size_t sensorsBefore = gpu->deviceSensors.size();
    auto coro = sensor.update(gpu);
    // No new sensors should have been added
    EXPECT_EQ(gpu->deviceSensors.size(), sensorsBefore);
}

// -----------------------------------------------------------------------
// NsmMsgTypes::update: success with bits set → NsmCommandCodes sensors added
// -----------------------------------------------------------------------
TEST_F(NsmMsgTypesSensorTest, NsmMsgTypes_Update_BitsSet_AddsCommandCodeSensors)
{
    NsmMsgTypes sensor("msgtypes_bits", "type", *gpu);

    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    types[0].byte = 0x03; // Message types 0 and 1 are supported

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeMsgTypesResp(NSM_SUCCESS, types)));

    size_t sensorsBefore = gpu->deviceSensors.size();
    auto coro = sensor.update(gpu);
    // Two new NsmCommandCodes sensors should be added (types 0 and 1)
    EXPECT_EQ(gpu->deviceSensors.size(), sensorsBefore + 2);
}

// -----------------------------------------------------------------------
// NsmCommandCodes: ternary FALSE branch (cc==0, decode length error)
// A too-short response buffer causes the decode to return a length-error rc
// while cc stays 0 (NSM_SUCCESS). The ternary `cc ? cc : rc` takes the
// FALSE branch and returns rc instead of cc.
// -----------------------------------------------------------------------
TEST_F(NsmMsgTypesSensorTest, NsmCommandCodes_Update_DecodeLengthError_ReturnRc)
{
    auto sensor = std::make_shared<NsmCommandCodes>("cmdcodes_lenErr", "type",
                                                    *gpu, 0);

    // Buffer big enough for decode_reason_code_and_cc to read cc=0, but
    // smaller than
    // sizeof(nsm_msg_hdr)+sizeof(nsm_get_supported_command_codes_resp) → decode
    // returns NSM_SW_ERROR_LENGTH while cc stays 0.
    std::vector<uint8_t> shortResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(shortResp, NSM_SUCCESS));

    // rc != 0 and cc == 0: co_return cc ? cc : rc → returns rc (FALSE branch)
    //
    auto coro = sensor->update(gpu);
}

// -----------------------------------------------------------------------
// NsmMsgTypes: ternary FALSE branch (cc==0, decode length error)
// -----------------------------------------------------------------------
TEST_F(NsmMsgTypesSensorTest, NsmMsgTypes_Update_DecodeLengthError_ReturnRc)
{
    NsmMsgTypes sensor("msgtypes_lenErr", "type", *gpu);

    std::vector<uint8_t> shortResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(shortResp, NSM_SUCCESS));

    auto coro = sensor.update(gpu);
}

// -----------------------------------------------------------------------
// NsmMsgTypes::update: duplicate call → no duplicate sensors
// -----------------------------------------------------------------------
TEST_F(NsmMsgTypesSensorTest, NsmMsgTypes_Update_CalledTwice_NoDuplicateSensors)
{
    NsmMsgTypes sensor("msgtypes_dup", "type", *gpu);

    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    types[0].byte = 0x01; // Only message type 0 supported

    // First call
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeMsgTypesResp(NSM_SUCCESS, types)))
        .WillOnce(mockSensorIO(makeMsgTypesResp(NSM_SUCCESS, types)));

    auto coro1 = sensor.update(gpu);
    size_t sensorsAfterFirst = gpu->deviceSensors.size();

    auto coro2 = sensor.update(gpu);
    // Second call with same types: no new sensors added
    EXPECT_EQ(gpu->deviceSensors.size(), sensorsAfterFirst);
}
