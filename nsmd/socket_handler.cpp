/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
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

#include "config.h"

#include "socket_handler.hpp"

#include "libnsm/base.h"

#include "common/globals.hpp"
#include "requester/handler.hpp"
#include "socket_manager.hpp"

#include <linux/if_arp.h>
#include <linux/mctp.h>
#include <sys/types.h>
#include <sys/un.h>

// The userspace <linux/mctp.h> comes from linux-libc-headers, which lags the
// BMC kernel's uapi, so this option may be missing. (SOL_MCTP itself comes from
// glibc and is always defined.) Remove once linux-libc-headers carries it.
#ifndef MCTP_OPT_TAG_TIMEOUT_MS
#define MCTP_OPT_TAG_TIMEOUT_MS 4
#endif

#include <phosphor-logging/lg2.hpp>

#ifdef LTTNG_TRACING
#include "tracepoints/nsmd-tp.h"
#endif

// Activates syscall redirect macros — must be the last include so all system
// headers are already parsed before the macros take effect.
#define NSMD_SOCKET_REDIRECT
#include "socketIo.hpp"

namespace mctp_socket
{

alignas(64) uint8_t
    InKernelHandler::staticBuffer[InKernelHandler::STATIC_BUF_SIZE];
alignas(64) uint8_t DaemonHandler::staticBuffer[DaemonHandler::STATIC_BUF_SIZE];

std::optional<Response> Handler::processRxMsg(uint8_t tag, uint8_t eid,
                                              [[maybe_unused]] uint8_t type,
                                              const uint8_t* nsmMsg,
                                              size_t nsmMsgSize)
{
    nsm_header_info hdrFields{};
    auto hdr = reinterpret_cast<const nsm_msg_hdr*>(nsmMsg);
    if (NSM_SUCCESS != unpack_nsm_header(hdr, &hdrFields))
    {
        lg2::error(
            "Failure in unpacking NSM request header: eid={EID} tag={TAG} "
            "mctpType={MTYPE} msgLen={LEN}",
            "EID", eid, "TAG", tag, "MTYPE", type, "LEN", nsmMsgSize);
        utils::printBuffer(utils::Rx, nsmMsg, nsmMsgSize, tag, eid);
        return std::nullopt;
    }

    const size_t nsmRespMinimusLen = sizeof(struct nsm_msg_hdr) +
                                     NSM_RESPONSE_MIN_LEN;
    const size_t nsmEventMinimusLen = sizeof(struct nsm_msg_hdr) +
                                      NSM_EVENT_MIN_LEN;

    if (NSM_EVENT == hdrFields.nsm_msg_type && nsmMsgSize >= nsmEventMinimusLen)
    {
        auto event = reinterpret_cast<const nsm_msg*>(hdr);
        size_t eventLen = nsmMsgSize;
        uint8_t type = event->hdr.nvidia_msg_type;
        uint8_t eventId = event->payload[1];
        if (verbose)
        {
            lg2::info(
                "received nsm event type={TYPE} eventId={ID} eventLen={LEN} from EID={EID}",
                "TYPE", type, "ID", eventId, "LEN", eventLen, "EID", eid);
        }
        return eventManager.handle(eid, type, eventId, event, eventLen);
    }
    else if (NSM_RESPONSE == hdrFields.nsm_msg_type &&
             nsmMsgSize >= nsmRespMinimusLen)
    {
        auto response = reinterpret_cast<const nsm_msg*>(hdr);
        size_t responseLen = nsmMsgSize;

#ifdef LTTNG_TRACING
        lttng_ust_tracepoint(nsmd, socket_recv, eid, response, responseLen);
#endif

        handler.handleResponse(tag, eid, hdrFields.instance_id,
                               hdrFields.nvidia_msg_type, response->payload[0],
                               response, responseLen);
    }
    return std::nullopt;
}

int DaemonHandler::registerMctpEndpoint(eid_t eid, int type, int protocol,
                                        const std::vector<uint8_t>& pathName)
{
    auto entry = socketInfoMap.find(pathName);
    if (entry == socketInfoMap.end())
    {
        auto [fd, currentSendBufferSize] = initSocket(eid, type, protocol,
                                                      pathName);
        if (fd < 0)
        {
            return fd;
        }
        else
        {
            manager.registerEndpoint(eid, fd, currentSendBufferSize);
        }
    }
    else
    {
        manager.registerEndpoint(eid, (*(std::get<0>(entry->second)).get())(),
                                 std::get<1>(entry->second));
    }

    return 0;
}

SocketInfo DaemonHandler::initSocket([[maybe_unused]] eid_t eid, int type,
                                     int protocol,
                                     const std::vector<uint8_t>& pathName)
{
    /* Create socket */
    int rc = 0;
    int sendBufferSize = 0;
    int sockFd = socket(AF_UNIX, type, protocol);
    if (sockFd == -1)
    {
        rc = -errno;
        lg2::error("Failed to create the socket, RC={RC}", "RC", strerror(-rc));
        return {rc, sendBufferSize};
    }

    auto fd = std::make_unique<utils::CustomFD>(sockFd);

    /* Get socket current buffer size */
    socklen_t optlen;
    optlen = sizeof(sendBufferSize);
    rc = getsockopt(sockFd, SOL_SOCKET, SO_SNDBUF, &sendBufferSize, &optlen);
    if (rc == -1)
    {
        rc = -errno;
        lg2::error("Error getting the default socket send buffer size, RC={RC}",
                   "RC", strerror(-rc));
        return {rc, sendBufferSize};
    }

    // /* Initiate a connection to the socket */
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, pathName.data(), pathName.size());
    rc = connect(sockFd, reinterpret_cast<struct sockaddr*>(&addr),
                 pathName.size() + sizeof(addr.sun_family));
    if (rc == -1)
    {
        rc = -errno;
        lg2::error("Failed to connect to the socket, RC={RC}", "RC",
                   strerror(-rc));
        return {rc, sendBufferSize};
    }

