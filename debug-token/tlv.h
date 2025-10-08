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

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace debug_token
{

constexpr uint8_t TLV_IDENTIFIER[4] = {0x54, 0x4C, 0x56, 0x31};

/**
 * @brief Header structure for TLV (Type-Length-Value) containers
 *
 * This structure defines the header format for TLV containers, including
 * version information and size metadata. The structure is packed to ensure
 * consistent binary layout.
 */
#pragma pack(1)
struct StructureHeader
{
    uint8_t identifier[4];
    uint16_t versionMajor;
    uint16_t versionMinor;
    uint32_t size;
    uint8_t reserved[20];
};
#pragma pack()

/**
 * @brief Header structure for individual TLV items
 *
 * This structure defines the header format for individual TLV items,
 * containing type and size information. The structure is packed to ensure
 * consistent binary layout.
 */
#pragma pack(1)
struct ItemHeader
{
    uint16_t type;
    uint16_t size;
};
#pragma pack()

namespace tlv_decoder
{

/**
 * @brief Concept for vector-like types that can be used with TLV decoder
 *
 * This concept ensures that a type has iterator support and can be used
 * as a container for decoded TLV data.
 */
template <typename T>
concept VectorType = requires(T t) {
                         typename T::iterator;
                         { t.begin() } -> std::same_as<typename T::iterator>;
                         { t.end() } -> std::same_as<typename T::iterator>;
                     };

/**
 * @brief Represents a single TLV item for decoding operations
 *
 * This class handles the parsing and extraction of data from individual
 * TLV items, providing type-safe access to the contained data.
 */
class Item
{
  public:
    /**
     * @brief Construct a TLV item from input data
     *
     * @param input Span containing the TLV item data
     */
    Item(const std::span<const uint8_t> input);
    ~Item() = default;

    /**
     * @brief Get the type identifier of this TLV item
     *
     * @return The type identifier
     */
    uint16_t getType() const;

    /**
     * @brief Get the total size of this TLV item (header + data)
     *
     * @return Total size in bytes
     */
    size_t getTotalSize() const;

    /**
     * @brief Get the size of the value data (excluding header)
     *
     * @return Value size in bytes
     */
    size_t getValueSize() const;

    /**
     * @brief Get the value as a vector of the specified type
     *
     * @tparam T Vector type (must satisfy VectorType concept)
     * @return Decoded vector data
     */
    template <VectorType T>
    T getValue() const;

    /**
     * @brief Get the value as an unsigned integral type
     *
     * @tparam T Unsigned integral type
     * @return Decoded value
     */
    template <std::unsigned_integral T>
    T getValue() const;

    /**
     * @brief Get the raw value data as a byte vector
     *
     * @return Raw byte data
     */
    const std::vector<uint8_t>& getRawValue() const;

    /**
     * @brief Get a human-readable name for a TLV type
     *
     * @param type The type identifier
     * @return Human-readable type name
     */
    static std::string getTypeName(uint16_t type);

  private:
    std::shared_ptr<ItemHeader> header;
    std::vector<uint8_t> data;
};

/**
 * @brief Represents a complete TLV structure for decoding operations
 *
 * This class handles the parsing and management of complete TLV structures,
 * providing access to individual TLV items by type.
 */
class Structure
{
  public:
    Structure() = default;

    /**
     * @brief Construct a TLV structure from input data
     *
     * @param input Vector containing the TLV structure data
     */
    Structure(const std::vector<uint8_t>& input);
    ~Structure() = default;

    /**
     * @brief Decode a TLV structure from input data
     *
     * @param input Vector containing the TLV structure data
     */
    void decode(const std::vector<uint8_t>& input);

    /**
     * @brief Get the version of this TLV structure
     *
     * @return Pair of major and minor version numbers
     */
    std::pair<uint16_t, uint16_t> getVersion() const;

    /**
     * @brief Get all type identifiers present in this structure
     *
     * @return Vector of type identifiers
     */
    std::vector<uint16_t> getTypes() const;

    /**
     * @brief Get a TLV item by type
     *
     * @param type The type identifier to look up
     * @return Reference to the TLV item
     * @throws std::runtime_error if the type is not found
     */
    const Item& get(uint16_t type) const;

  private:
    std::shared_ptr<StructureHeader> header;
    std::map<uint16_t, Item> data;
};
} // namespace tlv_decoder

namespace tlv_encoder
{
/**
 * @brief Represents a single TLV item for encoding operations
 *
 * This class handles the creation and encoding of individual TLV items,
 * providing a convenient interface for building TLV data structures.
 */
class Item
{
  public:
    /**
     * @brief Construct a TLV item with the given type and data
     *
     * @param type The type identifier for this item
     * @param input The data to encode
     */
    Item(uint16_t type, const std::vector<uint8_t>& input);
    ~Item() = default;

    /**
     * @brief Get the total size of this TLV item (header + data)
     *
     * @return Total size in bytes
     */
    size_t getTotalSize() const;

    /**
     * @brief Get the encoded TLV item data
     *
     * @return Vector containing the complete encoded TLV item
     */
    const std::vector<uint8_t>& getValue() const;

  private:
    std::vector<uint8_t> data;
};

/**
 * @brief Represents a complete TLV structure for encoding operations
 *
 * This class handles the creation and encoding of complete TLV structures,
 * providing methods to add various types of data and encode the final result.
 */
class Structure
{
  public:
    Structure();
    ~Structure() = default;

    /**
     * @brief Set the version of this TLV structure
     *
     * @param major Major version number
     * @param minor Minor version number
     */
    void setVersion(uint16_t major, uint16_t minor);

    /**
     * @brief Add a TLV item with raw byte data
     *
     * @param type The type identifier
     * @param input The raw data to encode
     */
    void add(uint16_t type, const std::vector<uint8_t>& input);

    /**
     * @brief Add a TLV item with 32-bit integer array data
     *
     * @param type The type identifier
     * @param input The 32-bit integer array to encode
     */
    void add(uint16_t type, const std::vector<uint32_t>& input);

    /**
     * @brief Add a TLV item with 8-bit integer data
     *
     * @param type The type identifier
     * @param value The 8-bit integer value to encode
     */
    void add(uint16_t type, uint8_t value);

    /**
     * @brief Add a TLV item with 16-bit integer data
     *
     * @param type The type identifier
     * @param value The 16-bit integer value to encode
     */
    void add(uint16_t type, uint16_t value);

    /**
     * @brief Add a TLV item with 32-bit integer data
     *
     * @param type The type identifier
     * @param value The 32-bit integer value to encode
     */
    void add(uint16_t type, uint32_t value);

    /**
     * @brief Encode the complete TLV structure
     *
     * @return Vector containing the complete encoded TLV structure
     */
    std::vector<uint8_t> encode();

  private:
    std::shared_ptr<StructureHeader> header;
    std::map<uint16_t, Item> data;
};

} // namespace tlv_encoder

} // namespace debug_token
