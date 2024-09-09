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

#include "config.h"

#include "libnsm/base.h"
#include "libnsm/requester/mctp.h"

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmd/instance_id.hpp"
#include "nsmd/socket_manager.hpp"
#include "request.hpp"
#include "request_timeout_tracker.hpp"

#include <function2/function2.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>

#include <cassert>
#include <chrono>
#include <memory>
#include <queue>
#include <tuple>
#include <unordered_map>

namespace requester
{

using ResponseHandler = fu2::unique_function<void(
    eid_t eid, const nsm_msg* response, size_t respMsgLen)>;

/** @class Handler
 *
 *  This class handles the lifecycle of the NSM request message based on the
 *  instance ID expiration interval, number of request retries and the timeout
 *  waiting for a response. The registered response handlers are invoked with
 *  response once the NSM responder sends the response. If no response is
 *  received within the instance ID expiration interval or any other failure the
 *  response handler is invoked with the empty response.
 *
 * @tparam RequestInterface - Request class type
 */
template <class RequestInterface>
class Handler
{
  private:
    /** @brief Container for storing the details of the NSM request
     *         message, handler for the corresponding NSM response, the
     *         timer object for the Instance ID expiration and valid flag
     */
    using RequestValue =
        std::tuple<std::unique_ptr<RequestInterface>, ResponseHandler,
                   std::unique_ptr<sdbusplus::Timer>, bool>;
    using RequestQueue = std::queue<RequestValue>;

  public:
    Handler() = delete;
    Handler(const Handler&) = delete;
    Handler(Handler&&) = delete;
    Handler& operator=(const Handler&) = delete;
    Handler& operator=(Handler&&) = delete;
    ~Handler() = default;

    /** @brief Constructor
     *
     *  @param[in] event - reference to NSM daemon's main event loop
     *  @param[in] instanceIdDb - reference to instance id allocator
     *  @param[in] sockManager - MCTP socket manager
     *  @param[in] verbose - verbose tracing flag
     *  @param[in] instanceIdExpiryInterval - instance ID expiration interval
     *  @param[in] numRetries - number of request retries which is in addition
     * to the first attempt
     *  @param[in] responseTimeOut - time to wait between each retry
     */
    explicit Handler(
        sdeventplus::Event& event, nsm::InstanceIdDb& instanceIdDb,
        mctp_socket::Manager& sockManager, bool verbose,
        std::chrono::seconds instanceIdExpiryInterval =
            std::chrono::seconds(INSTANCE_ID_EXPIRATION_INTERVAL),
        std::chrono::seconds instanceIdExpiryIntervalLongRunning =
            std::chrono::seconds(INSTANCE_ID_EXPIRATION_INTERVAL_LONG_RUNNING),
        uint8_t numRetries = static_cast<uint8_t>(NUMBER_OF_REQUEST_RETRIES),
        std::chrono::milliseconds responseTimeOut =
            std::chrono::milliseconds(RESPONSE_TIME_OUT),
        std::chrono::milliseconds responseTimeOutLongRunning =
            std::chrono::milliseconds(RESPONSE_TIME_OUT_LONG_RUNNING)) :
        event(event),
        instanceIdDb(instanceIdDb), sockManager(sockManager), verbose(verbose),
        instanceIdExpiryIntervalRegular(instanceIdExpiryInterval),
        instanceIdExpiryIntervalLongRunning(
            instanceIdExpiryIntervalLongRunning),
        numRetries(numRetries), responseTimeOutRegular(responseTimeOut),
        responseTimeOutLongRunning(responseTimeOutLongRunning)
    {}

