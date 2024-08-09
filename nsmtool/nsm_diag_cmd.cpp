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

// NSM: Nvidia Message type
//           - Diagnostics              [Type 4]

#include "nsm_diag_cmd.hpp"

#include "base.h"
#include "debug-token.h"
#include "diagnostics.h"

#include "cmd_helper.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>

#include <ctime>

namespace nsmtool
{

namespace diag
{

namespace
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

} // namespace

class QueryTokenParameters : public CommandInterface
{
  public:
    ~QueryTokenParameters() = default;
    QueryTokenParameters() = delete;
    QueryTokenParameters(const QueryTokenParameters&) = delete;
    QueryTokenParameters(QueryTokenParameters&&) = default;
    QueryTokenParameters& operator=(const QueryTokenParameters&) = delete;
    QueryTokenParameters& operator=(QueryTokenParameters&&) = default;

    using CommandInterface::CommandInterface;

    explicit QueryTokenParameters(const char* type, const char* name,
                                  CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required",
            "Query token parameters for the specified token opcode");
        ccOptionGroup->add_option(
            "-o,--opcode", tokenOpcode,
            "query token parameters for the specified token opcode");
        ccOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_query_token_parameters_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_query_token_parameters_req(
            instanceId, static_cast<nsm_debug_token_opcode>(tokenOpcode),
            request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        struct nsm_debug_token_request token_request;

        auto rc = decode_nsm_query_token_parameters_resp(
            responsePtr, payloadLength, &cc, &reason_code, &token_request);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        nlohmann::ordered_json result;
        result["Completion code"] = cc;
        result["Reason code"] = reason_code;
        result["Token request version"] =
            static_cast<uint32_t>(token_request.token_request_version);
        result["Token request size"] =
            static_cast<uint32_t>(token_request.token_request_size);
        result["Device UUID"] = bytesToHexString(token_request.device_uuid, 8);
        result["Device type"] =
            static_cast<uint32_t>(token_request.device_type);
        result["Device index"] =
            static_cast<uint32_t>(token_request.device_index);
        result["Status"] = token_request.status;
        result["Token opcode"] = token_request.token_opcode;
        result["Keypair UUID"] = bytesToHexString(token_request.keypair_uuid,
                                                  16);
        result["Base MAC"] = bytesToHexString(token_request.base_mac, 8);
        result["PSID"] = bytesToHexString(token_request.psid, 16);
        result["FW version"] = nlohmann::json::array();
        for (size_t i = 0; i < 5; ++i)
        {
            result["FW version"].push_back(token_request.fw_version[i]);
        }
        result["Source address"] =
            bytesToHexString(token_request.source_address, 16);
        result["Session ID"] = static_cast<uint32_t>(token_request.session_id);
        result["Challenge version"] = token_request.challenge_version;
        result["Challenge"] = bytesToHexString(token_request.challenge, 32);

        DisplayInJson(result);
    }

  private:
    uint8_t tokenOpcode;
};

class ProvideToken : public CommandInterface
{
  public:
    ~ProvideToken() = default;
    ProvideToken() = delete;
    ProvideToken(const ProvideToken&) = delete;
    ProvideToken(ProvideToken&&) = default;
    ProvideToken& operator=(const ProvideToken&) = delete;
    ProvideToken& operator=(ProvideToken&&) = default;

    using CommandInterface::CommandInterface;

    explicit ProvideToken(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup =
            app->add_option_group("Required", "Install specified token data");
        ccOptionGroup->add_option(
            "-t,--token", tokenHexstring,
            "hexadecimal string containing token data to be installed");
        ccOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req_v2) +
                                        tokenHexstring.size() / 2);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());

        std::vector<uint8_t> token;
        for (size_t i = 0; i < tokenHexstring.length(); i += 2)
        {
            std::string byteString = tokenHexstring.substr(i, 2);
            uint8_t byte = (uint8_t)strtoul(byteString.c_str(), NULL, 16);
            token.push_back(byte);
        }

        auto rc = encode_nsm_provide_token_req(instanceId, token.data(),
                                               token.size(), request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_nsm_provide_token_resp(responsePtr, payloadLength, &cc,
                                                &reason_code);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        nlohmann::ordered_json result;
        result["Completion code"] = cc;
        result["Reason code"] = reason_code;

        DisplayInJson(result);
    }

  private:
    std::string tokenHexstring;
};

