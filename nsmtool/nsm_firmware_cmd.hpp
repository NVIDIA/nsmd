/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved. 
 * SPDX-License-Identifier: Apache-2.0
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

// NSM: Nvidia Message type - Device diagnostic [Type 4]

#pragma once

#include "base.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

namespace nsmtool
{
using ordered_json = nlohmann::ordered_json;

namespace firmware
{
void registerCommand(CLI::App& app);
} // namespace firmware

} // namespace nsmtool
