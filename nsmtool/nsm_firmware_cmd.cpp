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

constexpr size_t ECDSA_KEY_SIZE = 96;
constexpr size_t ECDSA_COORDINATE_SIZE = 48;
constexpr size_t LMS_KEY_SIZE = 48;
constexpr size_t AUTH_SCHEME_SIZE = 4;
constexpr size_t CRYPTO_PCP_SIZE = AUTH_SCHEME_SIZE + ECDSA_COORDINATE_SIZE +
                                   ECDSA_COORDINATE_SIZE + LMS_KEY_SIZE;
constexpr size_t KEY_AUTH_DATA_SIZE = 148;
constexpr uint8_t KEY_AUTH_SCHEME_ECDSA = 0;
constexpr uint8_t KEY_AUTH_SCHEME_HYBRID = 1;
constexpr size_t STATIC_CHALLENGE_SIZE = 32;
constexpr size_t SIGNATURE_SIZE = 1840;
constexpr size_t ECDSA_SIGNATURE_SIZE = 96;
constexpr size_t LMS_SIGNATURE_SIZE = 1744;

/**
 * @brief Validates that a file exists and has the expected size
 *
 * @param filename Path to the file to validate
 * @param expectedSize Expected file size in bytes
 * @param fileType Description of the file type for error messages
 * @return Empty string if valid, error message string if invalid
 */
static std::string validateFileSize(const std::string& filename,
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

    return "";
}