class DisableTokens : public CommandInterface
{
  public:
    ~DisableTokens() = default;
    DisableTokens() = delete;
    DisableTokens(const DisableTokens&) = delete;
    DisableTokens(DisableTokens&&) = default;
    DisableTokens& operator=(const DisableTokens&) = delete;
    DisableTokens& operator=(DisableTokens&&) = default;

    using CommandInterface::CommandInterface;

    explicit DisableTokens(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {}

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_disable_tokens_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());

        auto rc = encode_nsm_disable_tokens_req(instanceId, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_nsm_disable_tokens_resp(responsePtr, payloadLength,
                                                 &cc, &reason_code);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        nlohmann::ordered_json result;
        result["Completion code"] = cc;
        result["Reason code"] = reason_code;

        DisplayInJson(result);
    }
};

class QueryTokenStatus : public CommandInterface
{
  public:
    ~QueryTokenStatus() = default;
    QueryTokenStatus() = delete;
    QueryTokenStatus(const QueryTokenStatus&) = delete;
    QueryTokenStatus(QueryTokenStatus&&) = default;
    QueryTokenStatus& operator=(const QueryTokenStatus&) = delete;
    QueryTokenStatus& operator=(QueryTokenStatus&&) = default;

    using CommandInterface::CommandInterface;

    explicit QueryTokenStatus(const char* type, const char* name,
                              CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required", "Query token status for the specified token type");
        ccOptionGroup->add_option(
            "-t,--type", tokenType,
            "query token status for the specified token type");
        ccOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_query_token_status_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_query_token_status_req(
            instanceId, static_cast<nsm_debug_token_type>(tokenType), request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        nsm_debug_token_status status;
        nsm_debug_token_status_additional_info additional_info;
        nsm_debug_token_type token_type;
        uint32_t time_left;

        auto rc = decode_nsm_query_token_status_resp(
            responsePtr, payloadLength, &cc, &reason_code, &status,
            &additional_info, &token_type, &time_left);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        nlohmann::ordered_json result;
        result["Completion code"] = cc;
        result["Reason code"] = reason_code;
        result["Status"] = status;
        result["Additional info"] = additional_info;
        result["Token type"] = token_type;
        result["Time left"] = time_left;

        DisplayInJson(result);
    }

  private:
    uint8_t tokenType;
};

class QueryDeviceIds : public CommandInterface
{
  public:
    ~QueryDeviceIds() = default;
    QueryDeviceIds() = delete;
    QueryDeviceIds(const QueryDeviceIds&) = delete;
    QueryDeviceIds(QueryDeviceIds&&) = default;
    QueryDeviceIds& operator=(const QueryDeviceIds&) = delete;
    QueryDeviceIds& operator=(QueryDeviceIds&&) = default;

    using CommandInterface::CommandInterface;

    explicit QueryDeviceIds(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {}

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_query_device_ids_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());

        auto rc = encode_nsm_query_device_ids_req(instanceId, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint8_t device_id[NSM_DEBUG_TOKEN_DEVICE_ID_SIZE];

        auto rc = decode_nsm_query_device_ids_resp(
            responsePtr, payloadLength, &cc, &reason_code, device_id);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        nlohmann::ordered_json result;
        result["Completion code"] = cc;
        result["Reason code"] = reason_code;
        result["Device ID"] = bytesToHexString(device_id,
                                               NSM_DEBUG_TOKEN_DEVICE_ID_SIZE);

        DisplayInJson(result);
    }
};

class EnableDisableWriteProtected : public CommandInterface
{
  public:
    ~EnableDisableWriteProtected() = default;
    EnableDisableWriteProtected() = delete;
    EnableDisableWriteProtected(const EnableDisableWriteProtected&) = delete;
    EnableDisableWriteProtected(EnableDisableWriteProtected&&) = default;
    EnableDisableWriteProtected&
        operator=(const EnableDisableWriteProtected&) = delete;
    EnableDisableWriteProtected&
        operator=(EnableDisableWriteProtected&&) = default;

    using CommandInterface::CommandInterface;

