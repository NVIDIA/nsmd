/**
 * @brief Async IPC client implementation
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ipc.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <system_error>

namespace gpu::telemetry {

IpcClient::IpcClient(const std::string& socketPath) {
    // Create non-blocking Unix domain socket
    sockfd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd_ < 0) {
        throw std::system_error(errno, std::system_category(), 
                              "Failed to create socket");
    }

    // Connect to server
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd_);
        throw std::system_error(errno, std::system_category(), 
                              "Failed to connect");
    }
}

IpcClient::~IpcClient() {
    if (sockfd_ >= 0) {
        close(sockfd_);
    }
}

IpcClient::IpcClient(IpcClient&& other) noexcept 
    : sockfd_(other.sockfd_),
      readBuffer_(std::move(other.readBuffer_)) {
    other.sockfd_ = -1;
}

IpcClient& IpcClient::operator=(IpcClient&& other) noexcept {
    if (this != &other) {
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
        sockfd_ = other.sockfd_;
        readBuffer_ = std::move(other.readBuffer_);
        other.sockfd_ = -1;
    }
    return *this;
}

stdexec::sender auto IpcClient::sendMessage(
    std::span<const std::uint8_t> message) {
    return stdexec::just() |
           stdexec::then([this, message]() {
               // Write message size
               uint32_t size = message.size();
               if (write(sockfd_, &size, sizeof(size)) != sizeof(size)) {
                   throw std::system_error(errno, std::system_category(), 
                                         "Failed to write size");
               }

               // Write message data
               if (write(sockfd_, message.data(), size) != size) {
                   throw std::system_error(errno, std::system_category(), 
                                         "Failed to write message");
               }

               return readResponse();
           });
}

std::vector<uint8_t> IpcClient::readResponse() {
    // Read response size
    uint32_t size;
    if (read(sockfd_, &size, sizeof(size)) != sizeof(size)) {
        throw std::system_error(errno, std::system_category(), 
                              "Failed to read size");
    }

    // Read response data
    readBuffer_.resize(size);
    if (read(sockfd_, readBuffer_.data(), size) != size) {
        throw std::system_error(errno, std::system_category(), 
                              "Failed to read response");
    }

    return readBuffer_;
}

} // namespace gpu::telemetry
