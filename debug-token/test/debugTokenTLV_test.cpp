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

#include "debug-token/error.h"
#include "debug-token/tlv.h"
#include "debug-token/types.h"

#include <stdlib.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Test;

using namespace debug_token;

std::vector<uint8_t> createItemHeader(uint16_t type, uint16_t size)
{
    std::vector<uint8_t> header(sizeof(ItemHeader));
    auto* h = reinterpret_cast<ItemHeader*>(header.data());
    h->type = htole16(type);
    h->size = htole16(size);
    return header;
}

std::vector<uint8_t> createStructureHeader(uint16_t versionMajor,
                                           uint16_t versionMinor, uint32_t size)
{
    std::vector<uint8_t> header(sizeof(StructureHeader));
    auto* h = reinterpret_cast<StructureHeader*>(header.data());
    std::memcpy(h->identifier, TLV_IDENTIFIER, sizeof(TLV_IDENTIFIER));
    h->versionMajor = htole16(versionMajor);
    h->versionMinor = htole16(versionMinor);
    h->size = htole32(size);
    return header;
}

// ============================================================================
// TLV decoder item tests
// ============================================================================

class TlvDecoderItemTest : public ::testing::Test
{
  protected:
    void SetUp() override {}
};

TEST_F(TlvDecoderItemTest, ConstructorValidInput)
{
    uint16_t itemType = 0x1234;
    uint16_t itemSize = 4;
    uint32_t itemValue = 0x01020304;

    auto itemHeader = createItemHeader(itemType, itemSize);
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), reinterpret_cast<uint8_t*>(&itemValue),
                 reinterpret_cast<uint8_t*>(&itemValue) + sizeof(itemValue));

    EXPECT_NO_THROW({
        tlv_decoder::Item item(std::span{input});
        EXPECT_EQ(item.getType(), itemType);
        EXPECT_EQ(item.getValueSize(), itemSize);
        EXPECT_EQ(item.getTotalSize(), sizeof(ItemHeader) + itemSize);
    });
}

TEST_F(TlvDecoderItemTest, ConstructorInputTooShort)
{
    std::vector<uint8_t> input = {0x01, 0x02}; // too short for header

    EXPECT_THROW({ tlv_decoder::Item item(std::span{input}); },
                 std::runtime_error);
}

TEST_F(TlvDecoderItemTest, ConstructorDataTooShort)
{
    uint16_t itemType = 0x1234;
    uint16_t itemSize = 10; // larger than available data
    uint32_t itemValue = 0x01020304;

    auto itemHeader = createItemHeader(itemType, itemSize);
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), reinterpret_cast<uint8_t*>(&itemValue),
                 reinterpret_cast<uint8_t*>(&itemValue) + sizeof(itemValue));

    EXPECT_THROW({ tlv_decoder::Item item(std::span{input}); },
                 std::runtime_error);
}

TEST_F(TlvDecoderItemTest, GetValueUint8)
{
    uint16_t itemType = 0x1234;
    uint8_t itemValue = 0x42;

    auto itemHeader = createItemHeader(itemType, sizeof(itemValue));
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), &itemValue, &itemValue + sizeof(itemValue));

    tlv_decoder::Item item(std::span{input});
    EXPECT_EQ(item.getValue<uint8_t>(), itemValue);
}

TEST_F(TlvDecoderItemTest, GetValueUint16)
{
    uint16_t itemType = 0x1234;
    uint16_t itemValue = 0x1234;

    auto itemHeader = createItemHeader(itemType, sizeof(itemValue));
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    uint16_t leValue = htole16(itemValue);
    input.insert(input.end(), reinterpret_cast<uint8_t*>(&leValue),
                 reinterpret_cast<uint8_t*>(&leValue) + sizeof(leValue));

    tlv_decoder::Item item(std::span{input});
    EXPECT_EQ(item.getValue<uint16_t>(), itemValue);
}

TEST_F(TlvDecoderItemTest, GetValueUint32)
{
    uint16_t itemType = 0x1234;
    uint32_t itemValue = 0x12345678;

    auto itemHeader = createItemHeader(itemType, sizeof(itemValue));
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    uint32_t leValue = htole32(itemValue);
    input.insert(input.end(), reinterpret_cast<uint8_t*>(&leValue),
                 reinterpret_cast<uint8_t*>(&leValue) + sizeof(leValue));

    tlv_decoder::Item item(std::span{input});
    EXPECT_EQ(item.getValue<uint32_t>(), itemValue);
}

TEST_F(TlvDecoderItemTest, GetValueUint64)
{
    uint16_t itemType = 0x1234;
    uint64_t itemValue = 0x123456789ABCDEF0;

    auto itemHeader = createItemHeader(itemType, sizeof(itemValue));
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    uint64_t leValue = htole64(itemValue);
    input.insert(input.end(), reinterpret_cast<uint8_t*>(&leValue),
                 reinterpret_cast<uint8_t*>(&leValue) + sizeof(leValue));

    tlv_decoder::Item item(std::span{input});
    EXPECT_EQ(item.getValue<uint64_t>(), itemValue);
}

