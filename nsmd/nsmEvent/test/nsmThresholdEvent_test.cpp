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
 * Tests for nsmd/nsmEvent/nsmThresholdEvent.cpp
 *
 *   - NsmThresholdEvent constructor (valid info)
 *   - NsmThresholdEvent constructor (invalid messageId → throws)
 *   - NsmThresholdEvent constructor (empty messageArgs → throws)
 *   - NsmThresholdEvent::handle (decode failure → returns error)
 *   - NsmThresholdEvent::handle (success, no error bits set)
 *   - NsmThresholdEvent::handle (success, all error bits set)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "network-ports.h"

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmEvent/nsmThresholdEvent.hpp"

using namespace nsm;

static const std::string kValidMessageId =
    "ResourceEvent.1.0.ResourceErrorsDetected";

// Build a valid NsmEventInfo for NsmThresholdEvent
static NsmEventInfo makeThresholdInfo()
{
    NsmEventInfo info{};
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    info.messageId = kValidMessageId;
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.loggingNamespace = "GPU_Threshold";
    info.resolution = "Check threshold logs";
    info.messageArgs = {"PortThreshold"};
    info.severity = Level::Critical;
    return info;
}

// Encode a health event into a buffer
static std::vector<uint8_t>
    buildHealthEvent(const nsm_health_event_payload& payload)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + sizeof(payload), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_health_event(0, false, &payload, msg);
    return buf;
}

struct NsmThresholdEventTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmThresholdEventTest() : SensorManagerTest(devices) {}

    ~NsmThresholdEventTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// Constructor tests
// =============================================================================

TEST_F(NsmThresholdEventTest, Constructor_ValidInfo_Succeeds)
{
    auto info = makeThresholdInfo();
    EXPECT_NO_THROW(
        { NsmThresholdEvent event("threshold", "Threshold", info); });
}

TEST_F(NsmThresholdEventTest, Constructor_InvalidMessageId_Throws)
{
    auto info = makeThresholdInfo();
    info.messageId = "SomeOther.MessageId";
    EXPECT_THROW({ NsmThresholdEvent event("threshold", "Threshold", info); },
                 std::invalid_argument);
}

TEST_F(NsmThresholdEventTest, Constructor_EmptyMessageArgs_Throws)
{
    auto info = makeThresholdInfo();
    info.messageArgs.clear();
    EXPECT_THROW({ NsmThresholdEvent event("threshold", "Threshold", info); },
                 std::invalid_argument);
}

// errorId well-formed with matching key → constructor stores it for later
// lookup in handle(); behavioural smoke check that ctor still succeeds.
TEST_F(NsmThresholdEventTest, Constructor_ValidErrorId_Succeeds)
{
    auto info = makeThresholdInfo();
    info.errorId = {"Threshold", "GPU-THRESHOLD-EVENT"};
    EXPECT_NO_THROW(
        { NsmThresholdEvent event("threshold", "Threshold", info); });
}

// =============================================================================
// handle – decode failure (buffer too small)
// =============================================================================

