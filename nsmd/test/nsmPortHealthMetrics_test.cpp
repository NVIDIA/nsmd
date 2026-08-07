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

// Unit tests for NsmPortCharacteristics portHealthMetricsIntf wiring:
// constructor defaults, link_health and attention_trigger decode, first-poll
// baseline, and the error-CC path.

// The "#define private public" shim below must not rewrite libstdc++ access
// specifiers (hard error on GCC 15), so parse <sstream> and <any> first.
// Do not reorder these includes below the macro.
#include "network-ports.h"

#include <any>
#include <sstream>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmPort/nsmPort.hpp"

#undef protected
#undef private

using namespace nsm;

static auto& testBus()
{
    static auto b = sdbusplus::bus::new_default();
    return b;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a full NSM query-port-characteristics response with the given
// port_status word and zero-valued line rate, data rate, lane info.
static std::vector<uint8_t> buildResponse(uint32_t statusWord,
                                          uint32_t lineRateMbps = 100000,
                                          uint32_t dataRateKbps = 50000,
                                          uint32_t laneInfo = 4)
{
    struct nsm_port_characteristics_data portData{};
    memcpy(&portData.port_status, &statusWord, sizeof(uint32_t));
    portData.nv_port_line_rate_mbps = lineRateMbps;
    portData.nv_port_data_rate_kbps = dataRateKbps;
    portData.status_lane_info = laneInfo;

    uint16_t reasonCode = ERR_NULL;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_port_characteristics_resp(0, NSM_SUCCESS, reasonCode,
                                           &portData, msg);
    return buf;
}

// Build a properly-encoded non-success response using the real encoder
// so the NSM header is well-formed and the decode path sees a real error CC.
static std::vector<uint8_t> buildErrorResponse()
{
    struct nsm_port_characteristics_data dummy{};
    uint16_t reasonCode = ERR_NULL;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_port_characteristics_resp(0, NSM_ERROR, reasonCode, &dummy,
                                           msg);
    return buf;
}

// ---------------------------------------------------------------------------
// Fixture: one NsmPortCharacteristics per test with unique D-Bus paths
// ---------------------------------------------------------------------------

class NsmPortHealthMetricsTest : public ::testing::Test
{
  protected:
    static int& counter()
    {
        static int n = 0;
        return n;
    }

    NsmPortHealthMetricsTest()
    {
        int id = ++counter();
        objPath = "/xyz/openbmc_project/test/port_health_" + std::to_string(id);
        portName = "NVLink_" + std::to_string(id);

        iBPortIntf = std::make_shared<IBPortIntf>(testBus(), objPath.c_str());
        portMetricsOem3Intf =
            std::make_shared<PortMetricsOem3Intf>(testBus(), objPath.c_str());
        portHealthMetricsIntf =
            std::make_shared<PortHealthMetricsIntf>(testBus(), objPath.c_str());

        // NsmPortCharacteristics creates PortInfoIntf on inventoryObjPath.
        sensor = std::make_unique<NsmPortCharacteristics>(
            testBus(), portName, /*portNum=*/0, "NSM_NVLink",
            /*deviceType=*/NSM_DEV_ID_GPU, portMetricsOem3Intf, iBPortIntf,
            portHealthMetricsIntf, objPath);
    }

    std::string objPath;
    std::string portName;
    std::shared_ptr<IBPortIntf> iBPortIntf;
    std::shared_ptr<PortMetricsOem3Intf> portMetricsOem3Intf;
    std::shared_ptr<PortHealthMetricsIntf> portHealthMetricsIntf;
    std::unique_ptr<NsmPortCharacteristics> sensor;
};

// ===========================================================================
// 1. Constructor defaults
// ===========================================================================

TEST_F(NsmPortHealthMetricsTest, ConstructorSetsDefaultNA)
{
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Unknown);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::Unknown);
}

TEST_F(NsmPortHealthMetricsTest, ConstructorSetsHealthStateNotInitialized)
{
    // First poll skip flag must start as false so first handleResponseMsg does
    // not fire an EventLog entry.
    EXPECT_FALSE(sensor->healthStateInitialized);
}

// ===========================================================================
// 2. link_health → EarlyHealthIndication mapping
// ===========================================================================

