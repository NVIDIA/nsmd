/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved. 
 * SPDX-License-Identifier: Apache-2.0
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

#include <mockupResponder.hpp>
#include <phosphor-logging/lg2.hpp>

namespace MockupResponder
{

std::optional<std::vector<uint8_t>>
    MockupResponder::queryFirmwareType(const nsm_msg* requestMsg,
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

} // namespace MockupResponder
