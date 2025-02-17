/**
 * @brief GPU telemetry server implementation
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "server.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <system_error>

namespace gpu::telemetry {

Server::Server(std::unique_ptr<Transport> transport, ServerConfig config)
    : config_(std::move(config)),
      transport_(std::move(transport)) {
}

Server::~Server() {
    if (listenFd_ >= 0) {
        close(listenFd_);
    }
    unlink(config_.socketPath.c_str());
}

stdexec::sender auto Server::start() {
    return stdexec::just() |
           stdexec::then([this]() {
               // Create non-blocking Unix domain socket
               listenFd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
               if (listenFd_ < 0) {
                   throw std::system_error(errno, std::system_category(),
                                         "Failed to create socket");
               }

               // Bind to socket path
               struct sockaddr_un addr = {};
               addr.sun_family = AF_UNIX;
               strncpy(addr.sun_path, config_.socketPath.c_str(),
                      sizeof(addr.sun_path) - 1);

               unlink(config_.socketPath.c_str());
               if (bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                   throw std::system_error(errno, std::system_category(),
                                         "Failed to bind socket");
               }

               // Set socket permissions
               if (chmod(config_.socketPath.c_str(), config_.socketPerms) < 0) {
                   throw std::system_error(errno, std::system_category(),
                                         "Failed to set socket permissions");
               }

               // Listen for connections
               if (listen(listenFd_, config_.maxClients) < 0) {
                   throw std::system_error(errno, std::system_category(),
                                         "Failed to listen on socket");
               }

               running_ = true;
               return acceptClients();
           });
}

stdexec::sender auto Server::stop() {
    return stdexec::just() |
           stdexec::then([this]() {
               running_ = false;

               // Close all client connections
               for (const auto& [fd, _] : clients_) {
                   close(fd);
               }
               clients_.clear();

               // Close listen socket
               if (listenFd_ >= 0) {
                   close(listenFd_);
                   listenFd_ = -1;
               }

               unlink(config_.socketPath.c_str());
           });
}

stdexec::sender auto Server::acceptClients() {
    return stdexec::just() |
           stdexec::then([this]() {
               while (running_) {
                   // Accept new connection
                   struct sockaddr_un addr;
                   socklen_t addrLen = sizeof(addr);
                   int clientFd = accept4(listenFd_, (struct sockaddr*)&addr,
                                        &addrLen, SOCK_NONBLOCK);

                   if (clientFd < 0) {
                       if (errno == EAGAIN || errno == EWOULDBLOCK) {
                           continue;  // No pending connections
                       }
                       throw std::system_error(errno, std::system_category(),
                                             "Failed to accept connection");
                   }

                   // Check client limit
                   if (clients_.size() >= config_.maxClients) {
                       close(clientFd);
                       continue;
                   }

                   // Create client context
                   auto ctx = std::make_unique<ClientContext>();
                   ctx->fd = clientFd;
                   clients_[clientFd] = std::move(ctx);

                   // Handle client messages
                   stdexec::start_detached(handleClient(clientFd));
               }
           });
}

stdexec::sender auto Server::handleClient(int clientFd) {
    return stdexec::just() |
           stdexec::then([this, clientFd]() {
               auto& ctx = clients_[clientFd];

               try {
                   while (running_) {
                       // Read message size
                       uint32_t size;
                       ssize_t n = read(clientFd, &size, sizeof(size));
                       if (n < 0) {
                           if (errno == EAGAIN || errno == EWOULDBLOCK) {
                               continue;  // No data available
                           }
                           throw std::system_error(errno, std::system_category(),
                                                 "Failed to read size");
                       }
                       if (n == 0) {
                           break;  // Client disconnected
                       }

                       // Read message data
                       ctx->buf.resize(size);
                       n = read(clientFd, ctx->buf.data(), size);
                       if (n < 0) {
                           throw std::system_error(errno, std::system_category(),
                                                 "Failed to read message");
                       }
                       if (n == 0) {
                           break;  // Client disconnected
                       }

                       // Forward to transport
                       transport_->sendMessage(
                           ctx->buf,
                           [clientFd](std::span<const std::uint8_t> response) {
                               uint32_t size = response.size();
                               write(clientFd, &size, sizeof(size));
                               write(clientFd, response.data(), size);
                           });
                   }
               } catch (...) {
                   // Handle errors
               }

               removeClient(clientFd);
           });
}

void Server::removeClient(int clientFd) {
    close(clientFd);
    clients_.erase(clientFd);
}

} // namespace gpu::telemetry
