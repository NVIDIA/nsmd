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
#include "nsmDotUtils.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace nsmtool::firmware
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

// DOT CAK/LAK key size constants
constexpr size_t ECDSA_KEY_SIZE =
    96; // Total ECDSA key size (x + y coordinates)
constexpr size_t ECDSA_COORDINATE_SIZE = 48; // Each coordinate (x or y) size
constexpr size_t LMS_KEY_SIZE = 48;          // LMS public key size
constexpr size_t AUTH_SCHEME_SIZE =
    4; // Authentication scheme field size (NvU32)
constexpr size_t CRYPTO_PCP_SIZE = AUTH_SCHEME_SIZE + ECDSA_COORDINATE_SIZE +
                                   ECDSA_COORDINATE_SIZE +
                                   LMS_KEY_SIZE; // 148 bytes total

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
            ->add_option("-c,--classification", classification,
                         "Component classification")
            ->required();
        ccOptionGroup
            ->add_option("-i,--identifier", identifier, "Component identifier")
            ->required();
        ccOptionGroup->add_option("-d,--index", index, "Component index")
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
        result["Background copy policy persistent"] = mapEnumToString(
            static_cast<uint32_t>(erot_info.fq_resp_hdr.background_copy_policy),
            bgCopyPolicyMap);
        result["Active Slot"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.active_slot);
        result["Active Keyset"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.active_keyset);
        result["Minimum security version"] = static_cast<uint32_t>(
            erot_info.fq_resp_hdr.minimum_security_version);
        result["Inband update policy persistent"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.inband_update_policy);
        result["Boot status code"] =
            static_cast<uint64_t>(erot_info.fq_resp_hdr.boot_status_code);
        result["Firmware slot count"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.firmware_slot_count);
        result["Inband update policy current"] = static_cast<uint32_t>(
            erot_info.fq_resp_hdr.inband_update_policy_current);
        result["Background copy policy current"] = static_cast<uint32_t>(
            erot_info.fq_resp_hdr.background_copy_policy_current);

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
        {6, "Failed authentication"},
        {7, "Pending image copy"},
        {8, "Image copy in progress"},
        {9, "Failed image copy"}};

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
            ->add_option("-c,--classification", classification,
                         "Component classification")
            ->required();
        optionGroup
            ->add_option("-i,--identifier", identifier, "Component identifier")
            ->required();
        optionGroup->add_option("-d,--index", index, "Component index")
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
                "-r,--requestType", requestType,
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
                "-n,--nonce", nonce,
                "Nonce obtained from Enable Irreversible Configuration command")
            ->required();
        optionGroup->add_option(
            "-k,--keys", revokedKeysString,
            "Comma-separated list of indexes of keys to be revoked. "
            "Cannot be used when request type is set to 0. "
            "Required when request type is set to 1.");
        optionGroup->add_option(
            "-b,--bitmap", bitmapSize,
            "Size of the permission bitmap to be sent. "
            "If set to 0 or omitted, the size is calculated automatically "
            "based on the provided indexes.");
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> indices;
        if (requestType == 0 && !revokedKeysString.empty())
        {
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }
        if (requestType == 1)
        {
            std::istringstream iss{revokedKeysString};
            std::string indexStr;
            while (getline(iss, indexStr, ','))
            {
                try
                {
                    indices.emplace_back(static_cast<uint8_t>(stoul(indexStr)));
                }
                catch (const std::exception&)
                {
                    return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
                }
            }
        }
        std::vector<uint8_t> bitmap;
        try
        {
            bitmap = utils::indicesToBitmap(indices, bitmapSize);
        }
        catch (const std::exception&)
        {
            return std::make_pair(NSM_SW_ERROR_LENGTH, std::vector<uint8_t>());
        }
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_update_req) +
            bitmap.size());
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
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
    uint32_t bitmapSize;
    std::string revokedKeysString;
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
            ->add_option("-c,--classification", classification,
                         "Component classification")
            ->required();
        ccOptionGroup
            ->add_option("-i,--identifier", identifier, "Component identifier")
            ->required();
        ccOptionGroup->add_option("-d,--index", index, "Component index")
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
                "-r,--requestType", requestType,
                "Request Type. 0 - most restrictive permitted value, 1 - specified value")
            ->required();
        ccOptionGroup->add_option("-c,--classification", classification,
                                  "Component classification");
        ccOptionGroup->add_option("-i,--identifier", identifier,
                                  "Component identifier");
        ccOptionGroup->add_option("-d,--index", index, "Component index");
        ccOptionGroup
            ->add_option(
                "-n,--nonce", nonce,
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
            ->add_option("-r,--requestType", requestType,
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
                    cfg_0_resp{};
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
                    cfg_2_resp{};
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

class SetRoTProperty : public CommandInterface
{
  public:
    ~SetRoTProperty() = default;
    SetRoTProperty() = delete;
    SetRoTProperty(const SetRoTProperty&) = delete;
    SetRoTProperty(SetRoTProperty&&) = default;
    SetRoTProperty& operator=(const SetRoTProperty&) = delete;
    SetRoTProperty& operator=(SetRoTProperty&&) = default;

    explicit SetRoTProperty(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required", "Parameters for Set RoT Property");
        ccOptionGroup->add_option("-c,--classification", classification,
                                  "Component classification");
        ccOptionGroup->add_option("-i,--identifier", identifier,
                                  "Component identifier");
        ccOptionGroup->add_option("-d,--index", index, "Component index");
        ccOptionGroup
            ->add_option(
                "-p,--property", property,
                "Property (0: Redundancy Policy, 1: In-band Update Policy)")
            ->check(CLI::Range(0, 1))
            ->required();
        ccOptionGroup
            ->add_option(
                "-r,--redundancy-policy", redundancyPolicy,
                "Redundancy Policy (0: Manual Background Copy, 1: Automatic Background Copy) - only for Property 0")
            ->check(CLI::Range(0, 1));
        ccOptionGroup
            ->add_option(
                "-u,--update-policy", updatePolicy,
                "In-band Update Policy (0: Disable, 1: Enable) - only for Property 1")
            ->check(CLI::Range(0, 1));
        ccOptionGroup
            ->add_option(
                "-l,--lifespan", lifespan,
                "Lifespan (0: Persistent, 1: One-shot for Property 0, Volatile for Property 1)")
            ->check(CLI::Range(0, 1));
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_req_command));
        nsm_firmware_set_rot_property_req nsm_req;
        nsm_req.component_classification = htole16(classification);
        nsm_req.component_classification_index = index;
        nsm_req.component_identifier = htole16(identifier);
        nsm_req.property = property;
        nsm_req.argument_length =
            ARGUMENT_DATA_LENGTH; // Fixed length for both properties

        // Populate argument data based on property value
        if (property == 0)
        {
            // Property 0: Redundancy Policy + Lifespan
            nsm_req.argument_data[0] = redundancyPolicy;
            nsm_req.argument_data[1] = lifespan;
        }
        else if (property == 1)
        {
            // Property 1: In-band Update Policy + Lifespan
            nsm_req.argument_data[0] = updatePolicy;
            nsm_req.argument_data[1] = lifespan;
        }

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &nsm_req,
                                                           request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_nsm_firmware_set_rot_property_resp(
            responsePtr, payloadLength, &cc, &reason_code);
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
        DisplayInJson(result);
    }

  private:
    uint16_t classification{DEFAULT_VALUE};
    uint16_t identifier{DEFAULT_VALUE};
    uint8_t index{DEFAULT_VALUE};
    uint8_t property{};
    uint8_t redundancyPolicy{DEFAULT_VALUE};
    uint8_t updatePolicy{DEFAULT_VALUE};
    uint8_t lifespan{};

    static constexpr uint8_t ARGUMENT_DATA_LENGTH = 2;
    static constexpr uint8_t DEFAULT_VALUE = 255;
};

