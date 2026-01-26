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
#include "types.h"

#include <endian.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>

namespace debug_token::tlv_decoder
{

// Explicit instantiation of template functions for supported value types
template uint8_t Item::getValue() const;
template uint16_t Item::getValue() const;
template uint32_t Item::getValue() const;
template uint64_t Item::getValue() const;
template std::vector<uint8_t> Item::getValue() const;
template std::vector<uint16_t> Item::getValue() const;
template std::vector<uint32_t> Item::getValue() const;
template std::vector<uint64_t> Item::getValue() const;

Item::Item(const std::span<const uint8_t> input)
{
    if (input.size() < sizeof(ItemHeader))
    {
        throw std::runtime_error(std::format(
            "TLV item input data is shorter than the header - size: {}",
            input.size()));
    }
    header = std::make_shared<ItemHeader>();
    std::memcpy(header.get(), input.data(), sizeof(ItemHeader));
    auto dataLength = static_cast<size_t>(le16toh(header->size));
    if (dataLength > input.size() - sizeof(ItemHeader))
    {
        throw std::runtime_error(
            std::format("TLV item input data is too short - type: {}, size: {}",
                        le16toh(header->type), le16toh(header->size)));
    }
    data =
        std::vector<uint8_t>(input.begin() + sizeof(ItemHeader),
                             input.begin() + sizeof(ItemHeader) + dataLength);
}

uint16_t Item::getType() const
{
    return static_cast<uint16_t>(le16toh(header->type));
}

size_t Item::getTotalSize() const
{
    return static_cast<size_t>(sizeof(ItemHeader) + getValueSize());
}

size_t Item::getValueSize() const
{
    return static_cast<size_t>(le16toh(header->size));
}

const std::vector<uint8_t>& Item::getRawValue() const
{
    return data;
}

template <VectorType T>
T Item::getValue() const
{
    if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
    {
        return data;
    }
    if (data.size() % sizeof(typename T::value_type) != 0)
    {
        throw std::runtime_error(std::format(
            "TLV item vector data size is not a multiple of the type size"
            " - type: {}, expected: {}, actual: {}",
            typeid(typename T::value_type).name(),
            sizeof(typename T::value_type), data.size()));
    }
    T result;
    result.reserve(data.size() / sizeof(typename T::value_type));
    for (size_t i = 0; i < data.size(); i += sizeof(typename T::value_type))
    {
        std::unique_ptr<typename T::value_type> value =
            std::make_unique<typename T::value_type>();
        std::memcpy(value.get(), data.data() + i,
                    sizeof(typename T::value_type));
        if (std::is_same_v<T, std::vector<uint16_t>>)
        {
            result.push_back(le16toh(*value));
        }
        if (std::is_same_v<T, std::vector<uint32_t>>)
        {
            result.push_back(le32toh(*value));
        }
        if (std::is_same_v<T, std::vector<uint64_t>>)
        {
            result.push_back(le64toh(*value));
        }
    }
    return result;
}

template <std::unsigned_integral T>
T Item::getValue() const
{
    if (sizeof(T) != data.size())
    {
        throw std::runtime_error(
            std::format("TLV item data size does not match the type size"
                        " - type: {}, expected: {}, actual: {}",
                        typeid(T).name(), sizeof(T), data.size()));
    }
    if (std::is_same_v<T, uint8_t>)
    {
        return data[0];
    }
    std::unique_ptr<T> result = std::make_unique<T>();
    std::memcpy(result.get(), data.data(), data.size());
    if (std::is_same_v<T, uint16_t>)
    {
        return le16toh(*result);
    }
    if (std::is_same_v<T, uint32_t>)
    {
        return le32toh(*result);
    }
    if (std::is_same_v<T, uint64_t>)
    {
        return le64toh(*result);
    }
    return *result;
}

std::string Item::getTypeName(uint16_t type)
{
    switch (type)
    {
        case types::Common::DeviceType:
            return "DeviceType";
        case types::Common::ChallengeNonce:
            return "ChallengeNonce";
        case types::Common::DeviceSerialNumber:
            return "DeviceSerialNumber";
        case types::Common::DeviceSerialNumberArray:
            return "DeviceSerialNumberArray";
        case types::Common::FirmwareVersion:
            return "FirmwareVersion";
        case types::Common::AgentVersion:
            return "AgentVersion";
        case types::Common::LifecycleState:
            return "LifecycleState";
        case types::Common::TokenIdentifier:
            return "TokenIdentifier";
        case types::Common::TokenType:
            return "TokenType";
        case types::Common::TokenConfig:
            return "TokenConfig";
        case types::Common::NvidiaSignature:
            return "NvidiaSignature";
        case types::Common::OemSignature:
            return "OemSignature";
        case types::Common::InstallationStatus:
            return "InstallationStatus";
        case types::Common::ProcessingStatus:
            return "ProcessingStatus";
        case types::Common::SkuInformation:
            return "SkuInformation";
        case types::Common::NvidiaRatchet:
            return "NvidiaRatchet";
        case types::Common::OemRatchet:
            return "OemRatchet";
        case types::Common::ValidityCounter:
            return "ValidityCounter";
        case types::Common::CertificateChain:
            return "CertificateChain";
        case types::Common::MeasurementTranscript:
            return "MeasurementTranscript";
        case types::Common::DeviceId:
            return "DeviceId";
        case types::Common::TokenTypeSubtypeList:
            return "TokenTypeSubtypeList";
        case types::Common::Payload:
            return "Payload";
        case types::Common::LegacyToken:
            return "LegacyToken";
        case types::GPU::FeatureMask:
            return "GPUFeatureMask";
        case types::GPU::ChipId:
            return "GPUChipId";
        case types::NBU::KeypairUUID:
            return "NBUKeypairUUID";
        case types::NBU::PSID:
            return "NBUPSID";
        case types::NBU::FileDeviceUnique:
            return "NBUFileDeviceUnique";
        case types::BMCIRoT::TokenVersion:
            return "BMCIRoTTokenVersion";
        case types::BMCIRoT::NvidiaSignatureAlgorithm:
            return "BMCIRoTNvidiaSignatureAlgorithm";
        default:
            return std::format("UnknownType(0x{:04X})", type);
    }
}

Structure::Structure(const std::vector<uint8_t>& input)
{
    decode(input);
}

void Structure::decode(const std::vector<uint8_t>& input)
{
    if (input.size() < sizeof(StructureHeader))
    {
        throw std::runtime_error(std::format(
            "TLV structure input data is shorter than the header - size: {}",
            input.size()));
    }
    header = std::make_shared<StructureHeader>();
    std::memcpy(header.get(), input.data(), sizeof(StructureHeader));
    if (std::memcmp(header->identifier, TLV_IDENTIFIER, 4) != 0)
    {
        throw std::runtime_error(std::format(
            "Invalid TLV identifier - identifier: {:02X}{:02X}{:02X}{:02X}",
            header->identifier[0], header->identifier[1], header->identifier[2],
            header->identifier[3]));
    }
    auto expectedSize = static_cast<size_t>(le32toh(header->size)) +
                        sizeof(StructureHeader);
    if (expectedSize != input.size())
    {
        throw std::runtime_error(std::format(
            "Invalid TLV header data size - expected: {}, actual: {}",
            expectedSize, input.size()));
    }
    auto currentItr = input.begin() + sizeof(StructureHeader);
    while (currentItr < input.end())
    {
        try
        {
            auto element = Item(std::span{currentItr, input.end()});
            auto type = element.getType();
            auto elementSize = element.getTotalSize();
            if (data.contains(type))
            {
                throw std::runtime_error(
                    std::format("Duplicate TLV data type - type: {}", type));
            }
            data.insert({type, std::move(element)});
            currentItr += elementSize;
        }
        catch (const std::runtime_error& e)
        {
            throw std::runtime_error(
                std::format("Invalid TLV data - {}", e.what()));
        }
    }
    if (data.empty())
    {
        throw std::runtime_error("TLV structure input data is empty");
    }
}

std::pair<uint16_t, uint16_t> Structure::getVersion() const
{
    return std::make_pair(le16toh(header->versionMajor),
                          le16toh(header->versionMinor));
}

std::vector<uint16_t> Structure::getTypes() const
{
    std::vector<uint16_t> types;
    std::transform(
        data.begin(), data.end(), std::back_inserter(types),
        [](const std::pair<uint16_t, Item>& pair) { return pair.first; });
    return types;
}

const Item& Structure::get(uint16_t type) const
{
    if (data.contains(type))
    {
        return data.at(type);
    }
    throw std::runtime_error(
        std::format("TLV item not found - type: {}", type));
}

} // namespace debug_token::tlv_decoder
