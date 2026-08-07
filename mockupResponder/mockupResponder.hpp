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

#pragma once

#include "base.h"
#include "device-capability-discovery.h"
#include "device-configuration.h"
#include "diagnostics.h"
#include "network-ports.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"
#include "requester/mctp.h"

#include "types.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/io.hpp>

#include <array>
#include <map>
#include <optional>
#include <queue>
#include <string_view>

namespace MockupResponder
{
const std::array<uint64_t, 4> bootReasonMockValue = {100, 548682072066, 0, 0};

const std::unordered_map<uint8_t, uint64_t> resetMetricsMockTable = {
    {0, 100}, // PF_FLR_ResetEntryCount
    {1, 95},  // PF_FLR_ResetExitCount
    {2, 50},  // ConventionalResetEntryCount
    {3, 45},  // ConventionalResetExitCount
    {4, 30},  // FundamentalResetEntryCount
    {5, 25},  // FundamentalResetExitCount
    {6, 10},  // IRoTResetExitCount
    {7, 2},   // LastResetType (enum8: e.g., 2 = Fundamental)
    {8, 3}    // BootReasonType (enum8: e.g., bit 0 = wake up, bit 1 = PowerOn)
};

const std::unordered_map<uint8_t, uint64_t> powerSmoothingFeatureInfoMockTable =
    {
        {0, 7},      // Feature Flag
        {1, 100000}, // Current TMP Setting
        {2, 200000}, // Current TMP Floor Setting
        {3, 95},     // Max TMP Floor Setting in Percent
        {4, 2},      // Min TMP Floor Setting in Percent
        {5, 100000}, // Current Primary Floor window multiplier
        {6, 100000}, // Min Primary Floor activation offset
        {7, 200000}  // Min Primary Floor target window multiplier
};

const std::unordered_map<uint8_t, uint64_t> currentProfileInfoMockTable = {
    {0, 1},      // Active preset profile
    {1, 100000}, // Admin override mask
    {2, 100},    // Current Percent TMP Floor
    {3, 100000}, // Current RampUp Rate in Milliwatts per Second
    {4, 100000}, // Current Rampdown Rate in Milliwatts per Second
    {5, 100000}, // Current Hysteresis Value in Milliseconds
    {6, 100000}, // Current Secondary Floor Setting
    {7, 20},     // Current Primary Floor Activation window multiplier
    {8, 20},     // Current Primary Floor target Window Multiplier
    {9, 100000}  // Current Primary Floor activation offset
};

const std::unordered_map<uint8_t, uint64_t> presetProfileInfoMockTable = {
    // Profile 0
    {0, 100},     // TMP Floor Setting in Percent
    {8, 100000},  // Ramp-up rate in Milliwatts per Second
    {16, 100000}, // Ramp-down rate in Milliwatts per Third
    {24, 100000}, // Hysteresis for ramp down in Milliseconds
    {32, 100000}, // Current secondary floor setting
    {40, 20},     // Current primary floor activation window multiplier
    {48, 20},     // Current primary floor target window multiplier
    {56, 100000}, // Current primary floor activation offset
    // Profile 1
    {1, 110},     // TMP Floor Setting in Percent
    {9, 110000},  // Ramp-up rate in Milliwatts per Second
    {17, 110000}, // Ramp-down rate in Milliwatts per Third
    {25, 110000}, // Hysteresis for ramp down in Milliseconds
    {33, 110000}, // Current secondary floor setting
    {41, 21},     // Current primary floor activation window multiplier
    {49, 21},     // Current primary floor target window multiplier
    {57, 110000}, // Current primary floor activation offset
    // Profile 2
    {2, 120},     // TMP Floor Setting in Percent
    {10, 120000}, // Ramp-up rate in Milliwatts per Second
    {18, 120000}, // Ramp-down rate in Milliwatts per Third
    {26, 120000}, // Hysteresis for ramp down in Milliseconds
    {34, 120000}, // Current secondary floor setting
    {42, 22},     // Current primary floor activation window multiplier
    {50, 22},     // Current primary floor target window multiplier
    {58, 120000}, // Current primary floor activation offset
    // Profile 3
    {3, 130},     // TMP Floor Setting in Percent
    {11, 130000}, // Ramp-up rate in Milliwatts per Second
    {19, 130000}, // Ramp-down rate in Milliwatts per Third
    {27, 130000}, // Hysteresis for ramp down in Milliseconds
    {35, 130000}, // Current secondary floor setting
    {43, 23},     // Current primary floor activation window multiplier
    {51, 23},     // Current primary floor target window multiplier
    {59, 130000}  // Current primary floor activation offset
};

const std::unordered_map<uint8_t, uint64_t> adminOverrideMockTable = {
    {0, 100},    // TMP Floor Setting in Percent
    {1, 100000}, // Ramp-up rate in Milliwatts per Second
    {2, 100000}, // Ramp-down rate in Milliwatts per Third
    {3, 100000}, // Hysteresis for ramp down in Milliseconds
    {4, 100000}, // Current secondary floor setting
    {5, 20},     // Current primary floor activation window multiplier
    {6, 20},     // Current primary floor target window multiplier
    {7, 100000}  // Current primary floor activation offset
};

// =====================================================================
// Dump-command failure cycle (Type 4 cmds 0x40/0x50/0x51/0x52/0x59).
//
// When the mockup is started with --failure_cycle, the five dump
// handlers stop returning the happy-path response and instead replay a
// fixed, ordered list of device responses — one list entry per dump —
// covering every NSM error -> AsyncOperationStatus mapping. A single
// global counter is shared across all five commands, so issuing the
// same NSM raw dump command repeatedly walks the whole list and
// exercises every failure -> AsyncOperationStatus mapping in turn. When
// the flag is off (the default) the handlers behave exactly as before.
//
// Each list entry (DumpCycleCase) is one complete dump. Most are a
// single page (pageCount == 1): the device answers the fresh request
// and the dump terminates. A firmware-protocol violation that needs the
// BMC to iterate carries up to 3 pages. The bytes on the wire are built
// with the same libnsm encode_*_resp helpers a real device uses, so the
// responses are bit-accurate.
// =====================================================================

// How an iterative dump handler (0x40/0x50/0x52) sets the next handle.
enum class DumpHandleMode
{
    EndSentinel, // next handle = END (0 for 0x50/0x52, 0xFF for 0x40)
    Advance,     // next handle = requestHandle + 1 (BMC will recurse)
    Stuck,       // next handle = requestHandle (BMC stuck-loop guard trips)
};

// One device response within a dump cycle case.
struct DumpCyclePage
{
    bool silent = false;            // true: send no MCTP reply (BMC times out)
    uint8_t cc = NSM_SUCCESS;       // completion code byte in the response
    uint16_t reasonCode = ERR_NULL; // reason code bytes in the response
    DumpHandleMode handleMode = DumpHandleMode::EndSentinel;
};

// One entry in the failure cycle: a complete dump (1..3 device responses).
struct DumpCycleCase
{
    std::string_view caseId;         // stable id, also referenced by unit tests
    std::string_view expectedStatus; // expected status (logged for triage)
    uint32_t pageCount = 1;          // device responses this dump produces
    std::array<DumpCyclePage, 3> pages{};
};

// Single-page completion-code case (reason = ERR_NULL): the device
// answers the fresh request with a non-success cc and terminates.
constexpr DumpCycleCase ccCase(std::string_view id, std::string_view expected,
                               uint8_t cc)
{
    return DumpCycleCase{
        id,
        expected,
        1,
        {{DumpCyclePage{false, cc, ERR_NULL, DumpHandleMode::EndSentinel}}}};
}

// Single-page reason-code case: cc is a non-success carrier (NSM_ERR_NOT_READY)
// while the reason code drives the mapping. nsmd's mapper gives a recognized
// reason code precedence over cc (see nsmDumpUtils.cpp), so the carrier cc
// never masks the reason under test.
constexpr DumpCycleCase reasonCase(std::string_view id,
                                   std::string_view expected, uint16_t reason)
{
    return DumpCycleCase{id,
                         expected,
                         1,
                         {{DumpCyclePage{false, NSM_ERR_NOT_READY, reason,
                                         DumpHandleMode::EndSentinel}}}};
}

// The failure cycle, in walk order. Index 0 is the success baseline.
inline constexpr std::array<DumpCycleCase, 25> kDumpFailureCycle = {{
    // Baseline: a clean single-page dump that terminates normally.
    DumpCycleCase{"SUCCESS",
                  "Success",
                  1,
                  {{DumpCyclePage{false, NSM_SUCCESS, ERR_NULL,
                                  DumpHandleMode::EndSentinel}}}},

    // Completion-code triggers (cc branch).
    ccCase("CC_UNSUPPORTED_COMMAND_CODE", "UnsupportedRequest",
           NSM_ERR_UNSUPPORTED_COMMAND_CODE),
    ccCase("CC_UNSUPPORTED_MSG_TYPE", "UnsupportedRequest",
           NSM_ERR_UNSUPPORTED_MSG_TYPE),
    ccCase("CC_NSM_BUSY", "Unavailable", NSM_BUSY),
    ccCase("CC_NSM_ERR_NOT_READY", "Unavailable", NSM_ERR_NOT_READY),
    ccCase("CC_NSM_ERR_BUS_ACCESS", "Unavailable", NSM_ERR_BUS_ACCESS),
    ccCase("CC_INVALID_STATE_FOR_COMMAND", "Unavailable",
           NSM_ERR_INVALID_STATE_FOR_COMMAND),
    ccCase("CC_INVALID_DATA", "InvalidArgument", NSM_ERR_INVALID_DATA),
    ccCase("CC_INVALID_DATA_LENGTH", "InvalidArgument",
           NSM_ERR_INVALID_DATA_LENGTH),
    ccCase("CC_INVALID_REQUEST_TYPE", "InvalidArgument",
           NSM_ERR_INVALID_REQUEST_TYPE),
    ccCase("CC_NSM_ACCEPTED", "InProgress", NSM_ACCEPTED),

    // Reason-code triggers (reason branch — overrides cc).
    reasonCase("REASON_ERR_TIMEOUT", "Timeout", ERR_TIMEOUT),
    reasonCase("REASON_ERR_DOWNSTREAM_TIMEOUT", "Timeout",
               ERR_DOWNSTREAM_TIMEOUT),
    reasonCase("REASON_ERR_NOT_SUPPORTED", "UnsupportedRequest",
               ERR_NOT_SUPPORTED),
    reasonCase("REASON_ERR_NO_BOOT_COMPLETE", "Unavailable",
               ERR_NO_BOOT_COMPLETE),
    reasonCase("REASON_ERR_UPDATE_IN_PROGRESS", "Unavailable",
               ERR_UPDATE_IN_PROGRESS),
    reasonCase("REASON_ERR_IMAGE_COPY_IN_PROGRESS", "Unavailable",
               ERR_IMAGE_COPY_IN_PROGRESS),
    reasonCase("REASON_ERR_FLASH_WEAR_MITIGATION", "Unavailable",
               ERR_FLASH_WEAR_MITIGATION),
    reasonCase("REASON_ERR_INVALID_PCI", "InvalidArgument", ERR_INVALID_PCI),
    reasonCase("REASON_ERR_INVALID_RQD", "InvalidArgument", ERR_INVALID_RQD),
    reasonCase("REASON_ERR_INCOMPLETE_COMPONENT_SET", "InvalidArgument",
               ERR_INCOMPLETE_COMPONENT_SET),
    reasonCase("REASON_ERR_I2C_NACK_FROM_DEV_ADDR", "Unavailable",
               ERR_I2C_NACK_FROM_DEV_ADDR),

    // Transport silence: device sends no reply, BMC sees NSM_SW_ERROR_TIMEOUT.
    DumpCycleCase{"NO_RESPONSE_TIMEOUT",
                  "Timeout",
                  1,
                  {{DumpCyclePage{true, NSM_SUCCESS, ERR_NULL,
                                  DumpHandleMode::EndSentinel}}}},

    // Unmapped (cc, reason) pair: exercises the mapper fall-through.
    DumpCycleCase{"CATCHALL_UNMAPPED",
                  "InternalFailure",
                  1,
                  {{DumpCyclePage{false, NSM_ERROR, ERR_NULL,
                                  DumpHandleMode::EndSentinel}}}},

    // Firmware-protocol violation page 0 advances the handle so the BMC
    // recurses,
    // page 1 repeats the request handle so nsmd's stuck-loop guard trips.
    DumpCycleCase{
        "PROTO_STUCK_HANDLE",
        "InternalFailure",
        2,
        {{DumpCyclePage{false, NSM_SUCCESS, ERR_NULL, DumpHandleMode::Advance},
          DumpCyclePage{false, NSM_SUCCESS, ERR_NULL, DumpHandleMode::Stuck}}}},
}};

constexpr uint8_t MCTP_MSG_TYPE_VDM = 0x7e;
constexpr uint8_t MCTP_MSG_EMU_PREFIX = 0xFF;
// these are for use with the mctp-demux-daemon
constexpr size_t mctpMaxMessageSize = 4096;

constexpr char MCTP_SOCKET_PATH[] = "\0mctp-pcie-mux";

struct HeaderType
{
    uint8_t eid;
    uint8_t type;
};

struct EventSource
{
    std::array<bitfield8_t, EVENT_SOURCES_LENGTH> events;
    EventSource() = default;
    EventSource(const std::vector<uint64_t>& events);
};
class MockupResponder
{
  public:
    MockupResponder(bool verbose, sdeventplus::Event& event,
                    sdbusplus::asio::object_server& server, eid_t eid,
                    uint8_t deviceType, uint8_t instanceId,
                    bool failureCycle = false);
    ~MockupResponder();

