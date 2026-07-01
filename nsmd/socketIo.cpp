/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
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

/**
 * Production implementation of SocketIoInterface.
 * Compiled into the production binary; replaced by socketIoMock.cpp in tests.
 */
#include "socketIo.hpp"

/**
 * @brief Concrete implementation that forwards every call to the real syscall.
 *
 * Uses the :: (global namespace) qualifier so that the SOCKET_IO_SHIM macros
 * (if ever active in this TU) cannot intercept these calls.
 */
class RealSocketIo : public SocketIoInterface
{
  public:
    int socket(int domain, int type, int protocol) override
    {
        return ::socket(domain, type, protocol);
    }
    int bind(int fd, const struct sockaddr* addr, socklen_t addrlen) override
    {
        return ::bind(fd, addr, addrlen);
    }
    int connect(int fd, const struct sockaddr* addr, socklen_t addrlen) override
    {
        return ::connect(fd, addr, addrlen);
    }
    int close(int fd) override
    {
        return ::close(fd);
    }
    ssize_t send(int fd, const void* buf, size_t len, int flags) override
    {
        return ::send(fd, buf, len, flags);
    }
    ssize_t sendto(int fd, const void* buf, size_t len, int flags,
                   const struct sockaddr* dest, socklen_t addrlen) override
    {
        return ::sendto(fd, buf, len, flags, dest, addrlen);
    }
    ssize_t sendmsg(int fd, const struct msghdr* msg, int flags) override
    {
        return ::sendmsg(fd, msg, flags);
    }
    ssize_t write(int fd, const void* buf, size_t count) override
    {
        return ::write(fd, buf, count);
    }
    ssize_t recv(int fd, void* buf, size_t len, int flags) override
    {
        return ::recv(fd, buf, len, flags);
    }
    ssize_t recvfrom(int fd, void* buf, size_t len, int flags,
                     struct sockaddr* src, socklen_t* addrlen) override
    {
        return ::recvfrom(fd, buf, len, flags, src, addrlen);
    }
    ssize_t recvmsg(int fd, struct msghdr* msg, int flags) override
    {
        return ::recvmsg(fd, msg, flags);
    }
    int poll(struct pollfd* fds, nfds_t nfds, int timeout) override
    {
        return ::poll(fds, nfds, timeout);
    }
    int getsockopt(int fd, int level, int optname, void* optval,
                   socklen_t* optlen) override
    {
        return ::getsockopt(fd, level, optname, optval, optlen);
    }
    int setsockopt(int fd, int level, int optname, const void* optval,
                   socklen_t optlen) override
    {
        return ::setsockopt(fd, level, optname, optval, optlen);
    }
};

SocketIoInterface& getSocketIo()
{
    static RealSocketIo realSocketIo;
    return realSocketIo;
}
