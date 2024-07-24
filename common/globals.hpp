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

#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

using std::filesystem::path;

const path chassisInventoryBasePath =
    "/xyz/openbmc_project/inventory/system/chassis";
const path processorsInventoryBasePath =
    "/xyz/openbmc_project/inventory/system/processors/";
const path fabricsInventoryBasePath =
    "/xyz/openbmc_project/inventory/system/fabrics";
const path firmwareInventoryBasePath = "/xyz/openbmc_project/software";
const path sotwareInventoryBasePath = "/xyz/openbmc_project/inventory_software";
const path buildTypeBasePath = "/xyz/openbmc_project/inventory/system/chassis";

const std::vector<uint8_t> supportedMessageTypes = {0, 1, 2, 3, 4, 5, 6};

const std::string nullDate = "0000-00-00T00:00:00Z";