    explicit EnableDisableWriteProtected(const char* type, const char* name,
                                         CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto writeProtectedGroup = app->add_option_group(
            "Required",
            "Data Index and Value for which write protected will be set.");

        dataId = 0;
        writeProtectedGroup->add_option(
            "-d, --dataId", dataId,
            "Data Index of write protected:\n"
            "128: Retimer EEPROM\n"
            "129: Baseboard FRU EEPROM\n"
            "130: PEX SW EEPROM\n"
            "131: NVSW EEPROM (both)\n"
            "133: NVSW EEPROM 1\n"
            "134: NVSW EEPROM 2\n"
            "160: GPU 1-4 SPI Flash\n"
            "161: GPU 5-8 SPI Flash\n"
            "162-169: Individual GPU SPI flash 1-8\n"
            "176: HMC SPI Flash\n"
            "192-199: Retimer EEPROM\n"
            "232: CX7 FRU EEPROM\n"
            "233: HMC FRU EEPROM\n");
        value = 0;
        writeProtectedGroup->add_option("-V, --value", value,
                                        "Disable - 0 / Enable - 1");
        writeProtectedGroup->require_option(2);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_enable_disable_wp_req));
        int rc = NSM_SW_ERROR;
        switch ((diagnostics_enable_disable_wp_data_index)dataId)
        {
            case RETIMER_EEPROM:
            case BASEBOARD_FRU_EEPROM:
            case PEX_SW_EEPROM:
            case NVSW_EEPROM_BOTH:
            case NVSW_EEPROM_1:
            case NVSW_EEPROM_2:
            case GPU_1_4_SPI_FLASH:
            case GPU_5_8_SPI_FLASH:
            case GPU_SPI_FLASH_1:
            case GPU_SPI_FLASH_2:
            case GPU_SPI_FLASH_3:
            case GPU_SPI_FLASH_4:
            case GPU_SPI_FLASH_5:
            case GPU_SPI_FLASH_6:
            case GPU_SPI_FLASH_7:
            case GPU_SPI_FLASH_8:
            case HMC_SPI_FLASH:
            case RETIMER_EEPROM_1:
            case RETIMER_EEPROM_2:
            case RETIMER_EEPROM_3:
            case RETIMER_EEPROM_4:
            case RETIMER_EEPROM_5:
            case RETIMER_EEPROM_6:
            case RETIMER_EEPROM_7:
            case RETIMER_EEPROM_8:
            case CX7_FRU_EEPROM:
            case HMC_FRU_EEPROM:
            {
                auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
                rc = encode_enable_disable_wp_req(
                    instanceId,
                    (diagnostics_enable_disable_wp_data_index)dataId, value,
                    request);
            }
            break;
            default:
                std::cerr << "Invalid Data Id \n";
                break;
        }
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_enable_disable_wp_resp(responsePtr, payloadLength, &cc,
                                                &reason_code);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;

        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t dataId;
    uint8_t value;
};

void registerCommand(CLI::App& app)
{
    auto diag = app.add_subcommand("diag", "Diagnostics type command");
    diag->require_subcommand(1);

    auto queryTokenParameters = diag->add_subcommand("QueryTokenParameters",
                                                     "Query token parameters");
    commands.push_back(std::make_unique<QueryTokenParameters>(
        "diag", "QueryTokenParameters", queryTokenParameters));

    auto provideToken = diag->add_subcommand("ProvideToken", "Provide token");
    commands.push_back(
        std::make_unique<ProvideToken>("diag", "ProvideToken", provideToken));

    auto disableTokens = diag->add_subcommand("DisableTokens",
                                              "Disable tokens");
    commands.push_back(std::make_unique<DisableTokens>("diag", "DisableTokens",
                                                       disableTokens));

    auto queryTokenStatus = diag->add_subcommand("QueryTokenStatus",
                                                 "Query token status");
    commands.push_back(std::make_unique<QueryTokenStatus>(
        "diag", "QueryTokenStatus", queryTokenStatus));

    auto queryDeviceIds = diag->add_subcommand("QueryDeviceIds",
                                               "Query device IDs");
    commands.push_back(std::make_unique<QueryDeviceIds>(
        "diag", "QueryDeviceIds", queryDeviceIds));

    auto enableDisableWriteProtected = diag->add_subcommand(
        "EnableDisableWriteProtected", "Enable/Disable WriteProtected");
    commands.push_back(std::make_unique<EnableDisableWriteProtected>(
        "diag", "EnableDisableWriteProtected", enableDisableWriteProtected));
}

} // namespace diag

} // namespace nsmtool
