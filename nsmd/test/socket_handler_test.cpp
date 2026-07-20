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

#include "../../common/test/mockSocketIo.hpp"

// Open up private/protected members so we can call handleReceivedMsg directly.
#define private public
#define protected public
#include "../socket_handler.hpp"
#undef private
#undef protected

#include "common/event.hpp"
#include "eventManager.hpp"
#include "instance_id.hpp"
#include "requester/handler.hpp"
#include "socket_manager.hpp"

#include <linux/mctp.h>
#include <sys/un.h>

#include <sdeventplus/source/io.hpp>

#include <gtest/gtest.h>

using ::testing::_;
using ::testing::NotNull;
using ::testing::Return;

/**
 * Test fixture — builds the full dependency chain needed to instantiate
 * DaemonHandler and InKernelHandler.
 */
class SocketHandlerTest : public MctpTestFixture
{
  protected:
    common::Event event;
    nsm::InstanceIdDb instanceIdDb;
    mctp_socket::Manager sockManager;
    nsm::EventManager eventManager;
    requester::Handler<requester::Request> reqHandler{event, instanceIdDb,
                                                      sockManager, false};
    mctp_socket::DaemonHandler daemonHandler{event, reqHandler, eventManager,
                                             sockManager, false};
    mctp_socket::InKernelHandler inKernelHandler{
        event, reqHandler, eventManager, sockManager, false};

    /**
     * Creates a real Unix socket pair so that sdeventplus::source::IO can
     * register an epoll watch on a valid fd.  Returns the two fds; the caller
     * is responsible for closing them (the handler closes its end via
     * CustomFD).
     */
    static std::pair<int, int> makeSocketPair()
    {
        int sv[2];
        if (::socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) != 0)
        {
            return {-1, -1};
        }
        return {sv[0], sv[1]};
    }
};

// ---------------------------------------------------------------------------
// DaemonHandler::initSocket (called via registerMctpEndpoint)
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, DaemonHandlerInitSocketSuccess)
{
    // Use a real socketpair fd so that sdeventplus::source::IO succeeds.
    auto [handlerFd, peerFd] = makeSocketPair();
    ASSERT_GE(handlerFd, 0);
    ::close(peerFd); // We only need the handler's end.

    const int sendBufferSize = 4096;
    const std::vector<uint8_t> pathName = {'\0', 'm', 'c', 't', 'p', '-',
                                           'd',  'e', 'm', 'u', 'x'};

    EXPECT_CALL(*mockIo_, socket(AF_UNIX, SOCK_SEQPACKET, 0))
        .WillOnce(Return(handlerFd));

    EXPECT_CALL(*mockIo_, getsockopt(handlerFd, SOL_SOCKET, SO_SNDBUF, _, _))
        .WillOnce([=](int, int, int, void* optval, socklen_t*) {
        *static_cast<int*>(optval) = sendBufferSize;
        return 0;
    });

    EXPECT_CALL(*mockIo_, connect(handlerFd, _, _))
        .WillOnce([&](int, const struct sockaddr* addr, socklen_t addrlen) {
        const auto* unAddr = reinterpret_cast<const struct sockaddr_un*>(addr);
        EXPECT_EQ(unAddr->sun_family, AF_UNIX);
        EXPECT_EQ(addrlen, pathName.size() + sizeof(unAddr->sun_family));
        return 0;
    });

    EXPECT_CALL(*mockIo_, write(handlerFd, _, 1))
        .WillOnce([](int, const void* buf, size_t) {
        EXPECT_EQ(*static_cast<const uint8_t*>(buf), 0x7E); // VDM type
        return 1;
    });

    int rc = daemonHandler.registerMctpEndpoint(0x10, SOCK_SEQPACKET, 0,
                                                pathName);
    EXPECT_EQ(rc, 0);
    // handlerFd is now owned by the DaemonHandler's CustomFD.
}

TEST_F(SocketHandlerTest, DaemonHandlerInitSocketFailure)
{
    const std::vector<uint8_t> pathName = {'\0', 'm', 'c', 't', 'p', '-',
                                           'd',  'e', 'm', 'u', 'x'};
    EXPECT_CALL(*mockIo_, socket(AF_UNIX, SOCK_SEQPACKET, 0))
        .WillOnce(Return(-1));

    int rc = daemonHandler.registerMctpEndpoint(0x10, SOCK_SEQPACKET, 0,
                                                pathName);
    EXPECT_LT(rc, 0);
}

TEST_F(SocketHandlerTest, DaemonHandlerInitSocketConnectFailure)
{
    const int fakeFd = 43;
    const int sendBufferSize = 4096;
    const std::vector<uint8_t> pathName = {'\0', 'm', 'c', 't', 'p', '-',
                                           'd',  'e', 'm', 'u', 'x'};

    EXPECT_CALL(*mockIo_, socket(AF_UNIX, SOCK_SEQPACKET, 0))
        .WillOnce(Return(fakeFd));

    EXPECT_CALL(*mockIo_, getsockopt(fakeFd, SOL_SOCKET, SO_SNDBUF, _, _))
        .WillOnce([=](int, int, int, void* optval, socklen_t*) {
        *static_cast<int*>(optval) = sendBufferSize;
        return 0;
    });

    EXPECT_CALL(*mockIo_, connect(fakeFd, _, _)).WillOnce(Return(-1));

    int rc = daemonHandler.registerMctpEndpoint(0x10, SOCK_SEQPACKET, 0,
                                                pathName);
    EXPECT_LT(rc, 0);
}

