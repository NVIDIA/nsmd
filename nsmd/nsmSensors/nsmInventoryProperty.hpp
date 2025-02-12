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

#include "globals.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmInterface.hpp"
#include "nsmMNNVLinkTopologyIntf.hpp"

#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Dimension/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PowerLimit/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Revision/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <type_traits>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using DimensionIntf = object_t<Inventory::Decorator::server::Dimension>;
using PowerLimitIntf = object_t<Inventory::Decorator::server::PowerLimit>;
using RevisionIntf = object_t<Inventory::Decorator::server::Revision>;
using VersionIntf = object_t<Software::server::Version>;

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
    {
        if constexpr (std::is_same_v<IntfType, NsmAssetIntf>)
        {
            this->invoke(pdiMethod(buildDate), nullDate);
        }
    }
};

template <>
inline void
    NsmInventoryProperty<NsmAssetIntf>::handleResponse(const Response& data)
{
    switch (property)
    {
        case BOARD_PART_NUMBER:
            invoke(pdiMethod(partNumber),
                   std::string((char*)data.data(), data.size()));
            break;
        case SERIAL_NUMBER:
            invoke(pdiMethod(serialNumber),
                   std::string((char*)data.data(), data.size()));
            break;
        case MARKETING_NAME:
            invoke(pdiMethod(model),
                   std::string((char*)data.data(), data.size()));
            break;
        case DEVICE_PART_NUMBER:
            invoke(pdiMethod(partNumber),
                   std::string((char*)data.data(), data.size()));
            break;
        case BUILD_DATE:
        {
            std::string dateValue((char*)data.data(), data.size());
            if (dateValue == "0")
                invoke(pdiMethod(buildDate), nullDate);
            else
                invoke(pdiMethod(buildDate), dateValue);
            break;
        }
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
            invoke(pdiMethod(depth), decode_inventory_information_as_uint32(
                                         data.data(), data.size()));
            break;
        case PRODUCT_HEIGHT:
            invoke(pdiMethod(height), decode_inventory_information_as_uint32(
                                          data.data(), data.size()));
            break;
        case PRODUCT_WIDTH:
            invoke(pdiMethod(width), decode_inventory_information_as_uint32(
                                         data.data(), data.size()));
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
            invoke(pdiMethod(minPowerWatts),
                   decode_inventory_information_as_uint32(data.data(),
                                                          data.size()) /
                       1000);
            break;
        case MAXIMUM_DEVICE_POWER_LIMIT:
            invoke(pdiMethod(maxPowerWatts),
                   decode_inventory_information_as_uint32(data.data(),
                                                          data.size()) /
                       1000);
            break;
        default:
            throw std::runtime_error("Not implemented PDI");
            break;
    }
}

template <>
inline void
    NsmInventoryProperty<VersionIntf>::handleResponse(const Response& data)
{
    switch (property)
    {
        case PCIERETIMER_0_EEPROM_VERSION:
        case PCIERETIMER_1_EEPROM_VERSION:
        case PCIERETIMER_2_EEPROM_VERSION:
        case PCIERETIMER_3_EEPROM_VERSION:
        case PCIERETIMER_4_EEPROM_VERSION:
        case PCIERETIMER_5_EEPROM_VERSION:
        case PCIERETIMER_6_EEPROM_VERSION:
        case PCIERETIMER_7_EEPROM_VERSION:
        {
            std::stringstream iss;
            iss << int(data[0]) << '.' << int(data[2]);
            iss << '.';
            iss << int(((data[4] << 8) | data[6]));
            invoke(pdiMethod(version), iss.str());
        }
        break;
        default:
            throw std::runtime_error("Not implemented PDI");
            break;
    }
}

template <>
inline void
    NsmInventoryProperty<RevisionIntf>::handleResponse(const Response& data)
{
    switch (property)
    {
        case INFO_ROM_VERSION:
            invoke(pdiMethod(version),
                   std::string((char*)data.data(), data.size()));
            break;
        default:
            throw std::runtime_error("Not implemented PDI");
            break;
    }
}

template <>
inline void NsmInventoryProperty<NsmMNNVLinkTopologyIntf>::handleResponse(
    const Response& data)
{
    switch (property)
    {
        case GPU_IBGUID:
            invoke(pdiMethod(ibguid),
                   utils::convertHexToString(data, data.size()));
            break;
        case CHASSIS_SERIAL_NUMBER:
        {
            std::string chassisSerialNumber("");
            try
            {
                chassisSerialNumber = std::string((char*)data.data(),
                                                  data.size());
            }
            catch (const std::exception& e)
            {
                chassisSerialNumber = utils::convertHexToString(data,
                                                                data.size());
            }
            invoke(pdiMethod(chassisSerialNumber), chassisSerialNumber);
            break;
        }
        case TRAY_SLOT_NUMBER:
            invoke(pdiMethod(traySlotNumber),
                   decode_inventory_information_as_uint32(data.data(),
                                                          data.size()));
            break;
        case TRAY_SLOT_INDEX:
            invoke(pdiMethod(traySlotIndex),
                   decode_inventory_information_as_uint32(data.data(),
                                                          data.size()));
            break;
        case GPU_HOST_ID:
        {
            auto hostID = decode_inventory_information_as_uint32(data.data(),
                                                                 data.size());
            // 1-based
            invoke(pdiMethod(hostID), hostID + 1);
            break;
        }
        case GPU_MODULE_ID:
        {
            auto moduleID = decode_inventory_information_as_uint32(data.data(),
                                                                   data.size());
            // 1-based
            invoke(pdiMethod(moduleID), moduleID + 1);
            break;
        }
        case GPU_NVLINK_PEER_TYPE:
        {
            auto peerType = decode_inventory_information_as_uint32(data.data(),
                                                                   data.size());
            std::string peerTypeStr("");
            if (peerType == NSM_PEER_TYPE_DIRECT)
            {
                peerTypeStr = std::string("Direct");
            }
            else
            {
                peerTypeStr = std::string("Bridge");
            }
            invoke(pdiMethod(peerType), peerTypeStr);
            break;
        }
        default:
            throw std::runtime_error("Not implemented PDI");
            break;
    }
}

} // namespace nsm
