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

// Include the .cpp directly so that its anonymous-namespace `commands`
// vector and all class definitions are in this translation unit.
#include "base.h"
#include "device-capability-discovery.h"

#include "../nsm_discovery_cmd.cpp"

#include <gtest/gtest.h>

// Discovery commands (after registerCommand) in order:
//   [0]  Ping
//   [1]  GetSupportedMessageTypes
//   [2]  GetSupportedCommandCodes
//   [3]  QueryDeviceIdentification
//   [4]  GetHistogramFormat
//   [5]  GetHistogramData
//   [6]  GetDeviceCapabilitiesV2
//   [7]  GetGpioState

namespace nsmtool
{
namespace discovery
{

// ---- Helpers ----------------------------------------------------------------

static void setupDiscoveryCommands(CLI::App& app)
{
    commands.clear();
    registerCommand(app);
}

static void parseSubcmdArgs(CLI::App& app, const std::string& cmdName,
                            std::vector<std::string> extraArgs = {})
{
    auto* discSub = app.get_subcommand("discovery");
    if (!discSub)
        return;
    auto* leafSub = discSub->get_subcommand(cmdName);
    if (!leafSub)
        return;

    std::vector<std::string> args = {"-m", "1"};
    args.insert(args.end(), extraArgs.begin(), extraArgs.end());
    std::reverse(args.begin(), args.end());
    try
    {
        leafSub->parse(args);
    }
    catch (...)
    {}
}

// ---- Ping -------------------------------------------------------------------

TEST(NsmDiscoveryCmdParse, Ping_ParseResponseError)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "Ping");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ---- GetSupportedMessageTypes -----------------------------------------------

TEST(NsmDiscoveryCmdParse, GetSupportedMessageTypes_ParseResponseError)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetSupportedMessageTypes");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// ---- GetSupportedCommandCodes -----------------------------------------------

TEST(NsmDiscoveryCmdParse, GetSupportedCommandCodes_ParseResponseError)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetSupportedCommandCodes", {"-t", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryDeviceIdentification ----------------------------------------------

TEST(NsmDiscoveryCmdParse, QueryDeviceIdentification_ParseResponseError)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// ---- GetHistogramFormat -----------------------------------------------------

TEST(NsmDiscoveryCmdParse, GetHistogramFormat_ParseResponseError)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ---- GetHistogramData -------------------------------------------------------

TEST(NsmDiscoveryCmdParse, GetHistogramData_ParseResponseError)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// ---- GetDeviceCapabilitiesV2 ------------------------------------------------

TEST(NsmDiscoveryCmdParse, GetDeviceCapabilitiesV2_ParseResponseError)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetDeviceCapabilitiesV2");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// ---- GetGpioState -----------------------------------------------------------

TEST(NsmDiscoveryCmdParse, GetGpioState_ParseResponseError)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetGpioState", {"-o", "0", "-l", "8"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// Covers the success path including the inner-loop break at L789:
// offset=0, length=1 (one GPIO), gpio_values={0x01} → bit 0 processed (false
// branch of the break condition), bit 1 >= length → break (true branch).
TEST(NsmDiscoveryCmdParse, GetGpioState_ParseResponseSuccess_InnerBreak)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetGpioState", {"-o", "0", "-l", "1"});

    uint8_t gpioValues[1] = {0x01};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_gpio_state_resp) - 1 +
                                 sizeof(gpioValues),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_gpio_state_resp(0, NSM_SUCCESS, ERR_NULL,
                               /*offset=*/0, /*length=*/1, gpioValues,
                               sizeof(gpioValues), msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

} // namespace discovery
} // namespace nsmtool
