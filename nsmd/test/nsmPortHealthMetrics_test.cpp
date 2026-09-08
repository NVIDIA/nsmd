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

// port_status packs link_health at bits 17:16 and attention_trigger at 25:18.
// Build an Attention status word carrying the given trigger.
static constexpr uint32_t attentionStatus(uint32_t trigger)
{
    return (static_cast<uint32_t>(NSM_LINK_HEALTH_ATTENTION) << 16) |
           (trigger << 18);
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
    auto warmup = buildResponse(attentionStatus(NSM_ATTENTION_TRIGGER_RAW_BER));
    auto* warmupMsg = reinterpret_cast<const nsm_msg*>(warmup.data());
    EXPECT_EQ(sensor->handleResponseMsg(warmupMsg, warmup.size()), NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::RawBER);

    auto buf = buildResponse(attentionStatus(NSM_ATTENTION_TRIGGER_NA));
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::Unknown);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_TriggerRawBER)
{
    auto buf = buildResponse(attentionStatus(NSM_ATTENTION_TRIGGER_RAW_BER));
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::RawBER);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_TriggerEffectiveBER)
{
    auto buf =
        buildResponse(attentionStatus(NSM_ATTENTION_TRIGGER_EFFECTIVE_BER));
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::EffectiveBER);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_TriggerSymbolBER)
{
    auto buf = buildResponse(attentionStatus(NSM_ATTENTION_TRIGGER_SYMBOL_BER));
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::SymbolBER);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_TriggerSymbolErrorCount)
{
    auto buf = buildResponse(
        attentionStatus(NSM_ATTENTION_TRIGGER_SYMBOL_ERROR_COUNT));
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::SymbolErrorCount);
}

TEST_F(NsmPortHealthMetricsTest,
       HandleResponse_TriggerUnknownMapsToNotApplicable)
{
    // Pre-seed a non-NA trigger to confirm it's overwritten
    auto warmup = buildResponse(attentionStatus(NSM_ATTENTION_TRIGGER_RAW_BER));
    auto* warmupMsg = reinterpret_cast<const nsm_msg*>(warmup.data());
    EXPECT_EQ(sensor->handleResponseMsg(warmupMsg, warmup.size()), NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::RawBER);

    // 10 is past the last defined trigger: must warn and report Unknown
    auto buf = buildResponse(attentionStatus(10));
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::Unknown);
}

TEST_F(NsmPortHealthMetricsTest, DecodeAttentionTrigger_AllDefinedValues)
{
    const std::vector<std::pair<uint8_t, AttentionTriggerReasonValues>>
        expected{
            {NSM_ATTENTION_TRIGGER_NA, AttentionTriggerReasonValues::Unknown},
            {NSM_ATTENTION_TRIGGER_PLR_TX_BANDWIDTH_LOSS,
             AttentionTriggerReasonValues::PLRTXBandwidthLoss},
            {NSM_ATTENTION_TRIGGER_RECOVERY_BANDWIDTH_LOSS,
             AttentionTriggerReasonValues::RecoveryBandwidthLoss},
            {NSM_ATTENTION_TRIGGER_EFFECTIVE_BER,
             AttentionTriggerReasonValues::EffectiveBER},
            {NSM_ATTENTION_TRIGGER_SYMBOL_ERROR_COUNT,
             AttentionTriggerReasonValues::SymbolErrorCount},
            {NSM_ATTENTION_TRIGGER_RAW_BER,
             AttentionTriggerReasonValues::RawBER},
            {NSM_ATTENTION_TRIGGER_PLR_RX_BANDWIDTH_LOSS,
             AttentionTriggerReasonValues::PLRRXBandwidthLoss},
            {NSM_ATTENTION_TRIGGER_PORT_TOTAL_BANDWIDTH_LOSS,
             AttentionTriggerReasonValues::PortTotalBandwidthLoss},
            {NSM_ATTENTION_TRIGGER_LINK_DOWN_COUNT,
             AttentionTriggerReasonValues::LinkDownCount},
            {NSM_ATTENTION_TRIGGER_SYMBOL_BER,
             AttentionTriggerReasonValues::SymbolBER},
        };
    for (const auto& [trigger, reason] : expected)
    {
        EXPECT_EQ(sensor->decodeAttentionTrigger(trigger), reason)
            << "trigger=" << static_cast<int>(trigger);
    }
}

