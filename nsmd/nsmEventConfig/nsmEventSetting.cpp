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

requester::Coroutine NsmEventSetting::update(SensorManager& manager, eid_t eid)
{
    uint8_t rc = NSM_SW_SUCCESS;
    auto localEid = manager.getLocalEid();
    rc = co_await setEventSubscription(manager, eid, eventGenerationSetting,
                                       localEid);
    if (rc != NSM_SW_SUCCESS)
    {
        if (rc != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error("setEventSubscription failed, eid={EID} rc={RC}", "EID",
                       eid, "RC", rc);
        }
    }
    nsmDevice->setEventMode(eventGenerationSetting);
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine
    NsmEventSetting::setEventSubscription(SensorManager& manager, eid_t eid,
                                          uint8_t globalSettting,
                                          eid_t receiverEid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_event_subscription_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_event_subscription_req(0, globalSettting,
                                                    receiverEid, requestMsg);

    if (rc)
    {
        lg2::error(
            "encode_nsm_set_event_subscription_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
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
    rc = decode_nsm_set_event_subscription_resp(responseMsg.get(), responseLen,
                                                &cc);
    if (rc)
    {
        lg2::error(
            "decode_nsm_set_event_subscription_resp failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
    }
    // coverity[missing_return]
    co_return cc;
}

static requester::Coroutine createNsmEventSetting(SensorManager& manager,
                                                  const std::string& interface,
                                                  const std::string& objPath)
{
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name",
        "xyz.openbmc_project.Configuration.NSM_EventSetting");
    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID",
        "xyz.openbmc_project.Configuration.NSM_EventSetting");
    auto eventGenerationSetting = co_await utils::coGetDbusProperty<uint64_t>(
        objPath.c_str(), "EventGenerationSetting",
        "xyz.openbmc_project.Configuration.NSM_EventSetting");

    auto nsmDevice = manager.getNsmDevice(uuid);
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
    nsmDevice->capabilityRefreshSensors.emplace_back(sensor);

    // update sensor
    nsmDevice->addStaticSensor(sensor);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmEventSetting, "xyz.openbmc_project.Configuration.NSM_EventSetting")

} // namespace nsm
