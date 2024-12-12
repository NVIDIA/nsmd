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

#include "nsmReady.hpp"

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmZone.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
static requester::Coroutine
    createNsmReadySensor(SensorManager& /*manager*/,
                         const std::string& /*interface*/,
                         const std::string& /*objPath*/)
{
    // dbus timeout seen
    nsm::SensorManagerImpl::isEMReady();
    nsm::SensorManagerImpl::isMCTPReady();

    // marking here directly was working
    //  markEMReady();
    lg2::info("createNsmReadySensor completed");
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmReadySensor, "xyz.openbmc_project.Configuration.NSM_Poll_Ready")

} // namespace nsm