// ---------------------------------------------------------------------------
// DaemonHandler::sendMsg
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, DaemonHandlerSendMsgSuccess)
{
    const int fakeFd = 43;
    const uint8_t eid = 0x10;
    const uint8_t tag = 0x01;
    const std::vector<uint8_t> nsmMsg = {0x80, 0x05, 0x01, 0x00, 0xAA};

    EXPECT_CALL(*mockIo_, sendmsg(fakeFd, _, 0))
        .WillOnce([=](int, const struct msghdr* msg, int) {
        EXPECT_EQ(msg->msg_iovlen, 2);
        const struct iovec* iov = msg->msg_iov;
        EXPECT_EQ(iov[0].iov_len, 3);
        const uint8_t* hdr = static_cast<const uint8_t*>(iov[0].iov_base);
        EXPECT_EQ(hdr[0], tag);
        EXPECT_EQ(hdr[1], eid);
        EXPECT_EQ(hdr[2], 0x7E); // VDM type
        EXPECT_EQ(iov[1].iov_len, nsmMsg.size());
        return static_cast<ssize_t>(iov[0].iov_len + iov[1].iov_len);
    });

    int rc = daemonHandler.sendMsg(tag, eid, fakeFd, nsmMsg.data(),
                                   nsmMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// InKernelHandler::registerMctpEndpoint (initSocket)
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerSocketCreationSuccess)
{
    auto [handlerFd, peerFd] = makeSocketPair();
    ASSERT_GE(handlerFd, 0);
    ::close(peerFd);

    const int sendBufferSize = 4096;

    EXPECT_CALL(*mockIo_, socket(AF_MCTP, SOCK_DGRAM, 0))
        .WillOnce(Return(handlerFd));

    EXPECT_CALL(*mockIo_, getsockopt(handlerFd, SOL_SOCKET, SO_SNDBUF, _, _))
        .WillOnce([=](int, int, int, void* optval, socklen_t*) {
        *static_cast<int*>(optval) = sendBufferSize;
        return 0;
    });

    EXPECT_CALL(*mockIo_, bind(handlerFd, _, sizeof(struct sockaddr_mctp)))
        .WillOnce([](int, const struct sockaddr* addr, socklen_t) {
        const auto* mctpAddr =
            reinterpret_cast<const struct sockaddr_mctp*>(addr);
        EXPECT_EQ(mctpAddr->smctp_family, AF_MCTP);
        EXPECT_EQ(mctpAddr->smctp_network, MCTP_NET_ANY);
        EXPECT_EQ(mctpAddr->smctp_addr.s_addr, MCTP_ADDR_ANY);
        EXPECT_EQ(mctpAddr->smctp_tag, MCTP_TAG_OWNER);
        EXPECT_EQ(mctpAddr->smctp_type, 0x7E); // VDM type
        return 0;
    });

    int rc = inKernelHandler.registerMctpEndpoint(0x10, SOCK_DGRAM, 0, {});
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ---------------------------------------------------------------------------
// InKernelHandler::sendMsg
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerSendMsgSuccess)
{
    const int fakeFd = 50;
    const uint8_t eid = 0x10;
    const std::vector<uint8_t> nsmMsg = {0x80, 0x05, 0x01, 0x00, 0xAA};

    EXPECT_CALL(*mockIo_, sendto(fakeFd, _, nsmMsg.size(), 0, _,
                                 sizeof(struct sockaddr_mctp)))
        .WillOnce([=](int, const void*, size_t len, int,
                      const struct sockaddr* addr, socklen_t) {
        const auto* mctpAddr =
            reinterpret_cast<const struct sockaddr_mctp*>(addr);
        EXPECT_EQ(mctpAddr->smctp_family, AF_MCTP);
        EXPECT_EQ(mctpAddr->smctp_network, MCTP_NET_ANY);
        EXPECT_EQ(mctpAddr->smctp_addr.s_addr, eid);
        EXPECT_EQ(mctpAddr->smctp_tag, MCTP_TAG_OWNER);
        EXPECT_EQ(mctpAddr->smctp_type, 0x7E); // VDM type
        return static_cast<ssize_t>(len);
    });

    int rc = inKernelHandler.sendMsg(0x01, eid, fakeFd, nsmMsg.data(),
                                     nsmMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// DaemonHandler::handleReceivedMsg (called directly via #define private public)
// ---------------------------------------------------------------------------

/** Helper: create a real IO source backed by a pipe for use as the io param. */
static sdeventplus::source::IO makeTestIo(sdeventplus::Event& event, int fd)
{
    return sdeventplus::source::IO(
        event, fd, EPOLLIN, [](sdeventplus::source::IO&, int, uint32_t) {});
}

TEST_F(SocketHandlerTest, ProcessRxMsgInvalidLength)
{
    const std::vector<uint8_t> shortMsg = {0x00};
    EXPECT_LT(shortMsg.size(), sizeof(nsm_msg_hdr));
}

TEST_F(SocketHandlerTest, HandleReceivedMsgPeekReadPattern)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 50;
    const std::vector<uint8_t> mctpMsg = {0x01, 0x10, 0x7E, 0x00,
                                          0x05, 0x01, 0x00};

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(mctpMsg.size()));

    EXPECT_CALL(*mockIo_, recv(fakeFd, _, mctpMsg.size(), 0))
        .WillOnce([&](int, void* buf, size_t, int) {
        memcpy(buf, mctpMsg.data(), mctpMsg.size());
        return static_cast<ssize_t>(mctpMsg.size());
    });

    daemonHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

TEST_F(SocketHandlerTest, HandleRecvFailure)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 50;

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(-1));

    daemonHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

TEST_F(SocketHandlerTest, HandleRecvZeroLength)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 50;

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(0));

    // peekedLength == 0 → calls io.get_event().exit(0); must not crash.
    daemonHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

