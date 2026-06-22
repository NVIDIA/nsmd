/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "nsmCommon/nsmPciePortIntf.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmPciePortIntf::NsmPciePortIntf(sdbusplus::bus_t& bus, const std::string& name,
                                 const std::string& type,
                                 std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    lg2::info("NsmPciePortIntf: create sensor:{NAME}", "NAME", name.c_str());
    pciePortIntf = std::make_shared<PciePortIntf>(bus,
                                                  inventoryObjPath.c_str());
}
} // namespace nsm
