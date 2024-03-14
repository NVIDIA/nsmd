// NSM: Nvidia Message type
//           - Network Ports            [Type 1]
//           - PCI links                [Type 2]
//           - Platform environments    [Type 3]

#pragma once

#include <CLI/CLI.hpp>

namespace nsmtool
{

namespace telemetry
{
void registerCommand(CLI::App& app);
} // namespace telemetry

} // namespace nsmtool