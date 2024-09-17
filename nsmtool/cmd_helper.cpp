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

#include "cmd_helper.hpp"

#include "requester/mctp.h"

#include "xyz/openbmc_project/Common/error.hpp"

#include <linux/mctp.h>
#include <systemd/sd-bus.h>

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <exception>

using namespace utils;

namespace nsmtool
{

namespace helper
{
/*
 * Initialize the socket, send nsm command & recieve response from socket
 *
 */
int mctpSockSendRecv(const std::vector<uint8_t>& requestMsg,
                     std::vector<uint8_t>& responseMsg, bool verbose)
{
    const char devPath[] = "\0mctp-pcie-mux";
    int returnCode = 0;

    int sockFd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (-1 == sockFd)
    {
        returnCode = -errno;
        std::cerr << "Failed to create the socket : RC = " << sockFd << "\n";
        return returnCode;
    }
    Logger(verbose, "Success in creating the socket : RC = ", sockFd);

    struct sockaddr_un addr
    {};
    addr.sun_family = AF_UNIX;

    memcpy(addr.sun_path, devPath, sizeof(devPath) - 1);

    CustomFD socketFd(sockFd);
    int result = connect(socketFd(), reinterpret_cast<struct sockaddr*>(&addr),
                         sizeof(devPath) + sizeof(addr.sun_family) - 1);
    if (-1 == result)
    {
        returnCode = -errno;
        std::cerr << "Failed to connect to socket : RC = " << returnCode
                  << "\n";
        return returnCode;
    }
    Logger(verbose, "Success in connecting to socket : RC = ", returnCode);

    auto mctpMsgType = MCTP_MSG_TYPE_PCI_VDM;
    result = write(socketFd(), &mctpMsgType, sizeof(mctpMsgType));
    if (-1 == result)
    {
        returnCode = -errno;
        std::cerr << "Failed to send message type as VDM to mctp : RC = "
                  << returnCode << "\n";
        return returnCode;
    }
    Logger(verbose, "Success in sending message type as VDM to mctp : RC = ",
           returnCode);

    result = send(socketFd(), requestMsg.data(), requestMsg.size(), 0);
    if (-1 == result)
    {
        returnCode = -errno;
        std::cerr << "Write to socket failure : RC = " << returnCode << "\n";
        return returnCode;
    }
    Logger(verbose, "Write to socket successful : RC = ", result);

    // Read the response from socket
    ssize_t peekedLength = recv(socketFd(), nullptr, 0, MSG_TRUNC | MSG_PEEK);
    if (0 == peekedLength)
    {
        std::cerr << "Socket is closed : peekedLength = " << peekedLength
                  << "\n";
        return returnCode;
    }
    else if (peekedLength <= -1)
    {
        returnCode = -errno;
        std::cerr << "recv() system call failed : RC = " << returnCode << "\n";
        return returnCode;
    }
    else
    {
        auto reqhdr = reinterpret_cast<const nsm_msg_hdr*>(&requestMsg[2]);
        do
        {
            auto peekedLength = recv(socketFd(), nullptr, 0,
                                     MSG_PEEK | MSG_TRUNC);
            if (-1 == peekedLength)
            {
                returnCode = -errno;
                std::cerr << "Failed to recv message length : RC = "
                          << returnCode << "\n";
                return returnCode;
            }
            responseMsg.resize(peekedLength);
            auto recvDataLength =
                recv(socketFd(), reinterpret_cast<void*>(responseMsg.data()),
                     peekedLength, 0);
            auto resphdr =
                reinterpret_cast<const nsm_msg_hdr*>(&responseMsg[2]);
            if (recvDataLength == peekedLength &&
                resphdr->instance_id == reqhdr->instance_id &&
                resphdr->request == 0)
            {
                Logger(verbose, "Total length:", recvDataLength);
                break;
            }
            else if (recvDataLength != peekedLength)
            {
                std::cerr << "Failure to read response length packet: length = "
                          << recvDataLength << "\n";
                return returnCode;
            }
        } while (1);
    }

    returnCode = shutdown(socketFd(), SHUT_RDWR);
    if (-1 == returnCode)
    {
        returnCode = -errno;
        std::cerr << "Failed to shutdown the socket : RC = " << returnCode
                  << "\n";
        return returnCode;
    }

    Logger(verbose, "Shutdown Socket successful :  RC = ", returnCode);
    return NSM_SW_SUCCESS;
}

/*
 * Initialize the socket, send nsm command & recieve response from socket with
 * in-kernel MCTP
 *
 */
int inKernelMctpSockSendRecv(const std::vector<uint8_t>& requestMsg,
                             std::vector<uint8_t>& responseMsg, bool verbose)
{
    int returnCode = 0;

    int sockFd = socket(AF_MCTP, SOCK_DGRAM, 0);
    if (-1 == sockFd)
    {
        returnCode = -errno;
        std::cerr << "Failed to create the socket : RC = " << sockFd << "\n";
        return returnCode;
    }
    Logger(verbose, "Success in creating the socket : RC = ", sockFd);

    CustomFD socketFd(sockFd);

    struct sockaddr_mctp addr;
    memset(&addr, 0, sizeof(addr));

    addr.smctp_family = AF_MCTP;
    addr.smctp_network = MCTP_NET_ANY;
    addr.smctp_addr.s_addr = requestMsg[1];
    addr.smctp_tag = MCTP_TAG_OWNER;
    addr.smctp_type = requestMsg[2];

    int result = sendto(socketFd(), &requestMsg[3], requestMsg.size() - 3, 0,
                        (struct sockaddr*)&addr, sizeof(addr));
    if (-1 == result)
    {
        returnCode = -errno;
        std::cerr << "Failed to send message type as VDM to mctp : RC = "
                  << returnCode << "\n";
        return returnCode;
    }
    Logger(verbose, "Success in sending message type as VDM to mctp : RC = ",
           returnCode);

    // Read the response from socket
    ssize_t peekedLength = recv(socketFd(), nullptr, 0, MSG_TRUNC | MSG_PEEK);
    if (0 == peekedLength)
    {
        std::cerr << "Socket is closed : peekedLength = " << peekedLength
                  << "\n";
        return returnCode;
    }
    else if (peekedLength <= -1)
    {
        returnCode = -errno;
        std::cerr << "recv() system call failed : RC = " << returnCode << "\n";
        return returnCode;
    }
    else
    {
        auto reqhdr = reinterpret_cast<const nsm_msg_hdr*>(&requestMsg[3]);
        do
        {
            auto peekedLength = recv(socketFd(), nullptr, 0,
                                     MSG_PEEK | MSG_TRUNC);
            if (-1 == peekedLength)
            {
                returnCode = -errno;
                std::cerr << "Failed to recv message length : RC = "
                          << returnCode << "\n";
                return returnCode;
            }
            responseMsg.resize(peekedLength);

            struct sockaddr_mctp addr;
            socklen_t addrlen;
            addrlen = sizeof(addr);
            memset(&addr, 0, sizeof(addr));

            ssize_t recvDataLength = recvfrom(
                socketFd(), reinterpret_cast<void*>(responseMsg.data()),
                peekedLength, 0, (struct sockaddr*)&addr, &addrlen);

            auto resphdr =
                reinterpret_cast<const nsm_msg_hdr*>(responseMsg.data());
            if (recvDataLength == peekedLength &&
                resphdr->instance_id == reqhdr->instance_id &&
                resphdr->request == 0)
            {
                return NSM_SW_SUCCESS;
            }
            else if (recvDataLength != peekedLength)
            {
                std::cerr << "Failure to read response length packet: length = "
                          << recvDataLength << "\n";
                return returnCode;
            }
        } while (1);
    }
}

// parser for bitField variable
void parseBitfieldVar(ordered_json& res, const std::string& key,
                      const bitfield8_t* value, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
    {
        if (value[i].bits.bit0)
        {
            res[key].push_back((i * 8) + 0);
        }
        if (value[i].bits.bit1)
        {
            res[key].push_back((i * 8) + 1);
        }
        if (value[i].bits.bit2)
        {
            res[key].push_back((i * 8) + 2);
        }
        if (value[i].bits.bit3)
        {
            res[key].push_back((i * 8) + 3);
        }
        if (value[i].bits.bit4)
        {
            res[key].push_back((i * 8) + 4);
        }
        if (value[i].bits.bit5)
        {
            res[key].push_back((i * 8) + 5);
        }
        if (value[i].bits.bit6)
        {
            res[key].push_back((i * 8) + 6);
        }
        if (value[i].bits.bit7)
        {
            res[key].push_back((i * 8) + 7);
        }
    }
}

void CommandInterface::exec()
{
    instanceId = 0;
    auto [rc, requestMsg] = createRequestMsg();
    if (rc != NSM_SW_SUCCESS)
    {
        std::cerr << "Failed to encode request message for " << nsmType << ":"
                  << commandName << " rc = " << rc << "\n";
        return;
    }

    std::vector<uint8_t> responseMsg;
    rc = nsmSendRecv(requestMsg, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        std::cerr << "nsmSendRecv: Failed to receive RC = " << rc << "\n";
        return;
    }

    auto responsePtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());
    parseResponseMsg(responsePtr, responseMsg.size());
}

std::tuple<int, int, std::vector<uint8_t>>
    CommandInterface::getMctpSockInfo(uint8_t remoteEID)
{
    std::set<dbus::Service> mctpCtrlServices;
    int type = 0;
    int protocol = 0;
    std::vector<uint8_t> address{};
    auto& bus = utils::DBusHandler::getBus();
    const auto mctpEndpointIntfName{"xyz.openbmc_project.MCTP.Endpoint"};
    const auto unixSocketIntfName{"xyz.openbmc_project.Common.UnixSocket"};

    try
    {
        const dbus::Interfaces ifaceList{"xyz.openbmc_project.MCTP.Endpoint"};
        auto getSubTreeResponse = utils::DBusHandler().getSubtree(
            "/xyz/openbmc_project/mctp", 0, ifaceList);
        for (const auto& [objPath, mapperServiceMap] : getSubTreeResponse)
        {
            for (const auto& [serviceName, interfaces] : mapperServiceMap)
            {
                dbus::ObjectValueTree objects{};

                auto method = bus.new_method_call(
                    serviceName.c_str(), "/xyz/openbmc_project/mctp",
                    "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
                auto reply = bus.call(method);
                reply.read(objects);
                for (const auto& [objectPath, interfaces] : objects)
                {
                    if (interfaces.contains(mctpEndpointIntfName))
                    {
                        const auto& mctpProperties =
                            interfaces.at(mctpEndpointIntfName);
                        auto eid = std::get<size_t>(mctpProperties.at("EID"));
                        if (remoteEID == eid)
                        {
                            if (interfaces.contains(unixSocketIntfName))
                            {
                                const auto& properties =
                                    interfaces.at(unixSocketIntfName);
                                type = std::get<size_t>(properties.at("Type"));
                                protocol =
                                    std::get<size_t>(properties.at("Protocol"));
                                address = std::get<std::vector<uint8_t>>(
                                    properties.at("Address"));
                                if (address.empty() || !type)
                                {
                                    address.clear();
                                    return {0, 0, address};
                                }
                                else
                                {
                                    return {type, protocol, address};
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        address.clear();
        return {0, 0, address};
    }

    return {type, protocol, address};
}

int CommandInterface::nsmSendRecv(std::vector<uint8_t>& requestMsg,
                                  std::vector<uint8_t>& responseMsg)
{
    bool mctpVerbose = verbose;

    // By default enable request/response msgs for nsmtool raw commands.
    if (CommandInterface::nsmType == "raw")
    {
        verbose = true;
    }

    if (verbose)
    {
        std::cout << "nsmtool: ";
        printBuffer(Tx, requestMsg);
    }

    if (mctp_eid != NSM_ENTITY_ID)
    {
#ifdef MCTP_IN_KERNEL
        std::vector<uint8_t> reqMsg{MCTP_MSG_TAG_REQ, mctp_eid,
                                    MCTP_MSG_TYPE_PCI_VDM};
        reqMsg.insert(reqMsg.end(), requestMsg.begin(), requestMsg.end());

        inKernelMctpSockSendRecv(reqMsg, responseMsg, mctpVerbose);
#else
        auto [type, protocol, sockAddress] = getMctpSockInfo(mctp_eid);
        if (sockAddress.empty())
        {
            std::cerr << "nsmtool: Remote MCTP endpoint not found"
                      << "\n";
            return -1;
        }

        int rc = 0;
        int sockFd = socket(AF_UNIX, type, protocol);
        if (-1 == sockFd)
        {
            rc = -errno;
            std::cerr << "Failed to create the socket : RC = " << sockFd
                      << "\n";
            return rc;
        }
        Logger(verbose, "Success in creating the socket : RC = ", sockFd);

        CustomFD socketFd(sockFd);

        struct sockaddr_un addr
        {};
        addr.sun_family = AF_UNIX;
        memcpy(addr.sun_path, sockAddress.data(), sockAddress.size());
        rc = connect(sockFd, reinterpret_cast<struct sockaddr*>(&addr),
                     sockAddress.size() + sizeof(addr.sun_family));
        if (-1 == rc)
        {
            rc = -errno;
            std::cerr << "Failed to connect to socket : RC = " << rc << "\n";
            return rc;
        }
        Logger(verbose, "Success in connecting to socket : RC = ", rc);

        auto mctpMsgType = MCTP_MSG_TYPE_PCI_VDM;
        rc = write(socketFd(), &mctpMsgType, sizeof(mctpMsgType));
        if (-1 == rc)
        {
            rc = -errno;
            std::cerr
                << "Failed to send message type as VDM to mctp demux daemon: RC = "
                << rc << "\n";
            return rc;
        }
        Logger(
            verbose,
            "Success in sending message type as VDM to mctp demux daemon : RC = ",
            rc);

        uint8_t* responseMessage = nullptr;
        size_t responseMessageSize{};
        rc = nsm_send_recv(mctp_eid, sockFd, requestMsg.data(),
                           requestMsg.size(), &responseMessage,
                           &responseMessageSize);
        if (rc < 0)
        {
            std::cerr << "nsm_send_recv() failed RC = " << rc << "\n";
        }

        responseMsg.resize(responseMessageSize);
        memcpy(responseMsg.data(), responseMessage, responseMsg.size());

        free(responseMessage);
#endif
        if (verbose)
        {
            std::cout << "nsmtool: ";
            printBuffer(Rx, responseMsg);
        }
    }
    else
    {
        requestMsg.insert(requestMsg.begin(), MCTP_MSG_TAG_REQ);
        requestMsg.insert(requestMsg.begin(), mctp_eid);
        requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PCI_VDM);

#ifdef MCTP_IN_KERNEL
        inKernelMctpSockSendRecv(requestMsg, responseMsg, mctpVerbose);
#else
        mctpSockSendRecv(requestMsg, responseMsg, mctpVerbose);
        responseMsg.erase(responseMsg.begin(),
                          responseMsg.begin() + 2 /* skip the mctp header */);
#endif

        if (verbose)
        {
            std::cout << "nsmtool: ";
            printBuffer(Rx, responseMsg);
        }
    }
    return NSM_SW_SUCCESS;
}
} // namespace helper
} // namespace nsmtool
