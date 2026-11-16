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
#include "features/associations.hpp"
#include "globals.hpp"
#include "nsmApSkuIdIntf.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmChassisFeatures.hpp"
#include "nsmCommon.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/MCTP/UUID/server.hpp>

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
    const auto modelProperty = getModelInventoryProperty(
        device->getDeviceType(), device->getDeviceRole());
    auto model = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        *assetObject, modelProperty);
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

void createIRoTResponderLocationCode(
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

void createIRoTResponderLocationContext(
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

void createIRoTResponderFieldReplaceable(
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

} // namespace nsm

namespace nsm::features
{

namespace
{
const std::string iRoTResponderType = "NSM_ChassisIRoTResponder";
using MctpUuidIntf = object_t<MCTP::server::UUID>;

std::string iRoTResponderName(const dbus::PropertyMap& params)
{
    return param<std::string>(params, "Name").value_or("");
}
} // namespace

requester::Coroutine
    createIRoTChassisTypeFeature([[maybe_unused]] SensorManager& manager,
                                 const DeviceContext& dev,
                                 const dbus::PropertyMap& params)
{
    if (!params.count("ChassisType"))
    {
        co_return NSM_SUCCESS;
    }
    auto name = iRoTResponderName(params);
    auto properties = params;
    createIRoTResponderChassis(dev.device, name, iRoTResponderType, properties);
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createIRoTLocationTypeFeature([[maybe_unused]] SensorManager& manager,
                                  const DeviceContext& dev,
                                  const dbus::PropertyMap& params)
{
    if (!params.count("LocationType"))
    {
        co_return NSM_SUCCESS;
    }
    auto name = iRoTResponderName(params);
    auto properties = params;
    createIRoTResponderLocation(dev.device, name, iRoTResponderType,
                                properties);
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createIRoTLocationContextFeature([[maybe_unused]] SensorManager& manager,
                                     const DeviceContext& dev,
                                     const dbus::PropertyMap& params)
{
    if (!params.count("LocationContext"))
    {
        co_return NSM_SUCCESS;
    }
    auto name = iRoTResponderName(params);
    auto properties = params;
    createIRoTResponderLocationContext(dev.device, name, iRoTResponderType,
                                       properties);
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createIRoTAssociationsFeature([[maybe_unused]] SensorManager& manager,
                                  const DeviceContext& dev,
                                  const dbus::PropertyMap& params)
{
    auto name = iRoTResponderName(params);
    std::vector<utils::Association> associations;
    addAssociations(associations, params, "ParentChassisAssociationPaths",
                    "parent_chassis", "all_chassis");
    // Components protected by this RoT
    addAssociations(associations, params, "RoTInventoryAssociationPaths",
                    "inventory", "associated_ROT");
    addAssociations(associations, params, "InventoryChassisAssociationPaths",
                    "inventory_chassis", "associated_chassis");
    if (associations.empty())
    {
        co_return NSM_SUCCESS;
    }

    auto associationsObject =
        std::make_shared<NsmIRoTResponder<AssociationDefinitionsIntf>>(
            name, iRoTResponderType);
    associationsObject->invoke(pdiMethod(associations),
                               utils::getAssociations(associations));
    dev.device->addStaticSensor(associationsObject);
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createIRoTAssetFeature([[maybe_unused]] SensorManager& manager,
                           const DeviceContext& dev,
                           const dbus::PropertyMap& params)
{
    auto name = iRoTResponderName(params);
    dbus::PropertyMap properties;
    if (auto assetName = param<std::string>(params, "AssetName"))
    {
        properties.emplace("Name", *assetName);
    }
    createIRoTResponderAsset(dev.device, name, iRoTResponderType, properties);
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createIRoTHealthFeature([[maybe_unused]] SensorManager& manager,
                            const DeviceContext& dev,
                            const dbus::PropertyMap& params)
{
    auto name = iRoTResponderName(params);
    createIRoTResponderHealth(dev.device, name, iRoTResponderType);
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createIRoTLocationCodeFeature([[maybe_unused]] SensorManager& manager,
                                  const DeviceContext& dev,
                                  const dbus::PropertyMap& params)
{
    if (!params.count("LocationCode"))
    {
        co_return NSM_SUCCESS;
    }
    auto name = iRoTResponderName(params);
    auto properties = params;
    createIRoTResponderLocationCode(dev.device, name, iRoTResponderType,
                                    properties);
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createIRoTFieldReplaceableFeature([[maybe_unused]] SensorManager& manager,
                                      const DeviceContext& dev,
                                      const dbus::PropertyMap& params)
{
    if (!params.count("FieldReplaceable"))
    {
        co_return NSM_SUCCESS;
    }
    auto name = iRoTResponderName(params);
    auto properties = params;
    createIRoTResponderFieldReplaceable(dev.device, name, iRoTResponderType,
                                        properties);
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createIRoTSPDMResponderFeature([[maybe_unused]] SensorManager& manager,
                                   const DeviceContext& dev,
                                   const dbus::PropertyMap& params)
{
    auto name = iRoTResponderName(params);
    auto uuidObject =
        std::make_shared<NsmIRoTResponder<UuidIntf>>(name, iRoTResponderType);
    uuidObject->invoke(pdiMethod(uuid), dev.staticUuid);
    dev.device->addStaticSensor(uuidObject);
    // spdmd matches the MCTP endpoint on MCTP.UUID before Common.UUID
    if (param<bool>(params, "MctpUuidEnabled").value_or(false))
    {
        auto mctpUuid = std::make_shared<NsmIRoTResponder<MctpUuidIntf>>(
            name, iRoTResponderType);
        mctpUuid->invoke(pdiMethod(uuid), dev.device->getUuid());
        dev.device->addStaticSensor(mctpUuid);
    }
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createIRoTSKUFeature([[maybe_unused]] SensorManager& manager,
                         const DeviceContext& dev,
                         const dbus::PropertyMap& params)
{
    auto name = iRoTResponderName(params);
    // Seeded ""; written by another producer (pldm EC SKU)
    dev.device->addStaticSensor(
        std::make_shared<NsmIRoTResponder<NsmApSkuIdIntf>>(name,
                                                           iRoTResponderType));
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
