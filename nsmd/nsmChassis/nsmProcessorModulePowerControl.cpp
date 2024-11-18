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

#include "nsmProcessorModulePowerControl.hpp"

#include "dBusAsyncUtils.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace nsm
{

NsmClearPowerCapIntf ::NsmClearPowerCapIntf(
    sdbusplus::bus::bus& bus, const std::string& inventoryObjPath) :
    ClearPowerCapIntf(bus, inventoryObjPath.c_str())
{}

int32_t NsmClearPowerCapIntf::clearPowerCap()
{
    return 0;
}

NsmProcessorModulePowerControl::NsmProcessorModulePowerControl(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    std::shared_ptr<PowerCapIntf> powerCapIntf,
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf,
    const std::string& path,
    const std::vector<std::tuple<std::string, std::string, std::string>>&
        associations_list) :
    NsmSensor(name, type),
    ClearPowerCapAsyncIntf(bus, path.c_str()), powerCapIntf(powerCapIntf),
    clearPowerCapIntf(clearPowerCapIntf), path(path)
{
    associationDefinitionsIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, path.c_str());
    powerCapIntf->powerCapEnable(true);
    associationDefinitionsIntf->associations(associations_list);
    decoratorAreaIntf = std::make_unique<DecoratorAreaIntf>(bus, path.c_str());
    decoratorAreaIntf->physicalContext(
        sdbusplus::common::xyz::openbmc_project::inventory::decorator::Area::
            PhysicalContextType::GPUSubsystem);
}

