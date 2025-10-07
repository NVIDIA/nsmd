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

constexpr auto CountersCount =
    static_cast<uint32_t>(ProgressCounterType::EnumCount);
using CountersArray = std::array<uint32_t, CountersCount>;

struct __attribute__((packed)) CounterDataRow
{
    uint32_t key;
    uint64_t timestamp;
    CountersArray counters;
};
} // namespace nsm
