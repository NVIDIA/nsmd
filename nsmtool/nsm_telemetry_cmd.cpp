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

#include "nsm_telemetry_cmd.hpp"

#include "base.h"
#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"

#include "cmd_helper.hpp"
#include "gpm_metrics_list.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>

#include <cstdint>
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
        struct nsm_port_counter_data portTeleData = {};

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

        if (portData->supported_counter.symbol_ber)
        {
            result[key].push_back(18);
            countersResult["Symbol BER"] =
                static_cast<uint64_t>(portData->symbol_ber);
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

        if (portData->supported_counter.effective_ber)
        {
            result[key].push_back(25);
            countersResult["Effective BER"] =
                static_cast<uint64_t>(portData->effective_ber);
        }

        if (portData->supported_counter.estimated_effective_ber)
        {
            result[key].push_back(26);
            countersResult["Estimated Effective BER"] =
                static_cast<uint64_t>(portData->estimated_effective_ber);
        }

        if (portData->supported_counter.effective_error)
        {
            result[key].push_back(27);
            countersResult["Effective Error"] =
                static_cast<uint64_t>(portData->effective_error);
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

class QueryPortsAvailable : public CommandInterface
{
  public:
    ~QueryPortsAvailable() = default;
    QueryPortsAvailable() = delete;
    QueryPortsAvailable(const QueryPortsAvailable&) = delete;
    QueryPortsAvailable(QueryPortsAvailable&&) = default;
    QueryPortsAvailable& operator=(const QueryPortsAvailable&) = delete;
    QueryPortsAvailable& operator=(QueryPortsAvailable&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_query_ports_available_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_query_ports_available_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataLen = 0;
        uint8_t number_of_ports = 0;

        auto rc = decode_query_ports_available_resp(responsePtr, payloadLength,
                                                    &cc, &reason_code, &dataLen,
                                                    &number_of_ports);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            ordered_json result;
            result["Number of Ports"] = number_of_ports;
            nsmtool::helper::DisplayInJson(result);
        }
        else
        {
            std::cerr
                << "Response message error: decode_query_ports_available_resp fail"
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }
        return;
    }
};

class SetPortDisableFuture : public CommandInterface
{
  public:
    ~SetPortDisableFuture() = default;
    SetPortDisableFuture() = delete;
    SetPortDisableFuture(const SetPortDisableFuture&) = delete;
    SetPortDisableFuture(SetPortDisableFuture&&) = default;
    SetPortDisableFuture& operator=(const SetPortDisableFuture&) = delete;
    SetPortDisableFuture& operator=(SetPortDisableFuture&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetPortDisableFuture(const char* type, const char* name,
                                  CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto setPortDisableFutureOptionGroup =
            app->add_option_group("Required", "Port mask [32 bytes].");

        portMask = {};
        setPortDisableFutureOptionGroup->add_option(
            "-p, --portMask", portMask, "Port mask from next reset");
        setPortDisableFutureOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_disable_future_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        std::vector<bitfield8_t> portMaskBitfield;
        for (const auto& value : portMask)
        {
            portMaskBitfield.emplace_back(value);
        }
        auto rc = encode_set_port_disable_future_req(
            instanceId, portMaskBitfield.data(), request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_set_port_disable_future_resp(
            responsePtr, payloadLength, &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            ordered_json result;
            result["Completion Code"] = cc;
            nsmtool::helper::DisplayInJson(result);
        }
        else
        {
            std::cerr
                << "Response message error: decode_query_ports_available_resp fail"
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }
        return;
    }

  private:
    std::vector<uint8_t> portMask;
};

class GetFabricManagerState : public CommandInterface
{
  public:
    ~GetFabricManagerState() = default;
    GetFabricManagerState() = delete;
    GetFabricManagerState(const GetFabricManagerState&) = delete;
    GetFabricManagerState(GetFabricManagerState&&) = default;
    GetFabricManagerState& operator=(const GetFabricManagerState&) = delete;
    GetFabricManagerState& operator=(GetFabricManagerState&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_fabric_manager_state_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_fabric_manager_state_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataLen = 0;
        struct nsm_fabric_manager_state_data fmStateData;

        auto rc = decode_get_fabric_manager_state_resp(
            responsePtr, payloadLength, &cc, &reason_code, &dataLen,
            &fmStateData);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            ordered_json result;
            result["FM State"] = fmStateData.fm_state;
            result["Report Status"] = fmStateData.report_status;
            result["Last Restart Timestamp (Epoch time in Sec)"] =
                static_cast<uint64_t>(fmStateData.last_restart_timestamp);
            result["Duration Since Last Restart (Sec)"] = static_cast<uint64_t>(
                fmStateData.duration_since_last_restart_sec);
            nsmtool::helper::DisplayInJson(result);
        }
        else
        {
            std::cerr
                << "Response message error: decode_get_fabric_manager_state_resp fail"
                << " rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }
        return;
    }
};

class GetPortDisableFuture : public CommandInterface
{
  public:
    ~GetPortDisableFuture() = default;
    GetPortDisableFuture() = delete;
    GetPortDisableFuture(const GetPortDisableFuture&) = delete;
    GetPortDisableFuture(GetPortDisableFuture&&) = default;
    GetPortDisableFuture& operator=(const GetPortDisableFuture&) = delete;
    GetPortDisableFuture& operator=(GetPortDisableFuture&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_disable_future_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_port_disable_future_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        bitfield8_t portMask[PORT_MASK_DATA_SIZE];

        auto rc = decode_get_port_disable_future_resp(
            responsePtr, payloadLength, &cc, &reason_code, &portMask[0]);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            ordered_json result;
            result["Completion Code"] = cc;
            std::string key("Port Mask Disable Future");

            parseBitfieldVar(result, key, &portMask[0], PORT_MASK_DATA_SIZE);
            nsmtool::helper::DisplayInJson(result);
        }
        else
        {
            std::cerr
                << "Response message error: decode_get_port_disable_future_resp fail"
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }
        return;
    }
};

class GetPowerMode : public CommandInterface
{
  public:
    ~GetPowerMode() = default;
    GetPowerMode() = delete;
    GetPowerMode(const GetPowerMode&) = delete;
    GetPowerMode(GetPowerMode&&) = default;
    GetPowerMode& operator=(const GetPowerMode&) = delete;
    GetPowerMode& operator=(GetPowerMode&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_power_mode_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_power_mode_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint16_t dataLen = 0;
        struct nsm_power_mode_data powerModeData;

        auto rc = decode_get_power_mode_resp(responsePtr, payloadLength, &cc,
                                             &reasonCode, &dataLen,
                                             &powerModeData);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            ordered_json result;
            result["Completion Code"] = cc;
            result["Data Length"] = dataLen;
            result["L1 HW Mode Control"] =
                static_cast<uint8_t>(powerModeData.l1_hw_mode_control);
            result["L1 HW Mode Threshold"] =
                static_cast<uint32_t>(powerModeData.l1_hw_mode_threshold);
            result["L1 FW Throttling Mode"] =
                static_cast<uint8_t>(powerModeData.l1_fw_throttling_mode);
            result["L1 Prediction Mode"] =
                static_cast<uint8_t>(powerModeData.l1_prediction_mode);
            result["L1 HW Active Time"] =
                static_cast<uint16_t>(powerModeData.l1_hw_active_time);
            result["L1 HW Inactive Time"] =
                static_cast<uint16_t>(powerModeData.l1_hw_inactive_time);
            result["L1 Prediction Inactive Time"] = static_cast<uint16_t>(
                powerModeData.l1_prediction_inactive_time);
            nsmtool::helper::DisplayInJson(result);
        }
        else
        {
            std::cerr
                << "Response message error: decode_get_power_mode_resp fail "
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reasonCode << "\n";
            return;
        }
        return;
    }
};

class SetPowerMode : public CommandInterface
{
  public:
    ~SetPowerMode() = default;
    SetPowerMode() = delete;
    SetPowerMode(const SetPowerMode&) = delete;
    SetPowerMode(SetPowerMode&&) = default;
    SetPowerMode& operator=(const SetPowerMode&) = delete;
    SetPowerMode& operator=(SetPowerMode&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetPowerMode(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto powerModeOptionGroup = app->add_option_group(
            "Required",
            "Port number for which counter value is to be retrieved.");

        l1HWModeControl = false;
        l1HWThreshold = 0;
        l1FWThrottlingMode = false;
        l1PredictionMode = false;
        l1HWActiveTime = 0;
        l1HWInactiveTime = 0;
        l1HWPredictionInactiveTime = 0;

        powerModeOptionGroup->add_option("-C, --modeControl", l1HWModeControl,
                                         "L1 hardware control mode");

        powerModeOptionGroup->add_option("-T, --modeThrottle",
                                         l1FWThrottlingMode,
                                         "L1 firmware throttling mode");

        powerModeOptionGroup->add_option("-P, --modePrediction",
                                         l1PredictionMode,
                                         "L1 prediction mechanism mode");

        powerModeOptionGroup->add_option("-t, --threshold", l1HWThreshold,
                                         "L1 state threshold bytes");

        powerModeOptionGroup->add_option("-a, --activeTime", l1HWActiveTime,
                                         "L1 state active time");

        powerModeOptionGroup->add_option("-i, --inactiveTime", l1HWInactiveTime,
                                         "L1 state inactive time");

        powerModeOptionGroup->add_option("-p, --predictionInactiveTime",
                                         l1HWPredictionInactiveTime,
                                         "L1 state prediction inactive time");

        powerModeOptionGroup->require_option(7);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        struct nsm_power_mode_data powerModeData;
        powerModeData.l1_hw_mode_control = l1HWModeControl;
        powerModeData.l1_hw_mode_threshold = l1HWThreshold;
        powerModeData.l1_fw_throttling_mode = l1FWThrottlingMode;
        powerModeData.l1_prediction_mode = l1PredictionMode;
        powerModeData.l1_hw_active_time = l1HWActiveTime;
        powerModeData.l1_hw_inactive_time = l1HWInactiveTime;
        powerModeData.l1_prediction_inactive_time = l1HWPredictionInactiveTime;

        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_set_power_mode_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_power_mode_req(instanceId, request, powerModeData);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;

        auto rc = decode_set_power_mode_resp(responsePtr, payloadLength, &cc,
                                             &reasonCode);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            ordered_json result;
            result["Completion Code"] = cc;
            nsmtool::helper::DisplayInJson(result);
        }
        else
        {
            std::cerr
                << "Response message error: decode_set_power_mode_resp fail "
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reasonCode << "\n";
            return;
        }
        return;
    }

  private:
    bool l1HWModeControl;
    uint32_t l1HWThreshold;
    bool l1FWThrottlingMode;
    bool l1PredictionMode;
    uint16_t l1HWActiveTime;
    uint16_t l1HWInactiveTime;
    uint16_t l1HWPredictionInactiveTime;
};

class GetSwitchIsolationMode : public CommandInterface
{
  public:
    ~GetSwitchIsolationMode() = default;
    GetSwitchIsolationMode() = delete;
    GetSwitchIsolationMode(const GetSwitchIsolationMode&) = delete;
    GetSwitchIsolationMode(GetSwitchIsolationMode&&) = default;
    GetSwitchIsolationMode& operator=(const GetSwitchIsolationMode&) = delete;
    GetSwitchIsolationMode& operator=(GetSwitchIsolationMode&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_switch_isolation_mode_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint8_t isolationMode;

        auto rc = decode_get_switch_isolation_mode_resp(
            responsePtr, payloadLength, &cc, &reasonCode, &isolationMode);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            ordered_json result;
            result["Completion Code"] = cc;
            result["Isolation Mode"] = isolationMode;
            nsmtool::helper::DisplayInJson(result);
        }
        else
        {
            std::cerr
                << "Response message error: decode_get_switch_isolation_mode_resp "
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reasonCode << "\n";
            return;
        }
        return;
    }
};