    int registerRequestImpl(
        uint8_t tag, eid_t eid, uint8_t type, uint8_t command,
        std::vector<uint8_t>&& requestMsg, ResponseHandler&& responseHandler,
        std::unordered_map<eid_t, RequestQueue>& handlers,
        std::unordered_map<eid_t, std::unique_ptr<sdbusplus::Timer>>&
            timerToFree,
        std::chrono::milliseconds responseTimeOut,
        std::chrono::seconds instanceIdExpiryInterval)
    {
        auto instanceIdExpiryCallBack = [eid, type, command, &handlers,
                                         &timerToFree, instanceIdExpiryInterval,
                                         this](void) {
            if (handlers.contains(eid) && !handlers[eid].empty())
            {
                auto& [request, responseHandler, timerInstance,
                       valid] = handlers[eid].front();

                // Note1: timeOutTracker object can be updated through
                // TimeoutEvent or a succesfull responseMsg, for  handling
                // please refer handleResponse as well.

                // Note2: timeoutTracker code should be above request->stop() or
                // any operation that can change requestMsg as part of cleanup
                nsm::DeviceRequestTimeOutTracker& timeoutTracker =
                    nsm::TimeOutTracker::getInstance().getDeviceTimeOutTracker(
                        eid);
                std::string msg = request->requestMsgToString();
                timeoutTracker.handleTimeout(msg);

                request->stop();
                auto rc = timerInstance->stop();
                if (rc)
                {
                    lg2::error(
                        "Failed to stop the instance ID expiry timer. RC={RC}",
                        "RC", rc);
                }

                // Defer to remove expired timer and run queued request
                // the timerInstance callback cannot free timerInstance itself
                timerToFree[eid] = std::move(timerInstance);
                this->removeRequestContainer[eid] =
                    std::make_unique<sdeventplus::source::Defer>(
                        event,
                        std::bind(&Handler::removeRequestEntry, this, eid,
                                  std::ref(handlers), std::ref(timerToFree),
                                  instanceIdExpiryInterval));

                // Call responseHandler after erase it from the handlers to
                // avoid starting the same request again in
                // runRegisteredRequest()
                auto unique_handler = std::move(responseHandler);

                instanceIdDb.free(eid, request->getInstanceId());
                handlers[eid].pop();

                // Call response handler with an empty response to indicate
                // no response
                unique_handler(eid, nullptr, 0);
            }
            else
            {
                // This condition is not possible, if a response is received
                // before the instance ID expiry, then the response handler
                // is executed and the entry will be removed.
                assert(false);
            }
        };

        if (requestMsg.size() >
            static_cast<size_t>(sockManager.getSendBufferSize(eid)))
        {
            sockManager.setSendBufferSize(sockManager.getSocket(eid),
                                          requestMsg.size());
        }

        auto request = std::make_unique<RequestInterface>(
            sockManager.getSocket(eid), eid, tag, event, std::move(requestMsg),
            numRetries, responseTimeOut, verbose);
        auto timer = std::make_unique<sdbusplus::Timer>(
            event.get(), instanceIdExpiryCallBack);

        handlers[eid].emplace(std::make_tuple(std::move(request),
                                              std::move(responseHandler),
                                              std::move(timer), true));
        return runRegisteredRequest(eid, handlers, instanceIdExpiryInterval);
    }

    /** @brief Register a NSM request message
     *
     *  @param[in] eid - endpoint ID of the remote MCTP endpoint
     *  @param[in] type - NSM message type
     *  @param[in] command - NSM command
     *  @param[in] requestMsg - NSM request message
     *  @param[in] responseHandler - Response handler for this request
     *
     *  @return return NSM_SUCCESS on success and NSM_ERROR otherwise
     */
    int registerRequestRegular(eid_t eid, uint8_t type, uint8_t command,
                               std::vector<uint8_t>&& requestMsg,
                               ResponseHandler&& responseHandler)
    {
        return registerRequestImpl(
            MCTP_MSG_TAG_REQ, eid, type, command, std::move(requestMsg),
            std::move(responseHandler), handlersRegular, timerToFreeRegular,
            responseTimeOutRegular, instanceIdExpiryIntervalRegular);
    }

    /** @brief Register a NSM Long-running request message
     *
     *  @param[in] eid - endpoint ID of the remote MCTP endpoint
     *  @param[in] type - NSM message type
     *  @param[in] command - NSM command
     *  @param[in] requestMsg - NSM request message
     *  @param[in] responseHandler - Response handler for this request
     *
     *  @return return NSM_SUCCESS on success and NSM_ERROR otherwise
     */
    int registerRequestLongRunning(eid_t eid, uint8_t type, uint8_t command,
                                   std::vector<uint8_t>&& requestMsg,
                                   ResponseHandler&& responseHandler)
    {
        return registerRequestImpl(
            MCTP_MSG_TAG_LONG_RUNNING_REQ, eid, type, command,
            std::move(requestMsg), std::move(responseHandler),
            handlersLongRunning, timerToFreeLongRunning,
            responseTimeOutLongRunning, instanceIdExpiryIntervalLongRunning);
    }

