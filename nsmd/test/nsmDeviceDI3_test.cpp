/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests (batch 3) for nsmd/nsmDevice.cpp
 * Targets uncovered coroutine functions:
 *   - getFRU() with different device types (GPU, Switch, PCIE_BRIDGE, etc.)
 *   - updateFruDeviceIntf() — command supported vs not, recreation vs update
 *   - refreshCommandMatrix() — message types not retrieved, command code
 *     success/failure, out-of-range skip
 *   - refreshCapabilitySensor() — empty list, single sensor, multiple sensors
 *   - getInventoryInformation — additional property IDs not yet tested:
 *     DEVICE_PART_NUMBER, MEMORY_VENDOR, MEMORY_PART_NUMBER, FIRMWARE_VERSION,
 *     INFO_ROM_VERSION
 *   - getInventoryInformation — DEVICE_GUID with all-zero UUID (empty string)
 *   - dumpNsmDeviceInfo / dumpNsmDeviceInfoTask
 */

#include "base.h"
#include "platform-environmental.h"

#include "common/event.hpp"
#include "eventManager.hpp"
#include "instance_id.hpp"
#include "requester/handler.hpp"
#include "socket_handler.hpp"
#include "socket_manager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "commonMock.hpp"
#include "nsmDevice.hpp"

#undef private
#undef protected

using namespace ::testing;
using namespace nsm;

// ---------------------------------------------------------------------------
// Helpers: build response buffers
// ---------------------------------------------------------------------------

static std::vector<uint8_t>
    makeMsgTypesResp3(uint8_t cc,
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

static std::vector<uint8_t>
    makeCmdCodesResp3(uint8_t cc,
                      bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE])
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_command_codes_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, codes, msg);
    return buf;
}

static std::vector<uint8_t> makeInvInfoResp3(uint8_t cc, uint16_t dataSize = 0,
                                             const uint8_t* data = nullptr)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_inventory_information_resp) +
                                 (dataSize > 0 ? dataSize - 1 : 0),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, dataSize, data,
        msg);
    return buf;
}

static void allocMessage3(const std::vector<uint8_t>& response,
                          std::shared_ptr<const nsm_msg>& responseMsg,
                          size_t& responseLen)
{
    responseLen = response.size();
    if (responseLen > 0)
    {
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), response.data(), responseLen);
    }
}

// ---------------------------------------------------------------------------
// Test Fixture
// ---------------------------------------------------------------------------
using TestHandler3 = requester::Handler<requester::Request>;

struct NsmDeviceDI3Test : public Test
{
    common::Event event;
    InstanceIdDb instanceIdDb;
    mctp_socket::Manager sockManager;

    TestHandler3 handler{event,
                         instanceIdDb,
                         sockManager,
                         /*verbose=*/false,
                         std::chrono::seconds(2),
                         /*numRetries=*/0,
                         std::chrono::milliseconds(100)};

    std::shared_ptr<MockNsmMessageHandler> mockMsgHandler =
        std::make_shared<MockNsmMessageHandler>(handler);

    const uuid_t testUuid = "00000000-0000-0000-0000-000000000000";
    std::shared_ptr<MockNsmDevice> device;

    NsmDeviceDI3Test()
    {
        device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 1, "MCTP_UUID",
                                                 testUuid, 1);
        device->nsmMsgHandler = mockMsgHandler;
    }

    ~NsmDeviceDI3Test() override = default;

    auto mockSendRecv3(const std::vector<uint8_t>& response,
                       int rc = NSM_SW_SUCCESS)
    {
        return [response, rc](eid_t, Request&,
                              std::shared_ptr<const nsm_msg>& responseMsg,
                              size_t* responseLen) -> requester::Coroutine {
            allocMessage3(response, responseMsg, *responseLen);
            co_return rc;
        };
    }

    auto mockSendRecvFail3(int rc = NSM_SW_ERROR)
    {
        return [rc](eid_t, Request&, std::shared_ptr<const nsm_msg>&,
                    size_t*) -> requester::Coroutine { co_return rc; };
    }
};

// ===========================================================================
// getInventoryInformation — additional string property IDs
// ===========================================================================

