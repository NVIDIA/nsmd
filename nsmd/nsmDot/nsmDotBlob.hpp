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

#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "requester/handler.hpp"
#include "types.hpp"

#include <com/nvidia/Async/Status/server.hpp>
#include <com/nvidia/Dot/Blob/server.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace nsm
{

using DotBlobIntf =
    sdbusplus::server::object_t<sdbusplus::server::com::nvidia::dot::Blob>;
using AsyncStatusIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::Async::server::Status>;

/**
 * @brief Object that provides DOT Blob management functionality.
 *
 * This class implements DOT Blob storage and retrieval by inheriting from
 * com.nvidia.Dot.Blob interface. It manages DOT blobs in EMMC storage at
 * /var/emmc/misc/dot-blob/ with filenames based on the EM config PathName
 * (e.g., CPU_0.bin, CPU_1.bin).
 */
class NsmDotBlobObject : public NsmObject, public DotBlobIntf
{
  public:
    /**
     * @brief Constructs a new NsmDotBlobObject
     *
     * @param bus D-Bus bus interface for communication
     * @param name Object name from EM config (e.g., "CPU_0")
     */
    NsmDotBlobObject(sdbusplus::bus_t& bus, const std::string& name);

    /**
     * @brief Get the DOT blob from EMMC storage
     *
     * Reads the stored DOT blob from /var/emmc/misc/dot-blob/<name>.bin
     * and returns a file descriptor containing the blob data.
     *
     * @return File descriptor containing the DOT blob
     * @throws Common::File::Error::Open if EMMC path doesn't exist
     * @throws Common::File::Error::Read if blob file cannot be read
     * @throws Common::Error::ResourceNotFound if blob doesn't exist
     */
    sdbusplus::message::unix_fd getBlob() override;

    /**
     * @brief Update the DOT blob in EMMC storage (async)
     *
     * Writes the provided DOT blob to /var/emmc/misc/dot-blob/<name>.bin,
     * creating the directory if it doesn't exist. The blob is duplicated
     * from the provided file descriptor. Returns an object path that
     * implements com.nvidia.Async.Status for tracking operation status.
     *
     * @param blobFd File descriptor containing the new DOT blob data
     * @return Object path implementing com.nvidia.Async.Status interface
     * @throws Common::Error::Unavailable if no async result object available
     */
    sdbusplus::object_path
        updateBlob(sdbusplus::message::unix_fd blobFd) override;

  private:
    /**
     * @brief Async handler for UpdateBlob operation
     *
     * Performs the actual blob update operation asynchronously,
     * updating the status interface as the operation progresses.
     *
     * @param blobFd File descriptor containing the new DOT blob data
     * @param statusIntf Async status interface for tracking operation
     */
    requester::Coroutine
        updateBlobAsyncHandler(sdbusplus::message::unix_fd blobFd,
                               std::shared_ptr<AsyncStatusIntf> statusIntf);
    /**
     * @brief Get the full EMMC file path for this blob
     *
     * Constructs the path: /var/emmc/misc/dot-blob/<name>.bin
     * where <name> is from the EM config PathName (e.g., CPU_0)
     *
     * @return Full path to the blob file
     */
    std::string getBlobFilePath() const;

    /**
     * @brief Device path name from EM config (e.g., "CPU_0")
     */
    std::string pathName_;
};

} // namespace nsm
