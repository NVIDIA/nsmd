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

#include "nsm_firmware_cmd.hpp"

#include "base.h"
#include "firmware-utils.h"

#include "cmd_helper.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>

namespace nsmtool::firmware
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

class GetRotInformation : public CommandInterface
{
  public:
    ~GetRotInformation() = default;
    GetRotInformation() = delete;
    GetRotInformation(const GetRotInformation&) = delete;
    GetRotInformation(GetRotInformation&&) = default;
    GetRotInformation& operator=(const GetRotInformation&) = delete;
    GetRotInformation& operator=(GetRotInformation&&) = default;

    explicit GetRotInformation(const char* type, const char* name,
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
        nsm_firmware_erot_state_info_resp erot_info = {};

        auto rc = decode_nsm_query_get_erot_state_parameters_resp(
            responsePtr, payloadLength, &cc, &reason_code, &erot_info);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            free(erot_info.slot_info);
            return;
        }

        ordered_json result;
        result["Completion code"] = cc;
        result["Reason code"] = reason_code;
        result["Background copy policy"] = mapEnumToString(
            static_cast<uint32_t>(erot_info.fq_resp_hdr.background_copy_policy),
            bgCopyPolicyMap);
        result["Active Slot"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.active_slot);
        result["Active Keyset"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.active_keyset);
        result["Minimum security version"] = static_cast<uint32_t>(
            erot_info.fq_resp_hdr.minimum_security_version);
        result["Update policy"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.inband_update_policy);
        result["Boot status code"] =
            static_cast<uint64_t>(erot_info.fq_resp_hdr.boot_status_code);
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
            slot_info["Build type"] = mapEnumToString(
                static_cast<uint32_t>(erot_info.slot_info[i].build_type),
                buildTypeMap);
            slot_info["Signing type"] = mapEnumToString(
                static_cast<uint32_t>(erot_info.slot_info[i].signing_type),
                signingTypeMap);
            slot_info["WR Protect State"] =
                mapEnumToString(static_cast<uint32_t>(
                                    erot_info.slot_info[i].write_protect_state),
                                writeProtectMap);
            slot_info["Firmware state"] = mapEnumToString(
                static_cast<uint32_t>(erot_info.slot_info[i].firmware_state),
                firmwareStateMap);
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

    const std::unordered_map<uint32_t, std::string> bgCopyPolicyMap = {
        {0, "Disabled"}, {1, "Enabled"}};

    const std::unordered_map<uint32_t, std::string> buildTypeMap = {
        {0, "Development"}, {1, "Release"}};

    const std::unordered_map<uint32_t, std::string> signingTypeMap = {
        {0, "Debug"}, {1, "Production"}, {2, "External"}, {4, "DOT"}};

    const std::unordered_map<uint32_t, std::string> writeProtectMap = {
        {0, "Disabled"}, {1, "Enabled"}};

    const std::unordered_map<uint32_t, std::string> firmwareStateMap = {
        {0, "Unknown"},
        {1, "Activated"},
        {2, "Pending Activation"},
        {3, "Staged"},
        {4, "Write in progress"},
        {5, "Inactive"},
        {6, "Failed authentication"}};

    std::string mapEnumToString(
        uint32_t value,
        const std::unordered_map<uint32_t, std::string>& mapping) const
    {
        auto it = mapping.find(value);
        return it != mapping.end() ? it->second : "Not Defined";
    }
};

class QueryCodeAuthKeyPerm : public CommandInterface
{
  public:
    ~QueryCodeAuthKeyPerm() = default;
    QueryCodeAuthKeyPerm() = delete;
    QueryCodeAuthKeyPerm(const QueryCodeAuthKeyPerm&) = delete;
    QueryCodeAuthKeyPerm(QueryCodeAuthKeyPerm&&) = default;
    QueryCodeAuthKeyPerm& operator=(const QueryCodeAuthKeyPerm&) = delete;
    QueryCodeAuthKeyPerm& operator=(QueryCodeAuthKeyPerm&&) = default;

    using CommandInterface::CommandInterface;

