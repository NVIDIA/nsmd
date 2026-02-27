/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <CLI/CLI.hpp>

namespace nsmtool
{
namespace dbus_cmd
{

/**
 * @brief Register DBus-mode commands
 *
 * Registers commands that execute via DBus using EID-based targeting.
 * These commands use the existing com.nvidia.Protocol.NSM.Raw.SendRequest
 * interface and require device discovery via FRU objects.
 */
void registerCommand(CLI::App& app);

} // namespace dbus_cmd
} // namespace nsmtool
