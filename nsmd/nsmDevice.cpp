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

#include "nsmDevice.hpp"

#include "common/sleep.hpp"
#include "nsmEvent/nsmFabricManagerStateEvent.hpp"
#include "nsmEvent/nsmLongRunningEventHandler.hpp"
#include "nsmFwSwInventory/GPUSWInventory.hpp"
#include "nsmLongRunning/nsmLongRunningSensor.hpp"
#include "nsmMsgTypesSensor.hpp"
#include "nsmNumericSensor/nsmNumericAggregator.hpp"
#include "progressCounters.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <ranges>
#include <string>

namespace nsm
{

std::shared_ptr<NsmNumericAggregator>
    NsmDevice::findAggregatorByType([[maybe_unused]] const std::string& type)
{
    std::shared_ptr<NsmNumericAggregator> aggregator{};

    auto itr =
        std::find_if(sensorAggregators.begin(), sensorAggregators.end(),
                     [&](const auto& e) { return e->getType() == type; });
    if (itr != sensorAggregators.end())
    {
        aggregator = *itr;
    }

    return aggregator;
}

void NsmDevice::setEventMode(uint8_t mode)
{
    if (mode > GLOBAL_EVENT_GENERATION_ENABLE_PUSH)
    {
        lg2::debug("event generation setting: invalid value={SETTING} provided",
                   "SETTING", mode);
        return;
    }
    eventMode = mode;
}

uint8_t NsmDevice::getEventMode()
{
    return eventMode;
}

void NsmDevice::recordEventSubscriptionStatus(std::string_view status)
{
    lastEventSubscriptionStatus = std::string(status);
    // Log failures/skips but not success (OK) or unsupported command
    if (!status.empty() && !status.starts_with("OK") &&
        status.find("unsupported command") == std::string_view::npos)
    {
        lg2::error("Event subscription eid={EID}: {STATUS}", "EID", eid,
                   "STATUS", status);
    }
}

void NsmDevice::recordEventSubscriptionStatus(
    std::string_view status, std::optional<std::vector<uint8_t>> request,
    std::optional<std::vector<uint8_t>> response)
{
    lastEventSubscriptionStatus = std::string(status);
    lastEventSubscriptionRequest = std::move(request);
    lastEventSubscriptionResponse = std::move(response);
    // Log failures/skips but not success (OK) or unsupported command
    if (!status.empty() && !status.starts_with("OK") &&
        status.find("unsupported command") == std::string_view::npos)
    {
        lg2::error("Event subscription eid={EID}: {STATUS}", "EID", eid,
                   "STATUS", status);
    }
}

std::optional<std::string> NsmDevice::getLastEventSubscriptionStatus() const
{
    return lastEventSubscriptionStatus;
}

std::optional<std::vector<uint8_t>>
    NsmDevice::getLastEventSubscriptionRequest() const
{
    return lastEventSubscriptionRequest;
}

std::optional<std::vector<uint8_t>>
    NsmDevice::getLastEventSubscriptionResponse() const
{
    return lastEventSubscriptionResponse;
}

void NsmDevice::setHasNsmEventSettingConfig(bool hasConfig)
{
    hasNsmEventSettingConfigFlag = hasConfig;
}

bool NsmDevice::hasNsmEventSettingConfig() const
{
    return hasNsmEventSettingConfigFlag;
}

bool NsmDevice::allCommandCodesAreRetrieved()
{
    return areMessageTypesRetrieved &&
           std::ranges::all_of(retrievedMessageTypes, [&](auto messageType) {
        // Skip message types that are out of the valid range
        if (messageType >= NUM_NSM_TYPES)
        {
            return true; // Consider out-of-range types as "retrieved" to not
                         // block
        }
        return commandCodesRetrieved[messageType];
    });
}

bool NsmDevice::isCommandSupported(uint8_t messageType, uint8_t commandCode)
{
    // Route through the bounds-checked accessor so we cannot fall off the
    // end of the inner row even if commandCode is FW-reported.
    return isCommandCodeSupportedSafe(messageType, commandCode).value_or(false);
}

std::optional<bool>
    NsmDevice::isCommandCodeSupportedSafe(uint8_t messageType,
                                          uint8_t commandCode) const noexcept
{
    if (messageType >= messageTypesToCommandCodeMatrix.size())
    {
        return std::nullopt;
    }
    const auto& row = messageTypesToCommandCodeMatrix[messageType];
    if (commandCode >= row.size())
    {
        return std::nullopt;
    }
    return row[commandCode];
}

bool NsmDevice::setCommandCodeSupportedSafe(uint8_t messageType,
                                            uint8_t commandCode,
                                            bool isSupported) noexcept
{
    if (messageType >= messageTypesToCommandCodeMatrix.size())
    {
        lg2::warning(
            "setCommandCodeSupportedSafe: messageType={MT} out of range (size={MAX}); dropping FW-reported support indicator (NvBug 5972182 class)",
            "MT", messageType, "MAX",
            static_cast<int>(messageTypesToCommandCodeMatrix.size()));
        return false;
    }
    auto& row = messageTypesToCommandCodeMatrix[messageType];
    if (commandCode >= row.size())
    {
        lg2::warning(
            "setCommandCodeSupportedSafe: commandCode={CC} out of range for messageType={MT} (row size={MAX}); dropping FW-reported support indicator (NvBug 5972182)",
            "CC", commandCode, "MT", messageType, "MAX",
            static_cast<int>(row.size()));
        return false;
    }
    row[commandCode] = isSupported;
    return true;
}

void NsmDevice::updateMessageTypesToCommandCodeMatrix(
    uint8_t messageType, const bitfield8_t supportedCommands[],
    size_t supportedCommandsSize)
{
    // Check if messageType is within the valid range
    if (messageType >= NUM_NSM_TYPES)
    {
        if (shouldLog("updateMessageTypesToCommandCodeMatrix:EID:" +
                          std::to_string(getEid()) +
                          ":MT:" + std::to_string(messageType),
                      true))
        {
            lg2::warning(
                "updateMessageTypesToCommandCodeMatrix: skipping invalid message type={MT} (>= NUM_NSM_TYPES={NUM})",
                "EID", getEid(), "MT", messageType, "NUM", NUM_NSM_TYPES);
        }
        return;
    }
    // check applied to avoid core dump in case of supportedCommandsSize*8 is
    // greater than NUM_COMMAND_CODES
    auto maxCommandCode = std::min(supportedCommandsSize * 8,
                                   static_cast<size_t>(NUM_COMMAND_CODES));
    for (size_t i = 0; i < maxCommandCode; i++)
    {
        auto isSupported = supportedCommands[i / 8].byte & (1 << (i % 8));
        // setCommandCodeSupportedSafe is a no-op if (messageType, i) falls
        // outside the matrix — same shape as the outer guard above. This
        // also closes the FW-payload OOB class (NvBug 5972182) if the row
        // is ever resized smaller than NUM_COMMAND_CODES.
        (void)setCommandCodeSupportedSafe(messageType, static_cast<uint8_t>(i),
                                          static_cast<bool>(isSupported));
    }
}

void NsmDevice::addSensorBase(const std::shared_ptr<NsmObject>& sensor,
                              PollingType pollingType)
{
    sensor->deviceIdentifier =
        utils::getDeviceInstanceName(getDeviceType(), getInstanceNumber());
    deviceSensors.push_back(sensor);
    switch (pollingType)
    {
        case PollingType::Priority:
            sensor->refreshLimitInUsec = PRIORITY_REFRESH_LIMIT_IN_USEC;
            break;
        case PollingType::GpuPerformanceMonitoring:
            sensor->refreshLimitInUsec = GPM_REFRESH_LIMIT_IN_USEC;
            break;
        case PollingType::LongRunning:
            sensor->refreshLimitInUsec = LONG_RUNNING_REFRESH_LIMIT_IN_USEC;
            break;
        case PollingType::Static:
        case PollingType::RoundRobin:
            sensor->refreshLimitInUsec = RR_REFRESH_LIMIT_IN_USEC;
            break;
        default:
            throw std::runtime_error(
                "NsmDevice::addSensorBase: Invalid polling type");
    }
    sensors[pollingType].push(sensor);
}

void NsmDevice::removeSensor(const std::shared_ptr<NsmObject>& sensor)
{
    for (auto& [_, queue] : sensors)
    {
        std::erase(queue, sensor);
    }
    std::erase(deviceSensors, sensor);
    std::erase(standByToDcRefreshSensors, sensor);
    std::erase(capabilityRefreshSensors, sensor);
}

/* Mark all the sensors as unrefreshed. Sleep for 10 seconds to avoid
 * overwhelming the system*/
requester::Coroutine NsmDevice::markSensorsUnrefreshed()
{
    uint32_t count = 0;
    auto sensors = deviceSensors;
    for (auto sensor : sensors)
    {
        sensor->isRefreshed = false;
        count++;
        if (count == MAX_SENSOR_UPDATE_BATCH_SIZE)
        {
            co_await common::Sleep(event, 10000, common::NonPriority);
            count = 0;
        }
    }
    co_return NSM_SW_SUCCESS;
}

// Steps for transition of Device to online
// 1. set isDeviceReady to false
// 2. set NsmServiceReadyIntf to starting
// 3. mark all the sensors as unrefreshed
// 4. notify sensors of device reconnect/online transition
// 5. set isDeviceActive to true
// 6. Refresh all the capability sensors

requester::Coroutine NsmDevice::setOnline()
{
    isDeviceReady = false;
    auto tmpEid = eid;
    NsmServiceReadyIntf::getInstance().setStateStarting();
    co_await markSensorsUnrefreshed();
    if (tmpEid != eid)
    {
        co_return NSM_SW_SUCCESS;
    }

    // Iterate a snapshot because onDeviceOnline() handlers may
    // modify deviceSensors list during discovery.
    auto deviceSensorsBeforeOnline = deviceSensors;
    for (auto& sensor : deviceSensorsBeforeOnline)
    {
        sensor->onDeviceOnline();
    }

    isDeviceActive = true;
    lg2::info(
        "NSMDevice: deviceType:{DEVTYPE} InstanceNumber:{INSTNUM} gets online",
        "DEVTYPE", getDeviceType(), "INSTNUM", getInstanceNumber());
    co_await refreshCapabilitySensor();
    co_return NSM_SW_SUCCESS;
}

/* Steps to update sensors for offline
 * Mark all the sensors as NaN
 * Sleep for 10 seconds to avoid overwhelming the system
 */
requester::Coroutine NsmDevice::updateSensorsForOffline()
{
    size_t sensorIndex{0};
    auto sensors = deviceSensors;

    while (sensorIndex < sensors.size())
    {
        uint32_t count = 0;
        while (count < MAX_SENSOR_UPDATE_BATCH_SIZE &&
               sensorIndex < sensors.size())
        {
            auto sensor = sensors[sensorIndex];
            sensor->handleOfflineState();
            ++sensorIndex;
            ++count;
        }
        co_await common::Sleep(event, 10000, common::NonPriority);
    }
    co_return NSM_SW_SUCCESS;
}

/* Steps for transition of Device to offline
 * Set Device as offline
 * Set GPU driver state to unknown
 * Update values of sensors to NaN
 */
requester::Coroutine NsmDevice::setOffline()
{
    isDeviceActive = false;
    if (gpuDriverSensor)
    {
        lg2::info(
            "Setting GPU driver state to unknown as eid = {EID} gets offline",
            "EID", eid);
        gpuDriverSensor->driverState = 0;
    }
    lg2::info(
        "NSMDevice: deviceType:{DEVTYPE} InstanceNumber:{INSTNUM} gets offline",
        "DEVTYPE", getDeviceType(), "INSTNUM", getInstanceNumber());

    co_await updateSensorsForOffline();
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDevice::waitForNsmDeviceUpdate()
{
    int iter = 0;
    while (discoveryPending && iter < DEVICE_UPDATE_POST_PATCH_SLEEP_MAX_ITER)
    {
        iter++;
        co_await common::Sleep(event, DEVICE_UPDATE_POST_PATCH_SLEEP_MS * 1000,
                               common::NonPriority);
    }
    if (discoveryPending)
    {
        co_return NSM_SW_ERROR_TIMEOUT;
    }
    co_return NSM_SW_SUCCESS;
}

void NsmDevice::registerLongRunningEventHandler()
{
    addDeviceEvent(std::make_shared<NsmLongRunningEventHandler>(),
                   NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                   NSM_LONG_RUNNING_EVENT);
}

void NsmDevice::registerLongRunningHandler(
    uint8_t messageType, uint8_t commandCode,
    std::shared_ptr<NsmLongRunningEvent> sensorInstance)
{
    clearLongRunningHandler();

    longRunningHandler = ActiveLongRunningHandlerInfo{messageType, commandCode,
                                                      sensorInstance};
}

void NsmDevice::clearLongRunningHandler()
{
    if (!longRunningHandler.has_value())
    {
        return;
    }

    longRunningHandler.reset();
}

int NsmDevice::invokeLongRunningHandler(eid_t eid, NsmType type,
                                        NsmEventId eventId,
                                        const nsm_msg* event, size_t eventLen)
{
    if (shouldLog("NsmDevice::invokeLongRunningHandler",
                  !longRunningHandler.has_value()))
    {
        lg2::error(
            "NsmDevice::invokeLongRunningHandler: No active handler registered for long-running event, EID={EID}",
            "EID", eid);
    }
    if (!longRunningHandler.has_value())
    {
        return NSM_SW_ERROR_DATA;
    }

    uint16_t eventState = 0;
    uint8_t dataSize = 0;
    auto rc = decode_nsm_event(event, eventLen, eventId,
                               NSM_NVIDIA_GENERAL_EVENT_CLASS, &eventState,
                               &dataSize);

    if (shouldLog("decode_nsm_event", nsm_sw_codes(rc)))
    {
        lg2::error(
            "NsmLongRunningEventHandler : Failed to decode long running event state : EID={EID}",
            "EID", eid);
    }
    if (rc != NSM_SW_SUCCESS)
    {
        return rc;
    }
    nsm_long_running_event_state state{};
    memcpy(&state, &eventState, sizeof(uint16_t));

    const auto& [messageType, commandCode,
                 sensorInstance] = *longRunningHandler;

    if (state.nvidia_message_type != messageType ||
        state.command != commandCode)
    {
        lg2::error(
            "NsmDevice::invokeLongRunningHandler: Mismatched message type or command code, "
            "Expected MessageType={EXPECTED_MSG_TYPE}, Received MessageType={RECEIVED_MSG_TYPE}, "
            "Expected CommandCode={EXPECTED_COMMAND_CODE}, Received CommandCode={RECEIVED_COMMAND_CODE}, EID={EID}",
            "EXPECTED_MSG_TYPE", messageType, "RECEIVED_MSG_TYPE",
            uint8_t(state.nvidia_message_type), "EXPECTED_COMMAND_CODE",
            commandCode, "RECEIVED_COMMAND_CODE", uint8_t(state.command), "EID",
            eid);
        return NSM_SW_ERROR_DATA;
    }

    // Call the `handle` method directly on the instance
    return sensorInstance->handle(eid, type, eventId, event, eventLen);
}

void NsmDevice::initMsgTypesSensor()
{
    msgTypesSensor = std::make_shared<NsmMsgTypes>(
        "Supported Message Types", "NSM_NVIDIA_MESSAGE_TYPE", *this);
    addSensor(msgTypesSensor, false);
}

requester::Coroutine
    NsmDevice::SendRecvNsmMsg(eid_t eid, Request& request,
                              std::shared_ptr<const nsm_msg>& responseMsg,
                              size_t* responseLen)
{
    auto rc = co_await nsmMsgHandler->SendRecvNsmMsg(eid, request, responseMsg,
                                                     responseLen);
    if (rc)
    {
        lg2::error("NsmDevice::SendRecvNsmMsg failed. eid={EID} rc={RC}", "EID",
                   eid, "RC", utils::nsmSwCodeToString(rc));
    }
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine
    NsmDevice::sensorIO(eid_t eid, Request& request,
                        std::shared_ptr<const nsm_msg>& responseMsg,
                        size_t& responseLen, bool bypassCommandCheck)
{
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    uint8_t messageType = requestMsg->hdr.nvidia_msg_type;
    uint8_t commandCode = requestMsg->payload[0];

    // bypassCommandCheck will be true for raw command only, by default it is
    // false
    if (!bypassCommandCheck &&
        (!isDeviceActive || !isCommandSupported(messageType, commandCode)))
    {
        co_return NSM_ERR_UNSUPPORTED_COMMAND_CODE;
    }

    auto rc = co_await nsmMsgHandler->SendRecvNsmMsg(eid, request, responseMsg,
                                                     &responseLen);
    if (rc && rc != NSM_SW_ERROR_TIMEOUT)
    {
        lg2::error("SendRecvNsmMsg failed. eid={EID} rc={RC}", "EID", eid, "RC",
                   utils::nsmSwCodeToString(rc));
    }

    co_return rc;
}

requester::Coroutine
    NsmDevice::postPatchIO(eid_t eid, Request& request,
                           std::shared_ptr<const nsm_msg>& responseMsg,
                           size_t& responseLen)
{
    auto rc = co_await waitForNsmDeviceUpdate();
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmDevice::postPatchIO unexpected error while waiting for NsmDevice update for eid={EID}, rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        co_return rc;
    }

    if (!isDeviceActive)
    {
        co_return NSM_ERR_UNSUPPORTED_COMMAND_CODE;
    }

    rc = co_await nsmMsgHandler->SendRecvNsmMsg(eid, request, responseMsg,
                                                &responseLen);
    // NSM_SW_ERROR_TIMEOUT: indicates no nsm response which is possible for
    // request that timedout
    if (rc && rc != NSM_SW_ERROR_TIMEOUT)
    {
        lg2::error("NsmDevice::postPatchIO failed. eid={EID} rc={RC}", "EID",
                   eid, "RC", utils::nsmSwCodeToString(rc));
    }
    progressCounters().increment(ProgressCounterType::PostPatch, rc);
    co_return rc;
}

bool NsmDevice::updateDiscoveryIdentifiers(eid_t eid, uuid_t uuid,
                                           uint8_t deviceInstanceNumber,
                                           std::string& associatedPath,
                                           std::string& mctpMedium,
                                           std::string& mctpBinding,
                                           std::optional<eid_t> mctpLocalEid)
{
    bool isPreferred = true;
    try
    {
        if (this->uuid.size() > 0)
        {
            isPreferred = utils::isPreferred(
                std::make_tuple(this->mctpMedium, this->mctpBinding),
                std::make_tuple(mctpMedium, mctpBinding));
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "NsmDevice::updateDiscoveryIdentifiers failed, eid={EID} uuid={UUID} mctpMedium={MCTP_MEDIUM} mctpBinding={MCTP_BINDING} error={ERROR}",
            "EID", eid, "UUID", uuid, "MCTP_MEDIUM", mctpMedium, "MCTP_BINDING",
            mctpBinding, "ERROR", e.what());
        return false;
    }

    if (isPreferred)
    {
        if (this->eid != eid && this->uuid.size() != 0)
        {
            fruDeviceManager.reset(objServer);
        }
        this->eid = eid;
        this->mctpLocalEid =
            mctpLocalEid; // may be nullopt if MCTP did not provide LocalEID
        this->uuid = uuid;
        this->mctpMedium = mctpMedium;
        this->mctpBinding = mctpBinding;
        this->nsmDeviceInstanceNumber = deviceInstanceNumber;
        this->associatedPath = associatedPath;
        lg2::info(
            "NsmDevice: DeviceType={TYPE} InstanceNumber={INST} DeviceRole={ROLE} is updated with eid={EID} uuid={UUID} mctpMedium={MCTP_MEDIUM} mctpBinding={MCTP_BINDING} deviceInstanceNumber = {DEVICE_INSTANCE_NUMBER} mctpAssociation = {MCTP_ASSOCIATION}",
            "TYPE", getDeviceType(), "INST", getInstanceNumber(), "ROLE",
            getDeviceRole(), "EID", eid, "UUID", uuid, "MCTP_MEDIUM",
            mctpMedium, "MCTP_BINDING", mctpBinding, "DEVICE_INSTANCE_NUMBER",
            deviceInstanceNumber, "MCTP_ASSOCIATION", associatedPath);
    }
    else
    {
        lg2::info(
            "NsmDevice: DeviceType={TYPE} InstanceNumber={INST} DeviceRole={ROLE} is not updated with eid={EID} uuid={UUID} mctpMedium={MCTP_MEDIUM} mctpBinding={MCTP_BINDING}, as device is already on EID={EXISTING_EID} medium={EXISTING_MEDIUM} binding={EXISTING_BINDING} mctpAssociation = {MCTP_ASSOCIATION}",
            "TYPE", getDeviceType(), "INST", getInstanceNumber(), "ROLE",
            getDeviceRole(), "EID", eid, "UUID", uuid, "MCTP_MEDIUM",
            mctpMedium, "MCTP_BINDING", mctpBinding, "EXISTING_EID", this->eid,
            "EXISTING_MEDIUM", this->mctpMedium, "EXISTING_BINDING",
            this->mctpBinding, "MCTP_ASSOCIATION", associatedPath);
    }

    return isPreferred;
}

requester::Coroutine NsmDevice::updateNsmDevice()
{
    auto localEid = eid;
    co_await discoverNsmDeviceSemaphore.acquire(localEid);
    lg2::info(
        "NsmDevice::updateNsmDevice discoverNsmDeviceSemaphore acqired for EID = {EID}",
        "EID", localEid);
    // Reset messageTypesToCommandCodeMatrix to all false entries
    messageTypesToCommandCodeMatrix.assign(
        NUM_NSM_TYPES, std::vector<bool>(NUM_COMMAND_CODES, false));

    std::vector<uint8_t> supportedMessageTypes;
    auto rc = co_await getSupportedNvidiaMessageType(supportedMessageTypes);
    discoveryEvents().setValue(
        DiscoveryEventType::GetSupportedNvidiaMessageType, rc);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("getSupportedNvidiaMessageType() failed, rc={RC} eid={EID}",
                   "RC", utils::nsmSwCodeToString(rc), "EID", eid);
        discoverNsmDeviceSemaphore.release();
        // coverity[missing_return]
        co_return rc;
    }

    // Update the NsmDevice object with the retrieved message types
    areMessageTypesRetrieved = true;
    retrievedMessageTypes = supportedMessageTypes;

    // Loop through supported message types
    for (uint8_t messageType : supportedMessageTypes)
    {
        // Skip message types that are out of the valid range
        if (messageType >= NUM_NSM_TYPES)
        {
            lg2::warning(
                "Skipping unsupported message type={MT} (>= NUM_NSM_TYPES={NUM}) for eid={EID}",
                "MT", messageType, "NUM", NUM_NSM_TYPES, "EID", eid);
            continue;
        }

        std::vector<uint8_t> supportedCommands;
        rc = co_await getSupportedCommandCodes(messageType, supportedCommands);
        discoveryEvents().setValue(
            static_cast<DiscoveryEventType>(
                static_cast<uint8_t>(
                    DiscoveryEventType::GetSupportedCommandCodes0) +
                messageType),
            rc);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "getSupportedCommands() for message type={MT} return failed, rc={RC} eid={EID}",
                "MT", messageType, "RC", utils::nsmSwCodeToString(rc), "EID",
                eid);
            commandCodesRetrieved[messageType] = false;
            continue;
        }