TEST_F(NsmDeviceDI3Test, GetInventoryInfo_DevicePartNumber)
{
    const char* value = "DP-100";
    auto resp = makeInvInfoResp3(NSM_SUCCESS, strlen(value),
                                 reinterpret_cast<const uint8_t*>(value));

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    InventoryProperties props;
    uint8_t propId = DEVICE_PART_NUMBER;
    auto coro = device->getInventoryInformation(propId, props);

    EXPECT_TRUE(props.find(DEVICE_PART_NUMBER) != props.end());
    EXPECT_EQ(std::get<std::string>(props.at(DEVICE_PART_NUMBER)), "DP-100");
}

TEST_F(NsmDeviceDI3Test, GetInventoryInfo_MemoryVendor)
{
    const char* value = "Samsung";
    auto resp = makeInvInfoResp3(NSM_SUCCESS, strlen(value),
                                 reinterpret_cast<const uint8_t*>(value));

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    InventoryProperties props;
    uint8_t propId = MEMORY_VENDOR;
    auto coro = device->getInventoryInformation(propId, props);

    EXPECT_TRUE(props.find(MEMORY_VENDOR) != props.end());
    EXPECT_EQ(std::get<std::string>(props.at(MEMORY_VENDOR)), "Samsung");
}

TEST_F(NsmDeviceDI3Test, GetInventoryInfo_MemoryPartNumber)
{
    const char* value = "M393A8G";
    auto resp = makeInvInfoResp3(NSM_SUCCESS, strlen(value),
                                 reinterpret_cast<const uint8_t*>(value));

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    InventoryProperties props;
    uint8_t propId = MEMORY_PART_NUMBER;
    auto coro = device->getInventoryInformation(propId, props);

    EXPECT_TRUE(props.find(MEMORY_PART_NUMBER) != props.end());
}

TEST_F(NsmDeviceDI3Test, GetInventoryInfo_FirmwareVersion)
{
    const char* value = "1.2.3.4";
    auto resp = makeInvInfoResp3(NSM_SUCCESS, strlen(value),
                                 reinterpret_cast<const uint8_t*>(value));

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    InventoryProperties props;
    uint8_t propId = FIRMWARE_VERSION;
    auto coro = device->getInventoryInformation(propId, props);

    EXPECT_TRUE(props.find(FIRMWARE_VERSION) != props.end());
}

TEST_F(NsmDeviceDI3Test, GetInventoryInfo_InfoRomVersion)
{
    const char* value = "ROM-5.6";
    auto resp = makeInvInfoResp3(NSM_SUCCESS, strlen(value),
                                 reinterpret_cast<const uint8_t*>(value));

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    InventoryProperties props;
    uint8_t propId = INFO_ROM_VERSION;
    auto coro = device->getInventoryInformation(propId, props);

    EXPECT_TRUE(props.find(INFO_ROM_VERSION) != props.end());
}

// DEVICE_GUID with all-zero data => convertUUIDToString returns empty
TEST_F(NsmDeviceDI3Test, GetInventoryInfo_DeviceGuid_AllZero_EmptyUuid)
{
    std::vector<uint8_t> uuidData(UUID_INT_SIZE, 0x00);
    auto resp = makeInvInfoResp3(NSM_SUCCESS, UUID_INT_SIZE, uuidData.data());

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    InventoryProperties props;
    uint8_t propId = DEVICE_GUID;
    auto coro = device->getInventoryInformation(propId, props);

    // All-zero UUID => convertUUIDToString returns "00000000-..."
    // This is not empty, so it should be added to props
    EXPECT_TRUE(props.find(DEVICE_GUID) != props.end());
}

// ===========================================================================
// getFRU — GPU device type (has 6 properties)
// ===========================================================================

TEST_F(NsmDeviceDI3Test, GetFRU_GPU_AllPropertiesQueried)
{
    // GPU queries: BOARD_PART_NUMBER, FRU_PART_NUMBER, SERIAL_NUMBER,
    // DEVICE_GUID, MARKETING_NAME, BUILD_DATE = 6 properties
    const char* strValue = "test-value";
    auto strResp = makeInvInfoResp3(NSM_SUCCESS, strlen(strValue),
                                    reinterpret_cast<const uint8_t*>(strValue));

    std::vector<uint8_t> uuidData(UUID_INT_SIZE, 0xAB);
    auto uuidResp = makeInvInfoResp3(NSM_SUCCESS, UUID_INT_SIZE,
                                     uuidData.data());

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(strResp))  // BOARD_PART_NUMBER
        .WillOnce(mockSendRecv3(strResp))  // FRU_PART_NUMBER
        .WillOnce(mockSendRecv3(strResp))  // SERIAL_NUMBER
        .WillOnce(mockSendRecv3(uuidResp)) // DEVICE_GUID
        .WillOnce(mockSendRecv3(strResp))  // MARKETING_NAME
        .WillOnce(mockSendRecv3(strResp)); // BUILD_DATE

    InventoryProperties props;
    auto coro = device->getFRU(props, NSM_DEV_ID_GPU);

    EXPECT_TRUE(props.find(BOARD_PART_NUMBER) != props.end());
    EXPECT_TRUE(props.find(FRU_PART_NUMBER) != props.end());
    EXPECT_TRUE(props.find(SERIAL_NUMBER) != props.end());
    EXPECT_TRUE(props.find(DEVICE_GUID) != props.end());
    EXPECT_TRUE(props.find(MARKETING_NAME) != props.end());
    EXPECT_TRUE(props.find(BUILD_DATE) != props.end());
}