TEST_F(NsmPortHealthMetricsTest, DecodeReservedThenValid_RearmsThrottles)
{
    // An out-of-spec value warns once; a valid value re-arms the throttle.
    EXPECT_EQ(sensor->decodeAttentionTrigger(10),
              AttentionTriggerReasonValues::Unknown);
    EXPECT_EQ(sensor->decodeAttentionTrigger(10),
              AttentionTriggerReasonValues::Unknown);
    EXPECT_EQ(sensor->decodeAttentionTrigger(NSM_ATTENTION_TRIGGER_RAW_BER),
              AttentionTriggerReasonValues::RawBER);
    EXPECT_EQ(sensor->decodeAttentionTrigger(10),
              AttentionTriggerReasonValues::Unknown);

    EXPECT_EQ(sensor->decodeLinkHealth(3),
              EarlyHealthIndicationValues::Unknown);
    EXPECT_EQ(sensor->decodeLinkHealth(3),
              EarlyHealthIndicationValues::Unknown);
    EXPECT_EQ(sensor->decodeLinkHealth(NSM_LINK_HEALTH_HEALTHY),
              EarlyHealthIndicationValues::Healthy);
    EXPECT_EQ(sensor->decodeLinkHealth(NSM_LINK_HEALTH_NA),
              EarlyHealthIndicationValues::Unknown);
    EXPECT_EQ(sensor->decodeLinkHealth(NSM_LINK_HEALTH_ATTENTION),
              EarlyHealthIndicationValues::Attention);
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

// ===========================================================================
// 7. Unavailable bracketing of the transition event
//
// Unavailable means the Link Health record could not be read, not a health
// transition: no event into it or for merely leaving it, and a change
// bracketed by it is reported once against the last known state. The event
// itself is fire-and-forget; the state the emitter keeps is asserted.
// ===========================================================================

namespace
{
constexpr auto kNeutralReason = AttentionTriggerReasonValues::Unknown;
constexpr auto kNeutralConfig = AttentionTriggerConfigurationValues::Unknown;
} // namespace

TEST_F(NsmPortHealthMetricsTest, Event_NoEventIntoOrOutOfUnavailable)
{
    sensor->updateHealth(EarlyHealthIndicationValues::Healthy, kNeutralReason,
                         0, kNeutralConfig);
    ASSERT_TRUE(sensor->healthStateInitialized);
    ASSERT_TRUE(sensor->lastKnownHealthStateValid);
    ASSERT_EQ(sensor->lastKnownHealthState,
              EarlyHealthIndicationValues::Healthy);

    // Into Unavailable: published, but the last known state is kept.
    sensor->updateHealth(EarlyHealthIndicationValues::Unavailable,
                         kNeutralReason, 0, kNeutralConfig);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Unavailable);
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Unavailable);
    EXPECT_EQ(sensor->lastKnownHealthState,
              EarlyHealthIndicationValues::Healthy);

    // Back to the same readable state: nothing changed since the last known
    // state, so no transition is recorded.
    sensor->updateHealth(EarlyHealthIndicationValues::Healthy, kNeutralReason,
                         0, kNeutralConfig);
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Healthy);
    EXPECT_EQ(sensor->lastKnownHealthState,
              EarlyHealthIndicationValues::Healthy);
}

TEST_F(NsmPortHealthMetricsTest, Event_BracketedChangeReportedOnce)
{
    sensor->updateHealth(EarlyHealthIndicationValues::Healthy, kNeutralReason,
                         0, kNeutralConfig);
    sensor->updateHealth(EarlyHealthIndicationValues::Unavailable,
                         kNeutralReason, 0, kNeutralConfig);
    ASSERT_EQ(sensor->lastKnownHealthState,
              EarlyHealthIndicationValues::Healthy);

    // Healthy -> Unavailable -> Attention: reported once the record is
    // readable again, against the last known state.
    sensor->updateHealth(EarlyHealthIndicationValues::Attention,
                         AttentionTriggerReasonValues::RawBER, 5,
                         AttentionTriggerConfigurationValues::Current);
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Attention);
    EXPECT_EQ(sensor->lastKnownHealthState,
              EarlyHealthIndicationValues::Attention);

    // Repeating the state reports nothing further.
    sensor->updateHealth(EarlyHealthIndicationValues::Attention,
                         AttentionTriggerReasonValues::RawBER, 5,
                         AttentionTriggerConfigurationValues::Current);
    EXPECT_EQ(sensor->lastKnownHealthState,
              EarlyHealthIndicationValues::Attention);
}

