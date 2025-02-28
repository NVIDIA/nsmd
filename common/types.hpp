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

#include <stdint.h>

#include <sdbusplus/message/types.hpp>

#include <bitset>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using eid_t = uint8_t;
using uuid_t = std::string;
using Request = std::vector<uint8_t>;
using Response = std::vector<uint8_t>;
using Command = uint8_t;
using NsmType = uint8_t;

using MctpMedium = std::string;
using MctpBinding = std::string;
using NetworkId = uint8_t;
using MctpInfo = std::tuple<eid_t, uuid_t, MctpMedium, NetworkId, MctpBinding>;
using MctpInfos = std::vector<MctpInfo>;
using VendorIANA = uint32_t;

namespace nsm
{
using InventoryPropertyId = uint8_t;
using InventoryPropertyData =
    std::variant<bool, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t,
                 uint64_t, int64_t, float, std::string, std::vector<uint8_t>>;
using InventoryProperties =
    std::map<InventoryPropertyId, InventoryPropertyData>;

enum PollingState
{
    POLL_PRIORITY,
    POLL_NON_PRIORITY,
};

} // namespace nsm

namespace dbus
{

using ObjectPath = std::string;
using Service = std::string;
using Interface = std::string;
using Interfaces = std::vector<std::string>;
using Property = std::string;
using PropertyType = std::string;
using Value = std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
                           int64_t, uint64_t, double, std::string,
                           std::vector<uint8_t>, sdbusplus::message::unix_fd>;

using PropertyMap = std::map<Property, Value>;
using InterfaceMap = std::map<Interface, PropertyMap>;
using ObjectValueTree = std::map<sdbusplus::message::object_path, InterfaceMap>;

typedef struct _pathAssociation
{
    std::string forward;
    std::string reverse;
    std::string path;
} PathAssociation;

} // namespace dbus
