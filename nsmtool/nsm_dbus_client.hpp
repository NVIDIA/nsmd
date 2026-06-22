/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sdbusplus/bus.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace nsmtool
{
namespace dbus
{

/**
 * @brief Device information retrieved from FRU
 */
struct DeviceInfo
{
    std::string fruPath;
    uint8_t deviceType;
    uint8_t instanceId;
    uint8_t deviceRole;
    uint8_t eid;
    std::string uuid;
};

/**
 * @brief DBus client for NSM raw command execution
 *
 * This class provides utilities to execute NSM commands via DBus
 * using the existing com.nvidia.Protocol.NSM.Raw.SendRequest interface.
 */
class NsmDbusClient
{
  public:
    /**
     * @brief Construct DBus client
     */
    explicit NsmDbusClient(sdbusplus::bus_t& bus);

    /**
     * @brief Find device by EID and get its properties
     *
     * @param eid MCTP Endpoint ID
     * @return DeviceInfo structure with device properties
     * @throws std::runtime_error if device not found
     */
    DeviceInfo getDeviceInfoByEid(uint8_t eid);

    /**
     * @brief Create memfd with payload data
     *
     * Creates an anonymous memory file descriptor and writes payload to it.
     * Caller is responsible for closing the returned fd.
     *
     * @param payload Data to write to memfd
     * @return File descriptor (must be closed by caller)
     * @throws std::runtime_error on failure
     */
    int createPayloadMemfd(const std::vector<uint8_t>& payload);

    /**
     * @brief Execute NSM command via SendRequest DBus method
     *
     * @param deviceType Device type (from FRU)
     * @param deviceRole Device role (from FRU)
     * @param instanceId Instance ID (from FRU)
     * @param messageType NSM message type
     * @param commandCode NSM command code
     * @param payload Command payload
     * @param msgFormatVersion NSM message format version (1 or 2)
     * @param isLongRunning Whether command is long-running
     * @return AsyncHandle object path
     * @throws std::runtime_error on failure
     */
    std::string sendRequest(uint8_t deviceType, uint8_t deviceRole,
                            uint8_t instanceId, uint8_t messageType,
                            uint8_t commandCode,
                            const std::vector<uint8_t>& payload,
                            uint8_t msgFormatVersion = 1,
                            bool isLongRunning = false);

    /**
     * @brief Get current status of async operation
     *
     * @param asyncHandle Object path returned by sendRequest
     * @return Status string (e.g., "InProgress", "WriteSuccess")
     * @throws std::runtime_error on failure
     */
    std::string getStatus(const std::string& asyncHandle);

    /**
     * @brief Get response value from completed async operation
     *
     * @param asyncHandle Object path returned by sendRequest
     * @return Response payload bytes
     * @throws std::runtime_error on failure
     */
    std::vector<uint8_t> getValue(const std::string& asyncHandle);

    /**
     * @brief Poll for command completion
     *
     * Polls status until completion or timeout.
     *
     * @param asyncHandle Object path returned by sendRequest
     * @param timeoutSeconds Maximum time to wait (default 30s)
     * @param verbose Enable verbose status logging
     * @return Response payload bytes
     * @throws std::runtime_error on timeout or error
     */
    std::vector<uint8_t> pollForCompletion(const std::string& asyncHandle,
                                           int timeoutSeconds = 30,
                                           bool verbose = false);

    /**
     * @brief Execute command and wait for response (convenience method)
     *
     * Combines sendRequest + pollForCompletion.
     *
     * @param eid MCTP Endpoint ID
     * @param messageType NSM message type
     * @param commandCode NSM command code
     * @param payload Command payload
     * @param msgFormatVersion NSM message format version (1 or 2)
     * @param isLongRunning Whether command is long-running
     * @param timeoutSeconds Maximum time to wait
     * @return Response payload bytes
     * @throws std::runtime_error on failure
     */
    std::vector<uint8_t>
        executeCommand(uint8_t eid, uint8_t messageType, uint8_t commandCode,
                       const std::vector<uint8_t>& payload,
                       uint8_t msgFormatVersion = 1, bool isLongRunning = false,
                       int timeoutSeconds = 30, bool verbose = false);

  private:
    sdbusplus::bus_t& bus;

    /**
     * @brief Get DBus property
     *
     * @tparam T Property type
     * @param path Object path
     * @param interface Interface name
     * @param property Property name
     * @return Property value
     */
    template <typename T>
    T getProperty(const std::string& path, const std::string& interface,
                  const std::string& property);

    /**
     * @brief Read response data from memfd
     *
     * @param fd File descriptor to read from
     * @return Response payload bytes
     * @throws std::runtime_error on failure
     */
    std::vector<uint8_t> readResponseFromMemfd(int fd);

    /**
     * @brief Internal helper to make the SendRequest DBus call
     *
     * This method performs the DBus call without managing fd lifecycle.
     * Caller is responsible for creating and closing the fd.
     *
     * @param deviceType Device type
     * @param deviceRole Device role
     * @param instanceId Instance ID
     * @param messageType NSM message type
     * @param commandCode NSM command code
     * @param fd File descriptor containing payload
     * @param msgFormatVersion NSM message format version
     * @param isLongRunning Whether command is long-running
     * @return AsyncHandle object path
     * @throws std::runtime_error on failure
     */
    std::string callSendRequestDbus(uint8_t deviceType, uint8_t deviceRole,
                                    uint8_t instanceId, uint8_t messageType,
                                    uint8_t commandCode, int fd,
                                    uint8_t msgFormatVersion,
                                    bool isLongRunning);
};

} // namespace dbus
} // namespace nsmtool