TEST_F(TlvDecoderItemTest, GetValueVectorUint8)
{
    uint16_t itemType = 0x1234;
    std::vector<uint8_t> itemValue = {0x01, 0x02, 0x03, 0x04};

    auto itemHeader = createItemHeader(itemType, itemValue.size());
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), itemValue.begin(), itemValue.end());

    tlv_decoder::Item item(std::span{input});
    auto result = item.getValue<std::vector<uint8_t>>();
    EXPECT_EQ(result, itemValue);
}

TEST_F(TlvDecoderItemTest, GetValueVectorUint16)
{
    uint16_t itemType = 0x1234;
    std::vector<uint16_t> itemValue = {0x1234, 0x5678, 0x9ABC};

    auto itemHeader = createItemHeader(itemType,
                                       itemValue.size() * sizeof(uint16_t));
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    for (const auto& value : itemValue)
    {
        uint16_t leValue = htole16(value);
        input.insert(input.end(), reinterpret_cast<uint8_t*>(&leValue),
                     reinterpret_cast<uint8_t*>(&leValue) + sizeof(leValue));
    }

    tlv_decoder::Item item(std::span{input});
    auto result = item.getValue<std::vector<uint16_t>>();
    EXPECT_EQ(result, itemValue);
}

TEST_F(TlvDecoderItemTest, GetValueVectorUint32)
{
    uint16_t itemType = 0x1234;
    std::vector<uint32_t> itemValue = {0x12345678, 0x9ABCDEF0};

    auto itemHeader = createItemHeader(itemType,
                                       itemValue.size() * sizeof(uint32_t));
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    for (const auto& value : itemValue)
    {
        uint32_t leValue = htole32(value);
        input.insert(input.end(), reinterpret_cast<uint8_t*>(&leValue),
                     reinterpret_cast<uint8_t*>(&leValue) + sizeof(leValue));
    }

    tlv_decoder::Item item(std::span{input});
    auto result = item.getValue<std::vector<uint32_t>>();
    EXPECT_EQ(result, itemValue);
}

TEST_F(TlvDecoderItemTest, GetValueVectorUint64)
{
    uint16_t itemType = 0x1234;
    std::vector<uint64_t> itemValue = {0x123456789ABCDEF0, 0xFEDCBA9876543210};

    auto itemHeader = createItemHeader(itemType,
                                       itemValue.size() * sizeof(uint64_t));
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    for (const auto& value : itemValue)
    {
        uint64_t leValue = htole64(value);
        input.insert(input.end(), reinterpret_cast<uint8_t*>(&leValue),
                     reinterpret_cast<uint8_t*>(&leValue) + sizeof(leValue));
    }

    tlv_decoder::Item item(std::span{input});
    auto result = item.getValue<std::vector<uint64_t>>();
    EXPECT_EQ(result, itemValue);
}

TEST_F(TlvDecoderItemTest, GetValueSizeMismatch)
{
    uint16_t itemType = 0x1234;
    uint16_t itemSize = 3; // not a multiple of uint32_t size
    std::vector<uint8_t> itemValue = {0x01, 0x02, 0x03};

    auto itemHeader = createItemHeader(itemType, itemSize);
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), itemValue.begin(), itemValue.end());

    tlv_decoder::Item item(std::span{input});
    EXPECT_THROW({ item.getValue<std::vector<uint32_t>>(); },
                 std::runtime_error);
}

TEST_F(TlvDecoderItemTest, GetValueWrongSize)
{
    uint16_t itemType = 0x1234;
    uint16_t itemSize = 2; // wrong size for uint32_t
    uint16_t itemValue = 0x1234;

    auto itemHeader = createItemHeader(itemType, itemSize);
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    uint16_t leValue = htole16(itemValue);
    input.insert(input.end(), reinterpret_cast<uint8_t*>(&leValue),
                 reinterpret_cast<uint8_t*>(&leValue) + sizeof(leValue));

    tlv_decoder::Item item(std::span{input});
    EXPECT_THROW({ item.getValue<uint32_t>(); }, std::runtime_error);
}

TEST_F(TlvDecoderItemTest, GetRawValue)
{
    uint16_t itemType = 0x1234;
    std::vector<uint8_t> itemValue = {0x01, 0x02, 0x03, 0x04};

    auto itemHeader = createItemHeader(itemType, itemValue.size());
    std::vector<uint8_t> input;
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), itemValue.begin(), itemValue.end());

    tlv_decoder::Item item(std::span{input});
    const auto& rawValue = item.getRawValue();
    EXPECT_EQ(rawValue, itemValue);
}

