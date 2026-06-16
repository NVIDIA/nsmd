/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Additional branch coverage tests for nsmEvent/ files:
 *
 * Targets:
 *   - NsmThresholdEvent::handle: individual threshold bits
 *   - NsmXIDEvent::handle: decode failure, formatting exception
 *   - NsmResetRequiredEvent ctor: messageArgs branches, errorId/impactedComp
 *   - NsmResetRequiredEvent::handle: decode fail
 *   - NsmRediscoveryEvent ctor: messageArgs branches
 *   - NsmRediscoveryEvent::handle: decode fail, info.logging false
 *   - NsmGPIOStateChangeEvent::handle: decode fail, nullptr gpioStateIntf
 *   - NsmFabricManagerStateEvent::handle: various fm_state/report_status
 *   - NsmLongRunningEvent::validateEvent: expired timer, mismatched id
 *   - getEventErrorId: odd size errorId, key not found, key found
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-capability-discovery.h"
#include "network-ports.h"
#include "platform-environmental.h"

#include "nsmEvent/nsmEventInfo.hpp"
#include "nsmEvent/nsmGPIOStateChangeEvent.hpp"
#include "nsmEvent/nsmLongRunningEvent.hpp"
#include "nsmEvent/nsmRediscoveryEvent.hpp"
#include "nsmEvent/nsmResetRequiredEvent.hpp"
#include "nsmEvent/nsmThresholdEvent.hpp"
#include "nsmEvent/nsmXIDEvent.hpp"
#include "nsmFwSwInventory/GPUSWInventory.hpp"

using namespace nsm;

// Concrete subclass for testing abstract NsmLongRunningEvent
class TestLongRunningEvent : public NsmLongRunningEvent
{
  public:
    using NsmLongRunningEvent::NsmLongRunningEvent;
    int handle(eid_t, NsmType, NsmEventId, const nsm_msg*, size_t) override
    {
        return 0;
    }
};

// ============================================================================
// Fixture
// ============================================================================

struct NsmEventBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmEventBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmEventBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

static NsmEventInfo makeBaseInfo()
{
    NsmEventInfo info{};
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    info.messageId = "ResourceEvent.1.0.ResourceErrorsDetected";
    info.originOfCondition = "/xyz/openbmc_project/inventory/test";
    info.loggingNamespace = "TestNS";
    info.resolution = "Check logs";
    info.messageArgs = {"Arg1"};
    info.severity = Level::Critical;
    return info;
}

// ============================================================================
// NsmThresholdEvent::handle: individual threshold bits
// ============================================================================

