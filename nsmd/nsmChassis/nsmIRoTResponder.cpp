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
#include "nsmAssetIntf.hpp"
#include "nsmCommon.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <phosphor-logging/lg2.hpp>

#include <format>
#include <sstream>
#include <unordered_map>

namespace nsm
{

template <typename IntfType>
requester::Coroutine
    NsmIRoTResponder<IntfType>::update(std::shared_ptr<NsmDevice> nsmDevice)
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
                       "EID", nsmDevice->getEid(), "RC", rc);
            // coverity[missing_return]
            co_return rc;
        }
        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto sendRc = co_await nsmDevice->sensorIO(
            nsmDevice->getEid(), *request, responseMsg, responseLen, false);
        if (sendRc)
        {
            lg2::debug("IRoTResponder: queryDeviceId sensorIO: "
                       "eid={EID} rc={RC}",
                       "EID", nsmDevice->getEid(), "RC", sendRc);
            // coverity[missing_return]
            co_return sendRc;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        size_t deviceIdLen = 0;
        rc = decode_nsm_query_device_ids_resp(responseMsg.get(), responseLen,
                                              &cc, &reasonCode, nullptr,
                                              &deviceIdLen);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            LG2_ERROR_FLT("decode_nsm_query_device_ids_resp failure"
                          "| reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                          "REASONCODE", reasonCode, "CC", cc, "RC", rc);
            // coverity[missing_return]
            co_return cc ? cc : rc;
        }
        std::vector<uint8_t> deviceId(deviceIdLen);
        rc = decode_nsm_query_device_ids_resp(responseMsg.get(), responseLen,
                                              &cc, &reasonCode, deviceId.data(),
                                              &deviceIdLen);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            LG2_ERROR_FLT("decode_nsm_query_device_ids_resp failure"
                          "| reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                          "REASONCODE", reasonCode, "CC", cc, "RC", rc);
            // coverity[missing_return]
            co_return cc ? cc : rc;
        }
        std::stringstream oss;
        oss << "0x";
        for (const auto& byte : deviceId)
        {
            oss << std::format("{:02X}", byte);
        }
        this->invoke(pdiMethod(serialNumber), oss.str());
    }

    if constexpr (std::is_same_v<IntfType, UuidIntf>)
    {
        mctp::MctpDiscovery& mctpDiscovery = mctp::MctpDiscovery::getInstance();
        uuid_t deviceUuid;
        auto rc = co_await getDeviceUUID(nsmDevice, mctpDiscovery, deviceUuid);
        if (rc == NSM_SW_SUCCESS && !deviceUuid.empty())
        {
            this->invoke(pdiMethod(uuid), deviceUuid);

            auto spdmResponderObject =
                std::make_shared<NsmIRoTResponder<SPDMResponderIntf>>(
                    this->name, "NSM_ChassisIRoTResponder");
            nsmDevice->addStaticSensor(spdmResponderObject);
            // coverity[missing_return]
            co_return NSM_SUCCESS;
        }
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

void createIRoTResponderAsset(std::shared_ptr<NsmDevice> device,
                              std::string& name, const std::string& baseType,
                              dbus::PropertyMap& allCurrentIfaceProperties)
{
    auto assetObject =
        std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(name, baseType);
    std::string assetName{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        assetName = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    std::string assetManufacturer = MANUFACTURER_NVIDIA;

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

void createIRoTResponderHealth(std::shared_ptr<NsmDevice> device,
                               std::string& name, const std::string& baseType)
{
    std::string health = HEALTH_TYPE_OK;
    auto healthObject =
        std::make_shared<NsmIRoTResponder<HealthIntf>>(name, baseType);
    healthObject->invoke(pdiMethod(health),
                         HealthIntf::convertHealthTypeFromString(health));
    device->addStaticSensor(healthObject);
}

void createIRoTResponderLocation(std::shared_ptr<NsmDevice> device,
                                 std::string& name, const std::string& baseType,
                                 dbus::PropertyMap& allCurrentIfaceProperties)
{
    auto locationObject =
        std::make_shared<NsmIRoTResponder<LocationIntf>>(name, baseType);
    std::string locationType =
        std::get<std::string>(allCurrentIfaceProperties.at("LocationType"));

    locationObject->invoke(
        pdiMethod(locationType),
        LocationIntf::convertLocationTypesFromString(locationType));
    device->addStaticSensor(locationObject);
}
void createIRoTResponderChassis(std::shared_ptr<NsmDevice> device,
                                std::string& name, const std::string& baseType,
                                dbus::PropertyMap& allCurrentIfaceProperties)
{
    auto chassisObject =
        std::make_shared<NsmIRoTResponder<ChassisIntf>>(name, baseType);
    std::string chassisType =
        std::get<std::string>(allCurrentIfaceProperties.at("ChassisType"));
    chassisObject->invoke(
        pdiMethod(type),
        ChassisIntf::convertChassisTypeFromString(chassisType));
    device->addStaticSensor(chassisObject);
}

static void createIRoTResponderLocationCode(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& baseType, dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string locationCode;
    if (allCurrentIfaceProperties.count("LocationCode"))
    {
        locationCode =
            std::get<std::string>(allCurrentIfaceProperties.at("LocationCode"));
    }

    auto locationCodeObject =
        std::make_shared<NsmIRoTResponder<LocationCodeIntf>>(name, baseType);
    locationCodeObject->invoke(pdiMethod(locationCode), locationCode);
    device->addStaticSensor(locationCodeObject);
}

static void createIRoTResponderLocationContext(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& baseType, dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string locationContext;
    if (allCurrentIfaceProperties.count("LocationContext"))
    {
        locationContext = std::get<std::string>(
            allCurrentIfaceProperties.at("LocationContext"));
    }

    auto locationContextObject =
        std::make_shared<NsmIRoTResponder<LocationContextIntf>>(name, baseType);
    locationContextObject->invoke(pdiMethod(locationContext), locationContext);
    device->addStaticSensor(locationContextObject);
}

static void createIRoTResponderFieldReplaceable(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& baseType, dbus::PropertyMap& allCurrentIfaceProperties)
{
    bool fieldReplaceable = false;
    if (allCurrentIfaceProperties.count("FieldReplaceable"))
    {
        fieldReplaceable =
            std::get<bool>(allCurrentIfaceProperties.at("FieldReplaceable"));
    }

    auto replaceableObject =
        std::make_shared<NsmIRoTResponder<ReplaceableIntf>>(name, baseType);
    replaceableObject->invoke(pdiMethod(fieldReplaceable), fieldReplaceable);
    device->addStaticSensor(replaceableObject);
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
    else if (type == "NSM_Chassis_Attributes")
    {
        createIRoTResponderAsset(device, name, baseType,
                                 allCurrentIfaceProperties);
        createIRoTResponderHealth(device, name, baseType);
        if (allCurrentIfaceProperties.count("LocationType"))
        {
            createIRoTResponderLocation(device, name, baseType,
                                        allCurrentIfaceProperties);
        }
        if (allCurrentIfaceProperties.count("ChassisType"))
        {
            createIRoTResponderChassis(device, name, baseType,
                                       allCurrentIfaceProperties);
        }
        if (allCurrentIfaceProperties.count("LocationCode"))
        {
            createIRoTResponderLocationCode(device, name, baseType,
                                            allCurrentIfaceProperties);
        }
        if (allCurrentIfaceProperties.count("LocationContext"))
        {
            createIRoTResponderLocationContext(device, name, baseType,
                                               allCurrentIfaceProperties);
        }
        if (allCurrentIfaceProperties.count("FieldReplaceable"))
        {
            createIRoTResponderFieldReplaceable(device, name, baseType,
                                                allCurrentIfaceProperties);
        }
    }
    co_return NSM_SUCCESS;
}

std::vector<std::string> IRoTResponderInterfaces{
    "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder",
    "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder.ChassisAttributes"};

REGISTER_NSM_CREATION_FUNCTION(createNsmIRoTResponder, IRoTResponderInterfaces)

} // namespace nsm