std::optional<std::vector<uint8_t>>
    NsmProcessorModulePowerControl::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_power_limit_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_module_power_limit_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_module_power_limit_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmProcessorModulePowerControl::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    uint32_t requested_persistent_limit;
    uint32_t requested_oneshot_limit;
    uint32_t enforced_limit;

    auto rc = decode_get_power_limit_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code,
        &requested_persistent_limit, &requested_oneshot_limit, &enforced_limit);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        uint32_t reading = (enforced_limit == INVALID_POWER_LIMIT)
                               ? INVALID_POWER_LIMIT
                               : enforced_limit / 1000;
        powerCapIntf->powerCap(reading);
        clearErrorBitMap("decode_get_module_power_limit_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_module_power_limit_resp_module",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

requester::Coroutine NsmProcessorModulePowerControl::setModulePowerCap(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    [[maybe_unused]] std::shared_ptr<NsmDevice> device)
{
    const uint32_t* powerLimit = std::get_if<uint32_t>(&value);

    if (!powerLimit)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (*powerLimit > powerCapIntf->maxPowerCapValue() ||
        *powerLimit < powerCapIntf->minPowerCapValue())
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    auto rc = co_await updatePowerLimitOnModule(status, NEW_LIMIT,
                                                (*powerLimit) * 1000);
    co_return rc;
}

requester::Coroutine NsmProcessorModulePowerControl::updatePowerLimitOnModule(
    AsyncOperationStatusType* status, const uint8_t action,
    const uint32_t value)
{
    if (patchPowerLimitInProgress)
    {
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }
    else
    {
        patchPowerLimitInProgress = true;
    }

    SensorManager& manager = SensorManager::getInstance();
    if (manager.processorModuleToDeviceMap.find(path) ==
        manager.processorModuleToDeviceMap.end())
    {
        *status = AsyncOperationStatusType::WriteFailure;
        patchPowerLimitInProgress = false;
        co_return NSM_SW_ERROR;
    }
    for (auto nsmDevice : manager.processorModuleToDeviceMap[path])
    {
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_limit_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_set_module_power_limit_req(0, action, true,
                                                    value * 1000, requestMsg);

        if (rc)
        {
            lg2::error(
                "updatePowerLimitOnModule encode_set_module_power_limit_req failed. module={MODULE}, rc={RC}",
                "MODULE", getName(), "RC", rc);
            *status = AsyncOperationStatusType::WriteFailure;
            patchPowerLimitInProgress = false;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto eid = manager.getEid(nsmDevice);
        lg2::info("update Power Limit On Module for eid = {EID}", "EID", eid);
        auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                                   responseLen);
        if (rc_)
        {
            lg2::error(
                "updatePowerLimitOnModule SendRecvNsmMsg failed for while setting power limit for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            *status = AsyncOperationStatusType::WriteFailure;
            patchPowerLimitInProgress = false;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size = 0;
        rc = decode_set_power_limit_resp(responseMsg.get(), responseLen, &cc,
                                         &data_size, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            lg2::info("update Power Limit On Module for EID: {EID} completed",
                      "EID", eid);
            *status = AsyncOperationStatusType::Success;
        }
        else
        {
            lg2::error(
                "updatePowerLimitOnModule: decode_set_power_limit_resp failed.eid ={EID},CC = {CC} reasoncode = {RC},RC = {A} ",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            *status = AsyncOperationStatusType::WriteFailure;
            patchPowerLimitInProgress = false;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
    }
    patchPowerLimitInProgress = false;
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmProcessorModulePowerControl::doClearPowerCapOnModule(
    std::shared_ptr<AsyncStatusIntf> statusInterface)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    auto rc = co_await updatePowerLimitOnModule(
        &status, DEFAULT_LIMIT, clearPowerCapIntf->defaultPowerCap());
    statusInterface->status(status);
    co_return rc;
}

sdbusplus::message::object_path NsmProcessorModulePowerControl::clearPowerCap()
{
    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    doClearPowerCapOnModule(statusInterface).detach();

    return objectPath;
}

NsmModulePowerLimit::NsmModulePowerLimit(
    std::string& name, std::string& type, uint8_t propertyId,
    std::shared_ptr<PowerCapIntf> powerCapIntf) :
    NsmObject(name, type),
    propertyId(propertyId), powerCapIntf(powerCapIntf)
{
    switch (propertyId)
    {
        case MAXIMUM_MODULE_POWER_LIMIT:
            propertyName = "MAXIMUM_MODULE_POWER_LIMIT";
            break;
        case MINIMUM_MODULE_POWER_LIMIT:
            propertyName = "MINIMUM_MODULE_POWER_LIMIT";
            break;
        default:
            propertyName = "";
            break;
    }
    lg2::info("NsmModulePowerLimit: create sensor:{NAME}, proprty: {PROPERTY}",
              "NAME", name.c_str(), "PROPERTY", propertyName);
}

requester::Coroutine NsmModulePowerLimit::update(SensorManager& manager,
                                                 eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_inventory_information_req(0, propertyId, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmModulePowerLimit encode_get_inventory_information_req failed. eid={EID} rc={RC} property = {PROPERTY}",
            "EID", eid, "RC", rc, "PROPERTY", propertyName);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmModulePowerLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}, property = {PROPERTY}",
            "RC", rc, "EID", eid, "PROPERTY", propertyName);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        uint32_t reading = (value == INVALID_POWER_LIMIT) ? INVALID_POWER_LIMIT
                                                          : value / 1000;
        switch (propertyId)
        {
            case MAXIMUM_MODULE_POWER_LIMIT:
                powerCapIntf->maxPowerCapValue(reading);
                break;
            case MINIMUM_MODULE_POWER_LIMIT:
                powerCapIntf->minPowerCapValue(reading);
                break;
            default:
                break;
        }
        clearErrorBitMap(
            "NsmModulePowerLimit decode_get_inventory_information_resp " +
            propertyName);
    }
    else
    {
        logHandleResponseMsg(
            "NsmModulePowerLimit decode_get_inventory_information_resp " +
                propertyName,
            reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

NsmDefaultModulePowerLimit::NsmDefaultModulePowerLimit(
    const std::string& name, const std::string& type,
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf) :
    NsmObject(name, type),
    clearPowerCapIntf(clearPowerCapIntf)
{
    lg2::info("NsmDefaultModulePowerLimit: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine NsmDefaultModulePowerLimit::update(SensorManager& manager,
                                                        eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_inventory_information_req(0, RATED_MODULE_POWER_LIMIT,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmDefaultModulePowerLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmDefaultModulePowerLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        uint32_t reading = (value == INVALID_POWER_LIMIT) ? INVALID_POWER_LIMIT
                                                          : value / 1000;
        clearPowerCapIntf->defaultPowerCap(reading);
        clearErrorBitMap(
            "NsmDefaultModulePowerLimit decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg(
            "NsmDefaultModulePowerLimit decode_get_inventory_information_resp",
            reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

static requester::Coroutine
    CreateProcessorModulePowerControl(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());

    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());

    auto priority = co_await utils::coGetDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto instanceNumber = co_await utils::coGetDbusProperty<uint64_t>(
        objPath.substr(0, objPath.rfind('/')).c_str(), "InstanceNumber",
        "xyz.openbmc_project.Inventory.Decorator.Instance");

    auto associations = co_await utils::coGetDbusProperty<
        std::vector<std::tuple<std::string, std::string, std::string>>>(
        objPath.substr(0, objPath.rfind('/')), "Associations",
        "xyz.openbmc_project.Association.Definitions");
    std::string chassisPath = "";
    std::vector<std::tuple<std::string, std::string, std::string>>
        associatedObjects;
    for (auto association : associations)
    {
        if (std::get<0>(association) == "parent_chassis")
        {
            chassisPath = std::get<2>(association);
            associatedObjects.emplace_back(
                std::make_tuple("chassis", "power_controls", chassisPath));
            break;
        }
    }

    if (chassisPath.size() == 0)
    {
        lg2::error(
            "CreateProcessorModulePowerControl: Unable to find Parent Chassis for : {NAME}",
            "NAME", name);
        co_return NSM_ERROR;
    }

    auto nsmDevice = manager.getNsmDevice(uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "The UUID of CreateProcessorModulePowerControl PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        co_return NSM_ERROR;
    }

    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/power/control/ProcessorModule_" +
        std::to_string(instanceNumber / NUM_GPU_PER_MODULE);

    if (manager.processorModuleToDeviceMap.find(inventoryObjPath) ==
        manager.processorModuleToDeviceMap.end())
    {
        manager.processorModuleToDeviceMap[inventoryObjPath] = {nsmDevice};
    }
    else
    {
        manager.processorModuleToDeviceMap[inventoryObjPath].push_back(
            nsmDevice);
    }
    if (instanceNumber % NUM_GPU_PER_MODULE != 0)
    {
        co_return NSM_SUCCESS;
    }

    auto powerCapIntf =
        std::make_shared<PowerCapIntf>(bus, inventoryObjPath.c_str());
    auto clearPowerCapIntf =
        std::make_shared<NsmClearPowerCapIntf>(bus, inventoryObjPath);

    auto nsmProcessorModulePowerControlSensor =
        std::make_shared<NsmProcessorModulePowerControl>(
            bus, name, type, powerCapIntf, clearPowerCapIntf, inventoryObjPath,
            associatedObjects);
    nsmDevice->addSensor(nsmProcessorModulePowerControlSensor, priority);
    auto nsmMaxModulePowerLimit = std::make_shared<NsmModulePowerLimit>(
        name, type, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);
    auto nsmMinModulePowerLimit = std::make_shared<NsmModulePowerLimit>(
        name, type, MINIMUM_MODULE_POWER_LIMIT, powerCapIntf);
    auto nsmDefaultModulePowerLimit =
        std::make_shared<NsmDefaultModulePowerLimit>(name, type,
                                                     clearPowerCapIntf);

    nsmDevice->addStaticSensor(nsmMaxModulePowerLimit);
    nsmDevice->addStaticSensor(nsmMinModulePowerLimit);
    nsmDevice->addStaticSensor(nsmDefaultModulePowerLimit);

    nsm::AsyncSetOperationHandler setModulePowerCapHandler =
        std::bind(&NsmProcessorModulePowerControl::setModulePowerCap,
                  nsmProcessorModulePowerControlSensor, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    AsyncOperationManager::getInstance()
        ->getDispatcher(inventoryObjPath)
        ->addAsyncSetOperation(
            "xyz.openbmc_project.Control.Power.Cap", "PowerCap",
            AsyncSetOperationInfo{setModulePowerCapHandler,
                                  nsmProcessorModulePowerControlSensor,
                                  nsmDevice});

    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    CreateProcessorModulePowerControl,
    "xyz.openbmc_project.Configuration.NSM_ModulePowerControl")
} // namespace nsm
// namespace nsm
