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

#include "libnsm/platform-environmental.h"

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Dimension/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PowerLimit/server.hpp>

#include <type_traits>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using DimensionIntf = object_t<Inventory::Decorator::server::Dimension>;
using PowerLimitIntf = object_t<Inventory::Decorator::server::PowerLimit>;

class NsmInventoryPropertyBase : public NsmSensor
{
  public:
    NsmInventoryPropertyBase(const NsmObject& provider,
                             nsm_inventory_property_identifiers property);
    NsmInventoryPropertyBase() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  protected:
    virtual void handleResponse(const Response& data) = 0;
    nsm_inventory_property_identifiers property;
};

template <typename IntfType>
class NsmInventoryProperty :
    public NsmInventoryPropertyBase,
    public NsmInterfaceContainer<IntfType>
{
  protected:
    void handleResponse(const Response& data) override;

  public:
    NsmInventoryProperty() = delete;
    NsmInventoryProperty(const NsmInterfaceProvider<IntfType>& provider,
                         nsm_inventory_property_identifiers property) :
        NsmInventoryPropertyBase(provider, property),
        NsmInterfaceContainer<IntfType>(provider)
    {}
};

template <>
inline void
    NsmInventoryProperty<AssetIntf>::handleResponse(const Response& data)
{
    switch (property)
    {
        case BOARD_PART_NUMBER:
            pdi().partNumber(std::string((char*)data.data(), data.size()));
            break;
        case SERIAL_NUMBER:
            pdi().serialNumber(std::string((char*)data.data(), data.size()));
            break;
        case MARKETING_NAME:
            pdi().model(std::string((char*)data.data(), data.size()));
            break;
        case DEVICE_PART_NUMBER:
            pdi().partNumber(std::string((char*)data.data(), data.size()));
            break;
        case BUILD_DATE:
            pdi().buildDate(std::string((char*)data.data(), data.size()));
            break;
        default:
            throw std::runtime_error("Not implemented PDI");
            break;
    }
}

template <>
inline void
    NsmInventoryProperty<DimensionIntf>::handleResponse(const Response& data)
{
    switch (property)
    {
        case PRODUCT_LENGTH:
            pdi().depth(decode_inventory_information_as_uint32(data.data(),
                                                               data.size()));
            break;
        case PRODUCT_HEIGHT:
            pdi().height(decode_inventory_information_as_uint32(data.data(),
                                                                data.size()));
            break;
        case PRODUCT_WIDTH:
            pdi().width(decode_inventory_information_as_uint32(data.data(),
                                                               data.size()));
            break;
        default:
            throw std::runtime_error("Not implemented PDI");
            break;
    }
}

template <>
inline void
    NsmInventoryProperty<PowerLimitIntf>::handleResponse(const Response& data)
{
    switch (property)
    {
        case MINIMUM_DEVICE_POWER_LIMIT:
            pdi().minPowerWatts(decode_inventory_information_as_uint32(
                data.data(), data.size()));
            break;
        case MAXIMUM_DEVICE_POWER_LIMIT:
            pdi().maxPowerWatts(decode_inventory_information_as_uint32(
                data.data(), data.size()));
            break;
        default:
            throw std::runtime_error("Not implemented PDI");
            break;
    }
}

} // namespace nsm
