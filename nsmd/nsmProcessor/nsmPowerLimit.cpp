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

#include "nsmPowerLimit.hpp"

#include "dBusAsyncUtils.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace nsm
{

constexpr std::array<std::string_view, 6> powerLimitIdToName = {
    "Device_Power_Limit",             // 0 = DEVICE
    "Module_Power_Limit",             // 1 = MODULE
    "Switch_Power_Limit",             // 2 = SWITCH
    "GPU_Base_Power_Limit",           // 3 = GPU_BASE
    "CPU_Limit_GPU_Copy_Power_Limit", // 4 = CPU_LIMIT_GPU_COPY
    "GPU_Copy_Switch_Power_Limit"     // 5 = Switch_Copy_CPU_Power_Limit
};

NsmClearPowerLimitIntf ::NsmClearPowerLimitIntf(
    sdbusplus::bus::bus& bus, const std::string& inventoryObjPath) :
    ClearPowerLimitIntf(bus, inventoryObjPath.c_str())
{}

int32_t NsmClearPowerLimitIntf::clearPowerCap()
{
    return 0;
}

NsmPowerLimit::NsmPowerLimit(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    std::shared_ptr<PowerLimitsIntf> powerLimitsIntf,
    std::shared_ptr<NsmClearPowerLimitIntf> clearPowerLimitIntf,
    std::shared_ptr<AssociationDefinitionsIntf> associationDefinitionsIntf,
    std::shared_ptr<NsmDevice> nsmDevice, uint8_t powerLimitId,
    const std::string& path) :
    NsmSensor(name, type), ClearPowerLimitAsyncIntf(bus, path.c_str()),
    powerLimitsIntf(powerLimitsIntf), clearPowerLimitIntf(clearPowerLimitIntf),
    associationDefinitionsIntf(associationDefinitionsIntf),
    nsmDevice(nsmDevice), powerLimitId(powerLimitId)
{
    powerLimitsIntf->powerCapEnable(true);
}

std::optional<std::vector<uint8_t>>
    NsmPowerLimit::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_power_limit_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_power_limit_req(instanceId, powerLimitId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_power_limit_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmPowerLimit::handleResponseMsg(const struct nsm_msg* responseMsg,
                                         size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;
    uint32_t requested_persistent_limit;
    uint32_t requested_oneshot_limit;
    uint32_t enforced_limit;

    auto rc = decode_get_power_limit_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode,
        &requested_persistent_limit, &requested_oneshot_limit, &enforced_limit);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        uint32_t reading =
            (enforced_limit == INVALID_POWER_LIMIT) ? 0 : enforced_limit / 1000;
        powerLimitsIntf->powerCap(reading);
    }
    else
    {
        LG2_ERROR_FLT(
            "decode_get_power_limit_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }
    return cc ? cc : rc;
}

void NsmPowerLimit::handleOfflineState()
{
    switch (powerLimitId)
    {
        case CPU_LIMIT_GPU_COPY:
            powerLimitsIntf->powerCap(INVALID_POWER_LIMIT);
            break;
        default:
            break;
    }
    return;
}

requester::Coroutine NsmPowerLimit::setPowerLimit(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    [[maybe_unused]] std::shared_ptr<NsmDevice> device)
{
    const uint32_t* powerLimit = std::get_if<uint32_t>(&value);

    if (!powerLimit)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    auto rc = co_await updatePowerLimit(status, nsmDevice, NEW_LIMIT,
                                        (*powerLimit));
    co_return rc;
}

requester::Coroutine
    NsmPowerLimit::updatePowerLimit(AsyncOperationStatusType* status,
                                    std::shared_ptr<NsmDevice> nsmDevice,
                                    const uint8_t action, const uint32_t value)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_power_limit_req(0, powerLimitId, action, true,
                                         value * 1000, requestMsg);

    if (rc)
    {
        lg2::error(
            "updatePowerLimit encode_set_power_limit_req failed for eid = {EID} and powerLimitId={POWER_LIMIT_ID}, rc={RC}",
            "EID", nsmDevice->getEid(), "POWER_LIMIT_ID", powerLimitId, "RC",
            rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto eid = nsmDevice->getEid();
    lg2::info(
        "update Power Limit for eid = {EID} and powerLimitId={POWER_LIMIT_ID}",
        "EID", eid, "POWER_LIMIT_ID", powerLimitIdToName[powerLimitId]);
    rc = co_await nsmDevice->postPatchIO(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "updatePowerLimit postPatchIO failed for eid = {EID} and powerLimitId={POWER_LIMIT_ID}, rc = {RC}",
            "EID", eid, "POWER_LIMIT_ID", powerLimitIdToName[powerLimitId],
            "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return rc;
    }

    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint8_t cc = NSM_SUCCESS;
    rc = decode_set_power_limit_resp(responseMsg.get(), responseLen, &cc,
                                     &dataSize, &reasonCode);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_set_power_limit_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }

    else
    {
        lg2::info(
            "updatePowerLimit success for eid = {EID} and powerLimitId={POWER_LIMIT_ID}",
            "EID", nsmDevice->getEid(), "POWER_LIMIT_ID",
            powerLimitIdToName[powerLimitId]);
    }

    *status = (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
                  ? AsyncOperationStatusType::Success
                  : AsyncOperationStatusType::WriteFailure;
    co_return cc ? cc : rc;
}

requester::Coroutine NsmPowerLimit::doClearPowerLimit(
    std::shared_ptr<AsyncStatusIntf> statusInterface)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    auto rc = co_await updatePowerLimit(&status, nsmDevice, DEFAULT_LIMIT,
                                        clearPowerLimitIntf->defaultPowerCap());
    statusInterface->status(status);
    co_return rc;
}

sdbusplus::message::object_path NsmPowerLimit::clearPowerCap()
{
    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    doClearPowerLimit(statusInterface).detach();

    return objectPath;
}

NsmPowerLimitRange::NsmPowerLimitRange(
    const std::string& name, const std::string& type, uint8_t propertyId,
    std::shared_ptr<PowerLimitsIntf> powerLimitsIntf) :
    NsmObject(name, type), propertyId(propertyId),
    powerLimitsIntf(powerLimitsIntf)
{
    switch (propertyId)
    {
        case MAXIMUM_GPU_BASE_POWER_LIMIT:
            propertyName = "MAXIMUM_GPU_BASE_POWER_LIMIT";
            break;
        case MINIMUM_GPU_BASE_POWER_LIMIT:
            propertyName = "MINIMUM_GPU_BASE_POWER_LIMIT";
            break;
        default:
            propertyName = "";
            break;
    }
    lg2::info("NsmPowerLimitRange: create sensor:{NAME}, proprty: {PROPERTY}",
              "NAME", name.c_str(), "PROPERTY", propertyName);
}

requester::Coroutine
    NsmPowerLimitRange::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_inventory_information_req(0, propertyId, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmPowerLimitRange encode_get_inventory_information_req failed. eid={EID} rc={RC} property = {PROPERTY}",
            "EID", nsmDevice->getEid(), "RC", rc, "PROPERTY", propertyName);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::error(
            "NsmPowerLimitRange SendRecvNsmMsg failed with RC={RC}, eid={EID}, property = {PROPERTY}",
            "RC", rc, "EID", nsmDevice->getEid(), "PROPERTY", propertyName);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog("NsmPowerLimitRange decode_get_inventory_information_resp " +
                      propertyName,
                  reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        lg2::error(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        uint32_t reading = (value == INVALID_POWER_LIMIT) ? 0 : value / 1000;
        switch (propertyId)
        {
            case MAXIMUM_GPU_BASE_POWER_LIMIT:
                powerLimitsIntf->maxPowerCapValue(reading);
                break;
            case MINIMUM_GPU_BASE_POWER_LIMIT:
                powerLimitsIntf->minPowerCapValue(reading);
                break;
            default:
                break;
        }
    }
    co_return cc ? cc : rc;
}

NsmDefaultPowerLimit::NsmDefaultPowerLimit(
    const std::string& name, const std::string& type, const uint8_t propertyId,
    std::shared_ptr<NsmClearPowerLimitIntf> clearPowerLimitIntf) :
    NsmObject(name, type), propertyId(propertyId),
    clearPowerLimitIntf(clearPowerLimitIntf)
{
    switch (propertyId)
    {
        case RATED_GPU_BASE_POWER_LIMIT:
            propertyName = "RATED_GPU_BASE_POWER_LIMIT";
            break;
        default:
            propertyName = "UNKNOWN";
            break;
    }
    lg2::info(
        "NsmDefaultPowerLimit: create sensor:{NAME} and propertyId={PROPERTY_ID}",
        "NAME", name.c_str(), "PROPERTY_ID", propertyId);
}

requester::Coroutine
    NsmDefaultPowerLimit::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_inventory_information_req(0, propertyId, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmDefaultPowerLimit encode_get_inventory_information_req failed. eid={EID} rc={RC} property = {PROPERTY}",
            "EID", nsmDevice->getEid(), "RC", rc, "PROPERTY", propertyName);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::error(
            "NsmDefaultPowerLimit sensorIO failed with RC={RC}, eid={EID} property = {PROPERTY}",
            "RC", rc, "EID", nsmDevice->getEid(), "PROPERTY", propertyName);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog(
            "NsmDefaultPowerLimit decode_get_inventory_information_resp " +
                propertyName,
            reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        lg2::error(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE} property = {PROPERTY}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize,
            "PROPERTY", propertyName);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        uint32_t reading = (value == INVALID_POWER_LIMIT) ? 0 : value / 1000;
        clearPowerLimitIntf->defaultPowerCap(reading);
    }
    co_return cc ? cc : rc;
}