static std::vector<uint8_t> readFileAsBytes(const std::string& filename)
{
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

    if (size == 0)
    {
        std::cerr << "Error: File " << filename << " is empty\n";
        return {};
    }

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        std::cerr << "Error: Failed to read file " << filename << "\n";
        return {};
    }

    return buffer;
}

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
        result["AP SKU ID"] =
            static_cast<uint32_t>(erot_info.fq_resp_hdr.ap_sku_id);
        result["Global failover policy"] = mapEnumToString(
            static_cast<uint32_t>(erot_info.fq_resp_hdr.global_failover_policy),
            globalFailoverPolicyMap);

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

    const std::unordered_map<uint32_t, std::string> globalFailoverPolicyMap = {
        {NSM_ROT_GLOBAL_FAILOVER_POLICY_NO_FAILOVER, "No Failover"},
        {NSM_ROT_GLOBAL_FAILOVER_POLICY_AUTOMATIC_FAILOVER,
         "Automatic Failover"},
        {NSM_ROT_GLOBAL_FAILOVER_POLICY_NOT_APPLICABLE, "Not Applicable"}};

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
    nsm_code_auth_key_perm_request_type requestType{};
    uint16_t classification{};
    uint16_t identifier{};
    uint8_t index{};
    uint64_t nonce{};
    uint32_t bitmapSize{};
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
                "Property (0: Redundancy Policy, 1: In-band Update Policy, 2: AP "
                "SKU ID, 3: Global Failover Policy)")
            ->check(CLI::Range(0, 3))
            ->required();
        ccOptionGroup
            ->add_option(
                "-r,--redundancy-policy", redundancyPolicy,
                "Redundancy Policy (0: Manual Background Copy, 1: Automatic Background Copy) - only for Redundancy Policy")
            ->check(CLI::Range(0, 1));
        ccOptionGroup
            ->add_option(
                "-u,--update-policy", updatePolicy,
                "In-band Update Policy (0: Disable, 1: Enable) - only for In-band Update Policy")
            ->check(CLI::Range(0, 1));
        ccOptionGroup
            ->add_option(
                "-a,--ap-sku-id", apSkuId,
                "AP SKU ID (32-bit unsigned integer) - only for AP SKU ID")
            ->check(CLI::Range(0U, UINT32_MAX));
        ccOptionGroup
            ->add_option(
                "-g,--global-failover-policy", globalFailoverPolicy,
                "Global Failover Policy (0: No Failover, 1: Automatic Failover) - only for Global Failover Policy")
            ->check(CLI::Range(0, 1));
        ccOptionGroup
            ->add_option(
                "-l,--lifespan", lifespan,
                "Lifespan (0: Persistent, 1: One-shot for Redundancy Policy, Volatile for In-band Update Policy) - not applicable for Global Failover Policy")
            ->check(CLI::Range(0, 1));
        // Add parse callback to validate conditional requirements based on
        // property value
        app->parse_complete_callback([this, ccOptionGroup]() {
            auto apSkuIdOption = ccOptionGroup->get_option("--ap-sku-id");
            if (property == NSM_ROT_PROPERTY_AP_SKU_ID &&
                apSkuIdOption->count() == 0)
            {
                throw CLI::ValidationError(
                    "--ap-sku-id",
                    "Option -a,--ap-sku-id is required when property is 2 (AP SKU ID)");
            }
        });
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_req_command));
        nsm_firmware_set_rot_property_req nsm_req = {};
        nsm_req.component_classification = htole16(classification);
        nsm_req.component_classification_index = index;
        nsm_req.component_identifier = htole16(identifier);
        nsm_req.property = property;
        nsm_req.argument_length =
            ARGUMENT_DATA_LENGTH; // Fixed length for first two properties

        // Populate argument data based on property value
        if (property == NSM_ROT_PROPERTY_REDUNDANCY_POLICY)
        {
            // Property 0: Redundancy Policy + Lifespan
            nsm_req.argument_data[0] = redundancyPolicy;
            nsm_req.argument_data[1] = lifespan;
        }
        else if (property == NSM_ROT_PROPERTY_INBAND_UPDATE_POLICY)
        {
            // Property 1: In-band Update Policy + Lifespan
            nsm_req.argument_data[0] = updatePolicy;
            nsm_req.argument_data[1] = lifespan;
        }
        else if (property == NSM_ROT_PROPERTY_AP_SKU_ID)
        {
            // Property 2: AP SKU ID
            nsm_req.argument_length = AP_SKU_ID_DATA_LENGTH;
            // Copy AP SKU ID to argument_data
            memcpy(&nsm_req.argument_data[0], &apSkuId, sizeof(uint32_t));
            nsm_req.argument_data[4] = lifespan;
        }
        else if (property == NSM_ROT_PROPERTY_GLOBAL_FAILOVER_POLICY)
        {
            // Property 3: Global Failover Policy (no lifespan)
            nsm_req.argument_length =
                NSM_ROT_GLOBAL_FAILOVER_POLICY_ARGUMENT_LENGTH;
            nsm_req.argument_data[0] = globalFailoverPolicy;
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
    uint32_t apSkuId{0};
    uint8_t globalFailoverPolicy{DEFAULT_VALUE};

    static constexpr uint8_t ARGUMENT_DATA_LENGTH = 2;
    static constexpr uint8_t AP_SKU_ID_DATA_LENGTH = 5;
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

        ccOptionGroup
            ->add_option(
                "--cak_key_auth_scheme", cakKeyAuthScheme,
                "Valid values are 0 and 1, 0-DOT_LOCK allowed, 1 not allowed")
            ->required()
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option("--cak_ecdsa_key", cakKeyEcdsaKeyFile,
                         "File containing 96 Bytes of ECDSA data (raw bytes)")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, ECDSA_KEY_SIZE, "CAK key");
        });

        app->add_option(
               "--cak_lms_key", cakLmsKeyFile,
               "File containing 48 Bytes (raw bytes) - required only for hybrid auth scheme (1)")
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, LMS_KEY_SIZE, "CAK LMS");
        });

        ccOptionGroup
            ->add_option("--lak_key_auth_scheme", lakKeyAuthScheme,
                         "LAK key authentication scheme (0 or 1)")
            ->required()
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option("--lak_ecdsa_key", lakKeyEcdsaKeyFile,
                         "File containing 96 Bytes LAK key (raw bytes)")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, ECDSA_KEY_SIZE, "LAK key");
        });

        app->add_option(
               "--lak_lms_key", lakLmsKeyFile,
               "File containing 48 Bytes LAK LMS (raw bytes) - required only for hybrid auth scheme (1)")
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, LMS_KEY_SIZE, "LAK LMS");
        });

        ccOptionGroup
            ->add_option("--lock_disable", lockDisable,
                         "Contains state for lock allowing (default: 0)")
            ->default_val(0)
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option("--vendor_min_svn", vendorMinSvn,
                         "Vendor Minimum Firmware Security Version (0-255)")
            ->default_val(0)
            ->check(CLI::Range(0U, 255U));

        ccOptionGroup
            ->add_option("--owner_min_svn", ownerMinSvn,
                         "Owner Minimum Firmware Security Version (0-255)")
            ->default_val(0)
            ->check(CLI::Range(0U, 255U));
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> cakKey = readFileAsBytes(cakKeyEcdsaKeyFile);
        if (cakKey.empty())
        {
            std::cerr << "Error: Failed to read CAK key file: "
                      << cakKeyEcdsaKeyFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> cakLms;
        if (cakKeyAuthScheme == KEY_AUTH_SCHEME_HYBRID)
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
        else
        {
            cakLms.resize(LMS_KEY_SIZE, 0);
        }

        std::vector<uint8_t> lakKey = readFileAsBytes(lakKeyEcdsaKeyFile);
        if (lakKey.empty())
        {
            std::cerr << "Error: Failed to read LAK key file: "
                      << lakKeyEcdsaKeyFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> lakLms;
        if (lakKeyAuthScheme == KEY_AUTH_SCHEME_HYBRID)
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

        memcpy(nsm_req.cak_pub, cakCryptoPcp.data(), CRYPTO_PCP_SIZE);
        memcpy(nsm_req.lak_pub, lakCryptoPcp.data(), CRYPTO_PCP_SIZE);

        nsm_req.lock_disable = lockDisable;
        uint32_t minSvnBitmap = ownerMinSvn |
                                (static_cast<uint32_t>(vendorMinSvn) << 8);
        nsm_req.min_svn = htole32(minSvnBitmap);

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_cak_install_req(instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
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

        struct nsm_common_resp* resp =
            (struct nsm_common_resp*)responsePtr->payload;

        if (resp->command != NSM_FW_DOT_CAK_INSTALL)
        {
            std::cerr << "Warning: Unexpected command code in response: 0x"
                      << std::hex << (int)resp->command << ", expected: 0x"
                      << (int)NSM_FW_DOT_CAK_INSTALL << std::dec << "\n";
        }

        ordered_json result;

        std::stringstream cmdCode, compCode, res;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)resp->command;
        compCode << std::hex << std::setw(2) << std::setfill('0')
                 << (int)resp->completion_code;
        res << std::hex << std::setw(4) << std::setfill('0')
            << (int)resp->reserved;

        result["CommandCode"] = cmdCode.str();
        result["CompletionCode"] = compCode.str();
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
    uint8_t vendorMinSvn;
    uint8_t ownerMinSvn;
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
    {}

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

        ordered_json result;

        if (cc != NSM_SUCCESS)
        {
            std::stringstream cmdCode;
            cmdCode << std::hex << std::setw(2) << std::setfill('0')
                    << (int)NSM_FW_DOT_CAK_BYPASS;
            result["CommandCode"] = cmdCode.str();

            std::stringstream compCode;
            compCode << std::hex << std::setw(2) << std::setfill('0')
                     << (int)cc;
            result["CompletionCode"] = compCode.str();

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

            result["CommandCode"] = cmdCode.str();
            result["CompletionCode"] = compCode.str();
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
            case NSM_IMAGE_COPY_QUERY_PROGRESS:
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
            case NSM_IMAGE_COPY_INITIATE_IMAGE_COPY:
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

class DotLock : public CommandInterface
{
  public:
    ~DotLock() = default;
    DotLock() = delete;
    DotLock(const DotLock&) = delete;
    DotLock(DotLock&&) = default;
    DotLock& operator=(const DotLock&) = delete;
    DotLock& operator=(DotLock&&) = default;

    explicit DotLock(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group("Required",
                                                   "Parameters for DotLock");

        ccOptionGroup
            ->add_option("--cak_key_auth_scheme", cakKeyAuthScheme,
                         "CAK key authentication scheme (0=ECDSA, 1=Hybrid)")
            ->required()
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option("--cak_ecdsa_key", cakEcdsaKeyFile,
                         "File containing 96 bytes of CAK ECDSA key")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, ECDSA_KEY_SIZE, "CAK ECDSA key");
        });

        app->add_option(
               "--cak_lms_key", cakLmsKeyFile,
               "File containing 48 bytes of CAK LMS key (required for hybrid)")
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, LMS_KEY_SIZE, "CAK LMS key");
        });

        ccOptionGroup
            ->add_option("--lak_key_auth_scheme", lakKeyAuthScheme,
                         "LAK key authentication scheme (0=ECDSA, 1=Hybrid)")
            ->required()
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option("--lak_ecdsa_key", lakEcdsaKeyFile,
                         "File containing 96 bytes of LAK ECDSA key")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, ECDSA_KEY_SIZE, "LAK ECDSA key");
        });

        app->add_option(
               "--lak_lms_key", lakLmsKeyFile,
               "File containing 48 bytes of LAK LMS key (required for hybrid)")
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, LMS_KEY_SIZE, "LAK LMS key");
        });

        ccOptionGroup
            ->add_option(
                "--unlock_method", unlockMethod,
                "Unlock method for future DOT_UNLOCK (0=DeviceUID, 1=RandomNonce, 2=StaticValue)")
            ->required()
            ->check(CLI::Range(0, 2));

        app->add_option(
               "--static_challenge", staticChallengeFile,
               "File containing 32 bytes static challenge (required when unlock_method == 2)")
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, STATIC_CHALLENGE_SIZE,
                                    "Static challenge");
        });

        ccOptionGroup
            ->add_option("--lock_signature_auth_scheme",
                         lockSignatureAuthScheme,
                         "Signature authentication scheme (0=ECDSA, 1=Hybrid)")
            ->required()
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option(
                "--signature", signatureFile,
                "File containing 1840 bytes of signature (96 bytes ECDSA + 1740 bytes LMS + 4 bytes RFU)")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, SIGNATURE_SIZE, "Signature");
        });

        app->add_option("--output", outputFile,
                        "Output file for DOT blob (1024 bytes)");
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> cakEcdsa = readFileAsBytes(cakEcdsaKeyFile);
        if (cakEcdsa.empty() || cakEcdsa.size() != ECDSA_KEY_SIZE)
        {
            std::cerr << "Error: Failed to read CAK ECDSA key file: "
                      << cakEcdsaKeyFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> cakLms;
        if (cakKeyAuthScheme == KEY_AUTH_SCHEME_HYBRID)
        {
            if (cakLmsKeyFile.empty())
            {
                std::cerr
                    << "Error: CAK LMS key file required for hybrid auth scheme\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
            cakLms = readFileAsBytes(cakLmsKeyFile);
            if (cakLms.empty() || cakLms.size() != LMS_KEY_SIZE)
            {
                std::cerr << "Error: Failed to read CAK LMS key file: "
                          << cakLmsKeyFile << "\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
        }
        else
        {
            cakLms.resize(LMS_KEY_SIZE, 0);
        }

        std::vector<uint8_t> lakEcdsa = readFileAsBytes(lakEcdsaKeyFile);
        if (lakEcdsa.empty() || lakEcdsa.size() != ECDSA_KEY_SIZE)
        {
            std::cerr << "Error: Failed to read LAK ECDSA key file: "
                      << lakEcdsaKeyFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> lakLms;
        if (lakKeyAuthScheme == KEY_AUTH_SCHEME_HYBRID)
        {
            if (lakLmsKeyFile.empty())
            {
                std::cerr
                    << "Error: LAK LMS key file required for hybrid auth scheme\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
            lakLms = readFileAsBytes(lakLmsKeyFile);
            if (lakLms.empty() || lakLms.size() != LMS_KEY_SIZE)
            {
                std::cerr << "Error: Failed to read LAK LMS key file: "
                          << lakLmsKeyFile << "\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
        }
        else
        {
            lakLms.resize(LMS_KEY_SIZE, 0);
        }

        std::vector<uint8_t> cakPub(CRYPTO_PCP_SIZE, 0);
        if (!nsm::dot::buildKeyAuthData(cakKeyAuthScheme, cakEcdsa.data(),
                                        cakLms.data(), cakPub.data()))
        {
            std::cerr << "Error: Failed to build CAK key authentication data\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> lakPub(CRYPTO_PCP_SIZE, 0);
        if (!nsm::dot::buildKeyAuthData(lakKeyAuthScheme, lakEcdsa.data(),
                                        lakLms.data(), lakPub.data()))
        {
            std::cerr << "Error: Failed to build LAK key authentication data\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> sChallenge(STATIC_CHALLENGE_SIZE, 0);
        if (unlockMethod == 2)
        {
            if (staticChallengeFile.empty())
            {
                std::cerr
                    << "Error: Static challenge file required when unlock_method == 2\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
            sChallenge = readFileAsBytes(staticChallengeFile);
            if (sChallenge.empty() ||
                sChallenge.size() != STATIC_CHALLENGE_SIZE)
            {
                std::cerr << "Error: Failed to read static challenge file: "
                          << staticChallengeFile << "\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
        }

        std::vector<uint8_t> signature = readFileAsBytes(signatureFile);
        if (signature.empty() || signature.size() != SIGNATURE_SIZE)
        {
            std::cerr << "Error: Failed to read signature file (expected "
                      << SIGNATURE_SIZE << " bytes): " << signatureFile
                      << std::endl;
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_dot_lock_req_command));
        nsm_dot_lock_req nsm_req;

        memcpy(nsm_req.cak_pub, cakPub.data(), KEY_AUTH_DATA_SIZE);
        memcpy(nsm_req.lak_pub, lakPub.data(), KEY_AUTH_DATA_SIZE);
        nsm_req.unlock_method = htole32(unlockMethod);
        memcpy(nsm_req.s_challenge, sChallenge.data(), STATIC_CHALLENGE_SIZE);
        memcpy(nsm_req.signature, signature.data(), SIGNATURE_SIZE);

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_lock_req(instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (responsePtr == nullptr)
        {
            std::cerr << "Error: Response pointer is null\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);

        auto rc = decode_nsm_dot_lock_resp(responsePtr, payloadLength, &cc,
                                           &reason_code, dotBlob.data());

        ordered_json result;

        std::stringstream cmdCode, compCode;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)NSM_FW_DOT_LOCK;
        compCode << std::hex << std::setw(2) << std::setfill('0') << (int)cc;

        result["CommandCode"] = cmdCode.str();
        result["CompletionCode"] = compCode.str();

        if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
        {
            std::stringstream reasonCodeHex;
            reasonCodeHex << std::hex << std::setw(4) << std::setfill('0')
                          << (int)reason_code;
            result["Reserved"] = "0000";
            result["reasonCode"] = reasonCodeHex.str();

            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
        }
        else
        {
            result["Reserved"] = "0000";

            result["DOTBlobSize"] = DOT_BLOB_SIZE;

            if (!outputFile.empty())
            {
                std::ofstream outFile(outputFile, std::ios::binary);
                if (outFile)
                {
                    outFile.write(reinterpret_cast<const char*>(dotBlob.data()),
                                  DOT_BLOB_SIZE);
                    outFile.close();
                    result["dotBlobFile"] = outputFile;
                }
                else
                {
                    std::cerr << "Error: Failed to write DOT blob to file: "
                              << outputFile << "\n";
                }
            }
        }

        DisplayInJson(result);
    }

  private:
    uint32_t cakKeyAuthScheme;
    std::string cakEcdsaKeyFile;
    std::string cakLmsKeyFile;
    uint32_t lakKeyAuthScheme;
    std::string lakEcdsaKeyFile;
    std::string lakLmsKeyFile;
    uint32_t unlockMethod;
    std::string staticChallengeFile;
    uint32_t lockSignatureAuthScheme;
    std::string signatureFile;
    std::string outputFile;
};

class DotCAKRotate : public CommandInterface
{
  public:
    ~DotCAKRotate() = default;
    DotCAKRotate() = delete;
    DotCAKRotate(const DotCAKRotate&) = delete;
    DotCAKRotate(DotCAKRotate&&) = default;
    DotCAKRotate& operator=(const DotCAKRotate&) = delete;
    DotCAKRotate& operator=(DotCAKRotate&&) = default;

    explicit DotCAKRotate(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup =
            app->add_option_group("Required", "Parameters for DotCAKRotate");

        ccOptionGroup
            ->add_option(
                "--new_cak_key_auth_scheme", newCakKeyAuthScheme,
                "New CAK key authentication scheme (0=ECDSA, 1=Hybrid)")
            ->required()
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option("--new_cak_ecdsa_key", newCakEcdsaKeyFile,
                         "File containing 96 bytes of new CAK ECDSA key")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, ECDSA_KEY_SIZE,
                                    "New CAK ECDSA key");
        });

        app->add_option(
               "--new_cak_lms_key", newCakLmsKeyFile,
               "File containing 48 bytes of new CAK LMS key (required for hybrid)")
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, LMS_KEY_SIZE, "New CAK LMS key");
        });

        ccOptionGroup
            ->add_option(
                "--lak_signature_auth_scheme", lakSignatureAuthScheme,
                "LAK signature authentication scheme (0=ECDSA, 1=Hybrid)")
            ->required()
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option(
                "--signature", signatureFile,
                "File containing 1840 bytes of LAK signature (96 bytes ECDSA + 1740 bytes LMS + 4 bytes RFU)")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, SIGNATURE_SIZE, "LAK signature");
        });

        app->add_option("--output", outputFile,
                        "Output file for new DOT blob (1024 bytes)");
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> newCakEcdsa = readFileAsBytes(newCakEcdsaKeyFile);
        if (newCakEcdsa.empty() || newCakEcdsa.size() != ECDSA_KEY_SIZE)
        {
            std::cerr << "Error: Failed to read new CAK ECDSA key file: "
                      << newCakEcdsaKeyFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> newCakLms;
        if (newCakKeyAuthScheme == KEY_AUTH_SCHEME_HYBRID)
        {
            if (newCakLmsKeyFile.empty())
            {
                std::cerr
                    << "Error: New CAK LMS key file required for hybrid auth scheme\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
            newCakLms = readFileAsBytes(newCakLmsKeyFile);
            if (newCakLms.empty() || newCakLms.size() != LMS_KEY_SIZE)
            {
                std::cerr << "Error: Failed to read new CAK LMS key file: "
                          << newCakLmsKeyFile << "\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
        }
        else
        {
            newCakLms.resize(LMS_KEY_SIZE, 0);
        }

        std::vector<uint8_t> newCakKey(KEY_AUTH_DATA_SIZE, 0);
        if (!nsm::dot::buildKeyAuthData(newCakKeyAuthScheme, newCakEcdsa.data(),
                                        newCakLms.data(), newCakKey.data()))
        {
            std::cerr
                << "Error: Failed to build new CAK key authentication data\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> signature = readFileAsBytes(signatureFile);
        if (signature.empty() || signature.size() != SIGNATURE_SIZE)
        {
            std::cerr << "Error: Failed to read signature file (expected "
                      << SIGNATURE_SIZE << " bytes): " << signatureFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_dot_cak_rotate_req_command));
        nsm_dot_cak_rotate_req nsm_req;

        memcpy(nsm_req.new_cak, newCakKey.data(), KEY_AUTH_DATA_SIZE);
        memcpy(nsm_req.signature, signature.data(), SIGNATURE_SIZE);

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_cak_rotate_req(instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (responsePtr == nullptr)
        {
            std::cerr << "Error: Response pointer is null\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);

        auto rc = decode_nsm_dot_cak_rotate_resp(
            responsePtr, payloadLength, &cc, &reason_code, dotBlob.data());

        ordered_json result;

        std::stringstream cmdCode, compCode;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)NSM_FW_DOT_CAK_ROTATE;
        compCode << std::hex << std::setw(2) << std::setfill('0') << (int)cc;

        result["CommandCode"] = cmdCode.str();
        result["CompletionCode"] = compCode.str();

        if (rc != NSM_SW_SUCCESS)
        {
            std::cerr << "Error: Failed to decode response, rc=" << (int)rc
                      << "\n";
            std::cout << result.dump(4) << "\n";
            return;
        }

        if (cc == NSM_SUCCESS)
        {
            result["Reserved"] = "0000";
            result["dataSize"] = "0400";

            if (!outputFile.empty())
            {
                std::ofstream outFile(outputFile, std::ios::binary);
                if (outFile.is_open())
                {
                    outFile.write(reinterpret_cast<char*>(dotBlob.data()),
                                  dotBlob.size());
                    outFile.close();
                    result["dotBlobFile"] = outputFile;
                    result["DOTBlobSize"] = dotBlob.size();
                }
                else
                {
                    std::cerr << "Error: Failed to write DOT blob to file: "
                              << outputFile << "\n";
                }
            }
            else
            {
                result["DOTBlobSize"] = dotBlob.size();
            }
        }
        else
        {
            result["Reserved"] = "0000";
            result["dataSize"] = "0000";
            std::stringstream reasonCodeHex;
            reasonCodeHex << std::hex << std::setw(4) << std::setfill('0')
                          << (int)reason_code;
            result["reasonCode"] = reasonCodeHex.str();
        }

        std::cout << result.dump(4) << "\n";
    }

  private:
    uint32_t newCakKeyAuthScheme;
    std::string newCakEcdsaKeyFile;
    std::string newCakLmsKeyFile;
    uint32_t lakSignatureAuthScheme;
    std::string signatureFile;
    std::string outputFile;
};

class DotUnlockChallenge : public CommandInterface
{
  public:
    ~DotUnlockChallenge() = default;
    DotUnlockChallenge() = delete;
    DotUnlockChallenge(const DotUnlockChallenge&) = delete;
    DotUnlockChallenge(DotUnlockChallenge&&) = default;
    DotUnlockChallenge& operator=(const DotUnlockChallenge&) = delete;
    DotUnlockChallenge& operator=(DotUnlockChallenge&&) = default;

    explicit DotUnlockChallenge(const char* type, const char* name,
                                CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required", "Parameters for DotUnlockChallenge");

        ccOptionGroup
            ->add_option("--unlock_type", unlockType,
                         "Unlock type: 1=Owner_Unlock, 2=Vendor_Unlock")
            ->required()
            ->check(CLI::Range(1, 2));

        app->add_option("--output", outputFile,
                        "File to save challenge (32 bytes)")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_req_command));

        nsm_dot_unlock_challenge_req nsm_req;
        nsm_req.unlock_type = unlockType;

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_unlock_challenge_req(instanceId, &nsm_req,
                                                      request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (responsePtr == nullptr)
        {
            std::cerr << "Error: Response pointer is null\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        std::vector<uint8_t> challenge(STATIC_CHALLENGE_SIZE);

        auto rc = decode_nsm_dot_unlock_challenge_resp(
            responsePtr, payloadLength, &cc, &reason_code, challenge.data());

        ordered_json result;

        std::stringstream cmdCode, compCode;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)NSM_FW_DOT_UNLOCK_CHALLENGE;
        compCode << std::hex << std::setw(2) << std::setfill('0') << (int)cc;

        result["CommandCode"] = cmdCode.str();
        result["CompletionCode"] = compCode.str();

        if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
        {
            std::stringstream reasonCodeHex;
            reasonCodeHex << std::hex << std::setw(4) << std::setfill('0')
                          << (int)reason_code;
            result["Reserved"] = "0000";
            result["dataSize"] = "0000";
            result["reasonCode"] = reasonCodeHex.str();

            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
        }
        else
        {
            result["Reserved"] = "0000";

            std::ofstream outFile(outputFile, std::ios::binary);
            if (outFile)
            {
                outFile.write(reinterpret_cast<const char*>(challenge.data()),
                              STATIC_CHALLENGE_SIZE);
                outFile.close();
                result["challengeFile"] = outputFile;
                result["challengeSize"] = STATIC_CHALLENGE_SIZE;
            }
            else
            {
                std::cerr << "Error: Failed to write challenge to file: "
                          << outputFile << "\n";
            }
        }

        DisplayInJson(result);
    }

  private:
    uint32_t unlockType;
    std::string outputFile;
};

class DotUnlock : public CommandInterface
{
  public:
    ~DotUnlock() = default;
    DotUnlock() = delete;
    DotUnlock(const DotUnlock&) = delete;
    DotUnlock(DotUnlock&&) = default;
    DotUnlock& operator=(const DotUnlock&) = delete;
    DotUnlock& operator=(DotUnlock&&) = default;

    explicit DotUnlock(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option(
               "--signature", signatureFile,
               "File containing 1840 bytes of LAK signature "
               "(ECDSA: 96B + 1744B padding; Hybrid: 96B ECDSA + 1740B LMS + 4B RFU)")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, SIGNATURE_SIZE, "Signature");
        });
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> signature = readFileAsBytes(signatureFile);
        if (signature.empty() || signature.size() != SIGNATURE_SIZE)
        {
            std::cerr << "Error: Failed to read signature file: "
                      << signatureFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_dot_unlock_req_command));

        nsm_dot_unlock_req nsm_req;
        memcpy(nsm_req.signature, signature.data(), SIGNATURE_SIZE);

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_unlock_req(instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (responsePtr == nullptr)
        {
            std::cerr << "Error: Response pointer is null\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_nsm_dot_unlock_resp(responsePtr, payloadLength, &cc,
                                             &reason_code);

        ordered_json result;

        std::stringstream cmdCode, compCode;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)NSM_FW_DOT_UNLOCK;
        compCode << std::hex << std::setw(2) << std::setfill('0') << (int)cc;

        result["CommandCode"] = cmdCode.str();
        result["CompletionCode"] = compCode.str();
        result["Reserved"] = "0000";

        if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
        {
            std::stringstream reasonCodeHex;
            reasonCodeHex << std::hex << std::setw(4) << std::setfill('0')
                          << (int)reason_code;
            result["DataSize"] = "0000";
            result["ReasonCode"] = reasonCodeHex.str();

            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
        }
        else
        {
            result["DataSize"] = "0000";
        }

        DisplayInJson(result);
    }

  private:
    std::string signatureFile;
};