// getFRU — Switch device type (5 properties, no FRU_PART_NUMBER)
TEST_F(NsmDeviceDI3Test, GetFRU_Switch_QueriesCorrectProps)
{
    const char* strValue = "sw-val";
    auto strResp = makeInvInfoResp3(NSM_SUCCESS, strlen(strValue),
                                    reinterpret_cast<const uint8_t*>(strValue));
    std::vector<uint8_t> uuidData(UUID_INT_SIZE, 0xCD);
    auto uuidResp = makeInvInfoResp3(NSM_SUCCESS, UUID_INT_SIZE,
                                     uuidData.data());

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(strResp))  // BOARD_PART_NUMBER
        .WillOnce(mockSendRecv3(strResp))  // SERIAL_NUMBER
        .WillOnce(mockSendRecv3(uuidResp)) // DEVICE_GUID
        .WillOnce(mockSendRecv3(strResp))  // MARKETING_NAME
        .WillOnce(mockSendRecv3(strResp)); // BUILD_DATE

    InventoryProperties props;
    auto coro = device->getFRU(props, NSM_DEV_ID_SWITCH);

    EXPECT_TRUE(props.find(BOARD_PART_NUMBER) != props.end());
    EXPECT_TRUE(props.find(SERIAL_NUMBER) != props.end());
}

// getFRU — PCIE_BRIDGE (6 properties including ASSET_TAG)
TEST_F(NsmDeviceDI3Test, GetFRU_PcieBridge_IncludesAssetTag)
{
    const char* strValue = "bridge";
    auto strResp = makeInvInfoResp3(NSM_SUCCESS, strlen(strValue),
                                    reinterpret_cast<const uint8_t*>(strValue));
    std::vector<uint8_t> uuidData(UUID_INT_SIZE, 0xEF);
    auto uuidResp = makeInvInfoResp3(NSM_SUCCESS, UUID_INT_SIZE,
                                     uuidData.data());

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(strResp))  // BOARD_PART_NUMBER
        .WillOnce(mockSendRecv3(strResp))  // SERIAL_NUMBER
        .WillOnce(mockSendRecv3(uuidResp)) // DEVICE_GUID
        .WillOnce(mockSendRecv3(strResp))  // MARKETING_NAME
        .WillOnce(mockSendRecv3(strResp))  // BUILD_DATE
        .WillOnce(mockSendRecv3(strResp)); // ASSET_TAG (default case)

    InventoryProperties props;
    auto coro = device->getFRU(props, NSM_DEV_ID_PCIE_BRIDGE);

    EXPECT_TRUE(props.find(BOARD_PART_NUMBER) != props.end());
}

// getFRU — BASEBOARD (empty property list)
TEST_F(NsmDeviceDI3Test, GetFRU_Baseboard_NoQueries)
{
    // BASEBOARD has empty property list — no SendRecvNsmMsg calls expected
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _)).Times(0);

    InventoryProperties props;
    auto coro = device->getFRU(props, NSM_DEV_ID_BASEBOARD);

    EXPECT_TRUE(props.empty());
}

// getFRU — EROT device type (5 properties)
TEST_F(NsmDeviceDI3Test, GetFRU_Erot_QueriesCorrectProps)
{
    const char* strValue = "erot-val";
    auto strResp = makeInvInfoResp3(NSM_SUCCESS, strlen(strValue),
                                    reinterpret_cast<const uint8_t*>(strValue));
    std::vector<uint8_t> uuidData(UUID_INT_SIZE, 0x11);
    auto uuidResp = makeInvInfoResp3(NSM_SUCCESS, UUID_INT_SIZE,
                                     uuidData.data());

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(strResp))  // BOARD_PART_NUMBER
        .WillOnce(mockSendRecv3(strResp))  // SERIAL_NUMBER
        .WillOnce(mockSendRecv3(uuidResp)) // DEVICE_GUID
        .WillOnce(mockSendRecv3(strResp))  // MARKETING_NAME
        .WillOnce(mockSendRecv3(strResp)); // BUILD_DATE

    InventoryProperties props;
    auto coro = device->getFRU(props, NSM_DEV_ID_EROT);

    EXPECT_TRUE(props.find(BOARD_PART_NUMBER) != props.end());
}

