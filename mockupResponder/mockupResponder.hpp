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
#include "device-configuration.h"
#include "diagnostics.h"
#include "network-ports.h"
#include "platform-environmental.h"
#include "requester/mctp.h"

#include "types.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/io.hpp>

#include <optional>

namespace MockupResponder
{
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

class MockupResponder
{
  public:
    MockupResponder(bool verbose, sdeventplus::Event& event,
                    sdbusplus::asio::object_server& server, eid_t eid,
                    uint8_t deviceType, uint8_t instanceId);
    ~MockupResponder() {}

    int initSocket();

    std::optional<std::vector<uint8_t>>
        processRxMsg(const std::vector<uint8_t>& rxMsg);

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
    std::optional<std::vector<uint8_t>>
        queryPortCharacteristicsHandler(const nsm_msg* requestMsg,
                                        size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryPortStatusHandler(const nsm_msg* requestMsg, size_t requestLen);
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
        setEventSubscription(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        setCurrentEventSources(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        configureEventAcknowledgement(const nsm_msg* requestMsg,
                                      size_t requestLen);

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

    std::optional<std::vector<uint8_t>>
        getMigModeHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        setMigModeHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getEccModeHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        setEccModeHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getEccErrorCountsHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getClockLimitHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setClockLimitHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getCurrClockFreqHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getMemoryCapacityUtilHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getProcessorThrottleReasonHandler(const nsm_msg* requestMsg,
                                          size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getAccumCpuUtilTimeHandler(const nsm_msg* requestMsg,
                                   size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getCurrentUtilizationHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getClockOutputEnableStateHandler(const nsm_msg* requestMsg,
                                         size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getPowerLimitHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        setPowerLimitHandler(const nsm_msg* requestMsg, size_t requestLen);

    std::optional<std::vector<uint8_t>>
        getViolationDurationHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);

    // send rediscovery event
    void sendRediscoveryEvent(uint8_t eid, bool ackr);

    // send event
    void sendNsmEvent(uint8_t dest, uint8_t nsmType, bool ackr, uint8_t ver,
                      uint8_t eventId, uint8_t eventClass, uint16_t eventState,
                      uint8_t dataSize, uint8_t* data);

    void sendXIDEvent(uint8_t dest, bool ackr, uint8_t flag, uint32_t reason,
                      uint32_t sequence_number, uint64_t timestamp,
                      std::string message_text);

    void sendResetRequiredEvent(uint8_t eid, bool ackr);

    std::optional<std::vector<uint8_t>>
        queryScalarGroupTelemetryHandler(const nsm_msg* requestMsg,
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

    int mctpSockSend(uint8_t dest, std::vector<uint8_t>& requestMsg,
                     bool verbose);
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
        getEDPpScalingFactorHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getFpgaDiagnosticsSettingsHandler(const nsm_msg* requestMsg,
                                          size_t requestLen);
    std::optional<std::vector<uint8_t>>
        enableDisableWriteProtectedHandler(const nsm_msg* requestMsg,
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
        enableDisableGpuIstModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen);

    std::optional<std::vector<uint8_t>>
        readThermalParameterHandler(const nsm_msg* requestMsg,
                                    size_t requestLen);

    std::optional<std::vector<uint8_t>>
        queryAggregatedGPMMetrics(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryPerInstanceGPMMetrics(const nsm_msg* requestMsg,
                                   size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getReconfigurationPermissionsV1Handler(const nsm_msg* requestMsg,
                                               size_t requestLen);
    std::optional<std::vector<uint8_t>>
        setReconfigurationPermissionsV1Handler(const nsm_msg* requestMsg,
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
        getErrorInjectionModeV1Handler(const nsm_msg* requestMsg,
                                       size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getSupportedErrorInjectionTypesV1Handler(const nsm_msg* requestMsg,
                                                 size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getCurrentErrorInjectionTypesV1Handler(const nsm_msg* requestMsg,
                                               size_t requestLen);

    std::optional<std::vector<uint8_t>>
        queryFirmwareType(const nsm_msg* requestMsg, size_t requestLen);

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

  private:
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
    struct State
    {
        nsm_fpga_diagnostics_settings_wp writeProtected;
        uint8_t istMode;
        std::map<reconfiguration_permissions_v1_index,
                 nsm_reconfiguration_permissions_v1>
            prcKnobs;
        nsm_error_injection_mode_v1 errorInjectionMode;
        std::map<uint8_t, std::map<error_injection_type, bool>>
            errorInjection;
    } state;
};

} // namespace MockupResponder
