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

#include <xyz/openbmc_project/Inventory/Decorator/PortInfo/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortWidth/server.hpp>

namespace nsm
{
using sdbusplus::server::object_t;
using PortInfoIntf = object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::PortInfo>;
using PortWidthIntf = object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::PortWidth>;

class NsmPortInfoIntf : public PortInfoIntf, public PortWidthIntf
{
  public:
    NsmPortInfoIntf() = delete;
    NsmPortInfoIntf(sdbusplus::bus::bus& bus, const char* path) :
        PortInfoIntf(bus, path), PortWidthIntf(bus, path)
    {}
};

} // namespace nsm