    int runRegisteredRequest(eid_t eid,
                             std::unordered_map<eid_t, RequestQueue>& handlers,
                             std::chrono::seconds instanceIdExpiryInterval)
    {
        if (handlers[eid].empty())
        {
            return NSM_SUCCESS;
        }

        auto& [request, responseHandler, timerInstance,
               valid] = handlers[eid].front();

        if (timerInstance->isRunning())
        {
            // A NSM request for the EID is running
            return NSM_SUCCESS;
        }

        try
        {
            // get instance_id from pool
            auto instanceId = instanceIdDb.next(eid);
            request->setInstanceId(instanceId);
        }
        catch (const std::exception& e)
        {
            lg2::error("Error while get MCTP instanceId for EID={EID}, {ERROR}",
                       "EID", eid, "ERROR", e);
            return NSM_ERROR;
        }

        auto rc = request->start();
        if (rc)
        {
            instanceIdDb.free(eid, request->getInstanceId());
            lg2::error("Failure to send the NSM request message");
            return rc;
        }

        try
        {
            timerInstance->start(duration_cast<std::chrono::microseconds>(
                instanceIdExpiryInterval));
        }
        catch (const std::runtime_error& e)
        {
            instanceIdDb.free(eid, request->getInstanceId());
            lg2::error("Failed to start the instance ID expiry timer.", "ERROR",
                       e);
            return NSM_ERROR;
        }

        return NSM_SUCCESS;
    }

    /** @brief Handle NSM response message
     *
     *  @param[in] tag - MCTP message tag of the response
     *  @param[in] eid - endpoint ID of the remote MCTP endpoint
     *  @param[in] instanceId - instance ID to match request and response
     *  @param[in] type - NVIDIA message type
     *  @param[in] command - NSM command
     *  @param[in] response - NSM response message
     *  @param[in] respMsgLen - length of the response message
     */
    void handleResponse(uint8_t tag, eid_t eid, uint8_t instanceId,
                        [[maybe_unused]] uint8_t type,
                        [[maybe_unused]] uint8_t command,
                        const nsm_msg* response, size_t respMsgLen)
    {
        switch (tag)
        {
            case MCTP_TAG_NSM:
                handleResponseImpl(eid, instanceId, type, command, response,
                                   respMsgLen, handlersRegular,
                                   instanceIdExpiryIntervalRegular);
                break;

            case MCTP_TAG_NSM_ASYNC:
                handleResponseImpl(eid, instanceId, type, command, response,
                                   respMsgLen, handlersLongRunning,
                                   instanceIdExpiryIntervalLongRunning);
                break;

            default:
                lg2::error(
                    "Received invalid MCTP Tag {TAG}. EID={EID}, Type={TYPE}, Command={CMD}.",
                    "TAG", tag, "EID", eid, "TYPE", type, "CMD", command);
                break;
        }
    }

    void handleResponseImpl(eid_t eid, uint8_t instanceId,
                            [[maybe_unused]] uint8_t type,
                            [[maybe_unused]] uint8_t command,
                            const nsm_msg* response, size_t respMsgLen,
                            std::unordered_map<eid_t, RequestQueue>& handlers,
                            std::chrono::seconds instanceIdExpiryInterval)
    {
        if (handlers.contains(eid) && !handlers[eid].empty())
        {
            auto& [request, responseHandler, timerInstance,
                   valid] = handlers[eid].front();
            if (request->getInstanceId() == instanceId)
            {
                // Note1: timeOutTracker can be updated through TimeoutEvent or
                // a succesfull responseMsg, for better handling please refer
                // instanceIdExpiryCallBack as well

                // Note2: timeoutTracker code should be above request->stop() or
                // any operation that can change requestMsg as part of cleanup
                nsm::DeviceRequestTimeOutTracker& timeoutTracker =
                    nsm::TimeOutTracker::getInstance().getDeviceTimeOutTracker(
                        eid);
                std::string msg = request->requestMsgToString();
                timeoutTracker.handleNoTimeout(msg);

                request->stop();
                auto rc = timerInstance->stop();
                if (rc)
                {
                    lg2::error(
                        "Failed to stop the instance ID expiry timer. RC={RC}",
                        "RC", rc);
                }
                // Call responseHandler after erase it from the handlers to
                // avoid starting it again in runRegisteredRequest()
                auto unique_handler = std::move(responseHandler);
                instanceIdDb.free(eid, request->getInstanceId());
                handlers[eid].pop();
                unique_handler(eid, response, respMsgLen);
            }
        }

        runRegisteredRequest(eid, handlers, instanceIdExpiryInterval);
    }

