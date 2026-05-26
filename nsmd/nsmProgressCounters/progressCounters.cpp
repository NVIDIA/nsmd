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

#include "progressCounters.hpp"

#include "globals.hpp"
#include "nsmDevice.hpp"

namespace nsm
{

#define ProgressCountersBaseClass                                              \
    ProgressCountersBase<CounterDataType, Size, MemFdBytesSize>

#define ProgressCountersBaseMacro(ReturnType)                                  \
    CountersTemplate ReturnType ProgressCountersBaseClass

// ProgressCountersBase template implementations
CountersTemplate ProgressCountersBaseClass::ProgressCountersBase(
    const std::string& path, const std::string& description,
    const CountersHeaders& countersHeaders, size_t countThreshold,
    size_t timeThreshold) :
    dumpObject(path), countThreshold(countThreshold),
    timeThreshold(timeThreshold)
{
    dumpObject.description(description);
    dumpObject.countersHeaders(countersHeaders);

    // Set the counterType property according to the type of CounterDataType
    if constexpr (std::is_same_v<CounterDataType, int8_t>)
    {
        dumpObject.counterType(CountersIntf::CounterType::INT8);
    }
    else if constexpr (std::is_same_v<CounterDataType, uint32_t>)
    {
        dumpObject.counterType(CountersIntf::CounterType::UINT32);
    }
    else
    {
        static_assert(false, "Unsupported counter type");
    }
}

ProgressCountersBaseMacro(bool)::flushIfNeeded(const uint64_t& time)
{
    if (timeThreshold > 0 && (time - lastUpdateTime) >= timeThreshold)
    {
        updateCounters(time);
        return true;
    }
    return false;
}

ProgressCountersBaseMacro(bool)::flushIfNeeded()
{
    if (countThreshold > 0 && totalCount >= countThreshold)
    {
        updateCounters();
        return true;
    }
    return false;
}

ProgressCountersBaseMacro(void)::updateCounters(const uint64_t& time)
{
#ifdef NVIDIA_PROGRESS_COUNTER
    dumpObject.updateCounters({dumpIteration, time, counters});
    dumpIteration++;
    lastUpdateTime = time;
    totalCount = 0;
    initializeCounters();
#else
    (void)time;
#endif
}

ProgressCountersBaseMacro(void)::increment(const uint32_t& counterType)
{
#ifdef NVIDIA_PROGRESS_COUNTER
    counters[counterType]++;
    totalCount++;
    flushIfNeeded();
#else
    (void)counterType;
#endif
}

// Explicit template instantiation for the type we use
template class ProgressCountersBase<uint32_t, PollingCountersSize,
                                    SENSOR_PROGRESS_COUNTERS_MEMFD_SIZE>;
template class ProgressCountersBase<int8_t, DiscoveryEventsSize,
                                    DISCOVERY_PROGRESS_COUNTERS_MEMFD_SIZE>;

ProgressCounters::ProgressCounters(eid_t eid) :
    PollingCountersBase(
        progressCountersObjectBasePath / "polling" / std::to_string(eid),
        "Polling Progress Counters for device EID=" + std::to_string(eid),
        {
            "Priority",
            "DumpCollection",
            "GPM",
            "LongRunning",
            "Static",
            "RoundRobin",
            "PriorityTimeExceeded",
            "PostPatch",
            "Event",
            "Error",
            "Timeout",
        },
        SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD,
        SENSOR_PROGRESS_COUNTERS_DUMP_TIME_THRESHOLD)
{
    initializeCounters();
}

void ProgressCounters::initializeCounters()
{
    counters.fill(0);
}

void ProgressCounters::increment(const PollingType& counterType,
                                 const uint8_t rc)
{
    increment(static_cast<ProgressCounterType>(counterType), rc);
}

void ProgressCounters::increment(const ProgressCounterType& counterType,
                                 const uint8_t rc)
{
    ProgressCounterType type;
    switch (rc)
    {
        case NSM_SW_SUCCESS:
            type = counterType;
            break;
        case NSM_SW_ERROR_TIMEOUT:
            type = ProgressCounterType::Timeout;
            break;
        default:
            type = ProgressCounterType::Error;
            break;
    }
    PollingCountersBase::increment(static_cast<uint32_t>(type));
}

DiscoveryEvents::DiscoveryEvents(eid_t eid) :
    DiscoveryEventsBase(
        progressCountersObjectBasePath / "discovery" / std::to_string(eid),
        "Discovery Progress Counters for device EID=" + std::to_string(eid),
        {
            "InterfaceAdded_Signal",
            "InterfaceRemoved_Signal",
            "Connectivity_Available", // 1: available, 0: not available
            "Online_coSetdeviceStateOnlineTask_RC",
            "Online_ping_RC",
            "Online_getQueryDeviceIdentification_RC",
            "Online_mapNsmDeviceUsingEid_Success", // 1: success, 0:
                                                   // failed
            "Online_getSupportedNvidiaMessageType_RC",
            "Online_getSupportedCommandCodes0_RC",
            "Online_getSupportedCommandCodes1_RC",
            "Online_getSupportedCommandCodes2_RC",
            "Online_getSupportedCommandCodes3_RC",
            "Online_getSupportedCommandCodes4_RC",
            "Online_getSupportedCommandCodes5_RC",
            "Online_getSupportedCommandCodes6_RC",
            "Online_getFRU_RC",
            "Offline_coSetdeviceStateOfflineTask_RC",
            "Offline_mapNsmDeviceUsingEid_Success", // 1: success, 0:
                                                    // failed
        })
{
    initializeCounters();
}

void DiscoveryEvents::initializeCounters()
{
    // Initialize all counters to -1 (not triggered/executed)
    counters.fill(-1);
}

void DiscoveryEvents::increment(const DiscoveryEventType& counterType)
{
#ifdef NVIDIA_PROGRESS_COUNTER
    switch (counterType)
    {
        case DiscoveryEventType::InterfaceAddedSignal:
        case DiscoveryEventType::InterfaceRemovedSignal:
        {
            if (counters[static_cast<uint32_t>(
                    DiscoveryEventType::InterfaceAddedSignal)] > 0 ||
                counters[static_cast<uint32_t>(
                    DiscoveryEventType::InterfaceRemovedSignal)] > 0 ||
                counters[static_cast<uint32_t>(
                    DiscoveryEventType::ConnectivityAvailable)] != -1)
            {
                // each event can be triggered only once, so flush the counters
                updateCounters();
            }
            auto type = static_cast<uint32_t>(counterType);
            // Signal counters: increment from 0
            if (counters[type] == -1)
            {
                counters[type] = 0;
            }
            DiscoveryEventsBase::increment(type);
        }
        break;
        default:
            throw std::runtime_error("Invalid counter type");
    }
#else
    (void)counterType;
#endif
}

void DiscoveryEvents::setValue(const DiscoveryEventType& counterType,
                               const uint8_t value)
{
#ifdef NVIDIA_PROGRESS_COUNTER

    if ((counterType == DiscoveryEventType::ConnectivityAvailable) &&
        (counters[static_cast<uint32_t>(
             DiscoveryEventType::InterfaceAddedSignal)] > 0 ||
         counters[static_cast<uint32_t>(
             DiscoveryEventType::InterfaceRemovedSignal)] > 0))
    {
        // each event can be triggered only once, so flush the counters
        updateCounters();
    }
    auto type = static_cast<uint32_t>(counterType);
    // Check if the counter has already been set and flush if needed
    if (counters[type] != -1)
    {
        updateCounters();
    }
    counters[type] = value;
#else
    (void)counterType;
    (void)value;
#endif
}

} // namespace nsm
