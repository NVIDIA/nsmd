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

#include "nsmEvent/nsmFabricManagerStateEvent.hpp"
#include "nsmEvent/nsmLongRunningEventHandler.hpp"
#include "nsmLongRunning/nsmLongRunningSensor.hpp"
#include "nsmNumericSensor/nsmNumericAggregator.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

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

int parseStaticUuid(uuid_t& uuid, uint8_t& deviceType, uint8_t& instanceNumber)
{
    int n1 = -1;
    int n2 = -1;

    std::sscanf(uuid.c_str(), "STATIC:%d:%d", &n1, &n2);

    if (n1 < 0 || n1 > 0xff)
    {
        return -1;
    }
    if (n2 < 0 || n2 > 0xff)
    {
        return -2;
    }

    deviceType = n1;
    instanceNumber = n2;
    return 0;
}

std::shared_ptr<NsmDevice>
    findNsmDeviceByIdentification(NsmDeviceTable& nsmDevices,
                                  uint8_t deviceType, uint8_t instanceNumber)
{
    std::shared_ptr<NsmDevice> ret{};

    for (auto nsmDevice : nsmDevices)
    {
        if (nsmDevice->getDeviceType() == deviceType &&
            nsmDevice->getInstanceNumber() == instanceNumber)
        {
            ret = nsmDevice;
            break;
        }
    }
    return ret;
}

std::shared_ptr<NsmDevice> findNsmDeviceByUUID(NsmDeviceTable& nsmDevices,
                                               const uuid_t& uuid)
{
    std::shared_ptr<NsmDevice> ret{};

    for (auto nsmDevice : nsmDevices)
    {
        if ((nsmDevice->uuid).substr(0, UUID_LEN) == uuid.substr(0, UUID_LEN))
        {
            ret = nsmDevice;
            break;
        }
    }
    return ret;
}

void NsmDevice::setEventMode(uint8_t mode)
{
    // TODO: Add CC and RC error log tracking to prevent log flooding.
    // Track issue: "Refactor error handling and logging in NSM components" MR.
    // Link: https://gitlab-master.nvidia.com/dgx/bmc/nsmd/-/merge_requests/527
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

bool NsmDevice::isCommandSupported(uint8_t messageType, uint8_t commandCode)
{
    bool supported = false;
    if (messageTypesToCommandCodeMatrix[messageType][commandCode])
    {
        supported = true;
    }
    return supported;
}

NsmObject& NsmDevice::addStaticSensor(std::shared_ptr<NsmObject> sensor)
{
    sensor->isStatic = true;
    deviceSensors.emplace_back(sensor);
    roundRobinSensors.emplace_back(sensor);
    return *sensor;
}

void NsmDevice::addSensor(const std::shared_ptr<NsmObject>& sensor,
                          bool priority, bool isLongRunning)
{
    std::string deviceInstanceName =
        utils::getDeviceInstanceName(getDeviceType(), getInstanceNumber());
    sensor->setDeviceIdentifier(deviceInstanceName);

    deviceSensors.emplace_back(sensor);
    if (isLongRunning)
    {
        longRunningSensors.emplace_back(sensor);
    }
    else
    {
        if (priority)
        {
            prioritySensors.emplace_back(sensor);
        }
        else
        {
            roundRobinSensors.emplace_back(sensor);
        }
    }
}

void NsmDevice::setOnline()
{
    isDeviceActive = true;
    lg2::info(
        "NSMDevice: deviceType:{DEVTYPE} InstanceNumber:{INSTNUM} gets online",
        "DEVTYPE", getDeviceType(), "INSTNUM", getInstanceNumber());
    isDeviceReady = false;
    NsmServiceReadyIntf::getInstance().setStateStarting();

    for (auto sensor : roundRobinSensors)
    {
        // Mark all the sensors as unrefreshed.
        sensor->isRefreshed = false;
    }
}

void NsmDevice::setOffline()
{
    isDeviceActive = false;
    lg2::info(
        "NSMDevice: deviceType:{DEVTYPE} InstanceNumber:{INSTNUM} gets offline",
        "DEVTYPE", getDeviceType(), "INSTNUM", getInstanceNumber());

    size_t sensorIndex{0};
    auto& sensors = deviceSensors;
    while (sensorIndex < sensors.size())
    {
        auto sensor = sensors[sensorIndex];
        sensor->handleOfflineState();
        ++sensorIndex;
    }
}

NsmLongRunningEventHandler& NsmDevice::registerLongRunningEventHandler()
{
    auto longRunningEventHandler =
        std::make_shared<NsmLongRunningEventHandler>();
    deviceEvents.emplace_back(longRunningEventHandler);
    eventDispatcher.addEvent(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                             NSM_LONG_RUNNING_EVENT, longRunningEventHandler);
    return *longRunningEventHandler;
}

void NsmDevice::registerLongRunningHandler(
    uint8_t messageType, uint8_t commandCode,
    std::shared_ptr<NsmLongRunningEvent> sensorInstance)
{
    clearLongRunningHandler();
    lg2::debug(
        "Registering long-running handler for MessageType={MT}, CommandCode={CC}",
        "MT", messageType, "CC", commandCode);

    longRunningHandler = ActiveLongRunningHandlerInfo{messageType, commandCode,
                                                      sensorInstance};
}

void NsmDevice::clearLongRunningHandler()
{
    if (!longRunningHandler.has_value())
    {
        return;
    }

    lg2::debug(
        "Clearing long-running handler for MessageType={MT}, CommandCode={CC}",
        "MT", longRunningHandler->messageType, "CC",
        longRunningHandler->commandCode);

    longRunningHandler.reset();
}

std::optional<ActiveLongRunningHandlerInfo>
    NsmDevice::getActiveLongRunningHandler() const
{
    return longRunningHandler;
}

int NsmDevice::invokeLongRunningHandler(eid_t eid, NsmType type,
                                        NsmEventId eventId,
                                        const nsm_msg* event, size_t eventLen)
{
    // TODO: Add CC and RC error log tracking to prevent log flooding.
    // Track issue: "Refactor error handling and logging in NSM components" MR.
    // Link: https://gitlab-master.nvidia.com/dgx/bmc/nsmd/-/merge_requests/527
    if (!longRunningHandler.has_value())
    {
        lg2::debug(
            "NsmDevice::invokeLongRunningHandler: No active handler registered for long-running event, EID={EID}",
            "EID", eid);
        return NSM_SW_ERROR_DATA;
    }

    uint16_t eventState = 0;
    uint8_t dataSize = 0;
    auto rc = decode_nsm_event(event, eventLen, eventId,
                               NSM_NVIDIA_GENERAL_EVENT_CLASS, &eventState,
                               &dataSize);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmLongRunningEventHandler : Failed to decode long running event state : EID={EID}",
            "EID", eid);
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

} // namespace nsm