void createGPUPowerLimit(std::shared_ptr<NsmDevice> nsmDevice,
                         sdbusplus::bus::bus& bus, const std::string& name,
                         const std::string type,
                         const std::string& inventoryObjPath)
{
    if (type == "NSM_GPU_BASE_POWER_LIMIT")
    {
        std::string deviceName =
            inventoryObjPath.substr(inventoryObjPath.find_last_of('/') + 1);

        std::string objPath = "/xyz/openbmc_project/control/" + deviceName +
                              "/Processor_Base_Power_Limit";
        auto powerLimitsIntf =
            std::make_shared<PowerLimitsIntf>(bus, objPath.c_str());
        auto clearPowerLimitIntf =
            std::make_shared<NsmClearPowerLimitIntf>(bus, objPath.c_str());
        auto associationDefinitionsIntf =
            std::make_shared<AssociationDefinitionsIntf>(bus, objPath.c_str());

        std::vector<std::tuple<std::string, std::string, std::string>>
            associations = {{"", "Base_Power_Limit", inventoryObjPath}};
        associationDefinitionsIntf->associations(associations);
        auto nsmPowerLimitSensor = std::make_shared<NsmPowerLimit>(
            bus, name, type, powerLimitsIntf, clearPowerLimitIntf,
            associationDefinitionsIntf, nsmDevice, GPU_BASE, objPath);
        nsmDevice->addSensor(nsmPowerLimitSensor, false);

        nsm::AsyncSetOperationHandler setPowerLimitHandler =
            std::bind(&NsmPowerLimit::setPowerLimit, nsmPowerLimitSensor,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objPath)
            ->addAsyncSetOperation(powerLimitsIntf->interface, "PowerCap",
                                   AsyncSetOperationInfo{setPowerLimitHandler,
                                                         nsmPowerLimitSensor,
                                                         nsmDevice});
        auto nsmDefaultPowerLimit = std::make_shared<NsmDefaultPowerLimit>(
            name, type, RATED_GPU_BASE_POWER_LIMIT, clearPowerLimitIntf);
        nsmDevice->addStaticSensor(nsmDefaultPowerLimit);
        auto nsmMaxPowerLimit = std::make_shared<NsmPowerLimitRange>(
            name, type, MAXIMUM_GPU_BASE_POWER_LIMIT, powerLimitsIntf);
        nsmDevice->addStaticSensor(nsmMaxPowerLimit);
        auto nsmMinPowerLimit = std::make_shared<NsmPowerLimitRange>(
            name, type, MINIMUM_GPU_BASE_POWER_LIMIT, powerLimitsIntf);
        nsmDevice->addStaticSensor(nsmMinPowerLimit);
    }
    else if (type == "NSM_GPU_COPY_CPU_POWER_LIMIT")
    {
        std::string deviceName =
            inventoryObjPath.substr(inventoryObjPath.find_last_of('/') + 1);
        std::string objPath = "/xyz/openbmc_project/control/" + deviceName +
                              "/GPU_Copy_CPU_Power_Limit";
        auto powerLimitsIntf =
            std::make_shared<PowerLimitsIntf>(bus, objPath.c_str());
        auto clearPowerLimitIntf =
            std::make_shared<NsmClearPowerLimitIntf>(bus, objPath.c_str());
        auto associationDefinitionsIntf =
            std::make_shared<AssociationDefinitionsIntf>(bus, objPath.c_str());

        std::vector<std::tuple<std::string, std::string, std::string>>
            associations = {{"", "GPU_copy_Cpu_Power", inventoryObjPath}};
        associationDefinitionsIntf->associations(associations);
        auto nsmPowerLimitSensor = std::make_shared<NsmPowerLimit>(
            bus, name, type, powerLimitsIntf, clearPowerLimitIntf,
            associationDefinitionsIntf, nsmDevice, CPU_LIMIT_GPU_COPY, objPath);
        nsmDevice->addSensor(nsmPowerLimitSensor, false);

        nsm::AsyncSetOperationHandler setPowerLimitHandler =
            std::bind(&NsmPowerLimit::setPowerLimit, nsmPowerLimitSensor,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objPath)
            ->addAsyncSetOperation(powerLimitsIntf->interface, "PowerCap",
                                   AsyncSetOperationInfo{setPowerLimitHandler,
                                                         nsmPowerLimitSensor,
                                                         nsmDevice});
    }
}

} // namespace nsm
// namespace nsm
