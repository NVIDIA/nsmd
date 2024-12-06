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

#include "nsmThresholdEvent.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

namespace nsm
{
requester::Coroutine createNsmThresholdEvent(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
};

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

    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";

    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>((uint8_t)NSM_DEV_ID_GPU, instanceId)},
    };
    NsmDevice& gpu = *devices[0];

    NsmThresholdEventTest() : SensorManagerTest(devices)
    {
        gpu.uuid = gpuUuid;
    }

    const PropertyValuesCollection error = {
        {"UUID", "992b3ec1-e468-f145-8686-badbadbadbad"},
        {"MessageArgs", std::vector<std::string>{}},
    };
    const PropertyValuesCollection basic = {
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
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(error, "UUID"));

    createNsmThresholdEvent(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, gpu.deviceEvents.size());
}

TEST_F(NsmThresholdEventTest, badTestMessageArgsSize)
{
    auto& values = utils::MockDbusAsync::getValues();
    for (auto& pair : basic)
    {
        if (pair.first == "MessageArgs")
        {
            values.push(objPath, get(error, pair.first));
        }
        else
        {
            values.push(objPath, pair);
        }
    }

    createNsmThresholdEvent(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, gpu.deviceEvents.size());
}

TEST_F(NsmThresholdEventTest, goodTestCreateEvent)
{
    auto& values = utils::MockDbusAsync::getValues();
    for (auto& it : basic)
    {
        values.push(objPath, it);
    }

    createNsmThresholdEvent(mockManager, basicIntfName, objPath);

    EXPECT_EQ(1, gpu.deviceEvents.size());
    EXPECT_EQ(1, gpu.eventDispatcher.eventsMap.size());

    auto event =
        dynamic_pointer_cast<NsmThresholdEvent>(gpu.deviceEvents.front());
    EXPECT_NE(nullptr, event);
    EXPECT_EQ(event.get(),
              gpu.eventDispatcher
                  .eventsMap[NSM_TYPE_NETWORK_PORT][NSM_THRESHOLD_EVENT]
                  .get());

    const nsm_health_event_payload payload{1, 1, 1, 1, 1, 1, 1, 0, 0};
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                  sizeof(nsm_health_event_payload));
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_health_event(eid, true, &payload, msg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    rc = gpu.eventDispatcher.handle(eid, NSM_TYPE_NETWORK_PORT,
                                    NSM_THRESHOLD_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    rc = gpu.eventDispatcher.handle(eid, NSM_TYPE_NETWORK_PORT,
                                    NSM_THRESHOLD_EVENT, msg,
                                    eventMsg.size() - 3);
    EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}
