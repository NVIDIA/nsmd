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

#include "firmware-utils.h"

#include "utils.hpp"

#include <mockupResponder.hpp>
#include <phosphor-logging/lg2.hpp>

namespace MockupResponder
{

class FirmwareStateMachine
{
  public:
    // global states to maintain the values for testing
    static const uint64_t fixedNonce = 123456789;
    uint8_t configState = 0;
    struct nsm_firmware_security_version_number_resp sec_respEc
    {
        3, 4, 0, 0
    };
    struct nsm_firmware_security_version_number_resp sec_respAp
    {
        3, 4, 1, 0
    };

    uint16_t apActiveComponentKeyIndex = 6;
    uint16_t apPendingComponentKeyIndex = 6;
    std::vector<uint8_t> apActiveComponentKeyPerm{0xFE, 0xFF, 0xFF, 0xFF,
                                                  0xFF, 0xFF, 0xFF, 0xFF};
    std::vector<uint8_t> apPendingComponentKeyPerm{0xFE, 0xFF, 0xFF, 0xFF,
                                                   0xFF, 0xFF, 0xFF, 0xFF};
    std::vector<uint8_t> apEfuseKeyPerm{0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> apPendingEfuseKeyPerm{0x00, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00};

    uint16_t ecActiveComponentKeyIndex = 2;
    uint16_t ecPendingComponentKeyIndex = 2;
    std::vector<uint8_t> ecActiveComponentKeyPerm{0x00};
    std::vector<uint8_t> ecPendingComponentKeyPerm{0x00};
    std::vector<uint8_t> ecEfuseKeyPerm{0x00};
    std::vector<uint8_t> ecPendingEfuseKeyPerm{0x00};
};

static std::unique_ptr<FirmwareStateMachine> fwStateMachine = nullptr;

std::optional<std::vector<uint8_t>>
    MockupResponder::getRotInformation(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    struct nsm_firmware_erot_state_info_req fq_req;
    auto rc = decode_nsm_query_get_erot_state_parameters_req(
        requestMsg, requestLen, &fq_req);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_nsm_query_get_erot_state_parameters_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }

