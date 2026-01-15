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

#include "nsmDotBlob.hpp"

#include "common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "globals.hpp"
#include "nsmDotBlobUtils.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSetAsync/asyncOperationManager.hpp"
#include "sensorManager.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

namespace nsm
{

using Common::Error::InvalidArgument;
using Common::Error::ResourceNotFound;
using Common::File::Error::Open;
using Common::File::Error::Read;
using Common::File::Error::Write;

NsmDotBlobObject::NsmDotBlobObject(sdbusplus::bus::bus& bus,
                                   const std::string& name) :
    NsmObject(name, "NSM_DotBlob"),
    DotBlobIntf(bus, ("/xyz/openbmc_project/dot_blob/" + name).c_str()),
    pathName_(name)
{
    lg2::info("DotBlob: Created DOT blob object: name={NAME}, path={PATH}",
              "NAME", name, "PATH", getBlobFilePath());
}

std::string NsmDotBlobObject::getBlobFilePath() const
{
    return dot_blob_utils::getBlobFilePath(pathName_);
}

sdbusplus::message::unix_fd NsmDotBlobObject::getBlob()
{
    lg2::debug("DotBlob: GetBlob called for: name={NAME}", "NAME", pathName_);

    std::string blobPath = getBlobFilePath();

    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(blobPath, ec))
    {
        lg2::error("DotBlob: Blob file not found: path={PATH}, error={ERROR}",
                   "PATH", blobPath, "ERROR", ec.message());
        throw ResourceNotFound();
    }

    int fd = open(blobPath.c_str(), O_RDONLY);
    if (fd < 0)
    {
        lg2::error(
            "DotBlob: Failed to open blob file: path={PATH}, errno={ERRNO}, error={ERROR}",
            "PATH", blobPath, "ERRNO", errno, "ERROR", std::strerror(errno));
        throw Open();
    }

    struct stat st;
    if (fstat(fd, &st) != 0)
    {
        close(fd);
        lg2::error(
            "DotBlob: Failed to stat blob file: path={PATH}, errno={ERRNO}, error={ERROR}",
            "PATH", blobPath, "ERRNO", errno, "ERROR", std::strerror(errno));
        throw Read();
    }

    if (static_cast<size_t>(st.st_size) != dot_blob_utils::DOT_BLOB_SIZE_BYTES)
    {
        close(fd);
        lg2::error(
            "DotBlob: Blob file size exceeds maximum allowed: size={SIZE}, max={MAX}",
            "SIZE", st.st_size, "MAX", dot_blob_utils::DOT_BLOB_SIZE_BYTES);
        throw Read();
    }

    lg2::info("DotBlob: GetBlob success: name={NAME}, path={PATH}, size={SIZE}",
              "NAME", pathName_, "PATH", blobPath, "SIZE", st.st_size);

    return sdbusplus::message::unix_fd(fd);
}

