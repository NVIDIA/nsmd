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

#include "NVLinkManagementNICSWInventory.hpp"

#include "dBusAsyncUtils.hpp"
#include "nsmAssetIntf.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
NsmSWInventoryDriverVersionAndStatus::NsmSWInventoryDriverVersionAndStatus(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const std::string& manufacturer) :
    NsmSensor(name, type)
{
    auto NVLinkManagementNICFWInvBasePath =
        std::string(sotwareInventoryBasePath) + "/" + name;

    lg2::info("NsmSWInventoryDriverVersionAndStatus: create sensor:{NAME}",
              "NAME", name.c_str());
    softwareVer_ = std::make_unique<SoftwareIntf>(
        bus, NVLinkManagementNICFWInvBasePath.c_str());
    operationalStatus_ = std::make_unique<OperationalStatusIntf>(
        bus, NVLinkManagementNICFWInvBasePath.c_str());
    // add all interfaces
    associationDef_ = std::make_unique<AssociationDefinitionsInft>(
        bus, NVLinkManagementNICFWInvBasePath.c_str());
    // handle associations
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDef_->associations(associations_list);
    asset_ = std::make_unique<NsmAssetIntf>(
        bus, NVLinkManagementNICFWInvBasePath.c_str());
    asset_->manufacturer(manufacturer);
}

void NsmSWInventoryDriverVersionAndStatus::updateValue(
    enum8 driverState, std::string driverVersion)
{
    softwareVer_->version(driverVersion);
    switch (static_cast<int>(driverState))
    {
        case 2:
            operationalStatus_->functional(true);
            break;
        default:
            operationalStatus_->functional(false);
            break;
    }

    // to be consumed by unit tests
    driverState_ = driverState;
    driverVersion_ = driverVersion;
}

std::optional<std::vector<uint8_t>>
    NsmSWInventoryDriverVersionAndStatus::genRequestMsg(eid_t eid,
                                                        uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_driver_info_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_driverVersion_req failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmSWInventoryDriverVersionAndStatus::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    enum8 driverState = 0;
    char driverVersion[MAX_VERSION_STRING_SIZE] = {0};

    auto rc = decode_get_driver_info_resp(responseMsg, responseLen, &cc,
                                          &reasonCode, &driverState,
                                          driverVersion);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        std::string version(driverVersion);
        updateValue(driverState, version);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_driver_info_resp sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

static requester::Coroutine
    createNsmNVLinkManagerDriverSensor(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto priority = co_await utils::coGetDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto manufacturer = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Manufacturer", interface.c_str());
    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);
    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDevice(uuid);
    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_NVLinkManagementSWInventory PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto sensor = std::make_shared<NsmSWInventoryDriverVersionAndStatus>(
        bus, name, associations, type, manufacturer);

    if (priority)
    {
        nsmDevice->prioritySensors.push_back(sensor);
    }
    else
    {
        nsmDevice->roundRobinSensors.push_back(sensor);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmNVLinkManagerDriverSensor,
    "xyz.openbmc_project.Configuration.NSM_NVLinkManagementSWInventory")

} // namespace nsm
