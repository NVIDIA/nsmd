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
#include "platform-environmental.h"

#include "cmd_helper.hpp"
#include "utils.hpp"

#include <endian.h>

#include <CLI/CLI.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace nsmtool
{

namespace config
{

namespace
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

bool parseHexToBytes(const std::string& hex, std::vector<uint8_t>& out)
{
    out.clear();
    if (hex.empty())
    {
        return true;
    }
    if (hex.size() % 2 != 0)
    {
        return false;
    }
    for (size_t i = 0; i < hex.size(); i += 2)
    {
        char pair[3] = {hex[i], hex[i + 1], '\0'};
        char* end = nullptr;
        unsigned long v = std::strtoul(pair, &end, 16);
        if (end != pair + 2 || v > 255)
        {
            return false;
        }
        out.push_back(static_cast<uint8_t>(v));
    }
    return true;
}

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
        result["Thermal error injection supported"] =
            bool((data.mask[0] >> EI_THERMAL_ERRORS) & 0x01);
        result["Device error injection supported"] =
            bool((data.mask[0] >> EI_DEVICE_ERRORS) & 0x01);
        result["GPIO spoofing error injection supported"] =
            bool((data.mask[0] >> EI_GPIO_SPOOFING) & 0x01);
        nsmtool::helper::DisplayInJson(result);
    }
};

