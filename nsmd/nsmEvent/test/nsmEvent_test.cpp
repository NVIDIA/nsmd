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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using namespace ::testing;

#define private public
#define protected public

#include "network-ports.h"

#include "nsmLongRunningEventHandler.hpp"
#include "nsmThresholdEvent.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

namespace nsm
{
requester::Coroutine createNsmThresholdEvent(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
}; // namespace nsm

using namespace nsm;

struct NsmThresholdEventTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Event_Threshold";
    const std::string name = "ThresholdEventSetting";
    const std::string objPath = chassisInventoryBasePath / name;

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmThresholdEventTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
    }

    dbus::PropertyMap error = {
        {"UUID", "992b3ec1-e468-f145-8686-badbadbadbad"},
        {"MessageArgs", std::vector<std::string>{}},
    };
    dbus::PropertyMap basic = {
        {"UUID", gpuUuid},
        {"Name", name},
        {"OriginOfCondition", "/redfish/v1/Chassis/HGX_GPU_SXM_1"},
        {"MessageId", "ResourceEvent.1.0.ResourceErrorsDetected"},
        {"LoggingNamespace", "GPU_SXM 1 Threshold"},
        {"Resolution",
         "Regarding Port Error documentation and further actions please refer to (TBD)"},
        {"MessageArgs",
         std::vector<std::string>{
             "GPU_SXM_1",
             "No Errors",
         }},
        {"Severity", "Critical"},
    };
};

TEST_F(NsmThresholdEventTest, badTestUuidNotFound)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties with INVALID UUID that doesn't match any device
    propertyMap["UUID"] = error["UUID"]; // Invalid UUID as uuid_t type

    EXPECT_THROW_COROUTINE(
        createNsmThresholdEvent(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmThresholdEventTest, badTestMessageArgsSize)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap = basic;
    propertyMap["MessageArgs"] = error["MessageArgs"];

    EXPECT_THROW_COROUTINE(
        createNsmThresholdEvent(mockManager, basicIntfName, objPath),
        std::invalid_argument);
}

TEST_F(NsmThresholdEventTest, badTestInvalidMessageId)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap = basic;
    propertyMap["MessageId"] = std::string("InvalidMessageId");

    EXPECT_THROW_COROUTINE(
        createNsmThresholdEvent(mockManager, basicIntfName, objPath),
        std::invalid_argument);
}

TEST_F(NsmThresholdEventTest, goodTestCreateEvent)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap = basic;

    createNsmThresholdEvent(mockManager, basicIntfName, objPath);

    EXPECT_EQ(2, gpu->deviceEvents.size());
    EXPECT_EQ(2, gpu->eventDispatcher.eventsMap.size());

    auto event =
        dynamic_pointer_cast<NsmThresholdEvent>(gpu->deviceEvents.back());
    EXPECT_NE(nullptr, event);
    auto eventMapEntry =
        gpu->eventDispatcher.eventsMap[NSM_TYPE_NETWORK_PORT].find(
            NSM_THRESHOLD_EVENT);
    EXPECT_EQ(event.get(), eventMapEntry->second.get());

    const nsm_health_event_payload payload{0, 0, 1, 1, 1, 1, 1, 1, 1, 0};
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                  sizeof(nsm_health_event_payload));
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_health_event(eid, true, &payload, msg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    rc = gpu->eventDispatcher.handle(eid, NSM_TYPE_NETWORK_PORT,
                                     NSM_THRESHOLD_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    rc = gpu->eventDispatcher.handle(eid, NSM_TYPE_NETWORK_PORT,
                                     NSM_THRESHOLD_EVENT, msg,
                                     eventMsg.size() - 3);
    EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST_F(NsmThresholdEventTest, goodTestCreateEventWithDeviceToPortMap)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap = basic;

    // Setup deviceToPortMap with port mapping
    uint8_t portNumber = 5;
    std::string portName = "Port_5";
    mockManager.deviceToPortMap[gpu][portNumber] = portName;

    createNsmThresholdEvent(mockManager, basicIntfName, objPath);

    EXPECT_EQ(2, gpu->deviceEvents.size());
    EXPECT_EQ(2, gpu->eventDispatcher.eventsMap.size());

    auto event =
        dynamic_pointer_cast<NsmThresholdEvent>(gpu->deviceEvents.back());
    EXPECT_NE(nullptr, event);

    // Create payload with the specific port number to trigger deviceToPortMap
    // lookup
    const nsm_health_event_payload payload{portNumber, 0, 1, 1, 1,
                                           1,          1, 1, 1, 0};
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                  sizeof(nsm_health_event_payload));
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_health_event(eid, true, &payload, msg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    // This should trigger the deviceToPortMap code path (lines 114-121)
    rc = gpu->eventDispatcher.handle(eid, NSM_TYPE_NETWORK_PORT,
                                     NSM_THRESHOLD_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

// =============================================================================
// NsmLongRunningEventHandler Tests
// =============================================================================

TEST(NsmLongRunningEventHandler, Constructor_CreatesObjectWithCorrectNameType)
{
    NsmLongRunningEventHandler handler;
    EXPECT_EQ(handler.getName(), "NsmLongRunningEventHandler");
    EXPECT_EQ(handler.getType(), "NSM_LONG_RUNNING_EVENT_HANDLER");
}

TEST(NsmLongRunningEventHandler, Handle_NoDeviceForEid_ReturnsErrorData)
{
    NsmLongRunningEventHandler handler;
    eid_t eid = 255;
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(eventData.data());
    // MctpDiscovery singleton is not initialized in unit tests, so handle()
    // throws std::runtime_error before looking up the device
    EXPECT_THROW(handler.handle(eid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                NSM_LONG_RUNNING_EVENT, msg, eventData.size()),
                 std::runtime_error);
}
