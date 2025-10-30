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

#include <array>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

namespace nsm
{

/** @class InstanceId
 *  @brief Implementation of NSM instance id
 *  @note This implementation supports max 1 active request per EID.
 *        Instance IDs are allocated cyclically (0-31) using a global counter.
 */
class InstanceIdDb
{
    static constexpr uint8_t LOCKED_BIT = 0b10000000;
    static constexpr uint8_t INSTANCE_MAX = NSM_INSTANCE_MAX;
    static constexpr uint8_t CYCLIC_COUNTER = NSM_INSTANCE_MAX + 1;

  public:
    InstanceIdDb()
    {
        // Initialize all EIDs as free with counter at INSTANCE_MAX (31)
        // so the first allocation will be (31+1)%32 = 0
        for (auto& eid : lockedEids)
        {
            eid = INSTANCE_MAX; // Free state, LOCKED_BIT not set
        }
    }

    /** @brief Allocate an instance ID for the given terminus
     *  @param[in] eid - the Endpoint ID the instance ID is associated with
     *  @return - instance id (0-31)
     *  @throw std::runtime_error if EID already has an active request
     */
    uint8_t next(uint8_t eid)
    {
        uint8_t current = lockedEids[eid];
        // Check if EID already has an active request
        if (current & LOCKED_BIT)
        {
            throw std::runtime_error(
                std::format("No free instance ids for EID {}", eid));
        }

        // Allocate next instance ID (0-31, cyclic)
        current = ((current & INSTANCE_MAX) + 1) % CYCLIC_COUNTER;
        lockedEids[eid] = LOCKED_BIT | current;

        return current;
    }

    /** @brief Mark an instance id as unused
     *  @param[in] eid - the terminus ID the instance ID is associated with
     *  @param[in] instanceId - NSM instance id to be freed
     *  @throw std::runtime_error if ID was not previously allocated for this
     * EID
     */
    void free(uint8_t eid, uint8_t instanceId)
    {
        const uint8_t current = lockedEids[eid];
        if (!(current & LOCKED_BIT) || (current & INSTANCE_MAX) != instanceId)
        {
            throw std::runtime_error(std::format(
                "Instance ID {} for EID {} was not previously allocated",
                instanceId, eid));
        }
        // Clear the locked bit to indicate that the instance ID is free
        lockedEids[eid] = current & ~LOCKED_BIT;
    }

  private:
    // Bit 7 (LOCKED_BIT): 1 = allocated, 0 = free
    // Bits 0-4: instance ID value (0-31) for cyclic counter
    std::array<uint8_t, 256> lockedEids;
};

} // namespace nsm
