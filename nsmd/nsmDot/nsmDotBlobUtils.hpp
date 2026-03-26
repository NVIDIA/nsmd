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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nsm::dot_blob_utils
{

constexpr const char* EMMC_BASE_PATH = "/var/emmc/misc";
constexpr const char* DOT_BLOB_DIR = "/var/emmc/misc/dot-blob";

constexpr size_t DOT_BLOB_SIZE_BYTES = 1024;

// Runtime-overridable paths (override in unit tests to redirect blob storage)
extern const char* blobDir;
extern const char* emmcBasePath;

/**
 * @brief Calculate SHA256 hash of data
 * @param data Input data to hash
 * @return Hex string representation of SHA256 hash
 */
std::string calculateSHA256(const std::vector<uint8_t>& data);

/**
 * @brief Ensure blob directory exists with correct permissions
 * @param dirPath Directory path to create
 * @throws Common::File::Error::Open if base path doesn't exist
 * @throws Common::File::Error::Write if directory creation fails
 */
void ensureBlobDirectory(const std::string& dirPath);

/**
 * @brief Write blob data to file atomically using temp file + rename
 * @param filePath Final file path
 * @param data Blob data to write
 * @throws Common::File::Error::Write if write or rename fails
 */
void writeBlobAtomic(const std::string& filePath,
                     const std::vector<uint8_t>& data);

/**
 * @brief Read blob data from file
 * @param filePath File path to read from
 * @return Blob data
 * @throws Common::File::Error::ResourceNotFound if file doesn't exist
 * @throws Common::File::Error::Read if read fails
 */
std::vector<uint8_t> readBlobFile(const std::string& filePath);

/**
 * @brief Construct blob file path from path name
 * @param pathName Path name (e.g., "CPU_0")
 * @return Full blob file path (e.g., "/var/emmc/misc/dot-blob/CPU_0.bin")
 */
std::string getBlobFilePath(const std::string& pathName);

} // namespace nsm::dot_blob_utils