    bool hasInProgressRequest(eid_t eid)
    {
        bool hasRegularReq{false};
        bool hasLongRunnigReq{false};

        if (handlersRegular.contains(eid) && !handlersRegular[eid].empty())
        {
            auto& valid = std::get<3>(handlersRegular[eid].front());
            auto& timerInstance = std::get<2>(handlersRegular[eid].front());
            hasRegularReq = valid && timerInstance->isRunning();
        }

        if (handlersLongRunning.contains(eid) &&
            !handlersLongRunning[eid].empty())
        {
            auto& valid = std::get<3>(handlersLongRunning[eid].front());
            auto& timerInstance = std::get<2>(handlersLongRunning[eid].front());
            hasLongRunnigReq = valid && timerInstance->isRunning();
        }

        return hasRegularReq || hasLongRunnigReq;
    }

    void invalidInProgressRequest(eid_t eid, [[maybe_unused]] uint8_t tag)
    {
        if (handlersRegular.contains(eid) && !handlersRegular[eid].empty())
        {
            // force timer timeout
            auto& timerInstance = std::get<2>(handlersRegular[eid].front());
            if (timerInstance->isRunning())
            {
                timerInstance->start(std::chrono::microseconds(1));

                auto& valid = std::get<3>(handlersRegular[eid].front());
                valid = false;
            }
        }

        if (handlersLongRunning.contains(eid) &&
            !handlersLongRunning[eid].empty())
        {
            // force timer timeout
            auto& timerInstance = std::get<2>(handlersLongRunning[eid].front());
            if (timerInstance->isRunning())
            {
                timerInstance->start(std::chrono::microseconds(1));

                auto& valid = std::get<3>(handlersLongRunning[eid].front());
                valid = false;
            }
        }
    }

  private:
    sdeventplus::Event& event; //!< reference to NSM daemon's main event loop
    nsm::InstanceIdDb& instanceIdDb; //!< reference to instanceIdDb object
    mctp_socket::Manager& sockManager;

    bool verbose;                        //!< verbose tracing flag
    std::chrono::seconds
        instanceIdExpiryIntervalRegular; //!< Instance ID expiration interval
    std::chrono::seconds
        instanceIdExpiryIntervalLongRunning; //!< Instance ID expiration
                                             //!< interval for Long Running
                                             //!< commands
    uint8_t numRetries;                      //!< number of request retries
    std::chrono::milliseconds
        responseTimeOutRegular;     //!< time to wait between each retry
    std::chrono::milliseconds
        responseTimeOutLongRunning; //!< time to wait between each retry for
                                    //!< Long Running commands

    /** @brief Container for storing the NSM request entries */
    std::unordered_map<eid_t, RequestQueue> handlersRegular;

    /** @brief Container for storing the NSM Long Running request entries */
    std::unordered_map<eid_t, RequestQueue> handlersLongRunning;

    /** @brief Container to store information about the request entries to be
     *         removed after the instance ID timer expires
     */
    std::unordered_map<eid_t, std::unique_ptr<sdeventplus::source::Defer>>
        removeRequestContainer;

    std::unordered_map<eid_t, std::unique_ptr<sdbusplus::Timer>>
        timerToFreeRegular;
    std::unordered_map<eid_t, std::unique_ptr<sdbusplus::Timer>>
        timerToFreeLongRunning;

