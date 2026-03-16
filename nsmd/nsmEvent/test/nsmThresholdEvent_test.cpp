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
