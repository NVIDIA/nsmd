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
#include "device-configuration.h"

#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <com/nvidia/ErrorInjection/ActivateErrorInjectionPayloadAsync/server.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>

namespace nsm
{

using ActivateErrorInjectionPayloadIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::ErrorInjection::server::
                                    ActivateErrorInjectionPayloadAsync>;

class NsmActivateErrorInjectionPayloadIntf :
    public ActivateErrorInjectionPayloadIntf,
    public StateChangeLogger
{
  public:
    NsmActivateErrorInjectionPayloadIntf(sdbusplus::bus::bus& bus,
                                         const char* path,
                                         std::shared_ptr<NsmDevice> device) :
        ActivateErrorInjectionPayloadIntf(bus, path), device(device)
    {}

    requester::Coroutine doActivateErrorInjectionPayload(
        std::shared_ptr<AsyncStatusIntf> statusInterface)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req_v2));
        auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_activate_error_injection_payload_req(0, requestPtr);

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "encode_activate_error_injection_payload_req failed. eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            statusInterface->status(AsyncOperationStatusType::WriteFailure);
            // coverity[missing_return]
            co_return rc;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = co_await manager.postPatchNsmCommand(eid, request, responseMsg,
                                                  responseLen);
        if (rc)
        {
            lg2::error(
                "NsmActivateErrorInjectionPayload::activatePayload: postPatchNsmCommand failed."
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            statusInterface->status(AsyncOperationStatusType::WriteFailure);
            // coverity[missing_return]
            co_return rc;
        }

        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;

        rc = decode_activate_error_injection_payload_resp(
            responseMsg.get(), responseLen, &cc, &reasonCode);
        LG2_ERROR_FLT(
            "NsmActivateErrorInjectionPayload::activatePayload failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);

        if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
        {
            statusInterface->status(AsyncOperationStatusType::WriteFailure);
        }
        else
        {
            statusInterface->status(AsyncOperationStatusType::Success);
        }
        // coverity[missing_return]
        co_return cc ? cc : rc;
    }

    sdbusplus::message::object_path activate() override
    {
        lg2::debug("NsmActivateErrorInjectionPayload::activate");
        const auto [objectPath, statusInterface, _] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        if (objectPath.empty())
        {
            lg2::error(
                "NsmActivateErrorInjectionPayload::activate failed. No available result Object to allocate for the Post request.");
            throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
        }

        doActivateErrorInjectionPayload(statusInterface).detach();

        return objectPath;
    }

  private:
    std::shared_ptr<NsmDevice> device;
};

} // namespace nsm
