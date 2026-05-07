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

#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <exception>
#include <string>

namespace nsm
{

/// @brief Fully-qualified StateType enum values exposed by
/// com.nvidia.PreBootDiag.App.interface.yaml. Only the NSM-loop StateType
/// are needed here; the three gate states are signaled by external
/// producers (postcode-manager, recovery flow).
namespace preBootDiagState
{
inline constexpr auto systemConfigRequested =
    "com.nvidia.PreBootDiag.App.StateType.SystemConfigRequested";
inline constexpr auto tidConfigRequested =
    "com.nvidia.PreBootDiag.App.StateType.TIDConfigRequested";
inline constexpr auto heartbeatReceived =
    "com.nvidia.PreBootDiag.App.StateType.HeartbeatReceived";
inline constexpr auto resultReceived =
    "com.nvidia.PreBootDiag.App.StateType.ResultReceived";
inline constexpr auto sessionEnded =
    "com.nvidia.PreBootDiag.App.StateType.SessionEnded";
} // namespace preBootDiagState

/// @brief Push an App.Notify notification to prebootdiag. Fire-and-forget,
/// async: the call is queued via async_method_call and returns immediately
/// without blocking the event loop. The completion callback only logs
/// failures — there is no recovery (prebootdiag re-derives state from the
/// next event).
inline void postStateUpdate(const std::string& state,
                            const std::string& payload)
{
    constexpr auto service = "com.nvidia.PreBootDiag";
    constexpr auto path = "/com/nvidia/prebootdiag";
    constexpr auto iface = "com.nvidia.PreBootDiag.App";

    try
    {
        auto& conn = utils::DBusHandler::getAsioConnection();
        conn->async_method_call([state](const boost::system::error_code& ec) {
            if (ec)
            {
                lg2::warning(
                    "PreBootDiag: App.Notify failed state={STATE} err={ERR}",
                    "STATE", state, "ERR", ec.message());
            }
        }, service, path, iface, "Notify", state, payload);
    }
    catch (const std::exception& e)
    {
        lg2::warning(
            "PreBootDiag: App.Notify dispatch failed state={STATE} err={ERR}",
            "STATE", state, "ERR", e.what());
    }
}

} // namespace nsm
