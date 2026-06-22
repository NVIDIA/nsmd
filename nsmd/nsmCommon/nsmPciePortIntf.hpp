/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "nsmObject.hpp"

#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>

namespace nsm
{
using PciePortIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Port>;

class NsmPciePortIntf : public NsmObject
{
  public:
    NsmPciePortIntf(sdbusplus::bus_t& bus, const std::string& name,
                    const std::string& type, std::string& inventoryObjPath);

  private:
    std::shared_ptr<PciePortIntf> pciePortIntf = nullptr;
};
} // namespace nsm
