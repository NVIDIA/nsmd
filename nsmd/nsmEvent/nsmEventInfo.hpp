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

#include "nsmEvent.hpp"

namespace nsm
{

struct NsmEventInfo
{
    std::string uuid;
    std::string originOfCondition;
    std::string messageId;
    Level severity;
    std::string loggingNamespace;
    std::string resolution;
    std::vector<std::string> errorId;
    std::string impactedComponent;
    std::vector<std::string> messageArgs;
    bool logging;
};

inline std::string getEventErrorId(const NsmEventInfo& info,
                                   const std::string& errorIdKey)
{
    if (info.errorId.size() % 2 == 0)
    {
        auto it = std::find(info.errorId.begin(), info.errorId.end(),
                            errorIdKey);
        if (it != info.errorId.end() && (it + 1) != info.errorId.end())
        {
            std::string errorIdStr = *(it + 1);
            return errorIdStr;
        }
        lg2::debug(
            "NSM_Event getEventErrorId : no errorId found for errorIdKey = {EID}, uuid = {UUID}",
            "EID", errorIdKey, "UUID", info.uuid);
        return "";
    }
    lg2::error(
        "NSM_Event getEventErrorId : Invalid ErrorId Map Size when getting errorIdKey = {EID}, uuid = {UUID}",
        "EID", errorIdKey, "UUID", info.uuid);
    return "";
}
}; // namespace nsm
