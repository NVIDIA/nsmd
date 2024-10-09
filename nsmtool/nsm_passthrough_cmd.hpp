/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "utils.hpp"

#include <CLI/CLI.hpp>
#include <sdbusplus/bus.hpp>

#include <optional>
#include <string>

namespace nsmtool
{
namespace passthrough
{

enum NSMDevID
{
    NSM_DEV_ID_GPU = 0,
    NSM_DEV_ID_SWITCH = 1,
    NSM_DEV_ID_PCIE_BRIDGE = 2,
    NSM_DEV_ID_BASEBOARD = 3,
};

class SendNSMCommand
{
  public:
    SendNSMCommand(CLI::App* app);
    void execute(const std::string& targetType, uint8_t targetInstanceId,
                 uint8_t messageType, uint8_t commandCode,
                 const std::string& filePath);

  private:
    void getMatchingFruDeviceObjectPath(
        const std::string& targetType, uint8_t targetInstanceId,
        std::function<void(const std::string&)> callback);
    void getDeviceType(const std::string& objectPath,
                       const std::string& targetType, uint8_t targetInstanceId,
                       std::function<void(const std::string&)> callback);
    void getInstanceNumber(const std::string& objectPath,
                           uint8_t targetInstanceId,
                           std::function<void(const std::string&)> callback);
};

class GetCommandStatus
{
  public:
    GetCommandStatus(CLI::App* app);
    void execute(const std::string& objectPath);

  private:
    std::string objectPath;
};

class WaitCommandStatusComplete
{
  public:
    WaitCommandStatusComplete(CLI::App* app);
    void execute(const std::string& objectPath);

  private:
    std::string objectPath;
};

class GetNSMResponse
{
  public:
    GetNSMResponse(CLI::App* app);
    void execute(const std::string& objectPath);

  private:
    std::string objectPath;
};

class GetDebugInfoFromFD
{
  public:
    GetDebugInfoFromFD(CLI::App* app);
    void execute(const std::string& objectPath);

  private:
    std::string objectPath;
};

class GetLogInfoFromFD
{
  public:
    GetLogInfoFromFD(CLI::App* app);
    void execute(const std::string& objectPath);

  private:
    std::string objectPath;
};
void registerCommand(CLI::App& app);
} // namespace passthrough
} // namespace nsmtool
