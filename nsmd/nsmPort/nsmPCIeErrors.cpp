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
#include "nsmPCIeErrors.hpp"

#include "sharedMemCommon.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

/**
 * This class provides a generalized implementation that leverages a group
 * parameter to manage shared logic, rather than tracking individual usage
 * counts for multiple classes with similar behavior. By consolidating this
 * logic into a single class, unit testing becomes simpler and more focused, as
 * only one class needs to be tested rather than several with overlapping
 * functionality. This approach also improves maintainability, offering greater
 * consistency and ease of modification when future changes or enhancements are
 * required.
 *
 * In the future, consider refactoring classes like NsmPCIeECCGroup<ID> to use
 * this generic implementation. This will further streamline the codebase and
 * reduce redundancy by consolidating related logic under a unified structure.
 */
NsmPCIeErrors::NsmPCIeErrors(const NsmInterfaceProvider<PCIeEccIntf>& provider,
                             uint8_t deviceIndex, uint8_t groupId) :
    NsmSensor(provider),
    NsmInterfaceContainer<PCIeEccIntf>(provider), deviceIndex(deviceIndex),
    groupId(groupId)
{
#define initHandleResponse(X)                                                  \
    {                                                                          \
        nsm_query_scalar_group_telemetry_group_##X data{};                     \
        handleResponse(data);                                                  \
    }
    switch (groupId)
    {
        case GROUP_ID_2:
            initHandleResponse(2);
            break;
        case GROUP_ID_3:
            initHandleResponse(3);
            break;
        case GROUP_ID_4:
            initHandleResponse(3);
            break;
    }
    updateMetricOnSharedMemory();
}

std::optional<Request> NsmPCIeErrors::genRequestMsg(eid_t eid,
                                                    uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(
        instanceId, deviceIndex, groupId, requestPtr);
    if (rc)
    {
        lg2::debug(
            "encode_query_scalar_group_telemetry_v1_req({GROUPID}) failed. eid={EID} rc={RC}",
            "GROUPID", groupId, "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

void NsmPCIeErrors::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    std::vector<uint8_t> data;
    switch (groupId)
    {
        case GROUP_ID_2:
            updateSharedMemoryOnSuccess(pdiPath(), pdi().interface,
                                        "CorrectableErrorCount", data,
                                        pdi().ceCount());
            updateSharedMemoryOnSuccess(pdiPath(), pdi().interface,
                                        "NonFatalErrorCount", data,
                                        pdi().nonfeCount());
            updateSharedMemoryOnSuccess(pdiPath(), pdi().interface,
                                        "FatalErrorCount", data,
                                        pdi().feCount());
            updateSharedMemoryOnSuccess(pdiPath(), pdi().interface,
                                        "UnsupportedRequestCount", data,
                                        pdi().unsupportedRequestCount());
            break;
        case GROUP_ID_3:
            updateSharedMemoryOnSuccess(pdiPath(), pdi().interface,
                                        "L0ToRecoveryCount", data,
                                        pdi().ceCount());
            break;
        case GROUP_ID_4:
            updateSharedMemoryOnSuccess(pdiPath(), pdi().interface,
                                        "ReplayCount", data,
                                        pdi().replayCount());
            updateSharedMemoryOnSuccess(pdiPath(), pdi().interface,
                                        "ReplayRolloverCount", data,
                                        pdi().replayRolloverCount());
            updateSharedMemoryOnSuccess(pdiPath(), pdi().interface,
                                        "NAKSentCount", data,
                                        pdi().nakSentCount());
            updateSharedMemoryOnSuccess(pdiPath(), pdi().interface,
                                        "NAKReceivedCount", data,
                                        pdi().nakReceivedCount());
            break;
    }
#endif
}

void NsmPCIeErrors::handleResponse(
    const nsm_query_scalar_group_telemetry_group_2& data)
{
    pdi().nonfeCount(data.non_fatal_errors);
    pdi().feCount(data.fatal_errors);
    pdi().ceCount(data.correctable_errors);
    pdi().unsupportedRequestCount(data.unsupported_request_count);
}
void NsmPCIeErrors::handleResponse(
    const nsm_query_scalar_group_telemetry_group_3& data)
{
    pdi().l0ToRecoveryCount(data.L0ToRecoveryCount);
}
void NsmPCIeErrors::handleResponse(
    const nsm_query_scalar_group_telemetry_group_4& data)
{
    pdi().replayCount(data.replay_cnt);
    pdi().replayRolloverCount(data.replay_rollover_cnt);
    pdi().nakSentCount(data.NAK_sent_cnt);
    pdi().nakReceivedCount(data.NAK_recv_cnt);
}

uint8_t NsmPCIeErrors::handleResponseMsg(const struct nsm_msg* responseMsg,
                                         size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t dataSize;
    uint16_t reasonCode = ERR_NULL;
    int rc = NSM_SW_ERROR_COMMAND_FAIL;

#define decode_query_scalar_group_telemetry_v1_group(X)                        \
    {                                                                          \
        nsm_query_scalar_group_telemetry_group_##X data{};                     \
        rc = decode_query_scalar_group_telemetry_v1_group##X##_resp(           \
            responseMsg, responseLen, &cc, &dataSize, &reasonCode, &data);     \
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)                         \
        {                                                                      \
            handleResponse(data);                                              \
            updateMetricOnSharedMemory();                                      \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            memset(&data, 0, sizeof(data));                                    \
            handleResponse(data);                                              \
            updateMetricOnSharedMemory();                                      \
            lg2::debug(                                                        \
                "NsmPCIeErrors::handleResponseMsg: "                           \
                "decode_query_scalar_group_telemetry_v1_group{GROUPID}_resp"   \
                "failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",    \
                "GROUPID", groupId, "REASONCODE", reasonCode, "CC", cc, "RC",  \
                rc);                                                           \
        }                                                                      \
    }

    switch (groupId)
    {
        case GROUP_ID_2:
            decode_query_scalar_group_telemetry_v1_group(2);
            break;
        case GROUP_ID_3:
            decode_query_scalar_group_telemetry_v1_group(3);
            break;
        case GROUP_ID_4:
            decode_query_scalar_group_telemetry_v1_group(4);
            break;
    }

    if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
    {
        logHandleResponseMsg(
            std::format("decode_query_scalar_group_telemetry_v1_group{}_resp",
                        groupId),
            reasonCode, cc, rc);
    }
    else
    {
        clearErrorBitMap(std::format(
            "decode_query_scalar_group_telemetry_v1_group{}_resp", groupId));
    }

    return cc ? cc : rc;
}
} // namespace nsm
