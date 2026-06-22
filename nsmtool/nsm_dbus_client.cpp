/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nsm_dbus_client.hpp"

#include <sys/mman.h>
#include <unistd.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

namespace nsmtool
{
namespace dbus
{

NsmDbusClient::NsmDbusClient(sdbusplus::bus_t& bus) : bus(bus) {}

template <typename T>
T NsmDbusClient::getProperty(const std::string& path,
                             const std::string& interface,
                             const std::string& property)
{
    auto method = bus.new_method_call("xyz.openbmc_project.NSM", path.c_str(),
                                      "org.freedesktop.DBus.Properties", "Get");
    method.append(interface, property);

    auto reply = bus.call(method);
    std::variant<T> value;
    reply.read(value);

    return std::get<T>(value);
}

DeviceInfo NsmDbusClient::getDeviceInfoByEid(uint8_t eid)
{
    std::string fruPath = "/xyz/openbmc_project/FruDevice/" +
                          std::to_string(eid);

    try
    {
        auto deviceType = getProperty<uint8_t>(
            fruPath, "xyz.openbmc_project.FruDevice", "DEVICE_TYPE");

        DeviceInfo info;
        info.fruPath = fruPath;
        info.eid = eid;
        info.deviceType = deviceType;
        info.instanceId = getProperty<uint8_t>(
            fruPath, "xyz.openbmc_project.FruDevice", "INSTANCE_NUMBER");
        info.deviceRole = getProperty<uint8_t>(
            fruPath, "xyz.openbmc_project.FruDevice", "DEVICE_ROLE");
        info.uuid = getProperty<std::string>(
            fruPath, "xyz.openbmc_project.FruDevice", "UUID");

        return info;
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Device not found for EID " +
                                 std::to_string(eid) + ": " + e.what());
    }
}

int NsmDbusClient::createPayloadMemfd(const std::vector<uint8_t>& payload)
{
    int fd = memfd_create("nsm_payload", MFD_ALLOW_SEALING);
    if (fd < 0)
    {
        throw std::runtime_error("Failed to create memfd: " +
                                 std::string(strerror(errno)));
    }

    ssize_t written = write(fd, payload.data(), payload.size());
    if (written < 0 || static_cast<size_t>(written) != payload.size())
    {
        close(fd);
        throw std::runtime_error("Failed to write to memfd: " +
                                 std::string(strerror(errno)));
    }

    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        close(fd);
        throw std::runtime_error("Failed to seek memfd: " +
                                 std::string(strerror(errno)));
    }

    return fd;
}

std::vector<uint8_t> NsmDbusClient::readResponseFromMemfd(int fd)
{
    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        throw std::runtime_error("Failed to seek memfd: " +
                                 std::string(strerror(errno)));
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0)
    {
        throw std::runtime_error("Failed to get memfd size: " +
                                 std::string(strerror(errno)));
    }

    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        throw std::runtime_error("Failed to seek memfd: " +
                                 std::string(strerror(errno)));
    }

    std::vector<uint8_t> data(size);
    ssize_t bytesRead = read(fd, data.data(), size);
    if (bytesRead < 0)
    {
        throw std::runtime_error("Failed to read from memfd: " +
                                 std::string(strerror(errno)));
    }

    data.resize(bytesRead);
    return data;
}

std::string NsmDbusClient::callSendRequestDbus(
    uint8_t deviceType, uint8_t deviceRole, uint8_t instanceId,
    uint8_t messageType, uint8_t commandCode, int fd, uint8_t msgFormatVersion,
    bool isLongRunning)
{
    sdbusplus::message::unix_fd unixFd(fd);

    auto method = bus.new_method_call(
        "xyz.openbmc_project.NSM", "/xyz/openbmc_project/NSM/Raw",
        "com.nvidia.Protocol.NSM.Raw", "SendRequest");

    method.append(deviceType, deviceRole, instanceId, isLongRunning,
                  messageType, commandCode, unixFd, msgFormatVersion);

    auto reply = bus.call(method);
    sdbusplus::object_path asyncHandle;
    reply.read(asyncHandle);

    return asyncHandle.str;
}

std::string NsmDbusClient::sendRequest(uint8_t deviceType, uint8_t deviceRole,
                                       uint8_t instanceId, uint8_t messageType,
                                       uint8_t commandCode,
                                       const std::vector<uint8_t>& payload,
                                       uint8_t msgFormatVersion,
                                       bool isLongRunning)
{
    int fd = createPayloadMemfd(payload);

    try
    {
        std::string asyncHandle = callSendRequestDbus(
            deviceType, deviceRole, instanceId, messageType, commandCode, fd,
            msgFormatVersion, isLongRunning);

        close(fd);
        return asyncHandle;
    }
    catch (const std::exception& e)
    {
        close(fd);
        throw std::runtime_error("SendRequest failed: " +
                                 std::string(e.what()));
    }
}

