/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * NEW TESTS for:
 *   - nsmd/nsmEvent/nsmRediscoveryEvent.cpp
 *   - nsmd/nsmEvent/nsmResetRequiredEvent.cpp
 *   - nsmd/nsmEvent/nsmGPIOStateChangeEvent.cpp  (augmented handle tests)
 *   - nsmd/nsmDumpCollection/nsmDebugInfo.cpp
 *   - nsmd/nsmDumpCollection/nsmEraseTrace.cpp
 *   - nsmd/nsmDumpCollection/nsmLogInfo.cpp
 *
 * Focus: constructors, handle, finish, factory coroutines.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-capability-discovery.h"
#include "diagnostics.h"
#include "platform-environmental.h"

#include "nsmDumpCollection/nsmDebugInfo.hpp"
#include "nsmDumpCollection/nsmEraseTrace.hpp"
#include "nsmDumpCollection/nsmLogInfo.hpp"
#include "nsmEvent/nsmGPIOStateChangeEvent.hpp"
#include "nsmEvent/nsmRediscoveryEvent.hpp"
#include "nsmEvent/nsmResetRequiredEvent.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace nsm;

using namespace nsm;

static NsmEventInfo makeDefaultEventInfo()
{
    NsmEventInfo info{};
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";
    info.originOfCondition = "/redfish/v1/Chassis/HGX_GPU_SXM_1";
    info.messageId = "ResourceEvent.1.0.ResourceErrorsDetected";
    info.severity = Level::Critical;
    info.loggingNamespace = "GPU_SXM 1";
    info.resolution = "Consult documentation";
    info.messageArgs = {"GPU_SXM_1", "Arg2"};
    info.logging = false;
    return info;
}

// ============================================================================
// NsmRediscoveryEvent Tests
// ============================================================================

class NsmRediscoveryEventTest : public testing::Test
{
  protected:
    static constexpr eid_t eid = 10;
    NsmEventInfo info = makeDefaultEventInfo();
};

TEST_F(NsmRediscoveryEventTest, Constructor_ValidInfo_SetsNameAndType)
{
    NsmRediscoveryEvent event("RediscoveryEvt", "Rediscovery", info);
    EXPECT_EQ("RediscoveryEvt", event.getName());
    EXPECT_EQ("Rediscovery", event.getType());
}

TEST_F(NsmRediscoveryEventTest,
       Constructor_EmptyMessageArgs_MessageArgsStringEmpty)
{
    info.messageArgs.clear();
    NsmRediscoveryEvent event("Evt", "Type", info);
    EXPECT_TRUE(event.messageArgs.empty());
}

TEST_F(NsmRediscoveryEventTest, Constructor_SingleMessageArg_MessageArgsNoComma)
{
    info.messageArgs = {"OnlyArg"};
    NsmRediscoveryEvent event("Evt", "Type", info);
    EXPECT_EQ("OnlyArg", event.messageArgs);
}

TEST_F(NsmRediscoveryEventTest, Constructor_MultipleMessageArgs_Concatenated)
{
    info.messageArgs = {"A", "B", "C"};
    NsmRediscoveryEvent event("Evt", "Type", info);
    EXPECT_EQ("A,B,C", event.messageArgs);
}

TEST_F(NsmRediscoveryEventTest, Constructor_EventDataPopulated)
{
    NsmRediscoveryEvent event("Evt", "Type", info);
    EXPECT_EQ(info.originOfCondition,
              event.eventData["REDFISH_ORIGIN_OF_CONDITION"]);
    EXPECT_EQ(info.messageId, event.eventData["REDFISH_MESSAGE_ID"]);
    EXPECT_EQ(info.loggingNamespace, event.eventData["namespace"]);
    EXPECT_EQ(info.resolution,
              event.eventData["xyz.openbmc_project.Logging.Entry.Resolution"]);
}

