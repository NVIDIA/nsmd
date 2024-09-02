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

#include "debug-token.h"

#include "mockupResponder.hpp"

#include <stdlib.h>
#include <time.h>

#include <phosphor-logging/lg2.hpp>

namespace MockupResponder
{

constexpr const uint8_t uuid[8] = {0xFE, 0xED, 0x0F, 0x0C,
                                   0xAC, 0xC1, 0x0A, 0x01};

std::optional<std::vector<uint8_t>>
    MockupResponder::queryTokenParametersHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    nsm_debug_token_opcode token_opcode;
    auto rc = decode_nsm_query_token_parameters_req(requestMsg, requestLen,
                                                    &token_opcode);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_query_token_parameters_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    time_t t;
    srand((unsigned)time(&t));
    nsm_debug_token_request token_request;
    memset(&token_request, 0, sizeof(nsm_debug_token_request));
    token_request.token_request_version = 1;
    token_request.token_request_size = sizeof(nsm_debug_token_request);
    memcpy(token_request.device_uuid, uuid, 8);
    token_request.device_type = NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_NVSWITCH;
    token_request.device_index = 0;
    token_request.status = NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_OK;
    token_request.token_opcode = token_opcode;
    for (size_t i = 0; i < 16; ++i)
    {
        token_request.keypair_uuid[i] = rand() % 0xFF;
    }
    for (size_t i = 0; i < 8; ++i)
    {
        token_request.base_mac[i] = rand() % 0xFF;
    }
    for (size_t i = 0; i < 16; ++i)
    {
        token_request.psid[i] = rand() % 0xFF;
    }
    token_request.fw_version[0] = 0x01;
    token_request.fw_version[1] = 0x02;
    token_request.fw_version[2] = 0x03;
    token_request.fw_version[3] = 0x04;
    token_request.fw_version[4] = 0x05;
    for (size_t i = 0; i < 16; ++i)
    {
        token_request.source_address[i] = rand() % 0xFF;
    }
    token_request.session_id = 1;
    token_request.challenge_version = 1;
    for (size_t i = 0; i < 32; ++i)
    {
        token_request.challenge[i] = rand() % 0xFF;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_parameters_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_nsm_query_token_parameters_resp(requestMsg->hdr.instance_id,
                                                NSM_SUCCESS, reason_code,
                                                &token_request, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_nsm_query_token_parameters_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::provideTokenHandler(const nsm_msg* requestMsg,
                                         size_t requestLen)
{
    uint8_t token_data[NSM_DEBUG_TOKEN_DATA_MAX_SIZE];
    uint8_t token_data_len;
    auto rc = decode_nsm_provide_token_req(requestMsg, requestLen, token_data,
                                           &token_data_len);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_provide_token_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_provide_token_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_nsm_provide_token_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                       reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_nsm_provide_token_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::disableTokensHandler(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    auto rc = decode_nsm_disable_tokens_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_disable_tokens_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_disable_tokens_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_nsm_disable_tokens_resp(requestMsg->hdr.instance_id,
                                        NSM_SUCCESS, reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_nsm_disable_tokens_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryTokenStatusHandler(const nsm_msg* requestMsg,
                                             size_t requestLen)
{
    nsm_debug_token_type token_type;
    auto rc = decode_nsm_query_token_status_req(requestMsg, requestLen,
                                                &token_type);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_query_token_parameters_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    nsm_debug_token_status status = NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED;
    nsm_debug_token_status_additional_info additional_info =
        NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NO_DEBUG_SESSION;
    uint32_t time_left = 1234;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_status_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_nsm_query_token_status_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, status,
        additional_info, token_type, time_left, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_nsm_query_token_status_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryDeviceIdsHandler(const nsm_msg* requestMsg,
                                           size_t requestLen)

{
    auto rc = decode_nsm_query_device_ids_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_query_device_ids_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_ids_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_nsm_query_device_ids_resp(requestMsg->hdr.instance_id,
                                          NSM_SUCCESS, reason_code, uuid,
                                          responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_nsm_query_device_ids_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    return response;
}

} // namespace MockupResponder