TEST_F(NsmPortHealthMetricsTest, Event_StartInUnavailableBaselinesFirstReadable)
{
    sensor->updateHealth(EarlyHealthIndicationValues::Unavailable,
                         kNeutralReason, 0, kNeutralConfig);
    EXPECT_TRUE(sensor->healthStateInitialized);
    EXPECT_FALSE(sensor->lastKnownHealthStateValid);
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Unavailable);

    // The first readable state is the baseline, not a transition.
    sensor->updateHealth(EarlyHealthIndicationValues::Attention,
                         AttentionTriggerReasonValues::EffectiveBER, 3,
                         AttentionTriggerConfigurationValues::Current);
    EXPECT_TRUE(sensor->lastKnownHealthStateValid);
    EXPECT_EQ(sensor->lastKnownHealthState,
              EarlyHealthIndicationValues::Attention);
    EXPECT_EQ(sensor->previousEarlyHealthIndication,
              EarlyHealthIndicationValues::Attention);
}

// ===========================================================================
// 8. updateHealth publisher and the extension properties
// ===========================================================================

TEST_F(NsmPortHealthMetricsTest, ConstructorSetsExtensionDefaults)
{
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerMetricId(), 0);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerConfiguration(),
              AttentionTriggerConfigurationValues::Unknown);
    EXPECT_FALSE(sensor->lastKnownHealthStateValid);
}

TEST_F(NsmPortHealthMetricsTest, UpdateHealth_PublishesAllFourProperties)
{
    sensor->updateHealth(EarlyHealthIndicationValues::Attention,
                         AttentionTriggerReasonValues::RawBER, 5,
                         AttentionTriggerConfigurationValues::Modified);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Attention);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::RawBER);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerMetricId(), 5);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerConfiguration(),
              AttentionTriggerConfigurationValues::Modified);
}

TEST_F(NsmPortHealthMetricsTest, UpdateHealth_OperatorClearedTransition)
{
    // Attention baseline, then the publish that follows an operator clear:
    // the transition to Healthy is recorded (logged Informational) with the
    // trigger fields neutral.
    sensor->updateHealth(EarlyHealthIndicationValues::Attention,
                         AttentionTriggerReasonValues::EffectiveBER, 3,
                         AttentionTriggerConfigurationValues::Current);
    sensor->updateHealth(EarlyHealthIndicationValues::Healthy, kNeutralReason,
                         0, kNeutralConfig, /*operatorCleared=*/true);
    EXPECT_EQ(sensor->lastKnownHealthState,
              EarlyHealthIndicationValues::Healthy);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Healthy);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::Unknown);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerMetricId(), 0);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerConfiguration(),
              AttentionTriggerConfigurationValues::Unknown);
}

TEST_F(NsmPortHealthMetricsTest, HandleResponse_KeepsExtensionFieldsNeutral)
{
    // The 0x42 status word carries no slot or configuration field: a Phase 1
    // poll publishes them neutral even after they were set.
    sensor->updateHealth(EarlyHealthIndicationValues::Attention,
                         AttentionTriggerReasonValues::RawBER, 9,
                         AttentionTriggerConfigurationValues::Current);
    ASSERT_EQ(portHealthMetricsIntf->attentionTriggerMetricId(), 9);

    auto buf = buildResponse(attentionStatus(NSM_ATTENTION_TRIGGER_RAW_BER));
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    EXPECT_EQ(sensor->handleResponseMsg(msg, buf.size()), NSM_SUCCESS);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Attention);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(),
              AttentionTriggerReasonValues::RawBER);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerMetricId(), 0);
    EXPECT_EQ(portHealthMetricsIntf->attentionTriggerConfiguration(),
              AttentionTriggerConfigurationValues::Unknown);
}

// ===========================================================================
// 9. NsmPortCharacteristicsV2 range-checked decoders of the Tag 0x04 fields
// ===========================================================================

class NsmPortHealthMetricsV2DecoderTest : public NsmPortHealthMetricsTest
{
  protected:
    NsmPortHealthMetricsV2DecoderTest()
    {
        v2Path = objPath + "_v2";
        v2Name = portName + "_v2";
        v2IBPortIntf = std::make_shared<IBPortIntf>(testBus(), v2Path.c_str());
        v2Oem3Intf = std::make_shared<PortMetricsOem3Intf>(testBus(),
                                                           v2Path.c_str());
        v2HealthIntf = std::make_shared<PortHealthMetricsIntf>(testBus(),
                                                               v2Path.c_str());
        // The decoders do not touch the device.
        v2 = std::make_unique<NsmPortCharacteristicsV2>(
            testBus(), v2Name, /*portNum=*/1, "NSM_NVLink", NSM_DEV_ID_GPU,
            v2Oem3Intf, v2IBPortIntf, v2HealthIntf, v2Path,
            /*device=*/nullptr);
    }