TEST_F(NsmThresholdEventTest, Handle_ShortBuffer_ReturnsError)
{
    auto info = makeThresholdInfo();
    NsmThresholdEvent event("threshold", "Threshold", info);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handle – success, no error bits set
// =============================================================================

TEST_F(NsmThresholdEventTest, Handle_NoErrorBits_ReturnsSuccess)
{
    auto info = makeThresholdInfo();
    NsmThresholdEvent event("threshold", "Threshold", info);

    nsm_health_event_payload payload = {};
    payload.portNumber = 0;
    // All threshold bits = 0

    auto buf = buildHealthEvent(payload);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handle – success, all error bits set
// =============================================================================

TEST_F(NsmThresholdEventTest, Handle_AllErrorBitsSet_ReturnsSuccess)
{
    auto info = makeThresholdInfo();
    NsmThresholdEvent event("threshold", "Threshold", info);

    nsm_health_event_payload payload = {};
    payload.portNumber = 1;
    payload.port_rcv_errors_threshold = 1;
    payload.port_xmit_discard_threshold = 1;
    payload.symbol_ber_threshold = 1;
    payload.port_rcv_remote_physical_errors_threshold = 1;
    payload.port_rcv_switch_relay_errors_threshold = 1;
    payload.effective_ber_threshold = 1;
    payload.estimated_effective_ber_threshold = 1;

    auto buf = buildHealthEvent(payload);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// handle – success, errorId well-formed → exercises the new
// getEventErrorId append branch inside handle().
TEST_F(NsmThresholdEventTest, Handle_WithValidErrorId_ReturnsSuccess)
{
    auto info = makeThresholdInfo();
    info.errorId = {"Threshold", "GPU-THRESHOLD-EVENT"};
    NsmThresholdEvent event("threshold", "Threshold", info);

    nsm_health_event_payload payload = {};
    payload.portNumber = 0;
    payload.port_rcv_errors_threshold = 1;

    auto buf = buildHealthEvent(payload);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// handle – success, errorId wrong key → getEventErrorId returns "" →
// no append branch exercised.
TEST_F(NsmThresholdEventTest, Handle_WithWrongErrorIdKey_ReturnsSuccess)
{
    auto info = makeThresholdInfo();
    info.errorId = {"Other", "X"};
    NsmThresholdEvent event("threshold", "Threshold", info);

    nsm_health_event_payload payload = {};
    payload.portNumber = 0;
    payload.symbol_ber_threshold = 1;

    auto buf = buildHealthEvent(payload);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// handle – success, errorId odd-sized → getEventErrorId logs error and
// returns "" → no append.
TEST_F(NsmThresholdEventTest, Handle_WithOddSizedErrorId_ReturnsSuccess)
{
    auto info = makeThresholdInfo();
    info.errorId = {"Threshold"};
    NsmThresholdEvent event("threshold", "Threshold", info);

    nsm_health_event_payload payload = {};
    payload.portNumber = 0;
    payload.effective_ber_threshold = 1;

    auto buf = buildHealthEvent(payload);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handle – deviceToPortMap TRUE branch, port number found → messageArg appended
// Covers: if (deviceToPortMap.find(...) != end()) TRUE
//         if (deviceToPortMap.find(portNumber) != end()) TRUE  (line 115→118)
// =============================================================================

TEST_F(NsmThresholdEventTest, Handle_PortMapHit_MessageArgAppended)
{
    auto info = makeThresholdInfo();
    NsmThresholdEvent event("threshold", "Threshold", info);

    // Pre-create the device so we can look it up and set portMap.
    auto device = mockManager.getNsmDeviceFromStaticUUID(info.uuid);
    ASSERT_NE(device, nullptr);

    const uint8_t portNumber = 3;
    mockManager.deviceToPortMap[device] = {{portNumber, "NVLink_Port_3"}};

    nsm_health_event_payload payload = {};
    payload.portNumber = portNumber; // matches the portMap entry

    auto buf = buildHealthEvent(payload);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Clean up to avoid leaking into next tests
    mockManager.deviceToPortMap.erase(device);
}

// =============================================================================
// handle – deviceToPortMap TRUE branch, port number NOT found → no append
// Covers: if (deviceToPortMap.find(...) != end()) TRUE
//         if (deviceToPortMap.find(portNumber) != end()) FALSE  (line 115)
// =============================================================================

TEST_F(NsmThresholdEventTest, Handle_PortMapMiss_NoAppend)
{
    auto info = makeThresholdInfo();
    NsmThresholdEvent event("threshold", "Threshold", info);

    // Pre-create the device and put a DIFFERENT port number in the map
    auto device = mockManager.getNsmDeviceFromStaticUUID(info.uuid);
    ASSERT_NE(device, nullptr);

    mockManager.deviceToPortMap[device] = {{uint8_t(7), "NVLink_Port_7"}};

    nsm_health_event_payload payload = {};
    payload.portNumber = 4; // does NOT match portMap entry (which has 7)

    auto buf = buildHealthEvent(payload);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    mockManager.deviceToPortMap.erase(device);
}

// =============================================================================
// createThresholdEventFeature: param defaults and error paths
// =============================================================================

static const uuid_t kThresholdUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
static const std::string kThresholdMsgId =
    "ResourceEvent.1.0.ResourceErrorsDetected";

struct NsmThresholdEventFeatureTest : public NsmThresholdEventTest
{
    std::shared_ptr<NsmDevice> gpu;

    NsmThresholdEventFeatureTest()
    {
        gpu = mockManager.getNsmDeviceFromStaticUUID(kThresholdUuid);
        EXPECT_NE(gpu, nullptr);
    }

    // Minimum valid params for a successful creator run
    static dbus::PropertyMap baseParams()
    {
        return {
            {"MessageId", kThresholdMsgId},
            {"MessageArgs", std::vector<std::string>{"PortThreshold"}},
        };
    }

    requester::Coroutine create(const dbus::PropertyMap& params,
                                bool withDevice = true)
    {
        features::DeviceContext dev;
        dev.name = "GPU_1";
        dev.kind = "gpu";
        dev.staticUuid = kThresholdUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = gpu;
        }
        return features::createThresholdEventFeature(mockManager, dev, params);
    }

    std::shared_ptr<NsmThresholdEvent> lastEvent()
    {
        if (!gpu || gpu->deviceEvents.empty())
        {
            return nullptr;
        }
        return std::dynamic_pointer_cast<NsmThresholdEvent>(
            gpu->deviceEvents.back());
    }
};

// No NsmDevice in the context → NSM_ERROR, nothing added
TEST_F(NsmThresholdEventFeatureTest, Factory_NoDevice_ReturnsError)
{
    const size_t before = gpu->deviceEvents.size();
    auto rc = create(baseParams(), false);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(gpu->deviceEvents.size(), before);
}

// MessageId absent → info.messageId="" → NsmThresholdEvent ctor throws
// (invalid messageId string)
TEST_F(NsmThresholdEventFeatureTest, Factory_MissingMessageId_Throws)
{
    auto params = baseParams();
    params.erase("MessageId");

    EXPECT_THROW_COROUTINE(create(params), std::exception);
}

// MessageArgs absent → info.messageArgs={} → NsmThresholdEvent ctor throws
// (empty messageArgs vector)
TEST_F(NsmThresholdEventFeatureTest, Factory_MissingMessageArgs_Throws)
{
    auto params = baseParams();
    params.erase("MessageArgs");

    EXPECT_THROW_COROUTINE(create(params), std::exception);
}

// Severity absent → convertStringToLevel("...Level.") → nullopt →
// value_or(Level::Critical) → event IS created
TEST_F(NsmThresholdEventFeatureTest, Factory_MissingSeverity_UsesDefault)
{
    const size_t before = gpu->deviceEvents.size();
    auto rc = create(baseParams());

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceEvents.size(), before + 1);
    auto event = lastEvent();
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->info.severity, Level::Critical);
}

TEST_F(NsmThresholdEventFeatureTest, Factory_SeverityWarning_Applied)
{
    auto params = baseParams();
    params["Severity"] = std::string("Warning");
    auto rc = create(params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    auto event = lastEvent();
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->info.severity, Level::Warning);
}

// Name absent → name="" → makeDBusNameValid("") → event IS created
TEST_F(NsmThresholdEventFeatureTest, Factory_MissingName_SensorCreated)
{
    const size_t before = gpu->deviceEvents.size();
    auto rc = create(baseParams());

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceEvents.size(), before + 1);
}

// OriginOfCondition is not a param any more: it is derived from the device
TEST_F(NsmThresholdEventFeatureTest, Factory_OriginOfConditionFromDeviceName)
{
    auto rc = create(baseParams());

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    auto event = lastEvent();
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->info.originOfCondition, "/redfish/v1/Chassis/GPU_1");
    EXPECT_EQ(event->info.uuid, kThresholdUuid);
}

// LoggingNamespace / Resolution absent → empty strings, event IS created
TEST_F(NsmThresholdEventFeatureTest, Factory_MissingNamespaceAndResolution)
{
    auto params = baseParams();
    params["Name"] = std::string("TestThreshold");
    auto rc = create(params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    auto event = lastEvent();
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->getName(), "TestThreshold");
    EXPECT_TRUE(event->info.loggingNamespace.empty());
    EXPECT_TRUE(event->info.resolution.empty());
}

// LoggingNamespace / Resolution present → copied into the event info
TEST_F(NsmThresholdEventFeatureTest, Factory_WithNamespaceAndResolution)
{
    auto params = baseParams();
    params["LoggingNamespace"] = std::string("TestNamespace");
    params["Resolution"] = std::string("Check threshold logs");
    auto rc = create(params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    auto event = lastEvent();
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->info.loggingNamespace, "TestNamespace");
    EXPECT_EQ(event->info.resolution, "Check threshold logs");
}

// EventIds present → stored in info.errorId
TEST_F(NsmThresholdEventFeatureTest, Factory_WithEventIds_SensorCreated)
{
    auto params = baseParams();
    params["Name"] = std::string("TestThreshold");
    params["Severity"] = std::string("Critical");
    params["EventIds"] = std::vector<std::string>{"Threshold",
                                                  "GPU-THRESHOLD-EVENT"};
    auto rc = create(params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    auto event = lastEvent();
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->info.errorId.size(), 2u);
}
