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
 * Tests for nsmd/nsmEvent/nsmRediscoveryEvent.cpp
 *
 *   - NsmRediscoveryEvent constructor (no messageArgs)
 *   - NsmRediscoveryEvent constructor (one messageArg)
 *   - NsmRediscoveryEvent constructor (multiple messageArgs joined with comma)
 *   - NsmRediscoveryEvent constructor (logging=false does not change eventData)
 *   - NsmRediscoveryEvent::handle (decode failure → returns error)
 *
 * NOTE: NsmRediscoveryEvent::handle() success path requires
 * mctp::MctpDiscovery to be initialized (deleted constructor singleton),
 * so only the decode-failure path is tested here.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "device-capability-discovery.h"
#include "platform-environmental.h"

#include "nsmEvent/nsmRediscoveryEvent.hpp"
#include "nsmFwSwInventory/GPUSWInventory.hpp"
#include "nsmObjectFactory.hpp"

using namespace nsm;

// Build a valid NsmEventInfo for NsmRediscoveryEvent
static NsmEventInfo makeRediscoveryInfo(bool logging = false)
{
    NsmEventInfo info{};
    info.messageId = "ResourceEvent.1.0.ResourceChanged";
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.loggingNamespace = "GPU_Rediscovery";
    info.resolution = "No action required";
    info.messageArgs = {"GPU_1"};
    info.severity = Level::Informational;
    info.logging = logging;
    return info;
}

// =============================================================================
// Constructor tests
// =============================================================================

TEST(NsmRediscoveryEvent, Constructor_NoMessageArgs_EmptyMessageArgs)
{
    auto info = makeRediscoveryInfo();
    info.messageArgs.clear();
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.getName(), "rediscovery");
    EXPECT_EQ(event.getType(), "NSM_Rediscovery");
    EXPECT_EQ(event.messageArgs, "");
}

TEST(NsmRediscoveryEvent, Constructor_OneMessageArg_MessageArgsSet)
{
    auto info = makeRediscoveryInfo();
    info.messageArgs = {"GPU_1"};
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.messageArgs, "GPU_1");
}

TEST(NsmRediscoveryEvent, Constructor_MultipleMessageArgs_JoinedWithComma)
{
    auto info = makeRediscoveryInfo();
    info.messageArgs = {"arg1", "arg2", "arg3"};
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.messageArgs, "arg1,arg2,arg3");
}

#ifdef ENABLE_EVENT_GPU_UUID_LABEL
// NsmEventInfo::uuid is the entity-manager static device-identification key
// (STATIC:d:d:s:s), never a device identity. It must never leak into the event
// label; see NVBug 6659807.
TEST(NsmRediscoveryEvent, Constructor_GpuMessageArg_DoesNotUseStaticUuid)
{
    auto info = makeRediscoveryInfo();
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    info.messageArgs = {"GPU_1 Rediscovery", "arg2"};
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.messageArgs, "GPU_1 Rediscovery,arg2");
    EXPECT_EQ(event.messageArgs.find("STATIC:"), std::string::npos);
}

TEST(NsmRediscoveryEvent, MessageArgs_WithDeviceUuid_UsesDeviceUuid)
{
    auto info = makeRediscoveryInfo();
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    info.messageArgs = {"GPU_1 Rediscovery", "arg2"};

    EXPECT_EQ(getUuidMessageArgs(info, "6463b098-ca8d-55c1-a65a-4d03c14fb86d"),
              "GPU UUID 6463b098-ca8d-55c1-a65a-4d03c14fb86d Rediscovery,arg2");
}

TEST(NsmRediscoveryEvent, MessageArgs_UnresolvedDeviceUuid_KeepsConfiguredName)
{
    auto info = makeRediscoveryInfo();
    info.uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    info.messageArgs = {"GPU_1 Rediscovery", "arg2"};

    EXPECT_EQ(getUuidMessageArgs(info, ""), "GPU_1 Rediscovery,arg2");
}
#endif // ENABLE_EVENT_GPU_UUID_LABEL