TEST_F(NsmPortHealthMetricsTest, HandleResponse_LinkHealthNA)
{
    // bits 17:16 = 00b → NA (0x00000000)
    auto buf = buildResponse(0x00000000u);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Unknown);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_LinkHealthAttention)
{
    // bits 17:16 = 01b → Attention (0x00010000)
    auto buf = buildResponse(0x00010000u);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Attention);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_LinkHealthHealthy)
{
    // bits 17:16 = 10b → Healthy (0x00020000)
    auto buf = buildResponse(0x00020000u);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Healthy);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_LinkHealthReserved)
{
    // bits 17:16 = 11b (0x00030000) is reserved for a future bitmap extension
    // and is not a real state → treated as invalid and reported as Unknown
    // (throttled warning, not a per-poll error).
    auto buf = buildResponse(0x00030000u);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Unknown);
}

// ===========================================================================
// 2b. Device-scope guardrail — NVSwitch/QM exposes health counters only
// ===========================================================================

TEST_F(NsmPortHealthMetricsTest, SwitchDeviceExposesHealthOnly)
{
    // A switch exposes health counters only, so the sensor is built without a
    // PortMetricsOem3 interface. It must still publish EarlyHealthIndication
    // and never dereference the absent interface.
    std::string switchPath = objPath + "_switch";
    std::string switchName = portName + "_switch";
    auto switchHealthIntf =
        std::make_shared<PortHealthMetricsIntf>(testBus(), switchPath.c_str());
    auto switchIBPort = std::make_shared<IBPortIntf>(testBus(),
                                                     switchPath.c_str());
    std::shared_ptr<PortMetricsOem3Intf> nullOem3; // NVSwitch: no Oem3 iface
    auto switchSensor = std::make_unique<NsmPortCharacteristics>(
        testBus(), switchName, /*portNum=*/0, "NSM_NVLink",
        /*deviceType=*/NSM_DEV_ID_SWITCH, nullOem3, switchIBPort,
        switchHealthIntf, switchPath);

    // link_health = Healthy (0x00020000): health is published and the null
    // Oem3 interface is never touched (no crash).
    auto buf = buildResponse(0x00020000u);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = switchSensor->handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(switchHealthIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Healthy);
}

// ===========================================================================
// 3. attention_trigger → AttentionTriggerReason mapping
// ===========================================================================

TEST_F(NsmPortHealthMetricsTest, HandleResponse_TriggerNA)
{
    // Pre-seed a non-NA trigger to confirm it's overwritten
    auto warmup = buildResponse(0x00050000u); // link=Attention, trigger=RawBER
    auto* warmupMsg = reinterpret_cast<const nsm_msg*>(warmup.data());
    EXPECT_EQ(sensor->handleResponseMsg(warmupMsg, warmup.size()), NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::RawBER);

    // link_health=Attention + attention_trigger=0 (NotApplicable): 0x00010000
    auto buf = buildResponse(0x00010000u);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::Unknown);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_TriggerRawBER)
{
    // link_health=Attention + attention_trigger=1 (RawBER): 0x00050000
    auto buf = buildResponse(0x00050000u);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::RawBER);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_TriggerEffectiveBER)
{
    // attention_trigger=2 (EffectiveBER): 2 << 18 = 0x00080000
    auto buf = buildResponse(0x00090000u); // link_health=Attention|trigger=2
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::EffectiveBER);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_TriggerSymbolBER)
{
    // attention_trigger=3: 3 << 18 = 0x000C0000 | link_health=Attention (bit
    // 16) 0x000C0000 | 0x00010000 = 0x000D0000
    auto buf = buildResponse(0x000D0000u);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::SymbolBER);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_TriggerSymbolErrorCount)
{
    // attention_trigger=9: 9 << 18 = 0x00240000 | link_health=Attention (bit
    // 16) 0x00240000 | 0x00010000 = 0x00250000
    auto buf = buildResponse(0x00250000u);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::SymbolErrorCount);
}

TEST_F(NsmPortHealthMetricsTest,
       HandleResponse_TriggerUnknownMapsToNotApplicable)
{
    // Pre-seed a non-NA trigger to confirm it's overwritten
    auto warmup = buildResponse(0x00050000u); // link=Attention, trigger=RawBER
    auto* warmupMsg = reinterpret_cast<const nsm_msg*>(warmup.data());
    EXPECT_EQ(sensor->handleResponseMsg(warmupMsg, warmup.size()), NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::RawBER);

    // attention_trigger >= 10 must log error and map to NotApplicable
    // value 10: 10 << 18 = 0x00280000 | link_health=Attention (bit 16)
    auto buf = buildResponse(0x00290000u);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::Unknown);
}

