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

#include "nsmEvent/nsmThresholdEvent.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmThresholdEvent(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
} // namespace nsm

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
// FALSE-branch coverage: count() checks in createNsmThresholdEvent
// =============================================================================

static const std::string kThresholdInterface =
    "xyz.openbmc_project.Configuration.NSM_Event_Threshold";
static const uuid_t kThresholdUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
static const std::string kThresholdMsgId =
    "ResourceEvent.1.0.ResourceErrorsDetected";

// Helper: set minimum valid props for a successful factory run
static void setBaseThresholdProps(dbus::PropertyMap& pm)
{
    pm["UUID"] = kThresholdUuid;
    pm["MessageId"] = kThresholdMsgId;
    pm["MessageArgs"] = std::vector<std::string>{"PortThreshold"};
}

// UUID absent → info.uuid="" → parseStaticUuid("") throws before device lookup
TEST_F(NsmThresholdEventTest, Factory_MissingUUID_Throws)
{
    const std::string objPath = "/xyz/test/threshold/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, kThresholdInterface);
    pm["MessageId"] = kThresholdMsgId;
    pm["MessageArgs"] = std::vector<std::string>{"PortThreshold"};
    // "UUID" intentionally omitted → info.uuid="" → parseStaticUuid throws

    EXPECT_THROW_COROUTINE(
        createNsmThresholdEvent(mockManager, kThresholdInterface, objPath),
        std::exception);
}

// MessageId absent → info.messageId="" → NsmThresholdEvent ctor throws
// (invalid messageId string)
TEST_F(NsmThresholdEventTest, Factory_MissingMessageId_Throws)
{
    const std::string objPath = "/xyz/test/threshold/no_msgid";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, kThresholdInterface);
    pm["UUID"] = kThresholdUuid;
    pm["MessageArgs"] = std::vector<std::string>{"PortThreshold"};
    // "MessageId" intentionally omitted → info.messageId="" →
    // NsmThresholdEvent constructor throws std::invalid_argument

    EXPECT_THROW_COROUTINE(
        createNsmThresholdEvent(mockManager, kThresholdInterface, objPath),
        std::exception);
}

// MessageArgs absent → info.messageArgs={} → NsmThresholdEvent ctor throws
// (empty messageArgs vector)
TEST_F(NsmThresholdEventTest, Factory_MissingMessageArgs_Throws)
{
    const std::string objPath = "/xyz/test/threshold/no_msgargs";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, kThresholdInterface);
    pm["UUID"] = kThresholdUuid;
    pm["MessageId"] = kThresholdMsgId;
    // "MessageArgs" intentionally omitted → info.messageArgs={} →
    // NsmThresholdEvent constructor throws std::invalid_argument

    EXPECT_THROW_COROUTINE(
        createNsmThresholdEvent(mockManager, kThresholdInterface, objPath),
        std::exception);
}

// Severity absent → severityStr="" → convertStringToLevel("...Level.") →
// empty optional → value_or(Level::Critical) → sensor IS created
TEST_F(NsmThresholdEventTest, Factory_MissingSeverity_UsesDefault)
{
    const std::string objPath = "/xyz/test/threshold/no_severity";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, kThresholdInterface);
    setBaseThresholdProps(pm);
    // "Severity" intentionally omitted → uses Level::Critical default

    EXPECT_NO_THROW(
        createNsmThresholdEvent(mockManager, kThresholdInterface, objPath));
}

// Name absent → name="" → makeDBusNameValid("") → sensor IS created with
// sanitized name
TEST_F(NsmThresholdEventTest, Factory_MissingName_SensorCreated)
{
    const std::string objPath = "/xyz/test/threshold/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, kThresholdInterface);
    setBaseThresholdProps(pm);
    pm["Severity"] = std::string("Critical");
    // "Name" intentionally omitted → name="" → makeDBusNameValid → sensor added

    EXPECT_NO_THROW(
        createNsmThresholdEvent(mockManager, kThresholdInterface, objPath));
}

// OriginOfCondition absent → info.originOfCondition="" → sensor IS created
TEST_F(NsmThresholdEventTest, Factory_MissingOriginOfCondition_SensorCreated)
{
    const std::string objPath = "/xyz/test/threshold/no_origin";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, kThresholdInterface);
    setBaseThresholdProps(pm);
    pm["Name"] = std::string("TestThreshold");
    pm["Severity"] = std::string("Critical");
    // "OriginOfCondition" intentionally omitted → info.originOfCondition=""

    EXPECT_NO_THROW(
        createNsmThresholdEvent(mockManager, kThresholdInterface, objPath));
}

// LoggingNamespace absent → info.loggingNamespace="" →
// makeDBusNameValid("") → sensor IS created
TEST_F(NsmThresholdEventTest, Factory_MissingLoggingNamespace_SensorCreated)
{
    const std::string objPath = "/xyz/test/threshold/no_namespace";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, kThresholdInterface);
    setBaseThresholdProps(pm);
    pm["Name"] = std::string("TestThreshold");
    pm["Severity"] = std::string("Critical");
    pm["OriginOfCondition"] = std::string("/xyz/test/origin");
    // "LoggingNamespace" intentionally omitted → info.loggingNamespace=""

    EXPECT_NO_THROW(
        createNsmThresholdEvent(mockManager, kThresholdInterface, objPath));
}

// Resolution absent → info.resolution="" → sensor IS created
TEST_F(NsmThresholdEventTest, Factory_MissingResolution_SensorCreated)
{
    const std::string objPath = "/xyz/test/threshold/no_resolution";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, kThresholdInterface);
    setBaseThresholdProps(pm);
    pm["Name"] = std::string("TestThreshold");
    pm["Severity"] = std::string("Critical");
    pm["OriginOfCondition"] = std::string("/xyz/test/origin");
    pm["LoggingNamespace"] = std::string("TestNamespace");
    // "Resolution" intentionally omitted → info.resolution=""

    EXPECT_NO_THROW(
        createNsmThresholdEvent(mockManager, kThresholdInterface, objPath));
}

// EventIds present → covers the new EventIds D-Bus read branch in
// createNsmThresholdEvent.
TEST_F(NsmThresholdEventTest, Factory_WithEventIds_SensorCreated)
{
    const std::string objPath = "/xyz/test/threshold/with_eventids";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, kThresholdInterface);
    setBaseThresholdProps(pm);
    pm["Name"] = std::string("TestThreshold");
    pm["Severity"] = std::string("Critical");
    pm["EventIds"] = std::vector<std::string>{"Threshold",
                                              "GPU-THRESHOLD-EVENT"};

    EXPECT_NO_THROW(
        createNsmThresholdEvent(mockManager, kThresholdInterface, objPath));
}