    /* Register for VDM(0x7e) message type */
    ssize_t result = write(sockFd, &MCTP_MSG_TYPE_VDM,
                           sizeof(MCTP_MSG_TYPE_VDM));
    if (result == -1)
    {
        rc = -errno;
        lg2::error(
            "Failed to register VDM message type to demux daemon, RC={RC}",
            "RC", strerror(-rc));
        return {rc, sendBufferSize};
    }

    auto io = std::make_unique<IO>(
        event, sockFd, EPOLLIN,
        std::bind_front(&DaemonHandler::handleReceivedMsg, this));
    // io->set_priority(SD_EVENT_SOURCE_MAX_PRIORITY);
    socketInfoMap[pathName] = std::tuple(std::move(fd), sendBufferSize,
                                         std::move(io));

    return {sockFd, sendBufferSize};
}

int DaemonHandler::sendMsg(uint8_t tag, eid_t eid, int mctpFd,
                           const uint8_t* nsmMsg, size_t nsmMsgLen) const
{
    uint8_t hdr[3] = {tag, eid,
                      MCTP_MSG_TYPE_PCI_VDM}; // TO_TAG, EID, MCTP_MSG_TYPE

    struct iovec iov[2];
    iov[0].iov_base = hdr;
    iov[0].iov_len = sizeof(hdr);
    iov[1].iov_base = (uint8_t*)nsmMsg;
    iov[1].iov_len = nsmMsgLen;
    struct msghdr msg = {};
    msg.msg_iov = iov;
    msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

    if (verbose)
    {
        utils::printBuffer(utils::Tx, nsmMsg, nsmMsgLen, tag, eid);
    }

    ssize_t rc = sendmsg(mctpFd, &msg, 0);
    if (rc == -1)
    {
        return NSM_SW_ERROR;
    }

    return NSM_SW_SUCCESS;
}

