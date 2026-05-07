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

#include "nsmDbusIfaceOverride/nsmPreBootDiagIntf.hpp"
#include "nsmObject.hpp"

#include <memory>
#include <string>

namespace nsm
{

/// Hosts com.nvidia.PreBootDiag.Config.System and
/// com.nvidia.PreBootDiag.Config.TID interfaces at
/// /com/nvidia/prebootdiag/<eid>, plus a com.nvidia.Async.Set dispatcher (added
/// on first getDispatcher() call) that routes Async.Set invocations to the
/// registered handlers. One instance per CPU; obtained via the static
/// `getOrCreate()` factory, which keeps an internal EID-keyed registry so the
/// instance survives across the four Type-4 event handlers without leaking
/// pre-boot-diagnostic state into the generic NsmDevice class.
class NsmPreBootDiag : public NsmObject
{
  public:
    NsmPreBootDiag(sdbusplus::bus::bus& bus, const std::string& name,
                   const std::string& type, const std::string& objPath,
                   std::shared_ptr<NsmDevice> device);

    /// Lazy-init accessor: returns the per-CPU instance for `device`,
    /// creating one (with `/com/nvidia/prebootdiag/<eid>` path and a fresh
    /// Async.Set dispatcher) on first call.
    static std::shared_ptr<NsmPreBootDiag>
        getOrCreate(std::shared_ptr<NsmDevice> device);

  private:
    std::shared_ptr<NsmPreBootDiagConfigSystemIntf> systemConfigIntf;
    std::shared_ptr<NsmPreBootDiagConfigTIDIntf> tidConfigIntf;
};

} // namespace nsm