// ===========================================================================
// 4. healthStateInitialized first-poll skip
// ===========================================================================

TEST_F(NsmPortHealthMetricsTest, FirstPollSetsInitializedFlag)
{
    EXPECT_FALSE(sensor->healthStateInitialized);

    auto buf = buildResponse(0x00020000u); // Healthy
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    sensor->handleResponseMsg(msg, buf.size());

    EXPECT_TRUE(sensor->healthStateInitialized);
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Healthy);
}

TEST_F(NsmPortHealthMetricsTest, SecondPollSameStatePreviousStateUpdated)
{
    // First poll — no event (initialization)
    auto buf1 = buildResponse(0x00020000u); // Healthy
    auto* msg1 = reinterpret_cast<const nsm_msg*>(buf1.data());
    sensor->handleResponseMsg(msg1, buf1.size());
    EXPECT_TRUE(sensor->healthStateInitialized);
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Healthy);

    // Second poll — same state (Healthy), no event logged, previous stays
    // Healthy
    auto buf2 = buildResponse(0x00020000u);
    auto* msg2 = reinterpret_cast<const nsm_msg*>(buf2.data());
    auto rc = sensor->handleResponseMsg(msg2, buf2.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Healthy);
}

TEST_F(NsmPortHealthMetricsTest,
       StateChangeUpdatesPreviousEarlyHealthIndication)
{
    // First poll: NA (initialization, no event)
    auto buf1 = buildResponse(0x00000000u);
    auto* msg1 = reinterpret_cast<const nsm_msg*>(buf1.data());
    sensor->handleResponseMsg(msg1, buf1.size());
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Unknown);

    // Second poll: Healthy (state change — event fires fire-and-forget, safe in
    // test)
    auto buf2 = buildResponse(0x00020000u);
    auto* msg2 = reinterpret_cast<const nsm_msg*>(buf2.data());
    sensor->handleResponseMsg(msg2, buf2.size());
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Healthy);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Healthy);
}

// ===========================================================================
// 5. Error CC and decode failure paths
// ===========================================================================

TEST_F(NsmPortHealthMetricsTest, HandleResponse_ErrorCC_ReturnsNonZero)
{
    auto buf = buildErrorResponse();
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_TooShort_ReturnsNonZero)
{
    // 6-byte buffer — too small for any valid response, decode returns error
    std::vector<uint8_t> buf(6, 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_ErrorCC_PropertiesUnchanged)
{
    // Properties set by constructor: NotApplicable / NotApplicable
    auto buf = buildErrorResponse();
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    sensor->handleResponseMsg(msg, buf.size());

    // Properties must remain at their constructor defaults
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Unknown);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::Unknown);
}

// ===========================================================================
// 6. Event severity mapping (Warning for Attention/Unknown, OK for Healthy)
// ===========================================================================

TEST_F(NsmPortHealthMetricsTest, SeverityIsWarningForAttention)
{
    EXPECT_TRUE(NsmPortCharacteristics::isWarningSeverity(
        EarlyHealthIndicationValues::Attention));
}

TEST_F(NsmPortHealthMetricsTest, SeverityIsWarningForUnknown)
{
    // A transition to Unknown is loss of a known health signal -> Warning.
    EXPECT_TRUE(NsmPortCharacteristics::isWarningSeverity(
        EarlyHealthIndicationValues::Unknown));
}

TEST_F(NsmPortHealthMetricsTest, SeverityIsInformationalForHealthy)
{
    EXPECT_FALSE(NsmPortCharacteristics::isWarningSeverity(
        EarlyHealthIndicationValues::Healthy));
}

// Exercise the event-firing path on a Healthy -> Unknown transition (the new
// Warning-severity case): first poll initializes, second fires the event.
TEST_F(NsmPortHealthMetricsTest, TransitionHealthyToUnknownFiresEvent)
{
    auto b1 = buildResponse(0x00020000u); // Healthy (init, no event)
    sensor->handleResponseMsg(reinterpret_cast<const nsm_msg*>(b1.data()),
                              b1.size());
    ASSERT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Healthy);

    auto b2 = buildResponse(0x00000000u); // Unknown (state change -> event)
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(b2.data()), b2.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Unknown);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Unknown);
}