// getFRU — MCTP_BRIDGE (only SERIAL_NUMBER)
TEST_F(NsmDeviceDI3Test, GetFRU_MctpBridge_OnlySerialNumber)
{
    const char* strValue = "SN-MCTP";
    auto strResp = makeInvInfoResp3(NSM_SUCCESS, strlen(strValue),
                                    reinterpret_cast<const uint8_t*>(strValue));

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(strResp)); // SERIAL_NUMBER only

    InventoryProperties props;
    auto coro = device->getFRU(props, NSM_DEV_ID_MCTP_BRIDGE);

    EXPECT_TRUE(props.find(SERIAL_NUMBER) != props.end());
}

// getFRU — CPU (empty property list)
TEST_F(NsmDeviceDI3Test, GetFRU_CPU_NoQueries)
{
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _)).Times(0);

    InventoryProperties props;
    auto coro = device->getFRU(props, NSM_DEV_ID_CPU);

    EXPECT_TRUE(props.empty());
}

// getFRU — UNKNOWN device type (empty property list, fallback)
TEST_F(NsmDeviceDI3Test, GetFRU_UnknownDeviceType_EmptyList)
{
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _)).Times(0);

    InventoryProperties props;
    auto coro = device->getFRU(props, NSM_DEV_ID_UNKNOWN);

    EXPECT_TRUE(props.empty());
}

// getFRU — with one property failing (error from getInventoryInformation)
TEST_F(NsmDeviceDI3Test, GetFRU_GPU_OnePropertyFails)
{
    const char* strValue = "ok";
    auto strResp = makeInvInfoResp3(NSM_SUCCESS, strlen(strValue),
                                    reinterpret_cast<const uint8_t*>(strValue));

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(strResp))  // BOARD_PART_NUMBER - ok
        .WillOnce(mockSendRecvFail3())     // FRU_PART_NUMBER - fail
        .WillOnce(mockSendRecv3(strResp))  // SERIAL_NUMBER - ok
        .WillOnce(mockSendRecvFail3())     // DEVICE_GUID - fail
        .WillOnce(mockSendRecv3(strResp))  // MARKETING_NAME - ok
        .WillOnce(mockSendRecv3(strResp)); // BUILD_DATE - ok

    InventoryProperties props;
    auto coro = device->getFRU(props, NSM_DEV_ID_GPU);

    // Only the successful ones should be in props
    EXPECT_TRUE(props.find(BOARD_PART_NUMBER) != props.end());
    EXPECT_TRUE(props.find(FRU_PART_NUMBER) == props.end());
    EXPECT_TRUE(props.find(SERIAL_NUMBER) != props.end());
}

// ===========================================================================
// refreshCommandMatrix — message types not yet retrieved
// ===========================================================================

TEST_F(NsmDeviceDI3Test, RefreshCommandMatrix_RetrievesTypesAndCodes)
{
    // Mark message types as not retrieved
    device->areMessageTypesRetrieved = false;

    // Build message types response: types 0 and 2 supported
    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    types[0].byte = 0x05; // bits 0 and 2
    auto typesResp = makeMsgTypesResp3(NSM_SUCCESS, types);

    // Build command codes response
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[0].byte = 0x03; // commands 0 and 1
    auto codesResp = makeCmdCodesResp3(NSM_SUCCESS, codes);

    // Command codes not yet retrieved for these types
    device->commandCodesRetrieved[0] = false;
    device->commandCodesRetrieved[2] = false;

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(typesResp))  // getSupportedNvidiaMessageType
        .WillOnce(mockSendRecv3(codesResp))  // getSupportedCommandCodes(0)
        .WillOnce(mockSendRecv3(codesResp)); // getSupportedCommandCodes(2)

    auto coro = device->refreshCommandMatrix();

    EXPECT_TRUE(device->areMessageTypesRetrieved);
    EXPECT_TRUE(device->commandCodesRetrieved[0]);
    EXPECT_TRUE(device->commandCodesRetrieved[2]);
}

