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

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nsm
{

/**
 * @brief Progress counter types for tracking sensor update operations
 *
 * These counters track different types of sensor polling operations and their
 * outcomes. Counters are incremented after each sensor update attempt and are
 * periodically dumped to a memory file descriptor for monitoring and debugging.
 */
enum class ProgressCounterType
{
    /**
     * @brief Priority sensor polling counter
     *
     * Incremented when: A priority sensor is successfully updated during the
     * priority polling phase (every 150ms). Priority sensors are critical
     * sensors that need frequent updates.
     *
     * Location: sensorManager.cpp::pollPrioritySensors()
     */
    Priority,

    /**
     * @brief Dump / CPER record collection polling counter
     *
     * Incremented when: a dump / CPER record collection sensor is polled
     * during the priority polling phase (shares the 150ms budget with
     * priority sensors). Mirrors PollingType::DumpCollection so that
     * ProgressCounters::increment(PollingType) can map by static_cast.
     *
     * Location: sensorManager.cpp dump-collection polling path
     */
    DumpCollection,

    /**
     * @brief GPU Performance Monitoring sensor counter
     *
     * Incremented when: A GPU Performance Monitoring (GPM) sensor is
     * successfully updated. These sensors track GPU-specific performance
     * metrics like NVDEC and NVJPG utilization (polled every 1000ms).
     *
     * Location: nsmGpmOemFactory.cpp when creating GPM sensors
     */
    GpuPerformanceMonitoring,

    /**
     * @brief Long running sensor operation counter
     *
     * Incremented when: A long-running sensor operation completes. Long-running
     * operations are those that may take extended time and potentially return
     * an event as a second response (e.g., throttle duration sensors).
     *
     * Location: sensorManager.cpp::updateLongRunningSensor()
     */
    LongRunning,

    /**
     * @brief Static sensor polling counter
     *
     * Incremented when: A static sensor is updated. Static sensors are polled
     * only once and removed from the queue upon successful update. These are
     * typically sensors with values that don't change during runtime.
     *
     * Location: sensorManager.cpp::pollNonPrioritySensors() when pollingType ==
     * Static
     */
    Static,

    /**
     * @brief Round-robin sensor polling counter
     *
     * Incremented when: A non-priority sensor is updated during round-robin
     * polling. These sensors are polled in a circular queue fashion when time
     * permits after priority sensors are updated.
     *
     * Location: sensorManager.cpp::pollNonPrioritySensors() when pollingType ==
     * RoundRobin
     */
    RoundRobin,

    /**
     * @brief Priority polling time exceeded counter
     *
     * Incremented when: Priority sensor polling takes longer than the allocated
     * time window (SENSOR_POLLING_TIME, typically 150ms). This indicates that
     * priority polling is taking too long and may affect system responsiveness.
     *
     * Location: sensorManager.cpp::pollPrioritySensors() when (t1 - t0) >
     * pollingTimeInUsec
     */
    PriorityTimeExceeded,

    /**
     * @brief Post-patch I/O operation counter
     *
     * Incremented when: A post-patch I/O operation is performed on the device.
     * This tracks operations that occur after device firmware updates or
     * patches to verify device state and functionality.
     *
     * Location: nsmDevice.cpp::postPatchIO()
     */
    PostPatch,

    /**
     * @brief Event handling counter
     *
     * Incremented when: An NSM event is received and processed by the event
     * dispatcher. Events are asynchronous notifications from devices (e.g.,
     * long-running operation completion, state changes).
     *
     * Location: nsmEvent.cpp::DelegatingEventHandler::delegate()
     */
    Event,

    /**
     * @brief Error counter
     *
     * Incremented when: Any sensor update or operation fails with an error code
     * other than NSM_SUCCESS or NSM_SW_ERROR_TIMEOUT. This is a catch-all for
     * various error conditions during polling operations.
     *
     * Location: progressCounters.cpp::increment() when rc != NSM_SUCCESS
     * and rc != NSM_SW_ERROR_TIMEOUT
     */
    Error,

    /**
     * @brief Timeout counter
     *
     * Incremented when: A sensor update or operation times out
     * (NSM_SW_ERROR_TIMEOUT). This indicates that the device did not respond
     * within the expected time window.
     *
     * Location: progressCounters.cpp::increment() when rc ==
     * NSM_SW_ERROR_TIMEOUT
     */
    Timeout,

    /**
     * @brief Enum count used only for calculating the number of counters in the
     * array.
     * @note Needs to be last in the enum.
     */
    EnumCount,
};

/**
 * @brief Progress counter types for tracking device discovery operations
 *
 * These counters track different types of device discovery operations and their
 * outcomes. Counters are incremented during device discovery and are
 * periodically dumped to a memory file descriptor for monitoring and debugging.
 */
enum class DiscoveryEventType
{
    /**
     * @brief MCTP interface added signal handler
     *
     * Incremented when: MCTP interface is added
     * Values: -1 (not triggered), 0+ (count of signals)
     */
    InterfaceAddedSignal,

    /**
     * @brief MCTP interface removed signal handler
     *
     * Incremented when: MCTP interface is removed
     * Values: -1 (not triggered), 0+ (count of signals)
     */
    InterfaceRemovedSignal,

    /**
     * @brief Connectivity available property change
     *
     * Incremented when: Connectivity property changes
     * Values: -1 (not set), 0 (not available), 1 (available)
     */
    ConnectivityAvailable,

    /**
     * @brief Online task: coSetdeviceStateOnlineTask return code
     *
     * Incremented when: Device online state task completes
     * Values: -1 (not executed), RC (return code)
     */
    SetDeviceStateOnline,

    /**
     * @brief Online task: ping return code
     *
     * Incremented when: Ping command is executed during online discovery
     * Values: -1 (not executed), RC (return code)
     */
    Ping,

    /**
     * @brief Online task: query device identification return code
     *
     * Incremented when: Query Device Identification during online discovery
     * Values: -1 (not executed), RC (return code)
     */
    QueryDeviceIdentification,

    /**
     * @brief Online task: map NSM device using EID success
     *
     * Incremented when: Device mapping is attempted during online discovery
     * Values: -1 (not attempted), 0 (failed), 1 (success)
     */
    OnlineMapNsmDeviceUsingEid,

    /**
     * @brief Online task: get supported NVIDIA message type return code
     *
     * Incremented when: Supported message types queried during online discovery
     * Values: -1 (not executed), RC (return code)
     */
    GetSupportedNvidiaMessageType,

    /**
     * @brief Online task: get supported command codes for message type 0
     *
     * Incremented when: Command codes queried for message type 0
     * Values: -1 (not executed), RC (return code)
     */
    GetSupportedCommandCodes0,

    /**
     * @brief Online task: get supported command codes for message type 1
     *
     * Incremented when: Command codes queried for message type 1
     * Values: -1 (not executed), RC (return code)
     */
    GetSupportedCommandCodes1,

    /**
     * @brief Online task: get supported command codes for message type 2
     *
     * Incremented when: Command codes queried for message type 2
     * Values: -1 (not executed), RC (return code)
     */
    GetSupportedCommandCodes2,

    /**
     * @brief Online task: get supported command codes for message type 3
     *
     * Incremented when: Command codes queried for message type 3
     * Values: -1 (not executed), RC (return code)
     */
    GetSupportedCommandCodes3,

    /**
     * @brief Online task: get supported command codes for message type 4
     *
     * Incremented when: Command codes queried for message type 4
     * Values: -1 (not executed), RC (return code)
     */
    GetSupportedCommandCodes4,

    /**
     * @brief Online task: get supported command codes for message type 5
     *
     * Incremented when: Command codes queried for message type 5
     * Values: -1 (not executed), RC (return code)
     */
    GetSupportedCommandCodes5,

    /**
     * @brief Online task: get supported command codes for message type 6
     *
     * Incremented when: Command codes queried for message type 6
     * Values: -1 (not executed), RC (return code)
     */
    GetSupportedCommandCodes6,

    /**
     * @brief Online task: get FRU device identification return code
     *
     * Incremented when: FRU information is retrieved during online discovery
     * Values: -1 (not executed), RC (return code)
     */
    GetFru,

    /**
     * @brief Offline task: coSetdeviceStateOfflineTask return code
     *
     * Incremented when: Device offline state task completes
     * Values: -1 (not executed), RC (return code)
     */
    SetDeviceStateOffline,

    /**
     * @brief Offline task: map NSM device using EID success
     *
     * Incremented when: Device mapping is attempted during offline discovery
     * Values: -1 (not attempted), 0 (failed), 1 (success)
     */
    OfflineMapNsmDeviceUsingEid,

    /**
     * @brief Enum count used only for calculating the number of counters in the
     * array.
     * @note Needs to be last in the enum.
     */
    EnumCount
};

constexpr auto PollingCountersSize =
    static_cast<uint32_t>(ProgressCounterType::EnumCount);

constexpr auto DiscoveryEventsSize =
    static_cast<uint32_t>(DiscoveryEventType::EnumCount);

template <typename CounterDataType, std::size_t Size>
using CountersArray = std::array<CounterDataType, Size>;

using CountersHeaders = std::vector<std::string>;

struct __attribute__((packed)) CountersDataHeader
{
    uint32_t key;
    uint64_t timestamp;
};

template <typename CounterDataType, std::size_t Size>
struct __attribute__((packed)) CountersDataRow : CountersDataHeader
{
    CountersArray<CounterDataType, Size> counters;
};
} // namespace nsm
