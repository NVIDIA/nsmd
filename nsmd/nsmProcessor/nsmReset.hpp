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
#include "base.h"
#include "pci-links.h"
#include "platform-environmental.h"

#include "nsmDbusIfaceOverride/nsmResetIface.hpp"
#include "nsmSensor.hpp"

#include <xyz/openbmc_project/Control/Processor/Reset/server.hpp>

namespace nsm
{
using ResetIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::processor::Reset>;
class NsmReset : public NsmObject
{
  public:
    NsmReset(sdbusplus::bus::bus& bus, const std::string& name,
             const std::string& type, std::string& inventoryObjPath,
             std::shared_ptr<NsmDevice> device, const uint8_t deviceIndex);

  private:
    std::shared_ptr<ResetIntf> resetIntf = nullptr;
    std::shared_ptr<NsmResetAsyncIntf> resetAsyncIntf = nullptr;
};
} // namespace nsm