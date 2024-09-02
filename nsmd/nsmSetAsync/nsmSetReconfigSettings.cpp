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

#include "nsmSetReconfigSettings.hpp"

#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace nsm
{

NsmSetReconfigSettings::NsmSetReconfigSettings(
    const std::string& name, SensorManager& manager, std::string objPath,
    reconfiguration_permissions_v1_index settingIndex) :
    NsmInterfaceProvider<ReconfigSettingsIntf>(
        name, "NSM_InbandReconfigSettings", dbus::Interfaces{objPath}),
    manager(manager), settingIndex(settingIndex)
{}

requester::Coroutine NsmSetReconfigSettings::allowOneShotConfig(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const auto allowValue = std::get_if<bool>(&value);

    if (!allowValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    // coverity[missing_return]
    co_return co_await setAllowPermission(RP_ONESHOOT_HOT_RESET, *allowValue,
                                          *status, device);
}
requester::Coroutine NsmSetReconfigSettings::allowPersistentConfig(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const auto allowValue = std::get_if<bool>(&value);

    if (!allowValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    // coverity[missing_return]
    co_return co_await setAllowPermission(RP_PERSISTENT, *allowValue, *status,
                                          device);
}
requester::Coroutine NsmSetReconfigSettings::allowFLRPersistentConfig(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const auto allowValue = std::get_if<bool>(&value);

    if (!allowValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    // coverity[missing_return]
    co_return co_await setAllowPermission(RP_ONESHOT_FLR, *allowValue, *status,
                                          device);
}

requester::Coroutine NsmSetReconfigSettings::setAllowPermission(
    reconfiguration_permissions_v1_setting configuration, const bool value,
    AsyncOperationStatusType& status, std::shared_ptr<NsmDevice> device)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));

    auto eid = manager.getEid(device);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        0, settingIndex, configuration, value, requestPtr);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_set_reconfiguration_permissions_v1_req({SI}) failed. eid={EID} rc={RC}",
            "SI", (int)settingIndex, "EID", eid, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "NsmSetReconfigSettings::setAllowPermission: SendRecvNsmMsgSync failed."
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_set_reconfiguration_permissions_v1_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);
    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmSetReconfigSettings::setAllowPermission decode_set_reconfiguration_permissions_v1_resp success - value={VALUE}, settingIndex={SI}",
            "VALUE", value, "SI", (int)settingIndex);
    }
    else
    {
        lg2::error(
            "NsmSetReconfigSettings::setAllowPermission: decode_set_reconfiguration_permissions_v1_resp failed with reasonCode = {REASONCODE}, cc = {CC} and rc = {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

} // namespace nsm