class DotGetInfo : public CommandInterface
{
  public:
    ~DotGetInfo() = default;
    DotGetInfo() = delete;
    DotGetInfo(const DotGetInfo&) = delete;
    DotGetInfo(DotGetInfo&&) = default;
    DotGetInfo& operator=(const DotGetInfo&) = delete;
    DotGetInfo& operator=(DotGetInfo&&) = default;

    explicit DotGetInfo(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->get_option("--mctp_eid")->required();

        app->add_option("--output", outputFile,
                        "Output file for DOT blob (1024 bytes)");
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_dot_get_info_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_get_info_req(instanceId, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (responsePtr == nullptr)
        {
            std::cerr << "Error: Response pointer is null\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t version = 0;
        uint8_t fuse_change_state = 0;
        uint8_t transfers_remaining = 0;
        std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);

        auto rc = decode_nsm_dot_get_info_resp(
            responsePtr, payloadLength, &cc, &reason_code, &version,
            &fuse_change_state, &transfers_remaining, dotBlob.data());

        ordered_json result;

        std::stringstream cmdCode, compCode;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)NSM_FW_DOT_GET_INFO;
        compCode << std::hex << std::setw(2) << std::setfill('0') << (int)cc;

        result["CommandCode"] = cmdCode.str();
        result["CompletionCode"] = compCode.str();

        if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
        {
            std::stringstream reasonCodeHex;
            reasonCodeHex << std::hex << std::setw(4) << std::setfill('0')
                          << (int)reason_code;
            result["Reserved"] = "0000";
            result["dataSize"] = "0000";
            result["reasonCode"] = reasonCodeHex.str();

            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
        }
        else
        {
            result["Reserved"] = "0000";

            std::stringstream versionHex;
            versionHex << std::hex << std::setw(4) << std::setfill('0')
                       << (int)version;
            result["version"] = versionHex.str();

            std::stringstream fuseStateHex;
            fuseStateHex << std::hex << std::setw(2) << std::setfill('0')
                         << (int)fuse_change_state;
            result["fuseChangeState"] = fuseStateHex.str();

            result["transfersRemaining"] = (int)transfers_remaining;

            // Save DOT blob to file if output file is specified
            if (!outputFile.empty())
            {
                std::ofstream outFile(outputFile, std::ios::binary);
                if (outFile)
                {
                    outFile.write(reinterpret_cast<const char*>(dotBlob.data()),
                                  DOT_BLOB_SIZE);
                    outFile.close();
                    result["dotBlobFile"] = outputFile;
                    result["DOTBlobSize"] = DOT_BLOB_SIZE;
                }
                else
                {
                    std::cerr << "Warning: Failed to write DOT blob to file: "
                              << outputFile << "\n";
                }
            }
            else
            {
                result["DOTBlobSize"] = DOT_BLOB_SIZE;
                result["note"] =
                    "DOT blob not saved (no output file specified)";
            }
        }

        DisplayInJson(result);
    }

