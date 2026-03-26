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

#include "mockupResponder.hpp"

#include "base.h"
#include "device-capability-discovery.h"
#include "device-configuration.h"
#include "diagnostics.h"
#include "firmware-utils.h"
#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "gpmMetricsList.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <endian.h>
#include <linux/mctp.h>
#include <sys/socket.h>

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <ctime>
#include <functional>

using namespace utils;

namespace MockupResponder
{

std::unordered_map<uint8_t, uint8_t> pciePortConfigMockTable = {
    {0, 17}, // PCIe_Gen_3_Preset
    {1, 17}, // PCIe_Gen_4_Preset
    {2, 17}, // PCIe_Gen_5_Preset
    {3, 17}, // PCIe_Gen_6_Preset
    {4, 1}   // PCIe_Tx_Amplitude
};

EventSource::EventSource(const std::vector<uint64_t>& events)
{
    this->events.fill({0});
    for (auto& event : events)
    {
        if (event >= EVENT_SOURCES_LENGTH * 8)
        {
            lg2::warning("Ignoring unsupported event id {ID}", "ID", event);
            continue;
        }
        auto index = static_cast<size_t>(event) >> 3;
        auto offset = static_cast<uint8_t>(event & 0x7);
        this->events[index].byte |= (1u << offset);
    }
}
std::unordered_map<uint8_t, EventSource>
    MockupResponder::supportedEventSources = {
        {NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
         std::vector<uint64_t>{NSM_REDISCOVERY_EVENT, NSM_LONG_RUNNING_EVENT}},
        {NSM_TYPE_NETWORK_PORT,
         std::vector<uint64_t>{NSM_THRESHOLD_EVENT,
                               NSM_FABRIC_MANAGER_STATE_EVENT}},
        {NSM_TYPE_PLATFORM_ENVIRONMENTAL,
         std::vector<uint64_t>{NSM_RESET_REQUIRED_EVENT, NSM_XID_EVENT}},
};

MockupResponder::MockupResponder(bool verbose, sdeventplus::Event& event,
                                 sdbusplus::asio::object_server& server,
                                 eid_t eid, uint8_t deviceType,
                                 uint8_t instanceId) :
    event(event), verbose(verbose), server(server), eventReceiverEid(0),
    globalEventGenerationSetting(GLOBAL_EVENT_GENERATION_DISABLE),
    state({
        {}, // writeProtected
        0,  // istMode
        {
            {RP_IN_SYSTEM_TEST, {}},
            {RP_FUSING_MODE, {}},
            {RP_CONFIDENTIAL_COMPUTE, {}},
            {RP_BAR0_FIREWALL, {}},
            {RP_CONFIDENTIAL_COMPUTE_DEV_MODE, {}},
            {RP_TOTAL_GPU_POWER_CURRENT_LIMIT, {}},
            {RP_TOTAL_GPU_POWER_RATED_LIMIT, {}},
            {RP_TOTAL_GPU_POWER_MAX_LIMIT, {}},
            {RP_TOTAL_GPU_POWER_MIN_LIMIT, {}},
            {RP_CLOCK_LIMIT, {}},
            {RP_NVLINK_DISABLE, {}},
            {RP_ECC_ENABLE, {}},
            {RP_PCIE_VF_CONFIGURATION, {}},
            {RP_ROW_REMAPPING_ALLOWED, {}},
            {RP_ROW_REMAPPING_FEATURE, {}},
            {RP_HBM_FREQUENCY_CHANGE, {}},
            {RP_HULK_LICENSE_UPDATE, {}},
            {RP_FORCE_TEST_COUPLING, {}},
            {RP_BAR0_TYPE_CONFIG, {}},
            {RP_EDPP_SCALING_FACTOR, {}},
            {RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_1, {}},
            {RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2, {}},
            {RP_EGM_MODE, {}},
            {RP_INFOROM_RECREATE_ALLOW_INB, {}},
        },     // prcKnobs
        {
            0, // mode
            0, // persistent
        },     // errorInjectionMode
        {},    // errorInjectionPayload
        0,
        {
            {NSM_DEV_ID_GPU,
             {
                 {EI_MEMORY_ERRORS, false},
                 {EI_THERMAL_ERRORS, false},
             }},
            {NSM_DEV_ID_SWITCH,
             {
                 {EI_NVLINK_ERRORS, false},
             }},
            {NSM_DEV_ID_PCIE_BRIDGE,
             {
                 {EI_PCI_ERRORS, false},
                 {EI_NVLINK_ERRORS, false},
             }},
            {NSM_DEV_ID_MCTP_BRIDGE,
             {
                 {EI_DEVICE_ERRORS, false},
             }},
        },  // errorInjection
        0,  // migMode
        0,  // eccMode
        0,  // protectionMode
        {}, // eventSources
    })
{
    std::string path = "/xyz/openbmc_project/NSM/" + std::to_string(eid);
    iface = server.add_interface(path, "xyz.openbmc_project.NSM.Device");

    iface->register_method("genRediscoveryEvent", [&](uint8_t eid, bool ackr) {
        sendRediscoveryEvent(eid, ackr);
    });

    iface->register_method("genRawEvent",
                           [&](uint8_t eid, uint8_t nsmType, bool ackr,
                               uint8_t ver, uint8_t eventId, uint8_t eventClass,
                               uint16_t eventState) {
        sendNsmEvent(eid, nsmType, ackr, ver, eventId, eventClass, eventState,
                     0, NULL);
    });

    iface->register_method("genXIDEvent",
                           [&](uint8_t eid, bool ackr, uint8_t flag,
                               uint32_t reason, uint32_t sequence_number,
                               uint64_t timestamp, std::string message_text) {
        sendXIDEvent(eid, ackr, flag, reason, sequence_number, timestamp,
                     message_text);
    });

    iface->register_method(
        "genResetRequiredEvent",
        [&](uint8_t eid, bool ackr) { sendResetRequiredEvent(eid, ackr); });

    iface->register_method(
        "genThreasholdEvent",
        [&](uint8_t dest, bool ackr, bool port_rcv_errors_threshold,
            bool port_xmit_discard_threshold, bool symbol_ber_threshold,
            bool port_rcv_remote_physical_errors_threshold,
            bool port_rcv_switch_relay_errors_threshold,
            bool effective_ber_threshold,
            bool estimated_effective_ber_threshold, uint8_t portNumber) {
        sendThreasholdEvent(
            dest, ackr, port_rcv_errors_threshold, port_xmit_discard_threshold,
            symbol_ber_threshold, port_rcv_remote_physical_errors_threshold,
            port_rcv_switch_relay_errors_threshold, effective_ber_threshold,
            estimated_effective_ber_threshold, portNumber);
    });

    iface->register_method("genFabricManagerStateEvent",
                           [&](uint8_t eid, bool ackr, uint8_t state,
                               uint8_t status, uint64_t last_restart_time,
                               uint64_t last_restart_duration) {
        sendFabricManagerStateEvent(eid, ackr, state, status, last_restart_time,
                                    last_restart_duration);
    });

    iface->register_method(
        "genGpioStateChangeEvent",
        [&](uint8_t dest, bool ackr, uint64_t timestamp,
            std::vector<std::pair<uint16_t, bool>> gpioEvents) {
        sendGpioStateChangeEvent(dest, ackr, timestamp, gpioEvents);
    });

    iface->initialize();

    mockEid = eid;
    mockDeviceType = deviceType;
    mockInstanceId = instanceId;

    sockFd = initSocket();
}

int MockupResponder::initSocket()
{
    auto fd = socket(AF_MCTP, SOCK_DGRAM, 0);

    int rc = 0;
    if (fd == -1)
    {
        rc = -errno;
        lg2::error("Failed to create the socket, RC={RC}, EID={ED}", "RC",
                   strerror(-rc), "ED", mockEid);
        return rc;
    }

    struct sockaddr_mctp addr;
    memset(&addr, 0, sizeof(addr));

    addr.smctp_family = AF_MCTP;
    addr.smctp_network = MCTP_NET_ANY;
    addr.smctp_addr.s_addr = mockEid;
    addr.smctp_tag = MCTP_TAG_OWNER;
    addr.smctp_type = MCTP_MSG_TYPE_PCI_VDM;

    rc = bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (rc == -1)
    {
        rc = -errno;
        lg2::error(
            "Error while binding the socket to NSM Msg Type, RC={RC}, EID={ED}",
            "RC", strerror(-rc), "ED", mockEid);
        return rc;
    }

    if (verbose)
    {
        lg2::info("connect to Mockup EID({EID})", "EID", mockEid);
    }

    auto callback = [this](sdeventplus::source::IO& io, int fd,
                           uint32_t revents) mutable {
        if (!(revents & EPOLLIN))
        {
            return;
        }

        ssize_t peekedLength = recv(fd, nullptr, 0, MSG_PEEK | MSG_TRUNC);
        if (peekedLength == 0)
        {
            io.get_event().exit(0);
        }
        else if (peekedLength <= -1)
        {
            lg2::error("recv system call failed");
            return;
        }

        struct sockaddr_mctp addr;
        memset(&addr, 0, sizeof(addr));

        socklen_t addrlen = sizeof(addr);
        size_t requestMsgSize = peekedLength;
        Request requestMsg(requestMsgSize);
        uint8_t* requestMsgData = requestMsg.data();

        ssize_t recvDataLength = recvfrom(
            fd, static_cast<void*>(requestMsgData), peekedLength, MSG_TRUNC,
            reinterpret_cast<struct sockaddr*>(&addr), &addrlen);

        if (recvDataLength != peekedLength)
        {
            lg2::error("Failure to read peeked length packet. peekedLength="
                       "{PEEKEDLENGTH} recvDataLength={RECVDATALENGTH}",
                       "PEEKEDLENGTH", peekedLength, "RECVDATALENGTH",
                       recvDataLength);
            return;
        }

        if (addr.smctp_type != MCTP_MSG_TYPE_VDM)
        {
            lg2::error("Received non VDM message type={TYPE}", "TYPE",
                       addr.smctp_type);
            return;
        }

        if (verbose)
        {
            utils::printBuffer(utils::Rx, requestMsgData, requestMsgSize,
                               addr.smctp_tag, addr.smctp_addr.s_addr);
        }

        // process message and send response
        std::optional<Response> longRunningEvent;
        auto response = processRxMsg(requestMsg, longRunningEvent);
        if (response)
        {
            struct sockaddr_mctp destAddr;
            memset(&destAddr, 0, sizeof(destAddr));

            destAddr.smctp_family = AF_MCTP;
            destAddr.smctp_network = addr.smctp_network;
            destAddr.smctp_addr.s_addr = addr.smctp_addr.s_addr;
            destAddr.smctp_type = MCTP_MSG_TYPE_VDM;

            constexpr uint8_t tagOwnerBitPos = 3;
            constexpr uint8_t tagOwnerMask = ~(1 << tagOwnerBitPos);
            destAddr.smctp_tag = addr.smctp_tag & tagOwnerMask;

            ssize_t rc = sendto(fd, response->data(), response->size(), 0,
                                reinterpret_cast<struct sockaddr*>(&destAddr),
                                sizeof(destAddr));
            if (rc == -1)
            {
                auto returnCode = -errno;
                lg2::error("sendmsg system call failed, RC={RC}", "RC",
                           returnCode);
            }
            if (verbose)
            {
                utils::printBuffer(utils::Tx, *response, destAddr.smctp_tag,
                                   destAddr.smctp_addr.s_addr);
            }
        }
        if (longRunningEvent)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto eid = requestMsg[1];
            auto rc = mctpSockSend(eid, *longRunningEvent);
            if (rc != NSM_SUCCESS)
            {
                lg2::error("mctpSockSend() failed, rc={RC}", "RC", rc);
            }
        }
    };

    io = std::make_unique<sdeventplus::source::IO>(event, fd, EPOLLIN,
                                                   std::move(callback));
    return fd;
}

std::optional<Response>
    MockupResponder::processRxMsg(const Request& rxMsg,
                                  std::optional<Request>& longRunningEvent)
{
    nsm_header_info hdrFields{};
    auto hdr = reinterpret_cast<const nsm_msg_hdr*>(rxMsg.data());
    if (NSM_SUCCESS != unpack_nsm_header(hdr, &hdrFields))
    {
        lg2::error("Empty NSM request header");
        return std::nullopt;
    }

    if (NSM_EVENT_ACKNOWLEDGMENT == hdrFields.nsm_msg_type)
    {
        if (verbose)
        {
            lg2::info("received NSM event acknowledgement length={LEN}", "LEN",
                      rxMsg.size());
        }
        return std::nullopt;
    }

    if (NSM_REQUEST == hdrFields.nsm_msg_type)
    {
        if (verbose)
        {
            lg2::info("received NSM request length={LEN}", "LEN", rxMsg.size());
        }
    }
    auto request = reinterpret_cast<const nsm_msg*>(hdr);
    size_t requestLen = rxMsg.size();

    auto nvidiaMsgType = request->hdr.nvidia_msg_type;
    auto command = request->payload[0];

    if (verbose)
    {
        lg2::info("nsm msg type={TYPE} command code={COMMAND}", "TYPE",
                  nvidiaMsgType, "COMMAND", command);
    }

    switch (nvidiaMsgType)
    {
        case NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY:
            switch (command)
            {
                case NSM_PING:
                    return pingHandler(request, requestLen);
                case NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES:
                    return getSupportNvidiaMessageTypesHandler(request,
                                                               requestLen);
                case NSM_SUPPORTED_COMMAND_CODES:
                    return getSupportCommandCodeHandler(request, requestLen);
                case NSM_QUERY_DEVICE_IDENTIFICATION:
                    return queryDeviceIdentificationHandler(request,
                                                            requestLen);
                case NSM_GET_EVENT_SUBSCRIPTION:
                    return getEventSubscription(request, requestLen);
                case NSM_SET_EVENT_SUBSCRIPTION:
                    return setEventSubscription(request, requestLen);
                case NSM_GET_SUPPORTED_EVENT_SOURCES:
                    return getSupportedEventSources(request, requestLen);
                case NSM_GET_CURRENT_EVENT_SOURCES:
                    return getCurrentEventSources(request, requestLen);
                case NSM_SET_CURRENT_EVENT_SOURCES:
                    return setCurrentEventSources(request, requestLen);
                case NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT:
                    return configureEventAcknowledgement(request, requestLen);
                case NSM_GET_HISTOGRAM_FORMAT:
                    return getHistogramFormatHandler(request, requestLen);
                case NSM_GET_HISTOGRAM_DATA:
                    return getHistogramDataHandler(request, requestLen);
                case NSM_GET_DEVICE_CAPABILITIES_V2:
                    return getDeviceCapabilitiesV2Handler(request, requestLen);
                case NSM_GET_GPIO_STATE:
                    return getGpioStateHandler(request, requestLen);
                default:
                    lg2::error(
                        "unsupported Command:{CMD} request length={LEN}, msgType={TYPE}",
                        "CMD", command, "LEN", requestLen, "TYPE",
                        nvidiaMsgType);

                    return unsupportedCommandHandler(request, requestLen);
            }
            break;
        case NSM_TYPE_NETWORK_PORT:
            switch (command)
            {
                case NSM_GET_PORT_TELEMETRY_COUNTER:
                    return getPortTelemetryCounterHandler(request, requestLen);
                case NSM_QUERY_PORT_CHARACTERISTICS:
                    return queryPortCharacteristicsHandler(request, requestLen);
                case NSM_QUERY_PORT_STATUS:
                    return queryPortStatusHandler(request, requestLen);
                case NSM_GET_FABRIC_MANAGER_STATE:
                    return getFabricManagerStateHandler(request, requestLen);
                case NSM_QUERY_PORTS_AVAILABLE:
                    return queryPortsAvailableHandler(request, requestLen);
                case NSM_SET_PORT_DISABLE_FUTURE:
                    return setPortDisableFutureHandler(request, requestLen);
                case NSM_GET_PORT_DISABLE_FUTURE:
                    return getPortDisableFutureHandler(request, requestLen);
                case NSM_GET_POWER_MODE:
                    return getPowerModeHandler(request, requestLen);
                case NSM_SET_POWER_MODE:
                    return setPowerModeHandler(request, requestLen);
                case NSM_GET_SWITCH_ISOLATION_MODE:
                    return getSwitchIsolationMode(request, requestLen);
                case NSM_SET_SWITCH_ISOLATION_MODE:
                    return setSwitchIsolationMode(request, requestLen);
                case NSM_GET_ETH_PORT_TELEMETRY_COUNTER:
                    return getEthPortTelemetryCounterHandler(request,
                                                             requestLen);
                case NSM_GET_NETWORK_ADDRESSES:
                    return getPortNetworkAddressesHandler(request, requestLen);
                case NSM_GET_PORT_ECC_COUNTERS:
                    return getPortEccCountersHandler(request, requestLen);
                default:
                    lg2::error(
                        "unsupported Command:{CMD} request length={LEN}, msgType={TYPE}",
                        "CMD", command, "LEN", requestLen, "TYPE",
                        nvidiaMsgType);

                    return unsupportedCommandHandler(request, requestLen);
            }
            break;
        case NSM_TYPE_PLATFORM_ENVIRONMENTAL:
            switch (command)
            {
                case NSM_GET_INVENTORY_INFORMATION:
                    return getInventoryInformationHandler(request, requestLen);
                case NSM_GET_TEMPERATURE_READING:
                    return getTemperatureReadingHandler(request, requestLen);
                case NSM_READ_THERMAL_PARAMETER:
                    return readThermalParameterHandler(request, requestLen);
                case NSM_GET_POWER:
                    return getCurrentPowerDrawHandler(request, requestLen);
                case NSM_GET_MAX_OBSERVED_POWER:
                    return getMaxObservedPowerHandler(request, requestLen);
                case NSM_GET_ENERGY_COUNT:
                    return getCurrentEnergyCountHandler(request, requestLen);
                case NSM_GET_VOLTAGE:
                    return getVoltageHandler(request, requestLen);
                case NSM_GET_ALTITUDE_PRESSURE:
                    return getAltitudePressureHandler(request, requestLen);
                case NSM_GET_DRIVER_INFO:
                    return getDriverInfoHandler(request, requestLen);
                case NSM_GET_MIG_MODE:
                    return getMigModeHandler(request, requestLen, true,
                                             longRunningEvent);
                case NSM_SET_MIG_MODE:
                    return setMigModeHandler(request, requestLen, true,
                                             longRunningEvent);
                case NSM_GET_ECC_MODE:
                    return getEccModeHandler(request, requestLen, true,
                                             longRunningEvent);
                case NSM_SET_ECC_MODE:
                    return setEccModeHandler(request, requestLen, true,
                                             longRunningEvent);
                case NSM_GET_ECC_ERROR_COUNTS:
                    return getEccErrorCountsHandler(request, requestLen);
                case NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR:
                    return getEDPpScalingFactorHandler(request, requestLen);
                case NSM_SET_PROGRAMMABLE_EDPP_SCALING_FACTOR:
                    return setEDPpScalingFactorHandler(request, requestLen);
                case NSM_GET_CLOCK_LIMIT:
                    return getClockLimitHandler(request, requestLen);
                case NSM_SET_CLOCK_LIMIT:
                    return setClockLimitHandler(request, requestLen);
                case NSM_GET_CURRENT_CLOCK_FREQUENCY:
                    return getCurrClockFreqHandler(request, requestLen);
                case NSM_GET_CLOCK_EVENT_REASON_CODES:
                    return getProcessorThrottleReasonHandler(request,
                                                             requestLen);
                case NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME:
                    return getAccumCpuUtilTimeHandler(request, requestLen);
                case NSM_GET_CURRENT_UTILIZATION:
                    return getCurrentUtilizationHandler(request, requestLen,
                                                        true, longRunningEvent);
                case NSM_SET_POWER_LIMITS:
                    return setPowerLimitHandler(request, requestLen);
                case NSM_GET_POWER_LIMITS:
                    return getPowerLimitHandler(request, requestLen);
                case NSM_GET_CLOCK_OUTPUT_ENABLE_STATE:
                    return getClockOutputEnableStateHandler(request,
                                                            requestLen);

                case NSM_GET_ROW_REMAP_STATE_FLAGS:
                    return getRowRemapStateHandler(request, requestLen);
                case NSM_GET_ROW_REMAPPING_COUNTS:
                    return getRowRemappingCountsHandler(request, requestLen);
                case NSM_GET_ROW_REMAP_AVAILABILITY:
                    return getRowRemapAvailabilityHandler(request, requestLen);
                case NSM_GET_LEAK_DETECTION_INFO:
                    return getLeakDetectionInfoHandler(request, requestLen);
                case NSM_GET_MEMORY_CAPACITY_UTILIZATION:
                    return getMemoryCapacityUtilHandler(request, requestLen,
                                                        true, longRunningEvent);
                case NSM_GET_SUPPORTED_GPM_METRICS:
                    return getSupportedGPMMetrics(request, requestLen);
                case NSM_QUERY_AGGREGATE_GPM_METRICS:
                    return queryAggregatedGPMMetrics(request, requestLen);
                case NSM_QUERY_PER_INSTANCE_GPM_METRICS:
                    return queryPerInstanceGPMMetrics(request, requestLen);
                case NSM_QUERY_PER_INSTANCE_GPM_METRICS_V2:
                    return queryPerInstanceGPMMetricsV2(request, requestLen);
                case NSM_GET_VIOLATION_DURATION:
                    return getViolationDurationHandler(request, requestLen,
                                                       true, longRunningEvent);
                /*
                 ** Power Smoothing related Mockups
                 */
                case NSM_PWR_SMOOTHING_TOGGLE_FEATURESTATE:
                    return toggleFeatureState(request, requestLen);
                case NSM_PWR_SMOOTHING_GET_FEATURE_INFO:
                    return getPowerSmoothingFeatureInfo(request, requestLen);
                case NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2:
                    return getPowerSmoothingFeatureInfoV2(request, requestLen);
                case NSM_PWR_SMOOTHING_GET_HARDWARE_CIRCUITRY_LIFETIME_USAGE:
                    return getHwCircuiteryUsage(request, requestLen);
                case NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION:
                    return getCurrentProfileInfo(request, requestLen);
                case NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE:
                    return getQueryAdminOverride(request, requestLen);
                case NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2:
                    return getCurrentProfileInfoV2(request, requestLen);
                case NSM_PWR_SMOOTHING_SET_ACTIVE_PRESET_PROFILE:
                    return setActivePresetProfile(request, requestLen);
                case NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2:
                    return getQueryAdminOverrideV2(request, requestLen);
                case NSM_PWR_SMOOTHING_SETUP_ADMIN_OVERRIDE:
                    return setupAdminOverride(request, requestLen);
                case NSM_PWR_SMOOTHING_APPLY_ADMIN_OVERRIDE:
                    return applyAdminOverride(request, requestLen);
                case NSM_PWR_SMOOTHING_TOGGLE_IMMEDIATE_RAMP_DOWN:
                    return toggleImmediateRampDown(request, requestLen);
                case NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION:
                    return getPresetProfileInfo(request, requestLen);
                case NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2:
                    return getPresetProfileInfoV2(request, requestLen);
                case NSM_PWR_SMOOTHING_UPDATE_PRESET_PROFILE_PARAMETERS:
                    return updatePresetProfileParams(request, requestLen);
                /*
                ** Workload Power profile mock responder
                */
                case NSM_ENABLE_WORKLOAD_POWER_PROFILE:
                    return enableWorkloadPowerProfile(request, requestLen);
                case NSM_DISABLE_WORKLOAD_POWER_PROFILE:
                    return disableWorkloadPowerProfile(request, requestLen);
                case NSM_GET_WORKLOAD_POWER_PROFILE_STATUS_INFO:
                    return getWorkLoadProfileStatusInfo(request, requestLen);
                case NSM_GET_WORKLOAD_POWER_PROFILE_INFO:
                    return getWorkloadPowerProfileInfo(request, requestLen);

                default:
                    lg2::error(
                        "unsupported Command:{CMD} request length={LEN}, msgType={TYPE}",
                        "CMD", command, "LEN", requestLen, "TYPE",
                        nvidiaMsgType);

                    return unsupportedCommandHandler(request, requestLen);
            }
            break;
        case NSM_TYPE_PCI_LINK:
            switch (command)
            {
                case NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1:
                    return queryScalarGroupTelemetryHandler(request,
                                                            requestLen);
                case NSM_ASSERT_PCIE_FUNDAMENTAL_RESET:
                    return pcieFundamentalResetHandler(request, requestLen);
                case NSM_GET_PORT_CONFIGURATION:
                    return getPciePortConfigHandler(request, requestLen);
                case NSM_SET_PORT_CONFIGURATION:
                    return setPciePortConfigHandler(request, requestLen);
                case NSM_CLEAR_DATA_SOURCE_V1:
                    return clearScalarDataSourceHandler(request, requestLen);
                case NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES:
                    return queryAvailableAndClearableScalarGroupHandler(
                        request, requestLen);
                case NSM_LIST_AVAILABLE_PCIE_PORTS:
                    return getListAvailablePciePortsHandler(request,
                                                            requestLen);
                case NSM_MULTIPORT_QUERY_SCALAR_GROUP_TELEMETRY_V2:
                    return queryMultiportScalarGroupTelemetryHandler(
                        request, requestLen);
                default:
                    lg2::error(
                        "unsupported Command:{CMD} request length={LEN}, msgType={TYPE}",
                        "CMD", command, "LEN", requestLen, "TYPE",
                        nvidiaMsgType);
                    return unsupportedCommandHandler(request, requestLen);
            }
            break;
        case NSM_TYPE_DIAGNOSTIC:
            switch (command)
            {
                case NSM_GET_DEVICE_RESET_STATISTICS:
                    return queryAggregatedResetMetrics(request, requestLen);
                case NSM_QUERY_TOKEN_PARAMETERS:
                    return queryTokenParametersHandler(request, requestLen);
                case NSM_PROVIDE_TOKEN:
                    return provideTokenHandler(request, requestLen);
                case NSM_DISABLE_TOKENS:
                    return disableTokensHandler(request, requestLen);
                case NSM_QUERY_TOKEN_STATUS:
                    return queryTokenStatusHandler(request, requestLen);
                case NSM_QUERY_DEVICE_IDS:
                    return queryDeviceIdsHandler(request, requestLen);
                case NSM_RESET_NETWORK_DEVICE:
                    return resetNetworkDeviceHandler(request, requestLen);
                case NSM_ENABLE_DISABLE_WP:
                    return enableDisableWriteProtectedHandler(request,
                                                              requestLen);
                case NSM_GET_DEVICE_DIAGNOSTICS:
                    return getDeviceDiagnosticsHandler(request, requestLen);
                case NSM_GET_NETWORK_DEVICE_DEBUG_INFO:
                    return getNetworkDeviceDebugInfoHandler(request,
                                                            requestLen);
                case NSM_ERASE_TRACE:
                    return eraseTraceHandler(request, requestLen);
                case NSM_GET_NETWORK_DEVICE_LOG_INFO:
                    return getNetworkDeviceLogInfoHandler(request, requestLen);
                case NSM_ERASE_DEBUG_INFO:
                    return eraseDebugInfoHandler(request, requestLen);
                case NSM_GET_DEVICE_DEBUG_PARAMETERS:
                    return getDeviceDebugParametersHandler(request, requestLen);
                case NSM_SET_DEVICE_DEBUG_PARAMETERS:
                    return setDeviceDebugParametersHandler(request, requestLen);
                default:
                    lg2::error(
                        "unsupported Command:{CMD} request length={LEN}, msgType={TYPE}",
                        "CMD", command, "LEN", requestLen, "TYPE",
                        nvidiaMsgType);
                    return unsupportedCommandHandler(request, requestLen);
            }
            break;
        case NSM_TYPE_DEVICE_CONFIGURATION:
            switch (command)
            {
                case NSM_GET_ERROR_INJECTION_MODE_V1:
                    return getErrorInjectionModeV1Handler(request, requestLen);
                case NSM_SET_ERROR_INJECTION_MODE_V1:
                    return setErrorInjectionModeV1Handler(request, requestLen);
                case NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1:
                    return getSupportedErrorInjectionTypesV1Handler(request,
                                                                    requestLen);
                case NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1:
                    return setCurrentErrorInjectionTypesV1Handler(request,
                                                                  requestLen);
                case NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1:
                    return getCurrentErrorInjectionTypesV1Handler(request,
                                                                  requestLen);
                case NSM_GET_ERROR_INJECTION_PAYLOAD:
                    return getErrorInjectionPayloadHandler(request, requestLen);
                case NSM_SET_ERROR_INJECTION_PAYLOAD:
                    return setErrorInjectionPayloadHandler(request, requestLen);
                case NSM_ACTIVATE_ERROR_INJECTION:
                    return activateErrorInjectionHandler(request, requestLen);
                case NSM_GET_RECONFIGURATION_PERMISSIONS_V1:
                    return getReconfigurationPermissionsV1Handler(request,
                                                                  requestLen);
                case NSM_SET_RECONFIGURATION_PERMISSIONS_V1:
                    return setReconfigurationPermissionsV1Handler(request,
                                                                  requestLen);
                case NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1:
                    return getConfidentialComputeModeHandler(request,
                                                             requestLen);
                case NSM_SET_CONFIDENTIAL_COMPUTE_MODE_V1:
                    return setConfidentialComputeModeHandler(request,
                                                             requestLen);
                case NSM_ENABLE_DISABLE_GPU_IST_MODE:
                    return enableDisableGpuIstModeHandler(request, requestLen);
                case NSM_GET_FPGA_DIAGNOSTICS_SETTINGS:
                    return getFpgaDiagnosticsSettingsHandler(request,
                                                             requestLen);
                case NSM_GET_DEVICE_MODE_SETTING:
                    return getDevicemodeSettingsHandler(request, requestLen);
                case NSM_SET_DEVICE_MODE_SETTING:
                    return setDevicemodeSettingsHandler(request, requestLen);
                case NSM_GET_DEVICE_MODE_SETTINGS_V2:
                    return getDeviceModeSettingsV2Handler(request, requestLen);
                case NSM_SET_DEVICE_MODE_SETTINGS_V2:
                    return setDeviceModeSettingsV2Handler(request, requestLen);
                case NSM_GET_PROTECTION_OPTIONS:
                    return getProtectionOptionsHandler(request, requestLen);
                default:
                    lg2::error(
                        "unsupported Command:{CMD} request length={LEN}, msgType={TYPE}",
                        "CMD", command, "LEN", requestLen, "TYPE",
                        nvidiaMsgType);
                    return unsupportedCommandHandler(request, requestLen);
            }
            break;
        case NSM_TYPE_FIRMWARE:
            switch (command)
            {
                case NSM_FW_GET_EROT_STATE_INFORMATION:
                    return getRotInformation(request, requestLen);
                case NSM_FW_IRREVERSABLE_CONFIGURATION:
                    return irreversibleConfig(request, requestLen);
                case NSM_FW_IMAGE_COPY_CONTROL:
                    return imageCopyControl(request, requestLen);
                case NSM_FW_QUERY_CODE_AUTH_KEY_PERM:
                    return codeAuthKeyPermQueryHandler(request, requestLen);
                case NSM_FW_UPDATE_CODE_AUTH_KEY_PERM:
                    return codeAuthKeyPermUpdateHandler(request, requestLen);
                case NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER:
                    return queryFirmwareSecurityVersion(request, requestLen);
                case NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER:
                    return updateMinSecurityVersion(request, requestLen);
                case NSM_FW_SET_ROT_PROPERTY:
                    return setRotProperty(request, requestLen);
                case NSM_FW_DOT_CAK_INSTALL:
                    return dotCAKInstallHandler(request, requestLen);
                case NSM_FW_DOT_CAK_BYPASS:
                    return dotCAKBypassHandler(request, requestLen);
                case NSM_FW_DOT_LOCK:
                    return dotLockHandler(request, requestLen);
                case NSM_FW_DOT_UNLOCK_CHALLENGE:
                    return dotUnlockChallengeHandler(request, requestLen);
                case NSM_FW_DOT_UNLOCK:
                    return dotUnlockHandler(request, requestLen);
                case NSM_FW_DOT_CAK_ROTATE:
                    return dotCAKRotateHandler(request, requestLen);
                case NSM_FW_DOT_GET_INFO:
                    return dotGetInfoHandler(request, requestLen);
                case NSM_FW_DOT_GET_STATUS:
                    return dotGetStatusHandler(request, requestLen);
                case NSM_FW_DOT_DISABLE:
                    return dotDisableHandler(request, requestLen);
                case NSM_FW_DOT_OVERRIDE:
                    return dotOverrideHandler(request, requestLen);
                case NSM_FW_DOT_RECOVERY:
                    return dotRecoveryHandler(request, requestLen);
                default:
                    lg2::error(
                        "unsupported Command:{CMD} request length={LEN}, msgType={TYPE}",
                        "CMD", command, "LEN", requestLen, "TYPE",
                        nvidiaMsgType);
                    return unsupportedCommandHandler(request, requestLen);
            }
        default:
            lg2::error("unsupported Message:{TYPE} request length={LEN}",
                       "TYPE", nvidiaMsgType, "LEN", requestLen);

            return unsupportedCommandHandler(request, requestLen);
    }

    return std::nullopt;
}