void DaemonHandler::handleReceivedMsg(IO& io, int fd, uint32_t revents)
{
    if (!(revents & EPOLLIN))
    {
        return;
    }

    // Outgoing message.
    struct iovec iov[2]{};

    // This structure contains the parameter information for the response
    // message.
    struct msghdr msg{};

    int returnCode = 0;
    ssize_t peekedLength = recv(fd, nullptr, 0, MSG_PEEK | MSG_TRUNC);
    if (peekedLength == 0)
    {
        // MCTP daemon has closed the socket this daemon is connected to.
        // This may or may not be an error scenario, in either case the
        // recovery mechanism for this daemon is to restart, and hence
        // exit the event loop, that will cause this daemon to exit with a
        // failure code.
        io.get_event().exit(0);
    }
    else if (peekedLength <= -1)
    {
        returnCode = -errno;
        lg2::error("recv system call failed, RC={RC}", "RC", returnCode);
    }
    else
    {
        uint8_t* requestMsgData;
        size_t requestMsgSize = peekedLength;

        // Only create dynamic buffer when actually needed
        std::optional<std::vector<uint8_t>> dynamicBuffer;
        if (static_cast<size_t>(peekedLength) <= STATIC_BUF_SIZE)
        {
            requestMsgData = staticBuffer;
        }
        else
        {
            // Constructs a vector of size peekedLength in the optional
            // dynamicBuffer
            dynamicBuffer.emplace(peekedLength);
            requestMsgData = dynamicBuffer->data();
        }

        auto recvDataLength = recv(fd, static_cast<void*>(requestMsgData),
                                   requestMsgSize, 0);
        if (recvDataLength == peekedLength)
        {
            if (verbose)
            {
                utils::printBuffer(utils::Rx, &requestMsgData[3],
                                   requestMsgSize - 3, requestMsgData[0],
                                   requestMsgData[1]);
            }

            if (MCTP_MSG_TYPE_VDM != requestMsgData[2])
            {
                // Skip this message and continue.
            }
            else
            {
                // process message and send response
                auto response = processRxMsg(
                    requestMsgData[0], requestMsgData[1], requestMsgData[2],
                    &requestMsgData[3], requestMsgSize - 3);
                if (response.has_value())
                {
                    constexpr uint8_t tagOwnerBitPos = 3;
                    constexpr uint8_t tagOwnerMask = ~(1 << tagOwnerBitPos);
                    // Set tag owner bit to 0 for NSM responses
                    requestMsgData[0] = requestMsgData[0] & tagOwnerMask;
                    iov[0].iov_base = &requestMsgData[0];
                    iov[0].iov_len = sizeof(requestMsgData[0]) +
                                     sizeof(requestMsgData[1]) +
                                     sizeof(requestMsgData[2]);
                    iov[1].iov_base = (*response).data();
                    iov[1].iov_len = (*response).size();

                    if (verbose)
                    {
                        utils::printBuffer(utils::Tx, *response,
                                           requestMsgData[0],
                                           requestMsgData[1]);
                    }

                    msg.msg_iov = iov;
                    msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

                    int result = sendmsg(fd, &msg, 0);
                    if (-1 == result)
                    {
                        returnCode = -errno;
                        lg2::error("sendto system call failed, RC={RC}", "RC",
                                   returnCode);
                    }
                }
            }
        }
        else
        {
            lg2::error("Failure to read peeked length packet. peekedLength="
                       "{PEEKEDLENGTH} recvDataLength={RECVDATALENGTH}",
                       "PEEKEDLENGTH", peekedLength, "RECVDATALENGTH",
                       recvDataLength);
        }
    }
}