TEST_F(TlvDecoderItemTest, GetTypeName)
{
    EXPECT_EQ(tlv_decoder::Item::getTypeName(0x0001), "DeviceType");
    EXPECT_EQ(tlv_decoder::Item::getTypeName(0x0002), "ChallengeNonce");
    EXPECT_EQ(tlv_decoder::Item::getTypeName(0x0003), "DeviceSerialNumber");
    EXPECT_EQ(tlv_decoder::Item::getTypeName(0x4000), "GPUFeatureMask");
    EXPECT_EQ(tlv_decoder::Item::getTypeName(0x4400), "NBUKeypairUUID");
    EXPECT_EQ(tlv_decoder::Item::getTypeName(0x4801), "BMCIRoTTokenVersion");
    EXPECT_EQ(tlv_decoder::Item::getTypeName(0x9999), "UnknownType(0x9999)");
}

// ============================================================================
// TLV decoder structure tests
// ============================================================================

class TlvDecoderStructureTest : public ::testing::Test
{
  protected:
    void SetUp() override {}
};

TEST_F(TlvDecoderStructureTest, ConstructorValidInput)
{
    uint16_t versionMajor = 1;
    uint16_t versionMinor = 2;
    uint16_t itemType = 0x1234;
    uint16_t itemSize = 4;
    uint32_t itemValue = 0x01020304;

    auto itemHeader = createItemHeader(itemType, itemSize);
    auto structureHeader = createStructureHeader(
        versionMajor, versionMinor, itemHeader.size() + sizeof(itemValue));

    std::vector<uint8_t> input;
    input.insert(input.end(), structureHeader.begin(), structureHeader.end());
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), reinterpret_cast<uint8_t*>(&itemValue),
                 reinterpret_cast<uint8_t*>(&itemValue) + sizeof(itemValue));

    EXPECT_NO_THROW({
        tlv_decoder::Structure structure(input);
        auto version = structure.getVersion();
        EXPECT_EQ(version.first, versionMajor);
        EXPECT_EQ(version.second, versionMinor);

        const auto& item = structure.get(itemType);
        EXPECT_EQ(item.getType(), itemType);
        EXPECT_EQ(item.getValueSize(), itemSize);
        EXPECT_EQ(item.getTotalSize(), sizeof(ItemHeader) + itemSize);
        EXPECT_EQ(item.getValue<uint32_t>(), le32toh(itemValue));
    });
}

TEST_F(TlvDecoderStructureTest, ConstructorInputTooShort)
{
    std::vector<uint8_t> input = {0x01, 0x02, 0x03}; // too short for header

    EXPECT_THROW({ tlv_decoder::Structure structure(input); },
                 std::runtime_error);
}

TEST_F(TlvDecoderStructureTest, ConstructorInvalidIdentifier)
{
    std::vector<uint8_t> input = createStructureHeader(1, 0, 0);
    auto* header = reinterpret_cast<StructureHeader*>(input.data());
    uint32_t invalidIdentifier = 0x12345678;
    std::memcpy(header->identifier, &invalidIdentifier,
                sizeof(header->identifier)); // invalid identifier

    EXPECT_THROW({ tlv_decoder::Structure structure(input); },
                 std::runtime_error);
}

TEST_F(TlvDecoderStructureTest, ConstructorInvalidSize)
{
    std::vector<uint8_t> input = createStructureHeader(1, 0,
                                                       100); // incorrect size
    input.resize(50);

    EXPECT_THROW({ tlv_decoder::Structure structure(input); },
                 std::runtime_error);
}

TEST_F(TlvDecoderStructureTest, ConstructorEmptyData)
{
    auto structureHeader = createStructureHeader(1, 0, 0);

    EXPECT_THROW({ tlv_decoder::Structure structure(structureHeader); },
                 std::runtime_error);
}

TEST_F(TlvDecoderStructureTest, ConstructorDuplicateType)
{
    uint16_t versionMajor = 1;
    uint16_t versionMinor = 0;
    uint16_t itemType = 0x1234;
    uint16_t itemSize = 2;
    std::vector<uint8_t> itemValue = {0x01, 0x02};

    auto itemHeader = createItemHeader(itemType, itemSize);
    auto structureHeader = createStructureHeader(
        versionMajor, versionMinor, 2 * (itemHeader.size() + itemValue.size()));

    std::vector<uint8_t> input;
    input.insert(input.end(), structureHeader.begin(), structureHeader.end());

    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), itemValue.begin(), itemValue.end());

    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), itemValue.begin(), itemValue.end());

    EXPECT_THROW({ tlv_decoder::Structure structure(input); },
                 std::runtime_error);
}

