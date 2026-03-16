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

#include "nsm_discovery_cmd.hpp"

#include "base.h"
#include "device-capability-discovery.h"

#include "cmd_helper.hpp"

#include <CLI/CLI.hpp>

namespace nsmtool
{

namespace discovery
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

class Ping : public CommandInterface
{
  public:
    ~Ping() = default;
    Ping() = delete;
    Ping(const Ping&) = delete;
    Ping(Ping&&) = default;
    Ping& operator=(const Ping&) = delete;
    Ping& operator=(Ping&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_ping_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_ping_resp(responsePtr, payloadLength, &cc,
                                   &reason_code);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;

        nsmtool::helper::DisplayInJson(result);
    }
};

class GetSupportedMessageTypes : public CommandInterface
{
  public:
    ~GetSupportedMessageTypes() = default;
    GetSupportedMessageTypes() = delete;
    GetSupportedMessageTypes(const GetSupportedMessageTypes&) = delete;
    GetSupportedMessageTypes(GetSupportedMessageTypes&&) = default;
    GetSupportedMessageTypes&
        operator=(const GetSupportedMessageTypes&) = delete;
    GetSupportedMessageTypes& operator=(GetSupportedMessageTypes&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_supported_nvidia_message_types_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_supported_nvidia_message_types_req(instanceId,
                                                                request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        bitfield8_t supportedTypes[SUPPORTED_MSG_TYPE_DATA_SIZE];

        auto rc = decode_get_supported_nvidia_message_types_resp(
            responsePtr, payloadLength, &cc, &reason_code, &supportedTypes[0]);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        std::string key("Supported Nvidia Message Types");

        parseBitfieldVar(result, key, &supportedTypes[0],
                         SUPPORTED_MSG_TYPE_DATA_SIZE);

        nsmtool::helper::DisplayInJson(result);
    }
};

class GetSupportedCommandCodes : public CommandInterface
{
  public:
    ~GetSupportedCommandCodes() = default;
    GetSupportedCommandCodes() = delete;
    GetSupportedCommandCodes(const GetSupportedCommandCodes&) = delete;
    GetSupportedCommandCodes(GetSupportedCommandCodes&&) = default;
    GetSupportedCommandCodes&
        operator=(const GetSupportedCommandCodes&) = delete;
    GetSupportedCommandCodes& operator=(GetSupportedCommandCodes&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetSupportedCommandCodes(const char* type, const char* name,
                                      CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required",
            "Retrieve supported command codes for the requested Nvidia message type");
        ccOptionGroup->add_option(
            "-t,--type", nvidiaMsgType,
            "retrieve supported command codes for the message type specified.");
        ccOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_supported_command_codes_req(
            instanceId, nvidiaMsgType, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        bitfield8_t supportedCommandCodes[SUPPORTED_COMMAND_CODE_DATA_SIZE];

        auto rc = decode_get_supported_command_codes_resp(
            responsePtr, payloadLength, &cc, &reason_code,
            &supportedCommandCodes[0]);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Nvidia Message Type"] = nvidiaMsgType;
        std::string key("Supported Command codes");

        parseBitfieldVar(result, key, &supportedCommandCodes[0],
                         SUPPORTED_COMMAND_CODE_DATA_SIZE);

        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t nvidiaMsgType{};
};

class QueryDeviceIdentification : public CommandInterface
{
  public:
    ~QueryDeviceIdentification() = default;
    QueryDeviceIdentification() = delete;
    QueryDeviceIdentification(const QueryDeviceIdentification&) = delete;
    QueryDeviceIdentification(QueryDeviceIdentification&&) = default;
    QueryDeviceIdentification&
        operator=(const QueryDeviceIdentification&) = delete;
    QueryDeviceIdentification& operator=(QueryDeviceIdentification&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_query_device_identification_req(instanceId,
                                                             request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint8_t device_identification;
        uint8_t device_instanceID;
        auto rc = decode_query_device_identification_resp(
            responsePtr, payloadLength, &cc, &reason_code,
            &device_identification, &device_instanceID);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Compeletion Code"] = cc;
        switch (device_identification)
        {
            case NSM_DEV_ID_GPU:
                result["Device Identification"] = "GPU";
                break;
            case NSM_DEV_ID_SWITCH:
                result["Device Identification"] = "Switch";
                break;
            case NSM_DEV_ID_PCIE_BRIDGE:
                result["Device Identification"] = "PCIe Bridge";
                break;
            case NSM_DEV_ID_BASEBOARD:
                result["Device Identification"] = "Baseboard";
                break;
            case NSM_DEV_ID_EROT:
                result["Device Identification"] = "ERoT";
                break;
            case NSM_DEV_ID_MCTP_BRIDGE:
                result["Device Identification"] = "MCTP Bridge";
                break;
            case NSM_DEV_ID_CPU:
                result["Device Identification"] = "CPU";
                break;
            case NSM_DEV_ID_UNKNOWN:
                result["Device Identification"] = "Unknown";
                break;
            default:
                std::cerr << "Invalid device identification received: "
                          << (int)device_identification << "\n";
                return;
        }
        result["Device Instance ID"] = device_instanceID;

        nsmtool::helper::DisplayInJson(result);
    }
};

class GetHistogramFormat : public CommandInterface
{
  public:
    ~GetHistogramFormat() = default;
    GetHistogramFormat() = delete;
    GetHistogramFormat(const GetHistogramFormat&) = delete;
    GetHistogramFormat(GetHistogramFormat&&) = default;
    GetHistogramFormat& operator=(const GetHistogramFormat&) = delete;
    GetHistogramFormat& operator=(GetHistogramFormat&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetHistogramFormat(const char* type, const char* name,
                                CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto histoFormatOptionGroup = app->add_option_group(
            "Required",
            "histogram Id value for which format info is to be retrieved.");

        histogramId = 0;
        parameter = 0;
        histoFormatOptionGroup->add_option(
            "-i, --histogramId", histogramId,
            "retrieve histogram format information for histogram Id");
        histoFormatOptionGroup->add_option(
            "-p, --parameter", parameter,
            "parameter value for histogram format request");
        histoFormatOptionGroup->require_option(2);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_histogram_format_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_histogram_format_req(instanceId, histogramId,
                                                  parameter, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        struct nsm_histogram_format_metadata metaData;
        std::vector<uint8_t> bucket_offsets(65535, 0);
        uint32_t total_bucket_offset_size;

        auto rc = decode_get_histogram_format_resp(
            responsePtr, payloadLength, &cc, &reason_code, &data_size,
            &metaData, bucket_offsets.data(), &total_bucket_offset_size);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Number of Buckets"] =
            static_cast<uint16_t>(metaData.num_of_buckets);
        result["Min Sampling Time"] =
            static_cast<uint32_t>(metaData.min_sampling_time);
        result["Accumulation Cycle"] =
            static_cast<uint8_t>(metaData.accumulation_cycle);
        result["Increment Duration"] =
            static_cast<uint32_t>(metaData.increment_duration);
        result["Unit of Measure"] =
            static_cast<uint8_t>(metaData.bucket_unit_of_measure);
        result["Data type of Bucket Data"] =
            static_cast<uint8_t>(metaData.bucket_data_type);

        size_t i;
        switch (metaData.bucket_data_type)
        {
            case NvU8:
                bucket_offsets.resize((metaData.num_of_buckets + 1) *
                                      sizeof(uint8_t));
                for (i = 0; i < metaData.num_of_buckets; i++)
                {
                    result["Bucket Offsets"].push_back(
                        static_cast<uint8_t>(bucket_offsets[i]));
                }
                break;
            case NvS8:
                bucket_offsets.resize((metaData.num_of_buckets + 1) *
                                      sizeof(int8_t));
                for (i = 0; i < metaData.num_of_buckets; i++)
                {
                    result["Bucket Offsets"].push_back(
                        static_cast<int8_t>(bucket_offsets[i]));
                }
                break;
            case NvU16:
            {
                bucket_offsets.resize((metaData.num_of_buckets + 1) *
                                      sizeof(uint16_t));
                uint16_t* data16 = (uint16_t*)bucket_offsets.data();
                for (i = 0; i < metaData.num_of_buckets; i++)
                {
                    result["Bucket Offsets"].push_back(
                        static_cast<uint16_t>(data16[i]));
                }
                break;
            }
            case NvS16:
            {
                bucket_offsets.resize((metaData.num_of_buckets + 1) *
                                      sizeof(int16_t));
                int16_t* data16 = (int16_t*)bucket_offsets.data();
                for (i = 0; i < metaData.num_of_buckets; i++)
                {
                    result["Bucket Offsets"].push_back(
                        static_cast<int16_t>(data16[i]));
                }
                break;
            }
            case NvU32:
            {
                bucket_offsets.resize((metaData.num_of_buckets + 1) *
                                      sizeof(uint32_t));
                uint32_t* data32 = (uint32_t*)bucket_offsets.data();
                for (i = 0; i < metaData.num_of_buckets; i++)
                {
                    result["Bucket Offsets"].push_back(
                        static_cast<uint32_t>(data32[i]));
                }
                break;
            }
            case NvS32:
            {
                bucket_offsets.resize((metaData.num_of_buckets + 1) *
                                      sizeof(int32_t));
                int32_t* data32 = (int32_t*)bucket_offsets.data();
                for (i = 0; i < metaData.num_of_buckets; i++)
                {
                    result["Bucket Offsets"].push_back(
                        static_cast<int32_t>(data32[i]));
                }
                break;
            }
            case NvU64:
            {
                bucket_offsets.resize((metaData.num_of_buckets + 1) *
                                      sizeof(uint32_t));
                uint64_t* data64 = (uint64_t*)bucket_offsets.data();
                for (i = 0; i < metaData.num_of_buckets; i++)
                {
                    result["Bucket Offsets"].push_back(
                        static_cast<uint64_t>(data64[i]));
                }
                break;
            }
            case NvS64:
            {
                bucket_offsets.resize((metaData.num_of_buckets + 1) *
                                      sizeof(uint64_t));
                int64_t* data64 = (int64_t*)bucket_offsets.data();
                for (i = 0; i < metaData.num_of_buckets; i++)
                {
                    result["Bucket Offsets"].push_back(
                        static_cast<int64_t>(data64[i]));
                }
                break;
            }
            case NvS24_8:
            {
                bucket_offsets.resize((metaData.num_of_buckets + 1) *
                                      sizeof(float));
                float* data32 = (float*)bucket_offsets.data();
                for (i = 0; i < metaData.num_of_buckets; i++)
                {
                    result["Bucket Offsets"].push_back(
                        static_cast<float>(data32[i]));
                }
                break;
            }
            default:
                printf("Unknown data type.\n");
                break;
        }
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint32_t histogramId;
    uint8_t parameter;
};

class GetHistogramData : public CommandInterface
{
  public:
    ~GetHistogramData() = default;
    GetHistogramData() = delete;
    GetHistogramData(const GetHistogramData&) = delete;
    GetHistogramData(GetHistogramData&&) = default;
    GetHistogramData& operator=(const GetHistogramData&) = delete;
    GetHistogramData& operator=(GetHistogramData&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetHistogramData(const char* type, const char* name,
                              CLI::App* app) : CommandInterface(type, name, app)
    {
        auto histoDataOptionGroup = app->add_option_group(
            "Required",
            "histogram Id value for which data is to be retrieved.");

        histogramId = 0;
        parameter = 0;
        histoDataOptionGroup->add_option(
            "-i, --histogramId", histogramId,
            "retrieve histogram format information for histogram Id");
        histoDataOptionGroup->add_option(
            "-p, --parameter", parameter,
            "parameter value for histogram data request");
        histoDataOptionGroup->require_option(2);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_histogram_data_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_histogram_data_req(instanceId, histogramId,
                                                parameter, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        uint16_t number_of_buckets;
        std::vector<uint8_t> bucket_data(65535, 0);
        uint32_t total_bucket_offset_size;
        uint8_t dataTypeOfBucket;

        auto rc = decode_get_histogram_data_resp(
            responsePtr, payloadLength, &cc, &reason_code, &data_size,
            &dataTypeOfBucket, &number_of_buckets, bucket_data.data(),
            &total_bucket_offset_size);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Bucket Data Type"] = dataTypeOfBucket;
        result["Number of Buckets"] = static_cast<uint16_t>(number_of_buckets);

        size_t i;
        switch (dataTypeOfBucket)
        {
            case NvU8:
                bucket_data.resize(number_of_buckets * sizeof(uint8_t));
                for (i = 0; i < number_of_buckets; i++)
                {
                    result["Bucket Data"].push_back(
                        static_cast<uint8_t>(bucket_data[i]));
                }
                break;
            case NvS8:
                bucket_data.resize(number_of_buckets * sizeof(int8_t));
                for (i = 0; i < number_of_buckets; i++)
                {
                    result["Bucket Data"].push_back(
                        static_cast<int8_t>(bucket_data[i]));
                }
                break;
            case NvU16:
            {
                bucket_data.resize(number_of_buckets * sizeof(uint16_t));
                uint16_t* data16 = (uint16_t*)bucket_data.data();
                for (i = 0; i < number_of_buckets; i++)
                {
                    result["Bucket Data"].push_back(
                        static_cast<uint16_t>(data16[i]));
                }
                break;
            }
            case NvS16:
            {
                bucket_data.resize(number_of_buckets * sizeof(int16_t));
                int16_t* data16 = (int16_t*)bucket_data.data();
                for (i = 0; i < number_of_buckets; i++)
                {
                    result["Bucket Data"].push_back(
                        static_cast<int16_t>(data16[i]));
                }
                break;
            }
            case NvU32:
            {
                bucket_data.resize(number_of_buckets * sizeof(uint32_t));
                uint32_t* data32 = (uint32_t*)bucket_data.data();
                for (i = 0; i < number_of_buckets; i++)
                {
                    result["Bucket Data"].push_back(
                        static_cast<uint32_t>(data32[i]));
                }
                break;
            }
            case NvS32:
            {
                bucket_data.resize(number_of_buckets * sizeof(int32_t));
                int32_t* data32 = (int32_t*)bucket_data.data();
                for (i = 0; i < number_of_buckets; i++)
                {
                    result["Bucket Data"].push_back(
                        static_cast<int32_t>(data32[i]));
                }
                break;
            }
            case NvU64:
            {
                bucket_data.resize(number_of_buckets * sizeof(uint64_t));
                uint64_t* data64 = (uint64_t*)bucket_data.data();
                for (i = 0; i < number_of_buckets; i++)
                {
                    result["Bucket Data"].push_back(
                        static_cast<uint64_t>(data64[i]));
                }
                break;
            }
            case NvS64:
            {
                bucket_data.resize(number_of_buckets * sizeof(int64_t));
                int64_t* data64 = (int64_t*)bucket_data.data();
                for (i = 0; i < number_of_buckets; i++)
                {
                    result["Bucket Data"].push_back(
                        static_cast<int64_t>(data64[i]));
                }
                break;
            }
            case NvS24_8:
            {
                bucket_data.resize(number_of_buckets * sizeof(float));
                float* data32 = (float*)bucket_data.data();
                for (i = 0; i < number_of_buckets; i++)
                {
                    result["Bucket Data"].push_back(
                        static_cast<float>(data32[i]));
                }
                break;
            }
            default:
                printf("Unknown data type.\n");
                break;
        }
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint32_t histogramId;
    uint8_t parameter;
};

class GetDeviceCapabilitiesV2 : public CommandInterface
{
  public:
    ~GetDeviceCapabilitiesV2() = default;
    GetDeviceCapabilitiesV2() = delete;
    GetDeviceCapabilitiesV2(const GetDeviceCapabilitiesV2&) = delete;
    GetDeviceCapabilitiesV2(GetDeviceCapabilitiesV2&&) = default;
    GetDeviceCapabilitiesV2& operator=(const GetDeviceCapabilitiesV2&) = delete;
    GetDeviceCapabilitiesV2& operator=(GetDeviceCapabilitiesV2&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_capabilities_v2_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_nsm_get_device_capabilities_v2_req(instanceId,
                                                            request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        uint8_t timestampGeneration = 0;
        uint32_t maximumInputBufferSize = 0;

        auto rc = decode_nsm_get_device_capabilities_v2_resp(
            responsePtr, payloadLength, &cc, &reasonCode, &timestampGeneration,
            &maximumInputBufferSize);

        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reasonCode << "\n";
            return;
        }

        ordered_json result;
        result["Completion code"] = cc;
        result["Reason code"] = reasonCode;
        result["Timestamp generation"] =
            timestampGenerationToString(timestampGeneration);
        result["Maximum input buffer size"] = maximumInputBufferSize;
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    std::string timestampGenerationToString(uint8_t timestampGeneration)
    {
        switch (timestampGeneration)
        {
            case NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_NONE:
                return "Device does not support generating timestamps";
            case NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_EPOCH_TIME:
                return "Device reports epoch-based timestamps";
            case NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_MONOTONIC_TIME:
                return "Device reports monotonic timer-based timestamps";
            default:
                return "Unknown value: " + std::to_string(timestampGeneration);
        }
    }
};

class GetGpioState : public CommandInterface
{
  public:
    ~GetGpioState() = default;
    GetGpioState() = delete;
    GetGpioState(const GetGpioState&) = delete;
    GetGpioState(GetGpioState&&) = default;
    GetGpioState& operator=(const GetGpioState&) = delete;
    GetGpioState& operator=(GetGpioState&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetGpioState(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto gpioStateOptionGroup = app->add_option_group(
            "Required", "GPIO offset and length parameters");

        offset = 0;
        length = 0;
        gpioStateOptionGroup
            ->add_option("-o, --offset", offset,
                         "Starting GPIO index offset (0-65535)")
            ->required();
        gpioStateOptionGroup
            ->add_option("-l, --length", length,
                         "Number of GPIO states to retrieve (1-65535)")
            ->required();
        gpioStateOptionGroup->require_option(2);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_gpio_state_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_gpio_state_req(instanceId, offset, length,
                                            request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        uint16_t resp_offset;
        uint16_t resp_length;
        std::vector<uint8_t> gpioValues(65535, 0);
        uint32_t gpioValuesSize;

        auto rc = decode_get_gpio_state_resp(
            responsePtr, payloadLength, &cc, &reason_code, &resp_offset,
            &resp_length, gpioValues.data(), &gpioValuesSize);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Offset"] = resp_offset;
        result["Length"] = resp_length;
        result["GPIO Values Size (bytes)"] = gpioValuesSize;

        // Parse GPIO values bit by bit
        ordered_json gpioStates = ordered_json::array();
        for (uint32_t byteIndex = 0; byteIndex < gpioValuesSize; ++byteIndex)
        {
            uint8_t byteValue = gpioValues[byteIndex];
            for (uint8_t bitIndex = 0; bitIndex < 8; ++bitIndex)
            {
                uint16_t gpioIndex = resp_offset + (byteIndex * 8) + bitIndex;

                // Only process up to resp_length GPIOs
                if ((byteIndex * 8 + bitIndex) >= resp_length)
                {
                    break;
                }

                bool bitValue = (byteValue >> bitIndex) & 0x01;
                ordered_json gpioEntry;
                gpioEntry["GPIO Index"] = gpioIndex;
                gpioEntry["Value"] = bitValue ? "HIGH" : "LOW";
                gpioEntry["Binary"] = bitValue ? 1 : 0;
                gpioStates.push_back(gpioEntry);
            }
        }
        result["GPIO States"] = gpioStates;

        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint16_t offset;
    uint16_t length;
};

void registerCommand(CLI::App& app)
{
    auto discovery = app.add_subcommand(
        "discovery", "Device capability discovery type command");
    discovery->require_subcommand(1);

    auto ping = discovery->add_subcommand(
        "Ping", "get the status of responder, if alive or not");
    commands.push_back(std::make_unique<Ping>("discovery", "Ping", ping));

    auto getSupportedMessageTypes = discovery->add_subcommand(
        "GetSupportedMessageTypes",
        "get supported nvidia message types by the device");
    commands.push_back(std::make_unique<GetSupportedMessageTypes>(
        "discovery", "GetSupportedMessageTypes", getSupportedMessageTypes));

    auto getSupportedCommandCodes =
        discovery->add_subcommand("GetSupportedCommandCodes",
                                  "get supported command codes by the device");
    commands.push_back(std::make_unique<GetSupportedCommandCodes>(
        "discovery", "GetSupportedCommandCodes", getSupportedCommandCodes));

    auto queryDeviceIdentification = discovery->add_subcommand(
        "QueryDeviceIdentification",
        "query compliant devices for self-identification information");
    commands.push_back(std::make_unique<QueryDeviceIdentification>(
        "discovery", "QueryDeviceIdentification", queryDeviceIdentification));

    auto getHistogramFormat = discovery->add_subcommand(
        "GetHistogramFormat", "get histogram format for histograms");
    commands.push_back(std::make_unique<GetHistogramFormat>(
        "discovery", "GetHistogramFormat", getHistogramFormat));

    auto getHistogramData = discovery->add_subcommand(
        "GetHistogramData", "get histogram data for the histograms");
    commands.push_back(std::make_unique<GetHistogramData>(
        "discovery", "GetHistogramData", getHistogramData));

    auto getDeviceCapabilitiesV2 = discovery->add_subcommand(
        "GetDeviceCapabilitiesV2", "get device capabilities v2");
    commands.push_back(std::make_unique<GetDeviceCapabilitiesV2>(
        "discovery", "GetDeviceCapabilitiesV2", getDeviceCapabilitiesV2));

    auto getGpioState = discovery->add_subcommand(
        "GetGpioState", "get GPIO states for specified offset and length");
    commands.push_back(std::make_unique<GetGpioState>(
        "discovery", "GetGpioState", getGpioState));
}

} // namespace discovery
} // namespace nsmtool