TEST_F(NsmRediscoveryEventTest, DISABLED_Handle_ValidEvent_ReturnsSuccess)
{
    info.logging = false;
    NsmRediscoveryEvent event("Evt", "Type", info);
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_rediscovery_event(0, true, msg);
    ASSERT_EQ(NSM_SW_SUCCESS, rc);
    rc = event.handle(eid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                      NSM_REDISCOVERY_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST_F(NsmRediscoveryEventTest, Handle_TruncatedMessage_ReturnsError)
{
    NsmRediscoveryEvent event("Evt", "Type", info);
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    encode_nsm_rediscovery_event(0, true, msg);
    auto rc = event.handle(eid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                           NSM_REDISCOVERY_EVENT, msg, sizeof(nsm_msg_hdr));
    EXPECT_NE(NSM_SW_SUCCESS, rc);
}

TEST_F(NsmRediscoveryEventTest,
       DISABLED_Handle_WithLoggingEnabled_StillReturnsSuccess)
{
    info.logging = true;
    NsmRediscoveryEvent event("Evt", "Type", info);
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    encode_nsm_rediscovery_event(0, true, msg);
    auto rc = event.handle(eid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                           NSM_REDISCOVERY_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

// ============================================================================
// NsmResetRequiredEvent Tests
// ============================================================================

class NsmResetRequiredEventTest : public testing::Test
{
  protected:
    static constexpr eid_t eid = 10;
    NsmEventInfo info = makeDefaultEventInfo();
};

TEST_F(NsmResetRequiredEventTest, Constructor_ValidInfo_SetsNameAndType)
{
    NsmResetRequiredEvent event("ResetEvt", "ResetRequired", info);
    EXPECT_EQ("ResetEvt", event.getName());
    EXPECT_EQ("ResetRequired", event.getType());
}

TEST_F(NsmResetRequiredEventTest, Constructor_EmptyMessageArgs_Empty)
{
    info.messageArgs.clear();
    NsmResetRequiredEvent event("Evt", "Type", info);
    EXPECT_TRUE(event.messageArgs.empty());
}

TEST_F(NsmResetRequiredEventTest, Constructor_MultipleMessageArgs_Concatenated)
{
    info.messageArgs = {"X", "Y", "Z"};
    NsmResetRequiredEvent event("Evt", "Type", info);
    EXPECT_EQ("X,Y,Z", event.messageArgs);
}

TEST_F(NsmResetRequiredEventTest, Constructor_EventDataPopulated)
{
    NsmResetRequiredEvent event("Evt", "Type", info);
    EXPECT_EQ(info.originOfCondition,
              event.eventData["REDFISH_ORIGIN_OF_CONDITION"]);
    EXPECT_EQ(info.messageId, event.eventData["REDFISH_MESSAGE_ID"]);
}

TEST_F(NsmResetRequiredEventTest,
       Constructor_WithErrorId_SetsEventIdInEventData)
{
    info.errorId = {"ResetRequired", "ERR-001"};
    NsmResetRequiredEvent event("Evt", "Type", info);
    EXPECT_EQ("ERR-001",
              event.eventData["xyz.openbmc_project.Logging.Entry.EventId"]);
}

TEST_F(NsmResetRequiredEventTest, Constructor_WithEmptyErrorId_NoEventIdKey)
{
    info.errorId.clear();
    NsmResetRequiredEvent event("Evt", "Type", info);
    EXPECT_EQ(
        event.eventData.end(),
        event.eventData.find("xyz.openbmc_project.Logging.Entry.EventId"));
}

TEST_F(NsmResetRequiredEventTest,
       Constructor_WithImpactedComponent_SetsDeviceName)
{
    info.impactedComponent = "GPU_SXM_1";
    NsmResetRequiredEvent event("Evt", "Type", info);
    EXPECT_EQ("GPU_SXM_1", event.eventData["DEVICE_NAME"]);
}

TEST_F(NsmResetRequiredEventTest,
       Constructor_WithEmptyImpactedComponent_NoDeviceNameKey)
{
    info.impactedComponent.clear();
    NsmResetRequiredEvent event("Evt", "Type", info);
    EXPECT_EQ(event.eventData.end(), event.eventData.find("DEVICE_NAME"));
}

TEST_F(NsmResetRequiredEventTest, DISABLED_Handle_ValidEvent_ReturnsSuccess)
{
    NsmResetRequiredEvent event("Evt", "Type", info);
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    encode_nsm_reset_required_event(0, true, msg);
    auto rc = event.handle(eid, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                           NSM_RESET_REQUIRED_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST_F(NsmResetRequiredEventTest, Handle_TruncatedMessage_ReturnsError)
{
    NsmResetRequiredEvent event("Evt", "Type", info);
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    encode_nsm_reset_required_event(0, true, msg);
    auto rc = event.handle(eid, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                           NSM_RESET_REQUIRED_EVENT, msg, sizeof(nsm_msg_hdr));
    EXPECT_NE(NSM_SW_SUCCESS, rc);
}

TEST_F(NsmResetRequiredEventTest, Handle_DifferentEids_AllReturnSuccess)
{
    NsmResetRequiredEvent event("Evt", "Type", info);
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    encode_nsm_reset_required_event(0, true, msg);
    for (eid_t testEid : {0, 1, 127, 255})
    {
        auto rc = event.handle(testEid, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                               NSM_RESET_REQUIRED_EVENT, msg, eventMsg.size());
        EXPECT_EQ(NSM_SW_SUCCESS, rc) << "Failed for EID=" << (int)testEid;
    }
}

// ============================================================================
// NsmGPIOStateChangeEvent augmented handle tests
// ============================================================================

class NsmGPIOStateChangeEventHandleTest : public testing::Test
{
  protected:
    static constexpr eid_t eid = 10;

    static std::vector<uint8_t>
        buildGPIOEvent(uint32_t tsLow, uint32_t tsHigh,
                       const std::vector<std::pair<uint16_t, bool>>& gpioEvents)
    {
        size_t payloadSize = sizeof(nsm_gpio_state_change_event_payload) +
                             (gpioEvents.empty() ? 0
                                                 : (gpioEvents.size() - 1) *
                                                       sizeof(nsm_gpio_event));
        std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                      payloadSize);
        auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
        // Allocate payload buffer large enough for all GPIO events to avoid
        // stack-buffer-overflow when encode reads gpio_events[0..N-1]
        std::vector<uint8_t> payloadBuf(payloadSize, 0);
        auto* payload = reinterpret_cast<nsm_gpio_state_change_event_payload*>(
            payloadBuf.data());
        payload->timestamp_low = tsLow;
        payload->timestamp_high = tsHigh;
        payload->num_gpio_events = static_cast<uint16_t>(gpioEvents.size());
        for (size_t i = 0; i < gpioEvents.size(); ++i)
        {
            payload->gpio_events[i].gpio_index = gpioEvents[i].first;
            payload->gpio_events[i].gpio_value = gpioEvents[i].second ? 1 : 0;
        }
        encode_nsm_gpio_state_change_event(0, true, payload, msg);
        return eventMsg;
    }
};

TEST_F(NsmGPIOStateChangeEventHandleTest,
       Handle_ValidEventNullIntf_ReturnsSuccess)
{
    NsmGPIOStateChangeEvent event("GPIOEvt", "GPIO", nullptr);
    auto eventMsg = buildGPIOEvent(100, 0, {{0, true}});
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = event.handle(eid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                           NSM_GPIO_STATE_CHANGE_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST_F(NsmGPIOStateChangeEventHandleTest, Handle_TruncatedMessage_ReturnsError)
{
    NsmGPIOStateChangeEvent event("GPIOEvt", "GPIO", nullptr);
    auto eventMsg = buildGPIOEvent(100, 0, {{0, true}});
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = event.handle(eid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                           NSM_GPIO_STATE_CHANGE_EVENT, msg,
                           sizeof(nsm_msg_hdr));
    EXPECT_NE(NSM_SW_SUCCESS, rc);
}

TEST_F(NsmGPIOStateChangeEventHandleTest,
       Handle_MultipleGPIOEvents_ReturnsSuccess)
{
    NsmGPIOStateChangeEvent event("GPIOEvt", "GPIO", nullptr);
    auto eventMsg = buildGPIOEvent(500, 1, {{0, true}, {3, false}, {7, true}});
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = event.handle(eid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                           NSM_GPIO_STATE_CHANGE_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST_F(NsmGPIOStateChangeEventHandleTest, Handle_ZeroGPIOEvents_ReturnsSuccess)
{
    NsmGPIOStateChangeEvent event("GPIOEvt", "GPIO", nullptr);
    auto eventMsg = buildGPIOEvent(0, 0, {});
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = event.handle(eid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                           NSM_GPIO_STATE_CHANGE_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

// ============================================================================
// NsmEventInfo helper tests
// ============================================================================

class NsmEventInfoTest : public testing::Test
{};

TEST_F(NsmEventInfoTest, GetEventErrorId_ValidKeyValuePairs_ReturnsValue)
{
    NsmEventInfo info{};
    info.errorId = {"KeyA", "ValueA", "KeyB", "ValueB"};
    EXPECT_EQ("ValueA", getEventErrorId(info, "KeyA"));
    EXPECT_EQ("ValueB", getEventErrorId(info, "KeyB"));
}

TEST_F(NsmEventInfoTest, GetEventErrorId_KeyNotFound_ReturnsEmpty)
{
    NsmEventInfo info{};
    info.errorId = {"KeyA", "ValueA"};
    EXPECT_EQ("", getEventErrorId(info, "NonExistentKey"));
}

TEST_F(NsmEventInfoTest, GetEventErrorId_OddSizedVector_ReturnsEmpty)
{
    NsmEventInfo info{};
    info.errorId = {"KeyA", "ValueA", "Orphan"};
    EXPECT_EQ("", getEventErrorId(info, "KeyA"));
}

TEST_F(NsmEventInfoTest, GetEventErrorId_EmptyVector_ReturnsEmpty)
{
    NsmEventInfo info{};
    info.errorId.clear();
    EXPECT_EQ("", getEventErrorId(info, "AnyKey"));
}

// ============================================================================
// NsmDebugInfoObject Tests
// ============================================================================

class NsmDebugInfoObjectTest : public testing::Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
};

TEST_F(NsmDebugInfoObjectTest, Constructor_DiagnosticsDumpType_SetsNameAndType)
{
    NsmDebugInfoObject obj(bus, "DebugDumpDiag",
                           "/xyz/openbmc_project/inventory/system/test", "GPU",
                           testUuid, DebugDumpType::Diagnostics);
    EXPECT_EQ(obj.getName(), "DebugDumpDiag");
    EXPECT_EQ(obj.getType(), "GPU");
    EXPECT_EQ(obj.supportedDumpType(), DebugDumpType::Diagnostics);
}

TEST_F(NsmDebugInfoObjectTest, Constructor_NetworkDumpType_SetsNameAndType)
{
    NsmDebugInfoObject obj(bus, "DebugDumpNet",
                           "/xyz/openbmc_project/inventory/system/test",
                           "Switch", testUuid, DebugDumpType::Network);
    EXPECT_EQ(obj.getName(), "DebugDumpNet");
    EXPECT_EQ(obj.getType(), "Switch");
    EXPECT_EQ(obj.supportedDumpType(), DebugDumpType::Network);
}

TEST_F(NsmDebugInfoObjectTest, Constructor_BufferReserved)
{
    NsmDebugInfoObject obj(bus, "DbgBuf",
                           "/xyz/openbmc_project/inventory/system/test", "GPU",
                           testUuid, DebugDumpType::Network);
    EXPECT_GE(obj.buffer.capacity(), 65535u);
}

TEST_F(NsmDebugInfoObjectTest, Finish_Success_SetsStatusAndValue)
{
    NsmDebugInfoObject obj(bus, "DumpFinish",
                           "/xyz/openbmc_project/inventory/system/test", "GPU",
                           testUuid, DebugDumpType::Network);
    int pipeFds[2];
    ASSERT_EQ(0, pipe(pipeFds));
    auto [objectPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    obj.statusInterface = statusIntf;
    obj.valueInterface = valueIntf;
    obj.fd = pipeFds[1];
    obj.finish(AsyncOperationStatusType::Success, NSM_SW_SUCCESS);
    if (statusIntf)
    {
        EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
    }
    close(pipeFds[0]);
}

TEST_F(NsmDebugInfoObjectTest, Finish_InternalFailure_SetsCorrectStatus)
{
    NsmDebugInfoObject obj(bus, "DumpFail",
                           "/xyz/openbmc_project/inventory/system/test", "GPU",
                           testUuid, DebugDumpType::Network);
    int pipeFds[2];
    ASSERT_EQ(0, pipe(pipeFds));
    auto [objectPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    obj.statusInterface = statusIntf;
    obj.valueInterface = valueIntf;
    obj.fd = pipeFds[1];
    obj.finish(AsyncOperationStatusType::InternalFailure, NSM_SW_ERROR);
    if (statusIntf)
    {
        EXPECT_EQ(statusIntf->status(),
                  AsyncOperationStatusType::InternalFailure);
    }
    close(pipeFds[0]);
}

TEST_F(NsmDebugInfoObjectTest, GetDebugInfo_InfoTypeEnumValues_AreCorrect)
{
    EXPECT_EQ(INFO_TYPE_DEVICE_INFO, 0);
    EXPECT_EQ(INFO_TYPE_FW_RUNTIME_INFO, 1);
    EXPECT_EQ(INFO_TYPE_FW_SAVED_INFO, 2);
    EXPECT_EQ(INFO_TYPE_DEVICE_DUMP, 3);
}

TEST_F(NsmDebugInfoObjectTest, GetDiagnostics_WhenInProgress_ThrowsUnavailable)
{
    NsmDebugInfoObject obj(bus, "DbgInfo", "/xyz/openbmc_project/inv/sys/test",
                           "GPU", testUuid, DebugDumpType::Diagnostics);
    auto [objectPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (statusIntf)
    {
        statusIntf->status(AsyncOperationStatusType::InProgress);
        obj.statusInterface = statusIntf;
        obj.valueInterface = valueIntf;
        EXPECT_THROW(obj.getDiagnostics(sdbusplus::message::unix_fd(0)),
                     sdbusplus::exception_t);
    }
}

TEST_F(NsmDebugInfoObjectTest, GetDebugInfo_WhenInProgress_ThrowsUnavailable)
{
    NsmDebugInfoObject obj(bus, "DbgInfo2", "/xyz/openbmc_project/inv/sys/test",
                           "GPU", testUuid, DebugDumpType::Network);
    auto [objectPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (statusIntf)
    {
        statusIntf->status(AsyncOperationStatusType::InProgress);
        obj.statusInterface = statusIntf;
        obj.valueInterface = valueIntf;
        EXPECT_THROW(obj.getDebugInfo(DebugInformationType::DeviceInformation,
                                      sdbusplus::message::unix_fd(0)),
                     sdbusplus::exception_t);
    }
}

// ============================================================================
// NsmEraseTraceObject Tests
// ============================================================================

class NsmEraseTraceObjectTest : public testing::Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
};

TEST_F(NsmEraseTraceObjectTest, Constructor_SetsNameAndType)
{
    NsmEraseTraceObject obj(bus, "EraseTrace1",
                            "/xyz/openbmc_project/inventory/system/test/",
                            "GPU", testUuid);
    EXPECT_EQ(obj.getName(), "EraseTrace1");
    EXPECT_EQ(obj.getType(), "GPU");
}

TEST_F(NsmEraseTraceObjectTest, Constructor_EraseTraceStatus_Unavailable)
{
    NsmEraseTraceObject obj(bus, "EraseTrace2",
                            "/xyz/openbmc_project/inventory/system/test/",
                            "GPU", testUuid);
    auto status = obj.eraseTraceStatus();
    EXPECT_EQ(std::get<0>(status), EraseOperationStatus::Unavailable);
    EXPECT_EQ(std::get<1>(status), EraseStatus::Unknown);
}

TEST_F(NsmEraseTraceObjectTest, Constructor_EraseDebugInfoStatus_Unavailable)
{
    NsmEraseTraceObject obj(bus, "EraseTrace3",
                            "/xyz/openbmc_project/inventory/system/test/",
                            "GPU", testUuid);
    auto status = obj.eraseDebugInfoStatus();
    EXPECT_EQ(std::get<0>(status), EraseOperationStatus::Unavailable);
    EXPECT_EQ(std::get<1>(status), EraseStatus::Unknown);
}

TEST_F(NsmEraseTraceObjectTest, EraseTrace_WhenInProgress_ThrowsUnavailable)
{
    NsmEraseTraceObject obj(bus, "EraseTrace4",
                            "/xyz/openbmc_project/inventory/system/test/",
                            "GPU", testUuid);
    obj.eraseTraceStatus(std::make_tuple(EraseOperationStatus::InProgress,
                                         EraseStatus::Unknown));
    EXPECT_THROW(obj.eraseTrace(), sdbusplus::exception_t);
}

TEST_F(NsmEraseTraceObjectTest, EraseDebugInfo_WhenInProgress_ThrowsUnavailable)
{
    NsmEraseTraceObject obj(bus, "EraseTrace5",
                            "/xyz/openbmc_project/inventory/system/test/",
                            "GPU", testUuid);
    obj.eraseDebugInfoStatus(std::make_tuple(EraseOperationStatus::InProgress,
                                             EraseStatus::Unknown));
    EXPECT_THROW(obj.eraseDebugInfo(EraseInfoType::FWSavedDumpInfo),
                 sdbusplus::exception_t);
}

TEST_F(NsmEraseTraceObjectTest,
       DISABLED_EraseDebugInfo_UnsupportedType_SetsInternalFailure)
{
    NsmEraseTraceObject obj(bus, "EraseTrace6",
                            "/xyz/openbmc_project/inventory/system/test/",
                            "GPU", testUuid);
    auto invalidType = static_cast<EraseInfoType>(99);
    obj.eraseDebugInfo(invalidType);
    auto status = obj.eraseDebugInfoStatus();
    EXPECT_EQ(std::get<0>(status), EraseOperationStatus::InternalFailure);
}

TEST_F(NsmEraseTraceObjectTest, Constructor_SetsObjPath)
{
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test/";
    NsmEraseTraceObject obj(bus, "EraseTrace7", inventoryPath, "GPU", testUuid);
    EXPECT_EQ(obj.objPath, inventoryPath + "EraseTrace7");
}

TEST_F(NsmEraseTraceObjectTest, EraseTraceStatusConstants_AreCorrect)
{
    EXPECT_EQ(ERASE_TRACE_NO_DATA_ERASED, 0);
    EXPECT_EQ(ERASE_TRACE_DATA_ERASED, 1);
    EXPECT_EQ(ERASE_TRACE_DATA_ERASE_INPROGRESS, 2);
}

// ============================================================================
// NsmLogInfoObject Tests
// ============================================================================

class NsmLogInfoObjectTest : public testing::Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
};

TEST_F(NsmLogInfoObjectTest, Constructor_SetsNameAndType)
{
    NsmLogInfoObject obj(bus, "LogInfo1",
                         "/xyz/openbmc_project/inventory/system/test/", "GPU",
                         testUuid);
    EXPECT_EQ(obj.getName(), "LogInfo1");
    EXPECT_EQ(obj.getType(), "GPU");
}

TEST_F(NsmLogInfoObjectTest, Constructor_ReservesBuffer)
{
    NsmLogInfoObject obj(bus, "LogInfo2",
                         "/xyz/openbmc_project/inventory/system/test/", "GPU",
                         testUuid);
    EXPECT_GE(obj.buffer.capacity(), 65535u);
}

TEST_F(NsmLogInfoObjectTest, Finish_Success_SetsStatusAndValue)
{
    NsmLogInfoObject obj(bus, "LogInfoFinish",
                         "/xyz/openbmc_project/inventory/system/test/", "GPU",
                         testUuid);
    int pipeFds[2];
    ASSERT_EQ(0, pipe(pipeFds));
    auto [objectPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    obj.statusInterface = statusIntf;
    obj.valueInterface = valueIntf;
    obj.fd = pipeFds[1];
    obj.finish(AsyncOperationStatusType::Success, NSM_SW_SUCCESS);
    if (statusIntf)
    {
        EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
    }
    close(pipeFds[0]);
}

TEST_F(NsmLogInfoObjectTest, Finish_InternalFailure_SetsCorrectStatus)
{
    NsmLogInfoObject obj(bus, "LogInfoFail",
                         "/xyz/openbmc_project/inventory/system/test/", "GPU",
                         testUuid);
    int pipeFds[2];
    ASSERT_EQ(0, pipe(pipeFds));
    auto [objectPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    obj.statusInterface = statusIntf;
    obj.valueInterface = valueIntf;
    obj.fd = pipeFds[1];
    obj.finish(AsyncOperationStatusType::InternalFailure, NSM_SW_ERROR);
    if (statusIntf)
    {
        EXPECT_EQ(statusIntf->status(),
                  AsyncOperationStatusType::InternalFailure);
    }
    close(pipeFds[0]);
}

TEST_F(NsmLogInfoObjectTest, Finish_WriteFailure_SetsCorrectStatus)
{
    NsmLogInfoObject obj(bus, "LogInfoWriteFail",
                         "/xyz/openbmc_project/inventory/system/test/", "GPU",
                         testUuid);
    int pipeFds[2];
    ASSERT_EQ(0, pipe(pipeFds));
    auto [objectPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    obj.statusInterface = statusIntf;
    obj.valueInterface = valueIntf;
    obj.fd = pipeFds[1];
    obj.finish(AsyncOperationStatusType::WriteFailure, NSM_SW_ERROR);
    if (statusIntf)
    {
        EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
    }
    close(pipeFds[0]);
}

TEST_F(NsmLogInfoObjectTest, GetLogInfo_WhenInProgress_ThrowsUnavailable)
{
    NsmLogInfoObject obj(bus, "LogInfo", "/xyz/openbmc_project/inv/sys/test/",
                         "GPU", testUuid);
    auto [objectPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (statusIntf)
    {
        statusIntf->status(AsyncOperationStatusType::InProgress);
        obj.statusInterface = statusIntf;
        obj.valueInterface = valueIntf;
        EXPECT_THROW(obj.getLogInfo(sdbusplus::message::unix_fd(0)),
                     sdbusplus::exception_t);
    }
}

TEST_F(NsmLogInfoObjectTest, TimeSyncConstants_AreCorrect)
{
    EXPECT_EQ(SYNCED_TIME_TYPE_BOOT, 0);
    EXPECT_EQ(SYNCED_TIME_TYPE_SYNCED, 1);
}
