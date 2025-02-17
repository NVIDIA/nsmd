/**
 * @brief GPU telemetry server
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "transport.hpp"
#include "gpu-telemetry/types.h"
#include <memory>
#include <unordered_map>
#include <stdexec/execution.hpp>

namespace gpu::telemetry {

/**
 * @brief Context for a connected client
 */
struct ClientContext {
    int fd;                     // Client socket
    std::vector<uint8_t> buf;   // Read buffer
};

/**
 * @brief Server for handling GPU telemetry requests
 */
class Server {
public:
    /**
     * @brief Construct server with transport
     * @param transport Transport implementation
     * @param config Server configuration
     */
    explicit Server(std::unique_ptr<Transport> transport,
                   ServerConfig config = ServerConfig{});

    /**
     * @brief Destructor cleans up socket
     */
    ~Server();

    // Disable copying
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /**
     * @brief Start server
     * @return Sender that completes when server is ready
     */
    stdexec::sender auto start();

    /**
     * @brief Stop server
     * @return Sender that completes when server is stopped
     */
    stdexec::sender auto stop();

private:
    /**
     * @brief Accept new client connections
     * @return Sender for accept operation
     */
    stdexec::sender auto acceptClients();

    /**
     * @brief Handle messages from a client
     * @param clientFd Client socket
     * @return Sender for client handling
     */
    stdexec::sender auto handleClient(int clientFd);

    /**
     * @brief Remove disconnected client
     * @param clientFd Client socket
     */
    void removeClient(int clientFd);

    ServerConfig config_;
    std::unique_ptr<Transport> transport_;
    int listenFd_{-1};
    bool running_{false};
    std::unordered_map<int, std::unique_ptr<ClientContext>> clients_;
};

} // namespace gpu::telemetry