requester::Coroutine NsmDotBlobObject::updateBlobAsyncHandler(
    sdbusplus::message::unix_fd blobFd,
    std::shared_ptr<AsyncStatusIntf> statusIntf)
{
    using AsyncOperationStatusType =
        sdbusplus::common::com::nvidia::async::Status::AsyncOperationStatus;

    statusIntf->status(AsyncOperationStatusType::InProgress);

    lg2::debug("DotBlob: UpdateBlob async handler started for: name={NAME}",
               "NAME", pathName_);

    try
    {
        dot_blob_utils::ensureBlobDirectory(dot_blob_utils::DOT_BLOB_DIR);

        // FD is already duplicated in updateBlob(), just use it directly
        int sourceFd = blobFd;

        struct stat fileStat;
        if (fstat(sourceFd, &fileStat) != 0)
        {
            close(sourceFd);
            lg2::error(
                "DotBlob: Failed to stat file descriptor: errno={ERRNO}, error={ERROR}",
                "ERRNO", errno, "ERROR", std::strerror(errno));
            statusIntf->status(AsyncOperationStatusType::InternalFailure);
            co_return NSM_SW_ERROR;
        }

        std::vector<uint8_t> blobData(static_cast<size_t>(fileStat.st_size));

        if (lseek(sourceFd, 0, SEEK_SET) < 0)
        {
            close(sourceFd);
            lg2::error(
                "DotBlob: Failed to seek file descriptor: errno={ERRNO}, error={ERROR}",
                "ERRNO", errno, "ERROR", std::strerror(errno));
            statusIntf->status(AsyncOperationStatusType::InternalFailure);
            co_return NSM_SW_ERROR;
        }

        ssize_t bytesRead = read(sourceFd, blobData.data(), blobData.size());
        close(sourceFd);

        if (bytesRead < 0 || static_cast<size_t>(bytesRead) != blobData.size())
        {
            lg2::error(
                "DotBlob: Failed to read complete data from file descriptor: expected={EXPECTED}, actual={ACTUAL}",
                "EXPECTED", blobData.size(), "ACTUAL", bytesRead);
            statusIntf->status(AsyncOperationStatusType::InternalFailure);
            co_return NSM_SW_ERROR;
        }

        std::string blobPath = getBlobFilePath();

        try
        {
            dot_blob_utils::writeBlobAtomic(blobPath, blobData);
        }
        catch (
            const sdbusplus::xyz::openbmc_project::Common::File::Error::Write&)
        {
            lg2::error("DotBlob: Failed to write blob atomically: path={PATH}",
                       "PATH", blobPath);
            statusIntf->status(AsyncOperationStatusType::WriteFailure);
            co_return NSM_SW_ERROR;
        }

        lg2::info(
            "DotBlob: UpdateBlob success: name={NAME}, path={PATH}, size={SIZE}",
            "NAME", pathName_, "PATH", blobPath, "SIZE", blobData.size());

        statusIntf->status(AsyncOperationStatusType::Success);
    }
    catch (const std::exception& e)
    {
        lg2::error("DotBlob: UpdateBlob async handler exception: error={ERROR}",
                   "ERROR", e.what());
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
    }

    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path
    NsmDotBlobObject::updateBlob(sdbusplus::message::unix_fd blobFd)
{
    lg2::debug("DotBlob: UpdateBlob called for: name={NAME}", "NAME",
               pathName_);

    auto dupFd = dup(blobFd);
    if (dupFd < 0)
    {
        lg2::error("DotBlob: Failed to duplicate file descriptor: {ERR}", "ERR",
                   strerror(errno));
        throw Common::Error::InternalFailure();
    }

    auto seekRc = lseek(dupFd, 0, SEEK_SET);
    if (seekRc < 0)
    {
        lg2::error("DotBlob: lseek failed {RETURNCODE} {ERROR}", "RETURNCODE",
                   seekRc, "ERROR", strerror(errno));
        close(dupFd);
        throw Common::Error::InternalFailure();
    }

    struct stat fileStat;
    auto fstatRc = fstat(dupFd, &fileStat);
    if (fstatRc < 0)
    {
        lg2::error("DotBlob: fstat failed {RETURNCODE} {ERROR}", "RETURNCODE",
                   fstatRc, "ERROR", strerror(errno));
        close(dupFd);
        throw Common::Error::InternalFailure();
    }

    if (fileStat.st_size < 0 || static_cast<size_t>(fileStat.st_size) !=
                                    dot_blob_utils::DOT_BLOB_SIZE_BYTES)
    {
        lg2::error(
            "DotBlob: Invalid file size: expected={EXPECTED}, actual={ACTUAL}",
            "EXPECTED", dot_blob_utils::DOT_BLOB_SIZE_BYTES, "ACTUAL",
            fileStat.st_size);
        close(dupFd);
        throw Common::Error::InvalidArgument();
    }

    const auto [objPath, statusIntf] =
        AsyncOperationManager::getInstance()->getNewStatusInterface();
    if (objPath.empty())
    {
        lg2::error("DotBlob: No available async result object for UpdateBlob");
        close(dupFd);
        throw Common::Error::Unavailable();
    }

    updateBlobAsyncHandler(sdbusplus::message::unix_fd(dupFd), statusIntf)
        .detach();

    return objPath;
}

static requester::Coroutine createNsmDotBlob(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap allBaseIfaceProperties;
    auto rc = co_await utils::coGetCachedBaseProperties(objPath, interface,
                                                        allBaseIfaceProperties);
    if (rc != NSM_SUCCESS)
    {
        co_return rc;
    }

    std::string pathName{};
    if (allBaseIfaceProperties.count("PathName"))
    {
        pathName = std::get<std::string>(allBaseIfaceProperties.at("PathName"));
    }

    if (pathName.empty())
    {
        lg2::error("DotBlob: PathName is missing or empty in EM config");
        co_return NSM_ERR_INVALID_DATA;
    }

    uuid_t uuid{};
    if (allBaseIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
    }

    auto device = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("DotBlob: Failed to find device for UUID, pathName={PATH}",
                   "PATH", pathName);
        co_return NSM_SW_ERROR;
    }

    auto dotBlobObject = std::make_shared<NsmDotBlobObject>(bus, pathName);
    device->addStaticSensor(dotBlobObject);

    lg2::info("DotBlob: Created and added DOT blob object: pathName={PATH}",
              "PATH", pathName);

    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(createNsmDotBlob,
                               "xyz.openbmc_project.Configuration.NSM_DOTBlob")

} // namespace nsm