TEST_F(SocketHandlerTest, HandleLargeMessageDynamicBuffer)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 50;
    // Larger than STATIC_BUF_SIZE (64) to force dynamic allocation.
    const size_t largeSize = 100;
    std::vector<uint8_t> largeMsg(largeSize, 0x00);
    // Non-VDM type so processRxMsg is not called (avoids handler dependency).
    largeMsg[2] = 0x00;

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(largeSize));

    EXPECT_CALL(*mockIo_, recv(fakeFd, _, largeSize, 0))
        .WillOnce([&](int, void* buf, size_t, int) {
        memcpy(buf, largeMsg.data(), largeMsg.size());
        return static_cast<ssize_t>(largeSize);
    });

    daemonHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// InKernelHandler::handleReceivedMsg
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerRecvFromWithAddr)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 50;
    const uint8_t srcEid = 0x10;
    std::vector<uint8_t> nsmMsg = {0x00, 0x05, 0x01, 0x00};
    const size_t totalSize = nsmMsg.size();

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(totalSize));

    EXPECT_CALL(*mockIo_, recvfrom(fakeFd, _, totalSize, MSG_TRUNC, _, _))
        .WillOnce([&](int, void* buf, size_t, int, struct sockaddr* addr,
                      socklen_t* addrlen) {
        auto* mctpAddr = reinterpret_cast<struct sockaddr_mctp*>(addr);
        mctpAddr->smctp_family = AF_MCTP;
        mctpAddr->smctp_addr.s_addr = srcEid;
        mctpAddr->smctp_tag = 0x01;
        mctpAddr->smctp_type = 0x7E;
        *addrlen = sizeof(struct sockaddr_mctp);
        memcpy(buf, nsmMsg.data(), nsmMsg.size());
        return static_cast<ssize_t>(totalSize);
    });

    inKernelHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ===========================================================================
// Additional branch-coverage tests
// ===========================================================================

// ---------------------------------------------------------------------------
// DaemonHandler::initSocket — getsockopt failure
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, DaemonHandlerGetsockoptFailure)
{
    const int fakeFd = 44;
    const std::vector<uint8_t> pathName = {'\0', 'g', 's', 'o'};

    EXPECT_CALL(*mockIo_, socket(AF_UNIX, SOCK_SEQPACKET, 0))
        .WillOnce(Return(fakeFd));

    EXPECT_CALL(*mockIo_, getsockopt(fakeFd, SOL_SOCKET, SO_SNDBUF, _, _))
        .WillOnce(Return(-1));

    int rc = daemonHandler.registerMctpEndpoint(0x10, SOCK_SEQPACKET, 0,
                                                pathName);
    EXPECT_LT(rc, 0);
}

// ---------------------------------------------------------------------------
// DaemonHandler::initSocket — write (VDM registration) failure
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, DaemonHandlerWriteVdmFailure)
{
    auto [handlerFd, peerFd] = makeSocketPair();
    ASSERT_GE(handlerFd, 0);
    ::close(peerFd);

    const int sendBufferSize = 4096;
    const std::vector<uint8_t> pathName = {'\0', 'w', 'v', 'd', 'm'};

    EXPECT_CALL(*mockIo_, socket(AF_UNIX, SOCK_SEQPACKET, 0))
        .WillOnce(Return(handlerFd));

    EXPECT_CALL(*mockIo_, getsockopt(handlerFd, SOL_SOCKET, SO_SNDBUF, _, _))
        .WillOnce([=](int, int, int, void* optval, socklen_t*) {
        *static_cast<int*>(optval) = sendBufferSize;
        return 0;
    });

    EXPECT_CALL(*mockIo_, connect(handlerFd, _, _)).WillOnce(Return(0));

    EXPECT_CALL(*mockIo_, write(handlerFd, _, 1)).WillOnce(Return(-1));

    int rc = daemonHandler.registerMctpEndpoint(0x10, SOCK_SEQPACKET, 0,
                                                pathName);
    EXPECT_LT(rc, 0);
}

// ---------------------------------------------------------------------------
// DaemonHandler::registerMctpEndpoint — socket already in socketInfoMap
// (else branch: re-use existing socket without calling initSocket)
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, DaemonHandlerRegisterExistingSocket)
{
    auto [handlerFd, peerFd] = makeSocketPair();
    ASSERT_GE(handlerFd, 0);
    ::close(peerFd);

    const int sendBufferSize = 4096;
    const std::vector<uint8_t> pathName = {'\0', 'r', 'e', 'x'};

    // First call — creates socket and adds to socketInfoMap.
    EXPECT_CALL(*mockIo_, socket(AF_UNIX, SOCK_SEQPACKET, 0))
        .WillOnce(Return(handlerFd));
    EXPECT_CALL(*mockIo_, getsockopt(handlerFd, SOL_SOCKET, SO_SNDBUF, _, _))
        .WillOnce([=](int, int, int, void* optval, socklen_t*) {
        *static_cast<int*>(optval) = sendBufferSize;
        return 0;
    });
    EXPECT_CALL(*mockIo_, connect(handlerFd, _, _)).WillOnce(Return(0));
    EXPECT_CALL(*mockIo_, write(handlerFd, _, 1)).WillOnce(Return(1));

    int rc = daemonHandler.registerMctpEndpoint(0x10, SOCK_SEQPACKET, 0,
                                                pathName);
    ASSERT_EQ(rc, 0);

    // Second call with the same pathName — takes the else branch (no
    // socket or getsockopt/connect/write calls expected).
    rc = daemonHandler.registerMctpEndpoint(0x20, SOCK_SEQPACKET, 0, pathName);
    EXPECT_EQ(rc, 0);
}

