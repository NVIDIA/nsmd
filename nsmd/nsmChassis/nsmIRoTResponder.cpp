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

#include "nsmIRoTResponder.hpp"

#include "debug-token.h"

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "deviceManager.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmCommon.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <format>
#include <unordered_map>

namespace nsm
{

template <typename IntfType>
requester::Coroutine NsmIRoTResponder<IntfType>::update(SensorManager& manager,
                                                        eid_t eid)
{
    if constexpr (std::is_same_v<IntfType, NsmAssetIntf>)
    {
        auto request = std::make_shared<Request>(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_ids_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
        auto rc = encode_nsm_query_device_ids_req(DEFAULT_INSTANCE_ID,
                                                  requestMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug("IRoTResponder: encode_nsm_query_device_ids_req: "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);
            // coverity[missing_return]
            co_return rc;
        }
        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto sendRc = co_await manager.SendRecvNsmMsg(eid, *request,
                                                      responseMsg, responseLen);
        if (sendRc)
        {
            lg2::debug("IRoTResponder: queryDeviceId SendRecvNsmMsg: "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", sendRc);
            // coverity[missing_return]
            co_return sendRc;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        uint8_t deviceId[NSM_DEBUG_TOKEN_DEVICE_ID_SIZE] = {0};
        rc = decode_nsm_query_device_ids_resp(responseMsg.get(), responseLen,
                                              &cc, &reasonCode, deviceId);
        LG2_ERROR_FLT(
            "decode_nsm_query_device_ids_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            // coverity[missing_return]
            co_return cc ? cc : rc;
        }
        std::string serialHex = "0x";
        for (auto i = 0; i < NSM_DEBUG_TOKEN_DEVICE_ID_SIZE; ++i)
        {
            serialHex += std::format("{:02X}", deviceId[i]);
        }
        this->invoke(pdiMethod(serialNumber), serialHex);
    }

    if constexpr (std::is_same_v<IntfType, UuidIntf>)
    {
        DeviceManager& deviceManager = DeviceManager::getInstance();
        uuid_t deviceUuid;
        auto rc = co_await getDeviceUUID(manager, eid, deviceManager,
                                         deviceUuid);
        if (rc == NSM_SW_SUCCESS && !deviceUuid.empty())
        {
            this->invoke(pdiMethod(uuid), deviceUuid);
            auto device = manager.getNsmDeviceFromStaticUUID(deviceUuid);
            auto spdmResponderObject =
                std::make_shared<NsmIRoTResponder<SPDMResponderIntf>>(
                    this->name, "NSM_ChassisIRoTResponder");
            device->addStaticSensor(spdmResponderObject);
            // coverity[missing_return]
            co_return NSM_SUCCESS;
        }
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

static requester::Coroutine createNsmIRoTResponder(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath)
{
    std::string baseType = "NSM_ChassisIRoTResponder";
    std::string baseInterface = "xyz.openbmc_project.Configuration." + baseType;

    dbus::PropertyMap allBaseIfaceProperties;
    auto rc = co_await utils::coGetCachedBaseProperties(objPath, baseInterface,
                                                        allBaseIfaceProperties);
    if (rc != NSM_SUCCESS)
    {
        co_return rc;
    }
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string name{};
    if (allBaseIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allBaseIfaceProperties.at("Name"));
    }
    std::string type{};
    if (allCurrentIfaceProperties.count("Type"))
    {
        type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
    }
    uuid_t uuid{};
    if (allBaseIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
    }

    auto device = manager.getNsmDeviceFromStaticUUID(uuid);

    if (type == baseType)
    {
        lg2::debug("IRoTResponder: {NAME}, {TYPE}", "NAME", name.c_str(),
                   "TYPE", type.c_str());
        auto uuidObject =
            std::make_shared<NsmIRoTResponder<UuidIntf>>(name, baseType);
        uuid_t uuid{};
        if (allCurrentIfaceProperties.count("UUID"))
        {
            uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
        }

        uuidObject->invoke(pdiMethod(uuid), uuid);
        device->addStaticSensor(uuidObject);

        std::vector<utils::Association> associations{};
        co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                          associations);
        auto associationsObject =
            std::make_shared<NsmIRoTResponder<AssociationDefinitionsIntf>>(
                name, baseType);
        associationsObject->invoke(pdiMethod(associations),
                                   utils::getAssociations(associations));
        device->addStaticSensor(associationsObject);
    }
    else if (type == "NSM_Asset")
    {
        lg2::debug("IRoTResponder: {NAME}, {TYPE}", "NAME", name.c_str(),
                   "TYPE", type.c_str());
        auto assetObject =
            std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(name, baseType);
        std::string assetName{};
        if (allCurrentIfaceProperties.count("Name"))
        {
            assetName =
                std::get<std::string>(allCurrentIfaceProperties.at("Name"));
        }
        std::string assetManufacturer{};
        if (allCurrentIfaceProperties.count("Manufacturer"))
        {
            assetManufacturer = std::get<std::string>(
                allCurrentIfaceProperties.at("Manufacturer"));
        }

        assetObject->invoke(pdiMethod(name), assetName);
        assetObject->invoke(pdiMethod(manufacturer), assetManufacturer);
        device->addStaticSensor(assetObject);

        auto buildDate = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            *assetObject, BUILD_DATE);
        auto model = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            *assetObject, MARKETING_NAME);
        auto partNumber = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            *assetObject, DEVICE_PART_NUMBER);
        device->addStaticSensor(buildDate);
        device->addStaticSensor(model);
        device->addStaticSensor(partNumber);
    }
    else if (type == "NSM_Chassis")
    {
        lg2::debug("IRoTResponder: {NAME}, {TYPE}", "NAME", name.c_str(),
                   "TYPE", type.c_str());
        auto chassisObject =
            std::make_shared<NsmIRoTResponder<ChassisIntf>>(name, baseType);
        std::string chassisType{};
        if (allCurrentIfaceProperties.count("ChassisType"))
        {
            chassisType = std::get<std::string>(
                allCurrentIfaceProperties.at("ChassisType"));
        }

        chassisObject->invoke(
            pdiMethod(type),
            ChassisIntf::convertChassisTypeFromString(chassisType));
        device->addStaticSensor(chassisObject);
    }
    else if (type == "NSM_Health")
    {
        lg2::debug("IRoTResponder: {NAME}, {TYPE}", "NAME", name.c_str(),
                   "TYPE", type.c_str());
        auto healthObject =
            std::make_shared<NsmIRoTResponder<HealthIntf>>(name, baseType);
        std::string health{};
        if (allCurrentIfaceProperties.count("Health"))
        {
            health =
                std::get<std::string>(allCurrentIfaceProperties.at("Health"));
        }

        healthObject->invoke(pdiMethod(health),
                             HealthIntf::convertHealthTypeFromString(health));
        device->addStaticSensor(healthObject);
    }
    else if (type == "NSM_Location")
    {
        lg2::debug("IRoTResponder: {NAME}, {TYPE}", "NAME", name.c_str(),
                   "TYPE", type.c_str());
        auto locationObject =
            std::make_shared<NsmIRoTResponder<LocationIntf>>(name, baseType);
        std::string locationType{};
        if (allCurrentIfaceProperties.count("LocationType"))
        {
            locationType = std::get<std::string>(
                allCurrentIfaceProperties.at("LocationType"));
        }

        locationObject->invoke(
            pdiMethod(locationType),
            LocationIntf::convertLocationTypesFromString(locationType));
        device->addStaticSensor(locationObject);
    }

    co_return NSM_SUCCESS;
}

std::vector<std::string> IRoTResponderInterfaces{
    "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder",
    "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder.Asset",
    "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder.Chassis",
    "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder.Health",
    "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder.Location"};

REGISTER_NSM_CREATION_FUNCTION(createNsmIRoTResponder, IRoTResponderInterfaces)

} // namespace nsm
