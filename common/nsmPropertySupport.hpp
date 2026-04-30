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
#include "libnsm/platform-environmental.h"

#include <string_view>
#include <unordered_set>

namespace nsm
{

constexpr std::string_view propertyNotSupported = "NOT_SUPPORTED";

// SMA devices do not expose PartNumber, SerialNumber, or Model via NSM. Return
// properties that should be marked NOT_SUPPORTED on D-Bus and excluded from
// polling, based on deviceType and deviceRole.
inline std::unordered_set<nsm_inventory_property_identifiers>
    getUnsupportedAssetProperties(uint8_t deviceType, uint8_t deviceRole)
{
    if (deviceType == NSM_DEV_ID_MCTP_BRIDGE &&
        (deviceRole == NSM_MCTP_BRIDGE_DEV_ROLE_SXM_SMA ||
         deviceRole == NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA ||
         deviceRole == NSM_MCTP_BRIDGE_DEV_ROLE_QM_SMA))
    {
        return {FRU_PART_NUMBER, SERIAL_NUMBER, MARKETING_NAME};
    }
    return {};
}

} // namespace nsm
