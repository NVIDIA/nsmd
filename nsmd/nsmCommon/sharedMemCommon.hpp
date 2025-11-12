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
#include <tal.hpp>

namespace nsm_shmem_utils
{

class SharedMemoryManager
{
  public:
    // Delete all constructors - this is a pure static class
    SharedMemoryManager() = delete;
    SharedMemoryManager(const SharedMemoryManager&) = delete;
    SharedMemoryManager& operator=(const SharedMemoryManager&) = delete;
    ~SharedMemoryManager() = delete;

    /**
     * @brief Update shared memory on success
     *
     * @param inventoryObjPath Dbus path
     * @param ifaceName interface name
     * @param propName Dbus property name
     * @param smbusData smbus data for the property
     * @param propValue dbus property value
     * @param associatedEntityPath associated entity path
     */
    static void cacheTALData(const std::string& inventoryObjPath,
                             const std::string& ifaceName,
                             const std::string& propName,
                             std::vector<uint8_t>& smbusData,
                             nv::sensor_aggregation::DbusVariantType propValue,
                             const std::string& associatedEntityPath = "",
                             uint8_t rc = 0);

    /*
     * @brief update telemtry on shmem/smbus
     */
    static void updateAggregateTelemetryOnTAL();

    /*
      @brief  initialize tal api
    */
    static void initTALNamespace();

  private:
    // Static member variables (previously global)
    static std::vector<tal::TelemetryData> telemetryData;
};
} // namespace nsm_shmem_utils
