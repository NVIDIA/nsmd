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

#include "nsmNetworkAdapter.hpp"

#include "dBusAsyncUtils.hpp"
#include "nsmDebugInfo.hpp"
#include "nsmDebugToken.hpp"
#include "nsmEraseTrace.hpp"
#include "nsmErrorInjectionCommon.hpp"
#include "nsmLogInfo.hpp"
#include "libnsm/device-configuration.h"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
NsmNetworkAdapterDI::NsmNetworkAdapterDI(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    auto objPath = inventoryObjPath + name;
    lg2::info("NsmNetworkAdapterDI: {NAME}", "NAME", name.c_str());

    associationDefIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, objPath.c_str());
    pcieDeviceIntf = std::make_unique<PCIeDeviceIntf>(bus, objPath.c_str());
    networkInterfaceIntf =
        std::make_unique<NetworkInterfaceIntf>(bus, objPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefIntf->associations(associations_list);
}

NsmDeviceProtectionOptions::NsmDeviceProtectionOptions(
    sdbusplus::bus::bus& bus, const char* path, const std::string& name,
    const std::string& type) : NsmSensor(name, type)
{
    lg2::info("NsmDeviceProtectionOptions: create sensor for {NAME}", "NAME",
              name.c_str());

    objPath = path;
    protectionIntf = std::make_shared<ProtectionIntf>(bus, objPath.c_str());
    protectionIntf->protectionLevel(ProtectionOption::Unknown);
    asyncPatchInProgress = false;
}

std::optional<std::vector<uint8_t>>
    NsmDeviceProtectionOptions::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_protection_options_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_protection_options_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

ProtectionOption NsmDeviceProtectionOptions::convertNsmdToDbusProtectionMode(
    uint8_t protectionMode)
{
    switch (protectionMode)
    {
        case PROTECTION_NONE:
            return ProtectionOption::NoProtection;
        case PROTECTION_PREVENT_FW_UPDATE_AND_CONFIG:
            return ProtectionOption::PreventAll;
        case PROTECTION_PREVENT_FW_UPDATE:
            return ProtectionOption::PreventHostFirmwareUpdates;
        case PROTECTION_PREVENT_CONFIG:
            return ProtectionOption::PreventHostConfigurations;
        default:
            lg2::debug("Unknown protection mode received: {MODE}", "MODE",
                       protectionMode);
            return ProtectionOption::Unknown;
    }
}

uint8_t NsmDeviceProtectionOptions::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint8_t protectionMode;

    auto rc = decode_get_protection_options_resp(responseMsg, responseLen, &cc,
                                                 &reason_code, &protectionMode);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        auto dbusProtectionOption =
            convertNsmdToDbusProtectionMode(protectionMode);
        protectionIntf->protectionLevel(dbusProtectionOption);
    }
    return cc ? cc : rc;
}