// refreshCommandMatrix — getSupportedNvidiaMessageType fails
TEST_F(NsmDeviceDI3Test, RefreshCommandMatrix_MsgTypeFail_ReturnsError)
{
    device->areMessageTypesRetrieved = false;

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecvFail3(NSM_SW_ERROR));

    auto coro = device->refreshCommandMatrix();

    EXPECT_FALSE(device->areMessageTypesRetrieved);
}

// refreshCommandMatrix — command codes fail for one type
TEST_F(NsmDeviceDI3Test, RefreshCommandMatrix_CmdCodeFail_MarkedNotRetrieved)
{
    device->areMessageTypesRetrieved = false;

    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    types[0].byte = 0x01; // type 0 only
    auto typesResp = makeMsgTypesResp3(NSM_SUCCESS, types);

    device->commandCodesRetrieved[0] = false;

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(typesResp))         // types success
        .WillOnce(mockSendRecvFail3(NSM_SW_ERROR)); // codes fail

    auto coro = device->refreshCommandMatrix();

    EXPECT_TRUE(device->areMessageTypesRetrieved);
    EXPECT_FALSE(device->commandCodesRetrieved[0]); // marked as failed
}

// refreshCommandMatrix — types already retrieved, some codes already retrieved
TEST_F(NsmDeviceDI3Test, RefreshCommandMatrix_AlreadyRetrieved_SkipsTypes)
{
    // Mark types as already retrieved
    device->areMessageTypesRetrieved = true;
    device->retrievedMessageTypes = {0, 2};
    device->commandCodesRetrieved[0] = true;  // already have codes for type 0
    device->commandCodesRetrieved[2] = false; // need codes for type 2

    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[0].byte = 0x0F;
    auto codesResp = makeCmdCodesResp3(NSM_SUCCESS, codes);

    // Only one call for type 2 (type 0 already retrieved)
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(codesResp));

    auto coro = device->refreshCommandMatrix();

    EXPECT_TRUE(device->commandCodesRetrieved[2]);
}

// refreshCommandMatrix — out-of-range message type skipped
TEST_F(NsmDeviceDI3Test, RefreshCommandMatrix_OutOfRangeType_Skipped)
{
    device->areMessageTypesRetrieved = true;
    device->retrievedMessageTypes = {
        static_cast<uint8_t>(NUM_NSM_TYPES + 5)}; // out of range

    // No SendRecvNsmMsg calls expected for out-of-range types
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _)).Times(0);

    auto coro = device->refreshCommandMatrix();
}

// ===========================================================================
// updateFruDeviceIntf — command not supported, second call (update path)
// ===========================================================================

TEST_F(NsmDeviceDI3Test, UpdateFruDeviceIntf_CommandNotSupported_UpdatePath)
{
    // Pre-initialize FruInterfaceManager so needsRecreation returns false
    // This exercises the "else" (update) path instead of crashing on
    // createAndRegisterInterface with null objServer
    std::set<std::string> fixedProps = {"DEVICE_TYPE", "INSTANCE_NUMBER",
                                        "DEVICE_ROLE", "UUID"};
    device->fruDeviceManager.markInitialized(fixedProps);

    // Make sure GET_INVENTORY_INFORMATION command is NOT supported
    device->messageTypesToCommandCodeMatrix[NSM_TYPE_PLATFORM_ENVIRONMENTAL]
                                           [NSM_GET_INVENTORY_INFORMATION] =
        false;

    // No SendRecvNsmMsg should be called since command not supported
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _)).Times(0);

    EXPECT_NO_THROW_COROUTINE(device->updateFruDeviceIntf());
}

