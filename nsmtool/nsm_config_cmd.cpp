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

// NSM: Nvidia Message type
//           - Network Ports            [Type 1]
//           - PCI links                [Type 2]
//           - Platform environments    [Type 3]
//           - Diagnostics              [Type 4]
//           - Device configuration     [Type 5]

#include "nsm_config_cmd.hpp"

#include "base.h"
#include "device-configuration.h"

#include "cmd_helper.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>

#include <ctime>

namespace nsmtool
{

namespace config
{

namespace
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

} // namespace

class EnableDisableGpuIstMode : public CommandInterface
{
  public:
    ~EnableDisableGpuIstMode() = default;
    EnableDisableGpuIstMode() = delete;
    EnableDisableGpuIstMode(const EnableDisableGpuIstMode&) = delete;
    EnableDisableGpuIstMode(EnableDisableGpuIstMode&&) = default;
    EnableDisableGpuIstMode& operator=(const EnableDisableGpuIstMode&) = delete;
    EnableDisableGpuIstMode& operator=(EnableDisableGpuIstMode&&) = default;

    using CommandInterface::CommandInterface;

    explicit EnableDisableGpuIstMode(const char* type, const char* name,
                                     CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto istModeGroup = app->add_option_group(
            "Required",
            "Device Index and Value for which GPU IST Mode will be set.");

        deviceIndex = 0;
        istModeGroup->add_option(
            "-d, --deviceIndex", deviceIndex,
            "Device GPU IST Mode: 0-7: select GPU, 10 all GPUs");
        value = 0;
        istModeGroup->add_option("-V, --value", value,
                                 "Disable - 0 / Enable - 1");
        istModeGroup->require_option(2);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_gpu_ist_mode_req));
        int rc = NSM_SW_ERROR;
        if (deviceIndex < 8 || deviceIndex == ALL_GPUS_DEVICE_INDEX)
        {
            auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
            rc = encode_enable_disable_gpu_ist_mode_req(instanceId, deviceIndex,
                                                        value, request);
        }
        else
        {
            std::cerr << "Invalid Device Index \n";
        }
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_enable_disable_gpu_ist_mode_resp(
            responsePtr, payloadLength, &cc, &reason_code);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
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
    uint8_t deviceIndex;
    uint8_t value;
};
class GetFpgaDiagnosticsSettings : public CommandInterface
{
  public:
    ~GetFpgaDiagnosticsSettings() = default;
    GetFpgaDiagnosticsSettings() = delete;
    GetFpgaDiagnosticsSettings(const GetFpgaDiagnosticsSettings&) = delete;
    GetFpgaDiagnosticsSettings(GetFpgaDiagnosticsSettings&&) = default;
    GetFpgaDiagnosticsSettings&
        operator=(const GetFpgaDiagnosticsSettings&) = delete;
    GetFpgaDiagnosticsSettings&
        operator=(GetFpgaDiagnosticsSettings&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetFpgaDiagnosticsSettings(const char* type, const char* name,
                                        CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto fpgaDiagnosticsSettingsGroup = app->add_option_group(
            "Required", "Data Index for which data source is to be retrieved.");

        dataId = 0;
        fpgaDiagnosticsSettingsGroup->add_option(
            "-d, --dataId", dataId,
            "retrieve data source for dataId\n"
            "  0 – Get WP Settings\n"
            "  1 – Get PCIe Fundamental Reset State\n"
            "  2 – Get WP Jumper Presence\n"
            "  3 – Get GPU Degrade Mode Settings\n"
            "  4 – Get GPU IST Mode Settings\n"
            "  5 – Get Power Supply Status\n"
            "  6 – Get Board Power Supply Status\n"
            "  7 – Get Power Brake State\n"
            "  8 – Get Thermal Alert State\n"
            "  9 – Get NVSW Flash Present Settings\n"
            " 10 – Get NVSW Fuse SRC Settings\n"
            " 11 – Get Retimer LTSSM Dump Mode Settings\n"
            " 12 – Get GPU Presence\n"
            " 13 – Get GPU Power Status\n"
            "255 – Get Aggregate Telemetry\n");
        fpgaDiagnosticsSettingsGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_fpga_diagnostics_settings_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_fpga_diagnostics_settings_req(
            instanceId, (fpga_diagnostics_settings_data_index)dataId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        switch ((fpga_diagnostics_settings_data_index)dataId)
        {
            case GET_WP_SETTINGS:
            {
                nsm_fpga_diagnostics_settings_wp data;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_get_fpga_diagnostics_settings_wp_resp(
                    responsePtr, payloadLength, &cc, &reason_code, &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: " << "rc=" << rc
                        << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(nsm_msg_hdr) +
                            sizeof(nsm_get_fpga_diagnostics_settings_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["GPUs 1-4 SPI Flash"] = (int)data.gpu1_4;
                result["Any NVSW EROT"] = (int)data.nvSwitch;
                result["PEXSW EROT"] = (int)data.pex;
                result["FRU EEPROM (Baseboard or CX7 or HMC)"] =
                    (int)data.baseboard;
                result["Any Retimer"] = (int)data.retimer;
                result["GPU 5-8 SPI Flash"] = (int)data.gpu5_8;
                result["Retimer 1"] = (int)data.retimer1;
                result["Retimer 2"] = (int)data.retimer2;
                result["Retimer 3"] = (int)data.retimer3;
                result["Retimer 4"] = (int)data.retimer4;
                result["Retimer 5"] = (int)data.retimer5;
                result["Retimer 6"] = (int)data.retimer6;
                result["Retimer 7"] = (int)data.retimer7;
                result["Retimer 8"] = (int)data.retimer8;
                result["GPU 1"] = (int)data.gpu1;
                result["GPU 2"] = (int)data.gpu2;
                result["GPU 3"] = (int)data.gpu3;
                result["GPU 4"] = (int)data.gpu4;
                result["NVSW 1"] = (int)data.nvSwitch1;
                result["NVSW 2"] = (int)data.nvSwitch2;
                result["GPU 5"] = (int)data.gpu5;
                result["GPU 6"] = (int)data.gpu6;
                result["GPU 7"] = (int)data.gpu7;
                result["GPU 8"] = (int)data.gpu8;

                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case GET_WP_JUMPER_PRESENCE:
            {
                nsm_fpga_diagnostics_settings_wp_jumper data;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
                    responsePtr, payloadLength, &cc, &reason_code, &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: " << "rc=" << rc
                        << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(nsm_msg_hdr) +
                            sizeof(nsm_get_fpga_diagnostics_settings_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["WP Presence"] = (int)data.presence;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case GET_POWER_SUPPLY_STATUS:
            {
                uint8_t data;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_get_power_supply_status_resp(
                    responsePtr, payloadLength, &cc, &reason_code, &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr << "Response message error: " << "rc=" << rc
                              << ", cc=" << (int)cc
                              << ", reasonCode=" << (int)reason_code << "\n"
                              << payloadLength << "...."
                              << (sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_power_supply_status_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["Power supply status"] = (int)data;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case GET_GPU_PRESENCE:
            {
                uint8_t data;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_get_gpu_presence_resp(
                    responsePtr, payloadLength, &cc, &reason_code, &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr << "Response message error: " << "rc=" << rc
                              << ", cc=" << (int)cc
                              << ", reasonCode=" << (int)reason_code << "\n"
                              << payloadLength << "...."
                              << (sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpu_presence_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["GPUs presence"] = (int)data;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case GET_GPU_POWER_STATUS:
            {
                uint8_t data;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_get_gpu_power_status_resp(
                    responsePtr, payloadLength, &cc, &reason_code, &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr << "Response message error: " << "rc=" << rc
                              << ", cc=" << (int)cc
                              << ", reasonCode=" << (int)reason_code << "\n"
                              << payloadLength << "...."
                              << (sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpu_power_status_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["GPUs power status"] = (int)data;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case GET_GPU_IST_MODE_SETTINGS:
            {
                uint8_t data;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_get_gpu_ist_mode_resp(
                    responsePtr, payloadLength, &cc, &reason_code, &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr << "Response message error: " << "rc=" << rc
                              << ", cc=" << (int)cc
                              << ", reasonCode=" << (int)reason_code << "\n"
                              << payloadLength << "...."
                              << (sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpu_ist_mode_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["GPUs IST Mode Settings"] = (int)data;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            default:
            {
                std::cerr << "Invalid Data Id \n";
                break;
            }
        }
    }

  private:
    uint8_t dataId;
};
class GetReconfigurationPermissionsV1 : public CommandInterface
{
  public:
    ~GetReconfigurationPermissionsV1() = default;
    GetReconfigurationPermissionsV1() = delete;
    GetReconfigurationPermissionsV1(const GetReconfigurationPermissionsV1&) =
        delete;
    GetReconfigurationPermissionsV1(GetReconfigurationPermissionsV1&&) =
        default;
    GetReconfigurationPermissionsV1&
        operator=(const GetReconfigurationPermissionsV1&) = delete;
    GetReconfigurationPermissionsV1&
        operator=(GetReconfigurationPermissionsV1&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetReconfigurationPermissionsV1(const char* type, const char* name,
                                             CLI::App* app) :
        CommandInterface(type, name, app), settingIndex(0),
        settingsDictionary({
            {0, "In system test"},
            {1, "Fusing Mode"},
            {2, "Confidential compute"},
            {3, "BAR0 Firewall"},
            {4, "Confidential compute dev-mode"},
            {5, "Total GPU Power (TGP) current limit"},
            {6, "Total GPU Power (TGP) rated limit"},
            {7, "Total GPU Power (TGP) max limit"},
            {8, "Total GPU Power (TGP) min limit"},
            {9, "Clock limit"},
            {10, "NVLink disable"},
            {11, "ECC enable"},
            {12, "PCIe VF configuration"},
            {13, "Row remapping allowed"},
            {14, "Row remapping feature"},
            {15, "HBM frequency change"},
            {16, "HULK license update"},
            {17, "Force test coupling"},
            {18, "BAR0 type config"},
            {19, "EDPp scaling factor"},
            {20, "Power Smoothing Feature Toggle"},
            {21, "Power Smoothing Privilege Level 0"},
            {22, "Power Smoothing Privilege Level 1"},
            {23, "Power Smoothing Privilege Level 2"},
        })
    {
        auto getReconfigurationPermissionsV1Group = app->add_option_group(
            "Required",
            "Setting Index for which data source is to be retrieved.");
        std::string list;
        for (auto [id, setting] : settingsDictionary)
        {
            list += std::to_string(id) + " - " + setting + "\n";
        }
        getReconfigurationPermissionsV1Group->add_option(
            "-s, --settingId", settingIndex,
            "retrieve data source for settingIndex\n" + list);
        getReconfigurationPermissionsV1Group->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_reconfiguration_permissions_v1_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_reconfiguration_permissions_v1_req(
            instanceId, (reconfiguration_permissions_v1_index)settingIndex,
            request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (settingsDictionary.find(settingIndex) == settingsDictionary.end())
        {
            std::cerr << "Invalid Settings Id \n";
            return;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        nsm_reconfiguration_permissions_v1 data;

        auto rc = decode_get_reconfiguration_permissions_v1_resp(
            responsePtr, payloadLength, &cc, &reason_code, &data);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_reconfiguration_permissions_v1_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["PRC Knob"] = settingsDictionary[settingIndex];
        result["Oneshot (hot reset)"] = bool(data.oneshot);
        result["Persistent"] = bool(data.persistent);
        result["Oneshot (FLR)"] = bool(data.flr_persistent);
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t settingIndex;
    std::map<uint8_t, std::string> settingsDictionary;
};

void registerCommand(CLI::App& app)
{
    auto config = app.add_subcommand("config",
                                     "Device configuration type command");
    config->require_subcommand(1);

    auto getFpgaDiagnosticsSettings = config->add_subcommand(
        "GetFpgaDiagnosticsSettings",
        "retrieve FPGA Diagnostics Settings for data index ");
    commands.push_back(std::make_unique<GetFpgaDiagnosticsSettings>(
        "config", "GetFpgaDiagnosticsSettings", getFpgaDiagnosticsSettings));

    auto enableDisableGpuIstMode = config->add_subcommand(
        "EnableDisableGpuIstMode",
        "Enable/Disable GPUs IST Mode Settings for device index ");
    commands.push_back(std::make_unique<EnableDisableGpuIstMode>(
        "config", "EnableDisableGpuIstMode", enableDisableGpuIstMode));

    auto getReconfigurationPermissionsV1 = config->add_subcommand(
        "GetReconfigurationPermissionsV1",
        "retrieve Get Reconfiguration Permissions v1 for setting index ");
    commands.push_back(std::make_unique<GetReconfigurationPermissionsV1>(
        "config", "GetReconfigurationPermissionsV1",
        getReconfigurationPermissionsV1));
}

} // namespace config

} // namespace nsmtool
