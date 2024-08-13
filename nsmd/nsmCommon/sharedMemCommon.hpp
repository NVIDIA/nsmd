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
/**
 * @brief Verify allowed device type and its instance number
 *
 * @param inventoryObjPath Dbus path
 * @param ifaceName interface name
 * @param propName Dbus property name
 * @param smbusData smbus data for the property
 * @param propValue dbus property value
 */
void updateSharedMemoryOnSuccess(
    const std::string& inventoryObjPath, const std::string& ifaceName,
    const std::string& propName, std::vector<uint8_t>& smbusData,
    nv::sensor_aggregation::DbusVariantType propValue);
} // namespace nsm_shmem_utils