    /* Exact message size will be counted in the encode function,
        just, make sure this buffer is big enough to cover
        the above number of slots */
    uint16_t msg_size = sizeof(struct nsm_msg_hdr) + 250;
    std::vector<uint8_t> response(msg_size, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    // Example response with firmware state
    struct nsm_firmware_erot_state_info_resp fq_resp = {};

    if ((fq_req.component_classification == 0x000A) &&
        (fq_req.component_identifier == 0xff00))
    {
        const char* firmware_version1 = "01.03.0210.0003";
        const char* firmware_version2 = "01.03.0210.0004";

        /* Emulate a real answer form EROT device */
        fq_resp.fq_resp_hdr.active_slot = 0x0;
        fq_resp.fq_resp_hdr.firmware_slot_count = 2;
        fq_resp.fq_resp_hdr.background_copy_policy = 1;
        fq_resp.fq_resp_hdr.active_keyset = 1;
        fq_resp.fq_resp_hdr.inband_update_policy = 1;
        fq_resp.fq_resp_hdr.minimum_security_version = 1;
        fq_resp.fq_resp_hdr.boot_status_code = 1;

        fq_resp.slot_info = (struct nsm_firmware_slot_info*)malloc(
            fq_resp.fq_resp_hdr.firmware_slot_count *
            sizeof(struct nsm_firmware_slot_info));
        memset((char*)(fq_resp.slot_info), 0,
               fq_resp.fq_resp_hdr.firmware_slot_count *
                   sizeof(struct nsm_firmware_slot_info));

        fq_resp.slot_info[0].slot_id = 0;
        strcpy((char*)(&(fq_resp.slot_info[0].firmware_version_string[0])),
               firmware_version1);
        fq_resp.slot_info[0].build_type = 0;
        fq_resp.slot_info[0].version_comparison_stamp = 1;
        fq_resp.slot_info[0].signing_type = 1;
        fq_resp.slot_info[0].write_protect_state = 0;
        fq_resp.slot_info[0].firmware_state = 1;
        fq_resp.slot_info[0].security_version_number = 1;
        fq_resp.slot_info[0].signing_key_index = 1;
        fq_resp.slot_info[1].slot_id = 1;
        strcpy((char*)(&(fq_resp.slot_info[1].firmware_version_string[0])),
               firmware_version2);
        fq_resp.slot_info[1].build_type = 1;
        fq_resp.slot_info[1].version_comparison_stamp = 1;
        fq_resp.slot_info[1].signing_type = 1;
        fq_resp.slot_info[1].write_protect_state = 1;
        fq_resp.slot_info[1].firmware_state = 1;
        fq_resp.slot_info[1].security_version_number = 1;
        fq_resp.slot_info[1].signing_key_index = 1;
        rc = encode_nsm_query_get_erot_state_parameters_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &fq_resp,
            responseMsg);
    }
    else if ((fq_req.component_classification == 0x000A) &&
             (fq_req.component_identifier == 0x0010))
    {
        const char* firmware_version1 = "24.07-1-rc26";
        const char* firmware_version2 = "24.07-1-rc27";

        /* Emulate a real answer form EROT device */
        fq_resp.fq_resp_hdr.active_slot = 0x1;
        fq_resp.fq_resp_hdr.firmware_slot_count = 2;
        fq_resp.fq_resp_hdr.background_copy_policy = 1;
        fq_resp.fq_resp_hdr.active_keyset = 1;
        fq_resp.fq_resp_hdr.inband_update_policy = 1;
        fq_resp.fq_resp_hdr.minimum_security_version = 1;
        fq_resp.fq_resp_hdr.boot_status_code = 1;

        fq_resp.slot_info = (struct nsm_firmware_slot_info*)malloc(
            fq_resp.fq_resp_hdr.firmware_slot_count *
            sizeof(struct nsm_firmware_slot_info));
        memset((char*)(fq_resp.slot_info), 0,
               fq_resp.fq_resp_hdr.firmware_slot_count *
                   sizeof(struct nsm_firmware_slot_info));

        fq_resp.slot_info[0].slot_id = 0;
        strcpy((char*)(&(fq_resp.slot_info[0].firmware_version_string[0])),
               firmware_version1);
        fq_resp.slot_info[0].build_type = 0;
        fq_resp.slot_info[0].version_comparison_stamp = 1;
        fq_resp.slot_info[0].signing_type = 1;
        fq_resp.slot_info[0].write_protect_state = 0;
        fq_resp.slot_info[0].firmware_state = 1;
        fq_resp.slot_info[0].security_version_number = 1;
        fq_resp.slot_info[0].signing_key_index = 1;
        fq_resp.slot_info[1].slot_id = 1;
        strcpy((char*)(&(fq_resp.slot_info[1].firmware_version_string[0])),
               firmware_version2);
        fq_resp.slot_info[1].build_type = 1;
        fq_resp.slot_info[1].version_comparison_stamp = 1;
        fq_resp.slot_info[1].signing_type = 1;
        fq_resp.slot_info[1].write_protect_state = 1;
        fq_resp.slot_info[1].firmware_state = 1;
        fq_resp.slot_info[1].security_version_number = 1;
        fq_resp.slot_info[1].signing_key_index = 1;

        rc = encode_nsm_query_get_erot_state_parameters_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &fq_resp,
            responseMsg);
    }
    else if ((fq_req.component_classification == 0x000A) &&
             (fq_req.component_identifier == 0x0050))
    {
        const char* firmware_version1 = "322e3044";
        const char* firmware_version2 = "322e3045";

        /* Emulate a real answer form EROT device */
        fq_resp.fq_resp_hdr.active_slot = 0x0;
        fq_resp.fq_resp_hdr.firmware_slot_count = 2;
        fq_resp.fq_resp_hdr.background_copy_policy = 1;
        fq_resp.fq_resp_hdr.active_keyset = 1;
        fq_resp.fq_resp_hdr.inband_update_policy = 1;
        fq_resp.fq_resp_hdr.minimum_security_version = 1;

        fq_resp.slot_info = (struct nsm_firmware_slot_info*)malloc(
            fq_resp.fq_resp_hdr.firmware_slot_count *
            sizeof(struct nsm_firmware_slot_info));
        memset((char*)(fq_resp.slot_info), 0,
               fq_resp.fq_resp_hdr.firmware_slot_count *
                   sizeof(struct nsm_firmware_slot_info));

        fq_resp.slot_info[0].slot_id = 0;
        strcpy((char*)(&(fq_resp.slot_info[0].firmware_version_string[0])),
               firmware_version1);
        fq_resp.slot_info[0].build_type = 0;
        fq_resp.slot_info[0].version_comparison_stamp = 1;
        fq_resp.slot_info[0].signing_type = 1;
        fq_resp.slot_info[0].write_protect_state = 0;
        fq_resp.slot_info[0].firmware_state = 1;
        fq_resp.slot_info[0].security_version_number = 1;
        fq_resp.slot_info[0].signing_key_index = 1;
        fq_resp.slot_info[1].slot_id = 1;
        strcpy((char*)(&(fq_resp.slot_info[1].firmware_version_string[0])),
               firmware_version2);
        fq_resp.slot_info[1].build_type = 1;
        fq_resp.slot_info[1].version_comparison_stamp = 1;
        fq_resp.slot_info[1].signing_type = 1;
        fq_resp.slot_info[1].write_protect_state = 1;
        fq_resp.slot_info[1].firmware_state = 1;
        fq_resp.slot_info[1].security_version_number = 1;
        fq_resp.slot_info[1].signing_key_index = 1;

        rc = encode_nsm_query_get_erot_state_parameters_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &fq_resp,
            responseMsg);
    }
    else
    {
        const char* firmware_version1 = "Version ABCDE";
        const char* firmware_version2 = "Version 12345";

        /* Emulate an answer with all possible fields,
            but random content */
        fq_resp.fq_resp_hdr.background_copy_policy = 0x30;
        fq_resp.fq_resp_hdr.active_slot = 0x1;
        fq_resp.fq_resp_hdr.active_keyset = 0x32;
        fq_resp.fq_resp_hdr.minimum_security_version = 0x3334;
        fq_resp.fq_resp_hdr.inband_update_policy = 0x35;
        fq_resp.fq_resp_hdr.firmware_slot_count = 2;
        fq_resp.fq_resp_hdr.boot_status_code = 0x0102030405060708;

        fq_resp.slot_info = (struct nsm_firmware_slot_info*)malloc(
            fq_resp.fq_resp_hdr.firmware_slot_count *
            sizeof(struct nsm_firmware_slot_info));
        memset((char*)(fq_resp.slot_info), 0,
               fq_resp.fq_resp_hdr.firmware_slot_count *
                   sizeof(struct nsm_firmware_slot_info));

        fq_resp.slot_info[0].slot_id = 0x40;
        strcpy((char*)(&(fq_resp.slot_info[0].firmware_version_string[0])),
               firmware_version1);
        fq_resp.slot_info[0].version_comparison_stamp = 0x09ABCDEF;
        fq_resp.slot_info[0].build_type = 0x1;
        fq_resp.slot_info[0].signing_type = 0x42;
        fq_resp.slot_info[0].write_protect_state = 0x43;
        fq_resp.slot_info[0].firmware_state = 0x44;
        fq_resp.slot_info[0].security_version_number = 0x4546;
        fq_resp.slot_info[0].signing_key_index = 0x4748;

        fq_resp.slot_info[1].slot_id = 0x50;
        strcpy((char*)(&(fq_resp.slot_info[1].firmware_version_string[0])),
               firmware_version2);
        fq_resp.slot_info[1].version_comparison_stamp = 0x23456789;
        fq_resp.slot_info[1].build_type = 0x1;
        fq_resp.slot_info[1].signing_type = 0x52;
        fq_resp.slot_info[1].write_protect_state = 0x53;
        fq_resp.slot_info[1].firmware_state = 0x54;
        fq_resp.slot_info[1].security_version_number = 0x5556;
        fq_resp.slot_info[1].signing_key_index = 0x5758;
        rc = encode_nsm_query_get_erot_state_parameters_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &fq_resp,
            responseMsg);
    }

    free(fq_resp.slot_info);

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
    MockupResponder::irreversibleConfig(const nsm_msg* requestMsg,
                                        size_t requestLen)
{
    if (fwStateMachine == nullptr)
    {
        fwStateMachine = std::make_unique<FirmwareStateMachine>();
    }
    struct nsm_firmware_irreversible_config_req cfg_req;
    auto rc = decode_nsm_firmware_irreversible_config_req(requestMsg,
                                                          requestLen, &cfg_req);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_nsm_firmware_irreversible_config_req failed: rc={RC}", "RC",
            rc);
        return std::nullopt;
    }

    // Sample Update Irreversible Config Response
    uint16_t msg_size = sizeof(struct nsm_msg_hdr) + 250;
    std::vector<uint8_t> response(msg_size, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    switch (cfg_req.request_type)
    {
        case QUERY_IRREVERSIBLE_CFG:
        {
            struct nsm_firmware_irreversible_config_request_0_resp cfg_0_resp
            {};
            cfg_0_resp.irreversible_config_state = fwStateMachine->configState;
            rc = encode_nsm_firmware_irreversible_config_request_0_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
                &cfg_0_resp, responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc)
            {
                lg2::error(
                    "nsm_firmware_irreversible_config_request_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
            break;
        }
        case DISABLE_IRREVERSIBLE_CFG:
        {
            fwStateMachine->configState = 0;
            rc = encode_nsm_firmware_irreversible_config_request_1_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
                responseMsg);
            break;
        }
        case ENABLE_IRREVERSIBLE_CFG:
        {
            fwStateMachine->configState = 1;
            struct nsm_firmware_irreversible_config_request_2_resp cfg_2_resp
            {};
            cfg_2_resp.nonce = fwStateMachine->fixedNonce;
            rc = encode_nsm_firmware_irreversible_config_request_2_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
                &cfg_2_resp, responseMsg);
            break;
        }
        default:
            lg2::error("Unknown request type {REQ_TYPE}", "REQ_TYPE",
                       cfg_req.request_type);
            break;
    }
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error(
            "nsm_firmware_irreversible_config_request_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::codeAuthKeyPermQueryHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    if (fwStateMachine == nullptr)
    {
        fwStateMachine = std::make_unique<FirmwareStateMachine>();
    }
    uint16_t componentClassification;
    uint16_t componentIdentifier;
    uint8_t componentClassificationIndex;
    auto rc = decode_nsm_code_auth_key_perm_query_req(
        requestMsg, requestLen, &componentClassification, &componentIdentifier,
        &componentClassificationIndex);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_code_auth_key_perm_query_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    if (componentClassification != 0x000A)
    {
        lg2::error("Invalid component classification value");
        return std::nullopt;
    }
    if (componentClassificationIndex != 0)
    {
        lg2::error("Invalid component classification index value");
        return std::nullopt;
    }
    if (componentIdentifier != 0x0010 && componentIdentifier != 0xFF00)
    {
        lg2::error("Invalid component identifier value");
        return std::nullopt;
    }
    bool isAp = componentIdentifier == 0x0010;
    uint8_t bitmapLength =
        isAp ? fwStateMachine->apActiveComponentKeyPerm.size()
             : fwStateMachine->ecActiveComponentKeyPerm.size();
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_query_resp) +
            bitmapLength * 4u,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reasonCode = ERR_NULL;
    if (isAp)
    {
        rc = encode_nsm_code_auth_key_perm_query_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reasonCode,
            fwStateMachine->apActiveComponentKeyIndex,
            fwStateMachine->apPendingComponentKeyIndex, bitmapLength,
            fwStateMachine->apActiveComponentKeyPerm.data(),
            fwStateMachine->apPendingComponentKeyPerm.data(),
            fwStateMachine->apEfuseKeyPerm.data(),
            fwStateMachine->apPendingEfuseKeyPerm.data(), responseMsg);
    }
    else
    {
        rc = encode_nsm_code_auth_key_perm_query_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reasonCode,
            fwStateMachine->ecActiveComponentKeyIndex,
            fwStateMachine->ecPendingComponentKeyIndex, bitmapLength,
            fwStateMachine->ecActiveComponentKeyPerm.data(),
            fwStateMachine->ecPendingComponentKeyPerm.data(),
            fwStateMachine->ecEfuseKeyPerm.data(),
            fwStateMachine->ecPendingEfuseKeyPerm.data(), responseMsg);
    }
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_nsm_code_auth_key_perm_query_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::codeAuthKeyPermUpdateHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    if (fwStateMachine == nullptr)
    {
        fwStateMachine = std::make_unique<FirmwareStateMachine>();
    }
    nsm_code_auth_key_perm_request_type requestType;
    uint16_t componentClassification;
    uint16_t componentIdentifier;
    uint8_t componentClassificationIndex;
    uint64_t nonce;
    uint8_t bitmapLength;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_update_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reasonCode = ERR_NULL;
    uint32_t updateMethod = 0;

    auto encodeResp = [&](uint8_t cc) -> std::optional<std::vector<uint8_t>> {
        int rc = encode_nsm_code_auth_key_perm_update_resp(
            requestMsg->hdr.instance_id, cc, reasonCode, updateMethod,
            responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "encode_nsm_code_auth_key_perm_update_resp failed: rc={RC}",
                "RC", rc);
            return std::nullopt;
        }
        return response;
    };

    auto rc = decode_nsm_code_auth_key_perm_update_req(
        requestMsg, requestLen, &requestType, &componentClassification,
        &componentIdentifier, &componentClassificationIndex, &nonce,
        &bitmapLength, nullptr);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_code_auth_key_perm_update_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    if (requestType !=
            NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE &&
        requestType != NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_SPECIFIED_VALUE)
    {
        lg2::error("Invalid request type");
        return encodeResp(NSM_ERR_INVALID_DATA);
    }
    if (requestType ==
            NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE &&
        bitmapLength != 0)
    {
        lg2::error("Invalid request type and bitmap length");
        return encodeResp(NSM_ERR_INVALID_DATA);
    }
    if (componentClassification != 0x000A)
    {
        lg2::error("Invalid component classification value");
        return encodeResp(NSM_ERR_INVALID_DATA);
    }
    if (componentClassificationIndex != 0)
    {
        lg2::error("Invalid component classification index value");
        return encodeResp(NSM_ERR_INVALID_DATA);
    }
    if (componentIdentifier != 0x0010 && componentIdentifier != 0xFF00)
    {
        lg2::error("Invalid component identifier value");
        return encodeResp(NSM_ERR_INVALID_DATA);
    }
    if (fwStateMachine->configState == 0)
    {
        return encodeResp(0x87); // irreversible config disabled
    }
    else if (nonce != fwStateMachine->fixedNonce)
    {
        return encodeResp(0x88); // nonce mismatch
    }
    bool isAp = componentIdentifier == 0x0010;
    std::vector<uint8_t> bitmap(bitmapLength);
    rc = decode_nsm_code_auth_key_perm_update_req(
        requestMsg, requestLen, &requestType, &componentClassification,
        &componentIdentifier, &componentClassificationIndex, &nonce,
        &bitmapLength, bitmap.data());
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_code_auth_key_perm_update_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    if (isAp)
    {
        if (requestType ==
            NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE)
        {
            std::vector<uint8_t> indices;
            for (auto i = 0; i < fwStateMachine->apActiveComponentKeyIndex; ++i)
            {
                indices.emplace_back(i);
            }
            try
            {
                bitmap = utils::indicesToBitmap(indices);
            }
            catch (const std::exception&)
            {
                return encodeResp(NSM_ERR_INVALID_DATA_LENGTH);
            }
        }
        if (bitmapLength > fwStateMachine->apPendingEfuseKeyPerm.size())
        {
            return encodeResp(NSM_ERR_INVALID_DATA_LENGTH);
        }
        for (size_t i = 0; i < bitmap.size(); ++i)
        {
            fwStateMachine->apPendingEfuseKeyPerm[i] |= bitmap[i];
        }
        updateMethod = NSM_EFUSE_UPDATE_METHOD_DC_POWER_CYCLE;
    }
    else
    {
        if (requestType ==
            NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE)
        {
            std::vector<uint8_t> indices;
            for (auto i = 0; i < fwStateMachine->ecActiveComponentKeyIndex; ++i)
            {
                indices.emplace_back(i);
            }
            try
            {
                bitmap = utils::indicesToBitmap(indices);
            }
            catch (const std::exception&)
            {
                return encodeResp(NSM_ERR_INVALID_DATA_LENGTH);
            }
        }
        if (bitmapLength > fwStateMachine->ecEfuseKeyPerm.size())
        {
            return encodeResp(NSM_ERR_INVALID_DATA_LENGTH);
        }
        for (size_t i = 0; i < bitmap.size(); ++i)
        {
            fwStateMachine->ecEfuseKeyPerm[i] |= bitmap[i];
            fwStateMachine->ecPendingEfuseKeyPerm[i] |= bitmap[i];
        }
        updateMethod = NSM_EFUSE_UPDATE_METHOD_AUTO;
    }

    return encodeResp(NSM_SUCCESS);
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryFirmwareSecurityVersion(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    if (fwStateMachine == nullptr)
    {
        fwStateMachine = std::make_unique<FirmwareStateMachine>();
    }
    struct nsm_firmware_security_version_number_req sec_req;
    auto rc = decode_nsm_query_firmware_security_version_number_req(
        requestMsg, requestLen, &sec_req);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_nsm_query_firmware_security_version_number_req failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    uint16_t msg_size =
        sizeof(struct nsm_msg_hdr) +
        sizeof(nsm_firmware_security_version_number_resp_command);
    std::vector<uint8_t> response(msg_size, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    if (sec_req.component_identifier == 0xFF00) // EC FW
    {
        rc = encode_nsm_query_firmware_security_version_number_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
            &fwStateMachine->sec_respEc, responseMsg);
    }
    else
    {
        rc = encode_nsm_query_firmware_security_version_number_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
            &fwStateMachine->sec_respAp, responseMsg);
    }

    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("nsm_firmware_security_version_number_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::updateMinSecurityVersion(const nsm_msg* requestMsg,
                                              size_t requestLen)
{
    if (fwStateMachine == nullptr)
    {
        fwStateMachine = std::make_unique<FirmwareStateMachine>();
    }
    struct nsm_firmware_update_min_sec_ver_req sec_req;
    auto rc = decode_nsm_firmware_update_sec_ver_req(requestMsg, requestLen,
                                                     &sec_req);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_nsm_firmware_update_sec_ver_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    // Sample Update Min Security Version Response
    struct nsm_firmware_update_min_sec_ver_resp sec_resp
    {};
    uint16_t msg_size = sizeof(struct nsm_msg_hdr) +
                        sizeof(nsm_firmware_update_min_sec_ver_req_command);
    std::vector<uint8_t> response(msg_size, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    if (sec_req.request_type == REQUEST_TYPE_SPECIFIED_VALUE &&
        sec_req.req_min_security_version == 0)
    {
        // invalid data
        rc = encode_nsm_firmware_update_sec_ver_resp(
            requestMsg->hdr.instance_id, NSM_ERR_INVALID_DATA, reason_code,
            &sec_resp, responseMsg);
        return response;
    }
    if (sec_req.nonce != fwStateMachine->fixedNonce)
    {
        uint8_t cc = 0x88; // nonce mismatch
        rc = encode_nsm_firmware_update_sec_ver_resp(
            requestMsg->hdr.instance_id, cc, reason_code, &sec_resp,
            responseMsg);
        return response;
    }
    if (fwStateMachine->configState == 0)
    {
        uint8_t cc = 0x87; // irreversible config disabled
        rc = encode_nsm_firmware_update_sec_ver_resp(
            requestMsg->hdr.instance_id, cc, reason_code, &sec_resp,
            responseMsg);
        return response;
    }

    if (sec_req.request_type == REQUEST_TYPE_MOST_RESTRICTIVE_VALUE)
    {
        if (sec_req.component_identifier == 0xFF00)
        {
            fwStateMachine->sec_respEc.minimum_security_version =
                fwStateMachine->sec_respEc.active_component_security_version;
            sec_resp.update_methods = 0x1; // Automatic
        }
        else
        {
            fwStateMachine->sec_respAp.pending_minimum_security_version =
                fwStateMachine->sec_respAp.active_component_security_version;
            sec_resp.update_methods = 0x30; // DC Power cycle & AC Power cycle
        }
    }
    else
    {
        if (sec_req.component_identifier == 0xFF00)
        {
            if (sec_req.req_min_security_version >=
                    fwStateMachine->sec_respEc.minimum_security_version &&
                sec_req.req_min_security_version <=
                    fwStateMachine->sec_respEc
                        .active_component_security_version)
            {
                fwStateMachine->sec_respEc.minimum_security_version =
                    sec_req.req_min_security_version;
                sec_resp.update_methods = 0x1; // Automatic
            }
            else
            {
                // invalid data
                rc = encode_nsm_firmware_update_sec_ver_resp(
                    requestMsg->hdr.instance_id, NSM_ERR_INVALID_DATA,
                    reason_code, &sec_resp, responseMsg);
                return response;
            }
        }
        else
        {
            if (sec_req.req_min_security_version > 0 &&
                sec_req.req_min_security_version <=
                    fwStateMachine->sec_respAp
                        .active_component_security_version)
            {
                fwStateMachine->sec_respAp.pending_minimum_security_version =
                    sec_req.req_min_security_version;
                sec_resp.update_methods =
                    0x30; // DC Power cycle & AC Power cycle
            }
            else
            {
                // invalid data
                rc = encode_nsm_firmware_update_sec_ver_resp(
                    requestMsg->hdr.instance_id, NSM_ERR_INVALID_DATA,
                    reason_code, &sec_resp, responseMsg);
                return response;
            }
        }
    }
    rc = encode_nsm_firmware_update_sec_ver_resp(requestMsg->hdr.instance_id,
                                                 NSM_SUCCESS, reason_code,
                                                 &sec_resp, responseMsg);

    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_nsm_firmware_update_sec_ver_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

} // namespace MockupResponder
