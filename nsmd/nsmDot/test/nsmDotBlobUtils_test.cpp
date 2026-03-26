/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "nsmDotBlobUtils.hpp"

#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

using namespace nsm::dot_blob_utils;

class NsmDotBlobUtilsTest : public ::testing::Test
{
  protected:
    std::string testDir = "/tmp/nsm_dot_blob_test";

    void SetUp() override
    {
        // Clean up test directory
        std::filesystem::remove_all(testDir);
    }

    void TearDown() override
    {
        // Clean up after tests
        std::filesystem::remove_all(testDir);
    }
};

// ============================================================================
// calculateSHA256 Tests
// ============================================================================

TEST_F(NsmDotBlobUtilsTest, CalculateSHA256EmptyData)
{
    std::vector<uint8_t> empty;
    std::string hash = calculateSHA256(empty);

    // SHA256 of empty string is known
    EXPECT_EQ(
        hash,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_F(NsmDotBlobUtilsTest, CalculateSHA256SingleByte)
{
    std::vector<uint8_t> data = {0x00};
    std::string hash = calculateSHA256(data);

    EXPECT_EQ(hash.length(), 64); // SHA256 is 64 hex characters
}

TEST_F(NsmDotBlobUtilsTest, CalculateSHA256MultipleBytes)
{
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    std::string hash = calculateSHA256(data);

    EXPECT_EQ(hash.length(), 64);
}

TEST_F(NsmDotBlobUtilsTest, CalculateSHA256LargeData)
{
    std::vector<uint8_t> data(1024, 0xFF);
    std::string hash = calculateSHA256(data);

    EXPECT_EQ(hash.length(), 64);
}

TEST_F(NsmDotBlobUtilsTest, CalculateSHA256Deterministic)
{
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
    std::string hash1 = calculateSHA256(data);
    std::string hash2 = calculateSHA256(data);

    EXPECT_EQ(hash1, hash2);
}

TEST_F(NsmDotBlobUtilsTest, CalculateSHA256DifferentData)
{
    std::vector<uint8_t> data1 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> data2 = {0x04, 0x05, 0x06};

    std::string hash1 = calculateSHA256(data1);
    std::string hash2 = calculateSHA256(data2);

    EXPECT_NE(hash1, hash2);
}

TEST_F(NsmDotBlobUtilsTest, CalculateSHA256AllLowercase)
{
    std::vector<uint8_t> data = {0xFF};
    std::string hash = calculateSHA256(data);

    // Verify all hex characters are lowercase
    for (char c : hash)
    {
        if (std::isalpha(c))
        {
            EXPECT_TRUE(std::islower(c));
        }
    }
}

// ============================================================================
// getBlobFilePath Tests
// ============================================================================

TEST_F(NsmDotBlobUtilsTest, GetBlobFilePathSimple)
{
    std::string path = getBlobFilePath("test");
    EXPECT_EQ(path, std::string(DOT_BLOB_DIR) + "/test.bin");
}

TEST_F(NsmDotBlobUtilsTest, GetBlobFilePathWithUnderscore)
{
    std::string path = getBlobFilePath("test_blob");
    EXPECT_EQ(path, std::string(DOT_BLOB_DIR) + "/test_blob.bin");
}

TEST_F(NsmDotBlobUtilsTest, GetBlobFilePathWithNumbers)
{
    std::string path = getBlobFilePath("blob123");
    EXPECT_EQ(path, std::string(DOT_BLOB_DIR) + "/blob123.bin");
}

TEST_F(NsmDotBlobUtilsTest, GetBlobFilePathEmpty)
{
    std::string path = getBlobFilePath("");
    EXPECT_EQ(path, std::string(DOT_BLOB_DIR) + "/.bin");
}

TEST_F(NsmDotBlobUtilsTest, GetBlobFilePathLong)
{
    std::string longName(100, 'a');
    std::string path = getBlobFilePath(longName);
    EXPECT_EQ(path, std::string(DOT_BLOB_DIR) + "/" + longName + ".bin");
}

// ============================================================================
// Integration Tests (require actual filesystem access)
// ============================================================================

TEST_F(NsmDotBlobUtilsTest, WriteBlobAtomicCreatesFile)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/test.bin";
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};

    EXPECT_NO_THROW(writeBlobAtomic(filePath, data));
    EXPECT_TRUE(std::filesystem::exists(filePath));
}

TEST_F(NsmDotBlobUtilsTest, WriteBlobAtomicCorrectContent)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/test.bin";
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD};

    writeBlobAtomic(filePath, data);

    std::ifstream file(filePath, std::ios::binary);
    std::vector<uint8_t> readData((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());

    EXPECT_EQ(readData, data);
}

TEST_F(NsmDotBlobUtilsTest, WriteBlobAtomicEmptyData)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/empty.bin";
    std::vector<uint8_t> emptyData;

    EXPECT_NO_THROW(writeBlobAtomic(filePath, emptyData));
    EXPECT_TRUE(std::filesystem::exists(filePath));
    EXPECT_EQ(std::filesystem::file_size(filePath), 0);
}

TEST_F(NsmDotBlobUtilsTest, WriteBlobAtomicLargeData)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/large.bin";
    std::vector<uint8_t> largeData(10000, 0x55);

    EXPECT_NO_THROW(writeBlobAtomic(filePath, largeData));
    EXPECT_EQ(std::filesystem::file_size(filePath), 10000);
}

