/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include "test/mockDBusHandler.hpp"

#define private public
#define protected public

#include "nsmDotBlob.hpp"
#include "nsmDotBlobUtils.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

using namespace nsm;
using namespace ::testing;

namespace fs = std::filesystem;

// Use the constant from dot_blob_utils namespace
constexpr size_t DOT_BLOB_SIZE = dot_blob_utils::DOT_BLOB_SIZE_BYTES;
constexpr const char* TEST_BLOB_DIR = "/tmp/nsm_dotblob_test";

class NsmDotBlobTest : public Test, public utils::DBusTest
{
  protected:
    void SetUp() override
    {
        // Override production paths to use temp directory in tests
        dot_blob_utils::blobDir = TEST_BLOB_DIR;
        dot_blob_utils::emmcBasePath = "/tmp";

        testBlobDir = TEST_BLOB_DIR;
        std::error_code ec;
        fs::remove_all(testBlobDir, ec);
        fs::create_directories(testBlobDir, ec);

        dotBlobObject = std::make_unique<NsmDotBlobObject>(
            utils::DBusHandler::getBus(), "CPU_0");

        dotBlobObject->pathName_ = "CPU_0";
    }

    void TearDown() override
    {
        dotBlobObject.reset();
        std::error_code ec;
        fs::remove_all(testBlobDir, ec);
        // Restore production paths
        dot_blob_utils::blobDir = dot_blob_utils::DOT_BLOB_DIR;
        dot_blob_utils::emmcBasePath = dot_blob_utils::EMMC_BASE_PATH;
    }

    std::vector<uint8_t> createTestBlobData(size_t size = DOT_BLOB_SIZE,
                                            uint8_t fillValue = 0x42)
    {
        return std::vector<uint8_t>(size, fillValue);
    }

    int createTempFileWithData(const std::vector<uint8_t>& data)
    {
        char tempPath[] = "/tmp/dotblob_test_XXXXXX";
        int fd = mkstemp(tempPath);
        EXPECT_NE(fd, -1);

        if (fd != -1)
        {
            ssize_t written = write(fd, data.data(), data.size());
            EXPECT_EQ(written, static_cast<ssize_t>(data.size()));
            lseek(fd, 0, SEEK_SET);
        }

        return fd;
    }

    void writeBlobToFile(const std::string& path,
                         const std::vector<uint8_t>& data)
    {
        std::ofstream outFile(path, std::ios::binary);
        outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
        outFile.close();
    }

    std::vector<uint8_t> readBlobFromFile(const std::string& path)
    {
        std::ifstream inFile(path, std::ios::binary);
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(inFile)),
                                    std::istreambuf_iterator<char>());
    }

    std::string getBlobFilePath() const
    {
        return dotBlobObject->getBlobFilePath();
    }

    std::string testBlobDir;
    std::unique_ptr<NsmDotBlobObject> dotBlobObject;
};

TEST_F(NsmDotBlobTest, UpdateBlobCreatesNewFile)
{
    auto testData = createTestBlobData();
    int fd = createTempFileWithData(testData);
    ASSERT_NE(fd, -1);

    sdbusplus::message::unix_fd blobFd(fd);

    EXPECT_NO_THROW(dotBlobObject->updateBlob(blobFd));

    close(fd);

    std::string blobPath = getBlobFilePath();
    EXPECT_TRUE(fs::exists(blobPath));

    auto readData = readBlobFromFile(blobPath);
    EXPECT_EQ(readData, testData);
}

TEST_F(NsmDotBlobTest, UpdateBlobOverwritesExistingFile)
{
    auto oldData = createTestBlobData(DOT_BLOB_SIZE, 0x11);
    std::string blobPath = getBlobFilePath();
    writeBlobToFile(blobPath, oldData);

    auto newData = createTestBlobData(DOT_BLOB_SIZE, 0x22);
    int fd = createTempFileWithData(newData);
    ASSERT_NE(fd, -1);

    sdbusplus::message::unix_fd blobFd(fd);

    EXPECT_NO_THROW(dotBlobObject->updateBlob(blobFd));

    close(fd);

    auto readData = readBlobFromFile(blobPath);
    EXPECT_EQ(readData, newData);
    EXPECT_NE(readData, oldData);
}

TEST_F(NsmDotBlobTest, UpdateBlobHandlesEmptyData)
{
    std::vector<uint8_t> emptyData;
    int fd = createTempFileWithData(emptyData);
    ASSERT_NE(fd, -1);

    sdbusplus::message::unix_fd blobFd(fd);

    EXPECT_THROW(
        {
            try
            {
                dotBlobObject->updateBlob(blobFd);
            }
            catch (const sdbusplus::xyz::openbmc_project::Common::Error::
                       InvalidArgument& e)
            {
                throw;
            }
        },
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);

    close(fd);
}

