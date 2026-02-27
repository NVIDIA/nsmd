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

#include "cmd_helper.hpp"
#include "nsm_config_cmd.hpp"
#include "nsm_dbus_cmd.hpp"
#include "nsm_diag_cmd.hpp"
#include "nsm_discovery_cmd.hpp"
#include "nsm_firmware_cmd.hpp"
#include "nsm_passthrough_cmd.hpp"
#include "nsm_telemetry_cmd.hpp"

#include <dbus/dbus.h>

#include <CLI/CLI.hpp>

namespace nsmtool
{

namespace raw
{

using namespace nsmtool::helper;

namespace
{
std::vector<std::unique_ptr<CommandInterface>> commands;
}

class RawOp : public CommandInterface
{
  public:
    ~RawOp() = default;
    RawOp() = delete;
    RawOp(const RawOp&) = delete;
    RawOp(RawOp&&) = default;
    RawOp& operator=(const RawOp&) = delete;
    RawOp& operator=(RawOp&&) = default;

    explicit RawOp(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-d,--data", rawData, "raw data")
            ->required()
            ->expected(-3);
    }
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override

    {
        return {NSM_SUCCESS, rawData};
    }

    void parseResponseMsg(nsm_msg* /* responsePtr */,
                          size_t /* payloadLength */) override
    {}

  private:
    std::vector<uint8_t> rawData;
};

void registerCommand(CLI::App& app)
{
    auto raw = app.add_subcommand("raw",
                                  "send a raw request and print response");
    commands.push_back(std::make_unique<RawOp>("raw", "raw", raw));
}

} // namespace raw
} // namespace nsmtool

bool isNsmdServiceActive()
{
    DBusConnection* conn = dbus_bus_get(DBUS_BUS_SYSTEM, nullptr);
    if (!conn)
        return false;

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "NameHasOwner");
    if (!msg)
    {
        dbus_connection_unref(conn);
        return false;
    }

    const char* service = "xyz.openbmc_project.NSM";
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &service,
                             DBUS_TYPE_INVALID);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg,
                                                                   -1, nullptr);
    dbus_message_unref(msg);
    dbus_connection_unref(conn);

    if (!reply)
        return false;

    dbus_bool_t hasOwner = FALSE;
    dbus_message_get_args(reply, nullptr, DBUS_TYPE_BOOLEAN, &hasOwner,
                          DBUS_TYPE_INVALID);
    dbus_message_unref(reply);

    return hasOwner == TRUE;
}

int main(int argc, char** argv)
{
    try
    {
        CLI::App app{"NSM requester tool for OpenBMC"};

        // Check if "dbus" subcommand is used
        // Since no global options are defined before subcommand registration,
        // the subcommand is always at argv[1]
        bool useDbusMode = (argc > 1 && std::string(argv[1]) == "dbus");

        // Check if nsmd is running - only block for non-dbus modes
        if (!useDbusMode && isNsmdServiceActive())
        {
            std::cerr
                << "Error: NSM daemon (nsmd) is currently active.\n\n"
                << "Concurrent use of nsmtool is not permitted while nsmd is running,"
                << " as this would interrupt crucial NSM operations.\n\n"
                << "To proceed with nsmtool, please Stop the nsmd service first: systemctl stop nsmd\n\n"
                << "Note: nsmtool is intended for internal use. For production or "
                << "automated workflows, out-of-band methods are recommended.\n\n"
                << "Tip: Use 'nsmtool dbus' commands to interact via DBus when nsmd is running.\n";
            return -1;
        }

        // For DBus mode, nsmd MUST be running
        if (useDbusMode && !isNsmdServiceActive())
        {
            std::cerr
                << "Error: NSM daemon (nsmd) is not running.\n\n"
                << "DBus mode requires nsmd to be active.\n"
                << "Please start the nsmd service: systemctl start nsmd\n";
            return -1;
        }

        app.require_subcommand(1)->ignore_case();

        // Register DBus commands (requires nsmd to be running)
        nsmtool::dbus_cmd::registerCommand(app);

        // Register direct access commands (requires nsmd to be stopped)
        nsmtool::raw::registerCommand(app);
        nsmtool::config::registerCommand(app);
        nsmtool::diag::registerCommand(app);
        nsmtool::firmware::registerCommand(app);
        nsmtool::discovery::registerCommand(app);
        nsmtool::telemetry::registerCommand(app);
        nsmtool::passthrough::registerCommand(app);

        CLI11_PARSE(app, argc, argv);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return -1;
    }
}
