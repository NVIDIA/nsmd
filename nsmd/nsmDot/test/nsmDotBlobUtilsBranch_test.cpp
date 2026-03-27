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

/*
 * Branch coverage tests for nsmDotBlobUtils.cpp
 *
 * Targets:
 * - ensureBlobDirectory line 62: emmcBasePath does not exist → throws Open()
 * - ensureBlobDirectory line 70: dirPath already exists → no create_directories
 * - ensureBlobDirectory: emmcBasePath exists, dirPath doesn't → creates dir
 * - readBlobFile line 164: file exists but cannot be opened (no read perms)
 */

#include "nsmDotBlobUtils.hpp"

#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

using namespace nsm::dot_blob_utils;

// Save & restore the global pointers around each test
class NsmDotBlobUtilsBranchTest : public ::testing::Test
{
  protected:
    const char* savedEmmcBasePath = nullptr;
    const char* savedBlobDir = nullptr;
    std::string testDir = "/tmp/nsm_dot_blob_branch_test";

    void SetUp() override
    {
        savedEmmcBasePath = emmcBasePath;
        savedBlobDir = blobDir;
        std::filesystem::remove_all(testDir);
    }

    void TearDown() override
    {
        emmcBasePath = savedEmmcBasePath;
        blobDir = savedBlobDir;
        std::filesystem::remove_all(testDir);
    }
};

// ============================================================================
// ensureBlobDirectory: EMMC base path does not exist → throws Open()
// Line 62: if (!fs::exists(dot_blob_utils::emmcBasePath, ec)) TRUE branch
// ============================================================================

TEST_F(NsmDotBlobUtilsBranchTest, EnsureBlobDirectory_EmmcMissing_ThrowsOpen)
{
    emmcBasePath = "/tmp/nonexistent_emmc_path_nsmd_test_xyz_abc";

    EXPECT_THROW(ensureBlobDirectory("/tmp/some_dir"),
                 sdbusplus::xyz::openbmc_project::Common::File::Error::Open);
}

// ============================================================================
// ensureBlobDirectory: EMMC exists, dirPath already exists → no error, no
// create_directories call. Line 70: if (!fs::exists(dirPath, ec)) FALSE branch
// ============================================================================

TEST_F(NsmDotBlobUtilsBranchTest, EnsureBlobDirectory_DirAlreadyExists_NoError)
{
    // Create both the emmc base and the target dir
    std::string emmcDir = testDir + "/emmc";
    std::string blobTarget = testDir + "/blobs";
    std::filesystem::create_directories(emmcDir);
    std::filesystem::create_directories(blobTarget);

    emmcBasePath = emmcDir.c_str();

    // dirPath already exists — should succeed without throwing
    EXPECT_NO_THROW(ensureBlobDirectory(blobTarget));
    EXPECT_TRUE(std::filesystem::exists(blobTarget));
}

// ============================================================================
// ensureBlobDirectory: EMMC exists, dirPath doesn't → creates dir successfully
// Line 70 TRUE branch: create_directories called and succeeds
// ============================================================================

TEST_F(NsmDotBlobUtilsBranchTest,
       EnsureBlobDirectory_DirMissing_CreatesDirectory)
{
    std::string emmcDir = testDir + "/emmc2";
    std::string newBlobDir = testDir + "/new_blobs";
    std::filesystem::create_directories(emmcDir);
    // newBlobDir does not exist yet

    emmcBasePath = emmcDir.c_str();

    EXPECT_NO_THROW(ensureBlobDirectory(newBlobDir));
    EXPECT_TRUE(std::filesystem::exists(newBlobDir));
}

// ============================================================================
// readBlobFile: file exists but cannot be opened (permissions removed)
// Line 164: if (!inFile) TRUE branch → throws Read()
// Only runs as non-root (root can open any file regardless of perms)
// ============================================================================

TEST_F(NsmDotBlobUtilsBranchTest, ReadBlobFile_NoReadPermission_ThrowsReadError)
{
    // Skip test if running as root (root ignores read permissions)
    if (getuid() == 0)
    {
        GTEST_SKIP() << "Skipping: running as root, permission test N/A";
    }

    std::filesystem::create_directories(testDir);
    std::string filePath = testDir + "/noperm.bin";

    // Create the file with content, then remove read permissions
    std::ofstream f(filePath, std::ios::binary);
    f << "data";
    f.close();
    std::filesystem::permissions(filePath, std::filesystem::perms::none);

    using ReadErr = sdbusplus::xyz::openbmc_project::Common::File::Error::Read;
    EXPECT_THROW(readBlobFile(filePath), ReadErr);

    // Restore permissions so TearDown can remove it
    std::filesystem::permissions(filePath,
                                 std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_write);
}