TEST(NsmRediscoveryEvent, Constructor_EventDataContainsExpectedKeys)
{
    auto info = makeRediscoveryInfo();
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.eventData.count("REDFISH_ORIGIN_OF_CONDITION"), 1u);
    EXPECT_EQ(event.eventData.count("REDFISH_MESSAGE_ARGS"), 1u);
    EXPECT_EQ(event.eventData.count("REDFISH_MESSAGE_ID"), 1u);
    EXPECT_EQ(event.eventData.count("namespace"), 1u);
    EXPECT_EQ(
        event.eventData.count("xyz.openbmc_project.Logging.Entry.Resolution"),
        1u);
}

// errorId vector empty → no EventId entry in eventData
TEST(NsmRediscoveryEvent, Constructor_EmptyErrorId_NoEventIdInEventData)
{
    auto info = makeRediscoveryInfo();
    info.messageArgs = {"GPU_1"};
    info.errorId = {};
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.messageArgs, "GPU_1");
    EXPECT_EQ(
        event.eventData.count("xyz.openbmc_project.Logging.Entry.EventId"), 0u);
}

// errorId well-formed with matching key → EventId set in eventData,
// messageArgs untouched.
TEST(NsmRediscoveryEvent, Constructor_ValidErrorId_PopulatesEventIdField)
{
    auto info = makeRediscoveryInfo();
    info.messageArgs = {"GPU_1", "rediscovery"};
    info.errorId = {"Rediscovery", "GPU-REDISCOVERY-EVENT"};
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.messageArgs, "GPU_1,rediscovery");
    EXPECT_EQ(event.eventData["xyz.openbmc_project.Logging.Entry.EventId"],
              "GPU-REDISCOVERY-EVENT");
}

// errorId well-formed but key does not match → getEventErrorId returns "" →
// no EventId entry.
TEST(NsmRediscoveryEvent, Constructor_WrongErrorIdKey_NoEventIdInEventData)
{
    auto info = makeRediscoveryInfo();
    info.messageArgs = {"GPU_1"};
    info.errorId = {"Other", "X"};
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.messageArgs, "GPU_1");
    EXPECT_EQ(
        event.eventData.count("xyz.openbmc_project.Logging.Entry.EventId"), 0u);
}

// errorId odd-sized vector → getEventErrorId logs error and returns "" →
// no EventId entry.
TEST(NsmRediscoveryEvent, Constructor_OddSizedErrorId_NoEventIdInEventData)
{
    auto info = makeRediscoveryInfo();
    info.messageArgs = {"GPU_1"};
    info.errorId = {"Rediscovery"};
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.messageArgs, "GPU_1");
    EXPECT_EQ(
        event.eventData.count("xyz.openbmc_project.Logging.Entry.EventId"), 0u);
}

// =============================================================================
// handle – decode failure (buffer too small)
// =============================================================================

TEST(NsmRediscoveryEvent, Handle_ShortBuffer_ReturnsError)
{
    auto info = makeRediscoveryInfo();
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// Valid rediscovery event buffer (decode succeeds) — MctpDiscovery not
// initialized → throws before nsmDevice lookup.
// logging=false → covers line 76 FALSE branch, skips logEventAsync block.
TEST(NsmRediscoveryEvent, Handle_ValidDecode_LoggingFalse_ThrowsMctpDiscovery)
{
    auto info = makeRediscoveryInfo(false); // logging = false
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_rediscovery_event(0, false, msg);
    const auto* cmsg = reinterpret_cast<const nsm_msg*>(buf.data());

    EXPECT_THROW(event.handle(5, {}, {}, cmsg, buf.size()), std::runtime_error);
}

// logging=true → covers lines 76-81 TRUE branch (logEventAsync + detach),
// then throws at MctpDiscovery::getInstance().
TEST(NsmRediscoveryEvent, Handle_ValidDecode_LoggingTrue_ThrowsMctpDiscovery)
{
    auto info = makeRediscoveryInfo(true); // logging = true
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_rediscovery_event(0, false, msg);
    const auto* cmsg = reinterpret_cast<const nsm_msg*>(buf.data());

    EXPECT_THROW(event.handle(5, {}, {}, cmsg, buf.size()), std::runtime_error);
}

// =============================================================================
// handleRediscovery tests
// =============================================================================

// isRediscoveryRequired=false → while body never executes → returns
// immediately. Covers the while(isRediscoveryRequired) false branch.
TEST(NsmRediscoveryEvent, HandleRediscovery_NotRequired_LoopSkipped)
{
    auto info = makeRediscoveryInfo();
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    auto mockDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        uint8_t(0), uint8_t(0), std::string("NSM_DEVICE_INSTANCE_NUMBER"),
        std::string("0"), uint8_t(0));

    event.isRediscoveryRequired = false;

    EXPECT_CALL(*mockDevice, updateNsmDevice()).Times(0);
    EXPECT_CALL(*mockDevice, refreshCapabilitySensor()).Times(0);

    event.handleRediscovery(mockDevice);

    EXPECT_FALSE(event.isRediscoveryRequired);
}