TEST_F(NsmDotBlobTest, UpdateBlobHandlesInvalidFd)
{
    // dup(-1) fails → updateBlob throws InternalFailure (not File::Error::Open)
    sdbusplus::message::unix_fd invalidFd(-1);

    EXPECT_THROW(
        {
            try
            {
                dotBlobObject->updateBlob(invalidFd);
            }
            catch (const sdbusplus::xyz::openbmc_project::Common::Error::
                       InternalFailure& e)
            {
                throw;
            }
        },
        sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure);
}

TEST_F(NsmDotBlobTest, UpdateBlobHandlesOversizedData)
{
    auto largeData = createTestBlobData(DOT_BLOB_SIZE + 100, 0x55);
    int fd = createTempFileWithData(largeData);
    ASSERT_NE(fd, -1);

    sdbusplus::message::unix_fd blobFd(fd);

    EXPECT_THROW(
        {
            try
            {
                dotBlobObject->updateBlob(blobFd);
            }
            catch (const sdbusplus::xyz::openbmc_project::Common::Error::
                       InvalidArgument& e)
            {
                throw;
            }
        },
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);

    close(fd);

    std::string blobPath = getBlobFilePath();
    EXPECT_FALSE(fs::exists(blobPath));
}

TEST_F(NsmDotBlobTest, UpdateBlobCreatesDirectoryIfMissing)
{
    std::error_code ec;
    std::string blobDir =
        fs::path(dotBlobObject->getBlobFilePath()).parent_path();
    fs::remove_all(blobDir, ec);
    EXPECT_FALSE(fs::exists(blobDir));

    auto testData = createTestBlobData();
    int fd = createTempFileWithData(testData);
    ASSERT_NE(fd, -1);

    sdbusplus::message::unix_fd blobFd(fd);

    EXPECT_NO_THROW(dotBlobObject->updateBlob(blobFd));

    close(fd);

    EXPECT_TRUE(fs::exists(blobDir));
    EXPECT_TRUE(fs::exists(getBlobFilePath()));
}

TEST_F(NsmDotBlobTest, GetBlobReturnsExistingBlob)
{
    auto testData = createTestBlobData();
    std::string blobPath = getBlobFilePath();
    writeBlobToFile(blobPath, testData);

    sdbusplus::message::unix_fd resultFd;
    EXPECT_NO_THROW(resultFd = dotBlobObject->getBlob());

    int fd = resultFd;
    ASSERT_NE(fd, -1);

    std::vector<uint8_t> readData(DOT_BLOB_SIZE);
    ssize_t bytesRead = read(fd, readData.data(), readData.size());
    EXPECT_EQ(bytesRead, static_cast<ssize_t>(DOT_BLOB_SIZE));

    close(fd);

    EXPECT_EQ(readData, testData);
}

TEST_F(NsmDotBlobTest, GetBlobThrowsWhenFileNotFound)
{
    std::string blobPath = getBlobFilePath();
    EXPECT_FALSE(fs::exists(blobPath));

    EXPECT_THROW(
        {
            try
            {
                dotBlobObject->getBlob();
            }
            catch (const sdbusplus::xyz::openbmc_project::Common::Error::
                       ResourceNotFound& e)
            {
                throw;
            }
        },
        sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound);
}

TEST_F(NsmDotBlobTest, GetBlobThrowsWhenDirectoryNotFound)
{
    std::error_code ec;
    std::string blobDir =
        fs::path(dotBlobObject->getBlobFilePath()).parent_path();
    fs::remove_all(blobDir, ec);

    EXPECT_THROW(
        {
            try
            {
                dotBlobObject->getBlob();
            }
            catch (const sdbusplus::xyz::openbmc_project::Common::Error::
                       ResourceNotFound& e)
            {
                throw;
            }
        },
        sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound);
}

TEST_F(NsmDotBlobTest, GetBlobHandlesEmptyFile)
{
    // getBlob() requires exactly DOT_BLOB_SIZE bytes; empty file (0 bytes)
    // triggers the size check and throws File::Error::Read
    std::string blobPath = getBlobFilePath();
    std::ofstream outFile(blobPath, std::ios::binary);
    outFile.close();

    EXPECT_THROW(
        {
            try
            {
                dotBlobObject->getBlob();
            }
            catch (const sdbusplus::xyz::openbmc_project::Common::File::Error::
                       Read& e)
            {
                throw;
            }
        },
        sdbusplus::xyz::openbmc_project::Common::File::Error::Read);
}

TEST_F(NsmDotBlobTest, GetBlobFilePathIsCorrect)
{
    std::string actualPath = dotBlobObject->getBlobFilePath();
    EXPECT_EQ(actualPath, std::string(TEST_BLOB_DIR) + "/CPU_0.bin");
    EXPECT_EQ(actualPath, getBlobFilePath());
}