TEST_F(NsmDotBlobUtilsTest, WriteBlobAtomicOverwritesExisting)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/overwrite.bin";

    std::vector<uint8_t> data1 = {0x01, 0x02};
    std::vector<uint8_t> data2 = {0x03, 0x04, 0x05};

    writeBlobAtomic(filePath, data1);
    writeBlobAtomic(filePath, data2);

    std::ifstream file(filePath, std::ios::binary);
    std::vector<uint8_t> readData((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());

    EXPECT_EQ(readData, data2);
}

TEST_F(NsmDotBlobUtilsTest, ReadBlobFileSuccess)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/read_test.bin";
    std::vector<uint8_t> originalData = {0x11, 0x22, 0x33, 0x44};

    writeBlobAtomic(filePath, originalData);

    std::vector<uint8_t> readData = readBlobFile(filePath);
    EXPECT_EQ(readData, originalData);
}

TEST_F(NsmDotBlobUtilsTest, ReadBlobFileEmpty)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/empty_read.bin";
    std::vector<uint8_t> emptyData;

    writeBlobAtomic(filePath, emptyData);

    std::vector<uint8_t> readData = readBlobFile(filePath);
    EXPECT_TRUE(readData.empty());
}

TEST_F(NsmDotBlobUtilsTest, ReadBlobFileLarge)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/large_read.bin";
    std::vector<uint8_t> largeData(50000, 0xAA);

    writeBlobAtomic(filePath, largeData);

    std::vector<uint8_t> readData = readBlobFile(filePath);
    EXPECT_EQ(readData.size(), 50000);
    EXPECT_EQ(readData, largeData);
}

TEST_F(NsmDotBlobUtilsTest, WriteAndReadRoundTrip)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/roundtrip.bin";
    std::vector<uint8_t> testData = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};

    writeBlobAtomic(filePath, testData);
    std::vector<uint8_t> readData = readBlobFile(filePath);

    EXPECT_EQ(testData, readData);
}

TEST_F(NsmDotBlobUtilsTest, WriteAndReadWithSHA256)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/hash_test.bin";
    std::vector<uint8_t> testData = {0x01, 0x02, 0x03};

    std::string originalHash = calculateSHA256(testData);
    writeBlobAtomic(filePath, testData);
    std::vector<uint8_t> readData = readBlobFile(filePath);
    std::string readHash = calculateSHA256(readData);

    EXPECT_EQ(originalHash, readHash);
}

// ============================================================================
// Error Cases
// ============================================================================

TEST_F(NsmDotBlobUtilsTest, ReadBlobFileNonexistent)
{
    std::string nonexistentPath = testDir + "/nonexistent.bin";

    EXPECT_THROW(
        readBlobFile(nonexistentPath),
        sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound);
}

TEST_F(NsmDotBlobUtilsTest, WriteBlobAtomicToInvalidDirectory)
{
    std::string invalidPath = "/invalid/path/that/does/not/exist/test.bin";
    std::vector<uint8_t> data = {0x01};

    EXPECT_THROW(writeBlobAtomic(invalidPath, data),
                 sdbusplus::xyz::openbmc_project::Common::File::Error::Write);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(NsmDotBlobUtilsTest, MultipleFilesInSameDirectory)
{
    std::filesystem::create_directories(testDir);

    std::vector<uint8_t> data1 = {0x01};
    std::vector<uint8_t> data2 = {0x02};
    std::vector<uint8_t> data3 = {0x03};

    writeBlobAtomic(testDir + "/file1.bin", data1);
    writeBlobAtomic(testDir + "/file2.bin", data2);
    writeBlobAtomic(testDir + "/file3.bin", data3);

    EXPECT_EQ(readBlobFile(testDir + "/file1.bin"), data1);
    EXPECT_EQ(readBlobFile(testDir + "/file2.bin"), data2);
    EXPECT_EQ(readBlobFile(testDir + "/file3.bin"), data3);
}

TEST_F(NsmDotBlobUtilsTest, SHA256ConsistencyAcrossWrites)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/consistency.bin";
    std::vector<uint8_t> data = {0xAB, 0xCD, 0xEF};

    std::string hash1 = calculateSHA256(data);
    writeBlobAtomic(filePath, data);
    std::vector<uint8_t> readData1 = readBlobFile(filePath);
    std::string hash2 = calculateSHA256(readData1);

    writeBlobAtomic(filePath, data);
    std::vector<uint8_t> readData2 = readBlobFile(filePath);
    std::string hash3 = calculateSHA256(readData2);

    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash2, hash3);
}

TEST_F(NsmDotBlobUtilsTest, BinaryDataIntegrity)
{
    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/binary.bin";

    // Test with all possible byte values
    std::vector<uint8_t> allBytes;
    for (int i = 0; i < 256; i++)
    {
        allBytes.push_back(static_cast<uint8_t>(i));
    }

    writeBlobAtomic(filePath, allBytes);
    std::vector<uint8_t> readData = readBlobFile(filePath);

    EXPECT_EQ(readData.size(), 256);
    EXPECT_EQ(readData, allBytes);
}
