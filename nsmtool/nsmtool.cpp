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
#include "nsm_discovery_cmd.hpp"
#include "nsm_telemetry_cmd.hpp"

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
    auto raw =
        app.add_subcommand("raw", "send a raw request and print response");
    commands.push_back(std::make_unique<RawOp>("raw", "raw", raw));
}

} // namespace raw
} // namespace nsmtool

int main(int argc, char** argv)
{
    try
    {
        CLI::App app{"NSM requester tool for OpenBMC"};
        app.require_subcommand(1)->ignore_case();

        nsmtool::raw::registerCommand(app);
        nsmtool::discovery::registerCommand(app);
        nsmtool::telemetry::registerCommand(app);

        CLI11_PARSE(app, argc, argv);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return -1;
    }
}