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

#include "request_timeout_tracker.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <queue>
#include <string>
#include <system_error>

namespace requester
{
DeviceRequestTimeOutTracker::DeviceRequestTimeOutTracker(eid_t eid) : eid(eid)
{}

DeviceRequestTimeOutTracker& DeviceRequestTimeOutTracker::instance(eid_t eid)
{
    if (instances.find(eid) == instances.end())
    {
        instances[eid] = std::shared_ptr<DeviceRequestTimeOutTracker>(
            new DeviceRequestTimeOutTracker(eid));
    }
    return *instances[eid];
}
void DeviceRequestTimeOutTracker::logFailures()
{
    for (const auto& pair : instances)
    {
        pair.second->logTimeOutFailure();
    }
}

// push on a full queue, will remove the oldest message
void DeviceRequestTimeOutTracker::push(const RequestBase& request)
{
    if (size() == MAXSIZE)
    {
        pop_front();
    }
    push_back(request);
}

void DeviceRequestTimeOutTracker::pushWithTimeout(const RequestBase& request)
{
    auto& tracker = instance(request.eid);
    if (tracker.timeoutMessage.has_value())
    {
        // skip further timeout messages as first timeout request has been
        // recorded
        return;
    }
    tracker.timeoutMessage = request;
}

void DeviceRequestTimeOutTracker::pushWithoutTimeout(const RequestBase& request)
{
    auto& tracker = instance(request.eid);
    if (tracker.timeoutMessage.has_value())
    {
        // device responded after a timeout, reset tracker params
        tracker.timeoutMessage = std::nullopt;
        tracker.clear();
    }
    tracker.push(request);
}

void DeviceRequestTimeOutTracker::logTimeOutFailure() const
{
    lg2::error("******Start logTimeOutFailure: EID={EID}*****", "EID", eid);
    if (timeoutMessage.has_value())
    {
        auto i = size() - 1;
        for (const auto& message : *this)
        {
            const std::string nthRequest = i > 0 ? "n-" + std::to_string(i--)
                                                 : "n";
            lg2::error(
                "logTimeOutFailure: EID={EID}, Last({N}) successful NSM request message before timeout: {REQ} ",
                "EID", eid, "N", nthRequest, "REQ",
                utils::convertMsgToString(true, message.requestMsg, message.tag,
                                          message.eid));
        }
        lg2::error(
            "logTimeOutFailure: EID={EID}, Timeout for NSM request: {REQ}",
            "EID", eid, "REQ",
            utils::convertMsgToString(true, timeoutMessage->requestMsg,
                                      timeoutMessage->tag,
                                      timeoutMessage->eid));
    }
    lg2::error("******End logTimeOutFailure: EID={EID}*****", "EID", eid);
}
} // namespace requester