    std::string v2Path;
    std::string v2Name;
    std::shared_ptr<IBPortIntf> v2IBPortIntf;
    std::shared_ptr<PortMetricsOem3Intf> v2Oem3Intf;
    std::shared_ptr<PortHealthMetricsIntf> v2HealthIntf;
    std::unique_ptr<NsmPortCharacteristicsV2> v2;
};

TEST_F(NsmPortHealthMetricsV2DecoderTest, DecodeMetricSlot_Ranges)
{
    EXPECT_EQ(v2->decodeMetricSlot(0), 0);
    EXPECT_EQ(v2->decodeMetricSlot(1), 1);
    EXPECT_EQ(v2->decodeMetricSlot(NSM_LINK_HEALTH_METRIC_SLOT_MAX),
              NSM_LINK_HEALTH_METRIC_SLOT_MAX);
    // 16..127 are reserved and published as 0 (one throttled warning per
    // reserved episode, re-armed by a valid value).
    EXPECT_EQ(v2->decodeMetricSlot(NSM_LINK_HEALTH_METRIC_SLOT_MAX + 1), 0);
    EXPECT_EQ(v2->decodeMetricSlot(127), 0);
    EXPECT_EQ(v2->decodeMetricSlot(2), 2);
    EXPECT_EQ(v2->decodeMetricSlot(64), 0);
}

TEST_F(NsmPortHealthMetricsV2DecoderTest, DecodeConfigChanged_Ranges)
{
    EXPECT_EQ(v2->decodeConfigChanged(NSM_LINK_HEALTH_CONFIG_NA),
              AttentionTriggerConfigurationValues::Unknown);
    EXPECT_EQ(v2->decodeConfigChanged(NSM_LINK_HEALTH_CONFIG_CURRENT),
              AttentionTriggerConfigurationValues::Current);
    EXPECT_EQ(v2->decodeConfigChanged(NSM_LINK_HEALTH_CONFIG_PREVIOUS),
              AttentionTriggerConfigurationValues::Modified);
    // 3 is reserved; anything wider than the 2-bit field is invalid too.
    EXPECT_EQ(v2->decodeConfigChanged(3),
              AttentionTriggerConfigurationValues::Unknown);
    EXPECT_EQ(v2->decodeConfigChanged(0xFF),
              AttentionTriggerConfigurationValues::Unknown);
    EXPECT_EQ(v2->decodeConfigChanged(NSM_LINK_HEALTH_CONFIG_CURRENT),
              AttentionTriggerConfigurationValues::Current);
}

// ===========================================================================
// 10. ClearEarlyHealthIndication on a Phase 1-only port
// ===========================================================================

TEST_F(NsmPortHealthMetricsTest, Phase1OnlyPort_ClearReportsUnsupportedRequest)
{
    // No NsmPortCharacteristicsV2 attached: the method answers through a
    // fresh async result object without contacting any device.
    ASSERT_FALSE(static_cast<bool>(portHealthMetricsIntf->clearHandler));

    auto path = portHealthMetricsIntf->clearEarlyHealthIndication();
    ASSERT_FALSE(path.str.empty());
    EXPECT_TRUE(path.str.starts_with(AsyncOperationResultObjPath));

    std::shared_ptr<AsyncStatusIntf> status;
    for (const auto& [index, statusInterface] :
         AsyncOperationManager::getInstance()->statusInterfaces)
    {
        if (std::string{AsyncOperationResultObjPath} + "/" +
                std::to_string(index) ==
            path.str)
        {
            status = statusInterface;
        }
    }
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->status(), AsyncOperationStatusType::UnsupportedRequest);
}

TEST_F(NsmPortHealthMetricsTest, AllocateClearResult_ReturnsPendingObject)
{
    auto [path, status] = PortHealthMetricsIntf::allocateClearResult(objPath);
    EXPECT_TRUE(path.starts_with(AsyncOperationResultObjPath));
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->status(), AsyncOperationStatusType::InProgress);
    // TODO(link-health): the Common.Error.Unavailable branch (no free result
    // object) is unreachable through AsyncOperationManager today:
    // getCurrentObjectCount() never reports exhaustion, so it cannot be
    // exercised without a test hook in the manager.
}
