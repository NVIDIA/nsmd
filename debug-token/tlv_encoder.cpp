/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION &
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

#include "tlv.h"

#include <endian.h>

#include <cstring>
#include <format>
#include <limits>
#include <stdexcept>

namespace debug_token::tlv_encoder
{

Item::Item(uint16_t type, const std::vector<uint8_t>& input)
{
    if (input.size() > std::numeric_limits<uint16_t>::max())
    {
        throw std::runtime_error(std::format(
            "TLV item value size is greater than the maximum value size - size: {}",
            input.size()));
    }
    data = std::vector<uint8_t>();
    data.reserve(sizeof(ItemHeader) + input.size());
    auto header = std::make_shared<ItemHeader>();
    header->type = htole16(type);
    header->size = htole16(input.size());
    data.resize(sizeof(ItemHeader));
    std::memcpy(data.data(), header.get(), sizeof(ItemHeader));
    data.insert(data.end(), input.begin(), input.end());
}

size_t Item::getTotalSize() const
{
    return data.size();
}

const std::vector<uint8_t>& Item::getValue() const
{
    return data;
}

Structure::Structure()
{
    header = std::make_shared<StructureHeader>();
    std::memcpy(header->identifier, TLV_IDENTIFIER, sizeof(TLV_IDENTIFIER));
    header->versionMajor = htole16(1);
    header->versionMinor = htole16(0);
    header->size = htole32(0);
    std::memset(header->reserved, 0, sizeof(header->reserved));
}

void Structure::setVersion(uint16_t major, uint16_t minor)
{
    header->versionMajor = htole16(major);
    header->versionMinor = htole16(minor);
}

void Structure::add(uint16_t type, const std::vector<uint8_t>& input)
{
    if (data.contains(type))
    {
        throw std::runtime_error(
            std::format("Duplicate TLV data type - type: {}", type));
    }
    try
    {
        data.emplace(type, Item(type, input));
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error(std::format(
            "Failed to add TLV data - type: {}, error: {}", type, e.what()));
    }
}

void Structure::add(uint16_t type, const std::vector<uint32_t>& input)
{
    if (data.contains(type))
    {
        throw std::runtime_error(
            std::format("Duplicate TLV data type - type: {}", type));
    }
    try
    {
        std::vector<uint8_t> inputU8;
        inputU8.reserve(input.size() * sizeof(uint32_t));
        for (const auto& value : input)
        {
            uint32_t leValue = htole32(value);
            inputU8.insert(inputU8.end(), reinterpret_cast<uint8_t*>(&leValue),
                           reinterpret_cast<uint8_t*>(&leValue) +
                               sizeof(leValue));
        }
        data.emplace(type, Item(type, inputU8));
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error(std::format(
            "Failed to add TLV data - type: {}, error: {}", type, e.what()));
    }
}

void Structure::add(uint16_t type, uint8_t value)
{
    add(type, std::vector<uint8_t>{value});
}

void Structure::add(uint16_t type, uint16_t value)
{
    uint16_t leValue = htole16(value);
    add(type, std::vector<uint8_t>(reinterpret_cast<uint8_t*>(&leValue),
                                   reinterpret_cast<uint8_t*>(&leValue) +
                                       sizeof(leValue)));
}

void Structure::add(uint16_t type, uint32_t value)
{
    uint32_t leValue = htole32(value);
    add(type, std::vector<uint8_t>(reinterpret_cast<uint8_t*>(&leValue),
                                   reinterpret_cast<uint8_t*>(&leValue) +
                                       sizeof(leValue)));
}

std::vector<uint8_t> Structure::encode()
{
    uint64_t totalSize = 0;
    for (const auto& [_, item] : data)
    {
        totalSize += item.getTotalSize();
        if (totalSize > std::numeric_limits<uint32_t>::max())
        {
            throw std::overflow_error("TLV structure size exceeds max value");
        }
    }
    header->size = static_cast<uint32_t>(totalSize);

    std::vector<uint8_t> result;
    result.reserve(sizeof(StructureHeader) + header->size);
    result.resize(sizeof(StructureHeader));
    header->size = htole32(header->size);
    std::memcpy(result.data(), header.get(), sizeof(StructureHeader));
    for (const auto& [type, value] : data)
    {
        result.insert(result.end(), value.getValue().begin(),
                      value.getValue().end());
    }
    return result;
}

} // namespace debug_token::tlv_encoder