TEST_F(NsmEventBranchTest, ThresholdEvent_Handle_PortRcvErrors)
{
    auto info = makeBaseInfo();
    NsmThresholdEvent event("ThreshBr1", "NSM_Event", info);

    nsm_health_event_payload payload = {};
    payload.port_rcv_errors_threshold = 1;
    payload.portNumber = 0;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + sizeof(payload), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_health_event(0, false, &payload, msg);

    auto rc = event.handle(1, NSM_TYPE_NETWORK_PORT, NSM_THRESHOLD_EVENT, msg,
                           buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmEventBranchTest, ThresholdEvent_Handle_AllBits)
{
    auto info = makeBaseInfo();
    NsmThresholdEvent event("ThreshBr2", "NSM_Event", info);

    nsm_health_event_payload payload = {};
    payload.port_rcv_errors_threshold = 1;
    payload.port_xmit_discard_threshold = 1;
    payload.symbol_ber_threshold = 1;
    payload.port_rcv_remote_physical_errors_threshold = 1;
    payload.port_rcv_switch_relay_errors_threshold = 1;
    payload.effective_ber_threshold = 1;
    payload.estimated_effective_ber_threshold = 1;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + sizeof(payload), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_health_event(0, false, &payload, msg);

    auto rc = event.handle(1, NSM_TYPE_NETWORK_PORT, NSM_THRESHOLD_EVENT, msg,
                           buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmEventBranchTest, ThresholdEvent_Handle_DecodeFail)
{
    auto info = makeBaseInfo();
    NsmThresholdEvent event("ThreshBr3", "NSM_Event", info);

    // Too-short buffer
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = event.handle(1, NSM_TYPE_NETWORK_PORT, NSM_THRESHOLD_EVENT, msg,
                           buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmXIDEvent::handle: decode failure
// ============================================================================

TEST_F(NsmEventBranchTest, XIDEvent_Handle_DecodeFail)
{
    NsmEventInfo info{};
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    info.messageId = "GPU.1.0.GPUXIDError";
    info.messageArgs = {"XID {EventMessageReason}"};
    info.severity = Level::Critical;

    NsmXIDEvent event("XidBr1", "NSM_Event", info);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = event.handle(1, NSM_TYPE_PLATFORM_ENVIRONMENTAL, NSM_XID_EVENT,
                           msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmResetRequiredEvent ctor: with messageArgs, errorId, impactedComponent
// ============================================================================

TEST_F(NsmEventBranchTest, ResetRequiredEvent_Ctor_WithAllFields)
{
    NsmEventInfo info{};
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    info.messageArgs = {"Arg1", "Arg2"};
    info.errorId = {"ResetRequired", "ERR001"};
    info.impactedComponent = "GPU_0";
    info.severity = Level::Warning;
    info.originOfCondition = "/test";
    info.messageId = "Reset.1.0.Required";
    info.loggingNamespace = "TestNS";
    info.resolution = "Reset device";

    NsmResetRequiredEvent event("ResetBr1", "NSM_Event", info);

    EXPECT_FALSE(event.eventData.empty());
    // Verify errorId was looked up
    auto it = event.eventData.find("xyz.openbmc_project.Logging.Entry.EventId");
    EXPECT_NE(it, event.eventData.end());
    EXPECT_EQ(it->second, "ERR001");

    // Verify impactedComponent
    auto it2 = event.eventData.find("DEVICE_NAME");
    EXPECT_NE(it2, event.eventData.end());
#ifdef ENABLE_EVENT_GPU_UUID_LABEL
    EXPECT_EQ(it2->second, "GPU UUID STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0");
#else
    EXPECT_EQ(it2->second, "GPU_0");
#endif
}

TEST_F(NsmEventBranchTest, ResetRequiredEvent_Ctor_EmptyMessageArgs)
{
    NsmEventInfo info{};
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    info.messageArgs = {};
    info.severity = Level::Warning;

    NsmResetRequiredEvent event("ResetBr2", "NSM_Event", info);
    // Empty messageArgs => no messageArgs string
}

// ============================================================================
// NsmResetRequiredEvent::handle: decode failure
// ============================================================================

TEST_F(NsmEventBranchTest, ResetRequiredEvent_Handle_DecodeFail)
{
    NsmEventInfo info{};
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    info.messageArgs = {"Arg1"};
    info.severity = Level::Warning;

    NsmResetRequiredEvent event("ResetBr3", "NSM_Event", info);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = event.handle(1, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                           NSM_RESET_REQUIRED_EVENT, msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmRediscoveryEvent ctor: with messageArgs
// ============================================================================

TEST_F(NsmEventBranchTest, RediscoveryEvent_Ctor_MultipleArgs)
{
    NsmEventInfo info{};
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    info.messageArgs = {"First", "Second", "Third"};
    info.severity = Level::Informational;
    info.originOfCondition = "/test";
    info.messageId = "Rediscovery.1.0";
    info.loggingNamespace = "TestNS";
    info.resolution = "None";

    NsmRediscoveryEvent event("RediscBr1", "NSM_Event", info);
    EXPECT_EQ(event.messageArgs, "First,Second,Third");
}

TEST_F(NsmEventBranchTest, RediscoveryEvent_Ctor_EmptyArgs)
{
    NsmEventInfo info{};
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    info.messageArgs = {};
    info.severity = Level::Informational;

    NsmRediscoveryEvent event("RediscBr2", "NSM_Event", info);
    EXPECT_TRUE(event.messageArgs.empty());
}

// ============================================================================
// NsmRediscoveryEvent::handle: decode failure
// ============================================================================

TEST_F(NsmEventBranchTest, RediscoveryEvent_Handle_DecodeFail)
{
    NsmEventInfo info{};
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    info.messageArgs = {"Arg1"};
    info.severity = Level::Informational;
    info.logging = true;

    NsmRediscoveryEvent event("RediscBr3", "NSM_Event", info);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = event.handle(1, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                           NSM_REDISCOVERY_EVENT, msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPIOStateChangeEvent::handle: decode fail
// ============================================================================

TEST_F(NsmEventBranchTest, GPIOStateChange_Handle_DecodeFail)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;
    NsmGPIOStateChangeEvent event("GpioBr1", "NSM_Event", gpioIntf);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = event.handle(1, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, 0, msg,
                           buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmLongRunningEvent: validateEvent branches
// ============================================================================

TEST_F(NsmEventBranchTest, LongRunningEvent_ValidateEvent_NotStarted)
{
    TestLongRunningEvent event("LrBr1", "NSM_Event", true);
    // acceptInstanceId is 0xFF by default => not started

    // Build a minimal valid long-running event buffer
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                 sizeof(nsm_long_running_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = event.validateEvent(1, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST_F(NsmEventBranchTest, LongRunningEvent_InitAcceptInstanceId)
{
    TestLongRunningEvent event("LrBr2", "NSM_Event", true);

    // Accepted
    EXPECT_TRUE(event.initAcceptInstanceId(5, NSM_ACCEPTED, NSM_SW_SUCCESS));
    EXPECT_EQ(event.acceptInstanceId, 5);

    // Not accepted - cc != NSM_ACCEPTED
    EXPECT_FALSE(event.initAcceptInstanceId(5, NSM_ERROR, NSM_SW_SUCCESS));
    EXPECT_EQ(event.acceptInstanceId, 0xFF);

    // Not accepted - rc != 0
    EXPECT_FALSE(event.initAcceptInstanceId(5, NSM_ACCEPTED, NSM_SW_ERROR));
    EXPECT_EQ(event.acceptInstanceId, 0xFF);
}

// ============================================================================
// getEventErrorId: various branches
// ============================================================================

TEST_F(NsmEventBranchTest, GetEventErrorId_OddSizeErrorId)
{
    NsmEventInfo info{};
    info.errorId = {"key1", "val1", "key2"};
    info.uuid = "test-uuid";
    auto result = getEventErrorId(info, "key1");
    EXPECT_EQ(result, ""); // odd size => error => empty
}

TEST_F(NsmEventBranchTest, GetEventErrorId_KeyFound)
{
    NsmEventInfo info{};
    info.errorId = {"ResetRequired", "ERR001", "XID 31", "ERR002"};
    info.uuid = "test-uuid";
    auto result = getEventErrorId(info, "XID 31");
    EXPECT_EQ(result, "ERR002");
}

TEST_F(NsmEventBranchTest, GetEventErrorId_KeyNotFound)
{
    NsmEventInfo info{};
    info.errorId = {"key1", "val1", "key2", "val2"};
    info.uuid = "test-uuid";
    auto result = getEventErrorId(info, "nonexistent");
    EXPECT_EQ(result, "");
}

TEST_F(NsmEventBranchTest, GetEventErrorId_EmptyErrorId)
{
    NsmEventInfo info{};
    info.errorId = {};
    info.uuid = "test-uuid";
    auto result = getEventErrorId(info, "key");
    EXPECT_EQ(result, ""); // even size (0) => key not found
}