    explicit QueryCodeAuthKeyPerm(const char* type, const char* name,
                                  CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto optionGroup = app->add_option_group(
            "Required", "Query firmware code authentication key permissions");
        optionGroup
            ->add_option("--classification", classification,
                         "Component classification")
            ->required();
        optionGroup
            ->add_option("--identifier", identifier, "Component identifier")
            ->required();
        optionGroup->add_option("--index", index, "Component index")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_query_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_code_auth_key_perm_query_req(
            instanceId, classification, identifier, index, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint16_t activeComponentKeyIndex;
        uint16_t pendingComponentKeyIndex;
        uint8_t permissionBitmapLength;
        auto rc = decode_nsm_code_auth_key_perm_query_resp(
            responsePtr, payloadLength, &cc, &reasonCode,
            &activeComponentKeyIndex, &pendingComponentKeyIndex,
            &permissionBitmapLength, NULL, NULL, NULL, NULL);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reasonCode << "\n";
            return;
        }

        std::vector<uint8_t> activeComponentKeyPermBitmap(
            permissionBitmapLength);
        std::vector<uint8_t> pendingComponentKeyPermBitmap(
            permissionBitmapLength);
        std::vector<uint8_t> efuseKeyPermBitmap(permissionBitmapLength);
        std::vector<uint8_t> pendingEfuseKeyPermBitmap(permissionBitmapLength);

        rc = decode_nsm_code_auth_key_perm_query_resp(
            responsePtr, payloadLength, &cc, &reasonCode,
            &activeComponentKeyIndex, &pendingComponentKeyIndex,
            &permissionBitmapLength, activeComponentKeyPermBitmap.data(),
            pendingComponentKeyPermBitmap.data(), efuseKeyPermBitmap.data(),
            pendingEfuseKeyPermBitmap.data());
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reasonCode << "\n";
            return;
        }

        nlohmann::ordered_json result;
        result["Completion code"] = cc;
        result["Reason code"] = reasonCode;
        result["Active component key index"] = activeComponentKeyIndex;
        result["Pending component key index"] = pendingComponentKeyIndex;
        result["Permission bitmap length"] = permissionBitmapLength;

        auto activeComponentKeyPermIndices =
            utils::bitmapToIndices(activeComponentKeyPermBitmap);
        auto pendingComponentKeyPermIndices =
            utils::bitmapToIndices(pendingComponentKeyPermBitmap);
        auto efuseKeyPermIndices = utils::bitmapToIndices(efuseKeyPermBitmap);
        auto pendingEfuseKeyPermIndices =
            utils::bitmapToIndices(pendingEfuseKeyPermBitmap);

        result["Active component trusted key indices"] =
            std::move(activeComponentKeyPermIndices.first);
        result["Active component revoked key indices"] =
            std::move(activeComponentKeyPermIndices.second);
        result["Pending component trusted key indices"] =
            std::move(pendingComponentKeyPermIndices.first);
        result["Pending component revoked key indices"] =
            std::move(pendingComponentKeyPermIndices.second);
        result["EFUSE trusted key indices"] =
            std::move(efuseKeyPermIndices.first);
        result["EFUSE revoked key indices"] =
            std::move(efuseKeyPermIndices.second);
        result["Pending EFUSE trusted key indices"] =
            std::move(pendingEfuseKeyPermIndices.first);
        result["Pending EFUSE revoked key indices"] =
            std::move(pendingEfuseKeyPermIndices.second);

        DisplayInJson(result);
    }

  private:
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
};

class UpdateCodeAuthKeyPerm : public CommandInterface
{
  public:
    ~UpdateCodeAuthKeyPerm() = default;
    UpdateCodeAuthKeyPerm() = delete;
    UpdateCodeAuthKeyPerm(const UpdateCodeAuthKeyPerm&) = delete;
    UpdateCodeAuthKeyPerm(UpdateCodeAuthKeyPerm&&) = default;
    UpdateCodeAuthKeyPerm& operator=(const UpdateCodeAuthKeyPerm&) = delete;
    UpdateCodeAuthKeyPerm& operator=(UpdateCodeAuthKeyPerm&&) = default;

    using CommandInterface::CommandInterface;