  private:
    std::string outputFile;
};

class DotGetStatus : public CommandInterface
{
  public:
    ~DotGetStatus() = default;
    DotGetStatus() = delete;
    DotGetStatus(const DotGetStatus&) = delete;
    DotGetStatus(DotGetStatus&&) = default;
    DotGetStatus& operator=(const DotGetStatus&) = delete;
    DotGetStatus& operator=(DotGetStatus&&) = default;

    explicit DotGetStatus(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {}

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_dot_get_status_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_get_status_req(instanceId, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (responsePtr == nullptr)
        {
            std::cerr << "Error: Response pointer is null\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint8_t status = 0;

        auto rc = decode_nsm_dot_get_status_resp(responsePtr, payloadLength,
                                                 &cc, &reason_code, &status);

        ordered_json result;

        std::stringstream cmdCode, compCode;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)NSM_FW_DOT_GET_STATUS;
        compCode << std::hex << std::setw(2) << std::setfill('0') << (int)cc;

        result["CommandCode"] = cmdCode.str();
        result["CompletionCode"] = compCode.str();

        if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
        {
            std::stringstream reasonCodeHex;
            reasonCodeHex << std::hex << std::setw(4) << std::setfill('0')
                          << (int)reason_code;
            result["Reserved"] = "0000";
            result["dataSize"] = "0000";
            result["reasonCode"] = reasonCodeHex.str();
            result["statusText"] = "Unknown";

            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
        }
        else
        {
            result["Reserved"] = "0000";

            std::stringstream statusHex;
            statusHex << std::hex << std::setw(2) << std::setfill('0')
                      << (int)status;
            result["status"] = statusHex.str();

            std::string statusText;
            switch (status & 0x03)
            {
                case 0:
                    statusText = "Uninitialized";
                    break;
                case 1:
                    statusText = "Volatile";
                    break;
                case 2:
                    statusText = "Mutable Locked";
                    break;
                case 3:
                    statusText = "Mutable Disabled";
                    break;
                default:
                    statusText = "Unknown";
                    break;
            }
            result["statusText"] = statusText;
        }

        DisplayInJson(result);
    }
};

class DotDisable : public CommandInterface
{
  public:
    ~DotDisable() = default;
    DotDisable() = delete;
    DotDisable(const DotDisable&) = delete;
    DotDisable(DotDisable&&) = default;
    DotDisable& operator=(const DotDisable&) = delete;
    DotDisable& operator=(DotDisable&&) = default;

