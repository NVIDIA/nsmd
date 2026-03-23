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

#include "nsmEventSetting.hpp"

#include "base.h"
#include "device-capability-discovery.h"

#include "dBusAsyncUtils.hpp"
#include "nsmObjectFactory.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmEventSetting::NsmEventSetting(const std::string& name,
                                 const std::string& type,
                                 uint8_t eventGenerationSetting,
                                 std::shared_ptr<NsmDevice> nsmDevice) :
    NsmObject(name, type), eventGenerationSetting(eventGenerationSetting),
    nsmDevice(nsmDevice)
{}

requester::Coroutine
    NsmEventSetting::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    uint8_t rc = NSM_SW_SUCCESS;
    rc = co_await setEventSubscription(nsmDevice);
    if (rc != NSM_SW_SUCCESS)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error("setEventSubscription failed, eid={EID} rc={RC}", "EID",
                       nsmDevice->getEid(), "RC", rc);
        }
    }
    nsmDevice->setEventMode(eventGenerationSetting);
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine
    NsmEventSetting::setEventSubscription(std::shared_ptr<NsmDevice> nsmDevice)
{
    auto localEidOpt = nsmDevice->getLocalEid();
    if (!localEidOpt)
    {
        lg2::error(
            "setEventSubscription skipped: localEid not set for eid={EID} (LocalEID not from MCTP)",
            "EID", nsmDevice->getEid());
        co_return NSM_ERR_INVALID_DATA;
    }
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_event_subscription_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_event_subscription_req(0, eventGenerationSetting,
                                                    *localEidOpt, requestMsg);

    if (rc)
    {
        lg2::error(
            "encode_nsm_set_event_subscription_req failed. eid={EID} rc={RC}",
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
    rc = decode_nsm_set_event_subscription_resp(responseMsg.get(), responseLen,
                                                &cc);
    if (rc)
    {
        lg2::error(
            "decode_nsm_set_event_subscription_resp failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmGetEventSetting::NsmGetEventSetting(
    const std::string& name, const std::string& type,
    std::shared_ptr<NsmEventSetting> eventSetting) :
    NsmObject(name, type), eventSetting(eventSetting)
{}

requester::Coroutine
    NsmGetEventSetting::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_nsm_get_event_subscription_req(0, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_nsm_get_event_subscription_req failed. "
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
    auto localEidOpt = nsmDevice->getLocalEid();
    if (!localEidOpt)
    {
        lg2::error(
            "NsmGetEventSetting update skipped: localEid not set for eid={EID} (LocalEID not from MCTP)",
            "EID", nsmDevice->getEid());
        co_return NSM_ERR_INVALID_DATA;
    }
    auto localEid = *localEidOpt;
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint8_t receiver_eid = 0;
    rc = decode_nsm_get_event_subscription_resp(
        responseMsg.get(), responseLen, &cc, &reason_code, &receiver_eid);

    // if the response is not successful or receiver_eid is not equal to
    // localEid, we need to call setEventSubscription
    if ((cc != NSM_SUCCESS && rc == NSM_SW_SUCCESS) || receiver_eid != localEid)
    {
        lg2::error(
            "NsmGetEventSetting update failed, cc={CC}, rc={RC}, receiver_eid={RECEIVER_EID}, localEid={LOCAL_EID}",
            "CC", cc, "RC", rc, "RECEIVER_EID", receiver_eid, "LOCAL_EID",
            localEid);
        rc = co_await eventSetting->setEventSubscription(nsmDevice);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("setEventSubscription failed, eid={EID} rc={RC}", "EID",
                       nsmDevice->getEid(), "RC", rc);
        }
    }

    // coverity[missing_return]
    co_return cc ? cc : rc;
}

requester::Coroutine createNsmEventSetting(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath)
{
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(),
        "xyz.openbmc_project.Configuration.NSM_EventSetting");

    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    auto type = interface.substr(interface.find_last_of('.') + 1);
    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    uint64_t eventGenerationSetting{};
    if (allCurrentIfaceProperties.count("EventGenerationSetting"))
    {
        eventGenerationSetting = std::get<uint64_t>(
            allCurrentIfaceProperties.at("EventGenerationSetting"));
    }

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "found NSM_EventSetting [{NAME}] but not applied since no NsmDevice UUID={UUID}",
            "NAME", name, "UUID", uuid);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    if (eventGenerationSetting > GLOBAL_EVENT_GENERATION_ENABLE_PUSH)
    {
        lg2::error(
            "NSM_EventSetting: Found an invalid setting={SETTING} for eventGenerationSetting",
            "SETTING", eventGenerationSetting);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto sensor = std::make_shared<NsmEventSetting>(
        name, type, eventGenerationSetting, nsmDevice);
    auto getEventSensor = std::make_shared<NsmGetEventSetting>(name, type,
                                                               sensor);
    nsmDevice->addCapabilityRefreshSensor(sensor);

    // update sensor
    nsmDevice->addStaticSensor(sensor);
    nsmDevice->addSensor(getEventSensor, false);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmEventSetting, "xyz.openbmc_project.Configuration.NSM_EventSetting")

} // namespace nsm
