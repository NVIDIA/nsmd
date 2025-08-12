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

#include "nsmEventConfig.hpp"

#include "device-capability-discovery.h"

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <unordered_map>

namespace nsm
{

NsmEventConfig::NsmEventConfig(const std::string& name, const std::string& type,
                               uint8_t messageType,
                               std::vector<uint64_t>& srcEventIds,
                               std::vector<uint64_t>& ackEventIds) :
    NsmObject(name, type), messageType(messageType), srcEventMask(8),
    ackEventMask(8)
{
    // convert id list to bitfield
    convertIdsToMask(srcEventIds, srcEventMask);
    convertIdsToMask(ackEventIds, ackEventMask);
}

void NsmEventConfig::convertIdsToMask(std::vector<uint64_t>& eventIds,
                                      std::vector<bitfield8_t>& bitfields)
{
    for (auto& value : bitfields)
    {
        value.byte = 0;
    }

    for (auto& value : eventIds)
    {
        uint8_t index = value / 8;
        uint8_t offset = value % 8;
        bitfields[index].byte |= (1 << offset);
    }
}

requester::Coroutine
    NsmEventConfig::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    uint8_t rc = NSM_SW_SUCCESS;
    rc = co_await setCurrentEventSources(nsmDevice, messageType, srcEventMask);
    if (rc != NSM_SW_SUCCESS)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error("setCurrentEventSources failed, eid={EID} rc={RC}",
                       "EID", nsmDevice->getEid(), "RC", rc);
        }
    }
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmEventConfig::setCurrentEventSources(
    std::shared_ptr<NsmDevice> nsmDevice, uint8_t nvidiaMessageType,
    std::vector<bitfield8_t>& eventIdMasks)
{
    if (eventIdMasks.size() != EVENT_SOURCES_LENGTH)
    {
        // coverity[missing_return]
        co_return NSM_ERR_INVALID_DATA_LENGTH;
    }

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_current_event_source_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_current_event_sources_req(
        0, nvidiaMessageType, eventIdMasks.data(), requestMsg);

    if (rc)
    {
        lg2::error(
            "encode_nsm_set_current_event_sources_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    rc = decode_nsm_set_current_event_sources_resp(responseMsg.get(),
                                                   responseLen, &cc);
    LG2_ERROR_FLT(
        "decode_nsm_set_current_event_sources_resp failed | cc: {CC}, rc: {RC}, eid: {EID}",
        "CC", nsm_completion_codes(cc), "RC", nsm_sw_codes(rc), "EID",
        nsmDevice->getEid());

    // coverity[missing_return]
    co_return cc;
}

requester::Coroutine NsmEventConfig::configureEventAcknowledgement(
    std::shared_ptr<NsmDevice> nsmDevice, uint8_t nvidiaMessageType,
    std::vector<bitfield8_t>& eventIdMasks)
{
    if (eventIdMasks.size() != EVENT_SOURCES_LENGTH)
    {
        // coverity[missing_return]
        co_return NSM_ERR_INVALID_DATA_LENGTH;
    }

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_configure_event_acknowledgement_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_configure_event_acknowledgement_req(
        0, nvidiaMessageType, eventIdMasks.data(), requestMsg);

    if (rc)
    {
        lg2::error(
            "encode_nsm_configure_event_acknowledgement_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    bitfield8_t* newEventIdMasks;

    uint8_t cc = NSM_SUCCESS;
    rc = decode_nsm_configure_event_acknowledgement_resp(
        responseMsg.get(), responseLen, &cc, &newEventIdMasks);
    if (rc)
    {
        lg2::error(
            "decode_nsm_configure_event_acknowledgement_resp failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
    }
    // coverity[missing_return]
    co_return cc;
}

NsmGetEventConfig::NsmGetEventConfig(
    const std::string& name, const std::string& type, uint8_t messageType,
    std::vector<uint64_t>& srcEventIds,
    std::shared_ptr<NsmEventConfig> eventConfig) :
    NsmObject(name, type), messageType(messageType), srcEventMask(8),
    eventConfig(eventConfig)
{
    // convert id list to bitfield
    convertIdsToMask(srcEventIds, srcEventMask);
}

void NsmGetEventConfig::convertIdsToMask(std::vector<uint64_t>& eventIds,
                                         std::vector<bitfield8_t>& bitfields)
{
    for (auto& value : bitfields)
    {
        value.byte = 0;
    }

    for (auto& value : eventIds)
    {
        uint8_t index = value / 8;
        uint8_t offset = value % 8;
        bitfields[index].byte |= (1 << offset);
    }
}

requester::Coroutine
    NsmGetEventConfig::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_nsm_get_supported_event_source_req(
        nsmDevice->getEid(), messageType, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_nsm_get_supported_event_source_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmGetEventConfig SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    bitfield8_t supported_event_sources[EVENT_SOURCES_LENGTH];
    rc = decode_nsm_get_supported_event_source_resp(
        responseMsg.get(), responseLen, &cc, &reason_code,
        &supported_event_sources[0]);

    LG2_ERROR_FLT(
        "decode_nsm_get_supported_event_source_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reason_code, "CC", cc, "RC", rc);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        bool validationPassed = validateEventIds(nsmDevice->getEid(),
                                                 supported_event_sources);
        if (!validationPassed)
        {
            co_await eventConfig->update(nsmDevice);
        }
    }
    // coverity[missing_return]
    co_return cc;
}

bool NsmGetEventConfig::validateEventIds(
    eid_t eid, bitfield8_t supported_event_sources[EVENT_SOURCES_LENGTH])
{
    bool validateEventSource = true;

    for (size_t byteIndex = 0; byteIndex < EVENT_SOURCES_LENGTH; byteIndex++)
    {
        uint8_t byte = srcEventMask[byteIndex].byte;
        for (uint8_t bitOffset = 0; bitOffset < 8; bitOffset++)
        {
            if (byte & (1 << bitOffset))
            {
                uint64_t eventId = (byteIndex * 8) + bitOffset;

                // Check if this configured event is supported
                uint8_t supportedByte = supported_event_sources[byteIndex].byte;
                bool validateEventSourceForEventId =
                    !(supportedByte & (1 << bitOffset));
                if (validateEventSourceForEventId)
                {
                    validateEventSource = false;
                }
                if (shouldLog("NsmGetEventConfig::validateEventIds " +
                                  std::to_string(eventId),
                              validateEventSourceForEventId))
                {
                    LG2_ERROR(
                        "EID: {EID} Configured event ID {ID} for Message Type {MSG_TYPE} is not supported",
                        "EID", eid, "ID", eventId, "MSG_TYPE", messageType);
                }
            }
        }
    }

    return validateEventSource;
}

static requester::Coroutine createNsmEventConfig(SensorManager& manager,
                                                 const std::string& interface,
                                                 const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_EventConfig";

    dbus::PropertyMap allBaseIfaceProperties;
    auto rc = co_await utils::coGetCachedBaseProperties(objPath, baseInterface,
                                                        allBaseIfaceProperties);
    if (rc != NSM_SUCCESS)
    {
        co_return rc;
    }

    std::string name{};
    if (allBaseIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allBaseIfaceProperties.at("Name"));
    }
    auto type = interface.substr(interface.find_last_of('.') + 1);
    uuid_t uuid{};
    if (allBaseIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
    }
    uint64_t messageType{};
    if (allBaseIfaceProperties.count("MessageType"))
    {
        messageType =
            std::get<uint64_t>(allBaseIfaceProperties.at("MessageType"));
    }
    std::vector<uint64_t> subscribedEventIds{};
    if (allBaseIfaceProperties.count("SubscribedEventIDs"))
    {
        subscribedEventIds = std::get<std::vector<uint64_t>>(
            allBaseIfaceProperties.at("SubscribedEventIDs"));
    }

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "found NSM_EventConfig [{NAME}] but not applied since no NsmDevice UUID={UUID}",
            "NAME", name, "UUID", uuid);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    // ack support to be added in future. JIRA
    // https://jirasw.nvidia.com/browse/DGXOPENBMC-11623
    std::vector<uint64_t> ackIds{};
    auto sensor = std::make_shared<NsmEventConfig>(name, type, messageType,
                                                   subscribedEventIds, ackIds);
    auto getEventSensor = std::make_shared<NsmGetEventConfig>(
        name, type, messageType, subscribedEventIds, sensor);

    nsmDevice->addCapabilityRefreshSensor(sensor);
    // update sensor
    nsmDevice->addStaticSensor(sensor);
    nsmDevice->addSensor(getEventSensor, false);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmEventConfig, "xyz.openbmc_project.Configuration.NSM_EventConfig")

} // namespace nsm