// ---------------------------------------------------------------------------
// DaemonHandler::sendMsg — sendmsg failure
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, DaemonHandlerSendMsgFailure)
{
    const int fakeFd = 45;
    const uint8_t eid = 0x10;
    const uint8_t tag = 0x01;
    const std::vector<uint8_t> nsmMsg = {0x80, 0x05, 0x01, 0x00};

    EXPECT_CALL(*mockIo_, sendmsg(fakeFd, _, 0)).WillOnce(Return(-1));

    int rc = daemonHandler.sendMsg(tag, eid, fakeFd, nsmMsg.data(),
                                   nsmMsg.size());
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

// ---------------------------------------------------------------------------
// DaemonHandler::handleReceivedMsg — revents does NOT include EPOLLIN
// (early-return branch)
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, HandleReceivedMsgNotEpollin)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 51;

    // No recv calls expected — handler returns immediately.
    daemonHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLOUT);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// DaemonHandler::handleReceivedMsg — recvDataLength != peekedLength
// (error-log branch inside the else block)
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, HandleReceivedMsgRecvLengthMismatch)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 52;
    const ssize_t peekedLen = 10;

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(peekedLen));

    // Second recv returns fewer bytes → mismatch
    EXPECT_CALL(*mockIo_, recv(fakeFd, _, static_cast<size_t>(peekedLen), 0))
        .WillOnce(Return(5));

    daemonHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// DaemonHandler::handleReceivedMsg — valid NSM_RESPONSE message in MCTP frame
// Covers the else-if (NSM_RESPONSE) branch in processRxMsg.
// pci_vendor_id bytes: {0x10, 0xDE} → uint16_t=0xDE10 → be16toh=0x10DE ✓
// request=0, datagram=0 → NSM_RESPONSE; nsmMsgSize(9) >= nsmRespMinimusLen(9)
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, HandleReceivedMsgNsmResponsePath)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 53;

    // NSM header: pci_vendor_id=0x10DE(BE), request=0, datagram=0,
    //             ocp_type=8, ocp_version=9, nvidia_msg_type=1
    // + 4 payload bytes to satisfy NSM_RESPONSE_MIN_LEN=4
    std::vector<uint8_t> mctpMsg = {
        0x01, 0x10, 0x7E,      // MCTP: tag, eid, VDM type
        0x10, 0xDE,            // pci_vendor_id (big-endian)
        0x00,                  // instance=0, rq=0, datagram=0
        0x89,                  // ocp_type=8, ocp_version=9
        0x01,                  // nvidia_msg_type
        0x00, 0x00, 0x00, 0x00 // 4 payload bytes
    };

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(mctpMsg.size()));

    EXPECT_CALL(*mockIo_, recv(fakeFd, _, mctpMsg.size(), 0))
        .WillOnce([&](int, void* buf, size_t, int) {
        memcpy(buf, mctpMsg.data(), mctpMsg.size());
        return static_cast<ssize_t>(mctpMsg.size());
    });

    daemonHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// DaemonHandler::handleReceivedMsg — valid NSM_EVENT message
// Covers the if (NSM_EVENT) branch in processRxMsg.
// request=1, datagram=1 → NSM_EVENT; nsmMsgSize(11) >= nsmEventMinimusLen(11)
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, HandleReceivedMsgNsmEventPath)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 54;

    // NSM event header: pci_vendor_id=0x10DE(BE), request=1, datagram=1 →
    //   NSM_EVENT, ocp_type=8, ocp_version=9, + 6 event payload bytes
    std::vector<uint8_t> mctpMsg = {
        0x01, 0x10, 0x7E, // MCTP header
        0x10, 0xDE,       // pci_vendor_id
        0xC0,             // request=1(b7), datagram=1(b6)
        0x89,             // ocp_type=8, ocp_version=9
        0x01,             // nvidia_msg_type
        0x00, 0x00, 0x00,
        0x00, 0x00, 0x00  // 6 payload bytes (NSM_EVENT_MIN_LEN)
    };

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(mctpMsg.size()));

    EXPECT_CALL(*mockIo_, recv(fakeFd, _, mctpMsg.size(), 0))
        .WillOnce([&](int, void* buf, size_t, int) {
        memcpy(buf, mctpMsg.data(), mctpMsg.size());
        return static_cast<ssize_t>(mctpMsg.size());
    });

    daemonHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// InKernelHandler::registerMctpEndpoint — socket(AF_MCTP) fails
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerSocketCreationFailure)
{
    EXPECT_CALL(*mockIo_, socket(AF_MCTP, SOCK_DGRAM, 0)).WillOnce(Return(-1));

    int rc = inKernelHandler.registerMctpEndpoint(0x10, SOCK_DGRAM, 0, {});
    EXPECT_LT(rc, 0);
}