TEST_F(TlvDecoderStructureTest, GetVersion)
{
    uint16_t versionMajor = 5;
    uint16_t versionMinor = 10;
    uint16_t itemType = 0x1234;
    uint16_t itemSize = 1;
    std::vector<uint8_t> itemValue = {0x01};

    auto itemHeader = createItemHeader(itemType, itemSize);
    auto structureHeader = createStructureHeader(
        versionMajor, versionMinor, itemHeader.size() + itemValue.size());

    std::vector<uint8_t> input;
    input.insert(input.end(), structureHeader.begin(), structureHeader.end());
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), itemValue.begin(), itemValue.end());

    tlv_decoder::Structure structure(input);
    auto version = structure.getVersion();
    EXPECT_EQ(version.first, versionMajor);
    EXPECT_EQ(version.second, versionMinor);
}

TEST_F(TlvDecoderStructureTest, GetExistingItem)
{
    uint16_t itemType = 0x5678;
    uint16_t itemSize = 2;
    std::vector<uint8_t> itemValue = {0xAA, 0xBB};

    auto itemHeader = createItemHeader(itemType, itemSize);
    auto structureHeader =
        createStructureHeader(1, 0, itemHeader.size() + itemValue.size());

    std::vector<uint8_t> input;
    input.insert(input.end(), structureHeader.begin(), structureHeader.end());
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), itemValue.begin(), itemValue.end());

    tlv_decoder::Structure structure(input);
    const auto& item = structure.get(itemType);
    EXPECT_EQ(item.getType(), itemType);
    EXPECT_EQ(item.getValueSize(), itemSize);
    EXPECT_EQ(item.getTotalSize(), sizeof(ItemHeader) + itemSize);
    EXPECT_EQ(std::memcmp(item.getValue<std::vector<uint8_t>>().data(),
                          itemValue.data(), itemValue.size()),
              0);
}

TEST_F(TlvDecoderStructureTest, GetNonExistentItem)
{
    uint16_t itemType = 0x1234;
    uint16_t itemSize = 1;
    std::vector<uint8_t> itemValue = {0x01};

    auto itemHeader = createItemHeader(itemType, itemSize);
    auto structureHeader =
        createStructureHeader(1, 0, itemHeader.size() + itemValue.size());

    std::vector<uint8_t> input;
    input.insert(input.end(), structureHeader.begin(), structureHeader.end());
    input.insert(input.end(), itemHeader.begin(), itemHeader.end());
    input.insert(input.end(), itemValue.begin(), itemValue.end());

    tlv_decoder::Structure structure(input);
    EXPECT_THROW({ structure.get(0x9999); }, std::runtime_error);
}

TEST_F(TlvDecoderStructureTest, GetTypes)
{
    uint16_t versionMajor = 2;
    uint16_t versionMinor = 1;
    std::vector<uint16_t> expectedTypes = {0x1001, 0x1002, 0x1003, 0x1004};
    std::map<uint16_t, std::vector<uint8_t>> testData = {
        {0x1001, {0x01, 0x02}},
        {0x1002, {0x03, 0x04, 0x05}},
        {0x1003, {0x06}},
        {0x1004, {0x07, 0x08, 0x09, 0x0A}}};

    size_t totalSize = 0;
    for (const auto& [type, value] : testData)
    {
        totalSize += sizeof(ItemHeader) + value.size();
    }

    auto structureHeader = createStructureHeader(versionMajor, versionMinor,
                                                 totalSize);

    std::vector<uint8_t> input;
    input.insert(input.end(), structureHeader.begin(), structureHeader.end());

    for (const auto& [type, value] : testData)
    {
        auto itemHeader = createItemHeader(type, value.size());
        input.insert(input.end(), itemHeader.begin(), itemHeader.end());
        input.insert(input.end(), value.begin(), value.end());
    }

    tlv_decoder::Structure structure(input);
    auto types = structure.getTypes();

    std::sort(types.begin(), types.end());
    std::sort(expectedTypes.begin(), expectedTypes.end());
    EXPECT_EQ(types, expectedTypes);
}

TEST_F(TlvDecoderStructureTest, MultipleItems)
{
    uint16_t versionMajor = 2;
    uint16_t versionMinor = 1;
    std::map<uint16_t, std::vector<uint8_t>> testData = {
        {0x1001, {0x01, 0x02}},
        {0x1002, {0x03, 0x04, 0x05}},
        {0x1003, {0x06}},
        {0x1004, {0x07, 0x08, 0x09, 0x0A}}};

    size_t totalSize = 0;
    for (const auto& [type, value] : testData)
    {
        totalSize += sizeof(ItemHeader) + value.size();
    }

    auto structureHeader = createStructureHeader(versionMajor, versionMinor,
                                                 totalSize);

    std::vector<uint8_t> input;
    input.insert(input.end(), structureHeader.begin(), structureHeader.end());

    for (const auto& [type, value] : testData)
    {
        auto itemHeader = createItemHeader(type, value.size());
        input.insert(input.end(), itemHeader.begin(), itemHeader.end());
        input.insert(input.end(), value.begin(), value.end());
    }

    tlv_decoder::Structure structure(input);
    auto version = structure.getVersion();
    EXPECT_EQ(version.first, versionMajor);
    EXPECT_EQ(version.second, versionMinor);

    for (const auto& [type, expectedValue] : testData)
    {
        const auto& item = structure.get(type);
        EXPECT_EQ(item.getType(), type);
        EXPECT_EQ(item.getValueSize(), expectedValue.size());
        EXPECT_EQ(item.getTotalSize(),
                  sizeof(ItemHeader) + expectedValue.size());
        EXPECT_EQ(std::memcmp(item.getValue<std::vector<uint8_t>>().data(),
                              expectedValue.data(), expectedValue.size()),
                  0);
    }
}

