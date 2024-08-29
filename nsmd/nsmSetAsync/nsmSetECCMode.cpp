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

#include "nsmSetECCMode.hpp"

#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
requester::Coroutine setECCModeOnDevice(const bool isLongRunning, bool migMode,
                                        AsyncOperationStatusType* status,
                                        std::shared_ptr<NsmDevice> device);

requester::Coroutine
    setECCModeEnabled(const bool isLongRunning,
                      const AsyncSetOperationValueType& value,
                      [[maybe_unused]] AsyncOperationStatusType* status,
                      std::shared_ptr<NsmDevice> device)
{
    const bool* eccMode = std::get_if<bool>(&value);

    if (!eccMode)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    const auto rc = co_await setECCModeOnDevice(isLongRunning, *eccMode, status,
                                                device);

    co_return rc;
}

requester::Coroutine
    setECCModeOnDevice(const bool isLongRunning, bool eccMode,
                       [[maybe_unused]] AsyncOperationStatusType* status,
                       std::shared_ptr<NsmDevice> device)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("setECCModeOnDevice for EID: {EID}", "EID", eid);
    // NSM spec expects  requestedECCMode mode to be uint8_t
    uint8_t requestedECCMode = static_cast<uint8_t>(eccMode);
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_ECC_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // first argument instanceid=0 is irrelevant
    auto rc = encode_set_ECC_mode_req(0, requestedECCMode, requestMsg);

    if (rc)
    {
        lg2::error(
            "setECCModeOnDevice encode_set_ECC_mode_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen, isLongRunning);
    if (rc_)
    {
        if (rc_ != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            lg2::error(
                "setECCModeOnDevice SendRecvNsmMsg failed for while setting ECCMode "
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc_);
        }
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_set_ECC_mode_resp(responseMsg.get(), responseLen, &cc,
                                  &data_size, &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info("setECCModeOnDevice for EID: {EID} completed", "EID", eid);
    }
    else
    {
        lg2::error(
            "setECCModeOnDevice decode_set_ECC_mode_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        lg2::error("throwing write failure exception");
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}
} // namespace nsm
