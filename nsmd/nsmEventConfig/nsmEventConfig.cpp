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

#include "dBusAsyncUtils.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

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

requester::Coroutine NsmEventConfig::update(SensorManager& manager, eid_t eid)
{
    uint8_t rc = NSM_SW_SUCCESS;
    rc = co_await setCurrentEventSources(manager, eid, messageType,
                                         srcEventMask);
    if (rc != NSM_SW_SUCCESS)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error("setCurrentEventSources failed, eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);
        }
    }
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmEventConfig::setCurrentEventSources(
    SensorManager& manager, eid_t eid, uint8_t nvidiaMessageType,
    std::vector<bitfield8_t>& eventIdMasks)
{
    if (eventIdMasks.size() != EVENT_SOURCES_LENGTH)
    {
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
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    rc = decode_nsm_set_current_event_sources_resp(responseMsg.get(),
                                                   responseLen, &cc);
    if (rc)
    {
        lg2::error(
            "decode_nsm_set_current_event_sources_resp failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
    }

    co_return cc;
}

requester::Coroutine NsmEventConfig::configureEventAcknowledgement(
    SensorManager& manager, eid_t eid, uint8_t nvidiaMessageType,
    std::vector<bitfield8_t>& eventIdMasks)
{
    if (eventIdMasks.size() != EVENT_SOURCES_LENGTH)
    {
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
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
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
            "EID", eid, "RC", rc);
    }

    co_return cc;
}

static requester::Coroutine createNsmEventConfig(SensorManager& manager,
                                                 const std::string& interface,
                                                 const std::string& objPath)
{
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name",
        "xyz.openbmc_project.Configuration.NSM_EventConfig");
    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID",
        "xyz.openbmc_project.Configuration.NSM_EventConfig");
    auto messageType = co_await utils::coGetDbusProperty<uint64_t>(
        objPath.c_str(), "MessageType",
        "xyz.openbmc_project.Configuration.NSM_EventConfig");
    auto subscribedEventIds =
        co_await utils::coGetDbusProperty<std::vector<uint64_t>>(
            objPath.c_str(), "SubscribedEventIDs",
            "xyz.openbmc_project.Configuration.NSM_EventConfig");

    auto nsmDevice = manager.getNsmDevice(uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "found NSM_EventConfig [{NAME}] but not applied since no NsmDevice UUID={UUID}",
            "NAME", name, "UUID", uuid);
        co_return NSM_ERROR;
    }

    // ack support to be added in future. JIRA
    // https://jirasw.nvidia.com/browse/DGXOPENBMC-11623
    std::vector<uint64_t> ackIds{};
    auto sensor = std::make_shared<NsmEventConfig>(name, type, messageType,
                                                   subscribedEventIds, ackIds);
    nsmDevice->capabilityRefreshSensors.emplace_back(sensor);

    // update sensor
    nsmDevice->addStaticSensor(sensor);

    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmEventConfig, "xyz.openbmc_project.Configuration.NSM_EventConfig")

} // namespace nsm