// isRediscoveryRequired=true, gpuDriverSensor=nullptr → else branch:
// calls updateNsmDevice() then refreshCapabilitySensor().
// After one iteration isRediscoveryRequired is set to false → loop exits.
TEST(NsmRediscoveryEvent,
     HandleRediscovery_Required_NoGpuDriver_CallsUpdateAndRefresh)
{
    auto info = makeRediscoveryInfo();
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    auto mockDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        uint8_t(0), uint8_t(0), std::string("NSM_DEVICE_INSTANCE_NUMBER"),
        std::string("0"), uint8_t(0));
    // gpuDriverSensor is nullptr by default

    event.isRediscoveryRequired = true;

    EXPECT_CALL(*mockDevice, updateNsmDevice())
        .WillOnce([]() -> requester::Coroutine { co_return NSM_SW_SUCCESS; });
    EXPECT_CALL(*mockDevice, refreshCapabilitySensor())
        .WillOnce([]() -> requester::Coroutine { co_return NSM_SW_SUCCESS; });

    event.handleRediscovery(mockDevice);

    EXPECT_FALSE(event.isRediscoveryRequired);
}

// isRediscoveryRequired=true, gpuDriverSensor!=nullptr → if branch (line 123):
// calls gpuDriverSensor->update() which calls sensorIO (mocked to NSM_ERROR).
// Covers the if(nsmDevice->gpuDriverSensor) TRUE branch in handleRediscovery.
// Uses NsmRediscoveryEventFactoryTest fixture for access to mockSensorIO.
struct HandleRediscovery_WithGpuDriverFixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    HandleRediscovery_WithGpuDriverFixture() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(mockDevice, nullptr);
    }
    ~HandleRediscovery_WithGpuDriverFixture()
    {
        cleanupDeviceSensors(devices);
    }
};

// Emitted-payload coverage. handle() logs before it resolves the device from
// the MctpDiscovery singleton, so the payload is recorded even though the
// singleton then throws in a unit-test context.
TEST_F(HandleRediscovery_WithGpuDriverFixture,
       Handle_EmittedPayloadUsesDeviceGuid)
{
    const uuid_t deviceGuid = "6463b098-ca8d-55c1-a65a-4d03c14fb86d";

    mockDevice->deviceUuid = deviceGuid;

    auto info = makeRediscoveryInfo(true); // logging enabled
    info.uuid = gpuUuid; // static EM key - must not be emitted
    info.messageArgs = {"GPU_1", "rediscovery"};

    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info,
                              mockDevice);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_rediscovery_event(0, false, msg);
    const auto* cmsg = reinterpret_cast<const nsm_msg*>(buf.data());

    EXPECT_THROW(event.handle(5, {}, {}, cmsg, buf.size()), std::runtime_error);

    const auto& data = event.eventData;
    ASSERT_NE(data.find("REDFISH_MESSAGE_ARGS"), data.end());

    EXPECT_EQ(data.at("REDFISH_MESSAGE_ARGS").find("STATIC:"),
              std::string::npos);

#ifdef ENABLE_EVENT_GPU_UUID_LABEL
    EXPECT_EQ(data.at("REDFISH_MESSAGE_ARGS"),
              "GPU UUID " + deviceGuid + ",rediscovery");