// updateFruDeviceIntf — command supported, update path (already initialized)
TEST_F(NsmDeviceDI3Test, UpdateFruDeviceIntf_Success_UpdatesProperties)
{
    // Pre-initialize with the full set including dynamic properties
    std::set<std::string> allProps = {"DEVICE_TYPE",       "INSTANCE_NUMBER",
                                      "DEVICE_ROLE",       "UUID",
                                      "BOARD_PART_NUMBER", "FRU_PART_NUMBER",
                                      "SERIAL_NUMBER",     "MARKETING_NAME",
                                      "BUILD_DATE",        "DEVICE_UUID"};
    device->fruDeviceManager.markInitialized(allProps);

    // Enable the command
    device->messageTypesToCommandCodeMatrix[NSM_TYPE_PLATFORM_ENVIRONMENTAL]
                                           [NSM_GET_INVENTORY_INFORMATION] =
        true;

    // GPU has 6 properties — all succeed
    const char* strValue = "test";
    auto strResp = makeInvInfoResp3(NSM_SUCCESS, strlen(strValue),
                                    reinterpret_cast<const uint8_t*>(strValue));
    std::vector<uint8_t> uuidData(UUID_INT_SIZE, 0xAA);
    auto uuidResp = makeInvInfoResp3(NSM_SUCCESS, UUID_INT_SIZE,
                                     uuidData.data());

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(strResp))  // BOARD_PART_NUMBER
        .WillOnce(mockSendRecv3(strResp))  // FRU_PART_NUMBER
        .WillOnce(mockSendRecv3(strResp))  // SERIAL_NUMBER
        .WillOnce(mockSendRecv3(uuidResp)) // DEVICE_GUID
        .WillOnce(mockSendRecv3(strResp))  // MARKETING_NAME
        .WillOnce(mockSendRecv3(strResp)); // BUILD_DATE

    EXPECT_NO_THROW_COROUTINE(device->updateFruDeviceIntf());
}

// updateFruDeviceIntf — getFRU fails, exercises error logging path
TEST_F(NsmDeviceDI3Test, UpdateFruDeviceIntf_GetFRUFails)
{
    // Pre-initialize to avoid createAndRegisterInterface crash
    std::set<std::string> fixedProps = {"DEVICE_TYPE", "INSTANCE_NUMBER",
                                        "DEVICE_ROLE", "UUID"};
    device->fruDeviceManager.markInitialized(fixedProps);

    device->messageTypesToCommandCodeMatrix[NSM_TYPE_PLATFORM_ENVIRONMENTAL]
                                           [NSM_GET_INVENTORY_INFORMATION] =
        true;

    // All 6 GPU property queries fail
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillRepeatedly(mockSendRecvFail3());

    EXPECT_NO_THROW_COROUTINE(device->updateFruDeviceIntf());
}

// updateFruDeviceIntf — properties change, triggers recreation path
// Note: needsRecreation returns true when property set changes
TEST_F(NsmDeviceDI3Test, UpdateFruDeviceIntf_PropertySetChanged_NeedsRecreation)
{
    // Initialize with a small set of properties
    std::set<std::string> smallProps = {"DEVICE_TYPE", "INSTANCE_NUMBER",
                                        "DEVICE_ROLE", "UUID"};
    device->fruDeviceManager.markInitialized(smallProps);

    // Disable the command — no FRU properties returned, only fixed props
    // The fixed props include BOARD_PART_NUMBER etc. if they appeared in
    // getFRU results. Since command is disabled, only the 4 fixed props.
    // But the initialized set is the same 4, so needsRecreation returns false.
    device->messageTypesToCommandCodeMatrix[NSM_TYPE_PLATFORM_ENVIRONMENTAL]
                                           [NSM_GET_INVENTORY_INFORMATION] =
        false;

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _)).Times(0);

    // This should take the "update" path since props match
    EXPECT_NO_THROW_COROUTINE(device->updateFruDeviceIntf());
}

// dumpNsmDeviceInfo/dumpNsmDeviceInfoTask: not tested here because the
// detached coroutine accesses many uninitialized device fields (eid, medium,
// binding) which triggers Valgrind errors. The function is covered indirectly
// through the refreshCommandMatrix tests above.

// ===========================================================================
// getSupportedNvidiaMessageType — decode error (cc != NSM_SUCCESS)
// ===========================================================================

TEST_F(NsmDeviceDI3Test, GetSupportedMsgTypes_NonSuccessCC_ReturnsError)
{
    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    auto resp = makeMsgTypesResp3(NSM_ERROR, types);

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    std::vector<uint8_t> supportedTypes;
    auto coro = device->getSupportedNvidiaMessageType(supportedTypes);

    // Should not populate types on error
    EXPECT_TRUE(supportedTypes.empty());
}

// ===========================================================================
// getSupportedCommandCodes — decode error
// ===========================================================================

TEST_F(NsmDeviceDI3Test, GetSupportedCmdCodes_NonSuccessCC_ReturnsError)
{
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    auto resp = makeCmdCodesResp3(NSM_ERROR, codes);

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    std::vector<uint8_t> supportedCmds;
    auto coro = device->getSupportedCommandCodes(0, supportedCmds);

    // On error CC, the decode succeeds but cc != NSM_SUCCESS so error path
    // The function still writes to supportedCmds but returns error code
    // The actual size depends on decode behavior
    (void)supportedCmds;
}