    explicit DotDisable(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group("Required",
                                                   "Parameters for DotDisable");

        ccOptionGroup
            ->add_option("--lak_key_auth_scheme", lakKeyAuthScheme,
                         "LAK key authentication scheme (0=ECDSA, 1=Hybrid)")
            ->required()
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option("--lak_ecdsa_key", lakEcdsaKeyFile,
                         "File containing 96 bytes of LAK ECDSA key")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, ECDSA_KEY_SIZE, "LAK ECDSA key");
        });

        app->add_option(
               "--lak_lms_key", lakLmsKeyFile,
               "File containing 48 bytes of LAK LMS key (required for hybrid)")
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, LMS_KEY_SIZE, "LAK LMS key");
        });

        ccOptionGroup
            ->add_option(
                "--unlock_method", unlockMethod,
                "Unlock method for future DOT_UNLOCK (0=DeviceUID, 1=RandomNonce, 2=StaticValue)")
            ->required()
            ->check(CLI::Range(0, 2));

        app->add_option(
               "--static_challenge", staticChallengeFile,
               "File containing 32 bytes static challenge (required when unlock_method == 2)")
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, STATIC_CHALLENGE_SIZE,
                                    "Static challenge");
        });

        ccOptionGroup
            ->add_option("--disable_signature_auth_scheme",
                         disableSignatureAuthScheme,
                         "Signature authentication scheme (0=ECDSA, 1=Hybrid)")
            ->required()
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option(
                "--signature", signatureFile,
                "File containing 1840 bytes of signature (96 bytes ECDSA + 1740 bytes LMS + 4 bytes RFU)")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, SIGNATURE_SIZE, "Signature");
        });

        app->add_option("--output", outputFile,
                        "Output file for DOT blob (1024 bytes)");
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> lakEcdsa = readFileAsBytes(lakEcdsaKeyFile);
        if (lakEcdsa.empty() || lakEcdsa.size() != ECDSA_KEY_SIZE)
        {
            std::cerr << "Error: Failed to read LAK ECDSA key file: "
                      << lakEcdsaKeyFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> lakLms;
        if (lakKeyAuthScheme == KEY_AUTH_SCHEME_HYBRID)
        {
            if (lakLmsKeyFile.empty())
            {
                std::cerr
                    << "Error: LAK LMS key file required for hybrid auth scheme\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
            lakLms = readFileAsBytes(lakLmsKeyFile);
            if (lakLms.empty() || lakLms.size() != LMS_KEY_SIZE)
            {
                std::cerr << "Error: Failed to read LAK LMS key file: "
                          << lakLmsKeyFile << "\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
        }
        else
        {
            lakLms.resize(LMS_KEY_SIZE, 0);
        }

        std::vector<uint8_t> lakPub(CRYPTO_PCP_SIZE, 0);
        if (!nsm::dot::buildKeyAuthData(lakKeyAuthScheme, lakEcdsa.data(),
                                        lakLms.data(), lakPub.data()))
        {
            std::cerr << "Error: Failed to build LAK key authentication data\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> sChallenge(STATIC_CHALLENGE_SIZE, 0);
        if (unlockMethod == 2)
        {
            if (staticChallengeFile.empty())
            {
                std::cerr
                    << "Error: Static challenge file required when unlock_method == 2\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
            sChallenge = readFileAsBytes(staticChallengeFile);
            if (sChallenge.empty() ||
                sChallenge.size() != STATIC_CHALLENGE_SIZE)
            {
                std::cerr << "Error: Failed to read static challenge file: "
                          << staticChallengeFile << "\n";
                return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
            }
        }

        std::vector<uint8_t> signature = readFileAsBytes(signatureFile);
        if (signature.empty() || signature.size() != SIGNATURE_SIZE)
        {
            std::cerr << "Error: Failed to read signature file (expected "
                      << SIGNATURE_SIZE << " bytes): " << signatureFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_dot_disable_req_command));

        nsm_dot_disable_req nsm_req;
        memcpy(nsm_req.lak_pub, lakPub.data(), DOT_KEY_AUTH_DATA_SIZE);
        nsm_req.unlock_method = htole32(unlockMethod);
        memcpy(nsm_req.s_challenge, sChallenge.data(), STATIC_CHALLENGE_SIZE);
        memcpy(nsm_req.signature, signature.data(), SIGNATURE_SIZE);

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_disable_req(instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (responsePtr == nullptr)
        {
            std::cerr << "Error: Response pointer is null\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);

        auto rc = decode_nsm_dot_disable_resp(responsePtr, payloadLength, &cc,
                                              &reason_code, dotBlob.data());

        ordered_json result;

        std::stringstream cmdCode, compCode;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)NSM_FW_DOT_DISABLE;
        compCode << std::hex << std::setw(2) << std::setfill('0') << (int)cc;

        result["CommandCode"] = cmdCode.str();
        result["CompletionCode"] = compCode.str();

        if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
        {
            std::stringstream reasonCodeHex;
            reasonCodeHex << std::hex << std::setw(4) << std::setfill('0')
                          << (int)reason_code;
            result["Reserved"] = "0000";
            result["ReasonCode"] = reasonCodeHex.str();

            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
        }
        else
        {
            result["Reserved"] = "0000";

            std::stringstream dataSizeHex;
            dataSizeHex << std::hex << std::setw(4) << std::setfill('0')
                        << DOT_BLOB_SIZE;
            result["DataSize"] = dataSizeHex.str();
            result["DOTBlobSize"] = DOT_BLOB_SIZE;

            if (!outputFile.empty())
            {
                std::ofstream outFile(outputFile, std::ios::binary);
                if (outFile)
                {
                    outFile.write(reinterpret_cast<const char*>(dotBlob.data()),
                                  DOT_BLOB_SIZE);
                    outFile.close();
                    result["DOTBlobFile"] = outputFile;
                }
                else
                {
                    std::cerr << "Error: Failed to write DOT blob to file: "
                              << outputFile << "\n";
                }
            }
        }

        DisplayInJson(result);
    }

  private:
    uint32_t lakKeyAuthScheme;
    std::string lakEcdsaKeyFile;
    std::string lakLmsKeyFile;
    uint32_t unlockMethod;
    std::string staticChallengeFile;
    uint32_t disableSignatureAuthScheme;
    std::string signatureFile;
    std::string outputFile;
};

class DotOverride : public CommandInterface
{
  public:
    ~DotOverride() = default;
    DotOverride() = delete;
    DotOverride(const DotOverride&) = delete;
    DotOverride(DotOverride&&) = default;
    DotOverride& operator=(const DotOverride&) = delete;
    DotOverride& operator=(DotOverride&&) = default;

    explicit DotOverride(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup =
            app->add_option_group("Required", "Parameters for DotOverride");

        ccOptionGroup
            ->add_option(
                "--vendor_signature_auth_scheme", vendorSignatureAuthScheme,
                "Vendor signature authentication scheme (0=ECDSA, 1=Hybrid)")
            ->required()
            ->check(CLI::Range(0, 1));

        ccOptionGroup
            ->add_option(
                "--signature", signatureFile,
                "File containing 1840 bytes of vendor signature "
                "(ECDSA: 96B + 1744B padding; Hybrid: 96B ECDSA + 1740B LMS + 4B RFU)")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, SIGNATURE_SIZE, "Signature");
        });
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> signature = readFileAsBytes(signatureFile);
        if (signature.empty() || signature.size() != SIGNATURE_SIZE)
        {
            std::cerr << "Error: Failed to read signature file: "
                      << signatureFile << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_dot_override_req_command));

        nsm_dot_override_req nsm_req;
        memcpy(nsm_req.signature, signature.data(), SIGNATURE_SIZE);

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_override_req(instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (responsePtr == nullptr)
        {
            std::cerr << "Error: Response pointer is null\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_nsm_dot_override_resp(responsePtr, payloadLength, &cc,
                                               &reason_code);

        ordered_json result;

        std::stringstream cmdCode, compCode;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)NSM_FW_DOT_OVERRIDE;
        compCode << std::hex << std::setw(2) << std::setfill('0') << (int)cc;

        result["CommandCode"] = cmdCode.str();
        result["CompletionCode"] = compCode.str();
        result["Reserved"] = "0000";

        if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
        {
            std::stringstream reasonCodeHex;
            reasonCodeHex << std::hex << std::setw(4) << std::setfill('0')
                          << (int)reason_code;
            result["DataSize"] = "0000";
            result["ReasonCode"] = reasonCodeHex.str();

            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
        }
        else
        {
            result["DataSize"] = "0000";
        }

        DisplayInJson(result);
    }

  private:
    uint32_t vendorSignatureAuthScheme;
    std::string signatureFile;
};

