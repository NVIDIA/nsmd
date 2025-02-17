/**
 * @brief Async IPC client implementation
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gpu-telemetry/error.h"
#include <stdexec/execution.hpp>
#include <span>
#include <vector>
#include <string>

namespace gpu::telemetry {

/**
 * @brief Async IPC client using Unix domain sockets
 */
class IpcClient {
public:
    /**
     * @brief Construct client and connect to socket
     * @param socketPath Path to Unix domain socket
     * @throws std::system_error on connection failure
     */
    explicit IpcClient(const std::string& socketPath);

    /**
     * @brief Destructor closes socket
     */
    ~IpcClient();

    // Disable copying
    IpcClient(const IpcClient&) = delete;
    IpcClient& operator=(const IpcClient&) = delete;

    // Allow moving
    IpcClient(IpcClient&&) noexcept;
    IpcClient& operator=(IpcClient&&) noexcept;

    /**
     * @brief Send message asynchronously
     * @param message Message data to send
     * @return Sender for response data
     * @throws std::system_error on send failure
     */
    stdexec::sender auto sendMessage(std::span<const std::uint8_t> message);

private:
    /**
     * @brief Read response from socket
     * @return Response data
     * @throws std::system_error on read failure
     */
    std::vector<uint8_t> readResponse();

    int sockfd_{-1};                    // Socket file descriptor
    std::vector<uint8_t> readBuffer_;   // Buffer for reading responses
};

} // namespace gpu::telemetry