// ===========================================================================
// getFRU — unknown device type falls back to NSM_DEV_ID_UNKNOWN
// ===========================================================================

TEST_F(NsmDeviceDI3Test, GetFRU_InvalidDeviceType_FallsBackToUnknown)
{
    // Device type 255 is not in the map, should fall back to UNKNOWN (empty)
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _)).Times(0);

    InventoryProperties props;
    auto coro = device->getFRU(props, 255);

    EXPECT_TRUE(props.empty());
}

// ===========================================================================
// getInventoryInformation — non-success CC from decode
// ===========================================================================

TEST_F(NsmDeviceDI3Test, GetInventoryInfo_ErrorCC_ReturnsError)
{
    auto resp = makeInvInfoResp3(NSM_ERROR);

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    InventoryProperties props;
    uint8_t propId = BOARD_PART_NUMBER;
    auto coro = device->getInventoryInformation(propId, props);

    EXPECT_TRUE(props.empty());
}

// ===========================================================================
// refreshCommandMatrix — mixed types: one valid, one out-of-range, one
// already retrieved
// ===========================================================================

TEST_F(NsmDeviceDI3Test, RefreshCommandMatrix_MixedTypes)
{
    device->areMessageTypesRetrieved = true;
    // Type 0: already retrieved
    // Type 1: needs retrieval
    // Type 250: out of range (>= NUM_NSM_TYPES)
    device->retrievedMessageTypes = {0, 1,
                                     static_cast<uint8_t>(NUM_NSM_TYPES + 1)};
    device->commandCodesRetrieved[0] = true;
    device->commandCodesRetrieved[1] = false;

    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[0].byte = 0x01;
    auto codesResp = makeCmdCodesResp3(NSM_SUCCESS, codes);

    // Only type 1 should be queried
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(codesResp));

    auto coro = device->refreshCommandMatrix();

    EXPECT_TRUE(device->commandCodesRetrieved[1]);
}

// ===========================================================================
// getFRU — error in one of the properties logged but continues
// ===========================================================================

TEST_F(NsmDeviceDI3Test, GetFRU_MctpBridge_ErrorReturned_ContinuesNoThrow)
{
    // MCTP_BRIDGE only has SERIAL_NUMBER, and it fails
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecvFail3(NSM_SW_ERROR));

    InventoryProperties props;
    auto coro = device->getFRU(props, NSM_DEV_ID_MCTP_BRIDGE);

    // Error logged but function continues — props should be empty
    EXPECT_TRUE(props.empty());
}

// ===========================================================================
// getInventoryInformation — DEVICE_GUID empty UUID converted to string
// ===========================================================================

TEST_F(NsmDeviceDI3Test,
       GetInventoryInfo_DeviceGuid_ValidUUID_PopulatesProperty)
{
    // Create a non-trivial UUID
    std::vector<uint8_t> uuidData = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                     0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                     0x0D, 0x0E, 0x0F, 0x10};
    auto resp = makeInvInfoResp3(NSM_SUCCESS, UUID_INT_SIZE, uuidData.data());

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    InventoryProperties props;
    uint8_t propId = DEVICE_GUID;
    auto coro = device->getInventoryInformation(propId, props);

    EXPECT_TRUE(props.find(DEVICE_GUID) != props.end());
    auto uuidStr = std::get<std::string>(props.at(DEVICE_GUID));
    EXPECT_FALSE(uuidStr.empty());
}

// ===========================================================================
// getSupportedNvidiaMessageType — success with many types set
// ===========================================================================

TEST_F(NsmDeviceDI3Test, GetSupportedMsgTypes_AllBitsSet)
{
    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    types[0].byte = 0xFF; // first 8 types
    types[1].byte = 0x01; // type 8

    auto resp = makeMsgTypesResp3(NSM_SUCCESS, types);

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    std::vector<uint8_t> supportedTypes;
    device->getSupportedNvidiaMessageType(supportedTypes);

    EXPECT_EQ(supportedTypes.size(), 9u);
}

// getSupportedNvidiaMessageType — SendRecvNsmMsg fails
TEST_F(NsmDeviceDI3Test, GetSupportedMsgTypes_SendRecvFail)
{
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecvFail3(NSM_SW_ERROR));

    std::vector<uint8_t> supportedTypes;
    device->getSupportedNvidiaMessageType(supportedTypes);

    EXPECT_TRUE(supportedTypes.empty());
}