class DotRecovery : public CommandInterface
{
  public:
    ~DotRecovery() = default;
    DotRecovery() = delete;
    DotRecovery(const DotRecovery&) = delete;
    DotRecovery(DotRecovery&&) = default;
    DotRecovery& operator=(const DotRecovery&) = delete;
    DotRecovery& operator=(DotRecovery&&) = default;

    explicit DotRecovery(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("--dot_blob", dotBlobFile,
                        "File containing 1024 bytes of DOT backup blob")
            ->required()
            ->check(CLI::ExistingFile)
            ->check([this](const std::string& filename) -> std::string {
            return validateFileSize(filename, DOT_BLOB_SIZE, "DOT blob");
        });
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> dotBlob = readFileAsBytes(dotBlobFile);
        if (dotBlob.empty() || dotBlob.size() != DOT_BLOB_SIZE)
        {
            std::cerr << "Error: Failed to read DOT blob file: " << dotBlobFile
                      << "\n";
            return std::make_pair(NSM_SW_ERROR, std::vector<uint8_t>());
        }

        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_dot_recovery_req_command));

        nsm_dot_recovery_req nsm_req;
        memcpy(nsm_req.dot_blob, dotBlob.data(), DOT_BLOB_SIZE);

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_dot_recovery_req(instanceId, &nsm_req, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (responsePtr == nullptr)
        {
            std::cerr << "Error: Response pointer is null\n";
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_nsm_dot_recovery_resp(responsePtr, payloadLength, &cc,
                                               &reason_code);

        ordered_json result;

        std::stringstream cmdCode, compCode;
        cmdCode << std::hex << std::setw(2) << std::setfill('0')
                << (int)NSM_FW_DOT_RECOVERY;
        compCode << std::hex << std::setw(2) << std::setfill('0') << (int)cc;

        result["CommandCode"] = cmdCode.str();
        result["CompletionCode"] = compCode.str();
        result["Reserved"] = "0000";

        if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
        {
            std::stringstream reasonCodeHex;
            reasonCodeHex << std::hex << std::setw(4) << std::setfill('0')
                          << (int)reason_code;
            result["DataSize"] = "0000";
            result["ReasonCode"] = reasonCodeHex.str();

            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
        }
        else
        {
            result["DataSize"] = "0000";
        }

        DisplayInJson(result);
    }

  private:
    std::string dotBlobFile;
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
    auto dotLock = firmware->add_subcommand(
        "DotLock", "Lock DOT and transition from volatile to locked state");
    commands.push_back(
        std::make_unique<DotLock>("firmware", "DotLock", dotLock));

    auto dotCAKRotate = firmware->add_subcommand(
        "DotCAKRotate", "Rotate CAK without changing DOT FUSE state");
    commands.push_back(std::make_unique<DotCAKRotate>(
        "firmware", "DotCAKRotate", dotCAKRotate));

    auto dotUnlockChallenge = firmware->add_subcommand(
        "DotUnlockChallenge", "Get challenge for DOT unlock operation");
    commands.push_back(std::make_unique<DotUnlockChallenge>(
        "firmware", "DotUnlockChallenge", dotUnlockChallenge));

    auto dotUnlock = firmware->add_subcommand(
        "DotUnlock", "Unlock DOT and transition from locked to volatile state");
    commands.push_back(
        std::make_unique<DotUnlock>("firmware", "DotUnlock", dotUnlock));
    auto dotGetInfo = firmware->add_subcommand(
        "DotGetInfo",
        "Get DOT information including version, fuse state, and DOT blob");
    commands.push_back(
        std::make_unique<DotGetInfo>("firmware", "DotGetInfo", dotGetInfo));
    auto dotGetStatus = firmware->add_subcommand(
        "DotGetStatus",
        "Get DOT status (Uninitialized/Volatile/Locked/Disabled/Unknown)");
    commands.push_back(std::make_unique<DotGetStatus>(
        "firmware", "DotGetStatus", dotGetStatus));
    auto dotDisable = firmware->add_subcommand(
        "DotDisable",
        "Disable DOT to transition from uninitialized state to disabled state");
    commands.push_back(
        std::make_unique<DotDisable>("firmware", "DotDisable", dotDisable));
    auto dotOverride = firmware->add_subcommand(
        "DotOverride",
        "Override DOT to reset ownership state when valid DOT data can no longer be recovered");
    commands.push_back(
        std::make_unique<DotOverride>("firmware", "DotOverride", dotOverride));
    auto dotRecovery = firmware->add_subcommand(
        "DotRecovery", "Recover corrupted DOT data using backup data");
    commands.push_back(
        std::make_unique<DotRecovery>("firmware", "DotRecovery", dotRecovery));
}
} // namespace nsmtool::firmware