// ---------------------------------------------------------------------------
// InKernelHandler::registerMctpEndpoint — getsockopt fails
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerGetsockoptFailure)
{
    auto [handlerFd, peerFd] = makeSocketPair();
    ASSERT_GE(handlerFd, 0);
    ::close(peerFd);

    EXPECT_CALL(*mockIo_, socket(AF_MCTP, SOCK_DGRAM, 0))
        .WillOnce(Return(handlerFd));

    EXPECT_CALL(*mockIo_, getsockopt(handlerFd, SOL_SOCKET, SO_SNDBUF, _, _))
        .WillOnce(Return(-1));

    int rc = inKernelHandler.registerMctpEndpoint(0x10, SOCK_DGRAM, 0, {});
    EXPECT_LT(rc, 0);
}

// ---------------------------------------------------------------------------
// InKernelHandler::registerMctpEndpoint — bind fails
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerBindFailure)
{
    auto [handlerFd, peerFd] = makeSocketPair();
    ASSERT_GE(handlerFd, 0);
    ::close(peerFd);

    const int sendBufferSize = 4096;

    EXPECT_CALL(*mockIo_, socket(AF_MCTP, SOCK_DGRAM, 0))
        .WillOnce(Return(handlerFd));

    EXPECT_CALL(*mockIo_, getsockopt(handlerFd, SOL_SOCKET, SO_SNDBUF, _, _))
        .WillOnce([=](int, int, int, void* optval, socklen_t*) {
        *static_cast<int*>(optval) = sendBufferSize;
        return 0;
    });

    EXPECT_CALL(*mockIo_, bind(handlerFd, _, sizeof(struct sockaddr_mctp)))
        .WillOnce(Return(-1));

    int rc = inKernelHandler.registerMctpEndpoint(0x10, SOCK_DGRAM, 0, {});
    EXPECT_LT(rc, 0);
}

// ---------------------------------------------------------------------------
// InKernelHandler::registerMctpEndpoint — isFdValid == true (already set up)
// Takes the early-return branch that just calls manager.registerEndpoint.
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerAlreadyRegistered)
{
    // Simulate isFdValid=true by setting private members directly.
    auto [handlerFd, peerFd] = makeSocketPair();
    ASSERT_GE(handlerFd, 0);
    ::close(peerFd);

    inKernelHandler.isFdValid = true;
    inKernelHandler.fd = handlerFd;
    inKernelHandler.sendBufferSize = 4096;

    // No socket/getsockopt/bind calls expected.
    int rc = inKernelHandler.registerMctpEndpoint(0x20, SOCK_DGRAM, 0, {});
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ---------------------------------------------------------------------------
// InKernelHandler::sendMsg — sendto fails
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerSendMsgFailure)
{
    const int fakeFd = 56;
    const uint8_t eid = 0x10;
    const std::vector<uint8_t> nsmMsg = {0x80, 0x05, 0x01};

    EXPECT_CALL(*mockIo_, sendto(fakeFd, _, nsmMsg.size(), 0, _,
                                 sizeof(struct sockaddr_mctp)))
        .WillOnce(Return(-1));

    int rc = inKernelHandler.sendMsg(0x01, eid, fakeFd, nsmMsg.data(),
                                     nsmMsg.size());
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

// ---------------------------------------------------------------------------
// InKernelHandler::handleReceivedMsg — peekedLength == 0
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerRecvPeekZero)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 57;

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(0));
    // Drain recv: discards the empty datagram and returns.
    EXPECT_CALL(*mockIo_, recv(fakeFd, NotNull(), 1, 0)).WillOnce(Return(0));

    inKernelHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// InKernelHandler::handleReceivedMsg — peekedLength <= -1
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerRecvPeekNegative)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 58;

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(-1));

    inKernelHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// InKernelHandler::handleReceivedMsg — peekedLength > STATIC_BUF_SIZE
// (forces dynamic buffer allocation)
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerLargeMsgDynamicBuffer)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 59;
    // > STATIC_BUF_SIZE (64) → dynamic buffer
    const size_t largeSize = 100;
    std::vector<uint8_t> largeMsg(largeSize, 0x00);
    // smctp_type != 0x7E → skip non-VDM message
    // (addr.smctp_type is from recvfrom's sockaddr, not message bytes)

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(largeSize));

    EXPECT_CALL(*mockIo_, recvfrom(fakeFd, _, largeSize, MSG_TRUNC, _, _))
        .WillOnce([&](int, void* buf, size_t, int, struct sockaddr* addr,
                      socklen_t* addrlen) {
        auto* mctpAddr = reinterpret_cast<struct sockaddr_mctp*>(addr);
        mctpAddr->smctp_family = AF_MCTP;
        mctpAddr->smctp_addr.s_addr = 0x10;
        mctpAddr->smctp_tag = 0x01;
        mctpAddr->smctp_type = 0x00; // non-VDM → skip
        *addrlen = sizeof(struct sockaddr_mctp);
        memcpy(buf, largeMsg.data(), largeMsg.size());
        return static_cast<ssize_t>(largeSize);
    });

    inKernelHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// InKernelHandler::handleReceivedMsg — recvDataLength != peekedLength
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerRecvLengthMismatch)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 60;
    const ssize_t peekedLen = 20;

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(peekedLen));

    // recvfrom returns fewer bytes → mismatch
    EXPECT_CALL(*mockIo_, recvfrom(fakeFd, _, static_cast<size_t>(peekedLen),
                                   MSG_TRUNC, _, _))
        .WillOnce([](int, void*, size_t, int, struct sockaddr*, socklen_t*) {
        return static_cast<ssize_t>(10);
    });

    inKernelHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// InKernelHandler::handleReceivedMsg — non-VDM smctp_type
