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
#include "diagnostics.h"
#include "platform-environmental.h"

#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <com/nvidia/Common/ResetEdppAsync/server.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>

namespace nsm
{

using ResetEdppAsyncIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::common::ResetEdppAsync>;

class NsmResetEdppAsyncIntf :
    public ResetEdppAsyncIntf,
    public StateChangeLogger
{
  public:
    NsmResetEdppAsyncIntf(sdbusplus::bus::bus& bus, const char* path,
                          std::shared_ptr<NsmDevice> device) :
        ResetEdppAsyncIntf(bus, path), device(device) {};

    requester::Coroutine clearSetPoint(AsyncOperationStatusType* status)
    {
        auto eid = device->getEid();
        lg2::info("Reset EDPp setpoint On Device for EID: {EID}", "EID", eid);

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_programmable_EDPp_scaling_factor_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

        auto rc = encode_set_programmable_EDPp_scaling_factor_req(
            0, RESET_TO_DEFAULT, PERSISTENT, 0, requestMsg);

        if (rc)
        {
            lg2::error(
                "NsmResetEdppAsyncIntf::clearSetPoint  failed. eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto rc_ = co_await device->postPatchIO(eid, request, responseMsg,
                                                responseLen);
        if (rc_)
        {
            lg2::error(
                "NsmResetEdppAsyncIntf::clearSetPoint postPatchIO failed for  "
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc_);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint16_t dataSize = 0;
        rc = decode_set_programmable_EDPp_scaling_factor_resp(
            responseMsg.get(), responseLen, &cc, &reasonCode, &dataSize);

        LG2_ERROR_FLT(
            "decode_set_programmable_EDPp_scaling_factor_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            *status = AsyncOperationStatusType::WriteFailure;
        }

        co_return cc ? cc : rc;
    };

    requester::Coroutine
        doResetEdppOnDevice(std::shared_ptr<AsyncStatusIntf> statusInterface)
    {
        AsyncOperationStatusType status{AsyncOperationStatusType::Success};

        auto rc_ = co_await clearSetPoint(&status);

        statusInterface->status(status);

        co_return rc_;
    };

    sdbusplus::message::object_path reset() override
    {
        const auto [objectPath, statusInterface, _] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        if (objectPath.empty())
        {
            lg2::error(
                "Edpp Reset failed. No available result Object to allocate for the Post Request.");
            throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
        }

        doResetEdppOnDevice(statusInterface).detach();

        return objectPath;
    };

  private:
    std::shared_ptr<NsmDevice> device;
};

} // namespace nsm