    /** @brief Remove request entry for which the instance ID expired
     *
     *  @param[in] eid - eid for the Request
     */
    void removeRequestEntry(
        eid_t eid, std::unordered_map<eid_t, RequestQueue>& handlers,
        std::unordered_map<eid_t, std::unique_ptr<sdbusplus::Timer>>&
            timerToFree,
        std::chrono::seconds instanceIdExpiryInterval)
    {
        timerToFree[eid] = nullptr;
        runRegisteredRequest(eid, handlers, instanceIdExpiryInterval);
    }
};

/** @struct SendRecvNsmMsg
 *
 * An awaitable object needed by co_await operator to send/recv NSM
 * message.
 * e.g.
 * rc = co_await SendRecvNsmMsg<h>(h, eid, req, respMsg, respLen);
 *
 * @tparam RequesterHandler - Requester::handler class type
 */
template <class RequesterHandler>
struct SendRecvNsmMsg
{
    /** @brief For recording the suspended coroutine where the co_await
     * operator is. When NSM response message is received, the resumeHandle()
     * will be called to continue the next line of co_await operator
     */
    std::coroutine_handle<> resumeHandle;

    /** @brief The RequesterHandler to send/recv NSM message.
     */
    RequesterHandler& handler;

    /** @brief Whether it is long-running NSM Request.
     */
    bool isLongRunning;

    /** @brief The EID where NSM message will be sent to.
     */
    uint8_t eid;

    /** @brief The NSM request message.
     */
    std::vector<uint8_t>& request;

    /** @brief The pointer of NSM response message.
     */
    const nsm_msg** responseMsg;

    /** @brief The length of NSM response message.
     */
    size_t* responseLen;

    /** @brief For keeping the return value of RequesterHandler.
     */
    uint8_t rc;

    /** @brief Returning false to make await_suspend() to be called.
     */
    bool await_ready() noexcept
    {
        return false;
    }

    /** @brief Called by co_await operator before suspending coroutine. The
     * method will send out NSM request message, register handleResponse() as
     * call back function for the event when NSM response message received.
     */
    bool await_suspend(std::coroutine_handle<> handle) noexcept
    {
        if (responseMsg == nullptr || responseLen == nullptr)
        {
            rc = NSM_SW_ERROR_NULL;
            return false;
        }

        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

        if (isLongRunning)
        {
            rc = handler.registerRequestLongRunning(
                eid, requestMsg->hdr.nvidia_msg_type, requestMsg->payload[0],
                std::move(request),
                std::move(
                    std::bind_front(&SendRecvNsmMsg::HandleResponse, this)));
        }
        else
        {
            rc = handler.registerRequestRegular(
                eid, requestMsg->hdr.nvidia_msg_type, requestMsg->payload[0],
                std::move(request),
                std::move(
                    std::bind_front(&SendRecvNsmMsg::HandleResponse, this)));
        }

        if (rc)
        {
            lg2::error("registerRequest failed, rc={RC}", "RC",
                       static_cast<unsigned>(rc));
            return false;
        }

        resumeHandle = handle;
        return true;
    }

    /** @brief Called by co_await operator to get return value when awaitable
     * object completed.
     */
    uint8_t await_resume() const noexcept
    {
        return rc;
    }

    /** @brief Constructor of awaitable object to initialize necessary member
     * variables.
     */
    SendRecvNsmMsg(RequesterHandler& handler, eid_t eid,
                   std::vector<uint8_t>& request, const nsm_msg** responseMsg,
                   size_t* responseLen, bool isLongRunning) :
        handler(handler),
        isLongRunning(isLongRunning), eid(eid), request(request),
        responseMsg(responseMsg), responseLen(responseLen), rc(NSM_ERROR)
    {}

    /** @brief The function will be registered by ReqisterHandler for handling
     * NSM response message. */
    void HandleResponse([[maybe_unused]] eid_t eid, const nsm_msg* response,
                        size_t length)
    {
        if (response == nullptr || !length)
        {
            rc = NSM_SW_ERROR_NULL;
        }
        else
        {
            *responseMsg = response;
            *responseLen = length;
            rc = NSM_SW_SUCCESS;
        }
        resumeHandle();
    }
};

} // namespace requester
