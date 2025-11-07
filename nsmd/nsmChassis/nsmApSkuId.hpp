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

#include "libnsm/firmware-utils.h"
#include "libnsm/platform-environmental.h"

#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/Progress/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/SKU/server.hpp>

#include <memory>

namespace nsm
{
using namespace sdbusplus::common::xyz::openbmc_project::common;
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using ApSkuIdIntf = object_t<Inventory::Decorator::server::SKU>;
using ProgressIntf = object_t<Common::server::Progress>;

class ApSkuIdConfiguration : public ApSkuIdIntf
{
  public:
    ApSkuIdConfiguration(sdbusplus::bus::bus& bus, const std::string& objPath,
                         const uuid_t& uuidIn,
                         std::shared_ptr<ProgressIntf> progressIntfIn,
                         NsmSensor& nsmSensor);

    virtual ~ApSkuIdConfiguration() = default;

  private:
    uuid_t uuid;
    std::shared_ptr<ProgressIntf> progressIntf = nullptr;
    NsmSensor& nsmSensor;
};

class NsmApSkuIdObject : public NsmSensor
{
  private:
    std::string getPath(const std::string& chassisName)
    {
        using namespace std::string_literals;
        return std::string(chassisInventoryBasePath) + "/" + chassisName;
    }

  public:
    NsmApSkuIdObject(sdbusplus::bus::bus& bus, const std::string& name,
                     const std::string& type, const uuid_t& uuid,
                     std::shared_ptr<ProgressIntf> progressIntfIn,
                     uint16_t classificationIn, uint16_t identifierIn,
                     uint8_t indexIn);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override;

    // Method to set SKU property value directly
    void setSKU(const std::string& value)
    {
        if (apSkuIdObject)
        {
            apSkuIdObject->sku(value);
        }
    }

  private:
    std::string objectPath;
    std::unique_ptr<ApSkuIdConfiguration> apSkuIdObject;
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
};

} // namespace nsm