std::string NsmDbusClient::getStatus(const std::string& asyncHandle)
{
    return getProperty<std::string>(asyncHandle, "com.nvidia.Async.Status",
                                    "Status");
}

std::vector<uint8_t> NsmDbusClient::getValue(const std::string& asyncHandle)
{
    // Value property is a variant, but for SendRequest it's always array[byte]
    auto method = bus.new_method_call("xyz.openbmc_project.NSM",
                                      asyncHandle.c_str(),
                                      "org.freedesktop.DBus.Properties", "Get");
    method.append("com.nvidia.Async.Value", "Value");

    auto reply = bus.call(method);
    std::variant<std::vector<uint8_t>> value;
    reply.read(value);

    return std::get<std::vector<uint8_t>>(value);
}

std::vector<uint8_t>
    NsmDbusClient::pollForCompletion(const std::string& asyncHandle,
                                     int timeoutSeconds, bool verbose)
{
    auto startTime = std::chrono::steady_clock::now();
    auto timeoutDuration = std::chrono::seconds(timeoutSeconds);
    int pollCount = 0;

    if (verbose)
    {
        std::cout << "Polling for async completion (timeout: " << timeoutSeconds
                  << "s)..." << std::endl;
    }

    while (true)
    {
        std::string status;
        try
        {
            status = getStatus(asyncHandle);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error("Failed to get status: " +
                                     std::string(e.what()));
        }

        pollCount++;
        if (verbose && pollCount == 1)
        {
            std::cout << "Async status: " << status << std::endl;
        }

        bool isInProgress = (status == "InProgress" ||
                             status.find("InProgress") != std::string::npos);

        if (!isInProgress)
        {
            if (verbose)
            {
                std::cout << "Command completed with status: " << status
                          << " (after " << pollCount << " polls)" << std::endl;
            }

            bool isSuccess = (status == "WriteSuccess" ||
                              status == "ReadSuccess" || status == "Success" ||
                              status.find("Success") != std::string::npos);

            if (isSuccess)
            {
                return std::vector<uint8_t>();
            }
            else
            {
                throw std::runtime_error("Command failed with status: " +
                                         status);
            }
        }

        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed >= timeoutDuration)
        {
            throw std::runtime_error(
                "Timeout waiting for command completion (status: " + status +
                ")");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

std::vector<uint8_t> NsmDbusClient::executeCommand(
    uint8_t eid, uint8_t messageType, uint8_t commandCode,
    const std::vector<uint8_t>& payload, uint8_t msgFormatVersion,
    bool isLongRunning, int timeoutSeconds, bool verbose)
{
    DeviceInfo deviceInfo = getDeviceInfoByEid(eid);

    if (verbose)
    {
        std::cout << "nsmtool: <6> EID: " << static_cast<int>(eid)
                  << ", Tx Parameters: MsgType=" << std::hex
                  << std::setfill('0') << std::setw(2)
                  << static_cast<int>(messageType)
                  << ", CmdCode=" << std::setw(2)
                  << static_cast<int>(commandCode);

        if (!payload.empty())
        {
            std::cout << ", Payload=[";
            for (size_t i = 0; i < payload.size(); ++i)
            {
                if (i > 0)
                    std::cout << " ";
                std::cout << std::setw(2) << static_cast<int>(payload[i]);
            }
            std::cout << "]";
        }
        std::cout << std::dec << std::endl;

        std::cout << "Sending via DBus to nsmd (DeviceType="
                  << static_cast<int>(deviceInfo.deviceType)
                  << ", Role=" << static_cast<int>(deviceInfo.deviceRole)
                  << ", Instance=" << static_cast<int>(deviceInfo.instanceId)
                  << ")" << std::endl;
    }

    int fd = createPayloadMemfd(payload);

    try
    {
        std::string asyncHandle = callSendRequestDbus(
            deviceInfo.deviceType, deviceInfo.deviceRole, deviceInfo.instanceId,
            messageType, commandCode, fd, msgFormatVersion, isLongRunning);

        pollForCompletion(asyncHandle, timeoutSeconds,
                          false); // Don't show polling details

        auto response = readResponseFromMemfd(fd);

        close(fd);

        if (verbose)
        {
            std::cout << "nsmtool: <6> EID: " << static_cast<int>(eid)
                      << ", Response Data: [" << std::hex << std::setfill('0');
            for (size_t i = 0; i < response.size(); ++i)
            {
                if (i > 0)
                    std::cout << " ";
                std::cout << std::setw(2) << static_cast<int>(response[i]);
            }
            std::cout << "]" << std::dec << std::endl;
        }

        return response;
    }
    catch (const std::exception& e)
    {
        close(fd);
        throw;
    }
}

} // namespace dbus
} // namespace nsmtool