    explicit UpdateCodeAuthKeyPerm(const char* type, const char* name,
                                   CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto optionGroup = app->add_option_group(
            "Required", "Update firmware code authentication key permissions");
        optionGroup
            ->add_option(
                "--requestType", requestType,
                "Request type - "
                "0 - most restrictive permitted value, 1 - specified value")
            ->required();
        optionGroup
            ->add_option("-c,--classification", classification,
                         "component classification")
            ->required();
        optionGroup
            ->add_option("-i,--identifier", identifier, "Component identifier")
            ->required();
        optionGroup
            ->add_option("-d,--index", index, "Component classification index")
            ->required();
        optionGroup
            ->add_option(
                "--nonce", nonce,
                "Nonce obtained from Enable Irreversible Configuration command")
            ->required();
        optionGroup
            ->add_option("-p,--perm", permissionBitmapHexstring,
                         "Hexadecimal string containing permission bitmap data")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_update_req) +
            permissionBitmapHexstring.size() / 2);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());

        std::vector<uint8_t> bitmap;
        for (size_t i = 0; i < permissionBitmapHexstring.length(); i += 2)
        {
            std::string byteString = permissionBitmapHexstring.substr(i, 2);
            uint8_t byte = (uint8_t)strtoul(byteString.c_str(), NULL, 16);
            bitmap.push_back(byte);
        }

        auto rc = encode_nsm_code_auth_key_perm_update_req(
            0, requestType, classification, identifier, index, nonce,
            bitmap.size(), bitmap.data(), request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint32_t updateMethod = 0;

        auto rc = decode_nsm_code_auth_key_perm_update_resp(
            responsePtr, payloadLength, &cc, &reasonCode, &updateMethod);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reasonCode << "\n";
            return;
        }

        nlohmann::ordered_json result;
        result["Completion code"] = cc;
        result["Reason code"] = reasonCode;
        // Fill update methods response
        ordered_json updateMethods;
        bitfield32_t updateMethodBits = {updateMethod};
        if (updateMethodBits.bits.bit0)
        {
            updateMethods.push_back("Automatic");
        }
        if (updateMethodBits.bits.bit1)
        {
            updateMethods.push_back("Self-Contained");
        }
        if (updateMethodBits.bits.bit2)
        {
            updateMethods.push_back("Medium-specific reset");
        }
        if (updateMethodBits.bits.bit3)
        {
            updateMethods.push_back("System reboot");
        }
        if (updateMethodBits.bits.bit4)
        {
            updateMethods.push_back("DC power cycle");
        }
        if (updateMethodBits.bits.bit5)
        {
            updateMethods.push_back("AC power cycle");
        }
        if (updateMethodBits.bits.bit16)
        {
            updateMethods.push_back("Warm Reset");
        }
        if (updateMethodBits.bits.bit17)
        {
            updateMethods.push_back("Hot Reset");
        }
        if (updateMethodBits.bits.bit18)
        {
            updateMethods.push_back("Function Level Reset");
        }
        result["UpdateMethods"] = updateMethods;

        DisplayInJson(result);
    }

  private:
    nsm_code_auth_key_perm_request_type requestType;
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
    uint64_t nonce;
    std::string permissionBitmapHexstring;
};

class QueryFirmwareSecurityVersion : public CommandInterface
{
  public:
    ~QueryFirmwareSecurityVersion() = default;
    QueryFirmwareSecurityVersion() = delete;
    QueryFirmwareSecurityVersion(const QueryFirmwareSecurityVersion&) = delete;
    QueryFirmwareSecurityVersion(QueryFirmwareSecurityVersion&&) = default;
    QueryFirmwareSecurityVersion&
        operator=(const QueryFirmwareSecurityVersion&) = delete;
    QueryFirmwareSecurityVersion&
        operator=(QueryFirmwareSecurityVersion&&) = default;

    explicit QueryFirmwareSecurityVersion(const char* type, const char* name,
                                          CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required", "Parameters for Query Minimum Security Version");
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
        printf("createRequestMsg() called");
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_req_command));
        nsm_firmware_security_version_number_req nsm_req;
        nsm_req.component_classification = htole16(classification);
        nsm_req.component_classification_index = index;
        nsm_req.component_identifier = htole16(identifier);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_query_firmware_security_version_number_req(
            instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        struct nsm_firmware_security_version_number_resp sec_info;

        auto rc = decode_nsm_query_firmware_security_version_number_resp(
            responsePtr, payloadLength, &cc, &reason_code, &sec_info);
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
        result["Security Version"] = static_cast<uint16_t>(
            htole16(sec_info.active_component_security_version));
        result["Pending Security Version"] = static_cast<uint16_t>(
            htole16(sec_info.pending_component_security_version));
        result["Minimum Security Version"] =
            static_cast<uint16_t>(htole16(sec_info.minimum_security_version));
        result["Pending Minimum Security Version"] = static_cast<uint16_t>(
            htole16(sec_info.pending_minimum_security_version));

        DisplayInJson(result);
    }

  private:
    uint16_t classification{};
    uint16_t identifier{};
    uint8_t index{};
};

