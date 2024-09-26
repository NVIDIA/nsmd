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

#include "socket_handler.hpp"

#include "libnsm/base.h"

#include "common/globals.hpp"
#include "socket_manager.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <phosphor-logging/lg2.hpp>

namespace mctp_socket
{
int Handler::registerMctpEndpoint(eid_t eid, int type, int protocol,
                                  const std::vector<uint8_t>& pathName)
{
    auto entry = socketInfoMap.find(pathName);
    if (entry == socketInfoMap.end())
    {
        auto [fd, currentSendBufferSize] = initSocket(type, protocol, pathName);
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

SocketInfo Handler::initSocket(int type, int protocol,
                               const std::vector<uint8_t>& pathName)
{
    auto callback = [this](IO& io, int fd, uint32_t revents) mutable {
        if (!(revents & EPOLLIN))
        {
            return;
        }

        struct pollfd pollSet[1];
        pollSet[0].fd = fd;
        pollSet[0].events = POLLIN;
        int numFds = 1;
        int timeout = 1;
        // The sendRecvNsmMsgSync() API also poll the same fd and recv data from
        // the fd just before the callback invoked. So add poll checking again
        // to avoid the recv() API blocking forever.
        int ret = poll(pollSet, numFds, timeout);
        if (ret <= 0)
        {
            return;
        }

        // Outgoing message.
        struct iovec iov[2]{};

        // This structure contains the parameter information for the response
        // message.
        struct msghdr msg
        {};

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
            std::vector<uint8_t> requestMsg(peekedLength);
            auto recvDataLength = recv(
                fd, static_cast<void*>(requestMsg.data()), peekedLength, 0);
            if (recvDataLength == peekedLength)
            {
                if (verbose)
                {
                    utils::printBuffer(utils::Rx, requestMsg);
                }

                if (MCTP_MSG_TYPE_VDM != requestMsg[2])
                {
                    // Skip this message and continue.
                }
                else
                {
                    // process message and send response
                    auto response = processRxMsg(requestMsg);
                    if (response.has_value())
                    {
                        if (verbose)
                        {
                            utils::printBuffer(utils::Tx, *response);
                        }

                        constexpr uint8_t tagOwnerBitPos = 3;
                        constexpr uint8_t tagOwnerMask = ~(1 << tagOwnerBitPos);
                        // Set tag owner bit to 0 for NSM responses
                        requestMsg[0] = requestMsg[0] & tagOwnerMask;
                        iov[0].iov_base = &requestMsg[0];
                        iov[0].iov_len = sizeof(requestMsg[0]) +
                                         sizeof(requestMsg[1]) +
                                         sizeof(requestMsg[2]);
                        iov[1].iov_base = (*response).data();
                        iov[1].iov_len = (*response).size();

                        msg.msg_iov = iov;
                        msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

                        int result = sendmsg(fd, &msg, 0);
                        if (-1 == result)
                        {
                            returnCode = -errno;
                            lg2::error("sendto system call failed, RC={RC}",
                                       "RC", returnCode);
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
    };

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
    struct sockaddr_un addr
    {};
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

    auto io = std::make_unique<IO>(event, sockFd, EPOLLIN, std::move(callback));
    io->set_priority(SD_EVENT_SOURCE_MAX_PRIORITY);
    socketInfoMap[pathName] = std::tuple(std::move(fd), sendBufferSize,
                                         std::move(io));

    return {sockFd, sendBufferSize};
}

std::optional<Response>
    Handler::processRxMsg(const std::vector<uint8_t>& nsmMsg)
{
    uint8_t eid = nsmMsg[1];
    const uint8_t tag = nsmMsg[0];
    nsm_header_info hdrFields{};
    auto hdr =
        reinterpret_cast<const nsm_msg_hdr*>(nsmMsg.data() + MCTP_DEMUX_PREFIX);
    if (NSM_SUCCESS != unpack_nsm_header(hdr, &hdrFields))
    {
        lg2::error("Empty NSM request header");
        return std::nullopt;
    }

    const size_t nsmRespMinimusLen =
        MCTP_DEMUX_PREFIX + sizeof(struct nsm_msg_hdr) + NSM_RESPONSE_MIN_LEN;
    const size_t nsmEventMinimusLen =
        MCTP_DEMUX_PREFIX + sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN;

    if (NSM_EVENT == hdrFields.nsm_msg_type &&
        nsmMsg.size() >= nsmEventMinimusLen)
    {
        auto event = reinterpret_cast<const nsm_msg*>(hdr);
        size_t eventLen = nsmMsg.size() - MCTP_DEMUX_PREFIX;
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
             nsmMsg.size() >= nsmRespMinimusLen)
    {
        auto response = reinterpret_cast<const nsm_msg*>(hdr);
        size_t responseLen = nsmMsg.size() - MCTP_DEMUX_PREFIX;
        handler.handleResponse(tag, eid, hdrFields.instance_id,
                               hdrFields.nvidia_msg_type, response->payload[0],
                               response, responseLen);
    }
    return std::nullopt;
}

} // namespace mctp_socket