class SetErrorInjectionPayload : public CommandInterface
{
  public:
    ~SetErrorInjectionPayload() = default;
    SetErrorInjectionPayload() = delete;
    SetErrorInjectionPayload(const SetErrorInjectionPayload&) = delete;
    SetErrorInjectionPayload(SetErrorInjectionPayload&&) = default;
    SetErrorInjectionPayload&
        operator=(const SetErrorInjectionPayload&) = delete;
    SetErrorInjectionPayload& operator=(SetErrorInjectionPayload&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetErrorInjectionPayload(const char* type, const char* name,
                                      CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-t,--errorInjectionType", errorInjectionType,
                        "Error Injection Type [Options: \n"
                        "  0x00: Memory Errors\n"
                        "  0x01: PCIe Errors\n"
                        "  0x02: Nvlink Errors\n"
                        "  0x03: Thermal Errors\n"
                        "  0x04: Device Errors\n"
                        "  0x05: GPIO Spoofing]");
        app->add_option("-s,--errorInjectionSubtype", errorInjectionSubtype,
                        "Error Injection Subtype [For Device errors Options: \n"
                        "  0x00: Fatal Error (if not applicable, use 0x00)\n"
                        "  0x01: Port Recovery Error\n"
                        "  0x02: USB Emulation Error\n"
                        "  0x03: Leak Detect Error]");
        app->add_option(
               "-d,--data", rawData,
               "Data for payload after error injection type and subtype")
            ->required()
            ->expected(-3);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        if (errorInjectionType == EI_DEVICE_ERRORS)
        {
            rawData.insert(rawData.begin(), 2, 0);
            *reinterpret_cast<uint16_t*>(rawData.data()) =
                errorInjectionSubtype;
        }

        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_set_error_injection_payload_req) -
            sizeof(uint8_t) + rawData.size());
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_error_injection_payload_req(
            instanceId, rawData.data(), rawData.size(), errorInjectionType,
            errorInjectionSubtype, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_set_error_injection_payload_resp(
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
    uint16_t errorInjectionType{};
    uint16_t errorInjectionSubtype{};
};

class GetErrorInjectionPayload : public CommandInterface
{
  public:
    ~GetErrorInjectionPayload() = default;
    GetErrorInjectionPayload() = delete;
    GetErrorInjectionPayload(const GetErrorInjectionPayload&) = delete;
    GetErrorInjectionPayload(GetErrorInjectionPayload&&) = default;
    GetErrorInjectionPayload&
        operator=(const GetErrorInjectionPayload&) = delete;
    GetErrorInjectionPayload& operator=(GetErrorInjectionPayload&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetErrorInjectionPayload(const char* type, const char* name,
                                      CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-t,--errorInjectionType", errorInjectionType,
                        "Error Injection Type [Options: \n"
                        "  0x00: Memory Errors\n"
                        "  0x01: PCIe Errors\n"
                        "  0x02: Nvlink Errors\n"
                        "  0x03: Thermal Errors\n"
                        "  0x04: Device Errors\n"
                        "  0x05: GPIO Spoofing]");
        app->add_option("-s,--errorInjectionSubtype", errorInjectionSubtype,
                        "Error Injection Subtype [For Device errors Options: \n"
                        "  0x00: Fatal Error (if not applicable, use 0x00)\n"
                        "  0x01: Port Recovery Error\n"
                        "  0x02: USB Emulation Error\n"
                        "  0x03: Leak Detect Error]");
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_error_injection_payload_req(
            instanceId, errorInjectionType, errorInjectionSubtype, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        // Allocate buffer large enough for sensors data
        std::vector<uint8_t> data(payloadLength, 0);
        size_t dataSize = 0;

        auto rc = decode_get_error_injection_payload_resp(
            responsePtr, payloadLength, errorInjectionType,
            errorInjectionSubtype, &cc, &reason_code, data.data(), &dataSize);
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
        // resize sensorsData as per datasize
        data.resize(dataSize);
        result["Completion Code"] = cc;
        result["Error Injection Type"] = errorInjectionType;
        result["Error Injection Subtype"] = errorInjectionSubtype;

        // For device errors (type 4), skip first 2 bytes which represent
        // the error injection subtype already displayed above
        if (errorInjectionType == EI_DEVICE_ERRORS && data.size() >= 2)
        {
            std::vector<uint8_t> faultPayload(data.begin() + 2, data.end());
            result["Fault payload"] = faultPayload;
        }
        else
        {
            result["Fault payload"] = data;
        }
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint16_t errorInjectionType{};
    uint16_t errorInjectionSubtype{};
};

class ActivateErrorInjectionPayload : public CommandInterface
{
  public:
    ~ActivateErrorInjectionPayload() = default;
    ActivateErrorInjectionPayload() = delete;
    ActivateErrorInjectionPayload(const ActivateErrorInjectionPayload&) =
        delete;
    ActivateErrorInjectionPayload(ActivateErrorInjectionPayload&&) = default;
    ActivateErrorInjectionPayload&
        operator=(const ActivateErrorInjectionPayload&) = delete;
    ActivateErrorInjectionPayload&
        operator=(ActivateErrorInjectionPayload&&) = default;

    using CommandInterface::CommandInterface;

    explicit ActivateErrorInjectionPayload(const char* type, const char* name,
                                           CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-t,--errorInjectionType", errorInjectionType,
                        "Error Injection Type [Options: \n"
                        "  0x00: Memory Errors\n"
                        "  0x01: PCIe Errors\n"
                        "  0x02: Nvlink Errors\n"
                        "  0x03: Thermal Errors\n"
                        "  0x04: Device Errors\n"
                        "  0x05: GPIO Spoofing]");
        app->add_option("-s,--errorInjectionSubtype", errorInjectionSubtype,
                        "Error Injection Subtype [For Device errors Options: \n"
                        "  0x00: Fatal Error (if not applicable, use 0x00)\n"
                        "  0x01: Port Recovery Error\n"
                        "  0x02: USB Emulation Error\n"
                        "  0x03: Leak Detect Error]");
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_activate_error_injection_payload_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_activate_error_injection_payload_req(
            instanceId, errorInjectionType, errorInjectionSubtype, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_activate_error_injection_payload_resp(
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
        result["Error Injection Type"] = errorInjectionType;
        result["Error Injection Subtype"] = errorInjectionSubtype;
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint16_t errorInjectionType{};
    uint16_t errorInjectionSubtype{};
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
        result["Thermal error injection enabled"] =
            bool((data.mask[0] >> EI_THERMAL_ERRORS) & 0x01);
        result["Device error injection enabled"] =
            bool((data.mask[0] >> EI_DEVICE_ERRORS) & 0x01);
        result["GPIO spoofing error injection enabled"] =
            bool((data.mask[0] >> EI_GPIO_SPOOFING) & 0x01);
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
                result["Any NVSW EROT"] = (int)data.nvSwitch;
                result["NVSW 1"] = (int)data.nvSwitch1;
                result["NVSW 2"] = (int)data.nvSwitch2;
                result["PEXSW EROT"] = (int)data.pex;
                result["FRU EEPROM (Baseboard or CX7 or HMC)"] =
                    (int)data.baseboard;
                result["HMC SPI Flash"] = (int)data.hmc;
                result["Any Retimer"] = (int)data.retimer;
                result["Retimer 1"] = (int)data.retimer1;
                result["Retimer 2"] = (int)data.retimer2;
                result["Retimer 3"] = (int)data.retimer3;
                result["Retimer 4"] = (int)data.retimer4;
                result["Retimer 5"] = (int)data.retimer5;
                result["Retimer 6"] = (int)data.retimer6;
                result["Retimer 7"] = (int)data.retimer7;
                result["Retimer 8"] = (int)data.retimer8;
                result["GPUs 1-4 SPI Flash"] = (int)data.gpu1_4;
                result["GPUs 5-8 SPI Flash"] = (int)data.gpu5_8;
                result["GPUs SPI Flash"] = (int)(data.gpu1_4 && data.gpu5_8 &&
                                                 data.gpu9_12 && data.gpu13_16);
                result["CX8 SPI Flash"] = (int)data.cx8;
                result["GPU 1"] = (int)data.gpu1;
                result["GPU 2"] = (int)data.gpu2;
                result["GPU 3"] = (int)data.gpu3;
                result["GPU 4"] = (int)data.gpu4;
                result["GPU 5"] = (int)data.gpu5;
                result["GPU 6"] = (int)data.gpu6;
                result["GPU 7"] = (int)data.gpu7;
                result["GPU 8"] = (int)data.gpu8;
                result["GPU 9"] = (int)data.gpu9;
                result["GPU 10"] = (int)data.gpu10;
                result["GPU 11"] = (int)data.gpu11;
                result["GPU 12"] = (int)data.gpu12;
                result["GPU 13"] = (int)data.gpu13;
                result["GPU 14"] = (int)data.gpu14;
                result["GPU 15"] = (int)data.gpu15;
                result["GPU 16"] = (int)data.gpu16;
                result["CPU 1"] = (int)data.cpu1;
                result["CPU 2"] = (int)data.cpu2;
                result["CPU 3"] = (int)data.cpu3;
                result["CPU 4"] = (int)data.cpu4;

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
        {RP_INFOROM_RECREATE_ALLOW_INB, "InfoROM filesystem recreate"},
        {RP_RUNTIME_IN_SYSTEM_TEST, "Runtime in system test"},
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

class SetDevicemodeSettings : public CommandInterface
{
  public:
    ~SetDevicemodeSettings() = default;
    SetDevicemodeSettings() = delete;
    SetDevicemodeSettings(const SetDevicemodeSettings&) = delete;
    SetDevicemodeSettings(SetDevicemodeSettings&&) = default;
    SetDevicemodeSettings& operator=(const SetDevicemodeSettings&) = delete;
    SetDevicemodeSettings& operator=(SetDevicemodeSettings&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetDevicemodeSettings(const char* type, const char* name,
                                   CLI::App* app) :
        CommandInterface(type, name, app), device_mode_index(0),
        l1_prediction_mode(nsm_l1_prediction_mode_config::DISABLED)
    {
        auto setNetworkInterfaceMode =
            app->add_option_group("Required", "Set Network Interface Mode");
        setNetworkInterfaceMode->add_option("-i, --index", device_mode_index,
                                            "device_mode_index");
        setNetworkInterfaceMode->add_option("-M, --mode", l1_prediction_mode,
                                            "l1_prediction_mode\n"
                                            "1 - Enable\n"
                                            "0 - Disable");
        setNetworkInterfaceMode->require_option(2);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_setting_req), 0);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_device_mode_setting_req(
            instanceId, device_mode_index, l1_prediction_mode, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_set_device_mode_setting_resp(
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
        result["Reason Code"] = reason_code;
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t device_mode_index;
    enum nsm_l1_prediction_mode_config l1_prediction_mode;
};

class GetDevicemodeSettings : public CommandInterface
{
  public:
    ~GetDevicemodeSettings() = default;
    GetDevicemodeSettings() = delete;
    GetDevicemodeSettings(const GetDevicemodeSettings&) = delete;
    GetDevicemodeSettings(GetDevicemodeSettings&&) = default;
    GetDevicemodeSettings& operator=(const GetDevicemodeSettings&) = delete;
    GetDevicemodeSettings& operator=(GetDevicemodeSettings&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetDevicemodeSettings(const char* type, const char* name,
                                   CLI::App* app) :
        CommandInterface(type, name, app)
    {
        mode_index = 0;
        auto getNetworkInterfaceMode =
            app->add_option_group("Required", "Get Network Interface Mode");
        getNetworkInterfaceMode->add_option("-r, --mode_index", mode_index,
                                            "mode_index");
        getNetworkInterfaceMode->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_req), 0);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_device_mode_setting_req(instanceId, mode_index,
                                                     request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        enum nsm_l1_prediction_mode_config device_mode;

        auto rc = decode_get_device_mode_setting_resp(
            responsePtr, payloadLength, &cc, &reason_code, &device_mode);

        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << static_cast<int>(reason_code)
                      << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_device_mode_setting_resp));
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Device Mode"] = static_cast<int>(device_mode);
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t mode_index;
};

class GetDeviceModeSettingsV2 : public CommandInterface
{
  public:
    ~GetDeviceModeSettingsV2() = default;
    GetDeviceModeSettingsV2() = delete;
    GetDeviceModeSettingsV2(const GetDeviceModeSettingsV2&) = delete;
    GetDeviceModeSettingsV2(GetDeviceModeSettingsV2&&) = default;
    GetDeviceModeSettingsV2& operator=(const GetDeviceModeSettingsV2&) = delete;
    GetDeviceModeSettingsV2& operator=(GetDeviceModeSettingsV2&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetDeviceModeSettingsV2(const char* type, const char* name,
                                     CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto getDeviceModeSettingsV2Group =
            app->add_option_group("Required", "Get Device Mode Settings v2");
        getDeviceModeSettingsV2Group->add_option(
            "-i, --deviceModeIndex", deviceModeIndex,
            "Device mode index (NvU32)\n"
            "  3 - DPU Operation Mode (1 byte: 0=DPU, 1=NIC)\n"
            "  4 - PCIe Device Mode (3 bytes: multi-socket, EW, bifurcation)\n"
            "      value encodes sub-modes in LE byte order:\n"
            "      byte[0]=multi-socket byte[1]=controlledEW byte[2]=bifurcation\n"
            "  7 - Max AC Power Ramp Rate Config (NvU32, Watts/sec)\n"
            "  8 - SoC Power Smoothing Enabled (NvBool)\n"
            "  9 - SoC Power Smoothing Preset Index (NvU8)\n"
            " 10 - SoC Power Brake Enabled (NvBool)\n"
            " 11 - One Shot GPU Base Power Limit\n"
            " 12 - Persistent GPU Base Power Limit\n"
            " 13 - One Shot CPU Power Limit GPU Copy\n"
            " 14 - Persistent CPU Power Limit GPU Copy\n"
            " 15 - One Shot GPU Copy Switch Power Limit\n"
            " 16 - Persistent GPU Copy Switch Power Limit");
        getDeviceModeSettingsV2Group->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_req),
            0);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_device_mode_settings_v2_req(
            instanceId, deviceModeIndex, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        uint8_t currentModeData[256];
        uint16_t currentModeLength = 0;
        uint8_t pendingModeData[256];
        uint16_t pendingModeLength = 0;

        auto rc = decode_get_device_mode_settings_v2_resp(
            responsePtr, payloadLength, &cc, &reasonCode, currentModeData,
            &currentModeLength, pendingModeData, &pendingModeLength);

        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << static_cast<int>(reasonCode)
                      << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Current Mode Length"] = currentModeLength;

        if (currentModeLength == sizeof(uint32_t))
        {
            uint32_t currentValue;
            memcpy(&currentValue, currentModeData, sizeof(uint32_t));
            currentValue = le32toh(currentValue);
            result["Current Mode Value"] = currentValue;
        }
        else if (currentModeLength > 0)
        {
            std::vector<uint8_t> currentData(
                currentModeData, currentModeData + currentModeLength);
            result["Current Mode Data"] = currentData;
        }

        result["Pending Mode Length"] = pendingModeLength;
        if (pendingModeLength == sizeof(uint32_t))
        {
            uint32_t pendingValue;
            memcpy(&pendingValue, pendingModeData, sizeof(uint32_t));
            pendingValue = le32toh(pendingValue);
            result["Pending Mode Value"] = pendingValue;
        }
        else if (pendingModeLength > 0)
        {
            std::vector<uint8_t> pendingData(
                pendingModeData, pendingModeData + pendingModeLength);
            result["Pending Mode Data"] = pendingData;
        }

        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint32_t deviceModeIndex = 0;
};

class SetDeviceModeSettingsV2 : public CommandInterface
{
  public:
    ~SetDeviceModeSettingsV2() = default;
    SetDeviceModeSettingsV2() = delete;
    SetDeviceModeSettingsV2(const SetDeviceModeSettingsV2&) = delete;
    SetDeviceModeSettingsV2(SetDeviceModeSettingsV2&&) = default;
    SetDeviceModeSettingsV2& operator=(const SetDeviceModeSettingsV2&) = delete;
    SetDeviceModeSettingsV2& operator=(SetDeviceModeSettingsV2&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetDeviceModeSettingsV2(const char* type, const char* name,
                                     CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto setDeviceModeSettingsV2Group =
            app->add_option_group("Required", "Set Device Mode Settings v2");
        setDeviceModeSettingsV2Group->add_option(
            "-i, --deviceModeIndex", deviceModeIndex,
            "Device mode index (NvU32)\n"
            "  3 - DPU Operation Mode (1 byte: 0=DPU, 1=NIC)\n"
            "  4 - PCIe Device Mode (3 bytes: multi-socket, EW, bifurcation)\n"
            "      value encodes sub-modes in LE byte order:\n"
            "      byte[0]=multi-socket byte[1]=controlledEW byte[2]=bifurcation\n"
            "  7 - Max AC Power Ramp Rate Config (NvU32, Watts/sec)\n"
            "  8 - SoC Power Smoothing Enabled (NvBool)\n"
            "  9 - SoC Power Smoothing Preset Index (NvU8)\n"
            " 10 - SoC Power Brake Enabled (NvBool)\n"
            " 11 - One Shot GPU Base Power Limit (NvU32)\n"
            " 12 - Persistent GPU Base Power Limit (NvU32)\n"
            " 13 - One Shot CPU Power Limit GPU Copy (NvU32)\n"
            " 14 - Persistent CPU Power Limit GPU Copy (NvU32)");
        setDeviceModeSettingsV2Group->add_option("-l, --value", deviceModeValue,
                                                 "Device mode value (uint32)");
        setDeviceModeSettingsV2Group->add_option(
            "-s, --size", deviceModeDataSize,
            "Number of bytes of mode data to send (default: 4).\n"
            "Must match the mode data length expected by the device\n"
            "for the given device mode index (see NSM spec).\n"
            "Only the first -s bytes of the LE value are sent.\n"
            "Max: 4 bytes.");
        setDeviceModeSettingsV2Group->require_option(2);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        uint8_t payload[sizeof(uint32_t)] = {0};
        uint16_t payloadLen = 0;

        switch (deviceModeIndex)
        {
            case DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED:
            case DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX:
            case DEVICE_MODE_SOC_POWER_BRAKE_ENABLED:
            {
                payload[0] = static_cast<uint8_t>(
                    static_cast<uint32_t>(deviceModeValue));
                payloadLen = sizeof(uint8_t);
                break;
            }
            default:
            {
                uint16_t modeDataLen =
                    static_cast<uint16_t>(deviceModeDataSize);
                if (modeDataLen > sizeof(uint32_t))
                {
                    std::cerr
                        << "Error: mode data size cannot exceed 4 bytes\n";
                    return {NSM_SW_ERROR, {}};
                }
                uint32_t valueLE =
                    htole32(static_cast<uint32_t>(deviceModeValue));
                memcpy(payload, &valueLE, sizeof(uint32_t));
                payloadLen = modeDataLen;
                break;
            }
        }

        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) +
                payloadLen - 1,
            0);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_device_mode_settings_v2_req(
            instanceId, deviceModeIndex, payload, payloadLen, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;

        auto rc = decode_set_device_mode_settings_v2_resp(
            responsePtr, payloadLength, &cc, &reasonCode);

        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << static_cast<int>(reasonCode)
                      << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint32_t deviceModeIndex = 0;
    uint32_t deviceModeValue = 0;
    uint32_t deviceModeDataSize = sizeof(uint32_t);
};

class SetDeviceConfig : public CommandInterface
{
  public:
    ~SetDeviceConfig() = default;
    SetDeviceConfig() = delete;
    SetDeviceConfig(const SetDeviceConfig&) = delete;
    SetDeviceConfig(SetDeviceConfig&&) = default;
    SetDeviceConfig& operator=(const SetDeviceConfig&) = delete;
    SetDeviceConfig& operator=(SetDeviceConfig&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetDeviceConfig(const char* type, const char* name,
                             CLI::App* app) : CommandInterface(type, name, app)
    {
        auto g = app->add_option_group("Required", "Set Device Config (0x10)");
        g->add_option("-t,--type", typeStr,
                      "Device configuration type (decimal or 0x hex)");
        g->add_option(
            "-d,--data-hex", dataHex,
            "Configuration data as hex string (even length; total with 4-byte "
            "type must fit uint16_t V2 data_size)");
        g->require_option(2);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> payload;
        if (!parseHexToBytes(dataHex, payload))
        {
            std::cerr << "Invalid --data-hex (use pairs of hex digits)\n";
            return {NSM_SW_ERROR_DATA, {}};
        }
        if (payload.size() > static_cast<size_t>(UINT16_MAX) - sizeof(uint32_t))
        {
            std::cerr << "Data plus 4-byte type exceeds uint16_t data_size\n";
            return {NSM_SW_ERROR_LENGTH, {}};
        }
        char* endptr = nullptr;
        unsigned long parsedType = std::strtoul(typeStr.c_str(), &endptr, 0);
        if (endptr == typeStr.c_str() || *endptr != '\0')
        {
            std::cerr << "Invalid --type value\n";
            return {NSM_SW_ERROR_DATA, {}};
        }
        uint32_t cfgType = static_cast<uint32_t>(parsedType);
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                            sizeof(nsm_common_req_v2) +
                                            sizeof(uint32_t) + payload.size(),
                                        0);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        int rc = encode_set_device_config_v2_req(
            instanceId, cfgType, payload.data(),
            static_cast<uint16_t>(payload.size()), request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        auto rc = decode_set_device_config_v2_resp(responsePtr, payloadLength,
                                                   &cc, &reasonCode);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "SetDeviceConfig response error: rc=" << rc
                      << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << reasonCode << "\n";
            return;
        }
        ordered_json result;
        result["Completion Code"] = static_cast<int>(cc);
        result["Command"] = "SetDeviceConfig (0x10)";
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    std::string typeStr;
    std::string dataHex;
};

class GetDeviceConfig : public CommandInterface
{
  public:
    ~GetDeviceConfig() = default;
    GetDeviceConfig() = delete;
    GetDeviceConfig(const GetDeviceConfig&) = delete;
    GetDeviceConfig(GetDeviceConfig&&) = default;
    GetDeviceConfig& operator=(const GetDeviceConfig&) = delete;
    GetDeviceConfig& operator=(GetDeviceConfig&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetDeviceConfig(const char* type, const char* name,
                             CLI::App* app) : CommandInterface(type, name, app)
    {
        auto g = app->add_option_group("Required", "Get Device Config (0x11)");
        g->add_option("-t,--type", typeStr,
                      "Device configuration type (decimal or 0x hex)");
        g->add_option(
            "-q,--query-hex", queryHex,
            "Optional query blob as hex (identifier fields per spec)");
        g->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> query;
        if (!parseHexToBytes(queryHex, query))
        {
            std::cerr << "Invalid --query-hex\n";
            return {NSM_SW_ERROR_DATA, {}};
        }
        char* endptr = nullptr;
        unsigned long parsedType = std::strtoul(typeStr.c_str(), &endptr, 0);
        if (endptr == typeStr.c_str() || *endptr != '\0')
        {
            std::cerr << "Invalid --type value\n";
            return {NSM_SW_ERROR_DATA, {}};
        }
        uint32_t cfgType = static_cast<uint32_t>(parsedType);
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                            sizeof(nsm_common_req_v2) +
                                            sizeof(uint32_t) + query.size(),
                                        0);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        int rc = encode_get_device_config_v2_req(
            instanceId, cfgType, query.data(),
            static_cast<uint16_t>(query.size()), request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        std::vector<uint8_t> cur(static_cast<size_t>(UINT16_MAX));
        uint16_t curLen = 0;
        std::vector<uint8_t> pend(static_cast<size_t>(UINT16_MAX));
        uint16_t pendLen = 0;
        auto rc = decode_get_device_config_v2_resp(
            responsePtr, payloadLength, &cc, &reasonCode, cur.data(), &curLen,
            pend.data(), &pendLen);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "GetDeviceConfig response error: rc=" << rc
                      << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << reasonCode << "\n";
            return;
        }
        ordered_json result;
        result["Completion Code"] = static_cast<int>(cc);
        result["Command"] = "GetDeviceConfig (0x11)";
        result["Current Config Length"] = curLen;
        result["Current Config Hex"] = bytesToHexString(cur.data(), curLen);
        result["Pending Config Length"] = pendLen;
        result["Pending Config Hex"] = bytesToHexString(pend.data(), pendLen);
        if (curLen == sizeof(uint32_t))
        {
            uint32_t v;
            std::memcpy(&v, cur.data(), sizeof(v));
            result["Current Config As NvU32"] = le32toh(v);
        }
        if (pendLen == sizeof(uint32_t))
        {
            uint32_t v;
            std::memcpy(&v, pend.data(), sizeof(v));
            result["Pending Config As NvU32"] = le32toh(v);
        }
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    std::string typeStr;
    std::string queryHex;
};

/* OOB Miswiring Detection: convenience nsmtool
 * subcommands targeting NSM Type 5 DEVICE_MODE_LLDP (index 24) LLDP mode
 * bitfield on devices reporting NSM_DEV_ID_PCIE_BRIDGE via Type 0
 * GetDeviceIdentification. */
class GetLLDPMode : public CommandInterface
{
  public:
    ~GetLLDPMode() = default;
    GetLLDPMode() = delete;
    GetLLDPMode(const GetLLDPMode&) = delete;
    GetLLDPMode(GetLLDPMode&&) = default;
    GetLLDPMode& operator=(const GetLLDPMode&) = delete;
    GetLLDPMode& operator=(GetLLDPMode&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetLLDPMode(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {}

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_req),
            0);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_device_mode_settings_v2_req(
            instanceId, static_cast<uint32_t>(DEVICE_MODE_LLDP), request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        constexpr size_t maxModeBytes = 16;
        uint8_t currentData[maxModeBytes] = {};
        uint8_t pendingData[maxModeBytes] = {};
        uint16_t currentLength = 0;
        uint16_t pendingLength = 0;

        auto rc = decode_get_device_mode_settings_v2_resp(
            responsePtr, payloadLength, &cc, &reasonCode, currentData,
            &currentLength, pendingData, &pendingLength);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: rc=" << rc
                      << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << static_cast<int>(reasonCode)
                      << "\n";
            return;
        }
        if (currentLength != sizeof(nsm_lldp_mode_bitfield))
        {
            std::cerr << "Invalid current-mode payload length " << currentLength
                      << " (expected 1 byte)\n";
            return;
        }
        struct nsm_lldp_mode_bitfield view = {};
        memcpy(&view, &currentData[0], sizeof(view));
        ordered_json result;
        result["TX Mode"] = static_cast<uint8_t>(view.tx_mode);
        result["RX Mode"] = static_cast<uint8_t>(view.rx_mode);
        result["DCBX Mode"] = static_cast<uint8_t>(view.dcbx_mode);
        result["Raw Byte"] = currentData[0];
        nsmtool::helper::DisplayInJson(result);
    }
};

class SetLLDPMode : public CommandInterface
{
  public:
    ~SetLLDPMode() = default;
    SetLLDPMode() = delete;
    SetLLDPMode(const SetLLDPMode&) = delete;
    SetLLDPMode(SetLLDPMode&&) = default;
    SetLLDPMode& operator=(const SetLLDPMode&) = delete;
    SetLLDPMode& operator=(SetLLDPMode&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetLLDPMode(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto g = app->add_option_group(
            "Required",
            "Set LLDP Mode (NSM Type 5 idx 24): tx/rx 0=Off 1=Mandatory 2=All; dcbx 0=Disabled 1=Enabled");
        g->add_option("--tx", txMode, "TX mode (0/1/2)");
        g->add_option("--rx", rxMode, "RX mode (0/1/2)");
        g->add_option("--dcbx", dcbxMode, "DCBX mode (0/1)");
        g->require_option(3);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        if (txMode > NSM_LLDP_DIR_MODE_ALL || rxMode > NSM_LLDP_DIR_MODE_ALL ||
            dcbxMode > NSM_LLDP_DCBX_ENABLED)
        {
            std::cerr << "Invalid LLDP bitfield values\n";
            return {NSM_SW_ERROR_DATA, {}};
        }
        struct nsm_lldp_mode_bitfield view = {};
        view.tx_mode = txMode;
        view.rx_mode = rxMode;
        view.dcbx_mode = dcbxMode;
        uint8_t encoded = 0;
        memcpy(&encoded, &view, sizeof(encoded));
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req),
            0);
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_device_mode_settings_v2_req(
            instanceId, static_cast<uint32_t>(DEVICE_MODE_LLDP), &encoded,
            sizeof(encoded), request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        auto rc = decode_set_device_mode_settings_v2_resp(
            responsePtr, payloadLength, &cc, &reasonCode);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response error: rc=" << rc
                      << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << static_cast<int>(reasonCode)
                      << "\n";
            return;
        }
        ordered_json result;
        result["Completion Code"] = cc;
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t txMode{0};
    uint8_t rxMode{0};
    uint8_t dcbxMode{0};
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

    auto setErrorInjectionPayload = config->add_subcommand(
        "SetErrorInjectionPayload", "Set Error Injection Payload");
    commands.push_back(std::make_unique<SetErrorInjectionPayload>(
        "config", "SetErrorInjectionPayload", setErrorInjectionPayload));

    auto getErrorInjectionPayload = config->add_subcommand(
        "GetErrorInjectionPayload", "Get Current Error Injection Payload");
    commands.push_back(std::make_unique<GetErrorInjectionPayload>(
        "config", "GetErrorInjectionPayload", getErrorInjectionPayload));

    auto activateErrorInjectionPayload = config->add_subcommand(
        "ActivateErrorInjectionPayload", "Activate Error Injection Payload");
    commands.push_back(std::make_unique<ActivateErrorInjectionPayload>(
        "config", "ActivateErrorInjectionPayload",
        activateErrorInjectionPayload));

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

    auto getDeviceModeSettings = config->add_subcommand(
        "GetDevicemodeSettings", "Get device mode settings");
    commands.push_back(std::make_unique<GetDevicemodeSettings>(
        "config", "GetDevicemodeSettings", getDeviceModeSettings));

    auto setDeviceModeSettings = config->add_subcommand(
        "SetDevicemodeSettings", "Set device mode settings");
    commands.push_back(std::make_unique<SetDevicemodeSettings>(
        "config", "SetDevicemodeSettings", setDeviceModeSettings));

    auto getDeviceModeSettingsV2 = config->add_subcommand(
        "GetDeviceModeSettingsV2", "Get device mode settings v2 (0x83)");
    commands.push_back(std::make_unique<GetDeviceModeSettingsV2>(
        "config", "GetDeviceModeSettingsV2", getDeviceModeSettingsV2));

    auto setDeviceModeSettingsV2 = config->add_subcommand(
        "SetDeviceModeSettingsV2", "Set device mode settings v2 (0x82)");
    commands.push_back(std::make_unique<SetDeviceModeSettingsV2>(
        "config", "SetDeviceModeSettingsV2", setDeviceModeSettingsV2));

    auto setDeviceConfig = config->add_subcommand(
        "SetDeviceConfig", "Set Device Config Type 5 command 0x10 (V2)");
    commands.push_back(std::make_unique<SetDeviceConfig>(
        "config", "SetDeviceConfig", setDeviceConfig));

    auto getDeviceConfig = config->add_subcommand(
        "GetDeviceConfig", "Get Device Config Type 5 command 0x11 (V2)");
    commands.push_back(std::make_unique<GetDeviceConfig>(
        "config", "GetDeviceConfig", getDeviceConfig));

    // OOB Miswiring Detection
    auto getLldpMode = config->add_subcommand(
        "GetLLDPMode", "Get LLDP mode bitfield (NSM Type 5 idx 24)");
    commands.push_back(
        std::make_unique<GetLLDPMode>("config", "GetLLDPMode", getLldpMode));

    auto setLldpMode = config->add_subcommand(
        "SetLLDPMode", "Set LLDP mode bitfield (NSM Type 5 idx 24)");
    commands.push_back(
        std::make_unique<SetLLDPMode>("config", "SetLLDPMode", setLldpMode));
}

} // namespace config

} // namespace nsmtool