class UpdateMinSecurityVersion : public CommandInterface
{
  public:
    ~UpdateMinSecurityVersion() = default;
    UpdateMinSecurityVersion() = delete;
    UpdateMinSecurityVersion(const UpdateMinSecurityVersion&) = delete;
    UpdateMinSecurityVersion(UpdateMinSecurityVersion&&) = default;
    UpdateMinSecurityVersion&
        operator=(const UpdateMinSecurityVersion&) = delete;
    UpdateMinSecurityVersion& operator=(UpdateMinSecurityVersion&&) = default;

    explicit UpdateMinSecurityVersion(const char* type, const char* name,
                                      CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required", "Parameters for Update Minimum Security Version");
        ccOptionGroup
            ->add_option(
                "--requestType", requestType,
                "Request Type. 0 - most restrictive permitted value, 1 - specified value")
            ->required();
        ccOptionGroup->add_option("--classification", classification,
                                  "Component classification");
        ccOptionGroup->add_option("--identifier", identifier,
                                  "Component identifier");
        ccOptionGroup->add_option("--index", index, "Component index");
        ccOptionGroup
            ->add_option(
                "--nonce", nonce,
                "Nonce obtained from Enable Irreversible Configuration command")
            ->required();
        ccOptionGroup->add_option("--reqMinSecVersion", reqMinSecVersion,
                                  "Required if request type is 1");
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_update_min_sec_ver_req_command));
        nsm_firmware_update_min_sec_ver_req nsm_req;
        nsm_req.request_type = requestType;
        nsm_req.component_classification = htole16(classification);
        nsm_req.component_classification_index = index;
        nsm_req.component_identifier = htole16(identifier);
        nsm_req.nonce = nonce;
        nsm_req.req_min_security_version = htole16(reqMinSecVersion);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &nsm_req,
                                                         request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        struct nsm_firmware_update_min_sec_ver_resp sec_info;

        auto rc = decode_nsm_firmware_update_sec_ver_resp(
            responsePtr, payloadLength, &cc, &reason_code, &sec_info);
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
        // Fill update methods response
        ordered_json updateMethods;
        bitfield32_t updateMethodBits = {sec_info.update_methods};
        if (updateMethodBits.bits.bit0)
        {
            updateMethods.push_back("Automatic");
        }
        if (updateMethodBits.bits.bit1)
        {
            updateMethods.push_back("Self-Contained");
        }
        if (updateMethodBits.bits.bit2)
        {
            updateMethods.push_back("Medium-specific reset");
        }
        if (updateMethodBits.bits.bit3)
        {
            updateMethods.push_back("System reboot");
        }
        if (updateMethodBits.bits.bit4)
        {
            updateMethods.push_back("DC power cycle");
        }
        if (updateMethodBits.bits.bit5)
        {
            updateMethods.push_back("AC power cycle");
        }
        if (updateMethodBits.bits.bit16)
        {
            updateMethods.push_back("Warm Reset");
        }
        if (updateMethodBits.bits.bit17)
        {
            updateMethods.push_back("Hot Reset");
        }
        if (updateMethodBits.bits.bit17)
        {
            updateMethods.push_back("Function Level Reset");
        }
        result["UpdateMethods"] = updateMethods;

        DisplayInJson(result);
    }

  private:
    uint16_t classification{};
    uint16_t identifier{};
    uint8_t index{};
    uint8_t requestType;
    uint64_t nonce;
    uint16_t reqMinSecVersion;
};

class IrreversibleConfig : public CommandInterface
{
  public:
    ~IrreversibleConfig() = default;
    IrreversibleConfig() = delete;
    IrreversibleConfig(const IrreversibleConfig&) = delete;
    IrreversibleConfig(IrreversibleConfig&&) = default;
    IrreversibleConfig& operator=(const IrreversibleConfig&) = delete;
    IrreversibleConfig& operator=(IrreversibleConfig&&) = default;