requester::Coroutine NsmDeviceProtectionOptions::setProtectionOptions(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    if (asyncPatchInProgress)
    {
        lg2::error("Protection options update already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }
    asyncPatchInProgress = true;

    uint8_t protectionMode = PROTECTION_NONE;

    const std::string* protectionOptionStr = std::get_if<std::string>(&value);
    if (!protectionOptionStr)
    {
        lg2::error("Invalid argument type for setProtectionOptions");
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress = false;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    try
    {
        auto protectionOption =
            ProtectionIntf::convertProtectionOptionFromString(
                *protectionOptionStr);

        switch (protectionOption)
        {
            case ProtectionOption::NoProtection:
                protectionMode = PROTECTION_NONE;
                break;
            case ProtectionOption::PreventAll:
                protectionMode = PROTECTION_PREVENT_FW_UPDATE_AND_CONFIG;
                break;
            case ProtectionOption::PreventHostFirmwareUpdates:
                protectionMode = PROTECTION_PREVENT_FW_UPDATE;
                break;
            case ProtectionOption::PreventHostConfigurations:
                protectionMode = PROTECTION_PREVENT_CONFIG;
                break;
            default:
                lg2::error("Unknown protection option: {OPTION}", "OPTION",
                           *protectionOptionStr);
                *status = AsyncOperationStatusType::InvalidArgument;
                asyncPatchInProgress = false;
                co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to convert protection option string: {ERROR}",
                   "ERROR", e.what());
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress = false;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    auto eid = device->getEid();
    lg2::info("set Protection Options for EID: {EID}, mode: {MODE}", "EID", eid,
              "MODE", static_cast<int>(protectionMode));

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_protection_options_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_protection_options_req(0, protectionMode, requestMsg);

    if (rc)
    {
        lg2::error(
            "NsmDeviceProtectionOptions::setProtectionOptions encode_set_protection_options_req failed. rc={RC} requestBytes={REQUESTBYTES}",
            "RC", rc, "REQUESTBYTES",
            utils::convertMsgToString(utils::Tx, request, MCTP_MSG_TAG_REQ,
                                      eid));
        *status = AsyncOperationStatusType::WriteFailure;
        asyncPatchInProgress = false;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    SensorManager& manager = SensorManager::getInstance();
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        std::vector<uint8_t> responseVec(
            reinterpret_cast<const uint8_t*>(responseMsg.get()),
            reinterpret_cast<const uint8_t*>(responseMsg.get()) + responseLen);
        lg2::error(
            "NsmDeviceProtectionOptions::setProtectionOptions postPatchIO failed for"
            "eid={EID} rc={RC} responseBytes={RESPONSEBYTES}",
            "EID", eid, "RC", rc_, "RESPONSEBYTES",
            utils::convertMsgToString(utils::Rx, responseVec, MCTP_TAG_NSM,
                                      eid));
        *status = AsyncOperationStatusType::WriteFailure;
        asyncPatchInProgress = false;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_set_protection_options_resp(responseMsg.get(), responseLen, &cc,
                                            &reason_code);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        lg2::info(
            "NsmDeviceProtectionOptions::setProtectionOptions for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        std::vector<uint8_t> responseVec(
            reinterpret_cast<const uint8_t*>(responseMsg.get()),
            reinterpret_cast<const uint8_t*>(responseMsg.get()) + responseLen);
        lg2::error(
            "NsmDeviceProtectionOptions::setProtectionOptions decode_set_protection_options_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A} responseBytes={RESPONSEBYTES}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc, "RESPONSEBYTES",
            utils::convertMsgToString(utils::Rx, responseVec, MCTP_TAG_NSM,
                                      eid));
        *status = AsyncOperationStatusType::WriteFailure;
        asyncPatchInProgress = false;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    asyncPatchInProgress = false;
    co_return NSM_SW_SUCCESS;
}

inline void createDeviceProtectionOptions(std::shared_ptr<NsmDevice> device,
                                          const std::string& type,
                                          const std::string& inventoryObjPath,
                                          const std::string& deviceName)
{
    auto dbusObjPath = inventoryObjPath + deviceName;
    auto nsmDeviceProtectionOptions =
        std::make_shared<NsmDeviceProtectionOptions>(
            utils::DBusHandler::getBus(), dbusObjPath.c_str(), deviceName,
            type);
    device->addSensor(nsmDeviceProtectionOptions, false);

    nsm::AsyncSetOperationHandler setProtectionOptionsHandler =
        std::bind(&NsmDeviceProtectionOptions::setProtectionOptions,
                  nsmDeviceProtectionOptions, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    AsyncOperationManager::getInstance()
        ->getDispatcher(dbusObjPath)
        ->addAsyncSetOperation(
            "com.nvidia.DeviceProtection", "ProtectionLevel",
            AsyncSetOperationInfo{setProtectionOptionsHandler,
                                  nsmDeviceProtectionOptions, device});
}

static requester::Coroutine
    createNSMNetworkAdapter(SensorManager& manager,
                            const std::string& interface,
                            const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", interface.c_str());
    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);
    auto nsmDevice = manager.getNsmDevice(uuid);

    auto networkAdapterDI = std::make_shared<NsmNetworkAdapterDI>(
        bus, name, associations, type, inventoryObjPath);
    nsmDevice->deviceSensors.emplace_back(networkAdapterDI);

    auto debugTokenObject = std::make_shared<NsmDebugTokenObject>(
        bus, name, associations, type, uuid);
    nsmDevice->addStaticSensor(debugTokenObject);

    auto networkAdapterDebugInfoObject = std::make_shared<NsmDebugInfoObject>(
        bus, name, inventoryObjPath, type, uuid, DebugDumpType::Network);
    nsmDevice->addStaticSensor(networkAdapterDebugInfoObject);

    auto networkAdapterEraseTraceObject = std::make_shared<NsmEraseTraceObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterEraseTraceObject);

    auto networkAdapterLogInfoObject = std::make_shared<NsmLogInfoObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterLogInfoObject);

    createNsmErrorInjectionSensors(manager, nsmDevice,
                                   path(inventoryObjPath) / name);

    auto ntwAdpResetSensor = std::make_shared<NsmNetworkAdapterDIReset>(
        bus, name, type, inventoryObjPath, nsmDevice);
    nsmDevice->deviceSensors.push_back(ntwAdpResetSensor);

#if defined(ENABLE_NETWORK_ADAPTER_PROTECTION_OPTION)
    createDeviceProtectionOptions(nsmDevice, type, inventoryObjPath, name);
#endif

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

NsmNetworkAdapterDIReset::NsmNetworkAdapterDIReset(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    std::string& inventoryObjPath, std::shared_ptr<NsmDevice> device) :
    NsmObject(name, type)
{
    lg2::info("NsmNetworkAdapterDIReset: create sensor:{NAME}", "NAME",
              name.c_str());

    objPath = inventoryObjPath + name;
    resetIntf = std::make_shared<NsmResetDeviceIntf>(bus, objPath.c_str());
    resetIntf->resetType(sdbusplus::common::xyz::openbmc_project::control::
                             Reset::ResetTypes::ForceRestart);
    resetAsyncIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, objPath.c_str(), device);
}

REGISTER_NSM_CREATION_FUNCTION(
    createNSMNetworkAdapter,
    "xyz.openbmc_project.Configuration.NSM_NetworkAdapter")

} // namespace nsm