// ============================================================================
// TLV Encoder Item Tests
// ============================================================================

class TlvEncoderItemTest : public ::testing::Test
{
  protected:
    void SetUp() override {}
};

TEST_F(TlvEncoderItemTest, ConstructorValidInput)
{
    uint16_t itemType = 0x1234;
    std::vector<uint8_t> itemValue = {0x01, 0x02, 0x03, 0x04};

    EXPECT_NO_THROW({
        tlv_encoder::Item item(itemType, itemValue);
        EXPECT_EQ(item.getTotalSize(), sizeof(ItemHeader) + itemValue.size());

        const auto& encoded = item.getValue();
        EXPECT_EQ(encoded.size(), sizeof(ItemHeader) + itemValue.size());

        // Verify header
        const auto* header =
            reinterpret_cast<const ItemHeader*>(encoded.data());
        EXPECT_EQ(le16toh(header->type), itemType);
        EXPECT_EQ(le16toh(header->size), itemValue.size());

        // Verify data
        EXPECT_EQ(std::memcmp(encoded.data() + sizeof(ItemHeader),
                              itemValue.data(), itemValue.size()),
                  0);
    });
}

TEST_F(TlvEncoderItemTest, ConstructorEmptyValue)
{
    uint16_t itemType = 0x1234;
    std::vector<uint8_t> itemValue;

    EXPECT_NO_THROW({
        tlv_encoder::Item item(itemType, itemValue);
        EXPECT_EQ(item.getTotalSize(), sizeof(ItemHeader));

        const auto& encoded = item.getValue();
        EXPECT_EQ(encoded.size(), sizeof(ItemHeader));

        const auto* header =
            reinterpret_cast<const ItemHeader*>(encoded.data());
        EXPECT_EQ(le16toh(header->type), itemType);
        EXPECT_EQ(le16toh(header->size), 0);
    });
}

TEST_F(TlvEncoderItemTest, ConstructorLargeValue)
{
    uint16_t itemType = 0x1234;
    std::vector<uint8_t> itemValue(1000, 0x42);

    EXPECT_NO_THROW({
        tlv_encoder::Item item(itemType, itemValue);
        EXPECT_EQ(item.getTotalSize(), sizeof(ItemHeader) + itemValue.size());

        const auto& encoded = item.getValue();
        EXPECT_EQ(encoded.size(), sizeof(ItemHeader) + itemValue.size());

        const auto* header =
            reinterpret_cast<const ItemHeader*>(encoded.data());
        EXPECT_EQ(le16toh(header->type), itemType);
        EXPECT_EQ(le16toh(header->size), itemValue.size());

        EXPECT_EQ(std::memcmp(encoded.data() + sizeof(ItemHeader),
                              itemValue.data(), itemValue.size()),
                  0);
    });
}

TEST_F(TlvEncoderItemTest, ConstructorValueTooLarge)
{
    uint16_t itemType = 0x1234;
    std::vector<uint8_t> itemValue(std::numeric_limits<uint16_t>::max() + 1,
                                   0x42);

    EXPECT_THROW({ tlv_encoder::Item item(itemType, itemValue); },
                 std::runtime_error);
}

// ============================================================================
// TLV Encoder Structure Tests
// ============================================================================

class TlvEncoderStructureTest : public ::testing::Test
{
  protected:
    void SetUp() override {}
};

TEST_F(TlvEncoderStructureTest, Constructor)
{
    EXPECT_NO_THROW({ tlv_encoder::Structure structure; });
}

TEST_F(TlvEncoderStructureTest, SetVersion)
{
    tlv_encoder::Structure structure;
    structure.setVersion(5, 10);
    auto encoded = structure.encode();
    auto* header = reinterpret_cast<const StructureHeader*>(encoded.data());
    EXPECT_EQ(
        std::memcmp(header->identifier, TLV_IDENTIFIER, sizeof(TLV_IDENTIFIER)),
        0);
    EXPECT_EQ(le16toh(header->versionMajor), 5);
    EXPECT_EQ(le16toh(header->versionMinor), 10);
    EXPECT_EQ(le32toh(header->size), 0);
}