    int initSocket();

    std::optional<Response>
        processRxMsg(const Request& rxMsg,
                     std::optional<Request>& longRunningEvent);

    // type0 handlers
    std::optional<std::vector<uint8_t>>
        unsupportedCommandHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>> pingHandler(const nsm_msg* requestMsg,
                                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getSupportNvidiaMessageTypesHandler(const nsm_msg* requestMsg,
                                            size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getSupportCommandCodeHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryDeviceIdentificationHandler(const nsm_msg* requestMsg,
                                         size_t requestLen);
    void generateDummyGUID(const uint8_t eid, uint8_t* data);

    // type1 handlers
    std::optional<std::vector<uint8_t>>
        getPortTelemetryCounterHandler(const nsm_msg* requestMsg,
                                       size_t requestLen);
    /* OOB Miswiring Detection: mock-up
     * responder for Type 1 cmd 0x16 GetLLDPPacket. Returns a synthetic
     * IEEE 802.1AB frame for direction RX (port 0), or an empty buffer
     * (CC=0x00, data_size=0) otherwise — exercises the OMD-REQ-05
     * working-assumption path through nsmLldpPacket.cpp. */
    std::optional<std::vector<uint8_t>>
        getLldpPacketHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryPortCharacteristicsHandler(const nsm_msg* requestMsg,
                                        size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryPortStatusHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getFabricManagerStateHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryPortsAvailableHandler(const nsm_msg* requestMsg,
                                   size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setPortDisableFutureHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getPortDisableFutureHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getPowerModeHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setPowerModeHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getSwitchIsolationMode(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setSwitchIsolationMode(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getEventSubscription(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setEventSubscription(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getSupportedEventSources(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getCurrentEventSources(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setCurrentEventSources(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        configureEventAcknowledgement(const nsm_msg* requestMsg,
                                      size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getHistogramFormatHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getHistogramDataHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getDeviceCapabilitiesV2Handler(const nsm_msg* requestMsg,
                                       size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getGpioStateHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getEventLogRecordV2Handler(const nsm_msg* requestMsg,
                                   size_t requestLen);
    std::optional<std::vector<uint8_t>> encodeEventLogRecordV2Resp(
        uint8_t instanceId, uint16_t eventHandle, uint16_t transferHandle,
        uint16_t nextTransferHandle, const std::vector<uint8_t>& eventData);

    // type3 handlers
    std::optional<std::vector<uint8_t>>
        getInventoryInformationHandler(const nsm_msg* requestMsg,
                                       size_t requestLen);
    std::vector<uint8_t> getProperty(uint8_t propertyIdentifier);
    std::optional<std::vector<uint8_t>>
        getTemperatureReadingHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getCurrentPowerDrawHandler(const nsm_msg* requestMsg,
                                   size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getMaxObservedPowerHandler(const nsm_msg* requestMsg,
                                   size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getDriverInfoHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<Response>
        getMigModeHandler(const nsm_msg* requestMsg, size_t requestLen,
                          bool isLongRunning,
                          std::optional<Request>& longRunningEvent);

    std::optional<Response>
        setMigModeHandler(const nsm_msg* requestMsg, size_t requestLen,
                          bool isLongRunning,
                          std::optional<Request>& longRunningEvent);

    std::optional<Response>
        getEccModeHandler(const nsm_msg* requestMsg, size_t requestLen,
                          bool isLongRunning,
                          std::optional<Request>& longRunningEvent);

    std::optional<Response>
        setEccModeHandler(const nsm_msg* requestMsg, size_t requestLen,
                          bool isLongRunning,
                          std::optional<Request>& longRunningEvent);

    std::optional<std::vector<uint8_t>>
        getEccErrorCountsHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getClockLimitHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setClockLimitHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getCurrClockFreqHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<Response>
        getMemoryCapacityUtilHandler(const nsm_msg* requestMsg,
                                     size_t requestLen, bool isLongRunning,
                                     std::optional<Request>& longRunningEvent);

    std::optional<std::vector<uint8_t>>
        getProcessorThrottleReasonHandler(const nsm_msg* requestMsg,
                                          size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getAccumCpuUtilTimeHandler(const nsm_msg* requestMsg,
                                   size_t requestLen);

    std::optional<Response>
        getCurrentUtilizationHandler(const nsm_msg* requestMsg,
                                     size_t requestLen, bool isLongRunning,
                                     std::optional<Request>& longRunningEvent);

    std::optional<std::vector<uint8_t>>
        getClockOutputEnableStateHandler(const nsm_msg* requestMsg,
                                         size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getPowerLimitHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        setPowerLimitHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getPciePortConfigHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setPciePortConfigHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<Response>
        getViolationDurationHandler(const nsm_msg* requestMsg,
                                    size_t requestLen, bool isLongRunning,
                                    std::optional<Request>& longRunningEvent);

    // send rediscovery event
    void sendRediscoveryEvent(uint8_t eid, bool ackr);

    // send event
    void sendNsmEvent(uint8_t dest, uint8_t nsmType, bool ackr, uint8_t ver,
                      uint8_t eventId, uint8_t eventClass, uint16_t eventState,
                      uint8_t dataSize, uint8_t* data);

    /**
     * @brief Sends a threshold event to a specified destination.
     *
     * This function sends a threshold event to the given destination with
     * various threshold parameters.
     *
     * @param dest The destination EID.
     * @param ackr Acknowledgment request flag.
     * @param port_rcv_errors_threshold Port receive errors threshold flag.
     * @param port_xmit_discard_threshold Port transmit discard threshold flag.
     * @param symbol_ber_threshold Symbol bit error rate threshold flag.
     * @param port_rcv_remote_physical_errors_threshold Port receive remote
     * physical errors threshold flag.
     * @param port_rcv_switch_relay_errors_threshold Port receive switch relay
     * errors threshold flag.
     * @param effective_ber_threshold Effective bit error rate threshold flag.
     * @param estimated_effective_ber_threshold Estimated effective bit error
     * rate threshold flag.
     * @param portNumber The port number associated with the event.
     */
    void sendThreasholdEvent(uint8_t dest, bool ackr,
                             bool port_rcv_errors_threshold,
                             bool port_xmit_discard_threshold,
                             bool symbol_ber_threshold,
                             bool port_rcv_remote_physical_errors_threshold,
                             bool port_rcv_switch_relay_errors_threshold,
                             bool effective_ber_threshold,
                             bool estimated_effective_ber_threshold,
                             uint8_t portNumber);

    void sendGpioStateChangeEvent(
        uint8_t dest, bool ackr, uint64_t timestamp,
        const std::vector<std::pair<uint16_t, bool>>& gpioEvents);

    void sendXIDEvent(uint8_t dest, bool ackr, uint8_t flag, uint32_t reason,
                      uint32_t sequence_number, uint64_t timestamp,
                      std::string message_text);

    void sendResetRequiredEvent(uint8_t eid, bool ackr);

    /** Type 4, event ID 0x01 -- Runtime IST Complete v1 (Diagnostics).
     *  Mockup hardcodes the spec-mandated event_state = 0; libnsm encoder
     *  itself takes the value as a parameter so it stays generic.
     */
    void sendRuntimeISTCompleteEvent(
        uint8_t dest, bool ackr, std::string gpu_identifier, uint64_t timestamp,
        std::string app_version, uint8_t result, uint64_t status_code,
        int32_t max_temperature, int32_t avg_temperature);

    /** Type 5, event ID 0x01 — device ready for volatile config (Fractal Boot).
     */
    void sendDeviceConfigurationRequestEventV1(uint8_t dest, bool ackr);

    void sendFabricManagerStateEvent(uint8_t dest, bool ackr, uint8_t state,
                                     uint8_t status, uint64_t last_restart_time,
                                     uint64_t last_restart_duration);

    // Vera CPU Pre-Boot Diagnostics event senders
    void sendDiagGetSystemConfigEvent(uint8_t dest, bool ackr,
                                      uint8_t configType);
    void sendDiagGetTidConfigEvent(uint8_t dest, bool ackr, uint8_t tid);
    void sendDiagSetTestResultEvent(uint8_t dest, bool ackr, uint8_t tid,
                                    uint16_t testErrorCode,
                                    const std::vector<uint8_t>& dynamicData);
    void sendDiagSetFlowControlEvent(uint8_t dest, bool ackr,
                                     uint8_t flowCtrlStatus);

    // Vera CPU Pre-Boot Diagnostics command handlers (CPU responder)
    std::optional<std::vector<uint8_t>>
        setDiagSystemConfigHandler(const nsm_msg* requestMsg,
                                   size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setDiagTidConfigHandler(const nsm_msg* requestMsg, size_t requestLen);

    // Vera CPU Pre-Boot Diagnostics - full session simulation
    void runDiagSession();
    void advanceDiagSession();
    std::pair<uint16_t, std::vector<uint8_t>> generateResultForTid(uint8_t tid);

    std::optional<Response>
        queryScalarGroupTelemetryHandler(const nsm_msg* requestMsg,
                                         size_t requestLen);
    std::optional<Response>
        queryMultiportScalarGroupTelemetryHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryAvailableAndClearableScalarGroupHandler(const nsm_msg* requestMsg,
                                                     size_t requestLen);

    std::optional<std::vector<uint8_t>>
        pcieFundamentalResetHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        clearScalarDataSourceHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);

    int mctpSockSend(uint8_t dest, std::vector<uint8_t>& requestMsg);

    std::optional<std::vector<uint8_t>>
        getCurrentEnergyCountHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getVoltageHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getRowRemapStateHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getRowRemappingCountsHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getRowRemapAvailabilityHandler(const nsm_msg* requestMsg,
                                       size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getAltitudePressureHandler(const nsm_msg* requestMsg,
                                   size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getApSkuIdHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        updateApSkuIdHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getEDPpScalingFactorHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setEDPpScalingFactorHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getFpgaDiagnosticsSettingsHandler(const nsm_msg* requestMsg,
                                          size_t requestLen);
    std::optional<std::vector<uint8_t>>
        enableDisableWriteProtectedHandler(const nsm_msg* requestMsg,
                                           size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getDeviceDiagnosticsHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getNetworkDeviceDebugInfoHandler(const nsm_msg* requestMsg,
                                         size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getNetworkDeviceLogInfoHandler(const nsm_msg* requestMsg,
                                       size_t requestLen);
    std::optional<std::vector<uint8_t>>
        eraseTraceHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        eraseDebugInfoHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getDeviceDebugParametersHandler(const nsm_msg* requestMsg,
                                        size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setDeviceDebugParametersHandler(const nsm_msg* requestMsg,
                                        size_t requestLen);

    std::optional<std::vector<uint8_t>>
        enableDisableGpuIstModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen);

    std::optional<std::vector<uint8_t>>
        readThermalParameterHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);

    std::optional<std::vector<uint8_t>>
        queryAggregatedGPMMetrics(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getSupportedGPMMetrics(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        queryAggregatedResetMetrics(const nsm_msg* requestMsg,
                                    size_t requestLen);

    std::optional<std::vector<uint8_t>>
        queryPerInstanceGPMMetrics(const nsm_msg* requestMsg,
                                   size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryPerInstanceGPMMetricsV2(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getReconfigurationPermissionsV1Handler(const nsm_msg* requestMsg,
                                               size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setReconfigurationPermissionsV1Handler(const nsm_msg* requestMsg,
                                               size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getConfidentialComputeModeHandler(const nsm_msg* requestMsg,
                                          size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setConfidentialComputeModeHandler(const nsm_msg* requestMsg,
                                          size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getPowerSmoothingFeatureInfo(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getHwCircuiteryUsage(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getCurrentProfileInfo(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getQueryAdminOverride(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        setActivePresetProfile(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        setupAdminOverride(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        applyAdminOverride(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        toggleImmediateRampDown(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        toggleFeatureState(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getPresetProfileInfo(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        updatePresetProfileParams(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setErrorInjectionModeV1Handler(const nsm_msg* requestMsg,
                                       size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getErrorInjectionModeV1Handler(const nsm_msg* requestMsg,
                                       size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getSupportedErrorInjectionTypesV1Handler(const nsm_msg* requestMsg,
                                                 size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setCurrentErrorInjectionTypesV1Handler(const nsm_msg* requestMsg,
                                               size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getCurrentErrorInjectionTypesV1Handler(const nsm_msg* requestMsg,
                                               size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getErrorInjectionPayloadHandler(const nsm_msg* requestMsg,
                                        size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setErrorInjectionPayloadHandler(const nsm_msg* requestMsg,
                                        size_t requestLen);
    std::optional<std::vector<uint8_t>>
        activateErrorInjectionHandler(const nsm_msg* requestMsg,
                                      size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryTokenParametersHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        provideTokenHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        disableTokensHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryTokenStatusHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryDeviceIdsHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        resetNetworkDeviceHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        enableWorkloadPowerProfile(const nsm_msg* requestMsg,
                                   size_t requestLen);

    std::optional<std::vector<uint8_t>>
        disableWorkloadPowerProfile(const nsm_msg* requestMsg,
                                    size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getWorkLoadProfileStatusInfo(const nsm_msg* requestMsg,
                                     size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getWorkloadPowerProfileInfo(const nsm_msg* requestMsg,
                                    size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getRotInformation(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        irreversibleConfig(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        imageCopyControl(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        codeAuthKeyPermQueryHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        codeAuthKeyPermUpdateHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryFirmwareSecurityVersion(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::optional<std::vector<uint8_t>>
        updateMinSecurityVersion(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setRotProperty(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getEgmModeHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        setEgmModeHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<Request>
        getListAvailablePciePortsHandler(const nsm_msg* requestMsg,
                                         size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getDevicemodeSettingsHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getProtectionOptionsHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getEthPortTelemetryCounterHandler(const nsm_msg* requestMsg,
                                          size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getPortNetworkAddressesHandler(const nsm_msg* requestMsg,
                                       size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getPortEccCountersHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setDevicemodeSettingsHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::vector<uint8_t> getDeviceModeSettingsV2Data(uint32_t deviceModeIndex);
    std::optional<std::vector<uint8_t>>
        getDeviceModeSettingsV2Handler(const nsm_msg* requestMsg,
                                       size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setDeviceModeSettingsV2Handler(const nsm_msg* requestMsg,
                                       size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setDeviceConfigV2Handler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getDeviceConfigV2Handler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        dotCAKInstallHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getPowerSmoothingFeatureInfoV2(const nsm_msg* requestMsg,
                                       size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getCurrentProfileInfoV2(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getQueryAdminOverrideV2(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getPresetProfileInfoV2(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        dotCAKBypassHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        dotLockHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        dotUnlockChallengeHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        dotUnlockHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        dotCAKRotateHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        dotGetInfoHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        dotGetStatusHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getLeakDetectionInfoHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);

    std::optional<std::vector<uint8_t>>
        dotDisableHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        dotOverrideHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        dotRecoveryHandler(const nsm_msg* requestMsg, size_t requestLen);

  private:
    std::optional<Response>
        getQueryScalarGroupTelemetryResponse(uint8_t requestInstanceId,
                                             uint32_t groupId);
    sdeventplus::Event& event;
    bool verbose;
    uint8_t mockEid;
    uint8_t mockDeviceType;
    uint8_t mockInstanceId;
    sdbusplus::asio::object_server& server;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
    int sockFd;
    std::unique_ptr<sdeventplus::source::IO> io;
    eid_t eventReceiverEid;
    uint8_t globalEventGenerationSetting;
    static std::unordered_map<uint8_t, EventSource> supportedEventSources;
    struct State
    {
        nsm_fpga_diagnostics_settings_wp writeProtected;
        uint8_t istMode;
        std::map<reconfiguration_permissions_v1_index,
                 nsm_reconfiguration_permissions_v1>
            prcKnobs;
        nsm_error_injection_mode_v1 errorInjectionMode;
        std::vector<uint8_t> errorInjectionPayload;
        uint8_t l1_prediction_mode;
        std::map<uint8_t, std::map<error_injection_type, bool>> errorInjection;
        uint8_t migMode;
        uint8_t eccMode;
        uint8_t protectionMode;
        // v2 device mode settings: map<device_mode_index, pair<current,
        // pending>> Type-safe structs (defined in libnsm) are serialized into
        // these vectors by the encoding/decoding layer for flexible storage in
        // the mock responder
        std::map<uint32_t,
                 std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>
            deviceModeSettingsV2;
        std::unordered_map<uint8_t,
                           std::array<bitfield8_t, EVENT_SOURCES_LENGTH>>
            eventSources;
    } state;

    // Dump failure-cycle state (see --failure_cycle / kDumpFailureCycle).
    // When failureCycle is false the dump handlers run their happy path.
    // When true, a single global counter walks kDumpFailureCycle: cyclePage-
    // Index selects the page within the current case, cycleCaseIndex selects
    // the case. The pair is advanced by nextDumpCyclePage() on every dump
    // request and wraps at the end of the list.
    bool failureCycle = false;
    uint32_t cycleCaseIndex = 0;
    uint32_t cyclePageIndex = 0;

    // Port-characteristics health-cycle index (see --failure_cycle /
    // queryPortCharacteristicsHandler). Per-instance so each mock starts at 0.
    size_t portHealthCycleIndex = 0;

    // Resolve the page for the current cycle position, log it, and advance
    // the global counter. iterative is true for the page-based commands
    // (0x40/0x50/0x52) and false for single-shot erase (0x51/0x59);
    DumpCyclePage nextDumpCyclePage(std::string_view cmdName, bool iterative);

    // Pre-boot diagnostic session simulation
    struct DiagSessionConfig
    {
        uint8_t configType = 0;
        uint8_t systemTestDuration = 0;
        std::vector<uint8_t> systemDynamicData;
        std::vector<uint8_t> requestedTids;

        struct TidConfig
        {
            uint8_t testDuration = 0;
            uint16_t loops = 0;
            uint8_t logLevel = 0;
            std::vector<uint8_t> dynamicData;
        };
        std::map<uint8_t, TidConfig> tidConfigs;

        void reset()
        {
            *this = {};
        }
    } diagSession;

    enum class DiagSessionState
    {
        IDLE,
        WAIT_SYSTEM_CONFIG,
        REQUESTING_TID_CONFIGS,
        EXECUTING,
        REPORTING,
        DONE,
    };
    DiagSessionState diagSessionState = DiagSessionState::IDLE;
    std::queue<uint8_t> pendingTidRequests;
    std::queue<uint8_t> pendingTidResults;
    sd_event_source* diagTimerSource = nullptr;

    // Helper to safely schedule the next timer, unreffing any prior source
    void scheduleDiagTimer(uint64_t delayUsec);
};

} // namespace MockupResponder