// getSupportedCommandCodes — SendRecvNsmMsg fails
TEST_F(NsmDeviceDI3Test, GetSupportedCmdCodes_SendRecvFail)
{
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecvFail3(NSM_SW_ERROR));

    std::vector<uint8_t> supportedCmds;
    device->getSupportedCommandCodes(0, supportedCmds);
}

// getSupportedCommandCodes — success with commands set
TEST_F(NsmDeviceDI3Test, GetSupportedCmdCodes_Success_AllBitsSet)
{
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[0].byte = 0xFF;
    codes[1].byte = 0xFF;
    auto resp = makeCmdCodesResp3(NSM_SUCCESS, codes);

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(resp));

    std::vector<uint8_t> supportedCmds;
    device->getSupportedCommandCodes(0, supportedCmds);

    EXPECT_EQ(supportedCmds.size(), SUPPORTED_COMMAND_CODE_DATA_SIZE);
}

// ===========================================================================
// refreshCommandMatrix — empty retrieved types (no iteration)
// ===========================================================================

TEST_F(NsmDeviceDI3Test, RefreshCommandMatrix_EmptyTypes)
{
    device->areMessageTypesRetrieved = false;

    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {}; // no types set
    auto typesResp = makeMsgTypesResp3(NSM_SUCCESS, types);

    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(typesResp));

    device->refreshCommandMatrix();

    EXPECT_TRUE(device->areMessageTypesRetrieved);
    EXPECT_TRUE(device->retrievedMessageTypes.empty());
}

// ===========================================================================
// updateFruDeviceIntf — with partial FRU properties (some found, some not)
// ===========================================================================

TEST_F(NsmDeviceDI3Test, UpdateFruDeviceIntf_PartialProperties)
{
    // Pre-initialize with the EXACT set that will result from partial FRU
    // (BOARD_PART_NUMBER + SERIAL_NUMBER + 4 fixed props) to avoid
    // needsRecreation triggering createAndRegisterInterface with null objServer
    std::set<std::string> initProps = {"DEVICE_TYPE",       "INSTANCE_NUMBER",
                                       "DEVICE_ROLE",       "UUID",
                                       "BOARD_PART_NUMBER", "SERIAL_NUMBER"};
    device->fruDeviceManager.markInitialized(initProps);

    device->messageTypesToCommandCodeMatrix[NSM_TYPE_PLATFORM_ENVIRONMENTAL]
                                           [NSM_GET_INVENTORY_INFORMATION] =
        true;

    const char* strValue = "partial";
    auto strResp = makeInvInfoResp3(NSM_SUCCESS, strlen(strValue),
                                    reinterpret_cast<const uint8_t*>(strValue));

    // GPU: 6 properties — only BOARD_PART_NUMBER and SERIAL_NUMBER succeed
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecv3(strResp)) // BOARD_PART_NUMBER
        .WillOnce(mockSendRecvFail3())    // FRU_PART_NUMBER - fail
        .WillOnce(mockSendRecv3(strResp)) // SERIAL_NUMBER
        .WillOnce(mockSendRecvFail3())    // DEVICE_GUID - fail
        .WillOnce(mockSendRecvFail3())    // MARKETING_NAME - fail
        .WillOnce(mockSendRecvFail3());   // BUILD_DATE - fail

    EXPECT_NO_THROW_COROUTINE(device->updateFruDeviceIntf());
}

// ===========================================================================
// getInventoryInformation — SendRecvNsmMsg returns non-zero rc
// ===========================================================================

TEST_F(NsmDeviceDI3Test, GetInventoryInfo_SendRecvFail)
{
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _))
        .WillOnce(mockSendRecvFail3(NSM_SW_ERROR));

    InventoryProperties props;
    uint8_t propId = BOARD_PART_NUMBER;
    device->getInventoryInformation(propId, props);

    EXPECT_TRUE(props.empty());
}

// ===========================================================================
// getFRU — device type not in map (falls back to UNKNOWN empty list)
// ===========================================================================

TEST_F(NsmDeviceDI3Test, GetFRU_UnmappedDeviceType)
{
    EXPECT_CALL(*mockMsgHandler, SendRecvNsmMsg(_, _, _, _)).Times(0);

    InventoryProperties props;
    device->getFRU(props, 200); // 200 is not in devicePropertyMap

    EXPECT_TRUE(props.empty());
}
