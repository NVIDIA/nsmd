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

#pragma once

#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @brief Pure virtual interface for socket I/O operations.
 *
 * Production code uses getSocketIo() to obtain a RealSocketIo that forwards
 * to actual syscalls.  Tests inject a MockSocketIo via setSocketIoForTesting().
 */
class SocketIoInterface
{
  public:
    virtual ~SocketIoInterface() = default;

    virtual int socket(int domain, int type, int protocol) = 0;
    virtual int bind(int fd, const struct sockaddr* addr,
                     socklen_t addrlen) = 0;
    virtual int connect(int fd, const struct sockaddr* addr,
                        socklen_t addrlen) = 0;
    virtual int close(int fd) = 0;

    virtual ssize_t send(int fd, const void* buf, size_t len, int flags) = 0;
    virtual ssize_t sendto(int fd, const void* buf, size_t len, int flags,
                           const struct sockaddr* dest, socklen_t addrlen) = 0;
    virtual ssize_t sendmsg(int fd, const struct msghdr* msg, int flags) = 0;
    virtual ssize_t write(int fd, const void* buf, size_t count) = 0;

    virtual ssize_t recv(int fd, void* buf, size_t len, int flags) = 0;
    virtual ssize_t recvfrom(int fd, void* buf, size_t len, int flags,
                             struct sockaddr* src, socklen_t* addrlen) = 0;
    virtual ssize_t recvmsg(int fd, struct msghdr* msg, int flags) = 0;

    virtual int poll(struct pollfd* fds, nfds_t nfds, int timeout) = 0;

    virtual int getsockopt(int fd, int level, int optname, void* optval,
                           socklen_t* optlen) = 0;
    virtual int setsockopt(int fd, int level, int optname, const void* optval,
                           socklen_t optlen) = 0;

    virtual int ioctl(int fd, unsigned long request, void* argp) = 0;
};

/** Returns the currently active SocketIoInterface (real or injected mock). */
SocketIoInterface& getSocketIo();

/**
 * Preprocessor redirect — maps raw POSIX socket syscalls to getSocketIo(),
 * enabling mock injection in unit tests without modifying call sites.
 *
 * Only activated when the includer defines NSMD_SOCKET_REDIRECT before this
 * header.  Include socketIo.hpp as the LAST include in translation units that
 * call socket syscalls directly (e.g. socket_handler.cpp):
 *
 *   #define NSMD_SOCKET_REDIRECT
 *   #include "socketIo.hpp"
 *
 * Files that define RealSocketIo or MockSocketIo must NOT define this macro —
 * the macros would interfere with method definitions that share syscall names.
 */
#ifdef NSMD_SOCKET_REDIRECT
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
// clang-format off
#define socket(...)     getSocketIo().socket(__VA_ARGS__)
#define getsockopt(...) getSocketIo().getsockopt(__VA_ARGS__)
#define connect(...)    getSocketIo().connect(__VA_ARGS__)
#define write(...)      getSocketIo().write(__VA_ARGS__)
#define sendmsg(...)    getSocketIo().sendmsg(__VA_ARGS__)
#define recv(...)       getSocketIo().recv(__VA_ARGS__)
#define recvfrom(...)   getSocketIo().recvfrom(__VA_ARGS__)
#define sendto(...)     getSocketIo().sendto(__VA_ARGS__)
#define bind(...)       getSocketIo().bind(__VA_ARGS__)
#define ioctl(...)      getSocketIo().ioctl(__VA_ARGS__)
// clang-format on
// NOLINTEND(cppcoreguidelines-macro-usage)
#endif // NSMD_SOCKET_REDIRECT