// Covers the "skip" branch where smctp_type != MCTP_MSG_TYPE_PCI_VDM.
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerNonVdmType)
{
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 61;
    std::vector<uint8_t> nsmMsg = {0x00, 0x05, 0x01, 0x00};
    const size_t totalSize = nsmMsg.size();

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(totalSize));

    EXPECT_CALL(*mockIo_, recvfrom(fakeFd, _, totalSize, MSG_TRUNC, _, _))
        .WillOnce([&](int, void* buf, size_t, int, struct sockaddr* addr,
                      socklen_t* addrlen) {
        auto* mctpAddr = reinterpret_cast<struct sockaddr_mctp*>(addr);
        mctpAddr->smctp_family = AF_MCTP;
        mctpAddr->smctp_addr.s_addr = 0x10;
        mctpAddr->smctp_tag = 0x01;
        mctpAddr->smctp_type = 0x00; // NOT 0x7E → skip
        *addrlen = sizeof(struct sockaddr_mctp);
        memcpy(buf, nsmMsg.data(), nsmMsg.size());
        return static_cast<ssize_t>(totalSize);
    });

    inKernelHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// DaemonHandler::sendMsg — verbose=true covers line 206
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, DaemonHandlerSendMsgVerbose)
{
    mctp_socket::DaemonHandler verboseHandler{event, reqHandler, eventManager,
                                              sockManager, true};
    const int fakeFd = 62;
    const uint8_t eid = 0x10;
    const uint8_t tag = 0x01;
    const std::vector<uint8_t> nsmMsg = {0x80, 0x05, 0x01};

    // Allow any sendmsg (event system may call it with different args/flags)
    // and succeed on the DaemonHandler::sendMsg call with our fakeFd.
    EXPECT_CALL(*mockIo_, sendmsg(_, _, _))
        .WillRepeatedly([fakeFd](int fd, const struct msghdr*, int) {
        return fd == fakeFd ? static_cast<ssize_t>(4)
                            : static_cast<ssize_t>(-1);
    });

    int rc = verboseHandler.sendMsg(tag, eid, fakeFd, nsmMsg.data(),
                                    nsmMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// DaemonHandler::handleReceivedMsg — verbose=true covers lines 273-275
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, DaemonHandlerHandleReceivedMsgVerbose)
{
    mctp_socket::DaemonHandler verboseHandler{event, reqHandler, eventManager,
                                              sockManager, true};
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 63;
    const std::vector<uint8_t> mctpMsg = {0x01, 0x10, 0x7E, 0x00,
                                          0x05, 0x01, 0x00};

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(mctpMsg.size()));
    EXPECT_CALL(*mockIo_, recv(fakeFd, _, mctpMsg.size(), 0))
        .WillOnce([&](int, void* buf, size_t, int) {
        memcpy(buf, mctpMsg.data(), mctpMsg.size());
        return static_cast<ssize_t>(mctpMsg.size());
    });

    verboseHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// DaemonHandler::processRxMsg — NSM_EVENT with verbose=true covers lines 75-77
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, DaemonHandlerNsmEventPathVerbose)
{
    mctp_socket::DaemonHandler verboseHandler{event, reqHandler, eventManager,
                                              sockManager, true};
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 64;
    // NSM event: pci_vendor_id=0x10DE(BE), request=1, datagram=1 → NSM_EVENT
    std::vector<uint8_t> mctpMsg = {
        0x01, 0x10, 0x7E, // MCTP header
        0x10, 0xDE,       // pci_vendor_id
        0xC0,             // request=1(b7), datagram=1(b6)
        0x89,             // ocp_type=8, ocp_version=9
        0x01,             // nvidia_msg_type
        0x00, 0x00, 0x00,
        0x00, 0x00, 0x00  // 6 payload bytes (NSM_EVENT_MIN_LEN)
    };

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(mctpMsg.size()));
    EXPECT_CALL(*mockIo_, recv(fakeFd, _, mctpMsg.size(), 0))
        .WillOnce([&](int, void* buf, size_t, int) {
        memcpy(buf, mctpMsg.data(), mctpMsg.size());
        return static_cast<ssize_t>(mctpMsg.size());
    });

    verboseHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// InKernelHandler::sendMsg — verbose=true covers line 410
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerSendMsgVerbose)
{
    mctp_socket::InKernelHandler verboseHandler{event, reqHandler, eventManager,
                                                sockManager, true};
    const int fakeFd = 65;
    const uint8_t eid = 0x10;
    const std::vector<uint8_t> nsmMsg = {0x80, 0x05, 0x01};

    EXPECT_CALL(*mockIo_, sendto(fakeFd, _, nsmMsg.size(), 0, _,
                                 sizeof(struct sockaddr_mctp)))
        .WillOnce([](int, const void*, size_t len, int, const struct sockaddr*,
                     socklen_t) { return static_cast<ssize_t>(len); });

    int rc = verboseHandler.sendMsg(0x01, eid, fakeFd, nsmMsg.data(),
                                    nsmMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// InKernelHandler::handleReceivedMsg — verbose=true covers lines 481-482
// ---------------------------------------------------------------------------

TEST_F(SocketHandlerTest, InKernelHandlerHandleReceivedMsgVerbose)
{
    mctp_socket::InKernelHandler verboseHandler{event, reqHandler, eventManager,
                                                sockManager, true};
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 66;
    const uint8_t srcEid = 0x10;
    std::vector<uint8_t> nsmMsg = {0x00, 0x05, 0x01, 0x00};
    const size_t totalSize = nsmMsg.size();

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(totalSize));

    EXPECT_CALL(*mockIo_, recvfrom(fakeFd, _, totalSize, MSG_TRUNC, _, _))
        .WillOnce([&](int, void* buf, size_t, int, struct sockaddr* addr,
                      socklen_t* addrlen) {
        auto* mctpAddr = reinterpret_cast<struct sockaddr_mctp*>(addr);
        mctpAddr->smctp_family = AF_MCTP;
        mctpAddr->smctp_addr.s_addr = srcEid;
        mctpAddr->smctp_tag = 0x01;
        mctpAddr->smctp_type = 0x7E; // VDM type
        *addrlen = sizeof(struct sockaddr_mctp);
        memcpy(buf, nsmMsg.data(), nsmMsg.size());
        return static_cast<ssize_t>(totalSize);
    });

    verboseHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// Minimal EventHandler for registering with EventManager in ack-path tests
// ---------------------------------------------------------------------------
class NoOpEventHandler : public nsm::EventHandler
{
  public:
    uint8_t nsmType() override
    {
        return NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    }
    void unsupportedEvent(uint8_t, const nsm_msg*, size_t) override {}
};

// Verify processRxMsg returns non-empty optional for NSM_EVENT with ackr=1
TEST_F(SocketHandlerTest, ProcessRxMsgNsmEventAckReturnsResponse)
{
    eventManager.registerHandler(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                 std::make_unique<NoOpEventHandler>());

    mctp_socket::DaemonHandler testHandler{event, reqHandler, eventManager,
                                           sockManager, false};
    // NSM payload: nsm_msg_hdr(5 bytes) + nsm_event(6 bytes)
    // nvidia_msg_type=0 (NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY)
    // nsm_event: ackr=1 (0x10), event_id=1, zeros
    std::vector<uint8_t> nsmPayload = {0x10, 0xDE, 0xC0, 0x89, 0x00, 0x10,
                                       0x01, 0x00, 0x00, 0x00, 0x00};
    auto result = testHandler.processRxMsg(0x09, 0x10, 0x7E, nsmPayload.data(),
                                           nsmPayload.size());
    EXPECT_TRUE(result.has_value());
}

// ---------------------------------------------------------------------------
// DaemonHandler: NSM_EVENT with ackr=1 → response.has_value() = true
// Covers socket_handler.cpp L289-311 (sendmsg success path)
// ---------------------------------------------------------------------------
TEST_F(SocketHandlerTest, DaemonHandlerNsmEventAckResponseSuccess)
{
    // Register a handler so EventManager::handle() returns an ack response
    eventManager.registerHandler(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                 std::make_unique<NoOpEventHandler>());

    // Use the fixture's daemonHandler (created with the same eventManager ref)
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 70;
    // NSM EVENT: ackr=1 (bit 4 of first payload byte = 0x10)
    // nsm_msg_hdr: pci_vendor_id=0x10DE, request=1+datagram=1 → 0xC0,
    //   ocp=0x89, nvidia_msg_type=NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY=0x01
    // nsm_event: version=0,ackr=1 → byte=0x10, event_id=0x01,
    //   event_class=0, event_state=0, data_size=0
    std::vector<uint8_t> mctpMsg = {
        0x09, 0x10, 0x7E, // MCTP header: tag=0x09, eid=0x10, type=0x7E
        0x10, 0xDE,       // pci_vendor_id (BE)
        0xC0,             // request=1(b7), datagram=1(b6), instance_id=0
        0x89,             // ocp_type=8, ocp_version=9
        0x00,      // nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY = 0
        0x10,      // nsm_event: ackr=1 (bit 4)
        0x01, 0x00, 0x00,
        0x00, 0x00 // event_id=1, class=0, state=0, data_size=0
    };

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(mctpMsg.size()));
    EXPECT_CALL(*mockIo_, recv(fakeFd, _, mctpMsg.size(), 0))
        .WillOnce([&](int, void* buf, size_t, int) {
        memcpy(buf, mctpMsg.data(), mctpMsg.size());
        return static_cast<ssize_t>(mctpMsg.size());
    });
    // Capture all sendmsg calls; verify the response ack is sent on fakeFd.
    // (Other sendmsg calls may come from phosphor-logging to the journal fd.)
    bool ackSent = false;
    EXPECT_CALL(*mockIo_, sendmsg(_, _, _))
        .WillRepeatedly([&](int fd, const struct msghdr*, int) {
        if (fd == fakeFd)
            ackSent = true;
        return 0;
    });

    EXPECT_NO_THROW(daemonHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN));
    EXPECT_TRUE(ackSent) << "sendmsg ack was never sent on fakeFd";

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// DaemonHandler: NSM_EVENT with ackr=1, sendmsg returns -1
// Covers socket_handler.cpp L312-316 (sendmsg failure path)
// ---------------------------------------------------------------------------
TEST_F(SocketHandlerTest, DaemonHandlerNsmEventAckResponseSendFail)
{
    eventManager.registerHandler(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                 std::make_unique<NoOpEventHandler>());

    // Use fixture's daemonHandler
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 71;
    std::vector<uint8_t> mctpMsg = {
        0x09, 0x10, 0x7E, 0x10, 0xDE, 0xC0,
        0x89, 0x00, // nvidia_msg_type = NSM_TYPE_DCD = 0
        0x10, 0x01, 0x00, 0x00, 0x00, 0x00};

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(mctpMsg.size()));
    EXPECT_CALL(*mockIo_, recv(fakeFd, _, mctpMsg.size(), 0))
        .WillOnce([&](int, void* buf, size_t, int) {
        memcpy(buf, mctpMsg.data(), mctpMsg.size());
        return static_cast<ssize_t>(mctpMsg.size());
    });
    // When sendmsg is called on fakeFd, return -1 to exercise the error path.
    // Other sendmsg calls (e.g. phosphor-logging to journal) return success.
    bool errorPathCovered = false;
    EXPECT_CALL(*mockIo_, sendmsg(_, _, _))
        .WillRepeatedly([&](int fd, const struct msghdr*, int) -> ssize_t {
        if (fd == fakeFd)
        {
            errorPathCovered = true;
            return -1; // trigger error-log path at socket_handler.cpp:L312-316
        }
        return 0;
    });

    EXPECT_NO_THROW(daemonHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN));
    EXPECT_TRUE(errorPathCovered) << "sendmsg error path was not exercised";

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// InKernelHandler: NSM_EVENT with ackr=1 → response.has_value() = true
// Covers socket_handler.cpp L495-518 (sendto success path)
// ---------------------------------------------------------------------------
TEST_F(SocketHandlerTest, InKernelHandlerNsmEventAckResponseSuccess)
{
    eventManager.registerHandler(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                 std::make_unique<NoOpEventHandler>());

    mctp_socket::InKernelHandler verboseHandler{event, reqHandler, eventManager,
                                                sockManager, true};
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 72;
    const uint8_t srcEid = 0x10;

    // InKernelHandler: requestMsgData is the NSM payload directly (no MCTP
    // header bytes) recvfrom provides the NSM payload + sockaddr_mctp with
    // EID/tag/type
    std::vector<uint8_t> nsmPayload = {
        0x10, 0xDE, // pci_vendor_id
        0xC0,       // request=1, datagram=1
        0x89,       // ocp
        0x00, // nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY = 0
        0x10, // nsm_event: ackr=1
        0x01, 0x00, 0x00, 0x00, 0x00 // event_id, class, state, data_size
    };

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(nsmPayload.size()));
    EXPECT_CALL(*mockIo_,
                recvfrom(fakeFd, _, nsmPayload.size(), MSG_TRUNC, _, _))
        .WillOnce([&](int, void* buf, size_t, int, struct sockaddr* addr,
                      socklen_t* addrlen) {
        auto* mctpAddr = reinterpret_cast<struct sockaddr_mctp*>(addr);
        mctpAddr->smctp_family = AF_MCTP;
        mctpAddr->smctp_addr.s_addr = srcEid;
        mctpAddr->smctp_tag = 0x09;
        mctpAddr->smctp_type = MCTP_MSG_TYPE_PCI_VDM;
        *addrlen = sizeof(struct sockaddr_mctp);
        memcpy(buf, nsmPayload.data(), nsmPayload.size());
        return static_cast<ssize_t>(nsmPayload.size());
    });
    // sendto sends the ack
    EXPECT_CALL(*mockIo_,
                sendto(fakeFd, _, _, 0, _, sizeof(struct sockaddr_mctp)))
        .WillOnce([](int, const void*, size_t len, int, const struct sockaddr*,
                     socklen_t) { return static_cast<ssize_t>(len); });

    verboseHandler.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN);

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

