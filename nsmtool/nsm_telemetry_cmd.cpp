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

#include "nsm_telemetry_cmd.hpp"

#include "base.h"
#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"

#include "cmd_helper.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>

#include <ctime>

namespace nsmtool
{

namespace telemetry
{

namespace
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

} // namespace

class GetPortTelemetryCounter : public CommandInterface
{
  public:
    ~GetPortTelemetryCounter() = default;
    GetPortTelemetryCounter() = delete;
    GetPortTelemetryCounter(const GetPortTelemetryCounter&) = delete;
    GetPortTelemetryCounter(GetPortTelemetryCounter&&) = default;
    GetPortTelemetryCounter& operator=(const GetPortTelemetryCounter&) = delete;
    GetPortTelemetryCounter& operator=(GetPortTelemetryCounter&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetPortTelemetryCounter(const char* type, const char* name,
                                     CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto portTeleOptionGroup = app->add_option_group(
            "Required",
            "Port number for which counter value is to be retrieved.");

        portNumber = 00;
        portTeleOptionGroup->add_option(
            "-p, --portNum", portNumber,
            "retrieve counter values for Port number");
        portTeleOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_port_telemetry_counter_req(instanceId, portNumber,
                                                        request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataLen = 0;
        struct nsm_port_counter_data portTeleData;

        auto rc = decode_get_port_telemetry_counter_resp(
            responsePtr, payloadLength, &cc, &reason_code, &dataLen,
            &portTeleData);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            printPortTeleInfo(dataLen, &portTeleData);
        }
        else
        {
            std::cerr
                << "Response message error: decode_get_port_telemetry_resp fail "
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }
        return;
    }

    void printPortTeleInfo(uint16_t dataLen,
                           const struct nsm_port_counter_data* portData)
    {
        if (portData == NULL)
        {
            std::cerr << "Failed to get port counter information" << std::endl;
            return;
        }

        ordered_json result;
        result["Port Number"] = portNumber;
        result["Data Length"] = dataLen;
        std::string key("Supported Counters");

        ordered_json countersResult;
        if (portData->supported_counter.port_rcv_pkts)
        {
            result[key].push_back(0);
            countersResult["Port Rcv Pkt"] =
                static_cast<uint64_t>(portData->port_rcv_pkts);
        }

        if (portData->supported_counter.port_rcv_data)
        {
            result[key].push_back(1);
            countersResult["Port Rcv Data"] =
                static_cast<uint64_t>(portData->port_rcv_data);
        }

        if (portData->supported_counter.port_multicast_rcv_pkts)
        {
            result[key].push_back(2);
            countersResult["Port Multicast Rcv Pkt"] =
                static_cast<uint64_t>(portData->port_multicast_rcv_pkts);
        }

        if (portData->supported_counter.port_unicast_rcv_pkts)
        {
            result[key].push_back(3);
            countersResult["Port Unicast Rcv Pkt"] =
                static_cast<uint64_t>(portData->port_unicast_rcv_pkts);
        }

        if (portData->supported_counter.port_malformed_pkts)
        {
            result[key].push_back(4);
            countersResult["Port Malformed Pkt"] =
                static_cast<uint64_t>(portData->port_malformed_pkts);
        }

        if (portData->supported_counter.vl15_dropped)
        {
            result[key].push_back(5);
            countersResult["Vl15 Dropped"] =
                static_cast<uint64_t>(portData->vl15_dropped);
        }

        if (portData->supported_counter.port_rcv_errors)
        {
            result[key].push_back(6);
            countersResult["Port Rcv Error"] =
                static_cast<uint64_t>(portData->port_rcv_errors);
        }

        if (portData->supported_counter.port_xmit_pkts)
        {
            result[key].push_back(7);
            countersResult["Port Tx Pkt"] =
                static_cast<uint64_t>(portData->port_xmit_pkts);
        }

        if (portData->supported_counter.port_xmit_pkts_vl15)
        {
            result[key].push_back(8);
            countersResult["Port Tx Pkt Vl15"] =
                static_cast<uint64_t>(portData->port_xmit_pkts_vl15);
        }

        if (portData->supported_counter.port_xmit_data)
        {
            result[key].push_back(9);
            countersResult["Port Tx Data"] =
                static_cast<uint64_t>(portData->port_xmit_data);
        }

        if (portData->supported_counter.port_xmit_data_vl15)
        {
            result[key].push_back(10);
            countersResult["Port Tx Data Vl15"] =
                static_cast<uint64_t>(portData->port_xmit_data_vl15);
        }

        if (portData->supported_counter.port_unicast_xmit_pkts)
        {
            result[key].push_back(11);
            countersResult["Port Unicast Tx Pkt"] =
                static_cast<uint64_t>(portData->port_unicast_xmit_pkts);
        }

        if (portData->supported_counter.port_multicast_xmit_pkts)
        {
            result[key].push_back(12);
            countersResult["Port Multicast Tx Pkt"] =
                static_cast<uint64_t>(portData->port_multicast_xmit_pkts);
        }

        if (portData->supported_counter.port_bcast_xmit_pkts)
        {
            result[key].push_back(13);
            countersResult["Port Broadcast Tx Pkt"] =
                static_cast<uint64_t>(portData->port_bcast_xmit_pkts);
        }

        if (portData->supported_counter.port_xmit_discard)
        {
            result[key].push_back(14);
            countersResult["Port Tx Discard"] =
                static_cast<uint64_t>(portData->port_xmit_discard);
        }

        if (portData->supported_counter.port_neighbor_mtu_discards)
        {
            result[key].push_back(15);
            countersResult["Port Neighbour MTU Discard"] =
                static_cast<uint64_t>(portData->port_neighbor_mtu_discards);
        }

        if (portData->supported_counter.port_rcv_ibg2_pkts)
        {
            result[key].push_back(16);
            countersResult["Port Rcv IBG2 Pkt"] =
                static_cast<uint64_t>(portData->port_rcv_ibg2_pkts);
        }

        if (portData->supported_counter.port_xmit_ibg2_pkts)
        {
            result[key].push_back(17);
            countersResult["Port Tx IBG2 Pkt"] =
                static_cast<uint64_t>(portData->port_xmit_ibg2_pkts);
        }

        if (portData->supported_counter.symbol_error)
        {
            result[key].push_back(18);
            countersResult["Symbol Error"] =
                static_cast<uint64_t>(portData->symbol_error);
        }

        if (portData->supported_counter.link_error_recovery_counter)
        {
            result[key].push_back(19);
            countersResult["Link Error Recovery Counter"] =
                static_cast<uint64_t>(portData->link_error_recovery_counter);
        }

        if (portData->supported_counter.link_downed_counter)
        {
            result[key].push_back(20);
            countersResult["Link Downed Counter"] =
                static_cast<uint64_t>(portData->link_downed_counter);
        }

        if (portData->supported_counter.port_rcv_remote_physical_errors)
        {
            result[key].push_back(21);
            countersResult["Port Rcv Remote Physical Error"] =
                static_cast<uint64_t>(
                    portData->port_rcv_remote_physical_errors);
        }

        if (portData->supported_counter.port_rcv_switch_relay_errors)
        {
            result[key].push_back(22);
            countersResult["Port Rcv Switch Relay Error"] =
                static_cast<uint64_t>(portData->port_rcv_switch_relay_errors);
        }

        if (portData->supported_counter.QP1_dropped)
        {
            result[key].push_back(23);
            countersResult["QP1 Dropped"] =
                static_cast<uint64_t>(portData->QP1_dropped);
        }

        if (portData->supported_counter.xmit_wait)
        {
            result[key].push_back(24);
            countersResult["Tx Wait"] =
                static_cast<uint64_t>(portData->xmit_wait);
        }
        result["Port Counter Information"] = countersResult;

        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t portNumber;
};

class QueryPortCharacteristics : public CommandInterface
{
  public:
    ~QueryPortCharacteristics() = default;
    QueryPortCharacteristics() = delete;
    QueryPortCharacteristics(const QueryPortCharacteristics&) = delete;
    QueryPortCharacteristics(QueryPortCharacteristics&&) = default;
    QueryPortCharacteristics&
        operator=(const QueryPortCharacteristics&) = delete;
    QueryPortCharacteristics& operator=(QueryPortCharacteristics&&) = default;

    using CommandInterface::CommandInterface;

    explicit QueryPortCharacteristics(const char* type, const char* name,
                                      CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto portTeleOptionGroup = app->add_option_group(
            "Required",
            "Port number for which counter value is to be retrieved.");

        portNumber = 00;
        portTeleOptionGroup->add_option(
            "-p, --portNum", portNumber,
            "retrieve counter values for Port number");
        portTeleOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_query_port_characteristics_req(instanceId, portNumber,
                                                        request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint16_t dataLen = 0;
        struct nsm_port_characteristics_data portCharData;

        auto rc = decode_query_port_characteristics_resp(
            responsePtr, payloadLength, &cc, &reasonCode, &dataLen,
            &portCharData);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            ordered_json result;
            result["Port Number"] = portNumber;
            result["Data Length"] = dataLen;
            result["Status"] = static_cast<uint32_t>(portCharData.status);
            result["NV Port Line Rate Mbps"] =
                static_cast<uint32_t>(portCharData.nv_port_line_rate_mbps);
            result["NV Port Data Rate Kbps"] =
                static_cast<uint32_t>(portCharData.nv_port_data_rate_kbps);
            result["Lane Info Status"] =
                static_cast<uint32_t>(portCharData.status_lane_info);
            nsmtool::helper::DisplayInJson(result);
        }
        else
        {
            std::cerr
                << "Response message error: decode_query_port_characteristics_resp fail "
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reasonCode << "\n";
            return;
        }
        return;
    }

  private:
    uint8_t portNumber;
};

class QueryPortStatus : public CommandInterface
{
  public:
    ~QueryPortStatus() = default;
    QueryPortStatus() = delete;
    QueryPortStatus(const QueryPortStatus&) = delete;
    QueryPortStatus(QueryPortStatus&&) = default;
    QueryPortStatus& operator=(const QueryPortStatus&) = delete;
    QueryPortStatus& operator=(QueryPortStatus&&) = default;

    using CommandInterface::CommandInterface;

    explicit QueryPortStatus(const char* type, const char* name,
                             CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto portStatusOptionGroup = app->add_option_group(
            "Required",
            "Port number for which counter value is to be retrieved.");

        portNumber = 00;
        portStatusOptionGroup->add_option(
            "-p, --portNum", portNumber,
            "retrieve counter values for Port number");
        portStatusOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_query_port_status_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_query_port_status_req(instanceId, portNumber, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint16_t dataLen = 0;
        uint8_t portState = NSM_PORTSTATE_UP;
        uint8_t portStatus = NSM_PORTSTATUS_ENABLED;

        auto rc = decode_query_port_status_resp(responsePtr, payloadLength, &cc,
                                                &reasonCode, &dataLen,
                                                &portState, &portStatus);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            ordered_json result;
            result["Port Number"] = portNumber;
            result["Data Length"] = dataLen;
            result["Port State"] = portState;
            result["Port Status"] = portStatus;
            nsmtool::helper::DisplayInJson(result);
        }
        else
        {
            std::cerr
                << "Response message error: decode_query_port_status_resp fail "
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reasonCode << "\n";
            return;
        }
        return;
    }

  private:
    uint8_t portNumber;
};

class GetInventoryInformation : public CommandInterface
{
  public:
    ~GetInventoryInformation() = default;
    GetInventoryInformation() = delete;
    GetInventoryInformation(const GetInventoryInformation&) = delete;
    GetInventoryInformation(GetInventoryInformation&&) = default;
    GetInventoryInformation& operator=(const GetInventoryInformation&) = delete;
    GetInventoryInformation& operator=(GetInventoryInformation&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetInventoryInformation(const char* type, const char* name,
                                     CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto getInventoryInformationOptionGroup = app->add_option_group(
            "Required",
            "Property Id for which Inventory Information is to be retrieved.");

        propertyId = 0;
        getInventoryInformationOptionGroup->add_option(
            "-p, --propertyId", propertyId,
            "retrieve inventory information for propertyId");
        getInventoryInformationOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_inventory_information_req(instanceId, propertyId,
                                                       request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataSize = 0;
        std::vector<uint8_t> data(65535, 0);

        auto rc = decode_get_inventory_information_resp(
            responsePtr, payloadLength, &cc, &reason_code, &dataSize,
            data.data());
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        printInventoryInfo(propertyId, dataSize, data);
    }

    void printInventoryInfo(uint8_t& propertyIdentifier,
                            const uint16_t dataSize,
                            const std::vector<uint8_t>& data)
    {
        ordered_json result;
        ordered_json propRecordResult;
        propRecordResult["Property ID"] = propertyIdentifier;
        propRecordResult["Data Length"] = static_cast<uint16_t>(dataSize);
        std::stringstream iss;
        std::string firmwareVersion;

        // todo: display aggregate data
        switch (propertyIdentifier)
        {
            case MAXIMUM_MEMORY_CAPACITY:
            case PRODUCT_LENGTH:
            case PRODUCT_WIDTH:
            case PRODUCT_HEIGHT:
            case RATED_DEVICE_POWER_LIMIT:
            case MINIMUM_DEVICE_POWER_LIMIT:
            case MAXIMUM_DEVICE_POWER_LIMIT:
            case MINIMUM_MODULE_POWER_LIMIT:
            case MAXMUM_MODULE_POWER_LIMIT:
            case RATED_MODULE_POWER_LIMIT:
            case DEFAULT_BOOST_CLOCKS:
            case DEFAULT_BASE_CLOCKS:
                propRecordResult["Data"] = le32toh(*(uint32_t*)data.data());
                break;
            case BOARD_PART_NUMBER:
            case SERIAL_NUMBER:
            case MARKETING_NAME:
            case DEVICE_PART_NUMBER:
            case FRU_PART_NUMBER:
            case MEMORY_VENDOR:
            case MEMORY_PART_NUMBER:
            case BUILD_DATE:
            case FIRMWARE_VERSION:
            case INFO_ROM_VERSION:
                propRecordResult["Data"] =
                    std::string((char*)data.data(), dataSize);
                break;
            case DEVICE_GUID:
                propRecordResult["Data"] = utils::convertUUIDToString(data);
                break;
            case PCIERETIMER_0_EEPROM_VERSION:
            case PCIERETIMER_1_EEPROM_VERSION:
            case PCIERETIMER_2_EEPROM_VERSION:
            case PCIERETIMER_3_EEPROM_VERSION:
            case PCIERETIMER_4_EEPROM_VERSION:
            case PCIERETIMER_5_EEPROM_VERSION:
            case PCIERETIMER_6_EEPROM_VERSION:
            case PCIERETIMER_7_EEPROM_VERSION:
                iss << int(data[0]);        // Major version number
                iss << '.' << int(data[2]); // Minor version number
                iss << '.' << int((data[4] << 8) | data[6]); // Patch number

                firmwareVersion = iss.str();
                propRecordResult["Data"] = firmwareVersion;
                break;
            default:
                std::cerr
                    << "Incorrect data type recieved in get inventory information"
                    << std::endl;
                return;
        }

        result["Inventory Information"] = propRecordResult;
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t propertyId;
};

class GetGpuPresenceAndPowerStatus : public CommandInterface
{
  private:
    uint8_t gpuInstanceId = 0;

  public:
    ~GetGpuPresenceAndPowerStatus() = default;
    GetGpuPresenceAndPowerStatus() = delete;
    GetGpuPresenceAndPowerStatus(const GetGpuPresenceAndPowerStatus&) = delete;
    GetGpuPresenceAndPowerStatus(GetGpuPresenceAndPowerStatus&&) = default;
    GetGpuPresenceAndPowerStatus&
        operator=(const GetGpuPresenceAndPowerStatus&) = delete;
    GetGpuPresenceAndPowerStatus&
        operator=(GetGpuPresenceAndPowerStatus&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetGpuPresenceAndPowerStatus(const char* type, const char* name,
                                          CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto optionGroup = app->add_option_group(
            "Required",
            "GPU Instance Id for which presence and power status is to be retrieved.");

        gpuInstanceId = 0;
        optionGroup->add_option(
            "-g, --gpuInstanceId", gpuInstanceId,
            "retrieve presence and power status for gpuInstanceId");
        optionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        Request requestMsg(sizeof(nsm_msg_hdr) +
                           sizeof(nsm_get_gpu_presence_and_power_status_req));
        auto request = reinterpret_cast<struct nsm_msg*>(requestMsg.data());
        auto rc =
            encode_get_gpu_presence_and_power_status_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint8_t gpus_presence = 0;
        uint8_t gpus_power = 0;

        auto rc = decode_get_gpu_presence_and_power_status_resp(
            responsePtr, payloadLength, &cc, &reasonCode, &gpus_presence,
            &gpus_power);

        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << static_cast<int>(reasonCode)
                      << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Power Status"] = ((gpus_power >> (gpuInstanceId)) & 0x1) != 0;
        result["Presence"] = ((gpus_presence >> (gpuInstanceId)) & 0x1) != 0;
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetPowerSupplyStatus : public CommandInterface
{
  private:
    uint8_t gpuInstanceId = 0;

  public:
    ~GetPowerSupplyStatus() = default;
    GetPowerSupplyStatus() = delete;
    GetPowerSupplyStatus(const GetPowerSupplyStatus&) = delete;
    GetPowerSupplyStatus(GetPowerSupplyStatus&&) = default;
    GetPowerSupplyStatus& operator=(const GetPowerSupplyStatus&) = delete;
    GetPowerSupplyStatus& operator=(GetPowerSupplyStatus&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetPowerSupplyStatus(const char* type, const char* name,
                                  CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto optionGroup = app->add_option_group(
            "Required",
            "GPU Instance Id for which power supply status is to be retrieved.");

        gpuInstanceId = 0;
        optionGroup->add_option(
            "-g, --gpuInstanceId", gpuInstanceId,
            "retrieve power supply status for gpuInstanceId");
        optionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        Request requestMsg(sizeof(nsm_msg_hdr) +
                           sizeof(nsm_get_power_supply_status_req));
        auto request = reinterpret_cast<struct nsm_msg*>(requestMsg.data());
        auto rc = encode_get_power_supply_status_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint8_t status = 0;

        auto rc = decode_get_power_supply_status_resp(
            responsePtr, payloadLength, &cc, &reasonCode, &status);

        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << static_cast<int>(reasonCode)
                      << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Power Supply"] =
            ((status >> gpuInstanceId) & 0x01) != 0 ? "On" : "Off";
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetDriverInfo : public CommandInterface
{
  public:
    ~GetDriverInfo() = default;
    GetDriverInfo() = delete;
    GetDriverInfo(const GetDriverInfo&) = delete;
    GetDriverInfo(GetDriverInfo&&) = default;
    GetDriverInfo& operator=(const GetDriverInfo&) = delete;
    GetDriverInfo& operator=(GetDriverInfo&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_driver_info_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        enum8 driverState = 0;
        char driverVersion[MAX_VERSION_STRING_SIZE] = {0};

        auto rc = decode_get_driver_info_resp(responsePtr, payloadLength, &cc,
                                              &reasonCode, &driverState,
                                              driverVersion);

        std::string version(driverVersion);

        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << static_cast<int>(reasonCode) << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) + sizeof(driverState) +
                          version.length());
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Driver State"] = static_cast<int>(driverState);
        result["Driver Version"] = version;
        nsmtool::helper::DisplayInJson(result);
    }
};

class AggregateResponseParser
{
  private:
    virtual int handleSampleData(uint8_t tag, const uint8_t* data,
                                 size_t data_len,
                                 ordered_json& sample_json) = 0;

  public:
    virtual ~AggregateResponseParser() = default;

    void parseAggregateResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        uint8_t cc{};
        uint16_t telemetry_count{};
        size_t consumed_len{};
        size_t msg_len = payloadLength + sizeof(nsm_msg_hdr);
        auto response_data = reinterpret_cast<const uint8_t*>(responsePtr);

        auto rc = decode_aggregate_resp(responsePtr, msg_len, &consumed_len,
                                        &cc, &telemetry_count);

        if (rc != NSM_SW_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc << "\n";

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sample Count"] = telemetry_count;

        uint64_t timestamp{};
        constexpr size_t time_size{100};
        char time[time_size];
        bool has_timestamp{false};

        std::vector<ordered_json> samples;
        while (telemetry_count--)
        {
            uint8_t tag{};
            bool valid;
            const uint8_t* data;
            size_t data_len;

            msg_len -= consumed_len;

            response_data += consumed_len;

            auto sample = reinterpret_cast<const nsm_aggregate_resp_sample*>(
                response_data);

            rc = decode_aggregate_resp_sample(sample, msg_len, &consumed_len,
                                              &tag, &valid, &data, &data_len);

            if (rc != NSM_SW_SUCCESS || !valid)
            {
                std::cerr
                    << "Response message error while parsing sample header: "
                    << "tag=" << static_cast<int>(tag) << ", rc=" << rc << "\n";

                continue;
            }

            if (tag == 0xFF)
            {
                if (data_len != 8)
                {
                    std::cerr
                        << "Response message error while parsing timestamp sample : "
                        << "tag=" << tag << ", rc=" << rc << "\n";

                    continue;
                }

                rc =
                    decode_aggregate_timestamp_data(data, data_len, &timestamp);
                if (rc != NSM_SW_SUCCESS)
                {
                    std::cerr
                        << "Response message error while parsing timestamp sample data : "
                        << "tag=" << tag << ", rc=" << rc << "\n";

                    continue;
                }

                has_timestamp = true;
                auto stime = static_cast<std::time_t>(timestamp);
                std::strftime(time, time_size, "%F %T %Z",
                              std::localtime(&stime));
            }
            else if (tag < 0xF0)
            {
                ordered_json sample_json;

                rc = handleSampleData(tag, data, data_len, sample_json);

                if (rc != NSM_SW_SUCCESS)
                {
                    std::cerr
                        << "Response message error while parsing sample data: "
                        << "tag=" << tag << ", rc=" << rc << "\n";

                    continue;
                }

                if (has_timestamp)
                {
                    sample_json["Timestamp"] = time;
                }

                samples.push_back(std::move(sample_json));
            }
        }

        result["Samples"] = std::move(samples);
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetTemperatureReading : public CommandInterface
{
  public:
    ~GetTemperatureReading() = default;
    GetTemperatureReading() = delete;
    GetTemperatureReading(const GetTemperatureReading&) = delete;
    GetTemperatureReading(GetTemperatureReading&&) = default;
    GetTemperatureReading& operator=(const GetTemperatureReading&) = delete;
    GetTemperatureReading& operator=(GetTemperatureReading&&) = delete;

    using CommandInterface::CommandInterface;

    explicit GetTemperatureReading(const char* type, const char* name,
                                   CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-s, --sensorId", sensorId, "sensor Id")->required();
    }

  private:
    void parseRegularResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        double temperatureReading = 0;

        auto rc = decode_get_temperature_reading_resp(
            responsePtr, payloadLength, &cc, &reason_code, &temperatureReading);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_temperature_reading_resp));
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Temperature Reading"] = temperatureReading;
        nsmtool::helper::DisplayInJson(result);
    }

    class GetTemperatureAggregateResponseParser : public AggregateResponseParser
    {
      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            double reading{};
            auto rc = decode_aggregate_temperature_reading_data(data, data_len,
                                                                &reading);

            if (rc == NSM_SUCCESS)
            {
                sample_json["Sensor Id"] = tag;
                sample_json["Temperature Reading"] = reading;
            }

            return rc;
        }
    };

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc =
            encode_get_temperature_reading_req(instanceId, sensorId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (sensorId == AggregateSensorId)
        {
            GetTemperatureAggregateResponseParser{}.parseAggregateResponse(
                responsePtr, payloadLength);
        }
        else
        {
            parseRegularResponse(responsePtr, payloadLength);
        }
    }

  private:
    uint8_t sensorId;
    static constexpr uint8_t AggregateSensorId{255};
};

class GetCurrentPowerDraw : public CommandInterface
{
  public:
    GetCurrentPowerDraw() = delete;
    GetCurrentPowerDraw(const GetCurrentPowerDraw&) = delete;
    GetCurrentPowerDraw(GetCurrentPowerDraw&&) = default;
    GetCurrentPowerDraw& operator=(const GetCurrentPowerDraw&) = delete;
    GetCurrentPowerDraw& operator=(GetCurrentPowerDraw&&) = delete;

    explicit GetCurrentPowerDraw(const char* type, const char* name,
                                 CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-s, --sensorId", sensorId, "sensor Id")->required();

        app->add_option("-a, --averagingInterval", averagingInterval,
                        "averaging interval of current power draw reading")
            ->required();
    }

  private:
    void parseRegularResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        const size_t msg_len = payloadLength + sizeof(nsm_msg_hdr);
        uint8_t cc;
        uint16_t reason_code;
        uint32_t reading;

        auto rc = decode_get_current_power_draw_resp(responsePtr, msg_len, &cc,
                                                     &reason_code, &reading);
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(nsm_get_current_power_draw_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Averaging Interval"] = averagingInterval;
        result["Current Power Draw"] = reading;

        nsmtool::helper::DisplayInJson(result);
    }

    class GetCurrentPowerDrawAggregateResponseParser :
        public AggregateResponseParser
    {
      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            uint32_t reading;
            auto rc = decode_aggregate_get_current_power_draw_reading(
                data, data_len, &reading);
            if (rc == NSM_SUCCESS)
            {
                sample_json["Sensor Id"] = tag;
                sample_json["Current Power Draw"] = reading;
            }

            return rc;
        }
    };

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_current_power_draw_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_current_power_draw_req(instanceId, sensorId,
                                                    averagingInterval, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (sensorId == AggregateSensorId)
        {
            GetCurrentPowerDrawAggregateResponseParser{}.parseAggregateResponse(
                responsePtr, payloadLength);
        }
        else
        {
            parseRegularResponse(responsePtr, payloadLength);
        }
    }

  private:
    uint8_t sensorId;
    static constexpr uint8_t AggregateSensorId{255};
    uint8_t averagingInterval;
};

class GetCurrentEnergyCount : public CommandInterface
{
  public:
    GetCurrentEnergyCount() = delete;
    GetCurrentEnergyCount(const GetCurrentEnergyCount&) = delete;
    GetCurrentEnergyCount(GetCurrentEnergyCount&&) = default;
    GetCurrentEnergyCount& operator=(const GetCurrentEnergyCount&) = delete;
    GetCurrentEnergyCount& operator=(GetCurrentEnergyCount&&) = delete;

    explicit GetCurrentEnergyCount(const char* type, const char* name,
                                   CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-s, --sensorId", sensorId, "sensor Id")->required();
    }

  private:
    void parseRegularResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        const size_t msg_len = payloadLength + sizeof(nsm_msg_hdr);
        uint8_t cc;
        uint16_t reason_code;
        uint64_t reading;

        auto rc = decode_get_current_energy_count_resp(
            responsePtr, msg_len, &cc, &reason_code, &reading);
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_current_energy_count_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Current Energy Count"] = reading;

        nsmtool::helper::DisplayInJson(result);
    }

    class GetCurrentEnergyCountAggregateResponseParser :
        public AggregateResponseParser
    {
      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            uint64_t reading{};
            auto rc =
                decode_aggregate_energy_count_data(data, data_len, &reading);

            if (rc == NSM_SUCCESS)
            {
                sample_json["Sensor Id"] = tag;
                sample_json["Current Energy Count"] = reading;
            }

            return rc;
        }
    };

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc =
            encode_get_current_energy_count_req(instanceId, sensorId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (sensorId == AggregateSensorId)
        {
            GetCurrentEnergyCountAggregateResponseParser{}
                .parseAggregateResponse(responsePtr, payloadLength);
        }
        else
        {
            parseRegularResponse(responsePtr, payloadLength);
        }
    }

  private:
    uint8_t sensorId;
    static constexpr uint8_t AggregateSensorId{255};
};

class GetVoltage : public CommandInterface
{
  public:
    GetVoltage() = delete;
    GetVoltage(const GetVoltage&) = delete;
    GetVoltage(GetVoltage&&) = default;
    GetVoltage& operator=(const GetVoltage&) = delete;
    GetVoltage& operator=(GetVoltage&&) = delete;

    explicit GetVoltage(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-s, --sensorId", sensorId, "sensor Id")->required();
    }

  private:
    void parseRegularResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        const size_t msg_len = payloadLength + sizeof(nsm_msg_hdr);
        uint8_t cc;
        uint16_t reason_code;
        uint32_t reading;

        auto rc = decode_get_voltage_resp(responsePtr, msg_len, &cc,
                                          &reason_code, &reading);
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(nsm_get_voltage_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Voltage"] = reading;

        nsmtool::helper::DisplayInJson(result);
    }

    class GetVoltageAggregateResponseParser : public AggregateResponseParser
    {
      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            uint32_t reading{};
            auto rc = decode_aggregate_voltage_data(data, data_len, &reading);

            if (rc == NSM_SUCCESS)
            {
                sample_json["Sensor Id"] = tag;
                sample_json["Voltage"] = reading;
            }

            return rc;
        }
    };

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_voltage_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_voltage_req(instanceId, sensorId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (sensorId == AggregateSensorId)
        {
            GetVoltageAggregateResponseParser{}.parseAggregateResponse(
                responsePtr, payloadLength);
        }
        else
        {
            parseRegularResponse(responsePtr, payloadLength);
        }
    }

  private:
    uint8_t sensorId;
    static constexpr uint8_t AggregateSensorId{255};
};

class GetAltitudePressure : public CommandInterface
{
  public:
    GetAltitudePressure() = delete;
    GetAltitudePressure(const GetAltitudePressure&) = delete;
    GetAltitudePressure(GetAltitudePressure&&) = default;
    GetAltitudePressure& operator=(const GetAltitudePressure&) = delete;
    GetAltitudePressure& operator=(GetAltitudePressure&&) = delete;

    explicit GetAltitudePressure(const char* type, const char* name,
                                 CLI::App* app) :
        CommandInterface(type, name, app)
    {}

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_altitude_pressure_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        const size_t msg_len = payloadLength + sizeof(nsm_msg_hdr);
        uint8_t cc;
        uint16_t reason_code;
        uint32_t reading;

        auto rc = decode_get_altitude_pressure_resp(responsePtr, msg_len, &cc,
                                                    &reason_code, &reading);
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(nsm_get_altitude_pressure_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Altitude Pressure"] = reading;

        nsmtool::helper::DisplayInJson(result);
    }
};

class GetMigMode : public CommandInterface
{
  public:
    ~GetMigMode() = default;
    GetMigMode() = delete;
    GetMigMode(const GetMigMode&) = delete;
    GetMigMode(GetMigMode&&) = default;
    GetMigMode& operator=(const GetMigMode&) = delete;
    GetMigMode& operator=(GetMigMode&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_MIG_mode_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        bitfield8_t flags;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_MIG_mode_resp(responsePtr, payloadLength, &cc,
                                           &data_size, &reason_code, &flags);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_MIG_mode_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        if (flags.bits.bit0)
        {
            result["MigModeEnabled"] = true;
        }
        else
        {
            result["MigModeEnabled"] = false;
        }
        nsmtool::helper::DisplayInJson(result);
    }
};

class SetMigMode : public CommandInterface
{
  public:
    ~SetMigMode() = default;
    SetMigMode() = delete;
    SetMigMode(const SetMigMode&) = delete;
    SetMigMode(SetMigMode&&) = default;
    SetMigMode& operator=(const SetMigMode&) = delete;
    SetMigMode& operator=(SetMigMode&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetMigMode(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto requestedMode =
            app->add_option_group("Required", "Requested Mig Mode can be 0/1.");

        requestedMode->add_option("-r, --mode", requested_mode,
                                  "retrieve requested mig mode");

        requestedMode->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_set_MIG_mode_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_MIG_mode_req(instanceId, requested_mode, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_set_MIG_mode_resp(responsePtr, payloadLength, &cc,
                                           &data_size, &reason_code);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_common_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        nsmtool::helper::DisplayInJson(result);
    }

    uint8_t requested_mode;
};

class GetEccMode : public CommandInterface
{
  public:
    ~GetEccMode() = default;
    GetEccMode() = delete;
    GetEccMode(const GetEccMode&) = delete;
    GetEccMode(GetEccMode&&) = default;
    GetEccMode& operator=(const GetEccMode&) = delete;
    GetEccMode& operator=(GetEccMode&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_ECC_mode_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        bitfield8_t flags;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_ECC_mode_resp(responsePtr, payloadLength, &cc,
                                           &data_size, &reason_code, &flags);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_ECC_mode_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        if (flags.bits.bit0)
        {
            result["ECCModeEnabled"] = true;
        }
        else
        {
            result["ECCModeEnabled"] = false;
        }
        if (flags.bits.bit1)
        {
            result["PendingECCState"] = true;
        }
        else
        {
            result["PendingECCState"] = false;
        }
        nsmtool::helper::DisplayInJson(result);
    }
};

class SetEccMode : public CommandInterface
{
  public:
    ~SetEccMode() = default;
    SetEccMode() = delete;
    SetEccMode(const SetEccMode&) = delete;
    SetEccMode(SetEccMode&&) = default;
    SetEccMode& operator=(const SetEccMode&) = delete;
    SetEccMode& operator=(SetEccMode&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetEccMode(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto requestedMode =
            app->add_option_group("Required", "Requested ECC Mode.");

        requestedMode->add_option("-r, --mode", requested_mode,
                                  "requested ECC mode");

        requestedMode->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_set_ECC_mode_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_ECC_mode_req(instanceId, requested_mode, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_set_ECC_mode_resp(responsePtr, payloadLength, &cc,
                                           &data_size, &reason_code);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_common_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        nsmtool::helper::DisplayInJson(result);
    }

    uint8_t requested_mode;
};

class GetEccErrorCounts : public CommandInterface
{
  public:
    ~GetEccErrorCounts() = default;
    GetEccErrorCounts() = delete;
    GetEccErrorCounts(const GetEccErrorCounts&) = delete;
    GetEccErrorCounts(GetEccErrorCounts&&) = default;
    GetEccErrorCounts& operator=(const GetEccErrorCounts&) = delete;
    GetEccErrorCounts& operator=(GetEccErrorCounts&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_ECC_error_counts_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        struct nsm_ECC_error_counts errorCounts;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_ECC_error_counts_resp(responsePtr, payloadLength,
                                                   &cc, &data_size,
                                                   &reason_code, &errorCounts);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_ECC_error_counts_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["ueCount"] = errorCounts.sram_uncorrected_parity +
                            errorCounts.sram_uncorrected_secded;
        result["ecCount"] = (int)errorCounts.sram_corrected;
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetEDPpScalingFactors : public CommandInterface
{
  public:
    ~GetEDPpScalingFactors() = default;
    GetEDPpScalingFactors() = delete;
    GetEDPpScalingFactors(const GetEDPpScalingFactors&) = delete;
    GetEDPpScalingFactors(GetEDPpScalingFactors&&) = default;
    GetEDPpScalingFactors& operator=(const GetEDPpScalingFactors&) = delete;
    GetEDPpScalingFactors& operator=(GetEDPpScalingFactors&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_programmable_EDPp_scaling_factor_req(instanceId,
                                                                  request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        struct nsm_EDPp_scaling_factors scaling_factors;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_get_programmable_EDPp_scaling_factor_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code,
            &scaling_factors);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr
                << "Response message error: " << "rc=" << rc
                << ", cc=" << (int)cc << ", reasonCode=" << (int)reason_code
                << "\n"
                << payloadLength << "...."
                << (sizeof(struct nsm_msg_hdr) +
                    sizeof(
                        struct nsm_get_programmable_EDPp_scaling_factor_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["AllowableMax"] = scaling_factors.maximum_scaling_factor;
        result["AllowableMin"] = scaling_factors.minimum_scaling_factor;

        nsmtool::helper::DisplayInJson(result);
    }
};

class QueryScalarGroupTelemetry : public CommandInterface
{
  public:
    ~QueryScalarGroupTelemetry() = default;
    QueryScalarGroupTelemetry() = delete;
    QueryScalarGroupTelemetry(const QueryScalarGroupTelemetry&) = delete;
    QueryScalarGroupTelemetry(QueryScalarGroupTelemetry&&) = default;
    QueryScalarGroupTelemetry&
        operator=(const QueryScalarGroupTelemetry&) = delete;
    QueryScalarGroupTelemetry& operator=(QueryScalarGroupTelemetry&&) = default;

    using CommandInterface::CommandInterface;

    explicit QueryScalarGroupTelemetry(const char* type, const char* name,
                                       CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto scalarTelemetryOptionGroup = app->add_option_group(
            "Required", "Group Id for which data source is to be retrieved.");

        groupId = 9;
        scalarTelemetryOptionGroup->add_option("-d, --deviceId", deviceId,
                                               "retrieve deviceId");
        scalarTelemetryOptionGroup->add_option(
            "-g, --groupId", groupId, "retrieve data source for groupId");
        scalarTelemetryOptionGroup->require_option(2);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_query_scalar_group_telemetry_v1_req(
            instanceId, deviceId, groupId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        switch (groupId)
        {
            case 0:
            {
                struct nsm_query_scalar_group_telemetry_group_0 data;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_query_scalar_group_telemetry_v1_group0_resp(
                    responsePtr, payloadLength, &cc, &data_size, &reason_code,
                    &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: " << "rc=" << rc
                        << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(struct nsm_msg_hdr) +
                            sizeof(
                                struct
                                nsm_query_scalar_group_telemetry_v1_group_2_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["PciVendorId"] = (int)data.pci_vendor_id;
                result["PciDeviceId"] = (int)data.pci_device_id;
                result["PciSubsystemVendorId"] =
                    (int)data.pci_subsystem_vendor_id;
                result["PciSubsystemDeviceId"] =
                    (int)data.pci_subsystem_device_id;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case 1:
            {
                struct nsm_query_scalar_group_telemetry_group_1 data;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
                    responsePtr, payloadLength, &cc, &data_size, &reason_code,
                    &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: " << "rc=" << rc
                        << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(struct nsm_msg_hdr) +
                            sizeof(
                                struct
                                nsm_query_scalar_group_telemetry_v1_group_2_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["NegotiatedLinkSpeed"] = (int)data.negotiated_link_speed;
                result["NegotiatedLinkWidth"] = (int)data.negotiated_link_width;
                result["maxLinkSpeed"] = (int)data.max_link_speed;
                result["maxLinkWidth"] = (int)data.max_link_width;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case 2:
            {
                struct nsm_query_scalar_group_telemetry_group_2 data;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_query_scalar_group_telemetry_v1_group2_resp(
                    responsePtr, payloadLength, &cc, &data_size, &reason_code,
                    &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: " << "rc=" << rc
                        << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(struct nsm_msg_hdr) +
                            sizeof(
                                struct
                                nsm_query_scalar_group_telemetry_v1_group_2_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["nonfeCount"] = (int)data.non_fatal_errors;
                result["feCount"] = (int)data.fatal_errors;
                result["ceCount"] = (int)data.correctable_errors;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case 3:
            {
                struct nsm_query_scalar_group_telemetry_group_3 data;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_query_scalar_group_telemetry_v1_group3_resp(
                    responsePtr, payloadLength, &cc, &data_size, &reason_code,
                    &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: " << "rc=" << rc
                        << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(struct nsm_msg_hdr) +
                            sizeof(
                                struct
                                nsm_query_scalar_group_telemetry_v1_group_3_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["l0ToRecoveryCount"] = (int)data.L0ToRecoveryCount;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case 4:
            {
                struct nsm_query_scalar_group_telemetry_group_4 data;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_query_scalar_group_telemetry_v1_group4_resp(
                    responsePtr, payloadLength, &cc, &data_size, &reason_code,
                    &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: " << "rc=" << rc
                        << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(struct nsm_msg_hdr) +
                            sizeof(
                                struct
                                nsm_query_scalar_group_telemetry_v1_group_4_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["replayCount"] = (int)data.replay_cnt;
                result["replayRolloverCount"] = (int)data.replay_rollover_cnt;
                result["nakSentCount"] = (int)data.NAK_sent_cnt;
                result["nakRecievedCount"] = (int)data.NAK_recv_cnt;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case 5:
            {
                struct nsm_query_scalar_group_telemetry_group_5 data;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_query_scalar_group_telemetry_v1_group5_resp(
                    responsePtr, payloadLength, &cc, &data_size, &reason_code,
                    &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: " << "rc=" << rc
                        << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(struct nsm_msg_hdr) +
                            sizeof(
                                struct
                                nsm_query_scalar_group_telemetry_v1_group_5_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["PCIeTXBytes"] = (int)data.PCIeTXBytes;
                result["PCIeRXBytes"] = (int)data.PCIeRXBytes;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case 6:
            {
                struct nsm_query_scalar_group_telemetry_group_6 data;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_query_scalar_group_telemetry_v1_group6_resp(
                    responsePtr, payloadLength, &cc, &data_size, &reason_code,
                    &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: " << "rc=" << rc
                        << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(struct nsm_msg_hdr) +
                            sizeof(
                                struct
                                nsm_query_scalar_group_telemetry_v1_group_5_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["InvalidFlitCounter"] = (int)data.invalid_flit_counter;
                result["LTSSMState"] = (int)data.ltssm_state;
                nsmtool::helper::DisplayInJson(result);
                break;
            }
            default:
            {
                std::cerr << "Invalid Group Id \n";
                break;
            }
        }
    }

  private:
    uint8_t deviceId;
    uint8_t groupId;
};

class GetClockLimit : public CommandInterface
{
  public:
    ~GetClockLimit() = default;
    GetClockLimit() = delete;
    GetClockLimit(const GetClockLimit&) = delete;
    GetClockLimit(GetClockLimit&&) = default;
    GetClockLimit& operator=(const GetClockLimit&) = delete;
    GetClockLimit& operator=(GetClockLimit&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetClockLimit(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto clockLimitOptionGroup = app->add_option_group(
            "Required",
            "Clock Id for which Limit is to be retrieved Graphics(0)/Memory(1).");

        clockId = 2;
        clockLimitOptionGroup->add_option("-c, --clockId", clockId,
                                          "retrieve clock Limit for clockId");
        clockLimitOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_clock_limit_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_clock_limit_req(instanceId, clockId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        struct nsm_clock_limit clockLimit;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;

        auto rc =
            decode_get_clock_limit_resp(responsePtr, payloadLength, &cc,
                                        &data_size, &reason_code, &clockLimit);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_clock_limit_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["MaxSpeed"] = (int)clockLimit.present_limit_max;
        result["MinSpeed"] = (int)clockLimit.present_limit_min;
        result["SpeedLimit"] = (int)clockLimit.requested_limit_max;
        if (clockLimit.requested_limit_max == clockLimit.requested_limit_min)
        {
            result["SpeedLocked"] = true;
            result["SpeedConfig"] =
                std::make_tuple(true, (uint32_t)clockLimit.requested_limit_max);
        }
        else
        {
            result["SpeedLocked"] = false;
            result["SpeedConfig"] = std::make_tuple(
                false, (uint32_t)clockLimit.requested_limit_max);
        }
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t clockId;
};

class GetCurrClockFreq : public CommandInterface
{
  public:
    ~GetCurrClockFreq() = default;
    GetCurrClockFreq() = delete;
    GetCurrClockFreq(const GetCurrClockFreq&) = delete;
    GetCurrClockFreq(GetCurrClockFreq&&) = default;
    GetCurrClockFreq& operator=(const GetCurrClockFreq&) = delete;
    GetCurrClockFreq& operator=(GetCurrClockFreq&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_curr_clock_freq_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint32_t clockFreq = 2;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_get_curr_clock_freq_resp(responsePtr, payloadLength,
                                                  &cc, &data_size, &reason_code,
                                                  &clockFreq);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_curr_clock_freq_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["OperatingSpeed"] = clockFreq;
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetAccumGpuUtilTime : public CommandInterface
{
  public:
    ~GetAccumGpuUtilTime() = default;
    GetAccumGpuUtilTime() = delete;
    GetAccumGpuUtilTime(const GetAccumGpuUtilTime&) = delete;
    GetAccumGpuUtilTime(GetAccumGpuUtilTime&&) = default;
    GetAccumGpuUtilTime& operator=(const GetAccumGpuUtilTime&) = delete;
    GetAccumGpuUtilTime& operator=(GetAccumGpuUtilTime&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_accum_GPU_util_time_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint32_t context_util_time;
        uint32_t SM_util_time;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_accum_GPU_util_time_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code,
            &context_util_time, &SM_util_time);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: " << "rc=" << rc
                      << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_accum_GPU_util_time_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["AccumulatedGPUContextUtilizationDuration"] =
            (int)context_util_time;
        result["AccumulatedSMUtilizationDuration"] = (int)SM_util_time;

        nsmtool::helper::DisplayInJson(result);
    }
};

void registerCommand(CLI::App& app)
{
    auto telemetry = app.add_subcommand(
        "telemetry", "Network, PCI link and platform telemetry type command");
    telemetry->require_subcommand(1);

    auto getPortTelemetryCounter = telemetry->add_subcommand(
        "GetPortTelemetryCounter", "get port telemetry counter");
    commands.push_back(std::make_unique<GetPortTelemetryCounter>(
        "telemetry", "GetPortTelemetryCounter", getPortTelemetryCounter));

    auto queryPortCharacteristics = telemetry->add_subcommand(
        "QueryPortCharacteristics", "query port characteristics");
    commands.push_back(std::make_unique<QueryPortCharacteristics>(
        "telemetry", "QueryPortCharacteristics", queryPortCharacteristics));

    auto queryPortStatus =
        telemetry->add_subcommand("QueryPortStatus", "query port status");
    commands.push_back(std::make_unique<QueryPortStatus>(
        "telemetry", "QueryPortStatus", queryPortStatus));

    auto getInventoryInformation = telemetry->add_subcommand(
        "GetInventoryInformation", "get inventory information");
    commands.push_back(std::make_unique<GetInventoryInformation>(
        "telemetry", "GetInventoryInformation", getInventoryInformation));

    auto getGpuPresenceAndPowerStatus = telemetry->add_subcommand(
        "GetGpuPresenceAndPowerStatus", "get gpu presence and power status");
    commands.push_back(std::make_unique<GetGpuPresenceAndPowerStatus>(
        "telemetry", "GetGpuPresenceAndPowerStatus",
        getGpuPresenceAndPowerStatus));

    auto getPowerSupplyStatus = telemetry->add_subcommand(
        "GetPowerSupplyStatus", "get power supply status");
    commands.push_back(std::make_unique<GetPowerSupplyStatus>(
        "telemetry", "GetPowerSupplyStatus", getPowerSupplyStatus));

    auto getTemperatureReading = telemetry->add_subcommand(
        "GetTemperatureReading", "get temperature reading of a sensor");
    commands.push_back(std::make_unique<GetTemperatureReading>(
        "telemetry", "GetTemperatureReading", getTemperatureReading));

    auto getCurrentPowerDraw = telemetry->add_subcommand(
        "GetCurrentPowerDraw", "get current power draw of a device");
    commands.push_back(std::make_unique<GetCurrentPowerDraw>(
        "telemetry", "GetCurrentPowerDraw", getCurrentPowerDraw));

    auto getCurrentEnergyCount = telemetry->add_subcommand(
        "GetCurrentEnergyCount",
        "get current energy counter value of a device");
    commands.push_back(std::make_unique<GetCurrentEnergyCount>(
        "telemetry", "GetCurrentEnergyCount", getCurrentEnergyCount));

    auto getVoltage =
        telemetry->add_subcommand("GetVoltage", "get voltage of a device");
    commands.push_back(
        std::make_unique<GetVoltage>("telemetry", "GetVoltage", getVoltage));

    auto getAltitudePressure = telemetry->add_subcommand(
        "GetAltitudePressure", "get current power draw of a device");
    commands.push_back(std::make_unique<GetAltitudePressure>(
        "telemetry", "GetAltitudePressure", getAltitudePressure));

    auto getDriverInfo =
        telemetry->add_subcommand("GetDriverInfo", "get Driver info");
    commands.push_back(std::make_unique<GetDriverInfo>(
        "telemetry", "GetDriverInfo", getDriverInfo));

    auto getMigMode = telemetry->add_subcommand("GetMigModes", "get MIG modes");
    commands.push_back(
        std::make_unique<GetMigMode>("telemetry", "GetMigMode", getMigMode));

    auto setMigMode = telemetry->add_subcommand("SetMigModes", "set MIG mode");
    commands.push_back(
        std::make_unique<SetMigMode>("telemetry", "SetMigMode", setMigMode));

    auto getEccMode = telemetry->add_subcommand("GetEccMode", "get ECC modes");
    commands.push_back(
        std::make_unique<GetEccMode>("telemetry", "GetEccMode", getEccMode));

    auto setEccMode = telemetry->add_subcommand("SetEccMode", "set ECC modes");
    commands.push_back(
        std::make_unique<SetEccMode>("telemetry", "SetEccMode", setEccMode));

    auto getEccErrorCounts =
        telemetry->add_subcommand("GetEccErrorCounts", "get ECC error counts");
    commands.push_back(std::make_unique<GetEccErrorCounts>(
        "telemetry", "GetEccErrorCounts", getEccErrorCounts));

    auto getEDPpScalingFactors = telemetry->add_subcommand(
        "GetEDPpScalingFactors", "get programmable EDPp Scaling Factors");
    commands.push_back(std::make_unique<GetEDPpScalingFactors>(
        "telemetry", "GetEDPpScalingFactors", getEDPpScalingFactors));

    auto queryScalarGroupTelemetry = telemetry->add_subcommand(
        "QueryScalarGroupTelemetry", "retrieve Scalar Data source for group ");
    commands.push_back(std::make_unique<QueryScalarGroupTelemetry>(
        "telemetry", "QueryScalarGroupTelemetry", queryScalarGroupTelemetry));
    auto getClockLimit = telemetry->add_subcommand(
        "GetClockLimit", "retrieve clock Limit for clockId");
    commands.push_back(std::make_unique<GetClockLimit>(
        "telemetry", "GetClockLimit", getClockLimit));

    auto getCurrClockFreq = telemetry->add_subcommand(
        "GetCurrClockFreq", "get current clock frequency of GPU");
    commands.push_back(std::make_unique<GetCurrClockFreq>(
        "telemetry", "GetCurrClockFreq", getCurrClockFreq));

    auto getAccumGpuUtilTime = telemetry->add_subcommand(
        "GetAccumGpuUtilTime",
        "Get Accumulated GPU Utilization Time Context/SM");
    commands.push_back(std::make_unique<GetAccumGpuUtilTime>(
        "telemetry", "GetAccumGpuUtilTime", getAccumGpuUtilTime));
}

} // namespace telemetry

} // namespace nsmtool