int InKernelHandler::registerMctpEndpoint(
    eid_t eid, [[maybe_unused]] int type, [[maybe_unused]] int protocol,
    [[maybe_unused]] const std::vector<uint8_t>& pathName)
{
    if (isFdValid)
    {
        manager.registerEndpoint(eid, fd, sendBufferSize);
        return NSM_SUCCESS;
    }

    fd = socket(AF_MCTP, SOCK_DGRAM, 0);

    int rc = 0;
    if (fd == -1)
    {
        rc = -errno;
        lg2::error("Failed to create the socket, RC={RC}, EID={ED}", "RC",
                   strerror(-rc), "ED", eid);
        return rc;
    }

    socklen_t optlen;
    optlen = sizeof(sendBufferSize);
    rc = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendBufferSize, &optlen);
    if (rc == -1)
    {
        rc = -errno;
        lg2::error(
            "Error getting the default socket send buffer size, RC={RC}, EID={ED}",
            "RC", strerror(-rc), "ED", eid);
        return rc;
    }

    struct timeval sendTimeout = {.tv_sec = MCTP_SEND_TIMEOUT_MS / 1000,
                                  .tv_usec = (MCTP_SEND_TIMEOUT_MS % 1000) *
                                             1000};
    rc = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &sendTimeout,
                    sizeof(sendTimeout));
    if (rc == -1)
    {
        rc = -errno;
        lg2::error(
            "Error setting send timeout on MCTP socket, RC={RC}, EID={ED}",
            "RC", strerror(-rc), "ED", eid);
        close(fd);
        return rc;
    }

    // Cap the MCTP tag/key lifetime at RESPONSE_TIME_OUT (defaults to 2s): NSM
    // responses arrive well within that, so tags can be reclaimed sooner than
    // the 6s kernel default. This is a best-effort optimisation — if the
    // running kernel does not support the option, log and carry on with the
    // default rather than failing.
    int tagTimeoutMs = RESPONSE_TIME_OUT;
    rc = setsockopt(fd, SOL_MCTP, MCTP_OPT_TAG_TIMEOUT_MS, &tagTimeoutMs,
                    sizeof(tagTimeoutMs));
    if (rc == -1)
    {
        lg2::warning(
            "Could not set MCTP tag timeout on socket (using kernel default), "
            "RC={RC}, EID={ED}",
            "RC", strerror(errno), "ED", eid);
    }

    struct sockaddr_mctp addr;
    memset(&addr, 0, sizeof(addr));

    addr.smctp_family = AF_MCTP;
    addr.smctp_network = MCTP_NET_ANY;
    addr.smctp_addr.s_addr = MCTP_ADDR_ANY;
    addr.smctp_tag = MCTP_TAG_OWNER;
    addr.smctp_type = MCTP_MSG_TYPE_PCI_VDM;

    rc = bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (rc == -1)
    {
        rc = -errno;
        lg2::error(
            "Error while binding the socket to NSM Msg Type, RC={RC}, EID={ED}",
            "RC", strerror(-rc), "ED", eid);
        return rc;
    }

    io = std::make_unique<IO>(
        event, fd, EPOLLIN,
        std::bind_front(&InKernelHandler::handleReceivedMsg, this));
    // io->set_priority(SD_EVENT_SOURCE_MAX_PRIORITY);

    manager.registerEndpoint(eid, fd, sendBufferSize);

    isFdValid = true;

    return NSM_SUCCESS;
}

int InKernelHandler::sendMsg([[maybe_unused]] uint8_t tag, eid_t eid,
                             int mctpFd, const uint8_t* nsmMsg,
                             size_t nsmMsgLen) const
{
    struct sockaddr_mctp addr;
    memset(&addr, 0, sizeof(addr));

    addr.smctp_family = AF_MCTP;
    addr.smctp_network = MCTP_NET_ANY;
    addr.smctp_addr.s_addr = eid;
    addr.smctp_tag = MCTP_TAG_OWNER;
    addr.smctp_type = MCTP_MSG_TYPE_PCI_VDM;

    if (verbose)
    {
        utils::printBuffer(utils::Tx, nsmMsg, nsmMsgLen, addr.smctp_tag, eid);
    }

    ssize_t rc = sendto(mctpFd, nsmMsg, nsmMsgLen, 0,
                        reinterpret_cast<struct sockaddr*>(&addr),
                        sizeof(addr));
    if (rc == -1)
    {
        int error = -errno;
        lg2::error("sendto syscall failed. EID={ED}, RC={RC}", "ED", eid, "RC",
                   strerror(-error));
        return NSM_SW_ERROR;
    }

    return NSM_SW_SUCCESS;
}