TEST_F(NsmDotBlobTest, UpdateAndGetBlobRoundTrip)
{
    auto testData = createTestBlobData(DOT_BLOB_SIZE, 0x99);
    int writeFd = createTempFileWithData(testData);
    ASSERT_NE(writeFd, -1);

    sdbusplus::message::unix_fd blobFdWrite(writeFd);
    EXPECT_NO_THROW(dotBlobObject->updateBlob(blobFdWrite));
    close(writeFd);

    sdbusplus::message::unix_fd blobFdRead;
    EXPECT_NO_THROW(blobFdRead = dotBlobObject->getBlob());

    int readFd = blobFdRead;
    ASSERT_NE(readFd, -1);

    std::vector<uint8_t> readData(DOT_BLOB_SIZE);
    ssize_t bytesRead = read(readFd, readData.data(), readData.size());
    EXPECT_EQ(bytesRead, static_cast<ssize_t>(DOT_BLOB_SIZE));

    close(readFd);

    EXPECT_EQ(readData, testData);
}

TEST_F(NsmDotBlobTest, UpdateBlobAtomicWrite)
{
    auto testData = createTestBlobData();
    std::string blobPath = getBlobFilePath();
    std::string tempPath = blobPath + ".tmp";

    int fd = createTempFileWithData(testData);
    ASSERT_NE(fd, -1);

    sdbusplus::message::unix_fd blobFd(fd);
    EXPECT_NO_THROW(dotBlobObject->updateBlob(blobFd));

    close(fd);

    EXPECT_TRUE(fs::exists(blobPath));
    EXPECT_FALSE(fs::exists(tempPath));
}

TEST_F(NsmDotBlobTest, UpdateBlobWithDifferentPathNames)
{
    dotBlobObject->pathName_ = "CPU_1";

    auto testData = createTestBlobData(DOT_BLOB_SIZE, 0xAA);
    int fd = createTempFileWithData(testData);
    ASSERT_NE(fd, -1);

    sdbusplus::message::unix_fd blobFd(fd);
    EXPECT_NO_THROW(dotBlobObject->updateBlob(blobFd));

    close(fd);

    std::string expectedPath = dotBlobObject->getBlobFilePath();
    EXPECT_TRUE(fs::exists(expectedPath));
    EXPECT_EQ(expectedPath, std::string(TEST_BLOB_DIR) + "/CPU_1.bin");

    auto readData = readBlobFromFile(expectedPath);
    EXPECT_EQ(readData, testData);
}

TEST_F(NsmDotBlobTest, GetBlobReturnsReadOnlyFd)
{
    auto testData = createTestBlobData();
    std::string blobPath = getBlobFilePath();
    writeBlobToFile(blobPath, testData);

    sdbusplus::message::unix_fd resultFd;
    EXPECT_NO_THROW(resultFd = dotBlobObject->getBlob());

    int fd = resultFd;
    ASSERT_NE(fd, -1);

    uint8_t writeByte = 0xFF;
    ssize_t written = write(fd, &writeByte, 1);
    EXPECT_EQ(written, -1);
    EXPECT_EQ(errno, EBADF);

    close(fd);
}

TEST_F(NsmDotBlobTest, MultipleUpdateBlobCallsSucceed)
{
    for (int i = 0; i < 5; ++i)
    {
        auto testData = createTestBlobData(DOT_BLOB_SIZE, 0x10 + i);
        int fd = createTempFileWithData(testData);
        ASSERT_NE(fd, -1);

        sdbusplus::message::unix_fd blobFd(fd);
        EXPECT_NO_THROW(dotBlobObject->updateBlob(blobFd));

        close(fd);

        std::string blobPath = getBlobFilePath();
        auto readData = readBlobFromFile(blobPath);
        EXPECT_EQ(readData, testData);
    }
}

TEST_F(NsmDotBlobTest, UpdateBlobHandlesUndersizedData)
{
    auto smallData = createTestBlobData(DOT_BLOB_SIZE - 100, 0x33);
    int fd = createTempFileWithData(smallData);
    ASSERT_NE(fd, -1);

    sdbusplus::message::unix_fd blobFd(fd);

    EXPECT_THROW(
        {
            try
            {
                dotBlobObject->updateBlob(blobFd);
            }
            catch (const sdbusplus::xyz::openbmc_project::Common::Error::
                       InvalidArgument& e)
            {
                throw;
            }
        },
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);

    close(fd);

    std::string blobPath = getBlobFilePath();
    EXPECT_FALSE(fs::exists(blobPath));
}

TEST_F(NsmDotBlobTest, GetBlobThrowsOnOversizedFile)
{
    auto largeData = createTestBlobData(DOT_BLOB_SIZE + 200, 0x77);
    std::string blobPath = getBlobFilePath();
    writeBlobToFile(blobPath, largeData);

    EXPECT_THROW(
        {
            try
            {
                dotBlobObject->getBlob();
            }
            catch (const sdbusplus::xyz::openbmc_project::Common::File::Error::
                       Read& e)
            {
                throw;
            }
        },
        sdbusplus::xyz::openbmc_project::Common::File::Error::Read);
}
