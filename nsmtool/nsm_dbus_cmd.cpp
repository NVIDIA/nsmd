/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nsm_dbus_cmd.hpp"

#include "base.h"

#include "nsm_dbus_client.hpp"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/bus.hpp>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace nsmtool
{
namespace dbus_cmd
{

using ordered_json = nlohmann::ordered_json;

/**
 * @brief Execute raw NSM command via DBus
 */
class RawCommand
{
  public:
    RawCommand(CLI::App* app)
    {
        auto* rawCmd = app->add_subcommand("raw",
                                           "Execute raw NSM command via DBus");

        rawCmd
            ->add_option("-m,--mctp_eid", eid, "Target device MCTP Endpoint ID")
            ->required();
        rawCmd
            ->add_option("-t,--message_type", messageType,
                         "NSM Message Type (0-7)")
            ->required();
        rawCmd
            ->add_option("-c,--command_code", commandCode,
                         "NSM Command Code (0-255)")
            ->required();
        rawCmd->add_option(
            "-p,--payload", payloadStr,
            "Request payload as hex string (e.g., 0x010203 or 010203)");
        rawCmd
            ->add_option("-f,--msg_format_version", msgFormatVersion,
                         "NSM message format version (1 or 2, default: 1)")
            ->check(CLI::Range(1, 2));
        rawCmd->add_option("-T,--timeout", timeoutSeconds,
                           "Timeout in seconds (default: 30)");
        rawCmd->add_flag("-L,--is_long_running", isLongRunning,
                         "Mark as long-running command");
        rawCmd->add_flag("-v,--verbose", verbose, "Verbose output");

        rawCmd->callback([this]() { execute(); });
    }

    void execute()
    {
        try
        {
            std::vector<uint8_t> payload;
            if (!payloadStr.empty())
            {
                payload = parseHexString(payloadStr);
                if (payload.empty() && !payloadStr.empty())
                {
                    std::cerr << "Error: Invalid payload hex string"
                              << std::endl;
                    std::exit(1);
                }
            }

            auto bus = sdbusplus::bus::new_default();
            dbus::NsmDbusClient client(bus);

            auto response = client.executeCommand(
                eid, messageType, commandCode, payload, msgFormatVersion,
                isLongRunning, timeoutSeconds, verbose);

            if (!response.empty())
            {
                // Response format: [CompletionCode, Data...]
                // Byte 0: Completion code
                // Bytes 1+: Response data

                uint8_t cc = response[0];

                ordered_json result;
                result["CommandCode"] = commandCode;
                result["CompletionCode"] = cc;

                std::vector<int> dataArray;
                for (size_t i = 1; i < response.size(); ++i)
                {
                    dataArray.push_back(static_cast<int>(response[i]));
                }
                result["Data"] = dataArray;

                result["MessageType"] = messageType;

                std::cout << result.dump(2) << std::endl;
            }
            else
            {
                std::cerr << "Error: Empty response received" << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

  private:
    std::vector<uint8_t> parseHexString(const std::string& str)
    {
        std::vector<uint8_t> result;
        std::string cleanStr = str;

        if (cleanStr.length() >= 2 && cleanStr.substr(0, 2) == "0x")
        {
            cleanStr = cleanStr.substr(2);
        }

        if (cleanStr.find_first_not_of("0123456789abcdefABCDEF") !=
            std::string::npos)
        {
            return result;
        }

        if (cleanStr.length() % 2 != 0)
        {
            cleanStr = "0" + cleanStr;
        }

        for (size_t i = 0; i < cleanStr.length(); i += 2)
        {
            try
            {
                uint8_t byte = static_cast<uint8_t>(
                    std::stoi(cleanStr.substr(i, 2), nullptr, 16));
                result.push_back(byte);
            }
            catch (const std::exception&)
            {
                result.clear();
                return result;
            }
        }

        return result;
    }

    void printHexDump(const std::vector<uint8_t>& data)
    {
        const size_t bytesPerLine = 16;

        for (size_t i = 0; i < data.size(); i += bytesPerLine)
        {
            std::cout << std::setw(8) << std::setfill('0') << std::hex << i
                      << "  ";

            for (size_t j = 0; j < bytesPerLine; ++j)
            {
                if (i + j < data.size())
                {
                    std::cout << std::setw(2) << std::setfill('0') << std::hex
                              << static_cast<int>(data[i + j]) << " ";
                }
                else
                {
                    std::cout << "   ";
                }

                if (j == 7)
                {
                    std::cout << " ";
                }
            }

            std::cout << " |";
            for (size_t j = 0; j < bytesPerLine && i + j < data.size(); ++j)
            {
                uint8_t byte = data[i + j];
                if (byte >= 32 && byte <= 126)
                {
                    std::cout << static_cast<char>(byte);
                }
                else
                {
                    std::cout << ".";
                }
            }
            std::cout << "|" << std::dec << std::endl;
        }
    }

    uint8_t eid = 0;
    uint8_t messageType = 0;
    uint8_t commandCode = 0;
    std::string payloadStr;
    uint8_t msgFormatVersion = 1; // Default to version 1
    int timeoutSeconds = 30;
    bool isLongRunning = false;
    bool verbose = false;
};

void registerCommand(CLI::App& app)
{
    auto* dbusApp = app.add_subcommand(
        "dbus", "Execute NSM commands via DBus (EID-based targeting)");
    dbusApp->require_subcommand(1);

    static std::unique_ptr<RawCommand> rawCommand;
    rawCommand = std::make_unique<RawCommand>(dbusApp);
}

} // namespace dbus_cmd
} // namespace nsmtool