#endif
}

TEST_F(HandleRediscovery_WithGpuDriverFixture,
       HandleRediscovery_Required_WithGpuDriver_CallsDriverUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    auto info = makeRediscoveryInfo();
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    // Attach a real GPU driver sensor → exercises the if(gpuDriverSensor) TRUE
    std::vector<utils::Association> associations;
    auto driverSensor =
        std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
            bus, "GPU_Driver_RedisTest", associations, "NSM_GPUSWInventory",
            "NVIDIA");
    driverSensor->nsmDeviceFound =
        std::static_pointer_cast<NsmDevice>(mockDevice);
    mockDevice->gpuDriverSensor = driverSensor;

    event.isRediscoveryRequired = true;

    // Mock sensorIO so the driver update coroutine exits early
    EXPECT_CALL(*mockDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    event.handleRediscovery(std::static_pointer_cast<NsmDevice>(mockDevice));

    EXPECT_FALSE(event.isRediscoveryRequired);
}

// =============================================================================
// createNsmRediscoveryEvent factory tests (via NsmObjectFactory dispatch)
// =============================================================================

struct NsmRediscoveryEventFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string interfaceName =
        "xyz.openbmc_project.Configuration.NSM_Event_Rediscovery";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/rediscovery0";
    const std::string name = "Rediscovery_Test";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmRediscoveryEventFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmRediscoveryEventFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", gpuUuid},
        {"OriginOfCondition",
         std::string("/xyz/openbmc_project/inventory/system/chassis/GPU_1")},
        {"MessageId", std::string("ResourceEvent.1.0.ResourceChanged")},
        {"LoggingNamespace", std::string("GPU_Rediscovery")},
        {"Resolution", std::string("No action required")},
        {"MessageArgs", std::vector<std::string>{"GPU_1"}},
        {"Severity", std::string("Informational")},
    };
};

// Full property map with valid UUID → event registered on device
TEST_F(NsmRediscoveryEventFactoryTest, Factory_CreateEvent_Success)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    propertyMap = basicProperties;

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               objPath);

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}

// Missing UUID → empty string → parseStaticUuid fails → exception caught
// internally
TEST_F(NsmRediscoveryEventFactoryTest, Factory_MissingUUID_NoEvent)
{
    const std::string uniquePath =
        "/xyz/openbmc_project/inventory/system/rediscovery_no_uuid";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props.erase("UUID");
    propertyMap = props;

    const size_t before = gpu->deviceEvents.size();

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               uniquePath);

    EXPECT_EQ(before, gpu->deviceEvents.size());
}

// With Severity=Warning → severityEnum has_value() → value_or uses actual value
TEST_F(NsmRediscoveryEventFactoryTest, Factory_CreateEvent_WithSeverityWarning)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/rediscovery_warning";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props["Severity"] = std::string("Warning");
    propertyMap = props;

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}

// Only UUID and Name → all other optional property checks hit FALSE branches
// Missing Severity → severityEnum empty → value_or(Level::Critical) used
TEST_F(NsmRediscoveryEventFactoryTest, Factory_CreateEvent_MinimalProperties)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/rediscovery_minimal";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    propertyMap = {
        {"Name", name},
        {"UUID", gpuUuid},
    };

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}

// With MessageArgs present → covers the MessageArgs branch
TEST_F(NsmRediscoveryEventFactoryTest, Factory_CreateEvent_WithMessageArgs)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/rediscovery_msgargs";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props["MessageArgs"] = std::vector<std::string>{"arg1", "arg2"};
    propertyMap = props;

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}

// With EventIds present → covers the new EventIds D-Bus read branch
TEST_F(NsmRediscoveryEventFactoryTest, Factory_CreateEvent_WithEventIds)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/rediscovery_eventids";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props["EventIds"] = std::vector<std::string>{"Rediscovery",
                                                 "GPU-REDISCOVERY-EVENT"};
    propertyMap = props;

    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_GE(gpu->deviceEvents.size(), 1);
}