void InKernelHandler::handleReceivedMsg([[maybe_unused]] IO& io, int fd,
                                        [[maybe_unused]] uint32_t revents)
{
    int returnCode{0};

    ssize_t peekedLength = recv(fd, nullptr, 0, MSG_PEEK | MSG_TRUNC);
    if (peekedLength == 0)
    {
        // A zero-length datagram is a valid (if unusual) message on a
        // SOCK_DGRAM AF_MCTP socket, not an end-of-stream/disconnect
        // condition (datagram sockets have no EOF). Terminating here let any
        // MCTP peer take the daemon down with a single empty packet, so drain
        // the empty datagram and continue processing instead.
        uint8_t drain = 0;
        ssize_t drained = recv(fd, &drain, sizeof(drain), 0);
        if (drained < 0)
        {
            lg2::error("Failed to drain zero-length MCTP datagram, RC={RC}",
                       "RC", -errno);
        }
        else
        {
            lg2::warning("Received zero-length MCTP datagram; ignoring.");
        }
        return;
    }
    else if (peekedLength <= -1)
    {
        returnCode = -errno;
        lg2::error("recv system call failed, RC={RC}", "RC", returnCode);
    }
    else
    {
        uint8_t* requestMsgData;
        size_t requestMsgSize = peekedLength;

        // Only create dynamic buffer when actually needed
        std::optional<std::vector<uint8_t>> dynamicBuffer;
        if (static_cast<size_t>(peekedLength) <= STATIC_BUF_SIZE)
        {
            requestMsgData = staticBuffer;
        }
        else
        {
            // Constructs a vector of size peekedLength in the optional
            // dynamicBuffer
            dynamicBuffer.emplace(peekedLength);
            requestMsgData = dynamicBuffer->data();
        }

        struct sockaddr_mctp addr;
        memset(&addr, 0, sizeof(addr));

        socklen_t addrlen = sizeof(addr);

        ssize_t recvDataLength = recvfrom(
            fd, static_cast<void*>(requestMsgData), peekedLength, MSG_TRUNC,
            reinterpret_cast<struct sockaddr*>(&addr), &addrlen);

        if (recvDataLength == peekedLength)
        {
            if (verbose)
            {
                utils::printBuffer(utils::Rx, requestMsgData, requestMsgSize,
                                   addr.smctp_tag, addr.smctp_addr.s_addr);
            }

            if (MCTP_MSG_TYPE_VDM != addr.smctp_type)
            {
                // Skip this message and continue.
            }
            else
            {
                // process message and send response
                auto response = processRxMsg(
                    addr.smctp_tag, addr.smctp_addr.s_addr, addr.smctp_type,
                    requestMsgData, requestMsgSize);
                if (response.has_value())
                {
                    if (verbose)
                    {
                        utils::printBuffer(utils::Tx, *response, addr.smctp_tag,
                                           addr.smctp_addr.s_addr);
                    }

                    struct sockaddr_mctp destAddr;
                    memset(&destAddr, 0, sizeof(destAddr));

                    destAddr.smctp_family = AF_MCTP;
                    destAddr.smctp_network = MCTP_NET_ANY;
                    destAddr.smctp_addr.s_addr = addr.smctp_addr.s_addr;
                    destAddr.smctp_type = addr.smctp_type;

                    constexpr uint8_t tagOwnerBitPos = 3;
                    constexpr uint8_t tagOwnerMask = ~(1 << tagOwnerBitPos);
                    destAddr.smctp_tag = addr.smctp_tag & tagOwnerMask;

                    ssize_t rc =
                        sendto(fd, response->data(), response->size(), 0,
                               reinterpret_cast<struct sockaddr*>(&destAddr),
                               sizeof(destAddr));
                    if (rc == -1)
                    {
                        returnCode = -errno;
                        lg2::error("sendmsg system call failed, RC={RC}", "RC",
                                   returnCode);
                    }
                }
            }
        }
        else
        {
            returnCode = -errno;
            lg2::error(
                "Failure to read peeked length packet. peekedLength="
                "{PEEKEDLENGTH} recvDataLength={RECVDATALENGTH} ErrorNo={ERROR}",
                "PEEKEDLENGTH", peekedLength, "RECVDATALENGTH", recvDataLength,
                "ERROR", returnCode);
        }
    }
}
} // namespace mctp_socket