TEST_F(TlvEncoderStructureTest, AddValidItem)
{
    tlv_encoder::Structure structure;
    uint16_t type = 0x1234;
    std::vector<uint8_t> value = {0x01, 0x02, 0x03};

    EXPECT_NO_THROW({ structure.add(type, value); });
}

TEST_F(TlvEncoderStructureTest, AddDuplicateItem)
{
    tlv_encoder::Structure structure;
    uint16_t type = 0x1234;
    std::vector<uint8_t> value1 = {0x01, 0x02};
    std::vector<uint8_t> value2 = {0x03, 0x04};

    structure.add(type, value1);
    EXPECT_THROW({ structure.add(type, value2); }, std::runtime_error);
}

TEST_F(TlvEncoderStructureTest, EncodeEmptyStructure)
{
    tlv_encoder::Structure structure;

    auto encoded = structure.encode();
    EXPECT_EQ(encoded.size(), sizeof(StructureHeader));

    auto* header = reinterpret_cast<const StructureHeader*>(encoded.data());
    EXPECT_EQ(
        std::memcmp(header->identifier, TLV_IDENTIFIER, sizeof(TLV_IDENTIFIER)),
        0);
    EXPECT_EQ(le16toh(header->versionMajor), 1);
    EXPECT_EQ(le16toh(header->versionMinor), 0);
    EXPECT_EQ(le32toh(header->size), 0);
}

TEST_F(TlvEncoderStructureTest, EncodeWithItems)
{
    tlv_encoder::Structure structure;
    structure.setVersion(2, 3);

    uint16_t type1 = 0x1234;
    std::vector<uint8_t> value1 = {0x01, 0x02};
    uint16_t type2 = 0x5678;
    std::vector<uint8_t> value2 = {0x03, 0x04, 0x05};

    structure.add(type1, value1);
    structure.add(type2, value2);

    auto encoded = structure.encode();

    auto* header = reinterpret_cast<const StructureHeader*>(encoded.data());
    EXPECT_EQ(
        std::memcmp(header->identifier, TLV_IDENTIFIER, sizeof(TLV_IDENTIFIER)),
        0);
    EXPECT_EQ(le16toh(header->versionMajor), 2);
    EXPECT_EQ(le16toh(header->versionMinor), 3);

    size_t expectedSize = sizeof(ItemHeader) + value1.size() +
                          sizeof(ItemHeader) + value2.size();
    EXPECT_EQ(le32toh(header->size), expectedSize);
}

TEST_F(TlvEncoderStructureTest, EncodeMultipleItems)
{
    tlv_encoder::Structure structure;
    structure.setVersion(3, 4);

    std::map<uint16_t, std::vector<uint8_t>> testData = {
        {0x1001, {0x01, 0x02}},
        {0x1002, {0x03, 0x04, 0x05}},
        {0x1003, {0x06}},
        {0x1004, {0x07, 0x08, 0x09, 0x0A}}};

    for (const auto& [type, value] : testData)
    {
        structure.add(type, value);
    }

    auto encoded = structure.encode();

    auto* header = reinterpret_cast<const StructureHeader*>(encoded.data());
    EXPECT_EQ(
        std::memcmp(header->identifier, TLV_IDENTIFIER, sizeof(TLV_IDENTIFIER)),
        0);
    EXPECT_EQ(le16toh(header->versionMajor), 3);
    EXPECT_EQ(le16toh(header->versionMinor), 4);

    size_t expectedSize = 0;
    for (const auto& [type, value] : testData)
    {
        expectedSize += sizeof(ItemHeader) + value.size();
    }
    EXPECT_EQ(le32toh(header->size), expectedSize);
}

TEST_F(TlvEncoderStructureTest, AddUint8Value)
{
    tlv_encoder::Structure structure;
    structure.setVersion(1, 0);

    uint16_t type = 0x1234;
    uint8_t value = 0x42;

    EXPECT_NO_THROW({ structure.add(type, value); });

    auto encoded = structure.encode();
    tlv_decoder::Structure decoder(encoded);
    const auto& item = decoder.get(type);
    EXPECT_EQ(item.getValue<uint8_t>(), value);
}

TEST_F(TlvEncoderStructureTest, AddUint16Value)
{
    tlv_encoder::Structure structure;
    structure.setVersion(1, 0);

    uint16_t type = 0x1234;
    uint16_t value = 0x1234;

    EXPECT_NO_THROW({ structure.add(type, value); });

    auto encoded = structure.encode();
    tlv_decoder::Structure decoder(encoded);
    const auto& item = decoder.get(type);
    EXPECT_EQ(item.getValue<uint16_t>(), value);
}

