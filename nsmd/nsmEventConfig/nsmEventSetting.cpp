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

#include "device-capability-discovery.h"

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
        lg2::error("setEventSubscription failed, eid={EID} rc={RC}", "EID", eid,
                   "RC", rc);
        co_return rc;
    }
    nsmDevice->setEventMode(eventGenerationSetting);
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
        co_return rc;
    }

    const nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, &responseMsg,
                                         &responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    rc = decode_nsm_set_event_subscription_resp(responseMsg, responseLen, &cc);
    if (rc)
    {
        lg2::error(
            "decode_nsm_set_event_subscription_resp failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    co_return cc;
}

static void createNsmEventSetting(SensorManager& manager,
                                  const std::string& interface,
                                  const std::string& objPath)
{
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name",
        "xyz.openbmc_project.Configuration.NSM_EventSetting");
    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID",
        "xyz.openbmc_project.Configuration.NSM_EventSetting");
    auto eventGenerationSetting =
        utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "EventGenerationSetting",
            "xyz.openbmc_project.Configuration.NSM_EventSetting");

    auto nsmDevice = manager.getNsmDevice(uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "found NSM_EventSetting [{NAME}] but not applied since no NsmDevice UUID={UUID}",
            "NAME", name, "UUID", uuid);
        return;
    }

    if (eventGenerationSetting > GLOBAL_EVENT_GENERATION_ENABLE_PUSH)
    {
        lg2::error(
            "NSM_EventSetting: Found an invalid setting={SETTING} for eventGenerationSetting",
            "SETTING", eventGenerationSetting);
        return;
    }

    auto sensor = std::make_shared<NsmEventSetting>(
        name, type, eventGenerationSetting, nsmDevice);
    nsmDevice->deviceSensors.emplace_back(sensor);

    // update sensor
    auto eid = manager.getEid(nsmDevice);
    sensor->update(manager, eid).detach();
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmEventSetting, "xyz.openbmc_project.Configuration.NSM_EventSetting")

} // namespace nsm