class DotCAKInstall : public CommandInterface
{
  public:
    ~DotCAKInstall() = default;
    DotCAKInstall() = delete;
    DotCAKInstall(const DotCAKInstall&) = delete;
    DotCAKInstall(DotCAKInstall&&) = default;
    DotCAKInstall& operator=(const DotCAKInstall&) = delete;
    DotCAKInstall& operator=(DotCAKInstall&&) = default;

    explicit DotCAKInstall(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup =
            app->add_option_group("Required", "Parameters for DotCAKInstall");

        // CAK Key Auth Scheme - must be 0 or 1
        ccOptionGroup
            ->add_option(
                "--cak_key_auth_scheme", cakKeyAuthScheme,
                "Valid values are 0 and 1, 0-DOT_LOCK allowed, 1 not allowed")
            ->required()
            ->check(CLI::Range(0, 1));

        // CAK Key ECDSA file - must be valid file path and exactly 96 bytes
        ccOptionGroup
            ->add_option("--cak_ecdsa_key", cakKeyEcdsaKeyFile,
                         "File containing 96 Bytes of ECDSA data (raw bytes)")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, ECDSA_KEY_SIZE, "CAK key");
        });

        // CAK LMS file - optional, only required for hybrid auth scheme (1)
        // For ECDSA-only auth scheme (0), LMS will be filled with 48 bytes of
        // zeros
        app->add_option(
               "--cak_lms_key", cakLmsKeyFile,
               "File containing 48 Bytes (raw bytes) - required only for hybrid auth scheme (1)")
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, LMS_KEY_SIZE, "CAK LMS");
        });

        // LAK Key Auth Scheme - must be 0 or 1
        ccOptionGroup
            ->add_option("--lak_key_auth_scheme", lakKeyAuthScheme,
                         "LAK key authentication scheme (0 or 1)")
            ->required()
            ->check(CLI::Range(0, 1));

        // LAK Key ECDSA file - must be valid file path and exactly 96 bytes
        ccOptionGroup
            ->add_option("--lak_ecdsa_key", lakKeyEcdsaKeyFile,
                         "File containing 96 Bytes LAK key (raw bytes)")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, ECDSA_KEY_SIZE, "LAK key");
        });

        // LAK LMS file - optional, only required for hybrid auth scheme (1)
        // For ECDSA-only auth scheme (0), LMS will be filled with 48 bytes of
        // zeros
        app->add_option(
               "--lak_lms_key", lakLmsKeyFile,
               "File containing 48 Bytes LAK LMS (raw bytes) - required only for hybrid auth scheme (1)")
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, LMS_KEY_SIZE, "LAK LMS");
        });

        // Lock Disable - must be 0 or 1 (default: 0 = lock enabled)
        ccOptionGroup
            ->add_option("--lock_disable", lockDisable,
                         "Contains state for lock allowing (default: 0)")
            ->default_val(0)
            ->check(CLI::Range(0, 1));

        // Min SVN - must be valid uint32_t (0 to UINT32_MAX)
        ccOptionGroup
            ->add_option("--min_svn", minSvn,
                         "Minimum Firmware Security Version")
            ->required()
            ->check(CLI::Range(0U, UINT32_MAX));
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        // Read CAK key from file
        std::vector<uint8_t> cakKey = readFileAsBytes(cakKeyEcdsaKeyFile);
        if (cakKey.empty())
        {
            std::cerr << "Error: Failed to read CAK key file: "
                      << cakKeyEcdsaKeyFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        // Read CAK LMS from file (only for Hybrid auth scheme)
        std::vector<uint8_t> cakLms;
        if (cakKeyAuthScheme == 1) // Hybrid (ECDSA + LMS)
        {
            if (cakLmsKeyFile.empty())
            {
                std::cerr
                    << "Error: CAK LMS key file required for hybrid auth scheme\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
            cakLms = readFileAsBytes(cakLmsKeyFile);
            if (cakLms.empty())
            {
                std::cerr << "Error: Failed to read CAK LMS file: "
                          << cakLmsKeyFile << "\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
        }
        else // ECDSA only - fill with zeros
        {
            cakLms.resize(LMS_KEY_SIZE, 0);
        }

        // Read LAK key from file
        std::vector<uint8_t> lakKey = readFileAsBytes(lakKeyEcdsaKeyFile);
        if (lakKey.empty())
        {
            std::cerr << "Error: Failed to read LAK key file: "
                      << lakKeyEcdsaKeyFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        // Read LAK LMS from file (only for Hybrid auth scheme)
        std::vector<uint8_t> lakLms;
        if (lakKeyAuthScheme == 1) // Hybrid (ECDSA + LMS)
        {
            if (lakLmsKeyFile.empty())
            {
                std::cerr
                    << "Error: LAK LMS key file required for hybrid auth scheme\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
            lakLms = readFileAsBytes(lakLmsKeyFile);
            if (lakLms.empty())
            {
                std::cerr << "Error: Failed to read LAK LMS file: "
                          << lakLmsKeyFile << "\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
        }
        else // ECDSA only - fill with zeros
        {
            lakLms.resize(LMS_KEY_SIZE, 0);
        }

        std::vector<uint8_t> cakCryptoPcp(CRYPTO_PCP_SIZE, 0);
        if (!nsm::dot::buildKeyAuthData(cakKeyAuthScheme, cakKey.data(),
                                        cakLms.data(), cakCryptoPcp.data()))
        {
            std::cerr << "Error: Failed to build CAK key authentication data\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> lakCryptoPcp(CRYPTO_PCP_SIZE, 0);
        if (!nsm::dot::buildKeyAuthData(lakKeyAuthScheme, lakKey.data(),
                                        lakLms.data(), lakCryptoPcp.data()))
        {
            std::cerr << "Error: Failed to build LAK key authentication data\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        // Display key authentication data in hex format (only when verbose is
        // enabled)
        if (isVerbose())
        {
            std::cout << "CAK key authentication data (" << CRYPTO_PCP_SIZE
                      << " bytes): ";
            for (size_t i = 0; i < cakCryptoPcp.size(); ++i)
            {
                std::cout << "0x" << std::hex << std::setw(2)
                          << std::setfill('0')
                          << static_cast<int>(cakCryptoPcp[i]);
                if (i < cakCryptoPcp.size() - 1)
                    std::cout << ", ";
            }
            std::cout << std::dec << std::endl;

            std::cout << "LAK key authentication data (" << CRYPTO_PCP_SIZE
                      << " bytes): ";
            for (size_t i = 0; i < lakCryptoPcp.size(); ++i)
            {
                std::cout << "0x" << std::hex << std::setw(2)
                          << std::setfill('0')
                          << static_cast<int>(lakCryptoPcp[i]);
                if (i < lakCryptoPcp.size() - 1)
                    std::cout << ", ";
            }
            std::cout << std::dec << std::endl;
        }

        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_install_req_command));
        nsm_dot_cak_install_req nsm_req;

        // Copy CAK key authentication data (148 bytes per spec)
        memcpy(nsm_req.cak_pub, cakCryptoPcp.data(), CRYPTO_PCP_SIZE);

        // Copy LAK key authentication data (148 bytes per spec)
        memcpy(nsm_req.lak_pub, lakCryptoPcp.data(), CRYPTO_PCP_SIZE);

        nsm_req.lock_disable = lockDisable;
        nsm_req.min_svn = htole32(minSvn);

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_cak_install_req(instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        // Validate input parameters
        if (responsePtr == nullptr)
        {
            std::cerr << "Error: Response pointer is null\n";
            return;
        }

        if (payloadLength < sizeof(nsm_common_resp))
        {
            std::cerr << "Error: Payload length too small, expected at least "
                      << sizeof(nsm_common_resp) << " bytes, got "
                      << payloadLength << " bytes\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_nsm_dot_cak_install_resp(responsePtr, payloadLength,
                                                  &cc, &reason_code);
        if (rc != NSM_SW_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        // Get response fields from nsm_common_resp
        struct nsm_common_resp* resp =
            (struct nsm_common_resp*)responsePtr->payload;

        // Validate response data types
        if (resp->command != NSM_FW_DOT_CAK_INSTALL)
        {
            std::cerr << "Warning: Unexpected command code in response: 0x"
                      << std::hex << (int)resp->command << ", expected: 0x"
                      << (int)NSM_FW_DOT_CAK_INSTALL << std::dec << "\n";
        }

        // Output response fields according to spec
        ordered_json result;

        // Add NSM header info as hex bytes
        std::stringstream nsmHeaderHex;
        for (int i = 0; i < 5; i++)
        {
            nsmHeaderHex << std::hex << std::setw(2) << std::setfill('0')
                         << (int)((uint8_t*)&responsePtr->hdr)[i];
            if (i < 4)
                nsmHeaderHex << " ";
        }
        result["nsm_header"] = nsmHeaderHex.str();

        std::stringstream cmdCode, compCode, res;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)resp->command;
        compCode << std::hex << std::setw(2) << std::setfill('0')
                 << (int)resp->completion_code;
        res << std::hex << std::setw(4) << std::setfill('0')
            << (int)resp->reserved;

        result["Command code"] = cmdCode.str();
        result["Completion code"] = compCode.str();
        result["Reserved"] = res.str();

        DisplayInJson(result);
    }

  private:
    uint8_t cakKeyAuthScheme;
    std::string cakKeyEcdsaKeyFile;
    std::string cakLmsKeyFile;
    uint8_t lakKeyAuthScheme;
    std::string lakKeyEcdsaKeyFile;
    std::string lakLmsKeyFile;
    uint8_t lockDisable;
    uint32_t minSvn;

    // Helper function to validate file size
    std::string validateFileSize(const std::string& filename,
                                 size_t expectedSize,
                                 const std::string& fileType)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file)
        {
            return "Cannot open " + fileType + " file: " + filename;
        }

        file.seekg(0, std::ios::end);
        size_t actualSize = file.tellg();
        file.close();

        if (actualSize != expectedSize)
        {
            return fileType + " file must contain exactly " +
                   std::to_string(expectedSize) + " bytes, got " +
                   std::to_string(actualSize) + " bytes";
        }

        return ""; // Empty string means validation passed
    }

    std::vector<uint8_t> readFileAsBytes(const std::string& filename)
    {
        // Validate filename
        if (filename.empty())
        {
            std::cerr << "Error: Filename is empty\n";
            return {};
        }

        std::ifstream file(filename, std::ios::binary);
        if (!file)
        {
            std::cerr << "Error: Cannot open file " << filename << "\n";
            return {};
        }

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Validate file size
        if (size == 0)
        {
            std::cerr << "Error: File " << filename << " is empty\n";
            return {};
        }

        if (size > SIZE_MAX)
        {
            std::cerr << "Error: File " << filename
                      << " is too large (size: " << size << " bytes)\n";
            return {};
        }

        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);

        // Validate read operation
        if (file.gcount() != static_cast<std::streamsize>(size))
        {
            std::cerr << "Error: Failed to read complete file " << filename
                      << " (expected: " << size
                      << " bytes, read: " << file.gcount() << " bytes)\n";
            return {};
        }

        return buffer;
    }
};

class DotCAKBypass : public CommandInterface
{
  public:
    ~DotCAKBypass() = default;
    DotCAKBypass() = delete;
    DotCAKBypass(const DotCAKBypass&) = delete;
    DotCAKBypass(DotCAKBypass&&) = default;
    DotCAKBypass& operator=(const DotCAKBypass&) = delete;
    DotCAKBypass& operator=(DotCAKBypass&&) = default;

    explicit DotCAKBypass(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        // No parameters required for this command
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_dot_cak_bypass_req));

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_cak_bypass_req(instanceId, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (payloadLength < sizeof(nsm_common_resp))
        {
            std::cerr << "Response payload length too short\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_nsm_dot_cak_bypass_resp(responsePtr, payloadLength,
                                                 &cc, &reason_code);
        if (rc != NSM_SW_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        // Output response fields according to spec
        ordered_json result;

        // Add NSM header info as hex bytes
        std::stringstream nsmHeaderHex;
        for (int i = 0; i < 5; i++)
        {
            nsmHeaderHex << std::hex << std::setw(2) << std::setfill('0')
                         << (int)((uint8_t*)&responsePtr->hdr)[i];
            if (i < 4)
                nsmHeaderHex << " ";
        }
        result["nsm_header"] = nsmHeaderHex.str();

        // For error responses, show reason code instead of detailed fields
        if (cc != NSM_SUCCESS)
        {
            std::stringstream cmdCode;
            cmdCode << std::hex << std::setw(2) << std::setfill('0')
                    << (int)NSM_FW_DOT_CAK_BYPASS;
            result["Command code"] = cmdCode.str();

            std::stringstream compCode;
            compCode << std::hex << std::setw(2) << std::setfill('0')
                     << (int)cc;
            result["Completion code"] = compCode.str();

            std::stringstream reasonCode;
            reasonCode << std::hex << std::setw(4) << std::setfill('0')
                       << (int)reason_code;
            result["reasonCode"] = reasonCode.str();
        }
        else
        {
            // Success case - show all fields per spec
            auto* resp =
                reinterpret_cast<nsm_common_resp*>(responsePtr->payload);
            std::stringstream cmdCode, compCode, res;
            cmdCode << std::hex << std::setw(2) << std::setfill('0')
                    << (int)resp->command;
            compCode << std::hex << std::setw(2) << std::setfill('0')
                     << (int)resp->completion_code;
            res << std::hex << std::setw(4) << std::setfill('0')
                << (int)le16toh(resp->reserved);

            result["Command code"] = cmdCode.str();
            result["Completion code"] = compCode.str();
            result["Reserved"] = res.str();
        }

        DisplayInJson(result);
    }
};

class ImageCopyControl : public CommandInterface
{
  public:
    ~ImageCopyControl() = default;
    ImageCopyControl() = delete;
    ImageCopyControl(const ImageCopyControl&) = delete;
    ImageCopyControl(ImageCopyControl&&) = default;
    ImageCopyControl& operator=(const ImageCopyControl&) = delete;
    ImageCopyControl& operator=(ImageCopyControl&&) = default;
    explicit ImageCopyControl(const char* type, const char* name,
                              CLI::App* app) : CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required", "Parameters for Image Copy Control");
        ccOptionGroup
            ->add_option(
                "-r,--requestType", requestType,
                "Request Type (0: Query Image Copy Progress, 1: Initiate Image Copy)")
            ->check(CLI::Range(0, 1))
            ->required();
        ccOptionGroup->add_option("-n,--componentCount", componentCount,
                                  "The number of component identities.");
        ccOptionGroup->add_option(
            "-c,--classification", classifications,
            "Component classification(s) - can be specified multiple times");
        ccOptionGroup->add_option(
            "-i,--identifier", identifiers,
            "Component identifier(s) - can be specified multiple times");
        ccOptionGroup->add_option(
            "-d,--index", indices,
            "Component classification index/indices - can be specified multiple times");
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        // Validate that all component arrays have the same size
        if (!classifications.empty() || !identifiers.empty() ||
            !indices.empty())
        {
            size_t maxSize = std::max(
                {classifications.size(), identifiers.size(), indices.size()});
            if (classifications.size() != maxSize ||
                identifiers.size() != maxSize || indices.size() != maxSize)
            {
                std::cerr
                    << "Error: All component parameters (classification, identifier, index) "
                    << "must be specified the same number of times\n";
                return std::make_pair(-1, std::vector<uint8_t>());
            }
        }

        uint8_t compCount = static_cast<uint8_t>(classifications.size());

        // Validate component count if expectedComponentCount was specified
        if (componentCount != compCount)
        {
            std::cerr << "Error: Expected component count ("
                      << (int)componentCount
                      << ") does not match actual component count ("
                      << (int)compCount << ")\n";
            return std::make_pair(-1, std::vector<uint8_t>());
        }

        // Calculate message size: header + common_req + request + (component
        // entries)
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
            sizeof(nsm_firmware_image_copy_control_req) +
            (compCount * sizeof(nsm_firmware_image_copy_component_entry)));

        // Build component entries array
        std::vector<nsm_firmware_image_copy_component_entry> componentEntries;
        componentEntries.reserve(compCount);
        for (size_t i = 0; i < compCount; ++i)
        {
            nsm_firmware_image_copy_component_entry entry;
            entry.component_classification = classifications[i];
            entry.component_identifier = identifiers[i];
            entry.component_classification_index = indices[i];
            componentEntries.push_back(entry);
        }

        nsm_firmware_image_copy_control_req nsm_req;
        nsm_req.request_type = requestType;
        nsm_req.component_count = compCount;

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_firmware_image_copy_control_req(
            instanceId, &nsm_req,
            compCount > 0 ? componentEntries.data() : nullptr, request);

        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        ordered_json result;
        switch (requestType)
        {
            case NSM_IMAGE_COPY_QUERY_PROGRESS: // Query Image Copy
                                                // Progress
            {
                struct nsm_firmware_image_copy_control_query_progress_resp
                    image_copy_control_query{};
                auto rc =
                    decode_nsm_firmware_image_copy_control_query_progress_resp(
                        responsePtr, payloadLength, &cc, &reason_code,
                        &image_copy_control_query);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr << "Response message error: " << "rc=" << rc
                              << "\n";
                    if (cc != NSM_SUCCESS)
                    {
                        std::cerr << "  Completion code: 0x" << std::hex
                                  << (int)cc << std::dec << " - "
                                  << getCompletionCodeDescription(cc) << "\n";
                    }
                    if (reason_code != ERR_NULL)
                    {
                        std::cerr << "  Reason code: 0x" << std::hex
                                  << reason_code << std::dec << " - "
                                  << getReasonCodeDescription(reason_code)
                                  << "\n";
                    }
                    return;
                }

                if (image_copy_control_query.image_copy_progress >
                    UNSUPPORTED_PROGRESS_PERCENT)
                {
                    std::cerr << "Incorrect progress percentage: "
                              << image_copy_control_query.image_copy_progress
                              << " (expected: 0-101)\n";
                    return;
                }

                // Map status code to human-readable string
                std::string statusStr;
                switch (image_copy_control_query.image_copy_status)
                {
                    case NSM_IMAGE_COPY_NOT_TRIGGERED:
                        statusStr = "Image copy not triggered";
                        break;
                    case NSM_IMAGE_COPY_IN_PROGRESS:
                        statusStr = "In progress";
                        break;
                    case NSM_IMAGE_COPY_COMPLETE:
                        statusStr = "Complete";
                        break;
                    case NSM_IMAGE_COPY_UNDEFINED_FAILURE:
                        statusStr = "Undefined failure";
                        break;
                    case NSM_IMAGE_COPY_NO_VALID_IMAGE:
                        statusStr = "No valid image";
                        break;
                    case NSM_IMAGE_COPY_DESTINATION_WRITE_PROTECTED:
                        statusStr = "Destination write protected";
                        break;
                    case NSM_IMAGE_COPY_FAIL_FLASH_ACCESS:
                        statusStr = "Fail flash access";
                        break;
                    case NSM_IMAGE_COPY_FAILED_VERIFY:
                        statusStr = "Failed verify";
                        break;
                    default:
                        statusStr =
                            "Unknown status (" +
                            std::to_string(
                                image_copy_control_query.image_copy_status) +
                            ")";
                        break;
                }

                result["Image copy status"] = statusStr;
                result["Image copy status code"] =
                    image_copy_control_query.image_copy_status;

                result["Image copy progress"] =
                    image_copy_control_query.image_copy_progress;
                break;
            }
            case NSM_IMAGE_COPY_INITIATE_IMAGE_COPY: // Initiate Image Copy
            {
                auto rc =
                    decode_nsm_firmware_image_copy_control_initiate_copy_resp(
                        responsePtr, payloadLength, &cc, &reason_code);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr << "Response message error: " << "rc=" << rc
                              << "\n";
                    if (cc != NSM_SUCCESS)
                    {
                        std::cerr << "  Completion code: 0x" << std::hex
                                  << (int)cc << std::dec << " - "
                                  << getCompletionCodeDescription(cc) << "\n";
                    }

                    if (reason_code != ERR_NULL)
                    {
                        std::cerr << "  Reason code: 0x" << std::hex
                                  << reason_code << std::dec << " - "
                                  << getReasonCodeDescription(reason_code)
                                  << "\n";
                    }

                    return;
                }
                break;
            }
            default:
            {
                std::cerr << "Unknown request type " << requestType << "\n";
                break;
            }
        }
        result["Completion code"] = cc;
        result["Reason code"] = reason_code;
        DisplayInJson(result);
    }

  private:
    uint8_t requestType{};
    std::vector<uint16_t> classifications;
    std::vector<uint16_t> identifiers;
    std::vector<uint8_t> indices;
    uint8_t componentCount{DEFAULT_VALUE};

    static constexpr uint8_t DEFAULT_VALUE = 0;
    static constexpr uint8_t UNSUPPORTED_PROGRESS_PERCENT = 101;

    // Helper function to get human-readable completion code description
    static std::string getCompletionCodeDescription(uint8_t cc)
    {
        switch (cc)
        {
            case NSM_ERR_INVALID_DATA:
                return "INVALID_DATA (The request payload contained invalid data or illegal value)";
            case NSM_ERR_INVALID_STATE_FOR_COMMAND:
                return "INVALID_STATE_FOR_COMMAND (The device is not in a state to expect this command)";
            case NSM_ERR_INVALID_REQUEST_TYPE:
                return "INVALID_REQUEST_TYPE (The requested request type is not supported by the device)";
            default:
                return "Unknown completion code";
        }
    }

    // Helper function to get human-readable reason code description
    static std::string getReasonCodeDescription(uint16_t reason_code)
    {
        switch (reason_code)
        {
            case ERR_PROPERTY_NOT_SUPPORTED:
                return "PROPERTY_NOT_SUPPORTED (Property to be updated via SetRoTProperty command is not supported by the device)";
            case ERR_LIFESPAN_VOLATILE_NOT_SUPPORTED:
                return "LIFESPAN_VOLATILE_NOT_SUPPORTED (Volatile lifespan for a property is not supported by device)";
            case ERR_LIFESPAN_PERSISTENT_NOT_SUPPORTED:
                return "LIFESPAN_PERSISTENT_NOT_SUPPORTED (Persistent lifespan for a property is not supported by device)";
            case ERR_NO_BOOT_COMPLETE:
                return "NO_BOOT_COMPLETE (The RoT has not received a boot complete indication from the AP)";
            case ERR_UPDATE_IN_PROGRESS:
                return "UPDATE_IN_PROGRESS (A firmware update is in progress)";
            case ERR_IMAGE_COPY_IN_PROGRESS:
                return "IMAGE_COPY_IN_PROGRESS (An image copy is in progress)";
            case ERR_IMAGE_COPY_COMPLETED:
                return "IMAGE_COPY_COMPLETED (An image copy was completed successfully)";
            case ERR_FLASH_WEAR_MITIGATION:
                return "FLASH_WEAR_MITIGATION (A flash wear out mitigation policy is in effect)";
            case ERR_INCOMPLETE_COMPONENT_SET:
                return "INCOMPLETE_COMPONENT_SET (The RoT requires additional components to be included in the request)";
            default:
                return "Unknown reason code";
        }
    }
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
    auto setRoTProperty = firmware->add_subcommand("SetRoTProperty",
                                                   "Set RoT Property");
    commands.push_back(std::make_unique<SetRoTProperty>(
        "firmware", "SetRoTProperty", setRoTProperty));
    auto dotCAKInstall = firmware->add_subcommand(
        "DotCAKInstall", "Cak install command in uninitialized state");
    commands.push_back(std::make_unique<DotCAKInstall>(
        "firmware", "DotCAKInstall", dotCAKInstall));
    auto dotCAKBypass = firmware->add_subcommand(
        "DotCAKBypass", "Bypass DOT CAK install and continue boot");
    commands.push_back(std::make_unique<DotCAKBypass>(
        "firmware", "DotCAKBypass", dotCAKBypass));
    auto imageCopyControl = firmware->add_subcommand("ImageCopyControl",
                                                     "Image Copy Control");
    commands.push_back(std::make_unique<ImageCopyControl>(
        "firmware", "ImageCopyControl", imageCopyControl));
}
} // namespace nsmtool::firmware
