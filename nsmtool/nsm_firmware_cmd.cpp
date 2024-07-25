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

#include "nsm_firmware_cmd.hpp"

#include "base.h"
#include "firmware-utils.h"

#include "cmd_helper.hpp"

#include <CLI/CLI.hpp>

namespace nsmtool::firmware
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

class QueryFirmwareType : public CommandInterface
{
  public:
    ~QueryFirmwareType() = default;
    QueryFirmwareType() = delete;
    QueryFirmwareType(const QueryFirmwareType&) = delete;
    QueryFirmwareType(QueryFirmwareType&&) = default;
    QueryFirmwareType& operator=(const QueryFirmwareType&) = delete;
    QueryFirmwareType& operator=(QueryFirmwareType&&) = default;

    explicit QueryFirmwareType(const char* type, const char* name,
                               CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required",
            "Get information about a particular firmware set installed on an endpoint");
        ccOptionGroup
            ->add_option("--classification", classification,
                         "Component classification")
            ->required();
        ccOptionGroup
            ->add_option("--identifier", identifier, "Component identifier")
            ->required();
        ccOptionGroup->add_option("--index", index, "Component index")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_get_erot_state_info_req));
        nsm_firmware_erot_state_info_req nsm_req;
        nsm_req.component_classification = classification;
        nsm_req.component_classification_index = index;
        nsm_req.component_identifier = identifier;
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_query_get_erot_state_parameters_req(
            instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        struct nsm_firmware_erot_state_info_resp erot_info = {};

        auto rc = decode_nsm_query_get_erot_state_parameters_resp(
            responsePtr, payloadLength, &cc, &reason_code, &erot_info);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Completion code"] = cc;
        result["Reason code"] = reason_code;
        result["Background copy policy"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.background_copy_policy);
        result["Active Slot"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.active_slot);
        result["Active Keyset"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.active_keyset);
        result["Minimum security version"] = static_cast<uint32_t>(
            erot_info.fq_resp_hdr.minimum_security_version);
        result["Update policy"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.inband_update_policy);
        result["Firmware slot count"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.firmware_slot_count);

        std::vector<ordered_json> slots;
        for (int i = 0; i < erot_info.fq_resp_hdr.firmware_slot_count; i++)
        {
            ordered_json slot_info;
            slot_info["Slot ID"] =
                static_cast<uint32_t>(erot_info.slot_info[i].slot_id);
            slot_info["Fw version string"] =
                (char*)(&(erot_info.slot_info[i].firmware_version_string[0]));
            slot_info["Version comp stamp"] = static_cast<uint32_t>(
                erot_info.slot_info[i].version_comparison_stamp);
            slot_info["Build type"] =
                static_cast<uint32_t>(erot_info.slot_info[i].build_type);
            slot_info["Signing type"] =
                static_cast<uint32_t>(erot_info.slot_info[i].signing_type);
            slot_info["WR Protect State"] = static_cast<uint32_t>(
                erot_info.slot_info[i].write_protect_state);
            slot_info["Firmware state"] =
                static_cast<uint32_t>(erot_info.slot_info[i].firmware_state);
            slot_info["Security version number"] = static_cast<uint32_t>(
                erot_info.slot_info[i].security_version_number);
            slot_info["Signing key index"] =
                static_cast<uint32_t>(erot_info.slot_info[i].signing_key_index);

            slots.push_back(std::move(slot_info));
        }

        result["Slot information"] = std::move(slots);

        DisplayInJson(result);

        free(erot_info.slot_info);
    }

  private:
    uint16_t classification{};
    uint16_t identifier{};
    uint8_t index{};
};

void registerCommand(CLI::App& app)
{
    auto firmware = app.add_subcommand("firmware",
                                       "Device firmware type commands");
    firmware->require_subcommand(1);

    auto queryFirmwareType = firmware->add_subcommand(
        "QueryFirmwareType",
        "Query information about a particular firmware set installed on an endpoint");
    commands.push_back(std::make_unique<QueryFirmwareType>(
        "firmware", "QueryRoTStateInformation", queryFirmwareType));
}
} // namespace nsmtool::firmware