TEST_F(TlvEncoderStructureTest, AddUint32Value)
{
    tlv_encoder::Structure structure;
    structure.setVersion(1, 0);

    uint16_t type = 0x1234;
    uint32_t value = 0x12345678;

    EXPECT_NO_THROW({ structure.add(type, value); });

    auto encoded = structure.encode();
    tlv_decoder::Structure decoder(encoded);
    const auto& item = decoder.get(type);
    EXPECT_EQ(item.getValue<uint32_t>(), value);
}

TEST_F(TlvEncoderStructureTest, AddVectorUint32)
{
    tlv_encoder::Structure structure;
    structure.setVersion(1, 0);

    uint16_t type = 0x1234;
    std::vector<uint32_t> value = {0x12345678, 0x9ABCDEF0, 0x11223344};

    EXPECT_NO_THROW({ structure.add(type, value); });

    auto encoded = structure.encode();
    tlv_decoder::Structure decoder(encoded);
    const auto& item = decoder.get(type);
    auto result = item.getValue<std::vector<uint32_t>>();
    EXPECT_EQ(result, value);
}

TEST_F(TlvEncoderStructureTest, AddItemSizeOverflow)
{
    tlv_encoder::Structure structure;
    structure.setVersion(1, 0);

    uint16_t type = 0x1234;
    std::vector<uint8_t> largeValue(std::numeric_limits<uint16_t>::max() + 1,
                                    0x42);

    EXPECT_THROW({ structure.add(type, largeValue); }, std::runtime_error);
}

TEST_F(TlvEncoderStructureTest, EncodeSizeOverflow)
{
    tlv_encoder::Structure structure;
    structure.setVersion(1, 0);

    // To trigger overflow, we need total size > uint32_t max (4,294,967,295
    // bytes) With max item size (65535 bytes) + header (4 bytes) = 65539 bytes
    // per item Strategy: Add items that sum just over uint32_t::max
    // uint32_t::max / 65539 ≈ 65535.5, so we need 65536 items
    // But we can optimize by only testing the overflow detection, not actually
    // allocating all the memory. We'll add items until we're certain to
    // overflow.

    std::vector<uint8_t> largeValue(std::numeric_limits<uint16_t>::max(), 0x42);
    // Add items 0 through 65535 (total 65536 items)
    // This should trigger overflow: 65536 * 65539 = 4,295,294,464 >
    // 4,294,967,295
    for (size_t i = 0; i <= std::numeric_limits<uint16_t>::max(); ++i)
    {
        structure.add(static_cast<uint16_t>(i), largeValue);
    }

    EXPECT_THROW(structure.encode(), std::overflow_error);
}

// ============================================================================
// Integration Tests
// ============================================================================

class TlvIntegrationTest : public ::testing::Test
{
  protected:
    void SetUp() override {}
};

TEST_F(TlvIntegrationTest, EncodeDecodeRoundTrip)
{
    tlv_encoder::Structure encoder;
    encoder.setVersion(1, 2);

    uint16_t type1 = 0x1234;
    std::vector<uint8_t> value1 = {0x01, 0x02, 0x03};
    uint16_t type2 = 0x5678;
    std::vector<uint8_t> value2 = {0xAA, 0xBB};

    encoder.add(type1, value1);
    encoder.add(type2, value2);

    auto encoded = encoder.encode();

    tlv_decoder::Structure decoder(encoded);

    auto version = decoder.getVersion();
    EXPECT_EQ(version.first, 1);
    EXPECT_EQ(version.second, 2);

    const auto& item1 = decoder.get(type1);
    EXPECT_EQ(item1.getType(), type1);
    EXPECT_EQ(item1.getValueSize(), value1.size());
    EXPECT_EQ(item1.getTotalSize(), sizeof(ItemHeader) + value1.size());
    EXPECT_EQ(std::memcmp(item1.getValue<std::vector<uint8_t>>().data(),
                          value1.data(), value1.size()),
              0);

    const auto& item2 = decoder.get(type2);
    EXPECT_EQ(item2.getType(), type2);
    EXPECT_EQ(item2.getValueSize(), value2.size());
    EXPECT_EQ(item2.getTotalSize(), sizeof(ItemHeader) + value2.size());
    EXPECT_EQ(std::memcmp(item2.getValue<std::vector<uint8_t>>().data(),
                          value2.data(), value2.size()),
              0);
}

