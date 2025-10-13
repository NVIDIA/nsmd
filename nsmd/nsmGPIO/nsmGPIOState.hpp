/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmInterface.hpp"
#include "utils.hpp"

#include <com/nvidia/GPIO/State/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefInft = object_t<Association::server::Definitions>;
using GPIOStateIntf = object_t<sdbusplus::com::nvidia::GPIO::server::State>;

class NsmGPIOState : public NsmSensor
{
  public:
    NsmGPIOState(
        sdbusplus::bus::bus& bus, const std::string& type,
        const std::string& name, std::string& inventoryObjPath,
        const std::vector<std::tuple<std::string, std::string, std::string>>&
            associations,
        std::shared_ptr<GPIOStateIntf> gpioStateIntf);
    NsmGPIOState() = delete;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::shared_ptr<GPIOStateIntf> gpioStateIntf = nullptr;
    std::unique_ptr<AssociationDefInft> associationDefIntf = nullptr;

    std::string objPath;
};

} // namespace nsm