class SetSwitchIsolationMode : public CommandInterface
{
  public:
    ~SetSwitchIsolationMode() = default;
    SetSwitchIsolationMode() = delete;
    SetSwitchIsolationMode(const SetSwitchIsolationMode&) = delete;
    SetSwitchIsolationMode(SetSwitchIsolationMode&&) = default;
    SetSwitchIsolationMode& operator=(const SetSwitchIsolationMode&) = delete;
    SetSwitchIsolationMode& operator=(SetSwitchIsolationMode&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetSwitchIsolationMode(const char* type, const char* name,
                                    CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto requestedMode = app->add_option_group(
            "Required",
            "Requested Switch Isolation Mode 0(Enabled)/1(Disabled)");

        requestedMode->add_option("-i, --isolation_mode", isolation_mode,
                                  "Isolation Mode of Switch");

        requestedMode->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_set_MIG_mode_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_switch_isolation_mode_req(instanceId,
                                                       isolation_mode, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_set_switch_isolation_mode_resp(
            responsePtr, payloadLength, &cc, &reason_code);
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

    uint8_t isolation_mode;
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
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
                propRecordResult["Data"] = le32toh(*(uint32_t*)data.data());
                break;
            case MAXIMUM_MODULE_POWER_LIMIT:
                propRecordResult["Data"] = le32toh(*(uint32_t*)data.data());
                break;
            case RATED_MODULE_POWER_LIMIT:
            case DEFAULT_BOOST_CLOCKS:
            case DEFAULT_BASE_CLOCKS:
            case TRAY_SLOT_NUMBER:
            case TRAY_SLOT_INDEX:
                propRecordResult["Data"] = le32toh(*(uint32_t*)data.data());
                break;
            case GPU_HOST_ID:
            case GPU_MODULE_ID:
                propRecordResult["Data"] = le32toh(*(uint32_t*)data.data()) + 1;
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
            case FPGA_FIRMWARE_VERSION:
                propRecordResult["Data"] = std::string((char*)data.data(),
                                                       dataSize);
                break;
            case DEVICE_GUID:
            {
                std::vector<uint8_t> nvu8ArrVal(UUID_INT_SIZE, 0);
                memcpy(nvu8ArrVal.data(), data.data(), dataSize);
                propRecordResult["Data"] =
                    utils::convertUUIDToString(nvu8ArrVal);
                break;
            }
            case GPU_NVLINK_PEER_TYPE:
            {
                auto peerTypeNvU32 = le32toh(*(uint32_t*)data.data());
                if (peerTypeNvU32 == NSM_PEER_TYPE_DIRECT)
                {
                    propRecordResult["Data"] = std::string("Direct");
                }
                else
                {
                    propRecordResult["Data"] = std::string("Bridge");
                }
                break;
            }
            case CHASSIS_SERIAL_NUMBER:
            {
                try
                {
                    propRecordResult["Data"] = std::string((char*)data.data(),
                                                           dataSize);
                }
                catch (const std::exception& e)
                {
                    propRecordResult["Data"] =
                        utils::convertHexToString(data, dataSize);
                }
                break;
            }
            case GPU_IBGUID:
                propRecordResult["Data"] = utils::convertHexToString(data,
                                                                     dataSize);
                break;
            case MINIMUM_MEMORY_CLOCK_LIMIT:
                propRecordResult["Data"] = le32toh(*(uint32_t*)data.data());
                break;
            case MAXIMUM_MEMORY_CLOCK_LIMIT:
                propRecordResult["Data"] = le32toh(*(uint32_t*)data.data());
                break;
            case MINIMUM_GRAPHICS_CLOCK_LIMIT:
                propRecordResult["Data"] = le32toh(*(uint32_t*)data.data());
                break;
            case MAXIMUM_GRAPHICS_CLOCK_LIMIT:
                propRecordResult["Data"] = le32toh(*(uint32_t*)data.data());
                break;
            case MINIMUM_EDPP_SCALING_FACTOR:
                propRecordResult["Data"] = *(uint8_t*)data.data();
                break;
            case MAXIMUM_EDPP_SCALING_FACTOR:
                propRecordResult["Data"] = *(uint8_t*)data.data();
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << static_cast<int>(cc)
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc << "\n";

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
                        << "tag=" << static_cast<int>(tag) << ", rc=" << rc
                        << "\n";

                    continue;
                }

                rc = decode_aggregate_timestamp_data(data, data_len,
                                                     &timestamp);
                if (rc != NSM_SW_SUCCESS)
                {
                    std::cerr
                        << "Response message error while parsing timestamp sample data : "
                        << "tag=" << static_cast<int>(tag) << ", rc=" << rc
                        << "\n";

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
                        << "tag=" << static_cast<int>(tag) << ", rc=" << rc
                        << "\n";

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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
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
        auto rc = encode_get_temperature_reading_req(instanceId, sensorId,
                                                     request);
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

class ReadThermalParameter : public CommandInterface
{
  public:
    ReadThermalParameter() = delete;
    ReadThermalParameter(const ReadThermalParameter&) = delete;
    ReadThermalParameter(ReadThermalParameter&&) = default;
    ReadThermalParameter& operator=(const ReadThermalParameter&) = delete;
    ReadThermalParameter& operator=(ReadThermalParameter&&) = delete;

    explicit ReadThermalParameter(const char* type, const char* name,
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
        int32_t threshold;

        auto rc = decode_read_thermal_parameter_resp(responsePtr, msg_len, &cc,
                                                     &reason_code, &threshold);
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(nsm_get_current_power_draw_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Thermal Parameter"] = threshold;

        nsmtool::helper::DisplayInJson(result);
    }

    class ReadThermalParameterAggregateResponseParser :
        public AggregateResponseParser
    {
      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            int32_t threshold;
            auto rc = decode_aggregate_thermal_parameter_data(data, data_len,
                                                              &threshold);
            if (rc == NSM_SUCCESS)
            {
                sample_json["Sensor Id"] = tag;
                sample_json["Thermal Parameter"] = threshold;
            }

            return rc;
        }
    };

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_read_thermal_parameter_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_read_thermal_parameter_req(instanceId, sensorId,
                                                    request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (sensorId == AggregateSensorId)
        {
            ReadThermalParameterAggregateResponseParser{}
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
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

class GetMaxObservedPower : public CommandInterface
{
  public:
    GetMaxObservedPower() = delete;
    GetMaxObservedPower(const GetMaxObservedPower&) = delete;
    GetMaxObservedPower(GetMaxObservedPower&&) = default;
    GetMaxObservedPower& operator=(const GetMaxObservedPower&) = delete;
    GetMaxObservedPower& operator=(GetMaxObservedPower&&) = delete;

    explicit GetMaxObservedPower(const char* type, const char* name,
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

        auto rc = decode_get_max_observed_power_resp(responsePtr, msg_len, &cc,
                                                     &reason_code, &reading);
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(nsm_get_current_power_draw_resp))
                      << '\n';

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Averaging Interval"] = averagingInterval;
        result["Max Observed Power"] = reading;

        nsmtool::helper::DisplayInJson(result);
    }

    class GetMaxObservedPowerAggregateResponseParser :
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
                sample_json["Max Observed Power"] = reading;
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
        auto rc = encode_get_max_observed_power_req(instanceId, sensorId,
                                                    averagingInterval, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (sensorId == AggregateSensorId)
        {
            GetMaxObservedPowerAggregateResponseParser{}.parseAggregateResponse(
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
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
            auto rc = decode_aggregate_energy_count_data(data, data_len,
                                                         &reading);

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
        auto rc = encode_get_current_energy_count_req(instanceId, sensorId,
                                                      request);
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
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
        auto requestedMode = app->add_option_group("Required",
                                                   "Requested ECC Mode.");

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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
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

class SetClockLimit : public CommandInterface
{
  public:
    ~SetClockLimit() = default;
    SetClockLimit() = delete;
    SetClockLimit(const SetClockLimit&) = delete;
    SetClockLimit(SetClockLimit&&) = default;
    SetClockLimit& operator=(const SetClockLimit&) = delete;
    SetClockLimit& operator=(SetClockLimit&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetClockLimit(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto requestedClockLimit =
            app->add_option_group("Required", "Requested Clock Limit.");

        requestedClockLimit->add_option(
            "-c, --clockId", clockId,
            "clock type (0/1) (graphics clock/memory clock)");
        requestedClockLimit->add_option("-f, --flags", flags, "flags");
        requestedClockLimit->add_option("-a, --limit_min", limitMin,
                                        "Minimum Clock frequency");
        requestedClockLimit->add_option("-b, --limit_max", limitMax,
                                        "Maximum clock frequency");
        requestedClockLimit->require_option(4);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_set_clock_limit_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_clock_limit_req(instanceId, clockId, flags,
                                             limitMin, limitMax, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_set_clock_limit_resp(responsePtr, payloadLength, &cc,
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
    uint32_t limitMin;
    uint32_t limitMax;
    uint8_t flags;
    uint8_t clockId;
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
                << "Response message error: "
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reason_code << "\n"
                << payloadLength << "...."
                << (sizeof(struct nsm_msg_hdr) +
                    sizeof(
                        struct nsm_get_programmable_EDPp_scaling_factor_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Requested_Persistent_Scaling_Factor"] =
            scaling_factors.persistent_scaling_factor;
        result["Requested_OneShot_Scaling_Factor"] =
            scaling_factors.oneshot_scaling_factor;
        result["Enforced_Scaling_Factor"] =
            scaling_factors.enforced_scaling_factor;
        nsmtool::helper::DisplayInJson(result);
    }
};

class SetEDPpScalingFactors : public CommandInterface
{
  public:
    ~SetEDPpScalingFactors() = default;
    SetEDPpScalingFactors() = delete;
    SetEDPpScalingFactors(const SetEDPpScalingFactors&) = delete;
    SetEDPpScalingFactors(SetEDPpScalingFactors&&) = default;
    SetEDPpScalingFactors& operator=(const SetEDPpScalingFactors&) = delete;
    SetEDPpScalingFactors& operator=(SetEDPpScalingFactors&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetEDPpScalingFactors(const char* type, const char* name,
                                   CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-a, --action", action,
                        "Action to be perform on EDPp Scaling factor")
            ->required();
        app->add_option("-p, --persistence", persistence,
                        "life Time of EDPp scaling factor")
            ->required();
        app->add_option(
               "-s, --scaling_factor", scaling_factor,
               "Requested EDPp scaling factor as an integer percentage value")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_set_programmable_EDPp_scaling_factor_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_set_programmable_EDPp_scaling_factor_req(
            instanceId, action, persistence, scaling_factor, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_set_programmable_EDPp_scaling_factor_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code);
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

  private:
    uint8_t action;
    uint8_t persistence;
    uint8_t scaling_factor;
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
        scalarTelemetryOptionGroup->add_option("-d, --deviceIndex", deviceIndex,
                                               "retrieve deviceIndex");
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
            instanceId, deviceIndex, groupId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        switch (groupId)
        {
            case GROUP_ID_0:
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
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
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
            case GROUP_ID_1:
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
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
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
            case GROUP_ID_2:
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
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
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
            case GROUP_ID_3:
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
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
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
            case GROUP_ID_4:
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
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
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
            case GROUP_ID_5:
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
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
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
            case GROUP_ID_6:
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
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(struct nsm_msg_hdr) +
                            sizeof(
                                struct
                                nsm_query_scalar_group_telemetry_v1_group_6_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["InvalidFlitCounter"] = (int)data.invalid_flit_counter;
                result["LTSSMState"] = (int)data.ltssm_state;
                nsmtool::helper::DisplayInJson(result);
                break;
            }

            case GROUP_ID_8:
            {
                struct nsm_query_scalar_group_telemetry_group_8 data;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_query_scalar_group_telemetry_v1_group8_resp(
                    responsePtr, payloadLength, &cc, &data_size, &reason_code,
                    &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(struct nsm_msg_hdr) +
                            sizeof(
                                struct
                                nsm_query_scalar_group_telemetry_v1_group_8_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                std::vector<uint32_t> error_counts;
                for (int idx = 0; idx < TOTAL_PCIE_LANE_COUNT; idx++)
                {
                    error_counts.push_back((uint32_t)data.error_counts[idx]);
                }
                result["Per_Lane_Error_Counts"] = error_counts;
                nsmtool::helper::DisplayInJson(result);
                break;
            }

            case GROUP_ID_9:
            {
                struct nsm_query_scalar_group_telemetry_group_9 data;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc = decode_query_scalar_group_telemetry_v1_group9_resp(
                    responsePtr, payloadLength, &cc, &data_size, &reason_code,
                    &data);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(struct nsm_msg_hdr) +
                            sizeof(
                                struct
                                nsm_query_scalar_group_telemetry_v1_group_9_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                result["AER_Uncorrectable_Error_status"] =
                    (uint32_t)data.aer_uncorrectable_error_status;
                result["AER_Correctable_Error_status"] =
                    (uint32_t)data.aer_correctable_error_status;
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
    uint8_t deviceIndex;
    uint8_t groupId;
};

class QueryAvailableAndClearableScalarGroup : public CommandInterface
{
  public:
    ~QueryAvailableAndClearableScalarGroup() = default;
    QueryAvailableAndClearableScalarGroup() = delete;
    QueryAvailableAndClearableScalarGroup(
        const QueryAvailableAndClearableScalarGroup&) = delete;
    QueryAvailableAndClearableScalarGroup(
        QueryAvailableAndClearableScalarGroup&&) = default;
    QueryAvailableAndClearableScalarGroup&
        operator=(const QueryAvailableAndClearableScalarGroup&) = delete;
    QueryAvailableAndClearableScalarGroup&
        operator=(QueryAvailableAndClearableScalarGroup&&) = default;

    using CommandInterface::CommandInterface;

    explicit QueryAvailableAndClearableScalarGroup(const char* type,
                                                   const char* name,
                                                   CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto scalarTelemetryOptionGroup = app->add_option_group(
            "Required", "Group Id for which data source is to be retrieved.");

        scalarTelemetryOptionGroup->add_option("-d, --deviceIndex", deviceIndex,
                                               "retrieve deviceIndex");
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
        auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
            instanceId, deviceIndex, groupId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        std::vector<std::string> clearableSource;
        std::vector<std::string> availableSource;
        switch (groupId)
        {
            case GROUP_ID_2:
            {
                bitfield8_t available_source[1];
                bitfield8_t clearable_source[1];
                uint8_t mask_length;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc =
                    decode_query_available_clearable_scalar_data_sources_v1_resp(
                        responsePtr, payloadLength, &cc, &data_size,
                        &reason_code, &mask_length, (uint8_t*)available_source,
                        (uint8_t*)clearable_source);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(nsm_msg_hdr) +
                            sizeof(
                                nsm_query_available_clearable_scalar_data_sources_v1_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                if (available_source[0].bits.bit0)
                {
                    availableSource.push_back("TotalNonFatalErrors");
                }
                if (available_source[0].bits.bit1)
                {
                    availableSource.push_back("TotalFatalErrors");
                }
                if (available_source[0].bits.bit2)
                {
                    availableSource.push_back("UnsupportedRequestCounts");
                }
                if (available_source[0].bits.bit3)
                {
                    availableSource.push_back("CorrectableErrorCount");
                }
                result["AvailableSource"] = availableSource;

                if (clearable_source[0].bits.bit0)
                {
                    clearableSource.push_back("TotalNonFatalErrors");
                }
                if (clearable_source[0].bits.bit1)
                {
                    clearableSource.push_back("TotalFatalErrors");
                }
                if (clearable_source[0].bits.bit2)
                {
                    clearableSource.push_back("UnsupportedRequestCounts");
                }
                if (clearable_source[0].bits.bit3)
                {
                    clearableSource.push_back("CorresctableErrorCount");
                }
                result["ClearableSource"] = clearableSource;

                nsmtool::helper::DisplayInJson(result);
                break;
            }

            case GROUP_ID_3:
            {
                bitfield8_t available_source[1];
                bitfield8_t clearable_source[1];
                uint8_t mask_length;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc =
                    decode_query_available_clearable_scalar_data_sources_v1_resp(
                        responsePtr, payloadLength, &cc, &data_size,
                        &reason_code, &mask_length, (uint8_t*)available_source,
                        (uint8_t*)clearable_source);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(nsm_msg_hdr) +
                            sizeof(
                                nsm_query_available_clearable_scalar_data_sources_v1_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                if (available_source[0].bits.bit0)
                {
                    availableSource.push_back("L0ToRecoveryCount");
                }
                result["AvailableSource"] = availableSource;

                if (clearable_source[0].bits.bit0)
                {
                    clearableSource.push_back("L0ToRecoveryCount");
                }
                result["ClearableSource"] = clearableSource;

                nsmtool::helper::DisplayInJson(result);
                break;
            }

            case GROUP_ID_4:
            {
                bitfield8_t available_source[1];
                bitfield8_t clearable_source[1];
                uint8_t mask_length;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc =
                    decode_query_available_clearable_scalar_data_sources_v1_resp(
                        responsePtr, payloadLength, &cc, &data_size,
                        &reason_code, &mask_length, (uint8_t*)available_source,
                        (uint8_t*)clearable_source);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(nsm_msg_hdr) +
                            sizeof(
                                nsm_query_available_clearable_scalar_data_sources_v1_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;

                if (available_source[0].bits.bit0)
                {
                    availableSource.push_back("RecieverErrorCount");
                }
                if (available_source[0].bits.bit1)
                {
                    availableSource.push_back("NaksRecievedCount");
                }
                if (available_source[0].bits.bit2)
                {
                    availableSource.push_back("NaksSentCount");
                }
                if (available_source[0].bits.bit3)
                {
                    availableSource.push_back("BadTlpCount");
                }
                if (available_source[0].bits.bit4)
                {
                    availableSource.push_back("ReplayRollOverCount");
                }
                if (available_source[0].bits.bit5)
                {
                    availableSource.push_back("FcTimeoutErrorCount");
                }
                if (available_source[0].bits.bit6)
                {
                    availableSource.push_back("ReplayCount");
                }
                result["AvailableSource"] = availableSource;

                if (clearable_source[0].bits.bit0)
                {
                    clearableSource.push_back("RecieverErrorCount");
                }
                if (clearable_source[0].bits.bit1)
                {
                    clearableSource.push_back("NaksRecievedCount");
                }
                if (available_source[0].bits.bit2)
                {
                    clearableSource.push_back("NaksSentCount");
                }
                if (clearable_source[0].bits.bit3)
                {
                    clearableSource.push_back("BadTlpCount");
                }
                if (clearable_source[0].bits.bit4)
                {
                    clearableSource.push_back("ReplayRollOverCount");
                }
                if (clearable_source[0].bits.bit5)
                {
                    clearableSource.push_back("FcTimeoutErrorCount");
                }
                if (clearable_source[0].bits.bit6)
                {
                    clearableSource.push_back("ReplayCount");
                }
                result["clearableSource"] = clearableSource;

                nsmtool::helper::DisplayInJson(result);
                break;
            }
            case GROUP_ID_8:
            {
                bitfield8_t available_source[1];
                bitfield8_t clearable_source[1];
                uint8_t mask_length;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc =
                    decode_query_available_clearable_scalar_data_sources_v1_resp(
                        responsePtr, payloadLength, &cc, &data_size,
                        &reason_code, &mask_length, (uint8_t*)available_source,
                        (uint8_t*)clearable_source);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(nsm_msg_hdr) +
                            sizeof(
                                nsm_query_available_clearable_scalar_data_sources_v1_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                if (available_source[0].bits.bit0)
                {
                    availableSource.push_back("PerLaneErrorCounts");
                }
                result["AvailableSource"] = availableSource;

                if (clearable_source[0].bits.bit0)
                {
                    clearableSource.push_back("PerLaneErrorCounts");
                }
                result["ClearableSource"] = clearableSource;

                nsmtool::helper::DisplayInJson(result);
                break;
            }

            case GROUP_ID_9:
            {
                bitfield8_t available_source[1];
                bitfield8_t clearable_source[1];
                uint8_t mask_length;
                uint16_t data_size;
                uint16_t reason_code = ERR_NULL;

                auto rc =
                    decode_query_available_clearable_scalar_data_sources_v1_resp(
                        responsePtr, payloadLength, &cc, &data_size,
                        &reason_code, &mask_length, (uint8_t*)available_source,
                        (uint8_t*)clearable_source);
                if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
                {
                    std::cerr
                        << "Response message error: "
                        << "rc=" << rc << ", cc=" << (int)cc
                        << ", reasonCode=" << (int)reason_code << "\n"
                        << payloadLength << "...."
                        << (sizeof(nsm_msg_hdr) +
                            sizeof(
                                nsm_query_available_clearable_scalar_data_sources_v1_resp));

                    return;
                }

                ordered_json result;
                result["Completion Code"] = cc;
                if (available_source[0].bits.bit0)
                {
                    availableSource.push_back("AerUncorrectableErrorStatus");
                }
                if (available_source[0].bits.bit1)
                {
                    availableSource.push_back("AerCorrectableErrorStatus");
                }
                result["AvailableSource"] = availableSource;
                if (clearable_source[0].bits.bit0)
                {
                    clearableSource.push_back("AerUncorrectableErrorStatus");
                }

                if (clearable_source[0].bits.bit1)
                {
                    clearableSource.push_back("AerCorrectableErrorStatus");
                }
                result["ClearableSource"] = clearableSource;

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
    uint8_t deviceIndex;
    uint8_t groupId;
};

class PcieFundamentalReset : public CommandInterface
{
  public:
    ~PcieFundamentalReset() = default;
    PcieFundamentalReset() = delete;
    PcieFundamentalReset(const PcieFundamentalReset&) = delete;
    PcieFundamentalReset(PcieFundamentalReset&&) = default;
    PcieFundamentalReset& operator=(const PcieFundamentalReset&) = delete;
    PcieFundamentalReset& operator=(PcieFundamentalReset&&) = default;

    using CommandInterface::CommandInterface;

    explicit PcieFundamentalReset(const char* type, const char* name,
                                  CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-d, --device_index", device_index,
                        "Device Index for which reset is performed")
            ->required();
        app->add_option("-a, --action", action,
                        "Action to be performed 0-Not reset 1-reset")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_assert_pcie_fundamental_reset_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_assert_pcie_fundamental_reset_req(
            instanceId, device_index, action, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_assert_pcie_fundamental_reset_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code);
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

    uint8_t device_index;
    uint8_t action;
};

class ClearScalarDataSource : public CommandInterface
{
  public:
    ~ClearScalarDataSource() = default;
    ClearScalarDataSource() = delete;
    ClearScalarDataSource(const ClearScalarDataSource&) = delete;
    ClearScalarDataSource(ClearScalarDataSource&&) = default;
    ClearScalarDataSource& operator=(const ClearScalarDataSource&) = delete;
    ClearScalarDataSource& operator=(ClearScalarDataSource&&) = default;

    using CommandInterface::CommandInterface;

    explicit ClearScalarDataSource(const char* type, const char* name,
                                   CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-d, --device_index", device_index,
                        "Device Index for which reset is performed")
            ->required();
        app->add_option("-g, --groupId", groupId,
                        "Identifier of group to query")
            ->required();
        app->add_option("-i, --dsId", dsId,
                        "Index of data source within the group")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_clear_data_source_v1_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_clear_data_source_v1_req(instanceId, device_index,
                                                  groupId, dsId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_clear_data_source_v1_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code);
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

    uint8_t device_index;
    uint8_t groupId;
    uint8_t dsId;
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

        auto rc = decode_get_clock_limit_resp(responsePtr, payloadLength, &cc,
                                              &data_size, &reason_code,
                                              &clockLimit);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_clock_limit_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["SpeedLimit"] = (uint32_t)clockLimit.present_limit_max;
        result["PresentLimitMin"] = (uint32_t)clockLimit.present_limit_min;
        result["PresentLimitMax"] = (uint32_t)clockLimit.present_limit_max;
        result["RequestedLimitMax"] = (uint32_t)clockLimit.requested_limit_max;
        result["RequestedLimitMin"] = (uint32_t)clockLimit.requested_limit_min;
        if (clockLimit.present_limit_min == clockLimit.present_limit_max)
        {
            result["SpeedLocked"] = true;
            result["SpeedConfig"] =
                std::make_tuple(true, (uint32_t)clockLimit.present_limit_max);
        }
        else
        {
            result["SpeedLocked"] = false;
            result["SpeedConfig"] =
                std::make_tuple(false, (uint32_t)clockLimit.present_limit_max);
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

    explicit GetCurrClockFreq(const char* type, const char* name,
                              CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto currClockFreqOptionGroup = app->add_option_group(
            "Required",
            "Clock Id for which Limit is to be retrieved Graphics(0)/Memory(1).");

        currClockFreqOptionGroup->add_option(
            "-c, --clockId", clockId, "retrieve clock Limit for clockId");
        currClockFreqOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_curr_clock_freq_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());

        auto rc = encode_get_curr_clock_freq_req(instanceId, clockId, request);
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
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

  private:
    uint8_t clockId;
};

class GetProcessorThrottleReason : public CommandInterface
{
  public:
    ~GetProcessorThrottleReason() = default;
    GetProcessorThrottleReason() = delete;
    GetProcessorThrottleReason(const GetMigMode&) = delete;
    GetProcessorThrottleReason(GetProcessorThrottleReason&&) = default;
    GetProcessorThrottleReason&
        operator=(const GetProcessorThrottleReason&) = delete;
    GetProcessorThrottleReason&
        operator=(GetProcessorThrottleReason&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_current_clock_event_reason_code_req(instanceId,
                                                                 request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        bitfield32_t flags;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_current_clock_event_reason_code_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code, &flags);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr
                << "Response message error: "
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reason_code << "\n"
                << payloadLength << "...."
                << (sizeof(struct nsm_msg_hdr) +
                    sizeof(
                        struct nsm_get_current_clock_event_reason_code_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        std::vector<std::string> throttleReasons;
        if (flags.bits.bit0)
        {
            throttleReasons.push_back(
                "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.SWPowerCap");
        }
        if (flags.bits.bit1)
        {
            throttleReasons.push_back(
                "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.HWSlowdown");
        }
        if (flags.bits.bit2)
        {
            throttleReasons.push_back(
                "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.HWThermalSlowdown");
        }
        if (flags.bits.bit3)
        {
            throttleReasons.push_back(
                "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.HWPowerBrakeSlowdown");
        }
        if (flags.bits.bit4)
        {
            throttleReasons.push_back(
                "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.SyncBoost");
        }
        if (flags.bits.bit5)
        {
            throttleReasons.push_back(
                "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.ClockOptimizedForThermalEngage");
        }

        result["ThrottleReasons"] = throttleReasons;
        nsmtool::helper::DisplayInJson(result);
    }
};

class SetPowerLimit : public CommandInterface
{
  public:
    ~SetPowerLimit() = default;
    SetPowerLimit() = delete;
    SetPowerLimit(const SetPowerLimit&) = delete;
    SetPowerLimit(SetPowerLimit&&) = default;
    SetPowerLimit& operator=(const SetPowerLimit&) = delete;
    SetPowerLimit& operator=(SetPowerLimit&&) = default;

    using CommandInterface::CommandInterface;

    explicit SetPowerLimit(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-i, --powerLimitId", powerLimitId,
                        "Power Limit Id Device/Module")
            ->required();

        app->add_option("-a, --action", action,
                        "action to be performed on power limit")
            ->required();

        app->add_option("-p, --persistence", persistence,
                        "Lifetime of new power limit")
            ->required();

        app->add_option("-l, --power_limit", power_limit,
                        "Device power Limit in miliwatts")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_set_power_limit_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());

        int rc = NSM_SW_ERROR_DATA;
        if (powerLimitId == DEVICE)
        {
            rc = encode_set_device_power_limit_req(
                instanceId, action, persistence, power_limit, request);
        }
        else if (powerLimitId == MODULE)
        {
            rc = encode_set_module_power_limit_req(
                instanceId, action, persistence, power_limit, request);
        }
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_set_power_limit_resp(responsePtr, payloadLength, &cc,
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

    uint32_t powerLimitId;
    uint8_t action;
    uint8_t persistence;
    uint32_t power_limit;
};

class GetPowerLimit : public CommandInterface
{
  public:
    ~GetPowerLimit() = default;
    GetPowerLimit() = delete;
    GetPowerLimit(const GetPowerLimit&) = delete;
    GetPowerLimit(GetPowerLimit&&) = default;
    GetPowerLimit& operator=(const GetPowerLimit&) = delete;
    GetPowerLimit& operator=(GetPowerLimit&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetPowerLimit(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-i, --powerLimitId", powerLimitId,
                        "Power Limit Id Device/Module")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_power_limit_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());

        int rc = NSM_SW_ERROR_DATA;
        if (powerLimitId == DEVICE)
        {
            rc = encode_get_device_power_limit_req(instanceId, request);
        }
        else if (powerLimitId == MODULE)
        {
            rc = encode_get_module_power_limit_req(instanceId, request);
        }
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        uint32_t requestedPersistentLimit;
        uint32_t requestedOneShotlimit;
        uint32_t enforcedLimit;
        auto rc = decode_get_power_limit_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code,
            &requestedPersistentLimit, &requestedOneShotlimit, &enforcedLimit);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_power_limit_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["requestedPersistentLimit"] = requestedPersistentLimit;
        result["requestedOneShotlimit"] = requestedOneShotlimit;
        result["enforcedLimit"] = enforcedLimit;
        nsmtool::helper::DisplayInJson(result);
    }

    uint32_t powerLimitId;
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
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
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

class GetRowRemapState : public CommandInterface
{
  public:
    ~GetRowRemapState() = default;
    GetRowRemapState() = delete;
    GetRowRemapState(const GetRowRemapState&) = delete;
    GetRowRemapState(GetRowRemapState&&) = default;
    GetRowRemapState& operator=(const GetRowRemapState&) = delete;
    GetRowRemapState& operator=(GetRowRemapState&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_row_remap_state_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        bitfield8_t flags;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_row_remap_state_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code, &flags);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_row_remap_state_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        if (flags.bits.bit0)
        {
            result["RowRemappingFailureState"] = true;
        }
        else
        {
            result["RowRemappingFailureState"] = false;
        }
        if (flags.bits.bit1)
        {
            result["RowRemappingPendingState"] = true;
        }
        else
        {
            result["RowRemappingPendingState"] = false;
        }
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetRowRemappingCounts : public CommandInterface
{
  public:
    ~GetRowRemappingCounts() = default;
    GetRowRemappingCounts() = delete;
    GetRowRemappingCounts(const GetRowRemappingCounts&) = delete;
    GetRowRemappingCounts(GetRowRemappingCounts&&) = default;
    GetRowRemappingCounts& operator=(const GetRowRemappingCounts&) = delete;
    GetRowRemappingCounts& operator=(GetRowRemappingCounts&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_row_remapping_counts_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint32_t correctable_error;
        uint32_t uncorrectable_error;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_row_remapping_counts_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code,
            &correctable_error, &uncorrectable_error);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_row_remapping_counts_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["ceRowRemappingCount"] = correctable_error;
        result["ueRowRemappingCount"] = uncorrectable_error;
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetRowRemapAvailability : public CommandInterface
{
  public:
    ~GetRowRemapAvailability() = default;
    GetRowRemapAvailability() = delete;
    GetRowRemapAvailability(const GetRowRemapAvailability&) = delete;
    GetRowRemapAvailability(GetRowRemapAvailability&&) = default;
    GetRowRemapAvailability& operator=(const GetRowRemapAvailability&) = delete;
    GetRowRemapAvailability& operator=(GetRowRemapAvailability&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_row_remap_availability_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        struct nsm_row_remap_availability data;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_row_remap_availability_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code, &data);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_row_remap_availability_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["NoRemappingAvailability"] = (uint16_t)data.no_remapping;
        result["LowRemappingAvaialability"] = (uint16_t)data.low_remapping;
        result["PartialRemappingAvailability"] =
            (uint16_t)data.partial_remapping;
        result["HighRemappingAvailability"] = (uint16_t)data.high_remapping;
        result["MaxRemappingAvailability"] = (uint16_t)data.max_remapping;
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetMemoryCapacityUtil : public CommandInterface
{
  public:
    ~GetMemoryCapacityUtil() = default;
    GetMemoryCapacityUtil() = delete;
    GetMemoryCapacityUtil(const GetMemoryCapacityUtil&) = delete;
    GetMemoryCapacityUtil(GetMemoryCapacityUtil&&) = default;
    GetMemoryCapacityUtil& operator=(const GetMemoryCapacityUtil&) = delete;
    GetMemoryCapacityUtil& operator=(GetMemoryCapacityUtil&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_memory_capacity_util_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        struct nsm_memory_capacity_utilization data;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_memory_capacity_util_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code, &data);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_memory_capacity_util_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["UsedMemory"] = static_cast<uint32_t>(data.used_memory);
        result["ReservedMemory"] = static_cast<uint32_t>(data.reserved_memory);
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetCurrentUtilization : public CommandInterface
{
  public:
    ~GetCurrentUtilization() = default;
    GetCurrentUtilization() = delete;
    GetCurrentUtilization(const GetCurrentUtilization&) = delete;
    GetCurrentUtilization(GetCurrentUtilization&&) = default;
    GetCurrentUtilization& operator=(const GetCurrentUtilization&) = delete;
    GetCurrentUtilization& operator=(GetCurrentUtilization&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_current_utilization_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        nsm_get_current_utilization_data data;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_get_current_utilization_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code, &data);

        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_current_utilization_resp))
                      << '\n';

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["GPU Utilizatin"] = uint32_t(data.gpu_utilization);
        result["Memory Utilizatin"] = uint32_t(data.memory_utilization);
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetClockOutputEnableState : public CommandInterface
{
  public:
    ~GetClockOutputEnableState() = default;
    GetClockOutputEnableState() = delete;
    GetClockOutputEnableState(const GetClockOutputEnableState&) = delete;
    GetClockOutputEnableState(GetClockOutputEnableState&&) = default;
    GetClockOutputEnableState&
        operator=(const GetClockOutputEnableState&) = delete;
    GetClockOutputEnableState& operator=(GetClockOutputEnableState&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetClockOutputEnableState(const char* type, const char* name,
                                       CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto clockBufOptionGroup = app->add_option_group(
            "Required",
            "Index for which clock buffer is to be retrieved [PCIe(0x80)/NVHS(0x81)/IBLink(0x82)]");

        index = 00;
        clockBufOptionGroup->add_option("-i, --index", index,
                                        "retrieve clock buffer for Index");
        clockBufOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_clock_output_enabled_state_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_clock_output_enable_state_req(instanceId, index,
                                                           request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint16_t dataLen = 0;
        uint32_t clkBuf = 0;

        auto rc = decode_get_clock_output_enable_state_resp(
            responsePtr, payloadLength, &cc, &reasonCode, &dataLen, &clkBuf);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            printClockBufferData(clkBuf);
        }
        else
        {
            std::cerr
                << "Response message error: decode_get_clock_output_enable_state_resp fail"
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reasonCode << "\n";
            return;
        }
        return;
    }

    void printClockBufferData(uint32_t& data)
    {
        ordered_json result;
        result["Index"] = index;
        ordered_json clockBufferResult;

        switch (index)
        {
            case PCIE_CLKBUF_INDEX:
            {
                auto clockBuffer =
                    reinterpret_cast<nsm_pcie_clock_buffer_data*>(&data);
                clockBufferResult["GPU PCIe Clock 1"] =
                    static_cast<int>(clockBuffer->clk_buf_gpu1);
                clockBufferResult["GPU PCIe Clock 2"] =
                    static_cast<int>(clockBuffer->clk_buf_gpu2);
                clockBufferResult["GPU PCIe Clock 3"] =
                    static_cast<int>(clockBuffer->clk_buf_gpu3);
                clockBufferResult["GPU PCIe Clock 4"] =
                    static_cast<int>(clockBuffer->clk_buf_gpu4);
                clockBufferResult["GPU PCIe Clock 5"] =
                    static_cast<int>(clockBuffer->clk_buf_gpu5);
                clockBufferResult["GPU PCIe Clock 6"] =
                    static_cast<int>(clockBuffer->clk_buf_gpu6);
                clockBufferResult["GPU PCIe Clock 7"] =
                    static_cast<int>(clockBuffer->clk_buf_gpu7);
                clockBufferResult["GPU PCIe Clock 8"] =
                    static_cast<int>(clockBuffer->clk_buf_gpu8);

                clockBufferResult["Retimer PCIe Clock 1"] =
                    static_cast<int>(clockBuffer->clk_buf_retimer1);
                clockBufferResult["Retimer PCIe Clock 2"] =
                    static_cast<int>(clockBuffer->clk_buf_retimer2);
                clockBufferResult["Retimer PCIe Clock 3"] =
                    static_cast<int>(clockBuffer->clk_buf_retimer3);
                clockBufferResult["Retimer PCIe Clock 4"] =
                    static_cast<int>(clockBuffer->clk_buf_retimer4);
                clockBufferResult["Retimer PCIe Clock 5"] =
                    static_cast<int>(clockBuffer->clk_buf_retimer5);
                clockBufferResult["Retimer PCIe Clock 6"] =
                    static_cast<int>(clockBuffer->clk_buf_retimer6);
                clockBufferResult["Retimer PCIe Clock 7"] =
                    static_cast<int>(clockBuffer->clk_buf_retimer7);
                clockBufferResult["Retimer PCIe Clock 8"] =
                    static_cast<int>(clockBuffer->clk_buf_retimer8);

                clockBufferResult["NvSwitch PCIe Clock 1"] =
                    static_cast<int>(clockBuffer->clk_buf_nvSwitch_1);
                clockBufferResult["NvSwitch PCIe Clock 2"] =
                    static_cast<int>(clockBuffer->clk_buf_nvSwitch_2);

                clockBufferResult["NvLink Mgmt NIC PCIe Clock"] =
                    static_cast<int>(clockBuffer->clk_buf_nvlinkMgmt_nic);
            }
            break;
            case NVHS_CLKBUF_INDEX:
            {
                auto clockBuffer =
                    reinterpret_cast<nsm_nvhs_clock_buffer_data*>(&data);
                clockBufferResult["SXM NVHS Clock 1"] =
                    static_cast<int>(clockBuffer->clk_buf_sxm_nvhs1);
                clockBufferResult["SXM NVHS Clock 2"] =
                    static_cast<int>(clockBuffer->clk_buf_sxm_nvhs2);
                clockBufferResult["SXM NVHS Clock 3"] =
                    static_cast<int>(clockBuffer->clk_buf_sxm_nvhs3);
                clockBufferResult["SXM NVHS Clock 4"] =
                    static_cast<int>(clockBuffer->clk_buf_sxm_nvhs4);
                clockBufferResult["SXM NVHS Clock 5"] =
                    static_cast<int>(clockBuffer->clk_buf_sxm_nvhs5);
                clockBufferResult["SXM NVHS Clock 6"] =
                    static_cast<int>(clockBuffer->clk_buf_sxm_nvhs6);
                clockBufferResult["SXM NVHS Clock 7"] =
                    static_cast<int>(clockBuffer->clk_buf_sxm_nvhs7);
                clockBufferResult["SXM NVHS Clock 8"] =
                    static_cast<int>(clockBuffer->clk_buf_sxm_nvhs8);

                clockBufferResult["NvSwitch NVHS Clock 1"] =
                    static_cast<int>(clockBuffer->clk_buf_nvSwitch_nvhs1);
                clockBufferResult["NvSwitch NVHS Clock 2"] =
                    static_cast<int>(clockBuffer->clk_buf_nvSwitch_nvhs2);
                clockBufferResult["NvSwitch NVHS Clock 3"] =
                    static_cast<int>(clockBuffer->clk_buf_nvSwitch_nvhs3);
                clockBufferResult["NvSwitch NVHS Clock 4"] =
                    static_cast<int>(clockBuffer->clk_buf_nvSwitch_nvhs4);
            }
            break;
            case IBLINK_CLKBUF_INDEX:
            {
                auto clockBuffer =
                    reinterpret_cast<nsm_iblink_clock_buffer_data*>(&data);
                clockBufferResult["NvSwitch IBLink Clock 1"] =
                    static_cast<int>(clockBuffer->clk_buf_nvSwitch_1);
                clockBufferResult["NvSwitch IBLink Clock 2"] =
                    static_cast<int>(clockBuffer->clk_buf_nvSwitch_2);

                clockBufferResult["NvLink Mgmt NIC IBLink Clock"] =
                    static_cast<int>(clockBuffer->clk_buf_nvlinkMgmt_nic);
            }
            break;
            default:
                clockBufferResult["Error"] = "Unsupported index in request";
                break;
        }
        result["Clock Enable State Information"] = clockBufferResult;

        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t index;
};

class QueryAggregatedGPMMetrics : public CommandInterface
{
  public:
    QueryAggregatedGPMMetrics() = delete;
    QueryAggregatedGPMMetrics(const QueryAggregatedGPMMetrics&) = delete;
    QueryAggregatedGPMMetrics(QueryAggregatedGPMMetrics&&) = default;
    QueryAggregatedGPMMetrics&
        operator=(const QueryAggregatedGPMMetrics&) = delete;
    QueryAggregatedGPMMetrics& operator=(QueryAggregatedGPMMetrics&&) = delete;

    explicit QueryAggregatedGPMMetrics(const char* type, const char* name,
                                       CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-r, --retrievalSource", retrievalSource,
                        "Retrieval Source")
            ->required();
        app->add_option("-g, --gpuInstance", gpuInstance, "GPU Instance")
            ->required();
        app->add_option("-c, --computeInstance", computeInstance,
                        "Compute Instance")
            ->required();
        app->add_option("-b, --metricsBitfield", metricsBitfield,
                        "Metrics Bitfield")
            ->required();
    }

  private:
    class QueryAggregatedGPMMetricsAggregateResponseParser :
        public AggregateResponseParser
    {
      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            const auto info = metricsTable.find(tag);
            if (info == metricsTable.end())
            {
                return NSM_SW_ERROR_DATA;
            }

            uint8_t rc = NSM_SW_SUCCESS;
            double value{};

            switch (info->second.unit)
            {
                case GPMMetricsUnit::PERCENTAGE:
                {
                    rc = decode_aggregate_gpm_metric_percentage_data(
                        data, data_len, &value);

                    break;
                }

                case GPMMetricsUnit::BANDWIDTH:
                {
                    uint64_t val;
                    rc = decode_aggregate_gpm_metric_bandwidth_data(
                        data, data_len, &val);

                    // unit of bandwidth is Bytes per seconds in NSM Command
                    // Response and unit for GPMMetrics PDI is Gbps. Hence it is
                    // converted to Gbps.
                    static constexpr uint64_t conversionFactor = 1024 * 1024 *
                                                                 128;
                    value = val / static_cast<double>(conversionFactor);

                    break;
                }
            }

            if (rc == NSM_SUCCESS)
            {
                sample_json["Metric Id"] = tag;
                sample_json["Metric Name"] = info->second.name;
                sample_json["Metric Value"] = value;
            }

            return rc;
        }
    };

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        // struct nsm_query_aggregate_gpm_metrics_req has char[1] as its last
        // member to handle variable size Metrics bitfield of the NSM Request.
        // 1 is subtracted from it's size to account for it.
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_aggregate_gpm_metrics_req) -
            1 + metricsBitfield.size());
        auto requestPtr = reinterpret_cast<nsm_msg*>(requestMsg.data());

        auto rc = encode_query_aggregate_gpm_metrics_req(
            instanceId, retrievalSource, gpuInstance, computeInstance,
            metricsBitfield.data(), metricsBitfield.size(), requestPtr);

        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        QueryAggregatedGPMMetricsAggregateResponseParser{}
            .parseAggregateResponse(responsePtr, payloadLength);
    }

  private:
    uint8_t retrievalSource;
    uint8_t gpuInstance;
    uint8_t computeInstance;
    std::vector<uint8_t> metricsBitfield;
};

class QueryPerInstanceGPMMetrics : public CommandInterface
{
  public:
    QueryPerInstanceGPMMetrics() = delete;
    QueryPerInstanceGPMMetrics(const QueryPerInstanceGPMMetrics&) = delete;
    QueryPerInstanceGPMMetrics(QueryPerInstanceGPMMetrics&&) = default;
    QueryPerInstanceGPMMetrics&
        operator=(const QueryPerInstanceGPMMetrics&) = delete;
    QueryPerInstanceGPMMetrics& operator=(QueryAggregatedGPMMetrics&&) = delete;

    explicit QueryPerInstanceGPMMetrics(const char* type, const char* name,
                                        CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-r, --retrievalSource", retrievalSource,
                        "Retrieval Source")
            ->required();
        app->add_option("-g, --gpuInstance", gpuInstance, "GPU Instance")
            ->required();
        app->add_option("-c, --computeInstance", computeInstance,
                        "Compute Instance")
            ->required();
        app->add_option("-i, --metricId", metricId, "Metric Id")->required();
        app->add_option("-b, --instanceBitfield", instanceBitfield,
                        "Instance Bitfield")
            ->required();
    }

  private:
    class QueryPerInstanceGPMMetricsAggregateResponseParser :
        public AggregateResponseParser
    {
      public:
        QueryPerInstanceGPMMetricsAggregateResponseParser(
            const MetricsInfo* info) :
            info(info)
        {}

      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            uint8_t rc = NSM_SW_SUCCESS;
            double value{};

            switch (info->unit)
            {
                case GPMMetricsUnit::PERCENTAGE:
                {
                    rc = decode_aggregate_gpm_metric_percentage_data(
                        data, data_len, &value);

                    break;
                }

                case GPMMetricsUnit::BANDWIDTH:
                {
                    uint64_t val;
                    rc = decode_aggregate_gpm_metric_bandwidth_data(
                        data, data_len, &val);

                    // unit of bandwidth is Bytes per seconds in NSM Command
                    // Response and unit for GPMMetrics PDI is Gbps. Hence it is
                    // converted to Gbps.
                    static constexpr uint64_t conversionFactor = 1024 * 1024 *
                                                                 128;
                    value = val / static_cast<double>(conversionFactor);

                    break;
                }
            }

            if (rc == NSM_SUCCESS)
            {
                sample_json["Instance Id"] = tag;
                sample_json["Metric Value"] = value;
            }

            return rc;
        }

      private:
        const MetricsInfo* info;
    };

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_per_instance_gpm_metrics_req));

        auto requestPtr = reinterpret_cast<nsm_msg*>(requestMsg.data());

        auto rc = encode_query_per_instance_gpm_metrics_req(
            instanceId, retrievalSource, gpuInstance, computeInstance, metricId,
            instanceBitfield, requestPtr);

        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        const auto find_result = metricsTable.find(metricId);
        if (find_result == metricsTable.end())
        {
            return;
        }
        QueryPerInstanceGPMMetricsAggregateResponseParser{&find_result->second}
            .parseAggregateResponse(responsePtr, payloadLength);
    }

  private:
    uint8_t retrievalSource;
    uint8_t gpuInstance;
    uint8_t computeInstance;
    uint8_t metricId;
    uint32_t instanceBitfield;
};

class GetViolationDuration : public CommandInterface
{
  public:
    ~GetViolationDuration() = default;
    GetViolationDuration() = delete;
    GetViolationDuration(const GetViolationDuration&) = delete;
    GetViolationDuration(GetViolationDuration&&) = default;
    GetViolationDuration& operator=(const GetViolationDuration&) = delete;
    GetViolationDuration& operator=(GetViolationDuration&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_violation_duration_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        struct nsm_violation_duration violationDuration;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;

        auto rc = decode_get_violation_duration_resp(
            responsePtr, payloadLength, &cc, &data_size, &reason_code,
            &violationDuration);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_violation_duration_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["SupportedCounter"] =
            static_cast<uint64_t>(violationDuration.supported_counter.byte);
        result["HwViolationDuration"] =
            static_cast<uint64_t>(violationDuration.hw_violation_duration);
        result["GlobalSwViolationDuration"] = static_cast<uint64_t>(
            violationDuration.global_sw_violation_duration);
        result["PowerViolationDuration"] =
            static_cast<uint64_t>(violationDuration.power_violation_duration);
        result["ThermalViolationDuration"] =
            static_cast<uint64_t>(violationDuration.thermal_violation_duration);
        result["Counter4"] = static_cast<uint64_t>(violationDuration.counter4);
        result["Counter5"] = static_cast<uint64_t>(violationDuration.counter5);
        result["Counter6"] = static_cast<uint64_t>(violationDuration.counter6);
        result["Counter7"] = static_cast<uint64_t>(violationDuration.counter7);
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

    auto queryPortStatus = telemetry->add_subcommand("QueryPortStatus",
                                                     "query port status");
    commands.push_back(std::make_unique<QueryPortStatus>(
        "telemetry", "QueryPortStatus", queryPortStatus));

    auto queryPortsAvailable = telemetry->add_subcommand(
        "QueryPortsAvailable", "query ports available");
    commands.push_back(std::make_unique<QueryPortsAvailable>(
        "telemetry", "QueryPortsAvailable", queryPortsAvailable));

    auto setPortDisableFuture = telemetry->add_subcommand(
        "SetPortDisableFuture", "query ports available");
    commands.push_back(std::make_unique<SetPortDisableFuture>(
        "telemetry", "SetPortDisableFuture", setPortDisableFuture));

    auto getFabricManagerState = telemetry->add_subcommand(
        "GetFabricManagerState", "Get Fabric Manager State");
    commands.push_back(std::make_unique<GetFabricManagerState>(
        "telemetry", "GetFabricManagerState", getFabricManagerState));

    auto getPortDisableFuture = telemetry->add_subcommand(
        "GetPortDisableFuture", "query ports available");
    commands.push_back(std::make_unique<GetPortDisableFuture>(
        "telemetry", "GetPortDisableFuture", getPortDisableFuture));

    auto getPowerMode = telemetry->add_subcommand("GetPowerMode",
                                                  "Get L1 power mode");
    commands.push_back(std::make_unique<GetPowerMode>(
        "telemetry", "GetPowerMode", getPowerMode));

    auto setPowerMode = telemetry->add_subcommand("SetPowerMode",
                                                  "Set L1 power mode");
    commands.push_back(std::make_unique<SetPowerMode>(
        "telemetry", "SetPowerMode", setPowerMode));

    auto getSwitchIsolationMode = telemetry->add_subcommand(
        "GetSwitchIsolationMode", "Get Switch Isolation Mode");
    commands.push_back(std::make_unique<GetSwitchIsolationMode>(
        "telemetry", "GetSwitchIsolationMode", getSwitchIsolationMode));

    auto setSwitchIsolationMode = telemetry->add_subcommand(
        "SetSwitchIsolationMode", "Set Switch Isolation Mode");
    commands.push_back(std::make_unique<SetSwitchIsolationMode>(
        "telemetry", "SetSwitchIsolationMode", setSwitchIsolationMode));

    auto getInventoryInformation = telemetry->add_subcommand(
        "GetInventoryInformation", "get inventory information");
    commands.push_back(std::make_unique<GetInventoryInformation>(
        "telemetry", "GetInventoryInformation", getInventoryInformation));

    auto getTemperatureReading = telemetry->add_subcommand(
        "GetTemperatureReading", "get temperature reading of a sensor");
    commands.push_back(std::make_unique<GetTemperatureReading>(
        "telemetry", "GetTemperatureReading", getTemperatureReading));

    auto readThermalParameter = telemetry->add_subcommand(
        "ReadThermalParameter", "read thermal parameter a device");
    commands.push_back(std::make_unique<ReadThermalParameter>(
        "telemetry", "ReadThermalParameter", readThermalParameter));

    auto getCurrentPowerDraw = telemetry->add_subcommand(
        "GetCurrentPowerDraw", "get current power draw of a device");
    commands.push_back(std::make_unique<GetCurrentPowerDraw>(
        "telemetry", "GetCurrentPowerDraw", getCurrentPowerDraw));

    auto getMaxObservedPower = telemetry->add_subcommand(
        "GetMaxObservedPower", "get peak power observed of a device");
    commands.push_back(std::make_unique<GetMaxObservedPower>(
        "telemetry", "GetMaxObservedPower", getMaxObservedPower));

    auto getCurrentEnergyCount = telemetry->add_subcommand(
        "GetCurrentEnergyCount",
        "get current energy counter value of a device");
    commands.push_back(std::make_unique<GetCurrentEnergyCount>(
        "telemetry", "GetCurrentEnergyCount", getCurrentEnergyCount));

    auto getVoltage = telemetry->add_subcommand("GetVoltage",
                                                "get voltage of a device");
    commands.push_back(
        std::make_unique<GetVoltage>("telemetry", "GetVoltage", getVoltage));

    auto getAltitudePressure = telemetry->add_subcommand(
        "GetAltitudePressure", "get current power draw of a device");
    commands.push_back(std::make_unique<GetAltitudePressure>(
        "telemetry", "GetAltitudePressure", getAltitudePressure));

    auto getDriverInfo = telemetry->add_subcommand("GetDriverInfo",
                                                   "get Driver info");
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

    auto getEccErrorCounts = telemetry->add_subcommand("GetEccErrorCounts",
                                                       "get ECC error counts");
    commands.push_back(std::make_unique<GetEccErrorCounts>(
        "telemetry", "GetEccErrorCounts", getEccErrorCounts));

    auto setClockLimit = telemetry->add_subcommand("SetClockLimit",
                                                   "set Clock Limit");
    commands.push_back(std::make_unique<SetClockLimit>(
        "telemetry", "SetClockLimit", setClockLimit));

    auto getEDPpScalingFactors = telemetry->add_subcommand(
        "GetEDPpScalingFactors", "get programmable EDPp Scaling Factors");
    commands.push_back(std::make_unique<GetEDPpScalingFactors>(
        "telemetry", "GetEDPpScalingFactors", getEDPpScalingFactors));

    auto setEDPpScalingFactors = telemetry->add_subcommand(
        "SetEDPpScalingFactors", "set programmable EDPp Scaling Factors");
    commands.push_back(std::make_unique<SetEDPpScalingFactors>(
        "telemetry", "SetEDPpScalingFactors", setEDPpScalingFactors));

    auto queryScalarGroupTelemetry = telemetry->add_subcommand(
        "QueryScalarGroupTelemetry", "retrieve Scalar Data source for group ");
    commands.push_back(std::make_unique<QueryScalarGroupTelemetry>(
        "telemetry", "QueryScalarGroupTelemetry", queryScalarGroupTelemetry));

    auto queryAvailableAndClearableScalarGroup = telemetry->add_subcommand(
        "QueryAvailableAndClearableScalarGroup",
        "retrieve available and clearable Scalar Data source for the group");
    commands.push_back(std::make_unique<QueryAvailableAndClearableScalarGroup>(
        "telemetry", "QueryAvailableAndClearableScalarGroup",
        queryAvailableAndClearableScalarGroup));

    auto pcieFundamentalReset = telemetry->add_subcommand(
        "PcieFundamentalReset", "Assert PCIe Fundamental Reset action");
    commands.push_back(std::make_unique<PcieFundamentalReset>(
        "telemetry", "PcieFundamentalReset", pcieFundamentalReset));

    auto clearScalarDataSource = telemetry->add_subcommand(
        "ClearScalarDataSource", "Clear Scalar Data Source");
    commands.push_back(std::make_unique<ClearScalarDataSource>(
        "telemetry", "ClearScalarDataSource", clearScalarDataSource));

    auto getClockLimit = telemetry->add_subcommand(
        "GetClockLimit", "retrieve clock Limit for clockId");
    commands.push_back(std::make_unique<GetClockLimit>(
        "telemetry", "GetClockLimit", getClockLimit));

    auto setPowerLimit = telemetry->add_subcommand("SetPowerLimit",
                                                   "set Power Limit");
    commands.push_back(std::make_unique<SetPowerLimit>(
        "telemetry", "SetPowerLimit", setPowerLimit));
    auto getPowerLimit = telemetry->add_subcommand("GetPowerLimit",
                                                   "get Power Limit");
    commands.push_back(std::make_unique<GetPowerLimit>(
        "telemetry", "getPowerLimit", getPowerLimit));

    auto getCurrClockFreq = telemetry->add_subcommand(
        "GetCurrClockFreq", "get current clock frequency of GPU");
    commands.push_back(std::make_unique<GetCurrClockFreq>(
        "telemetry", "GetCurrClockFreq", getCurrClockFreq));

    auto getAccumGpuUtilTime = telemetry->add_subcommand(
        "GetAccumGpuUtilTime",
        "Get Accumulated GPU Utilization Time Context/SM");
    commands.push_back(std::make_unique<GetAccumGpuUtilTime>(
        "telemetry", "GetAccumGpuUtilTime", getAccumGpuUtilTime));

    auto getProcessorThrottleReason = telemetry->add_subcommand(
        "GetProcessorThrottleReason", "Get Processor Throttle Reason");
    commands.push_back(std::make_unique<GetProcessorThrottleReason>(
        "telemetry", "GetProcessorThrottleReason", getProcessorThrottleReason));

    auto getRowRemapState = telemetry->add_subcommand("GetRowRemapState",
                                                      "get Row Remap State");
    commands.push_back(std::make_unique<GetRowRemapState>(
        "telemetry", "GetRowRemapState", getRowRemapState));

    auto getRowRemappingCounts = telemetry->add_subcommand(
        "GetRowRemappingCounts", "get Row Remapping Error Counts");
    commands.push_back(std::make_unique<GetRowRemappingCounts>(
        "telemetry", "GetRowRemappingCounts", getRowRemappingCounts));

    auto getRowRemapAvailability = telemetry->add_subcommand(
        "GetRowRemapAvailability", "get Row Remapping Availability ");
    commands.push_back(std::make_unique<GetRowRemapAvailability>(
        "telemetry", "GetRowRemapAvailability", getRowRemapAvailability));

    auto getMemoryCapacityUtil = telemetry->add_subcommand(
        "GetMemoryCapacityUtil", "Get memory Capacity Capacity Utilization");
    commands.push_back(std::make_unique<GetMemoryCapacityUtil>(
        "telemetry", "GetMemoryCapacityUtil", getMemoryCapacityUtil));

    auto getCurrentUtilization = telemetry->add_subcommand(
        "GetCurrentUtilization", "Get Current Capacity Utilization");
    commands.push_back(std::make_unique<GetCurrentUtilization>(
        "telemetry", "GetCurrentUtilization", getCurrentUtilization));

    auto getClockOutputEnableState = telemetry->add_subcommand(
        "GetClockOutputEnableState", "get clock output enable state");
    commands.push_back(std::make_unique<GetClockOutputEnableState>(
        "telemetry", "GetClockOutputEnableState", getClockOutputEnableState));

    auto queryAggregatedGPMMetrics = telemetry->add_subcommand(
        "QueryAggregatedGPMMetrics", "Query Aggregated GPM Metrics");
    commands.push_back(std::make_unique<QueryAggregatedGPMMetrics>(
        "telemetry", "QueryAggregatedGPMMetrics", queryAggregatedGPMMetrics));

    auto queryPerInstanceGPMMetrics = telemetry->add_subcommand(
        "QueryPerInstanceGPMMetrics", "Query Per-instance GPM Metrics");
    commands.push_back(std::make_unique<QueryPerInstanceGPMMetrics>(
        "telemetry", "QueryPerInstanceGPMMetrics", queryPerInstanceGPMMetrics));

    auto getViolationDuration = telemetry->add_subcommand(
        "GetViolationDuration", "get processor throttle duration");
    commands.push_back(std::make_unique<GetViolationDuration>(
        "telemetry", "GetViolationDuration", getViolationDuration));
}

} // namespace telemetry

} // namespace nsmtool