TEST_F(TlvIntegrationTest, MultipleItemsRoundTrip)
{
    tlv_encoder::Structure encoder;
    encoder.setVersion(3, 4);

    std::map<uint16_t, std::vector<uint8_t>> testData = {
        {0x1001, {0x01, 0x02}},
        {0x1002, {0x03, 0x04, 0x05}},
        {0x1003, {0x06}},
        {0x1004, {0x07, 0x08, 0x09, 0x0A}}};

    for (const auto& [type, value] : testData)
    {
        encoder.add(type, value);
    }

    auto encoded = encoder.encode();
    tlv_decoder::Structure decoder(encoded);

    auto version = decoder.getVersion();
    EXPECT_EQ(version.first, 3);
    EXPECT_EQ(version.second, 4);

    for (const auto& [type, expectedValue] : testData)
    {
        const auto& item = decoder.get(type);
        EXPECT_EQ(item.getType(), type);
        EXPECT_EQ(item.getValueSize(), expectedValue.size());
        EXPECT_EQ(item.getTotalSize(),
                  sizeof(ItemHeader) + expectedValue.size());
        EXPECT_EQ(std::memcmp(item.getValue<std::vector<uint8_t>>().data(),
                              expectedValue.data(), expectedValue.size()),
                  0);
    }
}

TEST_F(TlvIntegrationTest, EmptyValueRoundTrip)
{
    tlv_encoder::Structure encoder;
    encoder.setVersion(1, 0);

    uint16_t type = 0x1234;
    std::vector<uint8_t> emptyValue;

    encoder.add(type, emptyValue);

    auto encoded = encoder.encode();
    tlv_decoder::Structure decoder(encoded);

    auto version = decoder.getVersion();
    EXPECT_EQ(version.first, 1);
    EXPECT_EQ(version.second, 0);

    const auto& item = decoder.get(type);
    EXPECT_EQ(item.getType(), type);
    EXPECT_EQ(item.getValueSize(), 0);
    EXPECT_EQ(item.getTotalSize(), sizeof(ItemHeader));
}

TEST_F(TlvIntegrationTest, LargeValueRoundTrip)
{
    tlv_encoder::Structure encoder;
    encoder.setVersion(1, 0);

    uint16_t type = 0x1234;
    std::vector<uint8_t> largeValue(1000, 0x42);

    encoder.add(type, largeValue);

    auto encoded = encoder.encode();
    tlv_decoder::Structure decoder(encoded);

    auto version = decoder.getVersion();
    EXPECT_EQ(version.first, 1);
    EXPECT_EQ(version.second, 0);

    const auto& item = decoder.get(type);
    EXPECT_EQ(item.getType(), type);
    EXPECT_EQ(item.getValueSize(), largeValue.size());
    EXPECT_EQ(item.getTotalSize(), sizeof(ItemHeader) + largeValue.size());
    EXPECT_EQ(std::memcmp(item.getValue<std::vector<uint8_t>>().data(),
                          largeValue.data(), largeValue.size()),
              0);
}

TEST_F(TlvIntegrationTest, MixedDataTypesRoundTrip)
{
    tlv_encoder::Structure encoder;
    encoder.setVersion(2, 1);

    // Add different data types
    encoder.add(0x1001, static_cast<uint8_t>(0x42));
    encoder.add(0x1002, static_cast<uint16_t>(0x1234));
    encoder.add(0x1003, static_cast<uint32_t>(0x12345678));
    encoder.add(0x1004, std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04});
    encoder.add(0x1005, std::vector<uint32_t>{0x12345678, 0x9ABCDEF0});

    auto encoded = encoder.encode();
    tlv_decoder::Structure decoder(encoded);

    auto version = decoder.getVersion();
    EXPECT_EQ(version.first, 2);
    EXPECT_EQ(version.second, 1);

    // Verify all items
    EXPECT_EQ(decoder.get(0x1001).getValue<uint8_t>(), 0x42);
    EXPECT_EQ(decoder.get(0x1002).getValue<uint16_t>(), 0x1234);
    EXPECT_EQ(decoder.get(0x1003).getValue<uint32_t>(), 0x12345678);

    auto vec8 = decoder.get(0x1004).getValue<std::vector<uint8_t>>();
    EXPECT_EQ(vec8, std::vector<uint8_t>({0x01, 0x02, 0x03, 0x04}));

    auto vec32 = decoder.get(0x1005).getValue<std::vector<uint32_t>>();
    EXPECT_EQ(vec32, std::vector<uint32_t>({0x12345678, 0x9ABCDEF0}));
}

TEST_F(TlvIntegrationTest, MaxSizeValueRoundTrip)
{
    tlv_encoder::Structure encoder;
    encoder.setVersion(1, 0);

    uint16_t type = 0x1234;
    std::vector<uint8_t> maxValue(std::numeric_limits<uint16_t>::max(), 0x42);

    encoder.add(type, maxValue);

    auto encoded = encoder.encode();
    tlv_decoder::Structure decoder(encoded);

    const auto& item = decoder.get(type);
    EXPECT_EQ(item.getValueSize(), maxValue.size());
    EXPECT_EQ(item.getTotalSize(), sizeof(ItemHeader) + maxValue.size());
    EXPECT_EQ(std::memcmp(item.getValue<std::vector<uint8_t>>().data(),
                          maxValue.data(), maxValue.size()),
              0);
}
