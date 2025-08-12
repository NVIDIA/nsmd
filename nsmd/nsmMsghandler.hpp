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

#include "base.h"

#include "requester/handler.hpp"
#include "types.hpp"

#include <phosphor-logging/lg2.hpp>

#include <coroutine>
#include <memory>

// Forward declarations
namespace nsm
{
class NsmDevice;
}

namespace mctp
{
class MctpDiscovery;
}

namespace nsm
{

using RequesterHandler = requester::Handler<requester::Request>;

/**
 * @class NsmMessageHandler
 * @brief Singleton class for handling NSM message send/receive operations
 *
 * This class provides a centralized way to send and receive NSM messages.
 * Access is restricted to NsmDevice and MctpDiscovery classes through friend
 * declarations.
 */
class NsmMessageHandler : public std::enable_shared_from_this<NsmMessageHandler>
{
  public:
    // Delete copy constructor and assignment operator
    NsmMessageHandler(const NsmMessageHandler&) = delete;
    NsmMessageHandler& operator=(const NsmMessageHandler&) = delete;

    // Delete move constructor and assignment operator
    NsmMessageHandler(NsmMessageHandler&&) = delete;
    NsmMessageHandler& operator=(NsmMessageHandler&&) = delete;

    /**
     * @brief Get the singleton instance as a shared pointer
     * @return Shared pointer to the singleton instance
     */
    static std::shared_ptr<NsmMessageHandler> getSharedInstance()
    {
        if (!instance)
        {
            throw std::runtime_error(
                "NsmMessageHandler instance is not initialized yet");
        }
        return instance;
    }

    static void initialize(RequesterHandler& handler)
    {
        if (instance)
        {
            throw std::logic_error(
                "Initialize called on an already initialized NsmMessageHandler");
        }
        // Use a helper struct to access private constructor
        struct MakeSharedEnabler : public NsmMessageHandler
        {
            MakeSharedEnabler(RequesterHandler& h) : NsmMessageHandler(h) {}
        };
        instance = std::make_shared<MakeSharedEnabler>(handler);
    }

  private:
    // Static shared pointer for singleton
    static std::shared_ptr<NsmMessageHandler> instance;

    RequesterHandler& reqHandler;

  protected:
    // Protected constructor (accessible to MakeSharedEnabler)
    explicit NsmMessageHandler(RequesterHandler& handler) : reqHandler(handler)
    {}

    // Protected destructor
    ~NsmMessageHandler() = default;

  private:
    /**
     * @brief Send and receive NSM message (private - only accessible to friend
     * classes)
     * @param eid Endpoint ID
     * @param request Request message
     * @param responseMsg Pointer to response message
     * @param responseLen Response length
     * @return Coroutine with the result code
     */
    inline requester::Coroutine
        SendRecvNsmMsg(eid_t eid, Request& request,
                       std::shared_ptr<const nsm_msg>& responseMsg,
                       size_t* responseLen)
    {
        const nsm_msg* response = nullptr;
        auto rc = co_await requester::SendRecvNsmMsg<RequesterHandler>(
            reqHandler, eid, request, &response, responseLen);
        responseMsg = std::shared_ptr<const nsm_msg>(response, [](auto) {
        }); // the memory is allocated and free at sock_handler.cpp

        if (rc)
        {
            lg2::error(
                "NsmMessageHandler::SendRecvNsmMsg failed. eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
        }
        // coverity[missing_return]
        co_return rc;
    }

    // Friend classes that can access the private SendRecvNsmMsg method
    friend class NsmDevice;
    friend class mctp::MctpDiscovery;
};

// Initialize the static member - this must be in the header for header-only
// implementation
inline std::shared_ptr<NsmMessageHandler> NsmMessageHandler::instance = nullptr;

} // namespace nsm