    explicit IrreversibleConfig(const char* type, const char* name,
                                CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required", "Parameters for Irreversible Config Method");
        ccOptionGroup
            ->add_option("--requestType", requestType,
                         "Request Type. 0 - Query, 1 - Disable, 2 - Enable")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_req_command));
        nsm_firmware_irreversible_config_req nsm_req;
        nsm_req.request_type = requestType;
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        ordered_json result;
        switch (requestType)
        {
            case QUERY_IRREVERSIBLE_CFG:
            {
                struct nsm_firmware_irreversible_config_request_0_resp
                    cfg_0_resp
                {};
                auto rc =
                    decode_nsm_firmware_irreversible_config_request_0_resp(
                        responsePtr, payloadLength, &cc, &reason_code,
                        &cfg_0_resp);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr << "Response message error: "
                              << "rc=" << rc << ", cc=" << (int)cc
                              << ", reasonCode=" << (int)reason_code << "\n";
                    return;
                }
                result["IrreversibleConfigurationState"] =
                    cfg_0_resp.irreversible_config_state;
                break;
            }
            case DISABLE_IRREVERSIBLE_CFG:
            {
                auto rc =
                    decode_nsm_firmware_irreversible_config_request_1_resp(
                        responsePtr, payloadLength, &cc, &reason_code);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr << "Response message error: "
                              << "rc=" << rc << ", cc=" << (int)cc
                              << ", reasonCode=" << (int)reason_code << "\n";
                    return;
                }
                break;
            }
            case ENABLE_IRREVERSIBLE_CFG:
            {
                struct nsm_firmware_irreversible_config_request_2_resp
                    cfg_2_resp
                {};
                auto rc =
                    decode_nsm_firmware_irreversible_config_request_2_resp(
                        responsePtr, payloadLength, &cc, &reason_code,
                        &cfg_2_resp);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr << "Response message error: "
                              << "rc=" << rc << ", cc=" << (int)cc
                              << ", reasonCode=" << (int)reason_code << "\n";
                    return;
                }
                result["Nonce"] = static_cast<uint64_t>(cfg_2_resp.nonce);
                break;
            }
            default:
                std::cerr << "Unknown request type " << requestType << "\n";
                break;
        }
        result["Completion code"] = cc;
        result["Reason code"] = reason_code;
        DisplayInJson(result);
    }

  private:
    uint8_t requestType;
};

void registerCommand(CLI::App& app)
{
    auto firmware = app.add_subcommand("firmware",
                                       "Device firmware type commands");
    firmware->require_subcommand(1);

    auto getRotInformation = firmware->add_subcommand(
        "GetRotInformation",
        "Get information about a particular firmware set installed on an endpoint");
    commands.push_back(std::make_unique<GetRotInformation>(
        "firmware", "QueryRoTStateInformation", getRotInformation));
    auto irreversibleConfig = firmware->add_subcommand(
        "IrreversibleConfig",
        "Query/Disable/Enable Irreversible Configuration");
    commands.push_back(std::make_unique<IrreversibleConfig>(
        "firmware", "IrreversibleConfig", irreversibleConfig));
    auto queryCodeAuthKeyPerm = firmware->add_subcommand(
        "QueryFWCodeAuthKey",
        "Query firmware code authentication key permissions");
    commands.push_back(std::make_unique<QueryCodeAuthKeyPerm>(
        "firmware", "QueryFWCodeAuthKey", queryCodeAuthKeyPerm));
    auto updateCodeAuthKeyPerm = firmware->add_subcommand(
        "UpdateCodeAuthKeyPerm",
        "Update firmware code authentication key permissions");
    commands.push_back(std::make_unique<UpdateCodeAuthKeyPerm>(
        "firmware", "UpdateCodeAuthKeyPerm", updateCodeAuthKeyPerm));
    auto queryFirmwareSecurityVersion = firmware->add_subcommand(
        "QueryFirmwareSecurityVersion", "Query Firmware Security Version");
    commands.push_back(std::make_unique<QueryFirmwareSecurityVersion>(
        "firmware", "QueryFirmwareSecurityVersion",
        queryFirmwareSecurityVersion));
    auto updateMinSecurityVersion = firmware->add_subcommand(
        "UpdateMinSecurityVersion", "Update Minimum Firmware Security Version");
    commands.push_back(std::make_unique<UpdateMinSecurityVersion>(
        "firmware", "UpdateMinSecurityVersion", updateMinSecurityVersion));
}
} // namespace nsmtool::firmware
