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

#include "nsmAERError.hpp"

#include "base.h"
#include "libnsm/pci-links.h"

#include "asyncOperationManager.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmPCIeAERErrorStatus::NsmPCIeAERErrorStatus(
    const std::string& name, const std::string& type,
    std::shared_ptr<NsmAERErrorStatusIntf> aerErrorStatusIntf,
    uint8_t deviceIndex) :
    NsmSensor(name, type),
    aerErrorStatusIntf(aerErrorStatusIntf), deviceIndex(deviceIndex)
{
    lg2::info("NsmPCIeAERErrorStatus: create sensor:{NAME}", "NAME",
              name.c_str());
}

std::optional<Request> NsmPCIeAERErrorStatus::genRequestMsg(eid_t eid,
                                                            uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(
        instanceId, deviceIndex, GROUP_ID_9, requestPtr);
    if (rc)
    {
        lg2::error(
            "NsmPCIeAERErrorStatus: encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmPCIeAERErrorStatus::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_9 data = {};
    uint16_t size = 0;

    auto rc = decode_query_scalar_group_telemetry_v1_group9_resp(
        responseMsg, responseLen, &cc, &size, &reasonCode, &data);

    if (rc == NSM_SUCCESS && cc == NSM_SUCCESS)
    {
        auto hexFormat = [](const uint32_t value) -> std::string {
            std::string hexStr(8, '\0');
            sprintf(hexStr.data(), "0x%08X", value);
            return hexStr;
        };

        aerErrorStatusIntf->aerUncorrectableErrorStatus(
            hexFormat(data.aer_uncorrectable_error_status));
        aerErrorStatusIntf->aerCorrectableErrorStatus(
            hexFormat(data.aer_correctable_error_status));
    }
    else
    {
        lg2::error(
            "NsmPCIeAERErrorStatus: decode_query_scalar_group_telemetry_v1_group9_resp failed. rc={RC}, cc={CC}",
            "RC", rc, "CC", cc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmAERErrorStatusIntf::clearAERError(AsyncOperationStatusType* status)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_clear_data_source_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // first argument instanceid=0 is irrelevant
    auto rc = encode_clear_data_source_v1_req(0, deviceIndex, GROUP_ID_9,
                                              DS_ID_0, requestMsg);

    if (rc)
    {
        lg2::error(
            "clearAERError encode_clear_data_source_v1_req failed. eid={EID}, rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        lg2::error(
            "clearAERError SendRecvNsmMsgSync failed for for eid = {EID} rc = {RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_clear_data_source_v1_resp(responseMsg.get(), responseLen, &cc,
                                          &data_size, &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info("clearAERError for EID: {EID} completed", "EID", eid);
    }
    else
    {
        lg2::error(
            "clearAERError decode_clear_data_source_v1_resp failed.eid ={EID},CC = {CC} reasoncode = {RC},RC = {A} ",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmAERErrorStatusIntf::doclearAERErrorOnDevice(
    std::shared_ptr<AsyncStatusIntf> statusInterface)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    auto rc_ = co_await clearAERError(&status);

    statusInterface->status(status);
    // coverity[missing_return]
    co_return rc_;
};

sdbusplus::message::object_path NsmAERErrorStatusIntf::clearAERStatus()
{
    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        lg2::error(
            "NsmAERErrorStatusIntf::clearAERStatus failed. No available result Object to allocate for the Post request.");
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    doclearAERErrorOnDevice(statusInterface).detach();

    return objectPath;
}

} // namespace nsm