// ---------------------------------------------------------------------------
// InKernelHandler: NSM_EVENT with ackr=1, sendto returns -1
// Covers socket_handler.cpp L519-524 (sendto failure path)
// ---------------------------------------------------------------------------
TEST_F(SocketHandlerTest, InKernelHandlerNsmEventAckResponseSendFail)
{
    eventManager.registerHandler(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                 std::make_unique<NoOpEventHandler>());

    mctp_socket::InKernelHandler inKernelH{event, reqHandler, eventManager,
                                           sockManager, false};
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    auto ioSrc = makeTestIo(event, pipefd[0]);

    const int fakeFd = 73;
    const uint8_t srcEid = 0x10;
    std::vector<uint8_t> nsmPayload = {
        0x10, 0xDE, 0xC0, 0x89, 0x00, // nvidia_msg_type = NSM_TYPE_DCD = 0
        0x10, 0x01, 0x00, 0x00, 0x00, 0x00};

    EXPECT_CALL(*mockIo_, recv(fakeFd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
        .WillOnce(Return(nsmPayload.size()));
    EXPECT_CALL(*mockIo_,
                recvfrom(fakeFd, _, nsmPayload.size(), MSG_TRUNC, _, _))
        .WillOnce([&](int, void* buf, size_t, int, struct sockaddr* addr,
                      socklen_t* addrlen) {
        auto* mctpAddr = reinterpret_cast<struct sockaddr_mctp*>(addr);
        mctpAddr->smctp_family = AF_MCTP;
        mctpAddr->smctp_addr.s_addr = srcEid;
        mctpAddr->smctp_tag = 0x09;
        mctpAddr->smctp_type = MCTP_MSG_TYPE_PCI_VDM;
        *addrlen = sizeof(struct sockaddr_mctp);
        memcpy(buf, nsmPayload.data(), nsmPayload.size());
        return static_cast<ssize_t>(nsmPayload.size());
    });
    // sendto fails → covers the error-log branch
    EXPECT_CALL(*mockIo_,
                sendto(fakeFd, _, _, 0, _, sizeof(struct sockaddr_mctp)))
        .WillOnce(Return(-1));

    EXPECT_NO_THROW(inKernelH.handleReceivedMsg(ioSrc, fakeFd, EPOLLIN));

    ::close(pipefd[0]);
    ::close(pipefd[1]);
}
