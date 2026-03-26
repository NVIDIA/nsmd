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

#include "nsmDotBlobUtils.hpp"

#include <openssl/sha.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace nsm::dot_blob_utils
{

// Default to production paths; override in unit tests to redirect storage
const char* blobDir = DOT_BLOB_DIR;
const char* emmcBasePath = EMMC_BASE_PATH;

using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;
using sdbusplus::xyz::openbmc_project::Common::File::Error::Open;
using sdbusplus::xyz::openbmc_project::Common::File::Error::Read;
using sdbusplus::xyz::openbmc_project::Common::File::Error::Write;

std::string calculateSHA256(const std::vector<uint8_t>& data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    }
    return oss.str();
}

void ensureBlobDirectory(const std::string& dirPath)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::exists(dot_blob_utils::emmcBasePath, ec))
    {
        lg2::error(
            "DotBlobUtils: EMMC base path does not exist: path={PATH}, error={ERROR}",
            "PATH", dot_blob_utils::emmcBasePath, "ERROR", ec.message());
        throw Open();
    }

    if (!fs::exists(dirPath, ec))
    {
        lg2::info("DotBlobUtils: Creating blob directory: path={PATH}", "PATH",
                  dirPath);

        if (!fs::create_directories(dirPath, ec))
        {
            lg2::error(
                "DotBlobUtils: Failed to create blob directory: path={PATH}, error={ERROR}",
                "PATH", dirPath, "ERROR", ec.message());
            throw Write();
        }

        fs::permissions(dirPath,
                        fs::perms::owner_all | fs::perms::group_read |
                            fs::perms::group_exec | fs::perms::others_read |
                            fs::perms::others_exec,
                        ec);
        if (ec)
        {
            lg2::error(
                "DotBlobUtils: Failed to set directory permissions: path={PATH}, error={ERROR}",
                "PATH", dirPath, "ERROR", ec.message());
            throw Write();
        }
    }
}

void writeBlobAtomic(const std::string& filePath,
                     const std::vector<uint8_t>& data)
{
    namespace fs = std::filesystem;
    std::string tempPath = filePath + ".tmp";

    std::ofstream outFile(tempPath, std::ios::binary | std::ios::trunc);
    if (!outFile)
    {
        lg2::error("DotBlobUtils: Failed to create temporary file: path={PATH}",
                   "PATH", tempPath);
        throw Write();
    }

    outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (!outFile)
    {
        lg2::error(
            "DotBlobUtils: Failed to write to temporary file: path={PATH}",
            "PATH", tempPath);
        outFile.close();
        fs::remove(tempPath);
        throw Write();
    }

    outFile.close();

    std::error_code ec;
    fs::permissions(tempPath,
                    fs::perms::owner_read | fs::perms::owner_write |
                        fs::perms::group_read | fs::perms::others_read,
                    ec);
    if (ec)
    {
        lg2::error(
            "DotBlobUtils: Failed to set file permissions: path={PATH}, error={ERROR}",
            "PATH", tempPath, "ERROR", ec.message());
        fs::remove(tempPath);
        throw Write();
    }

    fs::rename(tempPath, filePath, ec);
    if (ec)
    {
        lg2::error(
            "DotBlobUtils: Failed to rename temporary file: tempPath={TEMP}, finalPath={FINAL}, error={ERROR}",
            "TEMP", tempPath, "FINAL", filePath, "ERROR", ec.message());
        fs::remove(tempPath);
        throw Write();
    }
}

std::vector<uint8_t> readBlobFile(const std::string& filePath)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::exists(filePath, ec))
    {
        lg2::error(
            "DotBlobUtils: Blob file not found: path={PATH}, error={ERROR}",
            "PATH", filePath, "ERROR", ec.message());
        throw ResourceNotFound();
    }

    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile)
    {
        lg2::error("DotBlobUtils: Failed to open blob file: path={PATH}",
                   "PATH", filePath);
        throw Read();
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(inFile)),
                              std::istreambuf_iterator<char>());
    inFile.close();

    return data;
}

std::string getBlobFilePath(const std::string& pathName)
{
    return std::string(blobDir) + "/" + pathName + ".bin";
}

} // namespace nsm::dot_blob_utils
