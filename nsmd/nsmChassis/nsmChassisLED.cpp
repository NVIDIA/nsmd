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

#include "nsmChassisLED.hpp"

#include "network-ports.h"

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"

#include <unordered_map>

#define NSM_CHASSIS_LED_INTERFACE                                              \
    "xyz.openbmc_project.Configuration.NSM_ChassisLED"

namespace nsm
{

NsmNvlinkLedIntf::NsmNvlinkLedIntf(sdbusplus::bus::bus& bus, std::string& name,
                                   std::string& type,
                                   std::string& inventoryObjPath) :
    NsmObject(name, type),
    inventoryObjPath(inventoryObjPath)
{
    nvlinkledIntf = std::make_unique<NvlinkLedIntf>(bus,
                                                    inventoryObjPath.c_str());
}

requester::Coroutine NsmNvlinkLedIntf::update(SensorManager& manager, eid_t eid)
{
    Request readNvLinkLed(sizeof(nsm_msg_hdr) +
                          sizeof(struct nsm_get_nvlink_agg_led_status_req));
    auto readNvLinkLedMsg =
        reinterpret_cast<struct nsm_msg*>(readNvLinkLed.data());

    auto rc = encode_get_nvlink_agg_led_status_req(0, readNvLinkLedMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmGetNvLinkLed encode_get_nvlink_agg_led_status_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> readNvLinkLedResponseMsg;
    size_t readNvLinkLedResponseLen = 0;

    rc = co_await manager.SendRecvNsmMsg(
        eid, readNvLinkLed, readNvLinkLedResponseMsg, readNvLinkLedResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGetNvLinkLed SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    nsm_nvlink_led_state ledAggredateState = NSM_NVLINK_LED_ERROR;

    uint8_t cc;
    uint16_t reason_code;

    rc = decode_get_nvlink_agg_led_status_resp(
        readNvLinkLedResponseMsg.get(), readNvLinkLedResponseLen, &cc,
        &reason_code, &ledAggredateState);

    if (rc == NSM_SW_SUCCESS)
    {
        switch (ledAggredateState)
        {
            case NSM_NVLINK_LED_GREEN:
                nvlinkledIntf->ledState(
                    sdbusplus::common::com::nvidia::nv_link::NVLinkLED::
                        LedState::Steady_Green);
                break;
            case NSM_NVLINK_LED_GREEN_BLINK:
                nvlinkledIntf->ledState(
                    sdbusplus::common::com::nvidia::nv_link::NVLinkLED::
                        LedState::Blinking_Green);
                break;
            case NSM_NVLINK_LED_AMBER:
                nvlinkledIntf->ledState(
                    sdbusplus::common::com::nvidia::nv_link::NVLinkLED::
                        LedState::Steady_Amber);
                break;
            case NSM_NVLINK_LED_AMBER_BLINK:
                nvlinkledIntf->ledState(
                    sdbusplus::common::com::nvidia::nv_link::NVLinkLED::
                        LedState::Blinking_Amber);
                break;
            case NSM_NVLINK_LED_OFF:
                nvlinkledIntf->ledState(sdbusplus::common::com::nvidia::
                                            nv_link::NVLinkLED::LedState::Off);
                break;
            default:
                nvlinkledIntf->ledState(
                    sdbusplus::common::com::nvidia::nv_link::NVLinkLED::
                        LedState::Read_Error);
                break;
        }
    }
    else
    {
        nvlinkledIntf->ledState(sdbusplus::common::com::nvidia::nv_link::
                                    NVLinkLED::LedState::Read_Error);

        LG2_ERROR_FLT(
            "NsmSpiRead decode_get_nvlink_agg_led_status_resp failure | reason_code: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reason_code, "CC", cc, "RC", rc);

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return rc;
}

static requester::Coroutine
    createNsmChassisLEDSensor(SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        dbus::PropertyMap allBaseIfaceProperties;
        auto rc = co_await utils::coGetCachedBaseProperties(
            objPath, NSM_CHASSIS_LED_INTERFACE, allBaseIfaceProperties);
        if (rc != NSM_SUCCESS)
        {
            co_return rc;
        }
        dbus::PropertyMap allCurrentIfaceProperties =
            co_await utils::coGetAllDbusProperty(utils::entityManagerServiceStr,
                                                 objPath, interface);

        std::string name{};
        std::string type{};
        uuid_t uuid{};
        std::string inventoryObjPath{};

        if (allBaseIfaceProperties.count("Name"))
        {
            name = std::get<std::string>(allBaseIfaceProperties.at("Name"));
        }
        if (allCurrentIfaceProperties.count("Type"))
        {
            type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
        }
        if (allBaseIfaceProperties.count("UUID"))
        {
            uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
        }
        if (allBaseIfaceProperties.count("InventoryObjPath"))
        {
            inventoryObjPath = std::get<std::string>(
                allBaseIfaceProperties.at("InventoryObjPath"));
        }

        auto nsmDevice = manager.getNsmDevice(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_Processor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);

            // coverity[missing_return]
            co_return NSM_ERROR;
        }

        if (type == "NSM_ChassisLED")
        {
            auto nvlinkLedStatusSensor = std::make_shared<NsmNvlinkLedIntf>(
                bus, name, type, inventoryObjPath);

            nsmDevice->addSensor(nvlinkLedStatusSensor, false, false);
        }
    }

    catch (const std::exception& e)
    {
        lg2::error(
            "Error while addSensor for path {PATH} and interface {INTF}, {ERROR}",
            "PATH", objPath, "INTF", interface, "ERROR", e);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmChassisLEDSensor,
    "xyz.openbmc_project.Configuration.NSM_ChassisLED")
} // namespace nsm