        // Update the NsmDevice object with the retrieved command codes
        commandCodesRetrieved[messageType] = true;

        std::vector<uint8_t> supportedCommandCodes;
        utils::convertBitMaskToVector(
            supportedCommandCodes,
            reinterpret_cast<const bitfield8_t*>(&supportedCommands[0]),
            SUPPORTED_COMMAND_CODE_DATA_SIZE);
        std::stringstream ss;
        for (uint8_t commandCode : supportedCommandCodes)
        {
            // Bounds-checked write — NvBug 5972182 mitigation. FW reports
            // outside the matrix dimensions are logged + dropped rather
            // than UB-indexing past the row.
            (void)setCommandCodeSupportedSafe(messageType, commandCode, true);
            ss << int(commandCode) << " ";
        }
        lg2::info(
            "Eid: {EID} - MessageType {ROW_NUM}: commandCodes {ROW_VALUES}",
            "EID", eid, "ROW_NUM", messageType, "ROW_VALUES", ss.str());
    }

    // Update fruDevice interface
    rc = co_await updateFruDeviceIntf();
    if (rc)
    {
        lg2::error("updateFruDeviceIntf failed, rc={RC} eid={EID}", "RC",
                   utils::nsmSwCodeToString(rc), "EID", eid);
    }

    discoverNsmDeviceSemaphore.release();

    lg2::info(
        "NsmDevice::updateNsmDevice discoverNsmDeviceSemaphore released for EID = {EID}",
        "EID", localEid);
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmDevice::getSupportedNvidiaMessageType(
    std::vector<uint8_t>& supportedNvidiaMessageTypes)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_nvidia_message_types_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_nvidia_message_types_req(DEFAULT_INSTANCE_ID,
                                                            requestMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmDevice::getSupportedNvidiaMessageType failed. eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmMsgHandler->SendRecvNsmMsg(eid, request, responseMsg,
                                                &responseLen);
    if (rc)
    {
        lg2::error(
            "NsmDevice::getSupportedNvidiaMessageType SendRecvNsmMsg failed. eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE];
    rc = decode_get_supported_nvidia_message_types_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, types);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmDevice::getSupportedNvidiaMessageType decode failed. eid={EID} cc={CC} reasonCode={REASONCODE} and rc={RC}",
            "EID", eid, "CC", utils::nsmCompletionCodeToString(cc),
            "REASONCODE", utils::nsmReasonCodeToString(reasonCode), "RC",
            utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    // convert bit matrix of types into supportedTypes
    supportedNvidiaMessageTypes.clear();
    for (size_t i = 0; i < SUPPORTED_MSG_TYPE_DATA_SIZE * 8; i++)
    {
        auto bi = i % 8;
        auto ti = i / 8;
        if (types[ti].byte & (1 << bi))
        {
            supportedNvidiaMessageTypes.push_back(i);
        }
    }

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmDevice::getSupportedCommandCodes(uint8_t nvidia_message_type,
                                        std::vector<uint8_t>& supportedCommands)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_command_codes_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_command_codes_req(
        DEFAULT_INSTANCE_ID, nvidia_message_type, requestMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmDevice::getSupportedCommandCodes failed. eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmMsgHandler->SendRecvNsmMsg(eid, request, responseMsg,
                                                &responseLen);
    if (rc)
    {
        lg2::error(
            "NsmDevice::getSupportedCommandCodes SendRecvNsmMsg failed. eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    bitfield8_t supportedCommandCodes[SUPPORTED_COMMAND_CODE_DATA_SIZE];
    rc = decode_get_supported_command_codes_resp(responseMsg.get(), responseLen,
                                                 &cc, &reasonCode,
                                                 &supportedCommandCodes[0]);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmDevice::getSupportedCommandCodes decode failed. eid={EID} cc={CC} reasonCode={REASONCODE} and rc={RC}",
            "EID", eid, "CC", utils::nsmCompletionCodeToString(cc),
            "REASONCODE", utils::nsmReasonCodeToString(reasonCode), "RC",
            utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    // copy supportedCommandCodes into supportedCommands
    supportedCommands.resize(SUPPORTED_COMMAND_CODE_DATA_SIZE);
    std::memcpy(supportedCommands.data(), supportedCommandCodes,
                SUPPORTED_COMMAND_CODE_DATA_SIZE * sizeof(bitfield8_t));
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDevice::updateFruDeviceIntf()
{
    // get inventory information from device
    InventoryProperties properties{};
    uint8_t rc = NSM_SW_SUCCESS;
    lg2::info("NsmDevice::updateFruDeviceIntf: eid={EID} deviceType={TYPE}",
              "EID", eid, "TYPE", getDeviceType());
    if (isCommandSupported(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                           NSM_GET_INVENTORY_INFORMATION))
    {
        rc = co_await getFRU(properties, getDeviceType());
        discoveryEvents().setValue(DiscoveryEventType::GetFru, rc);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("getFRU() return failed, rc={RC} eid={EID}", "RC",
                       utils::nsmSwCodeToString(rc), "EID", eid);
        }
    }

    // Determine currently supported properties by checking specific known
    // properties
    std::set<std::string> currentProperties;

    // Check for dynamic properties that the device might support
    if (properties.find(BOARD_PART_NUMBER) != properties.end())
    {
        currentProperties.insert("BOARD_PART_NUMBER");
    }
    if (properties.find(FRU_PART_NUMBER) != properties.end())
    {
        currentProperties.insert("FRU_PART_NUMBER");
    }
    if (properties.find(SERIAL_NUMBER) != properties.end())
    {
        currentProperties.insert("SERIAL_NUMBER");
    }
    if (properties.find(MARKETING_NAME) != properties.end())
    {
        currentProperties.insert("MARKETING_NAME");
    }
    if (properties.find(PRODUCT_NAME) != properties.end())
    {
        currentProperties.insert("PRODUCT_NAME");
    }
    if (properties.find(ASSET_TAG) != properties.end())
    {
        currentProperties.insert("ASSET_TAG");
    }
    if (properties.find(BUILD_DATE) != properties.end())
    {
        currentProperties.insert("BUILD_DATE");
    }
    if (properties.find(DEVICE_GUID) != properties.end())
    {
        currentProperties.insert("DEVICE_UUID");
    }

    // Always include fixed properties
    currentProperties.insert("DEVICE_TYPE");
    currentProperties.insert("INSTANCE_NUMBER");
    currentProperties.insert("DEVICE_ROLE");
    currentProperties.insert("UUID");

    // Check if interface needs recreation
    if (fruDeviceManager.needsRecreation(currentProperties))
    {
        lg2::info(
            "Creating/Recreating FRU interface for eid={EID} with {COUNT} properties",
            "EID", eid, "COUNT", currentProperties.size());

        // Create interface and register all properties
        fruDeviceManager.createAndRegisterInterface(
            objServer, eid, shared_from_this(), properties);

        fruDeviceManager.markInitialized(currentProperties);
    }
    else
    {
        lg2::debug("Updating FRU property values for eid={EID}", "EID", eid);

        // Just update property values
        fruDeviceManager.updateAllPropertyValues(shared_from_this(),
                                                 properties);
    }

    co_return rc;
}

requester::Coroutine NsmDevice::getFRU(nsm::InventoryProperties& properties,
                                       const uint8_t& deviceType)
{
    // creating map to avoid sending unsupported comamnds to devices
    // Define a map that stores property IDs based on deviceType
    static const std::unordered_map<uint8_t, std::vector<uint8_t>>
        devicePropertyMap = {
            {NSM_DEV_ID_GPU,
             {BOARD_PART_NUMBER, FRU_PART_NUMBER, SERIAL_NUMBER, DEVICE_GUID,
              MARKETING_NAME, BUILD_DATE}},
            {NSM_DEV_ID_SWITCH,
             {BOARD_PART_NUMBER, SERIAL_NUMBER, DEVICE_GUID, MARKETING_NAME,
              BUILD_DATE}},
            {NSM_DEV_ID_PCIE_BRIDGE,
             {BOARD_PART_NUMBER, SERIAL_NUMBER, DEVICE_GUID, MARKETING_NAME,
              PRODUCT_NAME, BUILD_DATE, ASSET_TAG}},
            {NSM_DEV_ID_BASEBOARD, {}},
            {NSM_DEV_ID_EROT,
             {BOARD_PART_NUMBER, SERIAL_NUMBER, DEVICE_GUID, MARKETING_NAME,
              BUILD_DATE}},
            {NSM_DEV_ID_MCTP_BRIDGE, {SERIAL_NUMBER}},
            {NSM_DEV_ID_CPU, {}},
            {NSM_DEV_ID_UNKNOWN, {}}};

    // Fetch property IDs based on deviceType; fallback to an empty list if not
    // found
    auto it = devicePropertyMap.find(deviceType);
    const std::vector<uint8_t>& propertyIds =
        (it != devicePropertyMap.end())
            ? it->second
            : devicePropertyMap.at(NSM_DEV_ID_UNKNOWN);

    for (auto propertyId : propertyIds)
    {
        auto rc = co_await getInventoryInformation(propertyId, properties);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "NsmDevice::getFRU getInventoryInformation failed for propertyId={PID} eid={EID} rc={RC}",
                "PID", propertyId, "EID", eid, "RC",
                utils::nsmSwCodeToString(rc));
        }
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmDevice::getInventoryInformation(uint8_t& propertyIdentifier,
                                       nsm::InventoryProperties& properties)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        DEFAULT_INSTANCE_ID, propertyIdentifier, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmDevice::getInventoryInformation encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmMsgHandler->SendRecvNsmMsg(eid, request, responseMsg,
                                                &responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    std::vector<uint8_t> data(65535, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        // coverity[missing_return]
        co_return cc ? cc : rc;
    }

    std::optional<nsm::InventoryPropertyData> property;
    switch (propertyIdentifier)
    {
        case BOARD_PART_NUMBER:
        case SERIAL_NUMBER:
        case MARKETING_NAME:
        case DEVICE_PART_NUMBER:
        case FRU_PART_NUMBER:
        case MEMORY_VENDOR:
        case MEMORY_PART_NUMBER:
        case BUILD_DATE:
        case FIRMWARE_VERSION:
        case INFO_ROM_VERSION:
        case PRODUCT_NAME:
        case ASSET_TAG:
        {
            property = std::string((char*)data.data(), dataSize);
        }
        break;
        case DEVICE_GUID:
        {
            std::vector<uint8_t> nvu8ArrVal(UUID_INT_SIZE, 0);
            // DEVICE_GUID is a fixed-width field: require the wire dataSize to
            // be exactly the UUID width and reject anything else, rather than
            // accepting an oversized value and truncating into the buffer.
            if (dataSize != UUID_INT_SIZE || data.size() < UUID_INT_SIZE)
            {
                lg2::error(
                    "NsmDevice::getInventoryInformation decode_get_inventory_information_resp invalid property data. eid={EID} porpertyID={PID}",
                    "EID", eid, "PID", propertyIdentifier);
                // coverity[missing_return]
                co_return NSM_SW_ERROR_LENGTH;
            }
            memcpy(nvu8ArrVal.data(), data.data(), UUID_INT_SIZE);

            uuid_t uuidStr = utils::convertUUIDToString(nvu8ArrVal);
            if (uuidStr.empty())
            {
                lg2::error(
                    "NsmDevice::getInventoryInformation id={ID} received incorrect GUID",
                    "ID", propertyIdentifier);
            }
            else
            {
                properties.emplace(propertyIdentifier, uuidStr);
            }
        }
        break;
        default:
        {
            lg2::info("NsmDevice::getInventoryInformation unsupported id={ID}",
                      "ID", propertyIdentifier);
        }
        break;
    }

    if (property.has_value())
    {
        auto& propertyValue = property.value();
        properties.emplace(propertyIdentifier, propertyValue);

        if (const auto* propertyStringPtr =
                std::get_if<std::string>(&propertyValue))
        {
            lg2::info(
                "NsmDevice::getInventoryInformation id={ID} value={VALUE}",
                "ID", propertyIdentifier, "VALUE",
                (*propertyStringPtr).c_str());
        }
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDevice::refreshCapabilitySensor()
{
    size_t sensorIndex{0};
    auto& sensors = capabilityRefreshSensors;
    while (sensorIndex < sensors.size())
    {
        auto sensor = sensors[sensorIndex];
        co_await sensor->update(shared_from_this());
        ++sensorIndex;
    }
    lg2::info("NsmDevice::refreshCapabilitySensor refreshed for EID : {EID}",
              "EID", eid);
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDevice::refreshCommandMatrix()
{
    // Check if message types have already been retrieved
    if (!areMessageTypesRetrieved)
    {
        // Retrieve supported message types
        std::vector<uint8_t> supportedMessageTypes;
        auto rc = co_await getSupportedNvidiaMessageType(supportedMessageTypes);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to get supported message types, rc={RC}, eid={EID}",
                "RC", utils::nsmSwCodeToString(rc), "EID", eid);
            co_return rc;
        }

        areMessageTypesRetrieved = true;
        retrievedMessageTypes = supportedMessageTypes;
    }

    // Retrieve supported command codes for each message type
    for (uint8_t messageType : retrievedMessageTypes)
    {
        // Skip message types that are out of the valid range
        if (messageType >= NUM_NSM_TYPES)
        {
            continue;
        }

        if (!commandCodesRetrieved[messageType])
        {
            std::vector<uint8_t> supportedCommands;
            auto rc = co_await getSupportedCommandCodes(messageType,
                                                        supportedCommands);
            if (rc == NSM_SW_SUCCESS)
            {
                commandCodesRetrieved[messageType] = true;

                std::vector<uint8_t> supportedCommandCodes;
                utils::convertBitMaskToVector(
                    supportedCommandCodes,
                    reinterpret_cast<const bitfield8_t*>(&supportedCommands[0]),
                    SUPPORTED_COMMAND_CODE_DATA_SIZE);

                for (uint8_t commandCode : supportedCommandCodes)
                {
                    // Bounds-checked write — NvBug 5972182 mitigation.
                    (void)setCommandCodeSupportedSafe(messageType, commandCode,
                                                      true);
                }
            }
            else
            {
                lg2::error(
                    "Failed to get supported commands for message type={MT}, rc={RC}, eid={EID}",
                    "MT", messageType, "RC", utils::nsmSwCodeToString(rc),
                    "EID", eid);
                commandCodesRetrieved[messageType] = false;
            }
        }
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDevice::dumpNsmDeviceInfoTask()
{
    lg2::error(
        "###### NsmDevice::dumpNsmDeviceInfo: deviceType={DT}, deviceRole={ROLE}, static instanceNumber={INST} ######",
        "DT", getDeviceType(), "ROLE", getDeviceRole(), "INST",
        getInstanceNumber());
    lg2::error(
        "EID: {EID}, UUID: {UUID}, , Device Instance Number: {DEVICE_INSTANCE_NUMBER}, Device UUID: {DEVICE_UUID}, MCTP Medium: {MCTP_MEDIUM}, MCTP Binding: {MCTP_BINDING} Associated Path: {ASSOCIATED_PATH}",
        "EID", eid, "UUID", getUuid(), "DEVICE_UUID", deviceUuid, "MCTP_MEDIUM",
        mctpMedium, "MCTP_BINDING", mctpBinding, "DEVICE_INSTANCE_NUMBER",
        nsmDeviceInstanceNumber, "ASSOCIATED_PATH", associatedPath);
    lg2::error(
        "Device Active: {DEVICE_ACTIVE}, Device Ready: {DEVICE_READY}, Discovery Pending: {DISCOVERY_PENDING}",
        "DEVICE_ACTIVE", isDeviceActive, "DEVICE_READY", isDeviceReady,
        "DISCOVERY_PENDING", discoveryPending);

    std::string eventSubStatus;
    if (auto status = getLastEventSubscriptionStatus())
    {
        eventSubStatus = *status;
    }
    else if (hasNsmEventSettingConfig())
    {
        eventSubStatus = "pending";
    }
    else
    {
        eventSubStatus = "N/A (no NSM_EventSetting config)";
    }
    lg2::error("EID: {EID} Event subscription: {STATUS}", "EID", eid, "STATUS",
               eventSubStatus);
    if (lastEventSubscriptionRequest && !lastEventSubscriptionRequest->empty())
    {
        lg2::error(
            "EID: {EID} Set event subscription request (hex): {REQ}", "EID",
            eid, "REQ",
            utils::convertHexToString(*lastEventSubscriptionRequest,
                                      lastEventSubscriptionRequest->size()));
    }
    if (lastEventSubscriptionResponse &&
        !lastEventSubscriptionResponse->empty())
    {
        lg2::error(
            "EID: {EID} Set event subscription response (hex): {RESP}", "EID",
            eid, "RESP",
            utils::convertHexToString(*lastEventSubscriptionResponse,
                                      lastEventSubscriptionResponse->size()));
    }

    co_await mctp::MctpDiscovery::getInstance().dumpPingInfoTask(eid);
    co_await refreshCommandMatrix();

    for (size_t messageType = 0;
         messageType < messageTypesToCommandCodeMatrix.size(); messageType++)
    {
        std::stringstream ss;
        for (size_t commandCode = 0;
             commandCode < messageTypesToCommandCodeMatrix[messageType].size();
             commandCode++)
        {
            if (messageTypesToCommandCodeMatrix[messageType][commandCode])
            {
                ss << commandCode << " ";
            }
        }
        lg2::error(
            "EID: {EID} Message Type: {MESSAGE_TYPE}, Command Codes: {COMMAND_CODES}",
            "EID", eid, "MESSAGE_TYPE", messageType, "COMMAND_CODES", ss.str());
    }
    lg2::error(
        "###### NsmDevice::dumpNsmDeviceInfo End for deviceType={DT}, deviceRole={ROLE}, static instanceNumber={INST} ######",
        "DT", getDeviceType(), "ROLE", getDeviceRole(), "INST",
        getInstanceNumber());

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}
void NsmDevice::dumpNsmDeviceInfo()
{
    dumpNsmDeviceInfoTask().detach();
}

ProgressCounters& NsmDevice::progressCounters()
{
    if (!sensorProgressCounters)
    {
        sensorProgressCounters = std::make_shared<ProgressCounters>(eid);
    }
    return *sensorProgressCounters;
}

DiscoveryEvents& NsmDevice::discoveryEvents()
{
    try
    {
        return mctp::MctpDiscovery::getInstance().discoveryEvents(eid);
    }
    catch (const std::runtime_error&)
    {
        // Fallback for test environments where MctpDiscovery is not
        // initialized. Return a thread-local static instance.
        static thread_local DiscoveryEvents fallback(eid);
        return fallback;
    }
}

// FruInterfaceManager method implementations
bool FruInterfaceManager::needsRecreation(
    const std::set<std::string>& newProperties) const
{
    return !initialized || (supportedProperties != newProperties);
}

void FruInterfaceManager::markInitialized(
    const std::set<std::string>& properties)
{
    supportedProperties = properties;
    initialized = true;
}

bool FruInterfaceManager::isPropertySupported(
    const std::string& propertyName) const
{
    return supportedProperties.find(propertyName) != supportedProperties.end();
}

void FruInterfaceManager::reset(
    const std::shared_ptr<sdbusplus::asio::object_server>& objServer)
{
    if (interface)
    {
        if (objServer)
        {
            objServer->remove_interface(interface);
        }
        interface.reset();
    }
    supportedProperties.clear();
    initialized = false;
}

void FruInterfaceManager::createAndRegisterInterface(
    std::shared_ptr<sdbusplus::asio::object_server> objServer, uint8_t eid,
    std::shared_ptr<NsmDevice> nsmDevice, const InventoryProperties& properties)
{
    std::string objPath = "/xyz/openbmc_project/FruDevice/" +
                          std::to_string(eid);
    reset(objServer);
    interface = objServer->add_interface(objPath,
                                         "xyz.openbmc_project.FruDevice");
    registerAllProperties(nsmDevice, properties);
    interface->initialize();
}

void FruInterfaceManager::updateAllPropertyValues(
    std::shared_ptr<NsmDevice> nsmDevice, const InventoryProperties& properties)
{
    if (!interface)
        return;

    if (properties.find(BOARD_PART_NUMBER) != properties.end())
    {
        interface->set_property(
            "BOARD_PART_NUMBER",
            std::get<std::string>(properties.at(BOARD_PART_NUMBER)));
    }

    if (properties.find(FRU_PART_NUMBER) != properties.end())
    {
        interface->set_property(
            "FRU_PART_NUMBER",
            std::get<std::string>(properties.at(FRU_PART_NUMBER)));
    }

    if (properties.find(SERIAL_NUMBER) != properties.end())
    {
        interface->set_property(
            "SERIAL_NUMBER",
            std::get<std::string>(properties.at(SERIAL_NUMBER)));
    }

    if (properties.find(MARKETING_NAME) != properties.end())
    {
        interface->set_property(
            "MARKETING_NAME",
            std::get<std::string>(properties.at(MARKETING_NAME)));
    }

    if (properties.find(PRODUCT_NAME) != properties.end())
    {
        interface->set_property(
            "PRODUCT_NAME", std::get<std::string>(properties.at(PRODUCT_NAME)));
    }

    if (properties.find(ASSET_TAG) != properties.end())
    {
        interface->set_property(
            "ASSET_TAG", std::get<std::string>(properties.at(ASSET_TAG)));
    }

    if (properties.find(BUILD_DATE) != properties.end())
    {
        interface->set_property(
            "BUILD_DATE", std::get<std::string>(properties.at(BUILD_DATE)));
    }

    if (properties.find(DEVICE_GUID) != properties.end())
    {
        interface->set_property("DEVICE_UUID",
                                std::get<uuid_t>(properties.at(DEVICE_GUID)));
        nsmDevice->deviceUuid = std::get<uuid_t>(properties.at(DEVICE_GUID));
    }

    interface->set_property("DEVICE_TYPE", nsmDevice->getDeviceType());
    interface->set_property("INSTANCE_NUMBER", nsmDevice->getInstanceNumber());
    interface->set_property("DEVICE_ROLE", nsmDevice->getDeviceRole());
    interface->set_property("UUID", nsmDevice->getUuid());
}

void FruInterfaceManager::registerAllProperties(
    std::shared_ptr<NsmDevice> nsmDevice, const InventoryProperties& properties)
{
    if (properties.find(BOARD_PART_NUMBER) != properties.end())
    {
        interface->register_property(
            "BOARD_PART_NUMBER",
            std::get<std::string>(properties.at(BOARD_PART_NUMBER)));
    }

    if (properties.find(FRU_PART_NUMBER) != properties.end())
    {
        interface->register_property(
            "FRU_PART_NUMBER",
            std::get<std::string>(properties.at(FRU_PART_NUMBER)));
    }

    if (properties.find(SERIAL_NUMBER) != properties.end())
    {
        interface->register_property(
            "SERIAL_NUMBER",
            std::get<std::string>(properties.at(SERIAL_NUMBER)));
    }

    if (properties.find(MARKETING_NAME) != properties.end())
    {
        interface->register_property(
            "MARKETING_NAME",
            std::get<std::string>(properties.at(MARKETING_NAME)));
    }

    if (properties.find(PRODUCT_NAME) != properties.end())
    {
        interface->register_property(
            "PRODUCT_NAME", std::get<std::string>(properties.at(PRODUCT_NAME)));
    }

    if (properties.find(ASSET_TAG) != properties.end())
    {
        interface->register_property(
            "ASSET_TAG", std::get<std::string>(properties.at(ASSET_TAG)));
    }

    if (properties.find(BUILD_DATE) != properties.end())
    {
        interface->register_property(
            "BUILD_DATE", std::get<std::string>(properties.at(BUILD_DATE)));
    }

    if (properties.find(DEVICE_GUID) != properties.end())
    {
        interface->register_property(
            "DEVICE_UUID", std::get<uuid_t>(properties.at(DEVICE_GUID)));
        nsmDevice->deviceUuid = std::get<uuid_t>(properties.at(DEVICE_GUID));
    }

    interface->register_property("DEVICE_TYPE", nsmDevice->getDeviceType());
    interface->register_property("INSTANCE_NUMBER",
                                 nsmDevice->getInstanceNumber());
    interface->register_property("DEVICE_ROLE", nsmDevice->getDeviceRole());
    interface->register_property("UUID", nsmDevice->getUuid());
}

} // namespace nsm
