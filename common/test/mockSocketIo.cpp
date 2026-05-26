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
 * Test-only implementation of getSocketIo() / getMockSocketIo().
 * Compiled into the test library (nsmd_test) instead of nsmd/socketIo.cpp,
 * following the same pattern as mockDBusHandler.cpp / dBusHandler.cpp.
 */
#include "mockSocketIo.hpp"

#include <gmock/gmock.h>

SocketIoInterface& getSocketIo()
{
    static ::testing::NiceMock<MockSocketIo> instance;
    return instance;
}

MockSocketIo& getMockSocketIo()
{
    return static_cast<MockSocketIo&>(getSocketIo());
}

/**
 * Link-time interposition: intercept raw POSIX socket calls from C/C++ code
 * that does not use the NSMD_SOCKET_REDIRECT macro path (e.g. mctp.cpp).
 * These definitions take precedence over libc at link time, forwarding every
 * syscall to the shared MockSocketIo singleton so EXPECT_CALLs work.
 *
 * `__wrap_*` companions exist for targets linked with -Wl,--wrap=<symbol>;
 * the linker rewrite wins against ASan's runtime interceptors.  The
 * `__*_chk` wraps handle the -D_FORTIFY_SOURCE=2 rewrites of poll/recv.
 */
// NOLINTBEGIN(readability-identifier-naming)
extern "C"
{
ssize_t sendmsg(int fd, const struct msghdr* msg, int flags)
{
    return getMockSocketIo().sendmsg(fd, msg, flags);
}

ssize_t recv(int fd, void* buf, size_t len, int flags)
{
    return getMockSocketIo().recv(fd, buf, len, flags);
}

ssize_t recvmsg(int fd, struct msghdr* msg, int flags)
{
    return getMockSocketIo().recvmsg(fd, msg, flags);
}

int poll(struct pollfd* fds, nfds_t nfds, int timeout)
{
    return getMockSocketIo().poll(fds, nfds, timeout);
}

ssize_t __wrap_sendmsg(int fd, const struct msghdr* msg, int flags)
{
    return getMockSocketIo().sendmsg(fd, msg, flags);
}

ssize_t __wrap_recv(int fd, void* buf, size_t len, int flags)
{
    return getMockSocketIo().recv(fd, buf, len, flags);
}

ssize_t __wrap_recvmsg(int fd, struct msghdr* msg, int flags)
{
    return getMockSocketIo().recvmsg(fd, msg, flags);
}

int __wrap_poll(struct pollfd* fds, nfds_t nfds, int timeout)
{
    return getMockSocketIo().poll(fds, nfds, timeout);
}

int __wrap___poll_chk(struct pollfd* fds, nfds_t nfds, int timeout,
                      __SIZE_TYPE__ /*fdslen*/)
{
    return getMockSocketIo().poll(fds, nfds, timeout);
}

ssize_t __wrap___recv_chk(int fd, void* buf, size_t len,
                          __SIZE_TYPE__ /*buflen*/, int flags)
{
    return getMockSocketIo().recv(fd, buf, len, flags);
}

} // extern "C"
// NOLINTEND(readability-identifier-naming)
