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

#pragma once

#include "libnsm/base.h"
#include "libnsm/requester/mctp.h"

#include "common/types.hpp"
#include "common/utils.hpp"
#include "nsmd/socket_handler.hpp"

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>

#include <chrono>
#include <functional>
#include <iostream>

namespace requester
{

/** @class RequestRetryTimer
 *
 *  The abstract base class for implementing the NSM request retry logic. This
 *  class handles number of times the NSM request needs to be retried if the
 *  response is not received and the time to wait between each retry. It
 *  provides APIs to start and stop the request flow.
 */
class RequestRetryTimer
{
  public:
    RequestRetryTimer() = delete;
    RequestRetryTimer(const RequestRetryTimer&) = delete;
    RequestRetryTimer(RequestRetryTimer&&) = default;
    RequestRetryTimer& operator=(const RequestRetryTimer&) = delete;
    RequestRetryTimer& operator=(RequestRetryTimer&&) = default;
    virtual ~RequestRetryTimer() = default;

    /** @brief Constructor
     *
     *  @param[in] event - reference to NSM daemon's main event loop
     *  @param[in] numRetries - number of request retries
     *  @param[in] timeout - time to wait between each retry in milliseconds
     */
    explicit RequestRetryTimer(sdeventplus::Event& event, uint8_t numRetries,
                               std::chrono::milliseconds timeout) :

        event(event),
        numRetries(numRetries), timeout(timeout),
        timer(event.get(), std::bind_front(&RequestRetryTimer::callback, this))
    {}

    /** @brief Starts the request flow and arms the timer for request retries
     *
     *  @return return NSM_SUCCESS on success and NSM_ERROR otherwise
     */
    int start()
    {
        auto rc = send();
        if (rc)
        {
            return rc;
        }

        try
        {
            if (numRetries)
            {
                timer.start(duration_cast<std::chrono::microseconds>(timeout),
                            true);
            }
        }
        catch (const std::runtime_error& e)
        {
            lg2::error("Failed to start the request timer.", "ERROR", e);
            return NSM_ERROR;
        }

        return NSM_SW_SUCCESS;
    }

    /** @brief Stops the timer and no further request retries happen */
    void stop()
    {
        auto rc = timer.stop();
        if (rc)
        {
            lg2::error("Failed to stop the request timer. RC={RC}", "RC",
                       unsigned(rc));
        }
    }

  protected:
    sdeventplus::Event& event; //!< reference to NSM daemon's main event loop
    uint8_t numRetries;        //!< number of request retries
    std::chrono::milliseconds
        timeout;            //!< time to wait between each retry in milliseconds
    sdbusplus::Timer timer; //!< manages starting timers and handling timeouts

    /** @brief Sends the NSM request message
     *
     *  @return return NSM_SUCCESS on success and NSM_ERROR otherwise
     */
    virtual int send() const = 0;

    /** @brief Callback function invoked when the timeout happens */
    void callback()
    {
        if (numRetries--)
        {
            send();
        }
        else
        {
            stop();
        }
    }
};

/** @class Request
 *
 *  The concrete implementation of RequestIntf. This class implements the send()
 *  to send the NSM request message over MCTP socket.
 *  This class encapsulates the NSM request message, the number of times the
 *  request needs to retried if the response is not received and the amount of
 *  time to wait between each retry. It provides APIs to start and stop the
 *  request flow.
 */
class Request final : public RequestRetryTimer
{
  public:
    Request() = delete;
    Request(const Request&) = delete;
    Request(Request&&) = default;
    Request& operator=(const Request&) = delete;
    Request& operator=(Request&&) = default;
    ~Request() = default;

    /** @brief Constructor
     *
     *  @param[in] fd - fd of the MCTP communication socket
     *  @param[in] eid - endpoint ID of the remote MCTP endpoint
     *  @param[in] event - reference to NSM daemon's main event loop
     *  @param[in] requestMsg - NSM request message
     *  @param[in] numRetries - number of request retries
     *  @param[in] timeout - time to wait between each retry in milliseconds
     *  @param[in] verbose - verbose tracing flag
     */
    explicit Request(int fd, eid_t eid, uint8_t tag, sdeventplus::Event& event,
                     const mctp_socket::Handler* handler,
                     std::vector<uint8_t>&& requestMsg, uint8_t numRetries,
                     std::chrono::milliseconds timeout, bool verbose) :
        RequestRetryTimer(event, numRetries, timeout),
        fd(fd), eid(eid), tag(tag), requestMsg(std::move(requestMsg)),
        verbose(verbose), socketHandler(handler)
    {}

    uint8_t getInstanceId()
    {
        auto nsmMsg = reinterpret_cast<nsm_msg*>(requestMsg.data());
        return nsmMsg->hdr.instance_id;
    }

    void setInstanceId(uint8_t instanceId)
    {
        auto nsmMsg = reinterpret_cast<nsm_msg*>(requestMsg.data());
        nsmMsg->hdr.instance_id = instanceId;
    }

    std::string requestMsgToString() const
    {
        std::ostringstream oss;
        for (const auto& byte : requestMsg)
        {
            oss << std::setfill('0') << std::setw(2) << std::hex
                << static_cast<int>(byte) << " ";
        }
        return oss.str();
    }

  private:
    int fd;      //!< file descriptor of MCTP communications socket
    eid_t eid;   //!< endpoint ID of the remote MCTP endpoint
    uint8_t tag; //!< tag mctp message tag to be used
    std::vector<uint8_t> requestMsg;           //!< NSM request message
    bool verbose;                              //!< verbose tracing flag
    const mctp_socket::Handler* socketHandler; // MCTP socket handler

    /** @brief Sends the NSM request message on the socket
     *
     *  @return return NSM_SUCCESS on success and NSM_ERROR otherwise
     */
    int send() const
    {
        if (verbose)
        {
            utils::printBuffer(utils::Tx, requestMsg);
        }

        auto rc = socketHandler->sendMsg(tag, eid, fd, requestMsg.data(),
                                         requestMsg.size());
        if (rc < 0)
        {
            lg2::error("Failed to send NSM message. RC={RC}, errno={ERRNO}",
                       "RC", unsigned(rc), "ERRNO", strerror(errno));
            return NSM_SW_ERROR;
        }
        return NSM_SW_SUCCESS;
    }

    static int nsm_send(eid_t eid, uint8_t tag, int mctp_fd,
                        const uint8_t* nsm_req_msg, size_t nsm_req_len)
    {
        uint8_t hdr[3] = {tag, eid,
                          MCTP_MSG_TYPE_PCI_VDM}; // TO_TAG, EID, MCTP_MSG_TYPE

        struct iovec iov[2];
        iov[0].iov_base = hdr;
        iov[0].iov_len = sizeof(hdr);
        iov[1].iov_base = (uint8_t*)nsm_req_msg;
        iov[1].iov_len = nsm_req_len;
        struct msghdr msg = {};
        msg.msg_iov = iov;
        msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

        ssize_t rc = sendmsg(mctp_fd, &msg, 0);
        if (rc == -1)
        {
            return NSM_SW_ERROR;
        }
        return NSM_SW_SUCCESS;
    }
};

} // namespace requester
