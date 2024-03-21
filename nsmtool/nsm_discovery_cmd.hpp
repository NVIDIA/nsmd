// NSM: Nvidia Message type - Device capability discovery [Type 0]

#pragma once

#include "base.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

namespace nsmtool
{
using ordered_json = nlohmann::ordered_json;

namespace discovery
{
void registerCommand(CLI::App& app);
} // namespace discovery

} // namespace nsmtool
