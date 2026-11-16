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
#include "nsmEventConfigFeatures.hpp"
#include "nsmObjectFactory.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace
{
std::optional<std::vector<uint8_t>>
    copySubscriptionResponse(const std::shared_ptr<const nsm_msg>& responseMsg,
                             size_t responseLen)
{
    if (!responseMsg || responseLen == 0)
    {
        return std::nullopt;
    }
    const auto* bytes = reinterpret_cast<const uint8_t*>(responseMsg.get());
    return std::vector<uint8_t>(bytes, bytes + responseLen);
}
} // namespace

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
    // recordEventSubscriptionStatus logs failure when rc != NSM_SW_SUCCESS
    nsmDevice->setEventMode(eventGenerationSetting);
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine
    NsmEventSetting::setEventSubscription(std::shared_ptr<NsmDevice> nsmDevice)
{
    auto localEidOpt = nsmDevice->getMctpLocalEid();
    if (!localEidOpt)
    {
        nsmDevice->recordEventSubscriptionStatus(
            "skipped: localEid not set (LocalEID not from MCTP)", std::nullopt,
            std::nullopt);
        co_return NSM_ERR_INVALID_DATA;
    }
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_event_subscription_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_event_subscription_req(0, eventGenerationSetting,
                                                    *localEidOpt, requestMsg);

    if (rc)
    {
        nsmDevice->recordEventSubscriptionStatus(
            "failed: encode rc=" + std::to_string(rc), Request(request),
            std::nullopt);
        co_return rc;
    }

    // sensorIO moves request bytes into the requester; keep a copy for logDump.
    const Request requestSnapshot(request);

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        nsmDevice->recordEventSubscriptionStatus("failed: sensorIO rc=" +
                                                     std::to_string(rc),
                                                 requestSnapshot, std::nullopt);
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    rc = decode_nsm_set_event_subscription_resp(responseMsg.get(), responseLen,
                                                &cc);
    if (rc)
    {
        nsmDevice->recordEventSubscriptionStatus(
            "failed: decode rc=" + std::to_string(rc), requestSnapshot,
            copySubscriptionResponse(responseMsg, responseLen));
        co_return rc;
    }
    if (cc != NSM_SUCCESS)
    {
        if (cc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            nsmDevice->recordEventSubscriptionStatus(
                "failed: unsupported command", requestSnapshot,
                copySubscriptionResponse(responseMsg, responseLen));
        }
        else
        {
            nsmDevice->recordEventSubscriptionStatus(
                "failed: device cc=" + std::to_string(cc), requestSnapshot,
                copySubscriptionResponse(responseMsg, responseLen));
        }
        // coverity[missing_return]
        co_return cc;
    }
    nsmDevice->recordEventSubscriptionStatus(
        "OK (localEid=" + std::to_string(*localEidOpt) + ")", requestSnapshot,
        copySubscriptionResponse(responseMsg, responseLen));
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
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
        nsmDevice->recordEventSubscriptionStatus("failed: get encode rc=" +
                                                 std::to_string(rc));
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        nsmDevice->recordEventSubscriptionStatus("failed: get sensorIO rc=" +
                                                 std::to_string(rc));
        co_return rc;
    }
    auto localEidOpt = nsmDevice->getMctpLocalEid();
    if (!localEidOpt)
    {
        nsmDevice->recordEventSubscriptionStatus(
            "skipped: localEid not set (get path)");
        co_return NSM_ERR_INVALID_DATA;
    }
    auto localEid = *localEidOpt;
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint8_t receiver_eid = 0;
    rc = decode_nsm_get_event_subscription_resp(
        responseMsg.get(), responseLen, &cc, &reason_code, &receiver_eid);

    if (rc != NSM_SW_SUCCESS)
    {
        nsmDevice->recordEventSubscriptionStatus("failed: get decode rc=" +
                                                 std::to_string(rc));
        co_return rc;
    }

    // if the response is not successful or receiver_eid is not equal to
    // localEid, we need to call setEventSubscription
    if ((cc != NSM_SUCCESS && rc == NSM_SW_SUCCESS) || receiver_eid != localEid)
    {
        lg2::error(
            "NsmGetEventSetting update failed, cc={CC}, rc={RC}, receiver_eid={RECEIVER_EID}, localEid={LOCAL_EID}",
            "CC", cc, "RC", rc, "RECEIVER_EID", receiver_eid, "LOCAL_EID",
            localEid);
        rc = co_await eventSetting->setEventSubscription(nsmDevice);
        // recordEventSubscriptionStatus logs failure if setEventSubscription
        // fails
    }
    else
    {
        nsmDevice->recordEventSubscriptionStatus(
            "OK (localEid=" + std::to_string(localEid) + ")");
    }

    // coverity[missing_return]
    co_return cc ? cc : rc;
}

} // namespace nsm

namespace nsm::features
{

requester::Coroutine
    createEventSettingFeature([[maybe_unused]] SensorManager& manager,
                              const DeviceContext& dev,
                              const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("EventSetting feature of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }

    auto name = std::get<std::string>(params.at("Name"));
    auto eventGenerationSetting =
        param<uint64_t>(params, "EventGenerationSetting").value_or(0);
    if (eventGenerationSetting > GLOBAL_EVENT_GENERATION_ENABLE_PUSH)
    {
        lg2::error(
            "NSM_EventSetting: Found an invalid setting={SETTING} for eventGenerationSetting",
            "SETTING", eventGenerationSetting);
        co_return NSM_ERROR;
    }

    const std::string type = "NSM_EventSetting";
    auto sensor = std::make_shared<NsmEventSetting>(
        name, type, eventGenerationSetting, dev.device);
    auto getEventSensor = std::make_shared<NsmGetEventSetting>(name, type,
                                                               sensor);
    dev.device->setHasNsmEventSettingConfig(true);
    dev.device->addCapabilityRefreshSensor(sensor);

    dev.device->addStaticSensor(sensor);
    dev.device->addSensor(getEventSensor, false);
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
