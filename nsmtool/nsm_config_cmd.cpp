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

class SetErrorInjectionModeV1 : public CommandInterface
{
  public:
    ~SetErrorInjectionModeV1() = default;
    SetErrorInjectionModeV1() = delete;
    SetErrorInjectionModeV1(const SetErrorInjectionModeV1&) = delete;
    SetErrorInjectionModeV1(SetErrorInjectionModeV1&&) = default;
    SetErrorInjectionModeV1& operator=(const SetErrorInjectionModeV1&) = delete;
    SetErrorInjectionModeV1& operator=(SetErrorInjectionModeV1&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetErrorInjectionModeV1(const char* type, const char* name,
                                     CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto modeGroup = app->add_option_group(
            "Required", "Global error injection mode knob");

        mode = 0;
        modeGroup->add_option("-M, --mode", mode, "Disable - 0 / Enable - 1");
        modeGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_set_error_injection_mode_v1_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_error_injection_mode_v1_req(instanceId, mode,
                                                         request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_set_error_injection_mode_v1_resp(
            responsePtr, payloadLength, &cc, &reason_code);
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
    uint8_t mode;
};

class GetErrorInjectionModeV1 : public CommandInterface
{
  public:
    ~GetErrorInjectionModeV1() = default;
    GetErrorInjectionModeV1() = delete;
    GetErrorInjectionModeV1(const GetErrorInjectionModeV1&) = delete;
    GetErrorInjectionModeV1(GetErrorInjectionModeV1&&) = default;
    GetErrorInjectionModeV1& operator=(const GetErrorInjectionModeV1&) = delete;
    GetErrorInjectionModeV1& operator=(GetErrorInjectionModeV1&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetErrorInjectionModeV1(const char* type, const char* name,
                                     CLI::App* app) :
        CommandInterface(type, name, app)
    {}

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_error_injection_mode_v1_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        nsm_error_injection_mode_v1 data;

        auto rc = decode_get_error_injection_mode_v1_resp(
            responsePtr, payloadLength, &cc, &reason_code, &data);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_error_injection_mode_v1_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Mode"] = bool(data.mode);
        result["Persistent"] = bool(data.flags.bits.bit0);
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetSupportedErrorInjectionTypesV1 : public CommandInterface
{
  public:
    ~GetSupportedErrorInjectionTypesV1() = default;
    GetSupportedErrorInjectionTypesV1() = delete;
    GetSupportedErrorInjectionTypesV1(
        const GetSupportedErrorInjectionTypesV1&) = delete;
    GetSupportedErrorInjectionTypesV1(GetSupportedErrorInjectionTypesV1&&) =
        default;
    GetSupportedErrorInjectionTypesV1&
        operator=(const GetSupportedErrorInjectionTypesV1&) = delete;
    GetSupportedErrorInjectionTypesV1&
        operator=(GetSupportedErrorInjectionTypesV1&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetSupportedErrorInjectionTypesV1(const char* type,
                                               const char* name,
                                               CLI::App* app) :
        CommandInterface(type, name, app)
    {}

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_supported_error_injection_types_v1_req(instanceId,
                                                                    request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        nsm_error_injection_types_mask data;

        auto rc = decode_get_error_injection_types_v1_resp(
            responsePtr, payloadLength, &cc, &reason_code, &data);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_error_injection_types_mask_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["DRAM and SRAM errors injection supported"] =
            bool((data.mask[0] >> EI_MEMORY_ERRORS) & 0x01);
        result["PCI link error injection supported"] =
            bool((data.mask[0] >> EI_PCI_ERRORS) & 0x01);
        result["Link error injection supported"] =
            bool((data.mask[0] >> EI_NVLINK_ERRORS) & 0x01);
        result["Device thermal error injection supported"] =
            bool((data.mask[0] >> EI_THERMAL_ERRORS) & 0x01);
        nsmtool::helper::DisplayInJson(result);
    }
};

class SetCurrentErrorInjectionTypesV1 : public CommandInterface
{
  public:
    ~SetCurrentErrorInjectionTypesV1() = default;
    SetCurrentErrorInjectionTypesV1() = delete;
    SetCurrentErrorInjectionTypesV1(const SetCurrentErrorInjectionTypesV1&) =
        delete;
    SetCurrentErrorInjectionTypesV1(SetCurrentErrorInjectionTypesV1&&) =
        default;
    SetCurrentErrorInjectionTypesV1&
        operator=(const SetCurrentErrorInjectionTypesV1&) = delete;
    SetCurrentErrorInjectionTypesV1&
        operator=(SetCurrentErrorInjectionTypesV1&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetCurrentErrorInjectionTypesV1(const char* type, const char* name,
                                             CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-d,--data", rawData, "raw data")
            ->required()
            ->expected(-3);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_set_error_injection_types_mask_req));
        nsm_error_injection_types_mask data;
        for (size_t i = 0; i < 8 && i < rawData.size(); i++)
        {
            data.mask[i] = rawData[i];
        }

        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_current_error_injection_types_v1_req(
            instanceId, &data, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_set_current_error_injection_types_v1_resp(
            responsePtr, payloadLength, &cc, &reason_code);
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
    std::vector<uint8_t> rawData;
};

class GetCurrentErrorInjectionTypesV1 : public CommandInterface
{
  public:
    ~GetCurrentErrorInjectionTypesV1() = default;
    GetCurrentErrorInjectionTypesV1() = delete;
    GetCurrentErrorInjectionTypesV1(const GetCurrentErrorInjectionTypesV1&) =
        delete;
    GetCurrentErrorInjectionTypesV1(GetCurrentErrorInjectionTypesV1&&) =
        default;
    GetCurrentErrorInjectionTypesV1&
        operator=(const GetCurrentErrorInjectionTypesV1&) = delete;
    GetCurrentErrorInjectionTypesV1&
        operator=(GetCurrentErrorInjectionTypesV1&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetCurrentErrorInjectionTypesV1(const char* type, const char* name,
                                             CLI::App* app) :
        CommandInterface(type, name, app)
    {}

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_current_error_injection_types_v1_req(instanceId,
                                                                  request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        nsm_error_injection_types_mask data;

        auto rc = decode_get_error_injection_types_v1_resp(
            responsePtr, payloadLength, &cc, &reason_code, &data);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_error_injection_types_mask_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["DRAM and SRAM errors injection enabled"] =
            bool((data.mask[0] >> EI_MEMORY_ERRORS) & 0x01);
        result["PCI link error injection enabled"] =
            bool((data.mask[0] >> EI_PCI_ERRORS) & 0x01);
        result["Link error injection enabled"] =
            bool((data.mask[0] >> EI_NVLINK_ERRORS) & 0x01);
        result["Device thermal error injection enabled"] =
            bool((data.mask[0] >> EI_THERMAL_ERRORS) & 0x01);
        nsmtool::helper::DisplayInJson(result);
    }
};

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
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
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
                result["HMC SPI Flash"] = (int)data.hmc;
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
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
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
                    std::cerr << "Response message error: "
                              << "rc=" << rc << ", cc=" << (int)cc
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
                    std::cerr << "Response message error: "
                              << "rc=" << rc << ", cc=" << (int)cc
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
                    std::cerr << "Response message error: "
                              << "rc=" << rc << ", cc=" << (int)cc
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
                    std::cerr << "Response message error: "
                              << "rc=" << rc << ", cc=" << (int)cc
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

const std::map<reconfiguration_permissions_v1_index, std::string>
    settingsDictionary = {{
        {RP_IN_SYSTEM_TEST, "In system test"},
        {RP_FUSING_MODE, "Fusing Mode"},
        {RP_CONFIDENTIAL_COMPUTE, "Confidential compute"},
        {RP_BAR0_FIREWALL, "BAR0 Firewall"},
        {RP_CONFIDENTIAL_COMPUTE_DEV_MODE, "Confidential compute dev-mode"},
        {RP_TOTAL_GPU_POWER_CURRENT_LIMIT,
         "Total GPU Power (TGP) current limit"},
        {RP_TOTAL_GPU_POWER_RATED_LIMIT, "Total GPU Power (TGP) rated limit"},
        {RP_TOTAL_GPU_POWER_MAX_LIMIT, "Total GPU Power (TGP) max limit"},
        {RP_TOTAL_GPU_POWER_MIN_LIMIT, "Total GPU Power (TGP) min limit"},
        {RP_CLOCK_LIMIT, "Clock limit"},
        {RP_NVLINK_DISABLE, "NVLink disable"},
        {RP_ECC_ENABLE, "ECC enable"},
        {RP_PCIE_VF_CONFIGURATION, "PCIe VF configuration"},
        {RP_ROW_REMAPPING_ALLOWED, "Row remapping allowed"},
        {RP_ROW_REMAPPING_FEATURE, "Row remapping feature"},
        {RP_HBM_FREQUENCY_CHANGE, "HBM frequency change"},
        {RP_HULK_LICENSE_UPDATE, "HULK license update"},
        {RP_FORCE_TEST_COUPLING, "Force test coupling"},
        {RP_BAR0_TYPE_CONFIG, "BAR0 type config"},
        {RP_EDPP_SCALING_FACTOR, "EDPp scaling factor"},
        {RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_1,
         "Power Smoothing Privilege Level 1"},
        {RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2,
         "Power Smoothing Privilege Level 2"},
        {RP_EGM_MODE, "Extend GPU Memory Mode"},
    }};
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
        CommandInterface(type, name, app),
        settingIndex((reconfiguration_permissions_v1_index)-1)
    {
        auto getReconfigurationPermissionsV1Group = app->add_option_group(
            "Required",
            "Setting Index for which data source is to be retrieved.");
        std::string list;
        for (auto [id, setting] : settingsDictionary)
        {
            list += std::to_string((int)id) + " - " + setting + "\n";
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_reconfiguration_permissions_v1_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["PRC Knob"] = settingsDictionary.at(settingIndex);
        result["Oneshot (Host)"] = bool(data.host_oneshot);
        result["Persistent (Host)"] = bool(data.host_persistent);
        result["FLR_Persistent (Host)"] = bool(data.host_flr_persistent);
        result["Oneshot (DOE)"] = bool(data.DOE_oneshot);
        result["Persistent (DOE)"] = bool(data.DOE_persistent);
        result["FLR_Persistent (DOE)"] = bool(data.DOE_flr_persistent);
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    reconfiguration_permissions_v1_index settingIndex;
};

class SetReconfigurationPermissionsV1 : public CommandInterface
{
  public:
    ~SetReconfigurationPermissionsV1() = default;
    SetReconfigurationPermissionsV1() = delete;
    SetReconfigurationPermissionsV1(const SetReconfigurationPermissionsV1&) =
        delete;
    SetReconfigurationPermissionsV1(SetReconfigurationPermissionsV1&&) =
        default;
    SetReconfigurationPermissionsV1&
        operator=(const SetReconfigurationPermissionsV1&) = delete;
    SetReconfigurationPermissionsV1&
        operator=(SetReconfigurationPermissionsV1&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetReconfigurationPermissionsV1(const char* type, const char* name,
                                             CLI::App* app) :
        CommandInterface(type, name, app),
        settingIndex((reconfiguration_permissions_v1_index)-1),
        configuration((reconfiguration_permissions_v1_setting)-1), permission()
    {
        std::string settingsList;
        for (auto [id, setting] : settingsDictionary)
        {
            settingsList += std::to_string((int)id) + " - " + setting + "\n";
        }
        std::string configsList;
        const std::map<reconfiguration_permissions_v1_setting, std::string>
            configurationsDictionary = {
                {RP_ONESHOOT_HOT_RESET, "Oneshot (hot reset)"},
                {RP_PERSISTENT, "Persistent"},
                {RP_ONESHOT_FLR, "Oneshot (FLR)"},
            };
        for (auto [id, config] : configurationsDictionary)
        {
            configsList += std::to_string((int)id) + " - " + config + "\n";
        }

        auto setReconfigurationPermissionsV1Group = app->add_option_group(
            "Required",
            "Setting Index, Configuration and Permission for which data source is to be retrieved.");
        setReconfigurationPermissionsV1Group->add_option(
            "-s, --settingId", settingIndex,
            "retrieve data source for settingIndex\n" + settingsList);
        setReconfigurationPermissionsV1Group->add_option(
            "-c, --configuration", configuration,
            "retrieve data source for configuration\n" + configsList);
        setReconfigurationPermissionsV1Group->add_option(
            "-V, --value", permission,
            "retrieve data source for permission value - \n0 - DISALLOW_HOST_DISALLOW_DOE \n1 - ALLOW_HOST_DISALLOW_DOE \n2 - DISALLOW_HOST_ALLOW_DOE \n3 - ALLOW_HOST_ALLOW_DOE\n");
        setReconfigurationPermissionsV1Group->require_option(3);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_set_reconfiguration_permissions_v1_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_reconfiguration_permissions_v1_req(
            instanceId, settingIndex, configuration, permission, request);
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

        auto rc = decode_set_reconfiguration_permissions_v1_resp(
            responsePtr, payloadLength, &cc, &reason_code);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_reconfiguration_permissions_v1_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    reconfiguration_permissions_v1_index settingIndex;
    reconfiguration_permissions_v1_setting configuration;
    uint8_t permission;
};

class GetConfidentialComputeModeV1 : public CommandInterface
{
  public:
    ~GetConfidentialComputeModeV1() = default;
    GetConfidentialComputeModeV1() = delete;
    GetConfidentialComputeModeV1(const GetConfidentialComputeModeV1&) = delete;
    GetConfidentialComputeModeV1(GetConfidentialComputeModeV1&&) = default;
    GetConfidentialComputeModeV1&
        operator=(const GetConfidentialComputeModeV1&) = delete;
    GetConfidentialComputeModeV1&
        operator=(GetConfidentialComputeModeV1&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_confidential_compute_mode_v1_req(instanceId,
                                                              request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size;
        uint8_t current_mode;
        uint8_t pending_mode;
        auto rc = decode_get_confidential_compute_mode_v1_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code,
            &current_mode, &pending_mode);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_confidential_compute_mode_v1_resp));
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Current Mode"] = current_mode;
        result["Pending Mode"] = pending_mode;
        nsmtool::helper::DisplayInJson(result);
    }
};

class SetConfidentialComputeModeV1 : public CommandInterface
{
  public:
    ~SetConfidentialComputeModeV1() = default;
    SetConfidentialComputeModeV1() = delete;
    SetConfidentialComputeModeV1(const SetConfidentialComputeModeV1&) = delete;
    SetConfidentialComputeModeV1(SetConfidentialComputeModeV1&&) = default;
    SetConfidentialComputeModeV1&
        operator=(const SetConfidentialComputeModeV1&) = delete;
    SetConfidentialComputeModeV1&
        operator=(SetConfidentialComputeModeV1&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetConfidentialComputeModeV1(const char* type, const char* name,
                                          CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto setConfidentialComputeModeV1 =
            app->add_option_group("Required", "Set Confidential Compute Mode");
        setConfidentialComputeModeV1->add_option("-r, --mode", mode, "mode");
        setConfidentialComputeModeV1->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_set_confidential_compute_mode_v1_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_confidential_compute_mode_v1_req(instanceId, mode,
                                                              request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size;
        auto rc = decode_set_confidential_compute_mode_v1_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code);
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
        result["Completion Code"] = cc; // check in nsm spec before merging
                                        // code and do required change

        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t mode;
};

void registerCommand(CLI::App& app)
{
    auto config = app.add_subcommand("config",
                                     "Device configuration type command");
    config->require_subcommand(1);

    auto setErrorInjectionModeV1 = config->add_subcommand(
        "SetErrorInjectionModeV1", "Set Error Injection Mode v1");
    commands.push_back(std::make_unique<SetErrorInjectionModeV1>(
        "config", "SetErrorInjectionModeV1", setErrorInjectionModeV1));

    auto getErrorInjectionModeV1 = config->add_subcommand(
        "GetErrorInjectionModeV1", "Get Error Injection Mode v1");
    commands.push_back(std::make_unique<GetErrorInjectionModeV1>(
        "config", "GetErrorInjectionModeV1", getErrorInjectionModeV1));

    auto getSupportedErrorInjectionTypesV1 =
        config->add_subcommand("GetSupportedErrorInjectionTypesV1",
                               "Get Supported Error Injection Types v1");
    commands.push_back(std::make_unique<GetSupportedErrorInjectionTypesV1>(
        "config", "GetSupportedErrorInjectionTypesV1",
        getSupportedErrorInjectionTypesV1));

    auto setCurrentErrorInjectionTypesV1 =
        config->add_subcommand("SetCurrentErrorInjectionTypesV1",
                               "Set Current Error Injection Types v1");
    commands.push_back(std::make_unique<SetCurrentErrorInjectionTypesV1>(
        "config", "SetCurrentErrorInjectionTypesV1",
        setCurrentErrorInjectionTypesV1));

    auto getCurrentErrorInjectionTypesV1 =
        config->add_subcommand("GetCurrentErrorInjectionTypesV1",
                               "Get Current Error Injection Types v1");
    commands.push_back(std::make_unique<GetCurrentErrorInjectionTypesV1>(
        "config", "GetCurrentErrorInjectionTypesV1",
        getCurrentErrorInjectionTypesV1));

    auto getFpgaDiagnosticsSettings =
        config->add_subcommand("GetFpgaDiagnosticsSettings",
                               "Get FPGA Diagnostics Settings for data index ");
    commands.push_back(std::make_unique<GetFpgaDiagnosticsSettings>(
        "config", "GetFpgaDiagnosticsSettings", getFpgaDiagnosticsSettings));

    auto enableDisableGpuIstMode = config->add_subcommand(
        "EnableDisableGpuIstMode",
        "Enable/Disable GPUs IST Mode Settings for device index ");
    commands.push_back(std::make_unique<EnableDisableGpuIstMode>(
        "config", "EnableDisableGpuIstMode", enableDisableGpuIstMode));

    auto getReconfigurationPermissionsV1 =
        config->add_subcommand("GetReconfigurationPermissionsV1",
                               "Get Reconfiguration Permissions v1");
    commands.push_back(std::make_unique<GetReconfigurationPermissionsV1>(
        "config", "GetReconfigurationPermissionsV1",
        getReconfigurationPermissionsV1));

    auto setReconfigurationPermissionsV1 =
        config->add_subcommand("SetReconfigurationPermissionsV1",
                               "Set Reconfiguration Permissions v1");
    commands.push_back(std::make_unique<SetReconfigurationPermissionsV1>(
        "config", "SetReconfigurationPermissionsV1",
        setReconfigurationPermissionsV1));

    auto getConfidentialComputeModeV1 = config->add_subcommand(
        "GetConfidentialComputeModeV1", "Get Confidential Compute Mode Data");
    commands.push_back(std::make_unique<GetConfidentialComputeModeV1>(
        "config", "GetConfidentialComputeModeV1",
        getConfidentialComputeModeV1));

    auto setConfidentialComputeModeV1 = config->add_subcommand(
        "SetConfidentialComputeModeV1", "Set Confidential Compute Mode Data");
    commands.push_back(std::make_unique<SetConfidentialComputeModeV1>(
        "config", "SetConfidentialComputeModeV1",
        setConfidentialComputeModeV1));
}

} // namespace config

} // namespace nsmtool