bool pmDisabled()
{
    // touch /tmp/pm-disabled for disable persistent mode
    // rm /tmp/pm-disabled for enable persistent mode
    return std::filesystem::exists("/tmp/pm-disabled");
}
Response unsupportedResponse(uint8_t instanceId, uint8_t nvidiaMessageType,
                             uint8_t commandCode)
{
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                      0);
    encode_common_resp(instanceId, NSM_ERR_UNSUPPORTED_COMMAND_CODE, ERR_NULL,
                       nvidiaMessageType, commandCode,
                       reinterpret_cast<nsm_msg*>(response.data()));
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::unsupportedCommandHandler(const nsm_msg* requestMsg,
                                               size_t requestLen)
{
    if (verbose)
    {
        lg2::info("unsupportedCommand: request length={LEN}", "LEN",
                  requestLen);
    }

    return unsupportedResponse(requestMsg->hdr.instance_id,
                               requestMsg->hdr.nvidia_msg_type,
                               requestMsg->payload[0]);
}

std::optional<std::vector<uint8_t>>
    MockupResponder::pingHandler(const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info("pingHandler: request length={LEN}", "LEN", requestLen);
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = 0;
    [[maybe_unused]] auto rc = encode_ping_resp(requestMsg->hdr.instance_id,
                                                reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getSupportNvidiaMessageTypesHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getSupportNvidiaMessageTypesHandler: request length={LEN}",
                  "LEN", requestLen);
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_supported_nvidia_message_types_resp),
        0);

    // this is to mock that type-0, 1, 2, 3, 4, 5 and 6 are supported
    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {
        0x7F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    [[maybe_unused]] auto rc = encode_get_supported_nvidia_message_types_resp(
        requestMsg->hdr.instance_id, cc, reason_code, types, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getSupportCommandCodeHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getSupportCommandCodeHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    auto nvidiaMsgType =
        reinterpret_cast<const nsm_get_supported_command_codes_req*>(
            requestMsg->payload)
            ->nvidia_message_type;

    if (nvidiaMsgType >= NUM_NSM_TYPES)
    {
        lg2::error(
            "getSupportCommandCodeHandler: request msg type={MSG} is not supported",
            "MSG", nvidiaMsgType);
        return std::nullopt;
    }
    std::map<uint8_t, std::map<uint8_t, std::vector<uint8_t>>>
        supportedCommands = {
            {NSM_DEV_ID_BASEBOARD,
             {
                 {0, {0, 1, 2, 9, 10}},
                 {1, {}},
                 {2, {4, 96}},
                 {3, {0, 2, 3, 4, 12, 15, 97, 106}},
                 {4, {101}},
                 {5, {98, 100}},
                 {6,
                  {1, NSM_FW_SET_ROT_PROPERTY, NSM_FW_DOT_CAK_INSTALL,
                   NSM_FW_DOT_CAK_BYPASS, NSM_FW_DOT_LOCK,
                   NSM_FW_DOT_UNLOCK_CHALLENGE, NSM_FW_DOT_UNLOCK,
                   NSM_FW_DOT_CAK_ROTATE, NSM_FW_DOT_GET_INFO,
                   NSM_FW_DOT_GET_STATUS}},
             }},
            {NSM_DEV_ID_SWITCH,
             {
                 {0,
                  {0, 1, 2, 3, 4, 5, 6, 7, 9, 10, NSM_DISCOVER_HISTOGRAM,
                   NSM_GET_HISTOGRAM_FORMAT, NSM_GET_HISTOGRAM_DATA,
                   NSM_GET_DEVICE_CAPABILITIES_V2}},
                 {1, {1, 8, 9, 10, 11, 14, 68, 69}},
                 {2, {4}},
                 {3, {12}},
                 {4,
                  {NSM_GET_NETWORK_DEVICE_DEBUG_INFO, NSM_ERASE_TRACE,
                   NSM_GET_NETWORK_DEVICE_LOG_INFO, NSM_RESET_NETWORK_DEVICE,
                   NSM_QUERY_TOKEN_PARAMETERS, NSM_ERASE_DEBUG_INFO,
                   NSM_PROVIDE_TOKEN, NSM_DISABLE_TOKENS,
                   NSM_QUERY_TOKEN_STATUS, NSM_QUERY_DEVICE_IDS,
                   NSM_GET_DEVICE_DEBUG_PARAMETERS,
                   NSM_SET_DEVICE_DEBUG_PARAMETERS}},
                 {5, {3, 4, 5, 6, 7, 128, 129}},
             }},
            {NSM_DEV_ID_PCIE_BRIDGE,
             {
                 {0, {0, 1, 2, 5, 6, 7, 9, 10, NSM_GET_DEVICE_CAPABILITIES_V2}},
                 {1,
                  {1, NSM_GET_ETH_PORT_TELEMETRY_COUNTER,
                   NSM_GET_NETWORK_ADDRESSES, NSM_GET_PORT_ECC_COUNTERS}},
                 {NSM_TYPE_PCI_LINK,
                  {
                      NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1,
                      NSM_LIST_AVAILABLE_PCIE_PORTS,
                      NSM_MULTIPORT_QUERY_SCALAR_GROUP_TELEMETRY_V2,
                      NSM_GET_PORT_CONFIGURATION,
                      NSM_SET_PORT_CONFIGURATION,
                  }},
                 {3, {12, 14, 97}},
                 {4,
                  {NSM_GET_NETWORK_DEVICE_DEBUG_INFO, NSM_ERASE_TRACE,
                   NSM_GET_NETWORK_DEVICE_LOG_INFO, NSM_QUERY_TOKEN_PARAMETERS,
                   NSM_PROVIDE_TOKEN, NSM_DISABLE_TOKENS,
                   NSM_QUERY_TOKEN_STATUS, NSM_QUERY_DEVICE_IDS}},
                 {5, {3, 4, 5, 6, 7, 128, 129}},
             }},
            {NSM_DEV_ID_GPU,
             {
                 {0,
                  {0, 1, 2, 3, 4, 5, 6, 7, 9, 10, NSM_DISCOVER_HISTOGRAM,
                   NSM_GET_HISTOGRAM_FORMAT, NSM_GET_HISTOGRAM_DATA,
                   NSM_GET_DEVICE_CAPABILITIES_V2}},
                 {1, {1, 65, 66, 67, 68, 69}},
                 {2, {2, 4, 5}},
                 {3, {0,   2,   3,   6,   7,   8,   9,   10,  11,  12,  14,
                      15,  16,  17,  69,  70,  71,  72,  73,  74,  75,  77,
                      78,  79,  97,  114, 115, 116, 117, 119, 121, 123, 124,
                      125, 126, 127, 163, 164, 165, 166, 167, 168, 169, 170,
                      171, 172, 173, 118, 113, 122, 120}},
                 {4,
                  {0, NSM_GET_DEVICE_DIAGNOSTICS,
                   NSM_GET_NETWORK_DEVICE_DEBUG_INFO, NSM_ERASE_TRACE,
                   NSM_GET_NETWORK_DEVICE_LOG_INFO, NSM_ERASE_DEBUG_INFO}},
                 {5,
                  {3, 4, 5, 6, 7, 8, 9, 64, 65, NSM_SET_DEVICE_MODE_SETTINGS_V2,
                   NSM_GET_DEVICE_MODE_SETTINGS_V2}},
                 {6,
                  {1, 2, 3, 4, 5, 6, NSM_FW_DOT_CAK_INSTALL,
                   NSM_FW_DOT_CAK_BYPASS, NSM_FW_DOT_LOCK,
                   NSM_FW_DOT_UNLOCK_CHALLENGE, NSM_FW_DOT_UNLOCK,
                   NSM_FW_DOT_CAK_ROTATE, NSM_FW_DOT_GET_INFO,
                   NSM_FW_DOT_GET_STATUS}},
             }},
            {NSM_DEV_ID_EROT,
             {
                 {0, {0, 1, 2, 9, NSM_GET_DEVICE_CAPABILITIES_V2}},
                 {6,
                  {NSM_FW_GET_EROT_STATE_INFORMATION,
                   NSM_FW_IRREVERSABLE_CONFIGURATION,
                   NSM_FW_QUERY_CODE_AUTH_KEY_PERM,
                   NSM_FW_UPDATE_CODE_AUTH_KEY_PERM,
                   NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER,
                   NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER,
                   NSM_FW_SET_ROT_PROPERTY, NSM_FW_DOT_CAK_INSTALL,
                   NSM_FW_DOT_CAK_BYPASS, NSM_FW_DOT_LOCK,
                   NSM_FW_DOT_UNLOCK_CHALLENGE, NSM_FW_DOT_UNLOCK,
                   NSM_FW_DOT_CAK_ROTATE, NSM_FW_DOT_GET_INFO,
                   NSM_FW_DOT_GET_STATUS}},
             }},
            {NSM_DEV_ID_MCTP_BRIDGE,
             {
                 {NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                  {
                      NSM_PING,
                      NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES,
                      NSM_SUPPORTED_COMMAND_CODES,
                      NSM_GET_SUPPORTED_EVENT_SOURCES,
                      NSM_QUERY_DEVICE_IDENTIFICATION,
                      NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT,
                  }},
                 {NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                  {
                      NSM_GET_TEMPERATURE_READING,
                      NSM_READ_THERMAL_PARAMETER,
                      NSM_GET_POWER,
                      NSM_GET_INVENTORY_INFORMATION,
                  }},
                 {NSM_TYPE_DIAGNOSTIC,
                  {
                      NSM_GET_DEVICE_RESET_STATISTICS,
                      NSM_GET_DEVICE_DIAGNOSTICS,
                  }},
                 {NSM_TYPE_DEVICE_CONFIGURATION,
                  {
                      NSM_SET_ERROR_INJECTION_MODE_V1,
                      NSM_GET_ERROR_INJECTION_MODE_V1,
                      NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1,
                      NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1,
                      NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1,
                      NSM_GET_ERROR_INJECTION_PAYLOAD,
                      NSM_SET_ERROR_INJECTION_PAYLOAD,
                      NSM_ACTIVATE_ERROR_INJECTION,
                      NSM_GET_PROTECTION_OPTIONS,
                  }},
             }},
            {NSM_DEV_ID_CPU,
             {
                 {0, {0, 1, 2, 9, 10}},
                 {1, {}},
                 {2, {}},
                 {3, {0, 2, 3, 12, 97}},
                 {4, {NSM_INSTALL_TOKEN, NSM_ERASE_TOKEN, NSM_QUERY_TOKEN}},
                 {5, {}},
                 {6,
                  {NSM_FW_GET_EROT_STATE_INFORMATION,
                   NSM_FW_IRREVERSABLE_CONFIGURATION,
                   NSM_FW_QUERY_CODE_AUTH_KEY_PERM,
                   NSM_FW_UPDATE_CODE_AUTH_KEY_PERM,
                   NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER,
                   NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER,
                   NSM_FW_SET_ROT_PROPERTY, NSM_FW_IMAGE_COPY_CONTROL,
                   NSM_FW_DOT_GET_INFO, NSM_FW_DOT_CAK_INSTALL, NSM_FW_DOT_LOCK,
                   NSM_FW_DOT_UNLOCK, NSM_FW_DOT_CAK_ROTATE, NSM_FW_DOT_DISABLE,
                   NSM_FW_DOT_OVERRIDE, NSM_FW_DOT_UNLOCK_CHALLENGE,
                   NSM_FW_DOT_RECOVERY, NSM_FW_DOT_CAK_BYPASS,
                   NSM_FW_DOT_GET_STATUS}},
             }},
        };

    bitfield8_t commandCodes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {0};
    for (auto command : supportedCommands[mockDeviceType][nvidiaMsgType])
    {
        commandCodes[command / 8].byte |= 1 << (command % 8);
    }
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    Response response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    [[maybe_unused]] auto rc = encode_get_supported_command_codes_resp(
        requestMsg->hdr.instance_id, cc, reasonCode, commandCodes, responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    return response;
}

void populateFrom(std::vector<uint8_t>& property, const std::string& data)
{
    property.resize(data.length());
    std::copy(data.data(), data.data() + data.length(), property.data());
}
void populateFrom(std::vector<uint8_t>& property, const uint32_t& data)
{
    property.resize(sizeof(data));
    std::copy((uint8_t*)&data, (uint8_t*)&data + sizeof(data), property.data());
}
void MockupResponder::generateDummyGUID(const uint8_t eid, uint8_t* data)
{
    // just adding eid for first byte and rest is zero
    memcpy(data, &eid, sizeof(eid));
}

std::vector<uint8_t> MockupResponder::getProperty(uint8_t propertyIdentifier)
{
    std::vector<uint8_t> property;
    switch (propertyIdentifier)
    {
        case BOARD_PART_NUMBER:
            populateFrom(property, "MCX750500B-0D00_DK");
            break;
        case FRU_PART_NUMBER:
            populateFrom(property, "FRU50500B-0D00_DK");
            break;
        case SERIAL_NUMBER:
            populateFrom(property, "SN123456789");
            break;
        case MARKETING_NAME:
            populateFrom(property, "NV123");
            break;
        case BUILD_DATE:
            populateFrom(property, "2022-08-06T00:00:00Z");
            break;
        case DEVICE_GUID:
            property = std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00};
            generateDummyGUID(mockEid, property.data());
            break;
        case ASSET_TAG:
            populateFrom(property, "MCX750500B-0D00_DK");
            break;
        case INFO_ROM_VERSION:
            populateFrom(property, "128");
            break;
        case PRODUCT_LENGTH:
            populateFrom(property, 850);
            break;
        case PRODUCT_WIDTH:
            populateFrom(property, 730);
            break;
        case PRODUCT_HEIGHT:
            populateFrom(property, 2600);
            break;
        case RATED_MODULE_POWER_LIMIT:
            populateFrom(property, 5555);
            break;
        case MAXIMUM_MODULE_POWER_LIMIT:
            populateFrom(property, 8788);
            break;
        case MINIMUM_MODULE_POWER_LIMIT:
            populateFrom(property, 2754);
            break;
        case MINIMUM_DEVICE_POWER_LIMIT:
            populateFrom(property, 10000);
            break;
        case MAXIMUM_DEVICE_POWER_LIMIT:
            populateFrom(property, 100000);
            break;
        case RATED_DEVICE_POWER_LIMIT:
            populateFrom(property, 80000);
            break;
        case MINIMUM_EDPP_SCALING_FACTOR:
            property.push_back(19);
            break;
        case MAXIMUM_EDPP_SCALING_FACTOR:
            property.push_back(86);
            break;
        case MINIMUM_GRAPHICS_CLOCK_LIMIT:
            populateFrom(property, 100);
            break;
        case DEFAULT_BOOST_CLOCKS:
            populateFrom(property, 30000);
            break;
        case DEFAULT_BASE_CLOCKS:
            populateFrom(property, 300);
            break;
        case MAXIMUM_GRAPHICS_CLOCK_LIMIT:
            populateFrom(property, 5000);
            break;
        case MINIMUM_MEMORY_CLOCK_LIMIT:
            populateFrom(property, 150);
            break;
        case MAXIMUM_MEMORY_CLOCK_LIMIT:
            populateFrom(property, 1500);
            break;
        case RATED_GPU_BASE_POWER_LIMIT:
            populateFrom(property, 200000);
            break;
        case MINIMUM_GPU_BASE_POWER_LIMIT:
            populateFrom(property, 20000);
            break;
        case MAXIMUM_GPU_BASE_POWER_LIMIT:
            populateFrom(property, 300000);
            break;
        case PCIERETIMER_0_EEPROM_VERSION:
        case PCIERETIMER_1_EEPROM_VERSION:
        case PCIERETIMER_2_EEPROM_VERSION:
        case PCIERETIMER_3_EEPROM_VERSION:
        case PCIERETIMER_4_EEPROM_VERSION:
        case PCIERETIMER_5_EEPROM_VERSION:
        case PCIERETIMER_6_EEPROM_VERSION:
        case PCIERETIMER_7_EEPROM_VERSION:
            property = std::vector<uint8_t>{0x01, 0x00, 0x1a, 0x00,
                                            0x00, 0x00, 0x0a, 0x00};
            break;
        case DEVICE_PART_NUMBER:
            populateFrom(property, "A1");
            break;
        case MAXIMUM_MEMORY_CAPACITY:
            populateFrom(property, 200000);
            break;
        default:
            break;
    }
    return property;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPortTelemetryCounterHandler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPortTelemetryCounterHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t portNumber = 0;
    auto rc = decode_get_port_telemetry_counter_req(requestMsg, requestLen,
                                                    &portNumber);
    if (rc != NSM_SW_SUCCESS)
    {
        if (verbose)
        {
            lg2::error("decode_get_port_telemetry_counter_req failed: rc={RC}",
                       "RC", rc);
        }
        return std::nullopt;
    }
    // mock data to send [that is 28 counter data]
    uint32_t supportedCounterData = 0xFFFFFFFF;
    std::vector<uint64_t> data{
        10000000001, 10000000002, 10000000003, 10000000004, 10000000005,
        10000000006, 10000000007, 10000000008, 10000000009, 10000000010,
        10000000011, 10000000012, 10000000013, 10000000014, 10000000015,
        10000000016, 10000000017, 10000000018, 10000000019, 10000000020,
        10000000021, 10000000022, 10000000023, 10000000024, 10000000025,
        10000000026, 10000000027, 10000000028, 10000000029, 10000000030,
        10000000031, 10000000032};

    nsm_port_counter_data portTelData = {};
    std::memcpy(&portTelData, &supportedCounterData,
                sizeof(nsm_supported_port_counter));
    std::memcpy(&portTelData.port_rcv_pkts, data.data(),
                data.size() * sizeof(uint64_t));
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_common_resp) +
                                      sizeof(nsm_port_counter_data),
                                  0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_port_telemetry_counter_resp(requestMsg->hdr.instance_id,
                                                NSM_SUCCESS, reason_code,
                                                &portTelData, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_port_telemetry_counter_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryPortCharacteristicsHandler(const nsm_msg* requestMsg,
                                                     size_t requestLen)
{
    if (verbose)
    {
        lg2::info("queryPortCharacteristicsHandler: request length={LEN}",
                  "LEN", requestLen);
    }

    uint8_t portNumber = 0;
    auto rc = decode_query_port_characteristics_req(requestMsg, requestLen,
                                                    &portNumber);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_query_port_characteristics_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    // mock data to send
    struct nsm_port_characteristics_data portCharData;
    portCharData.port_status.link_state = 1;
    portCharData.port_status.sub_link_state = 6;
    portCharData.port_status.rx_detect_state = 1;
    portCharData.port_status.port_down_reason_code =
        NSM_PORT_DOWN_REASON_CODE_HI_SER_BER;
    portCharData.nv_port_line_rate_mbps = 2500;
    portCharData.nv_port_data_rate_kbps = 3000;
    portCharData.status_lane_info = 225;
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_query_port_characteristics_resp(requestMsg->hdr.instance_id,
                                                NSM_SUCCESS, reason_code,
                                                &portCharData, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_query_port_characteristics_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryPortStatusHandler(const nsm_msg* requestMsg,
                                            size_t requestLen)
{
    if (verbose)
    {
        lg2::info("queryPortStatusHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t portNumber = 0;
    auto rc = decode_query_port_status_req(requestMsg, requestLen, &portNumber);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_query_port_status_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    uint16_t reason_code = ERR_NULL;
    uint8_t port_state = NSM_PORTSTATE_UP;
    uint8_t port_status = NSM_PORTSTATUS_ENABLED;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_query_port_status_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                       reason_code, port_state, port_status,
                                       responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_query_port_status_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getFabricManagerStateHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getFabricManagerStateHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_fabric_manager_state_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_fabric_manager_state_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    // mock data to send
    std::vector<uint8_t> data{
        0x03, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    auto fmStateData =
        reinterpret_cast<nsm_fabric_manager_state_data*>(data.data());
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fabric_manager_state_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_fabric_manager_state_resp(requestMsg->hdr.instance_id,
                                              NSM_SUCCESS, reason_code,
                                              fmStateData, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_fabric_manager_state_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryPortsAvailableHandler(const nsm_msg* requestMsg,
                                                size_t requestLen)
{
    if (verbose)
    {
        lg2::info("queryPortsAvailableHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_query_ports_available_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_query_ports_available_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    // mock data to send
    uint16_t reason_code = ERR_NULL;
    uint8_t number_of_ports = 6;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_ports_available_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_query_ports_available_resp(requestMsg->hdr.instance_id,
                                           NSM_SUCCESS, reason_code,
                                           number_of_ports, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_query_ports_available_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setPortDisableFutureHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setPortDisableFutureHandler: request length={LEN}", "LEN",
                  requestLen);
    }
    bitfield8_t portMask[PORT_MASK_DATA_SIZE];

    auto rc = decode_set_port_disable_future_req(requestMsg, requestLen,
                                                 &portMask[0]);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_port_disable_future_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    // mock data to send
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_disable_future_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_set_port_disable_future_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_query_ports_available_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPortDisableFutureHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPortDisableFutureHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_port_disable_future_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_port_disable_future_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    // mock data to send
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {
        0xFF, 0xFF, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_disable_future_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_port_disable_future_resp(requestMsg->hdr.instance_id, cc,
                                             reason_code, mask, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_port_disable_future_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPowerModeHandler(const nsm_msg* requestMsg,
                                         size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPowerModeHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_power_mode_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_power_mode_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    // mock data to send
    std::vector<uint8_t> data{0x01, 0x02, 0x00, 0x00, 0x00, 0x01, 0x01,
                              0x05, 0x00, 0x06, 0x00, 0x07, 0x00};

    auto powerModeData =
        reinterpret_cast<struct nsm_power_mode_data*>(data.data());
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_power_mode_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                    reason_code, powerModeData, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_power_mode_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setPowerModeHandler(const nsm_msg* requestMsg,
                                         size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setPowerModeHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    struct nsm_power_mode_data data;
    auto rc = decode_set_power_mode_req(requestMsg, requestLen, &data);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_power_mode_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_power_mode_resp(requestMsg->hdr.instance_id, reason_code,
                                    responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_set_power_mode_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getSwitchIsolationMode(const nsm_msg* requestMsg,
                                            size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getSwitchIsolationMode: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_switch_isolation_mode_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_switch_isolation_mode_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    uint8_t isolation_mode = 1;

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_switch_isolation_mode_resp(requestMsg->hdr.instance_id,
                                               NSM_SUCCESS, reason_code,
                                               isolation_mode, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_switch_isolation_mode_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setSwitchIsolationMode(const nsm_msg* requestMsg,
                                            size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setSwitchIsolationMode: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t isolationMode;
    auto rc = decode_set_switch_isolation_mode_req(requestMsg, requestLen,
                                                   &isolationMode);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_switch_isolation_mode_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_set_switch_isolation_mode_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_set_switch_isolation_mode_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getInventoryInformationHandler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getInventoryInformationHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t propertyIdentifier = 0;
    auto rc = decode_get_inventory_information_req(requestMsg, requestLen,
                                                   &propertyIdentifier);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_inventory_information_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    auto property = getProperty(propertyIdentifier);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + property.size(), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_inventory_information_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, property.size(),
        (uint8_t*)property.data(), responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_inventory_information_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryDeviceIdentificationHandler(const nsm_msg* requestMsg,
                                                      size_t requestLen)
{
    if (verbose)
    {
        lg2::info("queryDeviceIdentificationHandler: request length={LEN}",
                  "LEN", requestLen);
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint8_t mockupDeviceIdentification = mockDeviceType;
    uint8_t mockupDeviceInstance = mockInstanceId;

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    [[maybe_unused]] auto rc = encode_query_device_identification_resp(
        requestMsg->hdr.instance_id, cc, reason_code,
        mockupDeviceIdentification, mockupDeviceInstance, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getTemperatureReadingHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    auto request = reinterpret_cast<const nsm_get_temperature_reading_req*>(
        requestMsg->payload);
    uint8_t sensor_id{request->sensor_id};
    if (verbose)
    {
        lg2::info(
            "getTemperatureReadingHandler: Sensor_Id={ID}, request length={LEN}",
            "LEN", requestLen, "ID", sensor_id);
    }

    if (sensor_id == 255)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        [[maybe_unused]] auto rc = encode_aggregate_resp(
            requestMsg->hdr.instance_id, request->hdr.command, NSM_SUCCESS, 2,
            responseMsg);

        uint8_t reading[4]{};
        size_t consumed_len;
        std::array<uint8_t, 50> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        // add sample 1
        rc = encode_aggregate_temperature_reading_data(46.189, reading,
                                                       &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0, true, reading, 4, nsm_sample,
                                          &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 2
        rc = encode_aggregate_temperature_reading_data(-0.343878, reading,
                                                       &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(39, true, reading, 4, nsm_sample,
                                          &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        return response;
    }
    else
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp), 0);

        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        uint16_t reason_code = ERR_NULL;
        [[maybe_unused]] auto rc = encode_get_temperature_reading_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, 78,
            responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        return response;
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::readThermalParameterHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    auto request = reinterpret_cast<const nsm_read_thermal_parameter_req*>(
        requestMsg->payload);

    if (requestLen <
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req))
    {
        if (verbose)
        {
            lg2::info(
                "readThermalParameterHandler: invalid command request length of {LEN}.",
                "LEN", requestLen);
        }

        return std::nullopt;
    }

    uint8_t parameter_id{request->parameter_id};
    if (verbose)
    {
        lg2::info(
            "readThermalParameterHandler: Parameter_Id={ID}, request length={LEN}",
            "LEN", requestLen, "ID", parameter_id);
    }

    if (parameter_id == 255)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        [[maybe_unused]] auto rc = encode_aggregate_resp(
            requestMsg->hdr.instance_id, request->hdr.command, NSM_SUCCESS, 2,
            responseMsg);

        uint8_t reading[4]{};
        size_t consumed_len;
        std::array<uint8_t, 50> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        // add sample 1
        rc = encode_aggregate_thermal_parameter_data(110, reading,
                                                     &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0, true, reading, 4, nsm_sample,
                                          &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 2
        rc = encode_aggregate_thermal_parameter_data(-40, reading,
                                                     &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(39, true, reading, 4, nsm_sample,
                                          &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        return response;
    }
    else
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
                sizeof(struct nsm_read_thermal_parameter_resp),
            0);

        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        uint16_t reason_code = ERR_NULL;
        [[maybe_unused]] auto rc = encode_read_thermal_parameter_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, -10,
            responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        return response;
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getCurrentPowerDrawHandler(const nsm_msg* requestMsg,
                                                size_t requestLen)
{
    auto request = reinterpret_cast<const nsm_get_current_power_draw_req*>(
        requestMsg->payload);
    uint8_t sensor_id{request->sensor_id};
    if (verbose)
    {
        lg2::info(
            "getCurrentPowerDrawHandler: Sensor_Id={ID}, request length={LEN}",
            "LEN", requestLen, "ID", sensor_id);
    }

    if (sensor_id == 255)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        [[maybe_unused]] auto rc = encode_aggregate_resp(
            requestMsg->hdr.instance_id, request->hdr.command, NSM_SUCCESS, 3,
            responseMsg);

        const auto now = std::chrono::system_clock::now();
        std::time_t newt = std::chrono::system_clock::to_time_t(now);
        const auto timestamp = static_cast<uint64_t>(newt);
        const uint32_t power[2]{25890, 17023};
        uint8_t reading[8]{};
        size_t consumed_len;
        std::array<uint8_t, 50> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        // add sample 1
        rc = encode_aggregate_timestamp_data(timestamp, reading, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0xFF, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 2
        rc = encode_aggregate_get_current_power_draw_reading(power[0], reading,
                                                             &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 3
        rc = encode_aggregate_get_current_power_draw_reading(power[1], reading,
                                                             &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(10, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        return response;
    }
    else
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp), 0);

        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        uint16_t reason_code = ERR_NULL;
        uint32_t power{15870};
        [[maybe_unused]] auto rc = encode_get_current_power_draw_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, power,
            responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        return response;
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getMaxObservedPowerHandler(const nsm_msg* requestMsg,
                                                size_t requestLen)
{
    auto request = reinterpret_cast<const nsm_get_max_observed_power_req*>(
        requestMsg->payload);
    uint8_t sensor_id{request->sensor_id};
    if (verbose)
    {
        lg2::info(
            "getMaxObservedPowerHandler: Sensor_Id={ID}, request length={LEN}",
            "LEN", requestLen, "ID", sensor_id);
    }

    if (sensor_id == 255)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        [[maybe_unused]] auto rc = encode_aggregate_resp(
            requestMsg->hdr.instance_id, request->hdr.command, NSM_SUCCESS, 3,
            responseMsg);

        const auto now = std::chrono::system_clock::now();
        std::time_t newt = std::chrono::system_clock::to_time_t(now);
        const auto timestamp = static_cast<uint64_t>(newt);
        const uint32_t power[2]{701890, 682023};
        uint8_t reading[8]{};
        size_t consumed_len;
        std::array<uint8_t, 50> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        // add sample 1
        rc = encode_aggregate_timestamp_data(timestamp, reading, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0xFF, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 2
        rc = encode_aggregate_get_current_power_draw_reading(power[0], reading,
                                                             &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 3
        rc = encode_aggregate_get_current_power_draw_reading(power[1], reading,
                                                             &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(10, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        return response;
    }
    else
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_max_observed_power_resp), 0);

        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        uint16_t reason_code = ERR_NULL;
        uint32_t power{692870};
        [[maybe_unused]] auto rc = encode_get_max_observed_power_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, power,
            responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        return response;
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getDriverInfoHandler(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getDriverInfoHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    // Assuming decode_get_driver_info_req
    auto rc = decode_get_driver_info_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_driver_info_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    // Prepare mock driver info data
    std::string data = "MockDriverVersion 1.0.0";
    std::vector<uint8_t> driver_info_data;
    driver_info_data.resize(data.length() + 2); // +2 for state and null string
    driver_info_data[0] = 2;                    // driver state
    int index = 1;

    for (char& c : data)
    {
        driver_info_data[index++] = static_cast<uint8_t>(c);
    }
    // Add null character at the end, position is data.length() + 1 due
    // to starting at index 0
    driver_info_data[data.length() + 1] = static_cast<uint8_t>('\0');

    if (verbose)
    {
        lg2::info("Mock driver info - State: {STATE}, Version: {VERSION}",
                  "STATE", 2, "VERSION", data);
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      NSM_RESPONSE_CONVENTION_LEN +
                                      driver_info_data.size(),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_driver_info_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                     reason_code, driver_info_data.size(),
                                     (uint8_t*)driver_info_data.data(),
                                     responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_driver_info_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<Response>
    MockupResponder::getMigModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen, bool isLongRunning,
                                       std::optional<Request>& longRunningEvent)
{
    if (verbose)
    {
        lg2::info("getMigModeHandler: request length={LEN}", "LEN", requestLen);
    }
    auto rc = decode_common_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode request for getMigModeHandler failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    if (pmDisabled())
    {
        return unsupportedResponse(requestMsg->hdr.instance_id,
                                   NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   NSM_GET_MIG_MODE);
    }

    bitfield8_t flags;
    flags.byte = state.migMode;
    Response response(
        isLongRunning
            ? /*common resp*/ (sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp))
            : /* regular resp */ (sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_MIG_mode_resp)),
        0);
    if (isLongRunning)
    {
        auto commonMsg = reinterpret_cast<nsm_msg*>(response.data());
        rc = encode_common_resp(requestMsg->hdr.instance_id, NSM_ACCEPTED,
                                ERR_NULL, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_MIG_MODE, commonMsg);
        assert(rc == NSM_SW_SUCCESS);
        longRunningEvent =
            Response((sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                      sizeof(nsm_long_running_resp) + sizeof(bitfield8_t)),
                     0);
        auto eventMsg = reinterpret_cast<nsm_msg*>(longRunningEvent->data());
        auto eventRc = encode_get_MIG_mode_event_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, &flags,
            eventMsg);
        assert(eventRc == NSM_SW_SUCCESS);
    }
    else
    {
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        rc = encode_get_MIG_mode_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                      ERR_NULL, &flags, responseMsg);
    }
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_MIG_mode_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<Response>
    MockupResponder::setMigModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen, bool isLongRunning,
                                       std::optional<Request>& longRunningEvent)
{
    auto rc = decode_set_MIG_mode_req(requestMsg, requestLen, &state.migMode);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_MIG_mode_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (pmDisabled())
    {
        return unsupportedResponse(requestMsg->hdr.instance_id,
                                   NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   NSM_SET_MIG_MODE);
    }

    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_set_MIG_mode_resp(requestMsg->hdr.instance_id,
                                  isLongRunning ? NSM_ACCEPTED : NSM_SUCCESS,
                                  ERR_NULL, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (isLongRunning)
    {
        longRunningEvent = Response((sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                     sizeof(nsm_long_running_resp)),
                                    0);
        auto eventMsg = reinterpret_cast<nsm_msg*>(longRunningEvent->data());
        auto eventRc = encode_set_MIG_mode_event_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, eventMsg);
        assert(eventRc == NSM_SW_SUCCESS);
    }
    return response;
}

std::optional<Response>
    MockupResponder::getEccModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen, bool isLongRunning,
                                       std::optional<Request>& longRunningEvent)
{
    auto rc = decode_common_req(requestMsg, requestLen);
    if (rc)
    {
        lg2::error("decode request for getEccModeHandler failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    if (pmDisabled())
    {
        return unsupportedResponse(requestMsg->hdr.instance_id,
                                   NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   NSM_GET_ECC_MODE);
    }

    bitfield8_t flags;
    flags.byte = state.eccMode;
    Response response(
        isLongRunning
            ? /*common resp*/ (sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp))
            : /* regular resp */ (sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_ECC_mode_resp)),
        0);
    if (isLongRunning)
    {
        auto commonMsg = reinterpret_cast<nsm_msg*>(response.data());
        rc = encode_common_resp(requestMsg->hdr.instance_id, NSM_ACCEPTED,
                                ERR_NULL, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_ECC_MODE, commonMsg);
        assert(rc == NSM_SW_SUCCESS);
        longRunningEvent =
            Response((sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                      sizeof(nsm_long_running_resp) + sizeof(bitfield8_t)),
                     0);
        auto eventMsg = reinterpret_cast<nsm_msg*>(longRunningEvent->data());
        auto eventRc = encode_get_ECC_mode_event_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, &flags,
            eventMsg);
        assert(eventRc == NSM_SW_SUCCESS);
    }
    else
    {
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        rc = encode_get_ECC_mode_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                      ERR_NULL, &flags, responseMsg);
    }
    assert(rc == NSM_SW_SUCCESS);

    if (rc)
    {
        lg2::error("encode_get_ECC_mode_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<Response>
    MockupResponder::setEccModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen, bool isLongRunning,
                                       std::optional<Request>& longRunningEvent)
{
    auto rc = decode_set_ECC_mode_req(requestMsg, requestLen, &state.eccMode);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_ECC_mode_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (pmDisabled())
    {
        return unsupportedResponse(requestMsg->hdr.instance_id,
                                   NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   NSM_SET_ECC_MODE);
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_ECC_mode_resp(requestMsg->hdr.instance_id,
                                  isLongRunning ? NSM_ACCEPTED : NSM_SUCCESS,
                                  reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (isLongRunning)
    {
        longRunningEvent = Response((sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                     sizeof(nsm_long_running_resp)),
                                    0);
        auto eventMsg = reinterpret_cast<nsm_msg*>(longRunningEvent->data());
        auto eventRc = encode_set_ECC_mode_event_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, eventMsg);
        assert(eventRc == NSM_SW_SUCCESS);
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getEccErrorCountsHandler(const nsm_msg* requestMsg,
                                              size_t requestLen)
{
    auto rc = decode_common_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error(
            "decode requset for getEccErrorCountsHandler failed: rc={RC}", "RC",
            rc);
        return std::nullopt;
    }
    struct nsm_ECC_error_counts errorCounts;
    errorCounts.flags.byte = 132;
    errorCounts.sram_corrected = 1234;
    errorCounts.sram_uncorrected_secded = 4532;
    errorCounts.sram_uncorrected_parity = 6567;
    errorCounts.dram_corrected = 9876;
    errorCounts.dram_uncorrected = 9654;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_error_counts_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_ECC_error_counts_resp(requestMsg->hdr.instance_id,
                                          NSM_SUCCESS, reason_code,
                                          &errorCounts, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_ECC_error_counts_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getEventSubscription(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getEventSubscription: request length={LEN}", "LEN",
                  requestLen);
    }
    auto rc = decode_nsm_get_event_subscription_req(requestMsg, requestLen);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("decode_nsm_get_event_subscription_req failed RC={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    Response response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_subscription_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_nsm_get_event_subscription_resp(
        requestMsg->hdr.instance_id,
        eventReceiverEid == 0 ? NSM_ERROR : NSM_SUCCESS, ERR_NULL,
        eventReceiverEid, responseMsg);
    assert(rc == NSM_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setEventSubscription(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setEventSubscription: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t globalSetting = 0;
    uint8_t receiverEid = 0;
    auto rc = decode_nsm_set_event_subscription_req(
        requestMsg, requestLen, &globalSetting, &receiverEid);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("decode_nsm_set_event_subscription_req failed RC={RC}", "RC",
                   rc);
    }

    globalEventGenerationSetting = globalSetting;
    eventReceiverEid = receiverEid;
    if (verbose)
    {
        lg2::info("setEventSubscription: setting={SETTING} ReceiverEID={EID}",
                  "SETTING", globalEventGenerationSetting, "EID",
                  eventReceiverEid);
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN, 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_cc_only_resp(
        requestMsg->hdr.instance_id, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
        NSM_SET_EVENT_SUBSCRIPTION, NSM_SUCCESS, ERR_NULL, responseMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("encode_cc_only_resp failed RC={RC}", "RC", rc);
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getSupportedEventSources(const nsm_msg* requestMsg,
                                              size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getSupportedEventSources: request length={LEN}", "LEN",
                  requestLen);
    }
    uint8_t nvidia_message_type = 0;
    auto rc = decode_nsm_get_event_source_req(requestMsg, requestLen,
                                              &nvidia_message_type);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("decode_nsm_get_event_source_req failed RC={RC}", "RC", rc);
        return std::nullopt;
    }

    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_resp),
                      0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_nsm_get_supported_event_source_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL,
        supportedEventSources[nvidia_message_type].events.data(), responseMsg);
    assert(rc == NSM_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getCurrentEventSources(const nsm_msg* requestMsg,
                                            size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getCurrentEventSources: request length={LEN}", "LEN",
                  requestLen);
    }
    uint8_t nvidia_message_type = 0;
    auto rc = decode_nsm_get_event_source_req(requestMsg, requestLen,
                                              &nvidia_message_type);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("decode_nsm_get_event_source_req failed RC={RC}", "RC", rc);
        return std::nullopt;
    }

    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_resp),
                      0);

    auto& currentEventSources = state.eventSources[nvidia_message_type];

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_nsm_get_current_event_source_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL,
        currentEventSources.data(), responseMsg);
    assert(rc == NSM_SUCCESS);
    return response;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::setCurrentEventSources(const nsm_msg* requestMsg,
                                            size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setCurrentEventSources: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t nvidiaMessageType = 0;
    bitfield8_t eventSources[EVENT_SOURCES_LENGTH] = {0};
    auto rc = decode_nsm_set_current_event_sources_req(
        requestMsg, requestLen, &nvidiaMessageType, eventSources);
    if (rc != NSM_SUCCESS || nvidiaMessageType > NSM_TYPE_FIRMWARE)
    {
        lg2::error("decode_nsm_set_current_event_sources_req failed RC={RC}",
                   "RC", rc);
    }
    for (int i = 0; i < EVENT_SOURCES_LENGTH; i++)
    {
        state.eventSources[nvidiaMessageType][i].byte =
            supportedEventSources[nvidiaMessageType].events[i].byte &
            eventSources[i].byte;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN, 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_cc_only_resp(
        requestMsg->hdr.instance_id, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
        NSM_SET_CURRENT_EVENT_SOURCES, NSM_SUCCESS, ERR_NULL, responseMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("encode_cc_only_resp failed RC={RC}", "RC", rc);
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::configureEventAcknowledgement(const nsm_msg* requestMsg,
                                                   size_t requestLen)
{
    if (verbose)
    {
        lg2::info("configureEventAcknowledgement: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t nvidiaMessageType = 0;
    bitfield8_t* currentEventSourcesAcknowledgementMask;

    auto rc = decode_nsm_configure_event_acknowledgement_req(
        requestMsg, requestLen, &nvidiaMessageType,
        &currentEventSourcesAcknowledgementMask);
    if (rc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_nsm_configure_event_acknowledgement_req failed RC={RC}",
            "RC", rc);
    }

    if (verbose)
    {
        lg2::info(
            "receive configureEventAcknowledgement request, nvidiaMessageType={MSGTYPE} mask[0]={M0} mask[1]={M1}",
            "MSGTYPE", nvidiaMessageType, "M0",
            currentEventSourcesAcknowledgementMask[0].byte, "M1",
            currentEventSourcesAcknowledgementMask[1].byte);
    }

    std::vector<bitfield8_t> newEventSourcesAcknowledgementMask(8);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_configure_event_acknowledgement_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_nsm_configure_event_acknowledgement_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS,
        newEventSourcesAcknowledgementMask.data(), responseMsg);
    assert(rc == NSM_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPowerSmoothingFeatureInfo(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPowerSmoothingFeatureInfo: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_powersmoothing_featinfo_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_powersmoothing_featinfo_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    struct nsm_pwr_smoothing_featureinfo_data feat;
    feat.feature_flag = 7;
    feat.currentTmpSetting = 100000;
    feat.currentTmpFloorSetting = 200000;
    feat.maxTmpFloorSettingInPercent = doubleToNvUFXP4_12(0.95);
    feat.minTmpFloorSettingInPercent = doubleToNvUFXP4_12(0.2);

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_smoothing_feat_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_powersmoothing_featinfo_resp(requestMsg->hdr.instance_id,
                                                 NSM_SUCCESS, reason_code,
                                                 &feat, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_powersmoothing_featinfo_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getHwCircuiteryUsage(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getHwCircuiteryUsage: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_hardware_lifetime_cricuitry_req(requestMsg,
                                                         requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_hardware_lifetime_cricuitry_req: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    struct nsm_hardwarecircuitry_data lifetimeusage;
    lifetimeusage.reading = doubleToNvUFXP8_24(40.45);

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_hardwareciruitry_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    lg2::error("getHwCircuiteryUsage: instanceId={INSTANCEID}", "INSTANCEID",
               requestMsg->hdr.instance_id);

    rc = encode_get_hardware_lifetime_cricuitry_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &lifetimeusage,
        responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_query_admin_override_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPowerSmoothingFeatureInfoV2(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPowerSmoothingFeatureInfoV2: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_powersmoothing_featinfo_v2_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_powersmoothing_featinfo_v2_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    // Initialize response vector
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    response.reserve(256);

    uint16_t samplesCount{};

    // Iterate through mock power smoothing feature info data
    for (const auto& [tag, mockValue] : powerSmoothingFeatureInfoMockTable)
    {
        ++samplesCount;

        uint8_t reading[64]{};
        size_t sample_len{};
        std::array<uint8_t, 256> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());
        if (tag == 0)
        {
            rc = encodeFeatureFlagSample(static_cast<uint32_t>(mockValue),
                                         reading, &sample_len);
        }
        else if (tag == 1)
        {
            rc = encodeCurrentTMPSettingSample(static_cast<uint32_t>(mockValue),
                                               reading, &sample_len);
        }
        else if (tag == 2)
        {
            rc = encodeCurrentTMPFloorSettingSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 3)
        {
            rc = encodeMaxTmpFloorSettingSample(
                static_cast<uint16_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 4)
        {
            rc = encodeMinTmpFloorSettingSample(
                static_cast<uint16_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 5)
        {
            rc = encodeFloorWindowMultiplierSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 6)
        {
            rc = encodeMinPrimaryFloorActivationOffset(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 7)
        {
            rc = encodeMinPrimaryFloorActivationPoint(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else
        {
            lg2::error("Invalid tag: {TAG}", "TAG", tag);
            return std::nullopt;
        }

        rc = encode_aggregate_resp_sample(tag, true, reading, sample_len,
                                          nsm_sample, &sample_len);
        assert(rc == NSM_SW_SUCCESS);
        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), sample_len));
    }

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_get_powersmoothing_featinfo_v2_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, samplesCount, responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getCurrentProfileInfoV2(const nsm_msg* requestMsg,
                                             size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getCurrentProfileInfoV2: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_current_profile_info_v2_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_current_profile_info_v2_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    // Initialize response vector
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    response.reserve(256);

    uint16_t samplesCount{};

    // Iterate through mock current profile info data
    for (const auto& [tag, mockValue] : currentProfileInfoMockTable)
    {
        ++samplesCount;

        uint8_t reading[64]{};
        size_t sample_len{};
        std::array<uint8_t, 256> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());
        if (tag == 0)
        {
            rc = encodeActivePresetProfileSample(
                static_cast<uint8_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 1)
        {
            rc = encodeAdminOverrideMaskSample(static_cast<uint16_t>(mockValue),
                                               reading, &sample_len);
        }
        else if (tag == 2)
        {
            rc = encodeCurrentTMPFloorSample(static_cast<uint16_t>(mockValue),
                                             reading, &sample_len);
        }
        else if (tag == 3)
        {
            rc = encodeCurrentRampupRateSample(static_cast<uint32_t>(mockValue),
                                               reading, &sample_len);
        }
        else if (tag == 4)
        {
            rc = encodeCurrentRampdownRateSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 5)
        {
            rc = encodeCurrentRampdownHysteresisSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 6)
        {
            rc = encodeCurrentSecondaryFloorSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 7)
        {
            rc = encodeCurrentPrimaryFloorActivationWindowMultiplierSample(
                static_cast<uint8_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 8)
        {
            rc = encodeCurrentPrimaryFloorTargetWindowSample(
                static_cast<uint8_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 9)
        {
            rc = encodeCurrentPrimaryFloorActivationOffsetSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else
        {
            lg2::error("Invalid tag: {TAG}", "TAG", tag);
            return std::nullopt;
        }

        rc = encode_aggregate_resp_sample(tag, true, reading, sample_len,
                                          nsm_sample, &sample_len);
        assert(rc == NSM_SW_SUCCESS);
        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), sample_len));
    }

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_get_current_profile_info_v2_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, samplesCount, responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getQueryAdminOverrideV2(const nsm_msg* requestMsg,
                                             size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getQueryAdminOverrideV2: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_admin_override_profile_info_v2_req(requestMsg,
                                                            requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_get_admin_override_profile_info_v2_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    // Initialize response vector
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    response.reserve(256);

    uint16_t samplesCount{};

    // Iterate through mock admin override profile info data
    for (const auto& [tag, mockValue] : adminOverrideMockTable)
    {
        ++samplesCount;

        uint8_t reading[64]{};
        size_t sample_len{};
        std::array<uint8_t, 256> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());
        if (tag == 0)
        {
            rc = encodeCurrentTMPFloorSample(static_cast<uint16_t>(mockValue),
                                             reading, &sample_len);
        }
        else if (tag == 1)
        {
            rc = encodeCurrentRampupRateSample(static_cast<uint32_t>(mockValue),
                                               reading, &sample_len);
        }
        else if (tag == 2)
        {
            rc = encodeCurrentRampdownRateSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 3)
        {
            rc = encodeCurrentRampdownHysteresisSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 4)
        {
            rc = encodeCurrentSecondaryFloorSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 5)
        {
            rc = encodeCurrentPrimaryFloorActivationWindowMultiplierSample(
                static_cast<uint8_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 6)
        {
            rc = encodeCurrentPrimaryFloorTargetWindowSample(
                static_cast<uint8_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 7)
        {
            rc = encodeCurrentPrimaryFloorActivationOffsetSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else
        {
            lg2::error("Invalid tag: {TAG}", "TAG", tag);
            return std::nullopt;
        }

        rc = encode_aggregate_resp_sample(tag, true, reading, sample_len,
                                          nsm_sample, &sample_len);
        assert(rc == NSM_SW_SUCCESS);
        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), sample_len));
    }

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_get_admin_override_profile_info_v2_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, samplesCount, responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPresetProfileInfoV2(const nsm_msg* requestMsg,
                                            size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPresetProfileInfoV2: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_preset_profile_info_v2_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_preset_profile_info_v2_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    // Initialize response vector
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    response.reserve(512);

    uint16_t samplesCount{};

    // Iterate through mock preset profile info data
    for (const auto& [tmpTag, mockValue] : presetProfileInfoMockTable)
    {
        ++samplesCount;
        uint8_t tag = tmpTag >> 3;

        uint8_t reading[64]{};
        size_t sample_len{};
        std::array<uint8_t, 256> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());
        if (tag == 0)
        {
            rc = encodeCurrentTMPFloorSample(static_cast<uint16_t>(mockValue),
                                             reading, &sample_len);
        }
        else if (tag == 1)
        {
            rc = encodeCurrentRampupRateSample(static_cast<uint32_t>(mockValue),
                                               reading, &sample_len);
        }
        else if (tag == 2)
        {
            rc = encodeCurrentRampdownRateSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 3)
        {
            rc = encodeCurrentRampdownHysteresisSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 4)
        {
            rc = encodeCurrentSecondaryFloorSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 5)
        {
            rc = encodeCurrentPrimaryFloorActivationWindowMultiplierSample(
                static_cast<uint8_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 6)
        {
            rc = encodeCurrentPrimaryFloorTargetWindowSample(
                static_cast<uint8_t>(mockValue), reading, &sample_len);
        }
        else if (tag == 7)
        {
            rc = encodeCurrentPrimaryFloorActivationOffsetSample(
                static_cast<uint32_t>(mockValue), reading, &sample_len);
        }
        else
        {
            lg2::error("Invalid tag: {TAG}", "TAG", tag);
            return std::nullopt;
        }

        rc = encode_aggregate_resp_sample(tmpTag, true, reading, sample_len,
                                          nsm_sample, &sample_len);
        assert(rc == NSM_SW_SUCCESS);
        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), sample_len));
    }

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_get_preset_profile_info_v2_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, samplesCount, responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getCurrentProfileInfo(const nsm_msg* requestMsg,
                                           size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getCurrentProfileInfo: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_current_profile_info_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_current_profile_info_req: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    struct nsm_get_current_profile_data profileData;
    profileData.current_active_profile_id = 2;
    profileData.admin_override_mask.bits.tmp_floor_override = 1;
    profileData.admin_override_mask.bits.rampup_rate_override = 1;
    profileData.admin_override_mask.bits.rampdown_rate_override = 0;
    profileData.admin_override_mask.bits.hysteresis_value_override = 0;
    profileData.current_percent_tmp_floor = doubleToNvUFXP4_12(0.3);
    profileData.current_rampup_rate_in_miliwatts_per_second = 180;
    profileData.current_rampdown_rate_in_miliwatts_per_second = 200;
    profileData.current_rampdown_hysteresis_value_in_milisec = 150;
    uint16_t reason_code = ERR_NULL;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_profile_info_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    lg2::error("getCurrentProfileInfo: instanceId={INSTANCEID}", "INSTANCEID",
               requestMsg->hdr.instance_id);

    rc = encode_get_current_profile_info_resp(requestMsg->hdr.instance_id,
                                              NSM_SUCCESS, reason_code,
                                              &profileData, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_current_profile_info_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getQueryAdminOverride(const nsm_msg* requestMsg,
                                           size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getQueryAdminOverride: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_query_admin_override_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_query_admin_override_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    nsm_admin_override_data adminProfileData = {};
    adminProfileData.admin_override_percent_tmp_floor = 0;
    adminProfileData.admin_override_ramup_rate_in_miliwatts_per_second = 180;
    adminProfileData.admin_override_rampdown_rate_in_miliwatts_per_second = 150;
    adminProfileData.admin_override_rampdown_hysteresis_value_in_milisec = 200;
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_admin_override_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    lg2::error("getQueryAdminOverride: instanceId={INSTANCEID}", "INSTANCEID",
               requestMsg->hdr.instance_id);

    rc = encode_query_admin_override_resp(requestMsg->hdr.instance_id,
                                          NSM_SUCCESS, reason_code,
                                          &adminProfileData, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_query_admin_override_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setActivePresetProfile(const nsm_msg* requestMsg,
                                            size_t requestLen)
{
    uint8_t profile_id;
    auto rc = decode_set_active_preset_profile_req(requestMsg, requestLen,
                                                   &profile_id);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_active_preset_profile_req: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    if (verbose)
    {
        lg2::info(
            "setActivePresetProfile: profile_id={PROFILE_ID}, request length={LEN}",
            "LEN", requestLen, "PROFILE_ID", profile_id);
    }

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    lg2::error("setActivePresetProfile: instanceId={INSTANCEID}", "INSTANCEID",
               requestMsg->hdr.instance_id);

    rc = encode_set_active_preset_profile_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_set_active_preset_profile_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setupAdminOverride(const nsm_msg* requestMsg,
                                        size_t requestLen)
{
    uint8_t parameter_id;
    uint32_t param_value;
    auto rc = decode_setup_admin_override_req(requestMsg, requestLen,
                                              &parameter_id, &param_value);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_setup_admin_override_req: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    if (verbose)
    {
        lg2::info("setupAdminOverride: request length={LEN}", "LEN",
                  requestLen);
    }

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    if (parameter_id == 0)
    {
        uint16_t val = param_value >> 16;
        lg2::error(
            "setupAdminOverride: instanceId={INSTANCEID}, parameterId={PARAMETERID}, parameterValue={PARAMETERVALUE}",
            "INSTANCEID", requestMsg->hdr.instance_id, "PARAMETERID",
            parameter_id, "PARAMETERVALUE", val);
    }
    else
    {
        lg2::error(
            "setupAdminOverride: instanceId={INSTANCEID}, parameterId={PARAMETERID}, parameterValue={PARAMETERVALUE}",
            "INSTANCEID", requestMsg->hdr.instance_id, "PARAMETERID",
            parameter_id, "PARAMETERVALUE", param_value);
    }

    rc = encode_setup_admin_override_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_apply_admin_override_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::applyAdminOverride(const nsm_msg* requestMsg,
                                        size_t requestLen)
{
    auto rc = decode_apply_admin_override_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_apply_admin_override_req: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    if (verbose)
    {
        lg2::info("applyAdminOverride: request length={LEN}", "LEN",
                  requestLen);
    }

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    lg2::error("applyAdminOverride: instanceId={INSTANCEID}", "INSTANCEID",
               requestMsg->hdr.instance_id);

    rc = encode_apply_admin_override_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_apply_admin_override_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::toggleImmediateRampDown(const nsm_msg* requestMsg,
                                             size_t requestLen)
{
    uint8_t ramp_down_toggle;
    auto rc = decode_toggle_immediate_rampdown_req(requestMsg, requestLen,
                                                   &ramp_down_toggle);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_toggle_immediate_rampdown_req: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    if (verbose)
    {
        lg2::info("toggleImmediateRampDown: request length={LEN}", "LEN",
                  requestLen);
    }

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    lg2::error(
        "toggleImmediateRampDown: instanceId={INSTANCEID}, ramp_down_toggle={RAMPDOWNTOGGLE}",
        "INSTANCEID", requestMsg->hdr.instance_id, "RAMPDOWNTOGGLE",
        ramp_down_toggle);

    rc = encode_toggle_immediate_rampdown_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_toggle_immediate_rampdown_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::toggleFeatureState(const nsm_msg* requestMsg,
                                        size_t requestLen)
{
    uint8_t feature_state;
    auto rc = decode_toggle_feature_state_req(requestMsg, requestLen,
                                              &feature_state);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_toggle_feature_state_req: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    lg2::error(
        "toggleFeatureState: feature_state={FEATURE_STATE}, request length={LEN}",
        "LEN", requestLen, "FEATURE_STATE", feature_state);

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    lg2::error("toggleFeatureState: instanceId={INSTANCEID}", "INSTANCEID",
               requestMsg->hdr.instance_id);

    rc = encode_toggle_feature_state_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_toggle_feature_state_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPresetProfileInfo(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPresetProfileInfo: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_preset_profile_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_preset_profile_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    const int supported_number_of_profile = 4;

    struct nsm_get_all_preset_profile_meta_data profile_meta_data;
    profile_meta_data.max_profiles_supported = supported_number_of_profile;

    struct nsm_preset_profile_data profiles[supported_number_of_profile];
    for (int i = 0; i < supported_number_of_profile; i++)
    {
        profiles[i].tmp_floor_setting_in_percent =
            doubleToNvUFXP4_12(0.1 * (i + 1));
        profiles[i].ramp_up_rate_in_miliwattspersec = 20 * (i + 1);
        profiles[i].ramp_down_rate_in_miliwattspersec = 30 * (i + 1);
        profiles[i].ramp_hysterisis_rate_in_milisec = 40 * (i + 1);
    }

    uint16_t meta_data_size =
        sizeof(struct nsm_get_all_preset_profile_meta_data);
    uint16_t profile_data_size = sizeof(struct nsm_preset_profile_data);
    // data size is sum of metadata + number of profiles * size of one
    // profile
    uint16_t data_size = meta_data_size +
                         supported_number_of_profile * profile_data_size;

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp) + data_size, 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_preset_profile_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
        &profile_meta_data, profiles, profile_meta_data.max_profiles_supported,
        responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_preset_profile_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::updatePresetProfileParams(const nsm_msg* requestMsg,
                                               size_t requestLen)
{
    if (verbose)
    {
        lg2::info("updatePresetProfileParams: request length={LEN}", "LEN",
                  requestLen);
    }
    size_t s = sizeof(struct nsm_msg_hdr) +
               sizeof(struct nsm_update_preset_profile_req);
    if (verbose)
    {
        lg2::info("updatePresetProfileParams: request length={LEN}", "LEN", s);
    }
    uint8_t profile_id;
    uint8_t parameter_id;
    uint32_t param_value;

    auto rc = decode_update_preset_profile_param_req(
        requestMsg, requestLen, &profile_id, &parameter_id, &param_value);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_update_preset_profile_param_req: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    if (verbose)
    {
        lg2::info("updatePresetProfileParams: request length={LEN}", "LEN",
                  requestLen);
    }

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    if (parameter_id == 0)
    {
        uint16_t val = param_value >> 16;
        lg2::error(
            "updatePresetProfileParams: instanceId={INSTANCEID}, profileId={PROFILEID}, parameterId={PARAMETERID}, parameterValue={PARAMETERVALUE}",
            "INSTANCEID", requestMsg->hdr.instance_id, "PROFILEID", profile_id,
            "PARAMETERID", parameter_id, "PARAMETERVALUE", val);
    }
    else
    {
        lg2::error(
            "updatePresetProfileParams: instanceId={INSTANCEID}, profileId={PROFILEID}, parameterId={PARAMETERID}, parameterValue={PARAMETERVALUE}",
            "INSTANCEID", requestMsg->hdr.instance_id, "PROFILEID", profile_id,
            "PARAMETERID", parameter_id, "PARAMETERVALUE", param_value);
    }

    rc = encode_update_preset_profile_param_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_update_preset_profile_param_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::enableWorkloadPowerProfile(const nsm_msg* requestMsg,
                                                size_t requestLen)
{
    bitfield256_t profile_mask;

    auto rc = decode_enable_workload_power_profile_req(requestMsg, requestLen,
                                                       &profile_mask);

    if (verbose)
    {
        lg2::info("enableWorkloadPowerProfile: request length={LEN}", "LEN",
                  requestLen);
    }
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_enable_workload_power_profile_req: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    lg2::error("enableWorkloadPowerProfile: instanceId={INSTANCEID}",
               "INSTANCEID", requestMsg->hdr.instance_id);
    for (int i = 0; i < 8; i++)
    {
        lg2::error("ProfileMask = {MASK}", "MASK", lg2::hex,
                   profile_mask.fields[i].byte);
    }

    rc = encode_enable_workload_power_profile_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_enable_workload_power_profile_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::disableWorkloadPowerProfile(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    bitfield256_t profile_mask;

    auto rc = decode_disable_workload_power_profile_req(requestMsg, requestLen,
                                                        &profile_mask);

    if (verbose)
    {
        lg2::info("disableWorkloadPowerProfile: request length={LEN}", "LEN",
                  requestLen);
    }
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_disable_workload_power_profile_req: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    lg2::error("disableWorkloadPowerProfile: instanceId={INSTANCEID}",
               "INSTANCEID", requestMsg->hdr.instance_id);
    for (int i = 0; i < 8; i++)
    {
        lg2::error("ProfileMask = {MASK}", "MASK", lg2::hex,
                   profile_mask.fields[i].byte);
    }

    rc = encode_disable_workload_power_profile_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_enable_workload_power_profile_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getWorkLoadProfileStatusInfo(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getWorkLoadProfileStatusInfo: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_workload_power_profile_status_req(requestMsg,
                                                           requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_current_profile_info_req: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    struct workload_power_profile_status profileData;
    for (int i = 0; i < 8; i++)
    {
        profileData.supported_profile_mask.fields[i].byte = 0x0000000a;
        profileData.requested_profile_maks.fields[i].byte = 0x0000000b;
        profileData.enforced_profile_mask.fields[i].byte = 0x0000000c;
    }

    uint16_t reason_code = ERR_NULL;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_workload_power_profile_status_info_resp),
        0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    lg2::error("getWorkLoadProfileStatusInfo: instanceId={INSTANCEID}",
               "INSTANCEID", requestMsg->hdr.instance_id);

    rc = encode_get_workload_power_profile_status_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &profileData,
        responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_workload_power_profile_status_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getWorkloadPowerProfileInfo(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getWorkloadPowerProfileInfo: request length={LEN}", "LEN",
                  requestLen);
    }
    uint16_t identifier;
    uint16_t LAST_PAGE_ID = 3;

    auto rc = decode_get_workload_power_profile_info_req(requestMsg, requestLen,
                                                         &identifier);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_workload_power_profile_info_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    const int supported_number_of_profile_per_page = 5;
    if (verbose)
    {
        lg2::info("getWorkloadPowerProfileInfo: identifier in req = {ID}", "ID",
                  identifier);
    }

    struct nsm_all_workload_power_profile_meta_data profile_meta_data;
    profile_meta_data.number_of_profiles = supported_number_of_profile_per_page;
    if (identifier == LAST_PAGE_ID)
    {
        profile_meta_data.next_identifier =
            0; // last page will have next_identifier as 0
    }
    else
    {
        profile_meta_data.next_identifier = identifier + 1;
    }

    struct nsm_workload_power_profile_data
        profiles[supported_number_of_profile_per_page];
    int startIdentifier = identifier * supported_number_of_profile_per_page;
    int endIdentifier = (identifier + 1) * supported_number_of_profile_per_page;
    for (int i = startIdentifier; i < endIdentifier; i++)
    {
        profiles[i - startIdentifier].profile_id = i;
        profiles[i - startIdentifier].priority = i + 1;
        for (int nthbyte = 0; nthbyte < 8; nthbyte++)
        {
            profiles[i - startIdentifier].conflict_mask.fields[nthbyte].byte =
                0x0000000a;
        }
    }

    uint16_t meta_data_size =
        sizeof(struct nsm_all_workload_power_profile_meta_data);
    uint16_t profile_data_size = sizeof(struct nsm_workload_power_profile_data);
    // data size is sum of metadata + number of profiles * size of one profile
    uint16_t data_size = meta_data_size + supported_number_of_profile_per_page *
                                              profile_data_size;

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp) + data_size, 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_workload_power_profile_info_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
        &profile_meta_data, profiles, profile_meta_data.number_of_profiles,
        responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_workload_power_profile_info_resp failed: rc={RC}", "RC",
            rc);
        return std::nullopt;
    }
    return response;
}

template <class T>
void Logger(bool verbose, const char* msg, const T& data)
{
    if (verbose)
    {
        std::stringstream s;
        s << data;
        std::cout << msg << s.str() << std::endl;
    }
}

int MockupResponder::mctpSockSend(uint8_t destEid,
                                  std::vector<uint8_t>& requestMsg)
{
    if (sockFd < 0)
    {
        lg2::error("mctpSockSend failed, invalid sockFd");
        return NSM_ERROR;
    }

    msghdr msg{};
    iovec iov[2]{};

    uint8_t prefix[3];
    prefix[0] = MCTP_MSG_TAG_REQ;
    prefix[1] = destEid;
    prefix[2] = MCTP_MSG_TYPE_VDM;

    iov[0].iov_base = &prefix[0];
    iov[0].iov_len = sizeof(prefix);
    iov[1].iov_base = requestMsg.data();
    iov[1].iov_len = requestMsg.size();

    msg.msg_iov = iov;
    msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

    if (verbose)
    {
        utils::printBuffer(utils::Rx, requestMsg, prefix[0], prefix[1]);
    }

    auto rc = sendmsg(sockFd, &msg, 0);
    if (rc < 0)
    {
        lg2::error("sendmsg system call failed rc={RC}", "RC", rc);
        return NSM_ERROR;
    }

    return NSM_SUCCESS;
}

void MockupResponder::sendRediscoveryEvent(uint8_t dest, bool ackr)
{
    if (verbose)
    {
        lg2::info("sendRediscoveryEvent dest eid={EID}", "EID", dest);
    }

    uint8_t instanceId = 23;
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_rediscovery_event(instanceId, ackr, msg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("sendRediscoveryEvent failed");
    }

    rc = mctpSockSend(dest, eventMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("mctpSockSend() failed, rc={RC}", "RC", rc);
    }
}

void MockupResponder::sendXIDEvent(uint8_t dest, bool ackr, uint8_t flag,
                                   uint32_t reason, uint32_t sequence_number,
                                   uint64_t timestamp, std::string message_text)
{
    if (verbose)
    {
        lg2::info("sendXIDEvent dest eid={EID}", "EID", dest);
    }

    uint8_t instanceId = 23;
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                  sizeof(nsm_xid_event_payload) +
                                  message_text.size());
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    const nsm_xid_event_payload payload{.flag = flag,
                                        .reserved = {},
                                        .reason = reason,
                                        .sequence_number = sequence_number,
                                        .timestamp = timestamp};

    auto rc = encode_nsm_xid_event(instanceId, ackr, payload,
                                   message_text.data(), message_text.size(),
                                   msg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("sendXIDEvent failed");
    }

    rc = mctpSockSend(dest, eventMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("mctpSockSend() failed, rc={RC}", "RC", rc);
    }
}

void MockupResponder::sendResetRequiredEvent(uint8_t dest, bool ackr)
{
    if (verbose)
    {
        lg2::info("sendResetRequiredEvent dest eid={EID}", "EID", dest);
    }

    uint8_t instanceId = 23;
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_reset_required_event(instanceId, ackr, msg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("sendResetRequiredEvent failed");
    }

    rc = mctpSockSend(dest, eventMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("mctpSockSend() failed, rc={RC}", "RC", rc);
    }
}

void MockupResponder::sendFabricManagerStateEvent(
    uint8_t dest, bool ackr, uint8_t state, uint8_t status,
    uint64_t last_restart_time, uint64_t last_restart_duration)
{
    if (verbose)
    {
        lg2::info("sendFabricManagerStateEvent dest eid={EID}", "EID", dest);
    }

    uint8_t instanceId = 25;
    std::vector<uint8_t> eventMsg(
        sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
        sizeof(nsm_get_fabric_manager_state_event_payload));
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    const nsm_get_fabric_manager_state_event_payload payload{
        .fm_state = state,
        .report_status = status,
        .last_restart_timestamp = last_restart_time,
        .duration_since_last_restart_sec = last_restart_duration};

    auto rc = encode_nsm_get_fabric_manager_state_event(instanceId, ackr,
                                                        payload, msg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("sendFabricManagerStateEvent failed");
    }

    rc = mctpSockSend(dest, eventMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("mctpSockSend() failed, rc={RC}", "RC", rc);
    }
}

void MockupResponder::sendNsmEvent(uint8_t dest, uint8_t nsmType, bool ackr,
                                   uint8_t ver, uint8_t eventId,
                                   uint8_t eventClass, uint16_t eventState,
                                   uint8_t dataSize, uint8_t* data)
{
    if (verbose)
    {
        lg2::info("sendNsmEvent dest eid={EID}", "EID", dest);
    }

    uint8_t instanceId = 23;
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                  dataSize);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_event(instanceId, nsmType, ackr, ver, eventId,
                               eventClass, eventState, dataSize, data, msg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("sendNsmEvent failed");
    }

    rc = mctpSockSend(dest, eventMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("mctpSockSend() failed, rc={RC}", "RC", rc);
    }
}

void MockupResponder::sendThreasholdEvent(
    uint8_t dest, bool ackr, bool port_rcv_errors_threshold,
    bool port_xmit_discard_threshold, bool symbol_ber_threshold,
    bool port_rcv_remote_physical_errors_threshold,
    bool port_rcv_switch_relay_errors_threshold, bool effective_ber_threshold,
    bool estimated_effective_ber_threshold, uint8_t portNumber)
{
    const nsm_health_event_payload payload = {
        .portNumber = portNumber,
        .reserved1 = 0,
        .port_rcv_errors_threshold = port_rcv_errors_threshold,
        .port_xmit_discard_threshold = port_xmit_discard_threshold,
        .symbol_ber_threshold = symbol_ber_threshold,
        .port_rcv_remote_physical_errors_threshold =
            port_rcv_remote_physical_errors_threshold,
        .port_rcv_switch_relay_errors_threshold =
            port_rcv_switch_relay_errors_threshold,
        .effective_ber_threshold = effective_ber_threshold,
        .estimated_effective_ber_threshold = estimated_effective_ber_threshold,
        .reserved2 = 0};

    if (verbose)
    {
        lg2::info("sendThreasholdEvent dest eid={EID}", "EID", dest);
    }
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                  sizeof(nsm_health_event_payload));
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_health_event(mockInstanceId, ackr, &payload, msg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("sendThreasholdEvent failed");
    }

    rc = mctpSockSend(dest, eventMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("mctpSockSend() failed, rc={RC}", "RC", rc);
    }
}

void MockupResponder::sendGpioStateChangeEvent(
    uint8_t dest, bool ackr, uint64_t timestamp,
    const std::vector<std::pair<uint16_t, bool>>& gpioEvents)
{
    if (verbose)
    {
        lg2::info(
            "sendGpioStateChangeEvent dest eid={EID}, num_events={NUM}, timestamp={TS}",
            "EID", dest, "NUM", gpioEvents.size(), "TS", timestamp);
    }

    if (gpioEvents.empty())
    {
        lg2::warning("sendGpioStateChangeEvent: No GPIO events provided");
        return;
    }

    // Calculate the total size needed for the payload
    size_t payloadSize = sizeof(nsm_gpio_state_change_event_payload) -
                         sizeof(nsm_gpio_event) +
                         (gpioEvents.size() * sizeof(nsm_gpio_event));

    // Allocate memory for the payload
    std::vector<uint8_t> payloadBuffer(payloadSize);
    auto payload = reinterpret_cast<nsm_gpio_state_change_event_payload*>(
        payloadBuffer.data());

    // Fill in the payload
    payload->timestamp_low = static_cast<uint32_t>(timestamp & 0xFFFFFFFF);
    payload->timestamp_high =
        static_cast<uint32_t>((timestamp >> 32) & 0xFFFFFFFF);
    payload->num_gpio_events = static_cast<uint16_t>(gpioEvents.size());

    // Fill in the GPIO events
    for (size_t i = 0; i < gpioEvents.size(); ++i)
    {
        payload->gpio_events[i].gpio_index = gpioEvents[i].first &
                                             0x3FFF; // 14 bits
        payload->gpio_events[i].gpio_value = gpioEvents[i].second ? 1 : 0;
        payload->gpio_events[i].reserved = 0;

        if (verbose)
        {
            lg2::info("  GPIO event[{IDX}]: gpio_index={GPIO_IDX}, value={VAL}",
                      "IDX", i, "GPIO_IDX", gpioEvents[i].first, "VAL",
                      gpioEvents[i].second);
        }
    }

    // Allocate message buffer
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                  payloadSize);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    // Encode the event
    auto rc = encode_nsm_gpio_state_change_event(mockInstanceId, ackr, payload,
                                                 msg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("sendGpioStateChangeEvent: encode failed, rc={RC}", "RC",
                   rc);
        return;
    }

    // Send the event
    rc = mctpSockSend(dest, eventMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("sendGpioStateChangeEvent: mctpSockSend() failed, rc={RC}",
                   "RC", rc);
    }
    else if (verbose)
    {
        lg2::info("sendGpioStateChangeEvent: successfully sent to EID={EID}",
                  "EID", dest);
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getCurrentEnergyCountHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    auto request = reinterpret_cast<const nsm_get_current_energy_count_req*>(
        requestMsg->payload);
    uint8_t sensor_id{request->sensor_id};
    if (verbose)
    {
        lg2::info(
            "getCurrentEnergyCountHandler: Sensor_Id={ID}, request length={LEN}",
            "LEN", requestLen, "ID", sensor_id);
    }

    if (sensor_id == 255)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        [[maybe_unused]] auto rc = encode_aggregate_resp(
            requestMsg->hdr.instance_id, request->hdr.command, NSM_SUCCESS, 2,
            responseMsg);

        const uint64_t energy[2]{25890, 17023};
        uint8_t reading[8]{};
        size_t consumed_len{};
        std::array<uint8_t, 50> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        // add sample 1
        rc = encode_aggregate_energy_count_data(energy[0], reading,
                                                &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 2
        rc = encode_aggregate_energy_count_data(energy[1], reading,
                                                &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(45, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        return response;
    }
    else
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_resp), 0);

        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        uint16_t reason_code = ERR_NULL;
        uint32_t energy{15870};
        [[maybe_unused]] auto rc = encode_get_current_energy_count_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, energy,
            responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        return response;
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getVoltageHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    auto request =
        reinterpret_cast<const nsm_get_voltage_req*>(requestMsg->payload);
    uint8_t sensor_id{request->sensor_id};
    if (verbose)
    {
        lg2::info("getVoltageHandler: Sensor_Id={ID}, request length={LEN}",
                  "LEN", requestLen, "ID", sensor_id);
    }

    if (sensor_id == 255)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        [[maybe_unused]] auto rc = encode_aggregate_resp(
            requestMsg->hdr.instance_id, request->hdr.command, NSM_SUCCESS, 2,
            responseMsg);

        const uint32_t voltage[2]{32438470, 57978897};
        uint8_t reading[8]{};
        size_t consumed_len{};
        std::array<uint8_t, 50> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        // add sample 1
        rc = encode_aggregate_voltage_data(voltage[0], reading, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 2
        rc = encode_aggregate_voltage_data(voltage[1], reading, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(33, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        return response;
    }
    else
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_resp), 0);

        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        uint16_t reason_code = ERR_NULL;
        uint32_t voltage{25347808};
        [[maybe_unused]] auto rc =
            encode_get_voltage_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                    reason_code, voltage, responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        return response;
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getAltitudePressureHandler(const nsm_msg* requestMsg,
                                                size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getAltitudePressureHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_altitude_pressure_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    uint32_t pressure{943730};
    auto rc = encode_get_altitude_pressure_resp(requestMsg->hdr.instance_id,
                                                NSM_SUCCESS, reason_code,
                                                pressure, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
}

void getScalarTelemetryGroup0Data(
    struct nsm_query_scalar_group_telemetry_group_0* data)
{
    data->pci_vendor_id = 1234;
    data->pci_device_id = 4321;
    data->pci_subsystem_vendor_id = 3;
    data->pci_subsystem_device_id = 3;
}
void getScalarTelemetryGroup1Data(
    struct nsm_query_scalar_group_telemetry_group_1* data)
{
    data->negotiated_link_speed = 3;
    data->negotiated_link_width = 3;
    data->target_link_speed = 3;
    data->max_link_speed = 4;
    data->max_link_width = 5;
}

void getScalarTelemetryGroup2Data(
    struct nsm_query_scalar_group_telemetry_group_2* data)
{
    data->non_fatal_errors = 1111;
    data->fatal_errors = 2222;
    data->unsupported_request_count = 3333;
    data->correctable_errors = 4444;
}

void getScalarTelemetryGroup3Data(
    struct nsm_query_scalar_group_telemetry_group_3* data)
{
    data->L0ToRecoveryCount = 8769;
}

void getScalarTelemetryGroup4Data(
    struct nsm_query_scalar_group_telemetry_group_4* data)
{
    data->recv_err_cnt = 100;
    data->NAK_recv_cnt = 200;
    data->NAK_sent_cnt = 300;
    data->bad_TLP_cnt = 400;
    data->replay_rollover_cnt = 500;
    data->FC_timeout_err_cnt = 600;
    data->replay_cnt = 700;
}

void getScalarTelemetryGroup5Data(
    struct nsm_query_scalar_group_telemetry_group_5* data)
{
    data->PCIeTXDwords = 8769000;
    data->PCIeRXDwords = 876654;
}
void getScalarTelemetryGroup6Data(
    struct nsm_query_scalar_group_telemetry_group_6* data)
{
    data->ltssm_state = 0x11;
    data->invalid_flit_counter = 111;
}

void getScalarTelemetryGroup8Data(
    struct nsm_query_scalar_group_telemetry_group_8* data)
{
    for (int idx = 1; idx <= TOTAL_PCIE_LANE_COUNT; idx++)
    {
        data->error_counts[idx - 1] = 200 * idx;
    }
}

void getScalarTelemetryGroup9Data(
    struct nsm_query_scalar_group_telemetry_group_9* data)
{
    data->aer_uncorrectable_error_status = 500;
    data->aer_correctable_error_status = 590;
}

void getScalarTelemetryGroup10Data(
    nsm_query_scalar_group_telemetry_group_10* data)
{
    data->outbound_read_tlp_count = 100100;
    data->dwords_transferred_in_outbound_read_tlp_high = 2;
    data->dwords_transferred_in_outbound_read_tlp_low = 200200;
    data->outbound_write_tlp_count = 300300;
    data->dwords_transferred_in_outbound_write_tlp_high = 4;
    data->dwords_transferred_in_outbound_write_tlp_low = 400400;
    data->outbound_completion_tlp_count = 500500;
    data->dwords_transferred_in_outbound_completion = 600600;
    data->read_requests_dropped_tag_unavailable = 700700;
    data->read_requests_dropped_credit_exhaustion = 800800;
    data->read_requests_dropped_credit_not_posted = 900900;
}

std::optional<Response> MockupResponder::getQueryScalarGroupTelemetryResponse(
    uint8_t requestInstanceId, uint32_t groupId)
{
#define encode_query_scalar_group_telemetry_v1_group_resp(GROUP_ID)                           \
    {                                                                                         \
        Response response(                                                                    \
            sizeof(nsm_msg_hdr) +                                                             \
                sizeof(                                                                       \
                    nsm_query_scalar_group_telemetry_v1_group_##GROUP_ID##_resp),             \
            0);                                                                               \
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());                       \
        nsm_query_scalar_group_telemetry_group_##GROUP_ID data;                               \
        getScalarTelemetryGroup##GROUP_ID##Data(&data);                                       \
        auto rc =                                                                             \
            encode_query_scalar_group_telemetry_v1_group##GROUP_ID##_resp(                    \
                requestInstanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);                \
        if (rc != NSM_SW_SUCCESS)                                                             \
        {                                                                                     \
            lg2::error(                                                                       \
                "encode_query_scalar_group_telemetry_v1_group{GROUPID}_resp failed: rc={RC}", \
                "GROUPID", GROUP_ID, "RC", rc);                                               \
            return std::nullopt;                                                              \
        }                                                                                     \
        return response;                                                                      \
    }

    switch (groupId)
    {
        case GROUP_ID_0:
            encode_query_scalar_group_telemetry_v1_group_resp(0);
        case GROUP_ID_1:
            encode_query_scalar_group_telemetry_v1_group_resp(1);
        case GROUP_ID_2:
            encode_query_scalar_group_telemetry_v1_group_resp(2);
        case GROUP_ID_3:
            encode_query_scalar_group_telemetry_v1_group_resp(3);
        case GROUP_ID_4:
            encode_query_scalar_group_telemetry_v1_group_resp(4);
        case GROUP_ID_5:
            encode_query_scalar_group_telemetry_v1_group_resp(5);
        case GROUP_ID_6:
            encode_query_scalar_group_telemetry_v1_group_resp(6);
        case GROUP_ID_8:
            encode_query_scalar_group_telemetry_v1_group_resp(8);
        case GROUP_ID_9:
            encode_query_scalar_group_telemetry_v1_group_resp(9);
        case GROUP_ID_10:
            encode_query_scalar_group_telemetry_v1_group_resp(10);
        default:
            break;
    }

    return std::nullopt;
}

std::optional<Response>
    MockupResponder::queryScalarGroupTelemetryHandler(const nsm_msg* requestMsg,
                                                      size_t requestLen)
{
    uint8_t deviceId;
    uint8_t groupIndex;
    auto rc = decode_query_scalar_group_telemetry_v1_req(
        requestMsg, requestLen, &deviceId, &groupIndex);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_query_scalar_group_telemetry_v1_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    else if (verbose)
    {
        lg2::info(
            "queryScalarGroupTelemetryHandler: deviceId={DEVICEID}, groupIndex={GROUPINDEX}",
            "DEVICEID", deviceId, "GROUPINDEX", groupIndex);
    }

    return getQueryScalarGroupTelemetryResponse(requestMsg->hdr.instance_id,
                                                groupIndex);
}

std::optional<Response>
    MockupResponder::queryMultiportScalarGroupTelemetryHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    nsm_multiport_query_scalar_group_telemetry_v2_req_data data;
    auto rc = decode_multiport_query_scalar_group_telemetry_v1_req(
        requestMsg, requestLen, &data);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_multiport_query_scalar_group_telemetry_v1_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    else if (verbose)
    {
        lg2::info(
            "queryMultiportScalarGroupTelemetryHandler: groupIndex={GROUPINDEX}, type={TYPE}, upstreamPortIndex={UPSTREAMPORTINDEX}, index={INDEX}",
            "GROUPINDEX", int(data.group_index), "TYPE", int(data.type),
            "UPSTREAMPORTINDEX", int(data.upstream_port_index), "INDEX",
            int(data.index));
    }

    auto response = getQueryScalarGroupTelemetryResponse(
        requestMsg->hdr.instance_id, data.group_index);
    if (response)
    {
        auto responseMsg = reinterpret_cast<nsm_msg*>(response->data());
        // Encoded responses functions in
        // `MockupResponder::getQueryScalarGroupTelemetryResponse` uses
        // hardcoded command value `NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1` We need
        // to set the command to `NSM_MULTIPORT_QUERY_SCALAR_GROUP_TELEMETRY_V2`
        // for multiport query unit test coverage
        auto resp = reinterpret_cast<nsm_query_scalar_group_telemetry_v1_resp*>(
            responseMsg->payload);
        resp->hdr.command = NSM_MULTIPORT_QUERY_SCALAR_GROUP_TELEMETRY_V2;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryAvailableAndClearableScalarGroupHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    uint8_t device_index;
    uint8_t group_id;
    auto rc = decode_query_available_clearable_scalar_data_sources_v1_req(
        requestMsg, requestLen, &device_index, &group_id);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_query_available_clearable_scalar_data_sources_v1_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    uint16_t reason_code = ERR_NULL;

    switch (group_id)
    {
        case GROUP_ID_2:
        {
            uint8_t mask_length = 1;
            bitfield8_t available_source[1];
            bitfield8_t clearable_source[1];
            available_source[0].byte = 5;
            clearable_source[0].byte = 63;
            uint16_t data_size = 3;
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(
                        nsm_query_available_clearable_scalar_data_sources_v1_resp) +
                    2 * mask_length * sizeof(uint8_t),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            rc = encode_query_available_clearable_scalar_data_sources_v1_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
                data_size, mask_length, (uint8_t*)available_source,
                (uint8_t*)clearable_source, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_available_clearable_scalar_data_sources_v1_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        case GROUP_ID_3:
        {
            uint8_t mask_length = 1;
            bitfield8_t available_source[1];
            bitfield8_t clearable_source[1];
            available_source[0].byte = 15;
            clearable_source[0].byte = 63;
            uint16_t data_size = 3;
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(
                        nsm_query_available_clearable_scalar_data_sources_v1_resp) +
                    2 * mask_length * sizeof(uint8_t),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            rc = encode_query_available_clearable_scalar_data_sources_v1_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
                data_size, mask_length, (uint8_t*)available_source,
                (uint8_t*)clearable_source, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_available_clearable_scalar_data_sources_v1_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        case GROUP_ID_4:
        {
            uint8_t mask_length = 1;
            bitfield8_t available_source[1];
            bitfield8_t clearable_source[1];
            available_source[0].byte = 32;
            clearable_source[0].byte = 63;
            uint16_t data_size = 3;
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(
                        nsm_query_available_clearable_scalar_data_sources_v1_resp) +
                    2 * mask_length * sizeof(uint8_t),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            rc = encode_query_available_clearable_scalar_data_sources_v1_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
                data_size, mask_length, (uint8_t*)available_source,
                (uint8_t*)clearable_source, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_available_clearable_scalar_data_sources_v1_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }

        case GROUP_ID_8:
        {
            uint8_t mask_length = 1;
            bitfield8_t available_source[1];
            bitfield8_t clearable_source[1];
            available_source[0].byte = 9;
            clearable_source[0].byte = 63;
            uint16_t data_size = 3;
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(
                        nsm_query_available_clearable_scalar_data_sources_v1_resp) +
                    2 * mask_length * sizeof(uint8_t),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            rc = encode_query_available_clearable_scalar_data_sources_v1_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
                data_size, mask_length, (uint8_t*)available_source,
                (uint8_t*)clearable_source, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_available_clearable_scalar_data_sources_v1_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }

        case GROUP_ID_9:
        {
            uint8_t mask_length = 1;
            bitfield8_t available_source[1];
            bitfield8_t clearable_source[1];
            available_source[0].byte = 21;
            clearable_source[0].byte = 63;
            uint16_t data_size = 3;
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(
                        nsm_query_available_clearable_scalar_data_sources_v1_resp) +
                    2 * mask_length * sizeof(uint8_t),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            rc = encode_query_available_clearable_scalar_data_sources_v1_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
                data_size, mask_length, (uint8_t*)available_source,
                (uint8_t*)clearable_source, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_available_clearable_scalar_data_sources_v1_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }

        default:
            break;
    }
    return std::nullopt;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::pcieFundamentalResetHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    uint8_t device_index;
    uint8_t action;
    auto rc = decode_assert_pcie_fundamental_reset_req(requestMsg, requestLen,
                                                       &device_index, &action);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_assert_pcie_fundamental_reset_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_assert_pcie_fundamental_reset_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_assert_pcie_fundamental_reset_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::clearScalarDataSourceHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    uint8_t device_index;
    uint8_t groupId;
    uint8_t dsId;
    auto rc = decode_clear_data_source_v1_req(requestMsg, requestLen,
                                              &device_index, &groupId, &dsId);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_clear_data_source_v1_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_clear_data_source_v1_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_clear_data_source_v1_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getEDPpScalingFactorHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    [[maybe_unused]] auto rc = decode_common_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode request for getEDPpScalingFactorHandler failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    struct nsm_EDPp_scaling_factors scaling_factors;
    scaling_factors.persistent_scaling_factor = 70;
    scaling_factors.oneshot_scaling_factor = 90;
    scaling_factors.enforced_scaling_factor = 60;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_programmable_EDPp_scaling_factor_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_programmable_EDPp_scaling_factor_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &scaling_factors,
        responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_programmable_EDPp_scaling_factor_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setEDPpScalingFactorHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    uint8_t action;
    uint8_t persistence;
    uint8_t scaling_factor;
    auto rc = decode_set_programmable_EDPp_scaling_factor_req(
        requestMsg, requestLen, &action, &persistence, &scaling_factor);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_set_programmable_EDPp_scaling_factor_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_programmable_EDPp_scaling_factor_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error(
            "encode_set_programmable_EDPp_scaling_factor_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getClockLimitHandler(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    uint8_t clock_id;
    auto rc = decode_get_clock_limit_req(requestMsg, requestLen, &clock_id);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode_get_clock_limit_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    if (clock_id != GRAPHICS_CLOCK && clock_id != MEMORY_CLOCK)
    {
        lg2::error("getClockLimitHandler: invalid clock_id = {CLOCK_ID}",
                   "CLOCKID", clock_id);
        return std::nullopt;
    }
    struct nsm_clock_limit clockLimit;
    clockLimit.requested_limit_min = 800;
    clockLimit.requested_limit_max = 1800;
    clockLimit.present_limit_min = 200;
    clockLimit.present_limit_max = 2000;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_clock_limit_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                     reason_code, &clockLimit, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_clock_limit_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setClockLimitHandler(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    uint8_t clock_id;
    uint8_t flags;
    uint32_t limit_min;
    uint32_t limit_max;
    auto rc = decode_set_clock_limit_req(requestMsg, requestLen, &clock_id,
                                         &flags, &limit_min, &limit_max);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_clock_limit_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_clock_limit_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                     reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_set_clock_limit_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getCurrClockFreqHandler(const nsm_msg* requestMsg,
                                             size_t requestLen)
{
    uint8_t clock_id;
    auto rc = decode_get_curr_clock_freq_req(requestMsg, requestLen, &clock_id);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode req for getCurrClockFreqHandler failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    if (clock_id != GRAPHICS_CLOCK && clock_id != MEMORY_CLOCK)
    {
        lg2::error("getCurrClockFreqHandler: invalid clock_id = {CLOCK_ID}",
                   "CLOCKID", clock_id);
        return std::nullopt;
    }

    uint32_t clockFreq = 970;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_curr_clock_freq_resp(requestMsg->hdr.instance_id,
                                         NSM_SUCCESS, reason_code, &clockFreq,
                                         responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_curr_clock_freq_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getProcessorThrottleReasonHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getProcessorThrottleReasonHandler: request length={LEN}",
                  "LEN", requestLen);
    }

    auto rc = decode_get_current_clock_event_reason_code_req(requestMsg,
                                                             requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_get_current_clock_event_reason_code_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    bitfield32_t flags;
    flags.byte = 63;
    if (verbose)
    {
        lg2::info("value of flag is {FLAGS}", "FLAGS", flags.byte);
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_current_clock_event_reason_code_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_current_clock_event_reason_code_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &flags,
        responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error(
            "encode_get_current_clock_event_reason_code_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPowerLimitHandler(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPowerLimitHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint32_t id;
    auto rc = decode_get_power_limit_req(requestMsg, requestLen, &id);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_power_limit_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    assert(rc == NSM_SW_SUCCESS);
    // limits are in miliwatts
    uint32_t requested_persistent_limit = 10000;
    uint32_t requested_oneshot_limit = 15000;
    uint32_t enforced_limit = 12500;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_power_limit_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                     reason_code, requested_persistent_limit,
                                     requested_oneshot_limit, enforced_limit,
                                     responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_power_limit_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setPowerLimitHandler(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setPowerLimitHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint32_t id;
    uint8_t action;
    uint8_t persistent;
    uint32_t power_limit;
    auto rc = decode_set_power_limit_req(requestMsg, requestLen, &id, &action,
                                         &persistent, &power_limit);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_power_limit_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_power_limit_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                     reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_set_power_limit_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getAccumCpuUtilTimeHandler(const nsm_msg* requestMsg,
                                                size_t requestLen)
{
    auto rc = decode_common_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode req for getAccumCpuUtilTimeHandler failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    uint32_t context_util_time = 4987;
    uint32_t SM_util_time = 2564;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_accum_GPU_util_time_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_accum_GPU_util_time_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
        &context_util_time, &SM_util_time, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_accum_GPU_util_time_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<Response> MockupResponder::getCurrentUtilizationHandler(
    const nsm_msg* requestMsg, size_t requestLen, bool isLongRunning,
    std::optional<Request>& longRunningEvent)
{
    auto rc = decode_common_req(requestMsg, requestLen);
    if (rc)
    {
        lg2::error(
            "decode req for getCurrentUtilizationHandler failed: rc={RC}", "RC",
            rc);
        return std::nullopt;
    }

    if (pmDisabled())
    {
        return unsupportedResponse(requestMsg->hdr.instance_id,
                                   NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   NSM_GET_CURRENT_UTILIZATION);
    }

    nsm_get_current_utilization_data data{36, 75};

    Response response(
        isLongRunning
            ? /*common resp*/ (sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp))
            : /* regular resp */ (sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_current_utilization_resp)),
        0);
    if (isLongRunning)
    {
        auto commonMsg = reinterpret_cast<nsm_msg*>(response.data());
        rc = encode_common_resp(requestMsg->hdr.instance_id, NSM_ACCEPTED,
                                ERR_NULL, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_CURRENT_UTILIZATION, commonMsg);
        assert(rc == NSM_SW_SUCCESS);
        longRunningEvent = Response((sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                     sizeof(nsm_long_running_resp) +
                                     sizeof(nsm_get_current_utilization_data)),
                                    0);
        auto eventMsg = reinterpret_cast<nsm_msg*>(longRunningEvent->data());
        auto eventRc = encode_get_current_utilization_event_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, &data,
            eventMsg);
        assert(eventRc == NSM_SW_SUCCESS);
    }
    else
    {
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        rc = encode_get_current_utilization_resp(requestMsg->hdr.instance_id,
                                                 NSM_SUCCESS, ERR_NULL, &data,
                                                 responseMsg);
    }

    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_current_utilization_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getRowRemapStateHandler(const nsm_msg* requestMsg,
                                             size_t requestLen)
{
    auto rc = decode_get_row_remap_state_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_row_remap_state_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    bitfield8_t flags;
    flags.byte = 13;
    if (verbose)
    {
        lg2::info("value of flag is {FLAGS}", "FLAGS", flags.byte);
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_row_remap_state_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_row_remap_state_resp(requestMsg->hdr.instance_id,
                                         NSM_SUCCESS, reason_code, &flags,
                                         responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_row_remap_state_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getRowRemappingCountsHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    auto rc = decode_get_row_remapping_counts_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode_get_row_remapping_counts_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    uint32_t correctable_error = 4987;
    uint32_t uncorrectable_error = 2564;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_row_remapping_counts_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_row_remapping_counts_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
        correctable_error, uncorrectable_error, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_row_remapping_counts_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getRowRemapAvailabilityHandler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    auto rc = decode_get_row_remap_availability_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode_get_row_remap_availability_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    struct nsm_row_remap_availability data;
    data.high_remapping = 100;
    data.low_remapping = 200;
    data.max_remapping = 300;
    data.no_remapping = 400;
    data.partial_remapping = 500;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_row_remap_availability_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_row_remap_availability_resp(requestMsg->hdr.instance_id,
                                                NSM_SUCCESS, reason_code, &data,
                                                responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_row_remap_availability_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getLeakDetectionInfoHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    auto rc = decode_get_leak_detection_info_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_leak_detection_info_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    // Mock data: 2 sensors with 3 threshold levels each
    constexpr uint8_t numberOfSensors = 2;
    constexpr uint8_t numberOfThresholdLevels = 3;

    // Calculate expected sensor info size
    size_t expectedSensorInfoSize =
        sizeof(uint8_t) + sizeof(uint8_t) +
        (numberOfThresholdLevels * sizeof(uint16_t)) + sizeof(uint16_t);
    size_t sensorsDataLen = numberOfSensors * expectedSensorInfoSize;

    // Allocate buffer for sensors data
    std::vector<uint8_t> sensorsDataBuf(sensorsDataLen, 0);

    // Populate mock sensor data using the structure
    uint8_t* ptr = sensorsDataBuf.data();
    for (uint8_t i = 0; i < numberOfSensors; i++)
    {
        auto* sensor =
            reinterpret_cast<struct nsm_leak_detection_sensors_data*>(ptr);
        sensor->sensor_id = i;
        sensor->leak_state =
            (i == 0) ? 0 : 1; // First sensor no leak, second has leak
        sensor->thresholds[0] = 100 + (i * 10); // Threshold 1
        sensor->thresholds[1] = 200 + (i * 10); // Threshold 2
        sensor->thresholds[2] = 300 + (i * 10); // Threshold 3
        sensor->adc_reading = 150 + (i * 20);   // ADC reading
        ptr += expectedSensorInfoSize;
    }

    // Calculate response size
    size_t responseSize = sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_leak_detection_info_resp) - 1 +
                          sensorsDataLen;
    std::vector<uint8_t> response(responseSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    uint16_t reason_code = ERR_NULL;
    rc = encode_get_leak_detection_info_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, numberOfSensors,
        numberOfThresholdLevels, sensorsDataBuf.data(), sensorsDataLen,
        responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_leak_detection_info_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info(
            "getLeakDetectionInfoHandler: numberOfSensors={NS}, numberOfThresholdLevels={NTL}",
            "NS", numberOfSensors, "NTL", numberOfThresholdLevels);
    }

    return response;
}

std::optional<Response> MockupResponder::getMemoryCapacityUtilHandler(
    const nsm_msg* requestMsg, size_t requestLen, bool isLongRunning,
    std::optional<Request>& longRunningEvent)
{
    if (verbose)
    {
        lg2::info("getMemoryCapacityUtilHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_memory_capacity_util_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_memory_capacity_util_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    if (pmDisabled())
    {
        return unsupportedResponse(requestMsg->hdr.instance_id,
                                   NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   NSM_GET_MEMORY_CAPACITY_UTILIZATION);
    }

    struct nsm_memory_capacity_utilization data;
    data.reserved_memory = 2345567;
    data.used_memory = 128888;

    Response response(
        isLongRunning
            ? /*common resp*/ (sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp))
            : /* regular resp */ (sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_memory_capacity_util_resp)),
        0);

    if (isLongRunning)
    {
        auto commonMsg = reinterpret_cast<nsm_msg*>(response.data());
        rc = encode_common_resp(requestMsg->hdr.instance_id, NSM_ACCEPTED,
                                ERR_NULL, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_MEMORY_CAPACITY_UTILIZATION, commonMsg);
        assert(rc == NSM_SW_SUCCESS);
        longRunningEvent = Response((sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                     sizeof(nsm_long_running_resp) +
                                     sizeof(nsm_memory_capacity_utilization)),
                                    0);
        auto eventMsg = reinterpret_cast<nsm_msg*>(longRunningEvent->data());
        auto eventRc = encode_get_memory_capacity_util_event_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, &data,
            eventMsg);
        assert(eventRc == NSM_SW_SUCCESS);
    }
    else
    {
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        rc = encode_get_memory_capacity_util_resp(requestMsg->hdr.instance_id,
                                                  NSM_SUCCESS, ERR_NULL, &data,
                                                  responseMsg);
    }
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_memory_capacity_util_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getClockOutputEnableStateHandler(const nsm_msg* requestMsg,
                                                      size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getClockOutputEnableStateHandler: request length={LEN}",
                  "LEN", requestLen);
    }

    uint8_t index = 0;
    auto rc = decode_get_clock_output_enable_state_req(requestMsg, requestLen,
                                                       &index);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode_get_clock_output_enable_state_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    if (index != PCIE_CLKBUF_INDEX && index != NVHS_CLKBUF_INDEX &&
        index != IBLINK_CLKBUF_INDEX)
    {
        lg2::error("getClockOutputEnableStateHandler: invalid index = {INDEX}",
                   "INDEX", index);
        return std::nullopt;
    }
    uint32_t clk_buf_data = 0xABABABAB;
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_get_clock_output_enable_state_resp(requestMsg->hdr.instance_id,
                                                   NSM_SUCCESS, reason_code,
                                                   clk_buf_data, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_clock_output_enable_state_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::getFpgaDiagnosticsSettingsHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getFpgaDiagnosticsSettingsHandler: request length={LEN}",
                  "LEN", requestLen);
    }

    fpga_diagnostics_settings_data_index data_index;
    [[maybe_unused]] auto rc = decode_get_fpga_diagnostics_settings_req(
        requestMsg, requestLen, &data_index);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getFpgaDiagnosticsSettingsHandler: decode_query_scalar_group_telemetry_v1_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    switch (data_index)
    {
        case GET_WP_SETTINGS:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(nsm_fpga_diagnostics_settings_wp_resp),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

            uint16_t reason_code = ERR_NULL;
            rc = encode_get_fpga_diagnostics_settings_wp_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
                &state.writeProtected, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "getFpgaDiagnosticsSettingsHandler: encode_get_fpga_diagnostics_settings_wp_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        case GET_WP_JUMPER_PRESENCE:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            struct nsm_fpga_diagnostics_settings_wp_jumper data = {0, 0};

            uint16_t reason_code = ERR_NULL;
            rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &data,
                responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "getFpgaDiagnosticsSettingsHandler: encode_get_fpga_diagnostics_settings_wp_jumper_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        case GET_POWER_SUPPLY_STATUS:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_supply_status_resp),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

            uint16_t reason_code = ERR_NULL;
            rc = encode_get_power_supply_status_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
                0b00110011, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "getFpgaDiagnosticsSettingsHandler: encode_get_power_supply_status_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        case GET_GPU_PRESENCE:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpu_presence_resp), 0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

            uint16_t reason_code = ERR_NULL;
            rc = encode_get_gpu_presence_resp(requestMsg->hdr.instance_id,
                                              NSM_SUCCESS, reason_code,
                                              0b11111111, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "getFpgaDiagnosticsSettingsHandler: encode_get_gpu_presence_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        case GET_GPU_POWER_STATUS:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpu_power_status_resp), 0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

            uint16_t reason_code = ERR_NULL;
            rc = encode_get_gpu_power_status_resp(requestMsg->hdr.instance_id,
                                                  NSM_SUCCESS, reason_code,
                                                  0b11110111, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "getFpgaDiagnosticsSettingsHandler: encode_get_gpu_power_status_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        case GET_GPU_IST_MODE_SETTINGS:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpu_ist_mode_resp), 0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

            uint16_t reason_code = ERR_NULL;
            rc = encode_get_gpu_ist_mode_resp(requestMsg->hdr.instance_id,
                                              NSM_SUCCESS, reason_code,
                                              state.istMode, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "getFpgaDiagnosticsSettingsHandler: encode_get_gpu_ist_mode_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        default:
            break;
    }
    return std::nullopt;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::enableDisableWriteProtectedHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info("enableDisableWriteProtectedHandler: request length={LEN}",
                  "LEN", requestLen);
    }

    diagnostics_enable_disable_wp_data_index data_index;
    uint8_t value = 0;
    [[maybe_unused]] auto rc = decode_enable_disable_wp_req(
        requestMsg, requestLen, &data_index, &value);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "enableDisableWriteProtectedHandler: decode_enable_disable_wp_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    auto& writeProtected = state.writeProtected;
    switch (data_index)
    {
        case BASEBOARD_FRU_EEPROM:
        case CX7_FRU_EEPROM:
        case HMC_FRU_EEPROM:
            writeProtected.baseboard = value;
            break;
        case HMC_SPI_FLASH:
            writeProtected.hmc = value;
            break;
        case PEX_SW_EEPROM:
            writeProtected.pex = value;
            break;
        case CX8_SPI_FLASH:
            writeProtected.cx8 = value;
            break;
        case RETIMER_EEPROM:
            writeProtected.retimer = value;
            writeProtected.retimer1 = value;
            writeProtected.retimer2 = value;
            writeProtected.retimer3 = value;
            writeProtected.retimer4 = value;
            writeProtected.retimer5 = value;
            writeProtected.retimer6 = value;
            writeProtected.retimer7 = value;
            writeProtected.retimer8 = value;
            break;
        case NVSW_EEPROM_BOTH:
            writeProtected.nvSwitch = value;
            writeProtected.nvSwitch1 = value;
            writeProtected.nvSwitch2 = value;
            break;
        case NVSW_EEPROM_1:
            writeProtected.nvSwitch1 = value;
            writeProtected.nvSwitch = writeProtected.nvSwitch1 |
                                      writeProtected.nvSwitch2;
            break;
        case NVSW_EEPROM_2:
            writeProtected.nvSwitch2 = value;
            writeProtected.nvSwitch = writeProtected.nvSwitch1 |
                                      writeProtected.nvSwitch2;
            break;
        case GPU_1_4_SPI_FLASH:
            writeProtected.gpu1_4 = value;
            writeProtected.gpu1 = value;
            writeProtected.gpu2 = value;
            writeProtected.gpu3 = value;
            writeProtected.gpu4 = value;
            break;
        case GPU_5_8_SPI_FLASH:
            writeProtected.gpu5_8 = value;
            writeProtected.gpu5 = value;
            writeProtected.gpu6 = value;
            writeProtected.gpu7 = value;
            writeProtected.gpu8 = value;
            break;
        case GPU_SPI_FLASH:
            writeProtected.gpu1_4 = value;
            writeProtected.gpu5_8 = value;
            writeProtected.gpu9_12 = value;
            writeProtected.gpu13_16 = value;
            writeProtected.gpu1 = value;
            writeProtected.gpu2 = value;
            writeProtected.gpu3 = value;
            writeProtected.gpu4 = value;
            writeProtected.gpu5 = value;
            writeProtected.gpu6 = value;
            writeProtected.gpu7 = value;
            writeProtected.gpu8 = value;
            writeProtected.gpu9 = value;
            writeProtected.gpu10 = value;
            writeProtected.gpu11 = value;
            writeProtected.gpu12 = value;
            writeProtected.gpu13 = value;
            writeProtected.gpu14 = value;
            writeProtected.gpu15 = value;
            writeProtected.gpu16 = value;
            break;
        case GPU_SPI_FLASH_1:
            writeProtected.gpu1 = value;
            writeProtected.gpu1_4 = writeProtected.gpu1 | writeProtected.gpu2 |
                                    writeProtected.gpu3 | writeProtected.gpu4;
            break;
        case GPU_SPI_FLASH_2:
            writeProtected.gpu2 = value;
            writeProtected.gpu1_4 = writeProtected.gpu1 | writeProtected.gpu2 |
                                    writeProtected.gpu3 | writeProtected.gpu4;
            break;
        case GPU_SPI_FLASH_3:
            writeProtected.gpu3 = value;
            writeProtected.gpu1_4 = writeProtected.gpu1 | writeProtected.gpu2 |
                                    writeProtected.gpu3 | writeProtected.gpu4;
            break;
        case GPU_SPI_FLASH_4:
            writeProtected.gpu4 = value;
            writeProtected.gpu1_4 = writeProtected.gpu1 | writeProtected.gpu2 |
                                    writeProtected.gpu3 | writeProtected.gpu4;
            break;
        case GPU_SPI_FLASH_5:
            writeProtected.gpu5 = value;
            writeProtected.gpu5_8 = writeProtected.gpu5 | writeProtected.gpu6 |
                                    writeProtected.gpu7 | writeProtected.gpu8;
            break;
        case GPU_SPI_FLASH_6:
            writeProtected.gpu6 = value;
            writeProtected.gpu5_8 = writeProtected.gpu5 | writeProtected.gpu6 |
                                    writeProtected.gpu7 | writeProtected.gpu8;
            break;
        case GPU_SPI_FLASH_7:
            writeProtected.gpu7 = value;
            writeProtected.gpu5_8 = writeProtected.gpu5 | writeProtected.gpu6 |
                                    writeProtected.gpu7 | writeProtected.gpu8;
            break;
        case GPU_SPI_FLASH_8:
            writeProtected.gpu8 = value;
            writeProtected.gpu5_8 = writeProtected.gpu5 | writeProtected.gpu6 |
                                    writeProtected.gpu7 | writeProtected.gpu8;
            break;
        case RETIMER_EEPROM_1:
            writeProtected.retimer1 = value;
            writeProtected.retimer =
                writeProtected.retimer1 | writeProtected.retimer2 |
                writeProtected.retimer3 | writeProtected.retimer4 |
                writeProtected.retimer5 | writeProtected.retimer6 |
                writeProtected.retimer7 | writeProtected.retimer8;
            break;
        case RETIMER_EEPROM_2:
            writeProtected.retimer2 = value;
            writeProtected.retimer =
                writeProtected.retimer1 | writeProtected.retimer2 |
                writeProtected.retimer3 | writeProtected.retimer4 |
                writeProtected.retimer5 | writeProtected.retimer6 |
                writeProtected.retimer7 | writeProtected.retimer8;
            break;
        case RETIMER_EEPROM_3:
            writeProtected.retimer3 = value;
            writeProtected.retimer =
                writeProtected.retimer1 | writeProtected.retimer2 |
                writeProtected.retimer3 | writeProtected.retimer4 |
                writeProtected.retimer5 | writeProtected.retimer6 |
                writeProtected.retimer7 | writeProtected.retimer8;
            break;
        case RETIMER_EEPROM_4:
            writeProtected.retimer4 = value;
            writeProtected.retimer =
                writeProtected.retimer1 | writeProtected.retimer2 |
                writeProtected.retimer3 | writeProtected.retimer4 |
                writeProtected.retimer5 | writeProtected.retimer6 |
                writeProtected.retimer7 | writeProtected.retimer8;
            break;
        case RETIMER_EEPROM_5:
            writeProtected.retimer5 = value;
            writeProtected.retimer =
                writeProtected.retimer1 | writeProtected.retimer2 |
                writeProtected.retimer3 | writeProtected.retimer4 |
                writeProtected.retimer5 | writeProtected.retimer6 |
                writeProtected.retimer7 | writeProtected.retimer8;
            break;
        case RETIMER_EEPROM_6:
            writeProtected.retimer6 = value;
            writeProtected.retimer =
                writeProtected.retimer1 | writeProtected.retimer2 |
                writeProtected.retimer3 | writeProtected.retimer4 |
                writeProtected.retimer5 | writeProtected.retimer6 |
                writeProtected.retimer7 | writeProtected.retimer8;
            break;
        case RETIMER_EEPROM_7:
            writeProtected.retimer7 = value;
            writeProtected.retimer =
                writeProtected.retimer1 | writeProtected.retimer2 |
                writeProtected.retimer3 | writeProtected.retimer4 |
                writeProtected.retimer5 | writeProtected.retimer6 |
                writeProtected.retimer7 | writeProtected.retimer8;
            break;
        case RETIMER_EEPROM_8:
            writeProtected.retimer8 = value;
            writeProtected.retimer =
                writeProtected.retimer1 | writeProtected.retimer2 |
                writeProtected.retimer3 | writeProtected.retimer4 |
                writeProtected.retimer5 | writeProtected.retimer6 |
                writeProtected.retimer7 | writeProtected.retimer8;
            break;
        case CPU_SPI_FLASH_1:
            writeProtected.cpu1 = value;
            writeProtected.cpu1_4 = writeProtected.cpu1 | writeProtected.cpu2 |
                                    writeProtected.cpu3 | writeProtected.cpu4;
            break;
        case CPU_SPI_FLASH_2:
            writeProtected.cpu2 = value;
            writeProtected.cpu1_4 = writeProtected.cpu1 | writeProtected.cpu2 |
                                    writeProtected.cpu3 | writeProtected.cpu4;
            break;
        case CPU_SPI_FLASH_3:
            writeProtected.cpu3 = value;
            writeProtected.cpu1_4 = writeProtected.cpu1 | writeProtected.cpu2 |
                                    writeProtected.cpu3 | writeProtected.cpu4;
            break;
        case CPU_SPI_FLASH_4:
            writeProtected.cpu4 = value;
            writeProtected.cpu1_4 = writeProtected.cpu1 | writeProtected.cpu2 |
                                    writeProtected.cpu3 | writeProtected.cpu4;
            break;
        default:
            lg2::error(
                "enableDisableWriteProtectedHandler: Invalid Data Index");
            break;
    }
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_enable_disable_wp_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                       reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "enableDisableWriteProtectedHandler: encode_enable_disable_wp_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getDeviceDiagnosticsHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    uint8_t handle = 0;
    auto rc = decode_get_device_diagnostics_req(requestMsg, requestLen,
                                                &handle);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_device_diagnostics_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    if (verbose)
    {
        lg2::info("getDeviceDiagnosticsHandler: request length={LEN}, "
                  "handle={HANDLE}",
                  "LEN", requestLen, "HANDLE", handle);
    }
    std::string segmentData = "Hello World, this is device diagnostics info";

    Response response(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_device_diagnostics_resp) - 1 +
                          segmentData.size(),
                      0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reasonCode = ERR_NULL;
    uint8_t nextHandle =
        handle == 0 ? 1 : 0xFF; // mockup responder will send only 2 segments
    rc = encode_get_device_diagnostics_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reasonCode,
        (uint8_t*)segmentData.data(), segmentData.size(), nextHandle,
        responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_device_diagnostics_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::getNetworkDeviceDebugInfoHandler(const nsm_msg* requestMsg,
                                                      size_t requestLen)
{
    uint8_t debugType = 0;
    uint32_t handle = 0;
    auto rc = decode_get_network_device_debug_info_req(requestMsg, requestLen,
                                                       &debugType, &handle);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_network_device_debug_info_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    if (verbose)
    {
        lg2::info("getNetworkDeviceDebugInfoHandler: request length={LEN}, "
                  "debugType={DEBUG_TYPE}, handle={HANDLE}",
                  "LEN", requestLen, "DEBUG_TYPE", debugType, "HANDLE", handle);
    }

    std::string segmentData = "Hello World, this is device debug info data.";
    uint16_t reasonCode = ERR_NULL;
    uint32_t nextHandle =
        handle == 0 ? 1 : 0; // mockup responder will send only 2 segments

    Response response(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_network_device_debug_info_resp) - 1 +
                          segmentData.size(),
                      0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_network_device_debug_info_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reasonCode,
        (uint8_t*)segmentData.data(), segmentData.size(), nextHandle,
        responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_network_device_debug_info_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::eraseTraceHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    if (verbose)
    {
        lg2::info("eraseTraceHandler: request length={LEN}", "LEN", requestLen);
    }

    auto rc = decode_erase_trace_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_erase_trace_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    uint16_t reason_code = ERR_NULL;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_erase_trace_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_erase_trace_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                 reason_code, ERASE_TRACE_DATA_ERASED,
                                 responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_erase_trace_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getNetworkDeviceLogInfoHandler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    uint32_t handle = 0;
    auto rc = decode_get_network_device_log_info_req(requestMsg, requestLen,
                                                     &handle);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_network_device_log_info_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    if (verbose)
    {
        lg2::info("getNetworkDeviceLogInfoHandler: request length={LEN}, "
                  "handle={HANDLE}",
                  "LEN", requestLen, "HANDLE", handle);
    }

    // this is some dummy data segment with random size
    // It says "Hello World, this is device log info data."
    std::string logData = "Hello World, this is device log info data.";
    uint16_t reasonCode = ERR_NULL;
    uint32_t nextHandle = 0; // mockup responder will send only 1 log info
    nsm_device_log_info_breakdown logInfo;
    logInfo.lost_events = 02;
    logInfo.unused = 00;
    logInfo.synced_time = 00;
    logInfo.reserved1 = 00;
    logInfo.reserved2 = 00;
    logInfo.time_high = 100;
    logInfo.time_low = 200;
    logInfo.entry_prefix = 33;
    logInfo.length = (logData.size() / 4);
    logInfo.entry_suffix = 444;

    Response response(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_network_device_log_info_resp) - 1 +
                          logData.size(),
                      0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_network_device_log_info_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reasonCode, nextHandle,
        logInfo, (uint8_t*)logData.data(), logData.size(), responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_network_device_log_info_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::eraseDebugInfoHandler(const nsm_msg* requestMsg,
                                           size_t requestLen)
{
    if (verbose)
    {
        lg2::info("eraseDebugInfoHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t info_type;
    auto rc = decode_erase_debug_info_req(requestMsg, requestLen, &info_type);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_erase_debug_info_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    uint16_t reason_code = ERR_NULL;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_erase_debug_info_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                      reason_code, ERASE_TRACE_DATA_ERASED,
                                      responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_erase_debug_info_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPciePortConfigHandler(const nsm_msg* requestMsg,
                                              size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPciePortConfigHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t port_number;
    uint8_t port_type;
    uint8_t port_index;

    auto rc = decode_get_pcie_port_config_req(
        requestMsg, requestLen, &port_number, &port_type, &port_index);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_pcie_port_config_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    // Initialize response vector
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    response.reserve(256);

    uint16_t samplesCount{};

    // Iterate through mock reset metric data
    for (const auto& [tag, mockValue] : pciePortConfigMockTable)
    {
        ++samplesCount;

        uint8_t reading[64]{};
        size_t sample_len{};
        std::array<uint8_t, 256> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        if (tag <= 3) // PCIe_Gen_Preset tags (0-3)
        {
            rc = encode_preset_PCIe_data(static_cast<uint8_t>(mockValue),
                                         reading, &sample_len);
        }
        else if (tag == 4) // PCIe_Tx_Amplitude tag
        {
            rc = encode_PCIe_TxAmplitude_data(static_cast<uint8_t>(mockValue),
                                              reading, &sample_len);
        }
        else
        {
            lg2::error("Unexpected tag value: {TAG}", "TAG", tag);
            continue;
        }
        // Encode the sample into the response
        rc = encode_aggregate_resp_sample(tag, true, reading, sample_len,
                                          nsm_sample, &sample_len);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("encode_aggregate_resp_sample failed: rc={RC}", "RC",
                       rc);
        }
        assert(rc == NSM_SW_SUCCESS);

        // Add the sample to the response vector
        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), sample_len));
    }

    // Finalize the aggregate response
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_get_port_config_aggregate_resp(
        requestMsg->hdr.instance_id, NSM_GET_PORT_CONFIGURATION, NSM_SUCCESS,
        samplesCount, responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setPciePortConfigHandler(const nsm_msg* requestMsg,
                                              size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setPciePortConfigHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t port_number;
    uint8_t port_type;
    uint8_t port_index;
    uint16_t sample_count;
    const uint8_t* sample_data;
    size_t sample_data_len;

    auto rc = decode_set_port_config_aggregate_req(
        requestMsg, requestLen, &port_number, &port_type, &port_index,
        &sample_count, &sample_data, &sample_data_len);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_port_config_aggregate_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    size_t consumed_len{};
    // Decode sample data and set to mock values
    while (sample_count--)
    {
        uint8_t tag;
        bool valid;
        const uint8_t* data;
        size_t data_len;

        sample_data += consumed_len;
        sample_data_len -= consumed_len;
        auto sample =
            reinterpret_cast<const nsm_aggregate_resp_sample*>(sample_data);

        rc = decode_aggregate_resp_sample(sample, sample_data_len,
                                          &consumed_len, &tag, &valid, &data,
                                          &data_len);

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("decode_aggregate_resp_sample failed: rc={RC}", "RC",
                       rc);
        }
        assert(rc == NSM_SW_SUCCESS);
        if (valid)
        {
            if (tag <= 3) // PCIe_Gen_Preset tags (0-3)
            {
                uint8_t preset_0 = 0;
                uint8_t preset_1 = 0;
                rc = decode_preset_PCIe_data(data, data_len, &preset_0,
                                             &preset_1);
                if (rc != NSM_SW_SUCCESS)
                {
                    lg2::error("decode_preset_PCIe_data failed: rc={RC}", "RC",
                               rc);
                }
                lg2::info(
                    "decode_preset_PCIe_data: tag={TAG}, preset_0={PRESET_0}, preset_1={PRESET_1}",
                    "TAG", tag, "PRESET_0", preset_0, "PRESET_1", preset_1);
                assert(rc == NSM_SW_SUCCESS);
                pciePortConfigMockTable[tag] = preset_0 | (preset_1 << 4);
            }
            else if (tag == 4) // PCIe_Tx_Amplitude tag
            {
                uint8_t tx_amplitude = 0;
                rc = decode_PCIe_TxAmplitude_data(data, data_len,
                                                  &tx_amplitude);
                if (rc != NSM_SW_SUCCESS)
                {
                    lg2::error("decode_PCIe_TxAmplitude_data failed: rc={RC}",
                               "RC", rc);
                }
                lg2::info(
                    "decode_PCIe_TxAmplitude_data: tag={TAG}, tx_amplitude={TX_AMPLITUDE}",
                    "TAG", tag, "TX_AMPLITUDE", tx_amplitude);
                assert(rc == NSM_SW_SUCCESS);
                pciePortConfigMockTable[tag] = tx_amplitude;
            }
            else
            {
                lg2::error("Unexpected tag value: {TAG}", "TAG", tag);
                continue;
            }
        }
    }

    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_port_config_aggregate_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryAggregatedResetMetrics(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    if (verbose)
    {
        lg2::info("queryResetMetrics: request length={LEN}", "LEN", requestLen);
    }

    // Decode the request message (assuming decode function exists)
    auto rc = decode_get_device_reset_statistics_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);

    // Initialize response vector
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    response.reserve(256);

    uint16_t samplesCount{};

    // Iterate through mock reset metric data
    for (const auto& [tag, mockValue] : resetMetricsMockTable)
    {
        ++samplesCount;

        uint8_t reading[64]{};
        size_t sample_len{};
        std::array<uint8_t, 256> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        if (tag == 7) // Special case for "LastResetType" (enum8)
        {
            rc = encode_reset_enum_data(static_cast<uint8_t>(mockValue),
                                        reading, &sample_len);
        }
        else if (tag == 8) // Special case for boot reason (256 bytes)
        {
            rc = encode_reset_count_256data(bootReasonMockValue.data(), reading,
                                            &sample_len);
        }
        else // General case for reset counts (uint16_t)
        {
            rc = encode_reset_count_data(static_cast<uint16_t>(mockValue),
                                         reading, &sample_len);
        }
        assert(rc == NSM_SW_SUCCESS);

        // Encode the sample into the response
        rc = encode_aggregate_resp_sample(tag, true, reading, sample_len,
                                          nsm_sample, &sample_len);
        assert(rc == NSM_SW_SUCCESS);

        // Add the sample to the response vector
        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), sample_len));
    }

    // Finalize the aggregate response
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_aggregate_resp(requestMsg->hdr.instance_id,
                               NSM_GET_DEVICE_RESET_STATISTICS, NSM_SUCCESS,
                               samplesCount, responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getSupportedGPMMetrics(const nsm_msg* requestMsg,
                                            size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getSupportedGPMMetrics: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t metricType{};
    auto rc = decode_get_supported_gpm_metrics_req(requestMsg, requestLen,
                                                   &metricType);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getSupportedGPMMetrics: decode_get_supported_gpm_metrics_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("getSupportedGPMMetrics: metricType={METRIC_TYPE}",
                  "METRIC_TYPE", metricType);
    }

    // Mock response: support all 32 metrics (metric IDs 0-31)
    // 4 bytes = 32 bits, all set to 1
    constexpr uint16_t maskSize = 4;
    constexpr uint16_t maxMetricsPerCommand = 21;
    std::array<uint8_t, maskSize> supportedMetricsBitmask = {0xFF, 0xFF, 0xFF,
                                                             0xFF};

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_gpm_metrics_resp) - 1 +
            maskSize,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_supported_gpm_metrics_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, 0, maskSize,
        maxMetricsPerCommand, supportedMetricsBitmask.data(), responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getSupportedGPMMetrics: encode_get_supported_gpm_metrics_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryAggregatedGPMMetrics(const nsm_msg* requestMsg,
                                               size_t requestLen)
{
    auto request = reinterpret_cast<const nsm_query_aggregate_gpm_metrics_req*>(
        requestMsg->payload);
    if (verbose)
    {
        lg2::info("queryAggregatedGPMMetrics: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t retrieval_source;
    uint8_t gpu_instance;
    uint8_t compute_instance;
    const uint8_t* metrics_bitfield;
    size_t metrics_bitfield_length;

    auto rc = decode_query_aggregate_gpm_metrics_req(
        requestMsg, requestLen, &retrieval_source, &gpu_instance,
        &compute_instance, &metrics_bitfield, &metrics_bitfield_length);

    assert(rc == NSM_SW_SUCCESS);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    response.reserve(256);

    uint16_t samplesCount{};

    for (size_t i{}; i < metrics_bitfield_length; ++i)
    {
        for (uint8_t j{}; j < 8; ++j)
        {
            if (metrics_bitfield[i] & 1 << j)
            {
                const uint8_t metric_id = 8 * i + j;
                const auto info = metricsTable.find(metric_id);
                if (info == metricsTable.end())
                {
                    continue;
                }

                ++samplesCount;

                uint8_t reading[64]{};
                size_t sample_len{};
                std::array<uint8_t, 256> sample;
                auto nsm_sample =
                    reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

                switch (info->second.unit)
                {
                    case GPMMetricsUnit::PERCENTAGE:
                        rc = encode_aggregate_gpm_metric_percentage_data(
                            info->second.mockValue, reading, &sample_len);
                        assert(rc == NSM_SW_SUCCESS);

                        break;

                    case GPMMetricsUnit::BANDWIDTH:
                        rc = encode_aggregate_gpm_metric_bandwidth_data(
                            info->second.mockValue, reading, &sample_len);
                        assert(rc == NSM_SW_SUCCESS);

                        break;
                }

                rc = encode_aggregate_resp_sample(metric_id, true, reading,
                                                  sample_len, nsm_sample,
                                                  &sample_len);
                assert(rc == NSM_SW_SUCCESS);

                response.insert(response.end(), sample.begin(),
                                std::next(sample.begin(), sample_len));
            }
        }
    }

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_aggregate_resp(requestMsg->hdr.instance_id,
                               request->hdr.command, NSM_SUCCESS, samplesCount,
                               responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryPerInstanceGPMMetrics(const nsm_msg* requestMsg,
                                                size_t requestLen)
{
    auto request =
        reinterpret_cast<const nsm_query_per_instance_gpm_metrics_req*>(
            requestMsg->payload);
    if (verbose)
    {
        lg2::info("queryPerInstanceGPMMetrics: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t retrieval_source;
    uint8_t gpu_instance;
    uint8_t compute_instance;
    uint8_t metric_id;
    uint32_t instance_bitfield;

    auto rc = decode_query_per_instance_gpm_metrics_req(
        requestMsg, requestLen, &retrieval_source, &gpu_instance,
        &compute_instance, &metric_id, &instance_bitfield);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    response.reserve(256);

    uint16_t samplesCount{};

    const auto info = metricsTable.find(metric_id);
    if (info == metricsTable.end())
    {
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        rc = encode_cc_only_resp(requestMsg->hdr.instance_id,
                                 requestMsg->hdr.nvidia_msg_type,
                                 request->hdr.command, NSM_ERR_INVALID_DATA,
                                 ERR_NOT_SUPPORTED, responseMsg);
        assert(rc == NSM_SW_SUCCESS);

        return response;
    }

    for (size_t i{}; i < 32; ++i)
    {
        if (instance_bitfield & 1 << i)
        {
            ++samplesCount;

            uint8_t reading[64]{};
            size_t consumed_len{};
            std::array<uint8_t, 256> sample;
            auto nsm_sample =
                reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

            switch (info->second.unit)
            {
                case GPMMetricsUnit::PERCENTAGE:
                {
                    auto val = info->second.mockValue * (i + 1);
                    val -= 100 * (static_cast<int>(val) / 100);
                    rc = encode_aggregate_gpm_metric_percentage_data(
                        val, reading, &consumed_len);
                    assert(rc == NSM_SW_SUCCESS);

                    break;
                }

                case GPMMetricsUnit::BANDWIDTH:
                {
                    // To obtain a unique value for each instance
                    // Metric, base Metric value is multiplied by
                    // instance number and then the module of base 10 is
                    // taken on that number.
                    constexpr uint64_t mod = 10 * 1024 * 1024 * 128;
                    auto val = info->second.mockValue * (i + 1);
                    val -= mod * (static_cast<uint64_t>(val) / mod);
                    rc = encode_aggregate_gpm_metric_bandwidth_data(
                        val, reading, &consumed_len);
                    assert(rc == NSM_SW_SUCCESS);

                    break;
                }
            }

            rc = encode_aggregate_resp_sample(i, true, reading, consumed_len,
                                              nsm_sample, &consumed_len);
            assert(rc == NSM_SW_SUCCESS);

            response.insert(response.end(), sample.begin(),
                            std::next(sample.begin(), consumed_len));
        }
    }

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_aggregate_resp(requestMsg->hdr.instance_id,
                               request->hdr.command, NSM_SUCCESS, samplesCount,
                               responseMsg);

    assert(rc == NSM_SW_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryPerInstanceGPMMetricsV2(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    auto request =
        reinterpret_cast<const nsm_query_per_instance_gpm_metrics_v2_req*>(
            requestMsg->payload);
    if (verbose)
    {
        lg2::info("queryPerInstanceGPMMetricsV2: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t retrieval_source;
    uint8_t gpu_instance;
    uint8_t compute_instance;
    uint8_t metric_id;
    bitfield8_t* instance_bitfield;
    size_t instance_bitfield_length;

    auto rc = decode_query_per_instance_gpm_metrics_v2_req(
        requestMsg, requestLen, &retrieval_source, &gpu_instance,
        &compute_instance, &metric_id, &instance_bitfield,
        &instance_bitfield_length);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "queryPerInstanceGPMMetricsV2: decode_query_per_instance_gpm_metrics_v2_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    response.reserve(256);

    uint16_t samplesCount{};

    const auto info = metricsTable.find(metric_id);
    if (info == metricsTable.end())
    {
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        rc = encode_cc_only_resp(requestMsg->hdr.instance_id,
                                 requestMsg->hdr.nvidia_msg_type,
                                 request->hdr.command, NSM_ERR_INVALID_DATA,
                                 ERR_NOT_SUPPORTED, responseMsg);
        assert(rc == NSM_SW_SUCCESS);

        return response;
    }

    for (size_t i{}; i < instance_bitfield_length; ++i)
    {
        auto byte = instance_bitfield[i].byte;
        for (size_t j{}; j < 8; ++j)
        {
            if (byte & 1 << j)
            {
                ++samplesCount;

                uint8_t reading[64]{};
                size_t consumed_len{};
                std::array<uint8_t, 256> sample;
                auto nsm_sample =
                    reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

                switch (info->second.unit)
                {
                    case GPMMetricsUnit::PERCENTAGE:
                    {
                        auto val = info->second.mockValue * (i * 8 + j + 1);
                        val -= 100 * (static_cast<int>(val) / 100);
                        rc = encode_aggregate_gpm_metric_percentage_data(
                            val, reading, &consumed_len);
                        assert(rc == NSM_SW_SUCCESS);

                        break;
                    }

                    case GPMMetricsUnit::BANDWIDTH:
                    {
                        // To obtain a unique value for each instance
                        // Metric, base Metric value is multiplied by
                        // instance number and then the module of base 10 is
                        // taken on that number.
                        constexpr uint64_t mod = 10 * 1024 * 1024 * 128;
                        auto val = info->second.mockValue * (i * 8 + j + 1);
                        val -= mod * (static_cast<uint64_t>(val) / mod);
                        rc = encode_aggregate_gpm_metric_bandwidth_data(
                            val, reading, &consumed_len);
                        assert(rc == NSM_SW_SUCCESS);

                        break;
                    }
                }

                rc = encode_aggregate_resp_sample(i * 8 + j, true, reading,
                                                  consumed_len, nsm_sample,
                                                  &consumed_len);
                assert(rc == NSM_SW_SUCCESS);

                response.insert(response.end(), sample.begin(),
                                std::next(sample.begin(), consumed_len));
            }
        }
    }

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_aggregate_resp(requestMsg->hdr.instance_id,
                               request->hdr.command, NSM_SUCCESS, samplesCount,
                               responseMsg);

    assert(rc == NSM_SW_SUCCESS);

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::enableDisableGpuIstModeHandler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    if (verbose)
    {
        lg2::info("enableDisableGpuIstModeHandler: request length={LEN}", "LEN",
                  requestLen);
    }
    uint8_t device_index;
    uint8_t value = 0;
    [[maybe_unused]] auto rc = decode_enable_disable_gpu_ist_mode_req(
        requestMsg, requestLen, &device_index, &value);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "enableDisableGpuIstModeHandler: decode_enable_disable_gpu_ist_mode_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    auto& istMode = state.istMode;
    if (device_index < 8)
    {
        istMode = value ? (istMode | (1 << device_index))
                        : (istMode & (0 << device_index));
    }
    else if (device_index == ALL_GPUS_DEVICE_INDEX)
    {
        istMode = value ? 0b11111111 : 0b00000000;
    }
    else
    {
        lg2::error("enableDisableGpuIstModeHandler: Invalid Device Index");
        return std::nullopt;
    }
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_enable_disable_gpu_ist_mode_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "enableDisableGpuIstModeHandler: encode_enable_disable_gpu_ist_mode_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::getReconfigurationPermissionsV1Handler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info(
            "getReconfigurationPermissionsV1Handler: request length={LEN}",
            "LEN", requestLen);
    }
    reconfiguration_permissions_v1_index settingsIndex;
    [[maybe_unused]] auto rc = decode_get_reconfiguration_permissions_v1_req(
        requestMsg, requestLen, &settingsIndex);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getReconfigurationPermissionsV1Handler: decode_get_reconfiguration_permissions_v1_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    if (settingsIndex > RP_INFOROM_RECREATE_ALLOW_INB)
    {
        lg2::error(
            "getReconfigurationPermissionsV1Handler: Invalid Settings Index");
        return std::nullopt;
    }
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_reconfiguration_permissions_v1_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reasonCode = ERR_NULL;
    rc = encode_get_reconfiguration_permissions_v1_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reasonCode,
        &state.prcKnobs[settingsIndex], responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getReconfigurationPermissionsV1Handler: encode_get_reconfiguration_permissions_v1_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setReconfigurationPermissionsV1Handler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info(
            "setReconfigurationPermissionsV1Handler: request length={LEN}",
            "LEN", requestLen);
    }
    reconfiguration_permissions_v1_index settingsIndex;
    reconfiguration_permissions_v1_setting configuration;
    uint8_t permission;
    [[maybe_unused]] auto rc = decode_set_reconfiguration_permissions_v1_req(
        requestMsg, requestLen, &settingsIndex, &configuration, &permission);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "setReconfigurationPermissionsV1Handler: decode_set_reconfiguration_permissions_v1_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    if (settingsIndex > RP_INFOROM_RECREATE_ALLOW_INB)
    {
        lg2::error(
            "setReconfigurationPermissionsV1Handler: Invalid Settings Index");
        return std::nullopt;
    }
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reasonCode = ERR_NULL;
    rc = encode_set_reconfiguration_permissions_v1_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reasonCode, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "enableDisableGpuIstModeHandler: encode_set_reconfiguration_permissions_v1_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    switch (configuration)
    {
        case RP_ONESHOOT_HOT_RESET:
            if (permission & 1)
            {
                state.prcKnobs[settingsIndex].host_oneshot = 1;
            }
            if ((permission >> 1) & 1)
            {
                state.prcKnobs[settingsIndex].DOE_oneshot = 1;
            }
            break;
        case RP_PERSISTENT:
            if (permission & 1)
            {
                state.prcKnobs[settingsIndex].host_persistent = 1;
            }
            if ((permission >> 1) & 1)
            {
                state.prcKnobs[settingsIndex].DOE_persistent = 1;
            }
            break;
        case RP_ONESHOT_FLR:
            if (permission & 1)
            {
                state.prcKnobs[settingsIndex].host_flr_persistent = 1;
            }
            if ((permission >> 1) & 1)
            {
                state.prcKnobs[settingsIndex].DOE_flr_persistent = 1;
            }
            break;
        default:
            lg2::error(
                "setReconfigurationPermissionsV1Handler: invalid configuration: configuration={CONF}",
                "CONF", int(configuration));
            return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getConfidentialComputeModeHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    auto rc = decode_get_confidential_compute_mode_v1_req(requestMsg,
                                                          requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode request for getConfidentialComputeModeHandler failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    uint8_t current_mode = 2;
    uint8_t pending_mode = 2;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_confidential_compute_mode_v1_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_confidential_compute_mode_v1_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, current_mode,
        pending_mode, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error(
            "encode_get_confidential_compute_mode_v1_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setConfidentialComputeModeHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    uint8_t mode;
    auto rc = decode_set_confidential_compute_mode_v1_req(requestMsg,
                                                          requestLen, &mode);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode request for setConfidentialComputeModeHandler failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_confidential_compute_mode_v1_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error(
            "encode_set_confidential_compute_mode_v1_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setErrorInjectionModeV1Handler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setErrorInjectionModeV1Handler: request length={LEN}", "LEN",
                  requestLen);
    }
    uint8_t mode = 0;
    auto rc = decode_set_error_injection_mode_v1_req(requestMsg, requestLen,
                                                     &mode);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "setErrorInjectionModeV1Handler: decode_set_error_injection_mode_v1_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    state.errorInjectionMode.mode = mode;
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_set_error_injection_mode_v1_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, responseMsg);
    return response;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::getErrorInjectionModeV1Handler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getErrorInjectionModeV1Handler: request length={LEN}", "LEN",
                  requestLen);
    }
    auto rc = decode_get_error_injection_mode_v1_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getErrorInjectionModeV1Handler: decode_get_error_injection_mode_v1_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    Response response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_mode_v1_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_get_error_injection_mode_v1_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL,
        &state.errorInjectionMode, responseMsg);
    return response;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::getSupportedErrorInjectionTypesV1Handler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info(
            "getSupportedErrorInjectionTypesV1Handler: request length={LEN}",
            "LEN", requestLen);
    }
    auto rc = decode_get_error_injection_types_v1_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getSupportedErrorInjectionTypesV1Handler: decode_get_error_injection_types_v1_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    Response response(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_error_injection_types_mask_resp),
                      0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_error_injection_types_mask supportedTypes = {0, 0, 0, 0, 0, 0, 0, 0};
    auto errorInjectionIt = state.errorInjection.find(mockDeviceType);
    if (errorInjectionIt != state.errorInjection.end())
    {
        for (const auto& [type, _] : errorInjectionIt->second)
        {
            supportedTypes.mask[type / 8] |= (1 << (type % 8));
        }
    }
    rc = encode_get_supported_error_injection_types_v1_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, &supportedTypes,
        responseMsg);
    return response;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::setCurrentErrorInjectionTypesV1Handler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info(
            "setCurrentdErrorInjectionTypesV1Handler: request length={LEN}",
            "LEN", requestLen);
    }
    nsm_error_injection_types_mask data;
    auto rc = decode_set_current_error_injection_types_v1_req(
        requestMsg, requestLen, &data);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "setCurrentErrorInjectionTypesV1Handler: decode_set_error_injection_types_v1_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto errorInjectionIt = state.errorInjection.find(mockDeviceType);
    if (errorInjectionIt != state.errorInjection.end())
    {
        bool error = false;
        auto& errorInjectionTypes = errorInjectionIt->second;
        // check for errors;
        for (size_t i = 0; i < 64 && !error; i++)
        {
            bool enabled = (data.mask[i / 8] >> (i % 8)) & 0x01;
            // set error if bit is enabled, but not supported
            error = enabled &&
                    errorInjectionTypes.find(error_injection_type(i)) ==
                        errorInjectionTypes.end();
        }
        if (error)
        {
            response.resize(sizeof(nsm_msg_hdr) +
                            sizeof(nsm_common_non_success_resp));
            rc = encode_common_resp(
                requestMsg->hdr.instance_id, NSM_ERR_INVALID_DATA, ERR_NULL,
                NSM_TYPE_DEVICE_CONFIGURATION,
                NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1, responseMsg);
            return response;
        }
        else
        {
            for (auto& [type, enabled] : errorInjectionTypes)
            {
                enabled = (data.mask[type / 8] >> (type % 8)) & 0x01;
            }
        }
    }
    rc = encode_set_current_error_injection_types_v1_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, responseMsg);
    return response;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::getCurrentErrorInjectionTypesV1Handler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info(
            "getCurrentdErrorInjectionTypesV1Handler: request length={LEN}",
            "LEN", requestLen);
    }
    auto rc = decode_get_error_injection_types_v1_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getCurrentErrorInjectionTypesV1Handler: decode_get_error_injection_types_v1_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    Response response(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_error_injection_types_mask_resp),
                      0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_error_injection_types_mask enabledTypes = {0, 0, 0, 0, 0, 0, 0, 0};
    auto errorInjectionIt = state.errorInjection.find(mockDeviceType);
    if (errorInjectionIt != state.errorInjection.end())
    {
        for (const auto& [type, enabled] : errorInjectionIt->second)
        {
            enabledTypes.mask[type / 8] |= (uint8_t(enabled) << (type % 8));
        }
    }
    rc = encode_get_current_error_injection_types_v1_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, &enabledTypes,
        responseMsg);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getErrorInjectionPayloadHandler(const nsm_msg* requestMsg,
                                                     size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getErrorInjectionPayloadHandler: request length={LEN}",
                  "LEN", requestLen);
    }
    uint16_t error_injection_type = 4;
    uint16_t error_injection_subtype = 0;
    auto rc = decode_get_error_injection_payload_req(requestMsg, requestLen,
                                                     &error_injection_type,
                                                     &error_injection_subtype);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getErrorInjectionPayloadHandler: decode_get_error_injection_payload_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    Response response(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_error_injection_payload_resp) -
                          sizeof(uint8_t) + state.errorInjectionPayload.size(),
                      0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_get_error_injection_payload_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL,
        error_injection_type, error_injection_subtype,
        state.errorInjectionPayload.data(), state.errorInjectionPayload.size(),
        responseMsg);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setErrorInjectionPayloadHandler(const nsm_msg* requestMsg,
                                                     size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setErrorInjectionPayloadHandler: request length={LEN}",
                  "LEN", requestLen);
    }
    uint16_t errorInjectionType = 0;
    uint16_t errorInjectionSubtype = 0;
    size_t dataSize = 0;
    std::vector<uint8_t> data(256); // Pre-allocate buffer
    auto rc = decode_set_error_injection_payload_req(
        requestMsg, requestLen, &errorInjectionType, &errorInjectionSubtype,
        data.data(), &dataSize);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "setErrorInjectionPayloadHandler: decode_set_error_injection_payload_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    data.resize(dataSize);
    state.errorInjectionPayload = data;
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_set_error_injection_payload_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, responseMsg);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::activateErrorInjectionHandler(const nsm_msg* requestMsg,
                                                   size_t requestLen)
{
    uint16_t error_injection_type = 0;
    uint16_t error_injection_subtype = 0;
    auto rc = decode_activate_error_injection_payload_req(
        requestMsg, requestLen, &error_injection_type,
        &error_injection_subtype);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "activateErrorInjectionHandler: decode_activate_error_injection_payload_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_activate_error_injection_payload_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, responseMsg);
    return response;
}

std::optional<Response> MockupResponder::getViolationDurationHandler(
    const nsm_msg* requestMsg, size_t requestLen, bool isLongRunning,
    std::optional<Request>& longRunningEvent)
{
    auto rc = decode_get_violation_duration_req(requestMsg, requestLen);
    if (rc)
    {
        lg2::error("decode_get_violation_duration_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    if (pmDisabled())
    {
        return unsupportedResponse(requestMsg->hdr.instance_id,
                                   NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                   NSM_GET_VIOLATION_DURATION);
    }

    struct nsm_violation_duration data;
    data.supported_counter.byte = 255;
    data.hw_violation_duration = 2000000;
    data.global_sw_violation_duration = 3000000;
    data.power_violation_duration = 4000000;
    data.thermal_violation_duration = 5000000;
    data.counter4 = 6000000;
    data.counter5 = 7000000;
    data.counter6 = 8000000;
    data.counter7 = 9000000;

    Response response(
        isLongRunning
            ? /*common resp*/ (sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp))
            : /* regular resp */ (sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_violation_duration_resp)),
        0);

    if (isLongRunning)
    {
        auto commonMsg = reinterpret_cast<nsm_msg*>(response.data());
        rc = encode_common_resp(requestMsg->hdr.instance_id, NSM_ACCEPTED,
                                ERR_NULL, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_VIOLATION_DURATION, commonMsg);
        assert(rc == NSM_SW_SUCCESS);
        longRunningEvent = Response((sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                     sizeof(nsm_long_running_resp) +
                                     sizeof(nsm_violation_duration)),
                                    0);
        auto eventMsg = reinterpret_cast<nsm_msg*>(longRunningEvent->data());
        auto eventRc = encode_get_violation_duration_event_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, &data,
            eventMsg);
        assert(eventRc == NSM_SW_SUCCESS);
    }
    else
    {
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        rc = encode_get_violation_duration_resp(requestMsg->hdr.instance_id,
                                                NSM_SUCCESS, ERR_NULL, &data,
                                                responseMsg);
    }
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_violation_duration_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::resetNetworkDeviceHandler(const nsm_msg* requestMsg,
                                               size_t requestLen)
{
    if (verbose)
    {
        lg2::info("resetNetworkDeviceHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t mode = 0;
    auto rc = decode_reset_network_device_req(requestMsg, requestLen, &mode);
    if (rc != NSM_SW_SUCCESS)
    {
        if (verbose)
        {
            lg2::error("decode_reset_network_device_req failed: rc={RC}", "RC",
                       rc);
        }
        return std::nullopt;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    rc = encode_reset_network_device_resp(requestMsg->hdr.instance_id,
                                          reason_code, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_reset_network_device_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getEgmModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getEgmModeHandler: request length={LEN}", "LEN", requestLen);
    }
    auto rc = decode_common_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode request for getEgmModeHandler failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    bitfield8_t flags;
    flags.byte = 1;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_EGM_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_EGM_mode_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                  reason_code, &flags, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_EGM_mode_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setEgmModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    uint8_t requested_mode;
    auto rc = decode_set_EGM_mode_req(requestMsg, requestLen, &requested_mode);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_EGM_mode_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_EGM_mode_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                  reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_EGM_mode_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getHistogramFormatHandler(const nsm_msg* requestMsg,
                                               size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getHistogramFormatHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint32_t histogramId;
    uint16_t parameter;

    auto rc = decode_get_histogram_format_req(requestMsg, requestLen,
                                              &histogramId, &parameter);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_histogram_format_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    struct nsm_histogram_format_metadata metaData;
    uint16_t reason_code = ERR_NULL;
    std::vector<uint32_t> bucketOffsets = {00, 15, 30, 45, 60, 75};
    uint32_t totalBucketSize = bucketOffsets.size() * sizeof(bucketOffsets[0]);
    metaData.num_of_buckets = bucketOffsets.size();
    metaData.min_sampling_time = 16;
    metaData.accumulation_cycle = 12;
    metaData.increment_duration = 111;
    metaData.bucket_unit_of_measure = NSM_BUCKET_UNIT_WATTS;
    metaData.bucket_data_type = NvU32;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp) +
            sizeof(struct nsm_histogram_format_metadata) + totalBucketSize,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_histogram_format_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &metaData,
        reinterpret_cast<uint8_t*>(bucketOffsets.data()), totalBucketSize,
        responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_histogram_format_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getHistogramDataHandler(const nsm_msg* requestMsg,
                                             size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getHistogramDataHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint32_t histogramId;
    uint16_t parameter;

    auto rc = decode_get_histogram_data_req(requestMsg, requestLen,
                                            &histogramId, &parameter);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_histogram_data_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint32_t> bucketData = {11, 22, 33, 55, 66, 77};
    uint16_t numOfBuckets = bucketData.size();
    uint8_t bucketDataType = NvU32;
    uint32_t bucketDataSize = bucketData.size() * sizeof(bucketData[0]);
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      NSM_RESPONSE_CONVENTION_LEN +
                                      sizeof(numOfBuckets) + bucketDataSize + 1,
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_histogram_data_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, bucketDataType,
        numOfBuckets, reinterpret_cast<uint8_t*>(bucketData.data()),
        bucketDataSize, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_histogram_data_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getGpioStateHandler(const nsm_msg* requestMsg,
                                         size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getGpioStateHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint16_t offset;
    uint16_t length;

    auto rc = decode_get_gpio_state_req(requestMsg, requestLen, &offset,
                                        &length);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_gpio_state_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    // Calculate the number of bytes needed for the requested GPIO values
    // Each bit represents one GPIO state, so we need (length + 7) / 8 bytes
    uint32_t gpioValuesSize = (length + 7) / 8;
    std::vector<uint8_t> gpioValues(gpioValuesSize, 0);

    // Mock data: Generate some sample GPIO states
    for (uint32_t i = 0; i < gpioValuesSize; ++i)
    {
        gpioValues[i] = static_cast<uint8_t>((i % 2 == 0) ? 0xAA : 0x55);
        if (verbose)
        {
            lg2::info("Generated GPIO value at index {IDX}: 0x{VAL:X}", "IDX",
                      i, "VAL", gpioValues[i]);
        }
    }

    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + sizeof(offset) +
            sizeof(length) + gpioValuesSize,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_gpio_state_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, offset, length,
        gpioValues.data(), gpioValuesSize, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_gpio_state_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info(
            "getGpioStateHandler: returning GPIO states - offset={OFFSET}, length={LENGTH}, size={SIZE}",
            "OFFSET", offset, "LENGTH", length, "SIZE", gpioValuesSize);
    }

    return response;
}

std::optional<Response>
    MockupResponder::getListAvailablePciePortsHandler(const nsm_msg* requestMsg,
                                                      size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getListAvailablePciePortsHandler: request length={LEN}",
                  "LEN", requestLen);
    }
    auto rc = decode_list_available_pcie_ports_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_list_available_pcie_ports_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    Response response(NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reasonCode = ERR_NULL;
    nsm_list_available_pcie_ports_info info{1, {{1, 2}}};
    rc = encode_list_available_pcie_ports_resp(requestMsg->hdr.instance_id,
                                               NSM_SUCCESS, reasonCode, &info,
                                               responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_list_available_pcie_ports_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getDeviceCapabilitiesV2Handler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    static const uint32_t maxInputBufferSize = 4096;

    if (verbose)
    {
        lg2::info("getDeviceCapabilitiesV2Handler: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_nsm_get_device_capabilities_v2_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_get_device_capabilities_v2_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_capabilities_v2_resp) - 1 +
            NSM_GET_DEVICE_CAPABILITIES_V2_DATA_SIZE,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_nsm_get_device_capabilities_v2_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL,
        NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_EPOCH_TIME,
        maxInputBufferSize, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_nsm_get_device_capabilities_v2_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getEthPortTelemetryCounterHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getEthPortTelemetryCounterHandler: request length={LEN}",
                  "LEN", requestLen);
    }

    uint16_t portNumber = 0;
    auto rc = decode_get_eth_port_telemetry_counter_req(requestMsg, requestLen,
                                                        &portNumber);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_eth_port_telemetry_counter_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    const size_t maxResponseSize =
        ETH_PORT_TELEMETRY_COUNTER_ENABLED_COUNT * sizeof(uint64_t) +
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);
    static std::vector<uint8_t> ethPortTelemetryCounterResponse(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    ethPortTelemetryCounterResponse.reserve(maxResponseSize);
    ethPortTelemetryCounterResponse.clear();
    if (((ethPortTelemetryCounterResponse.capacity() - sizeof(nsm_msg_hdr) -
          sizeof(nsm_aggregate_resp)) >
         ETH_PORT_TELEMETRY_COUNTER_ENABLED_COUNT * sizeof(uint64_t)))
    {
        lg2::error(
            "Response capacity more than maximum aggregate response size");
        return std::nullopt;
    }

    uint16_t samplesCount = 0;
    for (uint8_t tag = 0; tag < ETH_PORT_TELEMETRY_COUNTER_ENABLED_COUNT; ++tag)
    {
        ++samplesCount;

        std::array<uint8_t, 256> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());
        size_t sample_len = 0;

        switch (tag)
        {
            case ETHERNET_PORT_COUNTER_TAG_RX_BYTES:
            case ETHERNET_PORT_COUNTER_TAG_TX_BYTES:
            case ETHERNET_PORT_COUNTER_TAG_RX_UNICAST_BYTES:
            case ETHERNET_PORT_COUNTER_TAG_RX_MULTICAST_BYTES:
            case ETHERNET_PORT_COUNTER_TAG_RX_BROADCAST_BYTES:
            case ETHERNET_PORT_COUNTER_TAG_TX_UNICAST_BYTES:
            case ETHERNET_PORT_COUNTER_TAG_TX_MULTICAST_BYTES:
            case ETHERNET_PORT_COUNTER_TAG_TX_BROADCAST_BYTES:
            {
                nsm_ethernet_port_counter_data mockValue{
                    .ethernet_port_counter_data_64bit = static_cast<uint64_t>(
                        2000000 + tag * 10)}; // Mock value for 64 bit counter
                uint8_t reading[sizeof(uint64_t)] = {};
                sample_len = sizeof(reading);

                rc = encode_aggregate_eth_port_telemetry_data(
                    tag, &mockValue, reading, &sample_len);
                if (rc != NSM_SW_SUCCESS)
                {
                    lg2::error(
                        "encode_aggregate_eth_port_telemetry_data failed: rc={RC}",
                        "RC", rc);
                    return std::nullopt;
                }
                rc = encode_aggregate_resp_sample(
                    tag, true, reading, sample_len, nsm_sample, &sample_len);
                break;
            }
            case ETHERNET_PORT_COUNTER_TAG_RX_FCS_ERRORS:
            case ETHERNET_PORT_COUNTER_TAG_RX_ALIGNMENT_ERRORS:
            case ETHERNET_PORT_COUNTER_TAG_RX_FALSE_CARRIER_DETECTIONS:
            case ETHERNET_PORT_COUNTER_TAG_RX_RUNT_BYTES:
            case ETHERNET_PORT_COUNTER_TAG_RX_JABBER_BYTES:
            case ETHERNET_PORT_COUNTER_TAG_RX_XON_FRAMES:
            case ETHERNET_PORT_COUNTER_TAG_RX_XOFF_FRAMES:
            case ETHERNET_PORT_COUNTER_TAG_TX_XON_FRAMES:
            case ETHERNET_PORT_COUNTER_TAG_TX_XOFF_FRAMES:
            case ETHERNET_PORT_COUNTER_TAG_RX_SINGLE_COLLISION_FRAMES:
            case ETHERNET_PORT_COUNTER_TAG_RX_MULTIPLE_COLLISION_FRAMES:
            case ETHERNET_PORT_COUNTER_TAG_RX_LATE_COLLISION_FRAMES:
            case ETHERNET_PORT_COUNTER_TAG_RX_EXCESSIVE_COLLISION_FRAMES:
            {
                nsm_ethernet_port_counter_data mockValue{
                    .ethernet_port_counter_data_32bit = static_cast<uint32_t>(
                        2000000 + tag * 10)}; // Mock value for 32 bit counter
                uint8_t reading[sizeof(uint32_t)] = {};
                sample_len = sizeof(reading);

                rc = encode_aggregate_eth_port_telemetry_data(
                    tag, &mockValue, reading, &sample_len);
                if (rc != NSM_SW_SUCCESS)
                {
                    lg2::error(
                        "encode_aggregate_eth_port_telemetry_data failed: rc={RC}",
                        "RC", rc);
                    return std::nullopt;
                }
                rc = encode_aggregate_resp_sample(
                    tag, true, reading, sample_len, nsm_sample, &sample_len);
                break;
            }
        }

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("encode_aggregate_resp_sample failed: rc={RC}", "RC",
                       rc);
            return std::nullopt;
        }

        if ((ethPortTelemetryCounterResponse.size() + sample_len) <=
            ethPortTelemetryCounterResponse.capacity())
        {
            ethPortTelemetryCounterResponse.insert(
                ethPortTelemetryCounterResponse.end(), sample.begin(),
                std::next(sample.begin(), sample_len));
        }
        else
        {
            lg2::debug(
                "Not enough capacity in ethPortTelemetryCounterResponse to insert sample");
            return std::nullopt;
        }
    }

    auto responseMsg =
        reinterpret_cast<nsm_msg*>(ethPortTelemetryCounterResponse.data());
    rc = encode_aggregate_resp(requestMsg->hdr.instance_id,
                               NSM_GET_ETH_PORT_TELEMETRY_COUNTER, NSM_SUCCESS,
                               samplesCount, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_aggregate_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    return ethPortTelemetryCounterResponse;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPortNetworkAddressesHandler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPortNetworkAddressesHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint16_t portNumber = 0;
    auto rc = decode_get_network_addresses_req(requestMsg, requestLen,
                                               &portNumber);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_network_addresses_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_aggregate_resp(requestMsg->hdr.instance_id,
                               NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 3,
                               responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    std::array<uint8_t, 50> sample;
    auto nsmSample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());
    size_t consumedLen = 0;

    network_address_sample_data address = {};
    address.link_type = portNumber % 2; // 0 or 1
    uint8_t data[1];
    size_t dataLen = 0;

    rc = encode_aggregate_network_address_data(NSM_TAG_LINK_TYPE, &address,
                                               data, &dataLen);
    assert(rc == NSM_SW_SUCCESS);

    rc = encode_aggregate_resp_sample(NSM_TAG_LINK_TYPE, true, data, dataLen,
                                      nsmSample, &consumedLen);
    assert(rc == NSM_SW_SUCCESS);
    response.insert(response.end(), sample.begin(),
                    std::next(sample.begin(), consumedLen));

    switch (address.link_type)
    {
        case NSM_PORT_PROTOCOL_ETHERNET:
        {
            network_address_sample_data macAddress = {
                .mac_address = {0x1A, 0x2B, 0x1C, 0x3D, 0x4E, 0x5F, 0x00,
                                0x00}};

            uint8_t reading[MAC_ADDRESS_LENGTH]{};
            size_t readingLen{};

            rc = encode_aggregate_network_address_data(
                NSM_TAG_MAC_ADDRESS, &macAddress, reading, &readingLen);
            assert(rc == NSM_SW_SUCCESS);

            rc = encode_aggregate_resp_sample(NSM_TAG_MAC_ADDRESS, true,
                                              reading, readingLen, nsmSample,
                                              &consumedLen);
            assert(rc == NSM_SW_SUCCESS);

            response.insert(response.end(), sample.begin(),
                            std::next(sample.begin(), consumedLen));

            network_address_sample_data permanentMacAddress = {
                .mac_address = {0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x00,
                                0x00}};

            rc = encode_aggregate_network_address_data(
                NSM_TAG_PERMANENT_MAC_ADDRESS, &permanentMacAddress, reading,
                &readingLen);
            assert(rc == NSM_SW_SUCCESS);

            rc = encode_aggregate_resp_sample(NSM_TAG_PERMANENT_MAC_ADDRESS,
                                              true, reading, readingLen,
                                              nsmSample, &consumedLen);
            assert(rc == NSM_SW_SUCCESS);
            response.insert(response.end(), sample.begin(),
                            std::next(sample.begin(), consumedLen));
            break;
        }
        case NSM_PORT_PROTOCOL_INFINIBAND:
        {
            network_address_sample_data nodeGuid = {};
            nodeGuid.network_identifier_64bit = 0x123456789ABCDEF0;

            uint8_t reading[sizeof(uint64_t)]{};
            size_t readingLen{};

            rc = encode_aggregate_network_address_data(
                NSM_TAG_NODE_GUID, &nodeGuid, reading, &readingLen);
            assert(rc == NSM_SW_SUCCESS);

            rc = encode_aggregate_resp_sample(NSM_TAG_NODE_GUID, true, reading,
                                              readingLen, nsmSample,
                                              &consumedLen);
            assert(rc == NSM_SW_SUCCESS);
            response.insert(response.end(), sample.begin(),
                            std::next(sample.begin(), consumedLen));

            network_address_sample_data portGuid = {};
            portGuid.network_identifier_64bit = 0xABCDEF0123456789;

            rc = encode_aggregate_network_address_data(
                NSM_TAG_PORT_GUID, &portGuid, reading, &readingLen);
            assert(rc == NSM_SW_SUCCESS);

            rc = encode_aggregate_resp_sample(NSM_TAG_PORT_GUID, true, reading,
                                              readingLen, nsmSample,
                                              &consumedLen);
            assert(rc == NSM_SW_SUCCESS);
            response.insert(response.end(), sample.begin(),
                            std::next(sample.begin(), consumedLen));
            break;
        }
        default:
        {
            break;
        }
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPortEccCountersHandler(const nsm_msg* requestMsg,
                                               size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getPortEccCountersHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint16_t portNumber = 0;
    auto rc = decode_get_port_ecc_counters_req(requestMsg, requestLen,
                                               &portNumber);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_port_ecc_counters_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_aggregate_resp(requestMsg->hdr.instance_id,
                               NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 6,
                               responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    std::array<uint8_t, 100> sample;
    auto nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());
    size_t consumed_len = 0;

    uint64_t counter_value = 2;
    uint8_t data[8];
    size_t data_len = 0;

    rc = encode_aggregate_port_ecc_counter_data(
        NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES, 10000000000 + counter_value, data,
        &data_len);
    assert(rc == NSM_SW_SUCCESS);

    rc = encode_aggregate_resp_sample(NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES, true,
                                      data, data_len, nsm_sample,
                                      &consumed_len);
    assert(rc == NSM_SW_SUCCESS);
    response.insert(response.end(), sample.begin(),
                    std::next(sample.begin(), consumed_len));

    counter_value = 8;
    rc = encode_aggregate_port_ecc_counter_data(NSM_TAG_ECC_CORRECTED_BITS,
                                                10000000000 + counter_value,
                                                data, &data_len);
    assert(rc == NSM_SW_SUCCESS);

    rc = encode_aggregate_resp_sample(NSM_TAG_ECC_CORRECTED_BITS, true, data,
                                      data_len, nsm_sample, &consumed_len);
    assert(rc == NSM_SW_SUCCESS);
    response.insert(response.end(), sample.begin(),
                    std::next(sample.begin(), consumed_len));

    for (int i = 0; i < 4; i++)
    {
        counter_value = i + 1;
        rc = encode_aggregate_port_ecc_counter_data(
            NSM_TAG_ECC_RAW_ERRORS_LANE_0 + i, 10000000000 + counter_value,
            data, &data_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(NSM_TAG_ECC_RAW_ERRORS_LANE_0 + i,
                                          true, data, data_len, nsm_sample,
                                          &consumed_len);
        assert(rc == NSM_SW_SUCCESS);
        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getDevicemodeSettingsHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getDevicemodeSettingsHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t data_index = 0;
    auto rc = decode_get_device_mode_setting_req(requestMsg, requestLen,
                                                 &data_index);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_device_mode_setting_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    uint8_t device_mode = state.l1_prediction_mode; // Mock device mode
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_device_mode_settings_resp(requestMsg->hdr.instance_id,
                                              NSM_SUCCESS, reason_code,
                                              device_mode, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_device_mode_settings_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getProtectionOptionsHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getProtectionOptionsHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    auto rc = decode_get_protection_options_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_protection_options_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    uint8_t protection_mode = state.protectionMode; // Mock protection mode
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_protection_options_resp(requestMsg->hdr.instance_id,
                                            NSM_SUCCESS, reason_code,
                                            protection_mode, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_protection_options_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setDevicemodeSettingsHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setDevicemodeSettingsHandler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint8_t device_mode_index = 0;
    enum nsm_l1_prediction_mode_config device_mode =
        nsm_l1_prediction_mode_config::ENABLED;

    auto rc = decode_set_device_mode_settings_req(
        requestMsg, requestLen, &device_mode_index, &device_mode);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_device_mode_settings_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    state.l1_prediction_mode = static_cast<uint8_t>(device_mode);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_device_mode_settings_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_set_device_mode_settings_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::vector<uint8_t>
    MockupResponder::getDeviceModeSettingsV2Data(uint32_t deviceModeIndex)
{
    std::vector<uint8_t> data;

    switch (deviceModeIndex)
    {
        case DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT:
        {
            uint32_t powerLimit = htole32(550000);
            data.resize(sizeof(uint32_t));
            memcpy(data.data(), &powerLimit, sizeof(uint32_t));
            break;
        }
        case DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT:
        {
            uint32_t powerLimit = htole32(500000);
            data.resize(sizeof(uint32_t));
            memcpy(data.data(), &powerLimit, sizeof(uint32_t));
            break;
        }
        case DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY:
        {
            uint32_t powerLimit = htole32(450000);
            data.resize(sizeof(uint32_t));
            memcpy(data.data(), &powerLimit, sizeof(uint32_t));
            break;
        }
        case DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY:
        {
            uint32_t powerLimit = htole32(400000);
            data.resize(sizeof(uint32_t));
            memcpy(data.data(), &powerLimit, sizeof(uint32_t));
            break;
        }
        default:
        {
            uint32_t defaultValue = htole32(0);
            data.resize(sizeof(uint32_t));
            memcpy(data.data(), &defaultValue, sizeof(uint32_t));
            break;
        }
    }

    return data;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getDeviceModeSettingsV2Handler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getDeviceModeSettingsV2Handler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint32_t deviceModeIndex = 0;
    auto rc = decode_get_device_mode_settings_v2_req(requestMsg, requestLen,
                                                     &deviceModeIndex);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_device_mode_settings_v2_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    auto currentModeData = getDeviceModeSettingsV2Data(deviceModeIndex);
    auto pendingModeData = getDeviceModeSettingsV2Data(deviceModeIndex);
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            currentModeData.size() + pendingModeData.size() - 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_device_mode_settings_v2_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
        currentModeData.data(), currentModeData.size(), pendingModeData.data(),
        pendingModeData.size(), responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_device_mode_settings_v2_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setDeviceModeSettingsV2Handler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setDeviceModeSettingsV2Handler: request length={LEN}", "LEN",
                  requestLen);
    }

    uint32_t deviceModeIndex = 0;
    uint8_t deviceModeData[sizeof(uint32_t)] = {0};
    uint16_t deviceModeDataLength = 0;

    auto rc = decode_set_device_mode_settings_v2_req(
        requestMsg, requestLen, &deviceModeIndex, deviceModeData,
        &deviceModeDataLength);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_device_mode_settings_v2_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    rc = encode_set_device_mode_settings_v2_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_set_device_mode_settings_v2_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getDeviceDebugParametersHandler(const nsm_msg* requestMsg,
                                                     size_t requestLen)
{
    if (verbose)
    {
        lg2::info("getDeviceDebugParametersHandler: request length={LEN}",
                  "LEN", requestLen);
    }

    uint8_t debug_configuration_type = 0;
    struct nsm_debug_parameter_id parameter_id = {
        .reserved = 0, .port_number = 0, .index = 0};
    nsm_debug_parameter_sub_id_bitfield parameter_sub_id = {.value = 0};

    auto rc = decode_get_device_debug_parameters_req(
        requestMsg, requestLen, &debug_configuration_type, &parameter_id,
        &parameter_sub_id);
    uint8_t debug_configuration_type_log = debug_configuration_type;
    uint8_t parameter_id_index_log = parameter_id.index;
    uint8_t parameter_sub_id_value_log = parameter_sub_id.bits.sub_id;
    uint16_t parameter_id_port_number_log = parameter_id.port_number;
    lg2::debug("decode_get_device_debug_parameters_req: rc={RC} DCT={DCT} "
               "PID={PID} PSUBID={PSUBID} PNB={PNB}",
               "RC", rc, "DCT", debug_configuration_type_log, "PID",
               parameter_id_index_log, "PSUBID", parameter_sub_id_value_log,
               "PNB", parameter_id_port_number_log);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_device_debug_parameters_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("getDeviceDebugParametersHandler: debug_config_type={TYPE}, "
                  "port={PORT}, index={INDEX}, sub_id={SUBID}",
                  "TYPE", debug_configuration_type, "PORT",
                  static_cast<uint16_t>(parameter_id.port_number), "INDEX",
                  parameter_id.index, "SUBID", parameter_sub_id.value);
    }

    std::vector<uint8_t> mockData = {
        0x00,
    };
    uint16_t dataSize = 1;

    switch (parameter_id.index)
    {
        case NSM_DEBUG_PARAMETER_ID_MLPC:
            mockData = {0x12, 0x34, 0x56, 0x78};
            break;
        case NSM_DEBUG_PARAMETER_ID_PPSLC:
            mockData = {0xAB, 0xCD, 0xEF, 0x01};
            break;
        case NSM_DEBUG_PARAMETER_ID_PPSLS:
            mockData = {0x11, 0x22, 0x33, 0x44};
            break;
        case NSM_DEBUG_PARAMETER_ID_PPSPI:
            mockData = {0x55, 0x66, 0x77, 0x88};
            break;
        case NSM_DEBUG_PARAMETER_ID_PPSPHI:
            mockData = {0x99, 0xAA, 0xBB, 0xCC};
            break;
        case NSM_DEBUG_PARAMETER_ID_PPSPC:
            mockData = {0xDD, 0xEE, 0xFF, 0x00};
            break;
        case NSM_DEBUG_PARAMETER_ID_PPCNT:
            switch (parameter_sub_id.bits.sub_id)
            {
                case NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L0_GENERAL_COUNTERS:
                    mockData = {0x01, 0x02, 0x03, 0x04};
                    break;
                case NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L1_GENERAL_COUNTERS:
                    mockData = {0x05, 0x06, 0x07, 0x08};
                    break;
                case NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L1_STAT_COUNTERS:
                    mockData = {0x09, 0x0A, 0x0B, 0x0C};
                    break;
                default:
                    break;
            }
            break;
        case NSM_DEBUG_PARAMETER_ID_MPSCR:
            mockData = {0x10, 0x20, 0x30, 0x40};
            break;
        case NSM_DEBUG_PARAMETER_ID_PPSLG:
            switch (parameter_sub_id.bits.sub_id)
            {
                case NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_CAPABILITIES_AND_STATUS:
                    mockData = {0x50, 0x51, 0x52, 0x53};
                    break;
                case NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_CONFIGURATION:
                    mockData = {0x54, 0x55, 0x56, 0x57};
                    break;
                case NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_DEBUG:
                    mockData = {0x58, 0x59, 0x5A, 0x5B};
                    break;
                case NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L0_CAPABILITIES_AND_STATUS:
                    mockData = {0x5C, 0x5D, 0x5E, 0x5F};
                    break;
                case NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L0_DEBUG:
                    mockData = {0x60, 0x61, 0x62, 0x63};
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    dataSize = mockData.size();
    uint16_t reason_code = ERR_NULL;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_resp) - 1 +
            dataSize,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t dataSize_log = dataSize;
    std::string data_log = requestMsgToHexString(mockData);
    uint16_t dataDataSize_log = mockData.size();
    lg2::debug("encode_get_device_debug_parameters_resp: dataSize={SIZE} "
               "data={DATA} dataSize={DATASIZE}",
               "SIZE", dataSize_log, "DATA", data_log, "DATASIZE",
               dataDataSize_log);

    rc = encode_get_device_debug_parameters_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &dataSize,
        mockData.data(), responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_device_debug_parameters_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setDeviceDebugParametersHandler(const nsm_msg* requestMsg,
                                                     size_t requestLen)
{
    if (verbose)
    {
        lg2::info("setDeviceDebugParametersHandler: request length={LEN}",
                  "LEN", requestLen);
    }

    uint8_t debug_configuration_type = 0;
    struct nsm_debug_parameter_id parameter_id = {
        .reserved = 0, .port_number = 0, .index = 0};
    nsm_debug_parameter_sub_id_bitfield parameter_sub_id = {.value = 0};
    uint8_t data_size = 0;
    uint8_t* data = nullptr;

    auto rc = decode_set_device_debug_parameters_req(
        requestMsg, requestLen, &debug_configuration_type, &parameter_id,
        &parameter_sub_id, &data_size, &data);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_device_debug_parameters_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info(
            "setDeviceDebugParametersHandler: debug_config_type={TYPE}, "
            "port={PORT}, index={INDEX}, sub_id={SUBID}, data_size={SIZE}",
            "TYPE", debug_configuration_type, "PORT",
            static_cast<uint16_t>(parameter_id.port_number), "INDEX",
            parameter_id.index, "SUBID", parameter_sub_id.value, "SIZE",
            data_size);
    }

    // For mockup, we return success for a valid set request
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_cc_only_resp(requestMsg->hdr.instance_id, NSM_TYPE_DIAGNOSTIC,
                             NSM_SET_DEVICE_DEBUG_PARAMETERS, NSM_SUCCESS,
                             ERR_NULL, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_cc_only_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotCAKInstallHandler(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    struct nsm_dot_cak_install_req dot_cak_req;
    auto rc = decode_nsm_dot_cak_install_req(requestMsg, requestLen,
                                             &dot_cak_req);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT CAK Install: decode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    rc = encode_nsm_dot_cak_install_resp(requestMsg->hdr.instance_id,
                                         NSM_SUCCESS, reason_code, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT CAK Install: encode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    lg2::debug("DOT CAK Install: operation completed successfully");
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotCAKBypassHandler(const nsm_msg* requestMsg,
                                         size_t requestLen)
{
    if (verbose)
    {
        lg2::info("Processing DOT CAK Bypass request");
    }

    auto rc = decode_nsm_dot_cak_bypass_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_dot_cak_bypass_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT CAK Bypass request decoded successfully");
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    rc = encode_nsm_dot_cak_bypass_resp(requestMsg->hdr.instance_id,
                                        NSM_SUCCESS, reason_code, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_nsm_dot_cak_bypass_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT CAK Bypass response encoded successfully");
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotLockHandler(const nsm_msg* requestMsg,
                                    size_t requestLen)
{
    struct nsm_dot_lock_req dot_lock_req;
    auto rc = decode_nsm_dot_lock_req(requestMsg, requestLen, &dot_lock_req);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT Lock: decode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t dummy_dot_blob[DOT_BLOB_SIZE];
    for (int i = 0; i < DOT_BLOB_SIZE; i++)
    {
        dummy_dot_blob[i] = static_cast<uint8_t>(i % 256);
    }

    rc = encode_nsm_dot_lock_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                  reason_code, dummy_dot_blob, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT Lock: encode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    lg2::debug("DOT Lock: operation completed successfully");
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotUnlockChallengeHandler(const nsm_msg* requestMsg,
                                               size_t requestLen)
{
    if (verbose)
    {
        lg2::info("Processing DOT Unlock Challenge request");
    }

    struct nsm_dot_unlock_challenge_req unlock_challenge_req;
    auto rc = decode_nsm_dot_unlock_challenge_req(requestMsg, requestLen,
                                                  &unlock_challenge_req);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_dot_unlock_challenge_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT Unlock Challenge request decoded successfully");
        lg2::info("Unlock type: {TYPE}", "TYPE",
                  static_cast<int>(unlock_challenge_req.unlock_type));
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t dummy_challenge[DOT_CHALLENGE_SIZE];
    for (int i = 0; i < DOT_CHALLENGE_SIZE; i++)
    {
        dummy_challenge[i] = static_cast<uint8_t>((i * 3 + 17) % 256);
    }

    rc = encode_nsm_dot_unlock_challenge_resp(requestMsg->hdr.instance_id,
                                              NSM_SUCCESS, reason_code,
                                              dummy_challenge, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_nsm_dot_unlock_challenge_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info(
            "DOT Unlock Challenge response encoded successfully with challenge");
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotUnlockHandler(const nsm_msg* requestMsg,
                                      size_t requestLen)
{
    struct nsm_dot_unlock_req dot_unlock_req;
    auto rc = decode_nsm_dot_unlock_req(requestMsg, requestLen,
                                        &dot_unlock_req);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT Unlock: decode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    rc = encode_nsm_dot_unlock_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                    reason_code, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT Unlock: encode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    lg2::debug("DOT Unlock: operation completed successfully");
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotCAKRotateHandler(const nsm_msg* requestMsg,
                                         [[maybe_unused]] size_t requestLen)
{
    nsm_dot_cak_rotate_req cak_rotate_req;
    auto decode_rc = decode_nsm_dot_cak_rotate_req(requestMsg, requestLen,
                                                   &cak_rotate_req);
    if (decode_rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT CAK Rotate: decode failed: rc={RC}", "RC", decode_rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_rotate_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t dummy_dot_blob[DOT_BLOB_SIZE];
    for (int i = 0; i < DOT_BLOB_SIZE; i++)
    {
        dummy_dot_blob[i] = static_cast<uint8_t>((i * 5 + 23) % 256);
    }

    auto rc = encode_nsm_dot_cak_rotate_resp(requestMsg->hdr.instance_id,
                                             NSM_SUCCESS, reason_code,
                                             dummy_dot_blob, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT CAK Rotate: encode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    lg2::debug("DOT CAK Rotate: operation completed successfully");
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotGetInfoHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    if (verbose)
    {
        lg2::info("Processing DOT Get Info request");
    }

    auto rc = decode_nsm_dot_get_info_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_dot_get_info_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT Get Info request decoded successfully");
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    uint16_t version = 1;
    uint8_t fuse_change_state = 0x00;
    uint8_t transfers_remaining = 128;

    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);
    for (int i = 0; i < DOT_BLOB_SIZE; i++)
    {
        dotBlob[i] = (i % 256);
    }

    rc = encode_nsm_dot_get_info_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, version,
        fuse_change_state, transfers_remaining, dotBlob.data(), responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_nsm_dot_get_info_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info(
            "DOT Get Info response encoded: version={VER}, fuseState={FUSE}, transfers={TRANS}",
            "VER", version, "FUSE", fuse_change_state, "TRANS",
            transfers_remaining);
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotGetStatusHandler(const nsm_msg* requestMsg,
                                         size_t requestLen)
{
    if (verbose)
    {
        lg2::info("Processing DOT Get Status request");
    }

    auto rc = decode_nsm_dot_get_status_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_dot_get_status_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT Get Status request decoded successfully");
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    uint8_t status = 1; // Volatile state

    rc = encode_nsm_dot_get_status_resp(requestMsg->hdr.instance_id,
                                        NSM_SUCCESS, reason_code, status,
                                        responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_nsm_dot_get_status_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        std::string statusText;
        switch (status)
        {
            case 0:
                statusText = "Uninitialized";
                break;
            case 1:
                statusText = "Volatile";
                break;
            case 2:
                statusText = "Locked";
                break;
            case 3:
                statusText = "Disabled";
                break;
            default:
                statusText = "Unknown";
                break;
        }
        lg2::info("DOT Get Status response encoded: status={STATUS} ({TEXT})",
                  "STATUS", status, "TEXT", statusText);
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotDisableHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    if (verbose)
    {
        lg2::info("Processing DOT Disable request");
    }

    struct nsm_dot_disable_req dot_disable_req;
    auto rc = decode_nsm_dot_disable_req(requestMsg, requestLen,
                                         &dot_disable_req);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT Disable: decode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT Disable request decoded successfully");
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t dummy_dot_blob[DOT_BLOB_SIZE];
    for (int i = 0; i < DOT_BLOB_SIZE; i++)
    {
        dummy_dot_blob[i] = static_cast<uint8_t>((i * 7 + 31) % 256);
    }

    rc = encode_nsm_dot_disable_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                     reason_code, dummy_dot_blob, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT Disable: encode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT Disable: operation completed successfully");
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotOverrideHandler(const nsm_msg* requestMsg,
                                        size_t requestLen)
{
    if (verbose)
    {
        lg2::info("Processing DOT Override request");
    }

    struct nsm_dot_override_req dot_override_req;
    auto rc = decode_nsm_dot_override_req(requestMsg, requestLen,
                                          &dot_override_req);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT Override: decode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT Override request decoded successfully");
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    rc = encode_nsm_dot_override_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                      reason_code, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT Override: encode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT Override: operation completed successfully");
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::dotRecoveryHandler(const nsm_msg* requestMsg,
                                        size_t requestLen)
{
    if (verbose)
    {
        lg2::info("Processing DOT Recovery request");
    }

    struct nsm_dot_recovery_req dot_recovery_req;
    auto rc = decode_nsm_dot_recovery_req(requestMsg, requestLen,
                                          &dot_recovery_req);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT Recovery: decode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT Recovery request decoded successfully");
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    rc = encode_nsm_dot_recovery_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                      reason_code, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("DOT Recovery: encode failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    if (verbose)
    {
        lg2::info("DOT Recovery: operation completed successfully");
    }

    return response;
}

} // namespace MockupResponder
