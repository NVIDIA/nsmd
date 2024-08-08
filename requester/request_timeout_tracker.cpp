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

namespace nsm
{
// Define static member
std::unordered_map<eid_t, std::shared_ptr<DeviceRequestTimeOutTracker>>
    DeviceRequestTimeOutTracker::instances;
DeviceRequestTimeOutTracker::DeviceRequestTimeOutTracker(eid_t eid) : eid(eid)
{}

DeviceRequestTimeOutTracker& DeviceRequestTimeOutTracker::getInstance(eid_t eid)
{
    if (instances.find(eid) == instances.end())
    {
        instances[eid] = std::shared_ptr<DeviceRequestTimeOutTracker>(
            new DeviceRequestTimeOutTracker(eid));
    }
    return *instances[eid];
}
void DeviceRequestTimeOutTracker::logFailuresForAllEids()
{
    for (const auto& pair : instances)
    {
        pair.second->logTimeOutFailure();
    }
}

bool DeviceRequestTimeOutTracker::isEmpty()
{
    return messages.empty();
}

bool DeviceRequestTimeOutTracker::isFull()
{
    return messages.size() == MAXSIZE;
}
// push on a full queue, will remove the oldest message
void DeviceRequestTimeOutTracker::push(std::string nsm_request)
{
    if (isFull())
    {
        messages.pop_front();
    }
    messages.push_back(nsm_request);
}

void DeviceRequestTimeOutTracker::pop()
{
    if (isEmpty())
    {
        return;
    }
    messages.pop_front();
}

std::string DeviceRequestTimeOutTracker::front()
{
    std::string nsm_request;
    if (isEmpty())
    {
        return nsm_request;
    }
    nsm_request = messages.front();
    return nsm_request;
}

void DeviceRequestTimeOutTracker::handleTimeout(std::string nsm_request)
{
    if (firstTimeoutMessage.has_value())
    {
        // skip further timeout messages as first timeout request has been
        // recorded
        return;
    }
    firstTimeoutMessage = nsm_request;
}
void DeviceRequestTimeOutTracker::handleNoTimeout(std::string nsm_request)
{
    if (firstTimeoutMessage.has_value())
    {
        // device responded after a timeout, reset tracker params
        firstTimeoutMessage = std::nullopt;
        emptyQueue();
    }
    push(nsm_request);
}

void DeviceRequestTimeOutTracker::emptyQueue()
{
    messages.clear();
}

void DeviceRequestTimeOutTracker::logTimeOutFailure()
{
    lg2::error("******logTimeOutFailure: EID={EID}*****", "EID", eid);
    if (firstTimeoutMessage.has_value())
    {
        for (const auto& message : messages)
        {
            lg2::error(
                "logTimeOutFailure: EID={EID}, Last(n) NSM request msg before timeout: {REQ} ",
                "EID", eid, "REQ", message);
        }
        lg2::error(
            "logTimeOutFailure: EID={EID}, Timeout for NSM request: {REQ}",
            "EID", eid, "REQ", *firstTimeoutMessage);
    }
    lg2::error("******logTimeOutFailure: EID={EID}*****", "EID", eid);
}
} // namespace nsm
