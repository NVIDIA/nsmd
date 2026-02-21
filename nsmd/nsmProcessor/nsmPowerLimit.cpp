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
#include <tuple>
#include <unordered_map>
#include <vector>

namespace nsm
{

const std::unordered_map<uint32_t, std::string_view> deviceModeIndexToName = {
    {DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT,
     "GPU_Base_Power_Limit_One_Shot"},
    {DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT,
     "GPU_Base_Power_Limit_Persistent"},
    {DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY,
     "CPU_Limit_GPU_Copy_Power_Limit_One_Shot"},
    {DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY,
     "CPU_Limit_GPU_Copy_Power_Limit_Persistent"},
};

NsmClearPowerLimitIntf ::NsmClearPowerLimitIntf(
    sdbusplus::bus::bus& bus, const std::string& inventoryObjPath) :
    ClearPowerLimitIntf(bus, inventoryObjPath.c_str())
{}

// Function overrided with dummy implementation to use the ClerPowerLimitIntf
int32_t NsmClearPowerLimitIntf::clearPowerCap()
{
    return 0;
}

NsmPersistentPowerLimit::NsmPersistentPowerLimit(
    const std::string& name, const std::string& type,
    std::shared_ptr<PowerLimitsIntf> powerLimitsIntf,
    std::shared_ptr<NsmClearPowerLimitIntf> clearPowerLimitIntf,
    std::shared_ptr<AssociationDefinitionsIntf> associationDefinitionsIntf,
    std::shared_ptr<NsmDevice> nsmDevice, uint8_t powerLimitId,
    std::shared_ptr<PowerPersistencyIntf> persistencyIntf) :
    NsmSensor(name, type), powerLimitsIntf(powerLimitsIntf),
    clearPowerLimitIntf(clearPowerLimitIntf),
    associationDefinitionsIntf(associationDefinitionsIntf),
    nsmDevice(nsmDevice), persistencyIntf(persistencyIntf),
    powerLimitId(powerLimitId)
{
    powerLimitsIntf->powerCapEnable(true);
}

static uint32_t powerLimitIdToDeviceModeIndex(uint8_t powerLimitId,
                                              bool persistent)
{
    switch (powerLimitId)
    {
        case GPU_BASE:
            return persistent ? DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT
                              : DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT;
        case CPU_LIMIT_GPU_COPY:
            return persistent ? DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY
                              : DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY;
        default:
            lg2::error(
                "powerLimitIdToDeviceModeIndex: unknown powerLimitId={ID}, using 0",
                "ID", powerLimitId);
            return 0;
    }
}

std::optional<std::vector<uint8_t>>
    NsmPersistentPowerLimit::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    uint32_t deviceModeIndex = powerLimitIdToDeviceModeIndex(powerLimitId,
                                                             true);
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, deviceModeIndex, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_device_mode_settings_v2_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmPersistentPowerLimit::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint8_t currentModeData[sizeof(uint32_t)];
    uint16_t currentModeLength = 0;
    uint8_t pendingModeData[sizeof(uint32_t)];
    uint16_t pendingModeLength = 0;

    auto rc = decode_get_device_mode_settings_v2_resp(
        responseMsg, responseLen, &cc, &reasonCode, currentModeData,
        &currentModeLength, pendingModeData, &pendingModeLength);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        if (currentModeLength == sizeof(uint32_t))
        {
            uint32_t currentLimit;
            memcpy(&currentLimit, currentModeData, sizeof(uint32_t));
            currentLimit = le32toh(currentLimit);
            uint32_t reading =
                (currentLimit == INVALID_POWER_LIMIT) ? 0 : currentLimit / 1000;
            powerLimitsIntf->powerCap(reading);

            if (persistencyIntf)
            {
                persistencyIntf->persistentPowerLimit(
                    static_cast<double>(reading));

                if (static_cast<double>(powerLimitsIntf->powerCap()) ==
                    persistencyIntf->persistentPowerLimit())
                {
                    persistencyIntf->persistency(true);
                }
                else
                {
                    persistencyIntf->persistency(false);
                }
            }
        }
        else
        {
            if (shouldLog(
                    "NsmPersistentPowerLimit handleResponseMsg " + getName(),
                    reasonCode, cc, rc, currentModeLength != sizeof(uint32_t)))
            {
                lg2::error(
                    "handleResponseMsg unexpected currentModeLength={LENGTH}, expected={EXPECTED}",
                    "LENGTH", currentModeLength, "EXPECTED", sizeof(uint32_t));
            }
        }

        if (pendingModeLength == sizeof(uint32_t))
        {
            uint32_t pendingLimit;
            memcpy(&pendingLimit, pendingModeData, sizeof(uint32_t));
            pendingLimit = le32toh(pendingLimit);
            pendingPowerLimit =
                (pendingLimit == INVALID_POWER_LIMIT)
                    ? std::nullopt
                    : std::optional<uint32_t>(pendingLimit / 1000);
        }
        else
        {
            pendingPowerLimit = std::nullopt;
        }
    }
    else
    {
        if (shouldLog(
                "NsmPersistentPowerLimit decode_get_device_mode_settings_v2_resp " +
                    getName(),
                reasonCode, cc, rc))
        {
            lg2::error(
                "decode_get_device_mode_settings_v2_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        }
    }
    return cc ? cc : rc;
}

void NsmPersistentPowerLimit::handleOfflineState()
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

requester::Coroutine NsmPersistentPowerLimit::setPowerLimit(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    [[maybe_unused]] std::shared_ptr<NsmDevice> device)
{
    bool persistent = true;
    uint32_t powerLimit = 0;

    if (const auto* powerLimitTuple =
            std::get_if<std::tuple<bool, uint32_t>>(&value))
    {
        persistent = std::get<0>(*powerLimitTuple);
        powerLimit = std::get<1>(*powerLimitTuple);
    }
    else if (const auto* powerLimitValue = std::get_if<uint32_t>(&value))
    {
        powerLimit = *powerLimitValue;
    }
    else
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    auto rc = co_await updatePowerLimit(status, nsmDevice, persistent,
                                        powerLimit);
    co_return rc;
}

requester::Coroutine NsmPersistentPowerLimit::updatePowerLimit(
    AsyncOperationStatusType* status, std::shared_ptr<NsmDevice> nsmDevice,
    const bool persistent, const uint32_t value)
{
    auto eid = nsmDevice->getEid();
    uint32_t powerLimitValue = htole32(value * 1000);
    uint32_t deviceModeIndex = powerLimitIdToDeviceModeIndex(powerLimitId,
                                                             persistent);

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_mode_settings_v2_req) +
                    sizeof(powerLimitValue) - 1);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_settings_v2_req(
        0, deviceModeIndex, reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(uint32_t), requestMsg);

    if (rc)
    {
        lg2::error(
            "updatePowerLimit encode_set_device_mode_settings_v2_req failed for eid = {EID} and powerLimitId={POWER_LIMIT_ID}, rc={RC}",
            "EID", eid, "POWER_LIMIT_ID", powerLimitId, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    lg2::info(
        "update Power Limit for eid = {EID} and deviceModeIndex={DEVICE_MODE_INDEX} setPowerLimit={POWER_LIMIT}W persistent={PERSISTENT}",
        "EID", eid, "DEVICE_MODE_INDEX",
        deviceModeIndexToName.at(deviceModeIndex), "POWER_LIMIT", value,
        "PERSISTENT", persistent);
    rc = co_await nsmDevice->postPatchIO(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "updatePowerLimit postPatchIO failed for eid = {EID} and deviceModeIndex={DEVICE_MODE_INDEX}, rc = {RC}",
            "EID", eid, "DEVICE_MODE_INDEX",
            deviceModeIndexToName.at(deviceModeIndex), "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return rc;
    }

    uint16_t reasonCode = ERR_NULL;
    uint8_t cc = NSM_SUCCESS;
    rc = decode_set_device_mode_settings_v2_resp(responseMsg.get(), responseLen,
                                                 &cc, &reasonCode);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_set_device_mode_settings_v2_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }
    else
    {
        lg2::info(
            "updatePowerLimit success for eid = {EID} and deviceModeIndex={DEVICE_MODE_INDEX} powerLimit={POWER_LIMIT}W",
            "EID", eid, "DEVICE_MODE_INDEX",
            deviceModeIndexToName.at(deviceModeIndex), "POWER_LIMIT", value);
    }

    *status = (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
                  ? AsyncOperationStatusType::Success
                  : AsyncOperationStatusType::WriteFailure;
    co_return cc ? cc : rc;
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
        if (shouldLog("NsmPowerLimitRange SendRecvNsmMsg " + propertyName +
                          " eid=" + std::to_string(nsmDevice->getEid()),
                      uint16_t(0), uint8_t(0), rc))
        {
            lg2::error(
                "NsmPowerLimitRange SendRecvNsmMsg failed with RC={RC}, eid={EID}, property = {PROPERTY}",
                "RC", rc, "EID", nsmDevice->getEid(), "PROPERTY", propertyName);
        }
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
                      propertyName +
                      " eid=" + std::to_string(nsmDevice->getEid()),
                  reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        lg2::error(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}, eid: {EID}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize,
            "EID", nsmDevice->getEid());
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
        if (shouldLog("NsmDefaultPowerLimit sensorIO " + propertyName +
                          " eid=" + std::to_string(nsmDevice->getEid()),
                      uint16_t(0), uint8_t(0), rc))
        {
            lg2::error(
                "NsmDefaultPowerLimit sensorIO failed with RC={RC}, eid={EID}, property = {PROPERTY}",
                "RC", rc, "EID", nsmDevice->getEid(), "PROPERTY", propertyName);
        }
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
                propertyName + " eid=" + std::to_string(nsmDevice->getEid()),
            reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        lg2::error(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}, property: {PROPERTY}, eid: {EID}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize,
            "PROPERTY", propertyName, "EID", nsmDevice->getEid());
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

NsmOneShotPowerLimit::NsmOneShotPowerLimit(
    const std::string& name, const std::string& type, uint8_t powerLimitId,
    std::shared_ptr<PowerPersistencyIntf> persistencyIntf,
    std::shared_ptr<PowerLimitsIntf> powerLimitsIntf) :
    NsmSensor(name, type), powerLimitId(powerLimitId),
    persistencyIntf(persistencyIntf), powerLimitsIntf(powerLimitsIntf)
{}

std::optional<std::vector<uint8_t>>
    NsmOneShotPowerLimit::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    uint32_t deviceModeIndex = powerLimitIdToDeviceModeIndex(powerLimitId,
                                                             false);
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, deviceModeIndex, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmOneShotPowerLimit encode_get_device_mode_settings_v2_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmOneShotPowerLimit::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint8_t currentModeData[sizeof(uint32_t)];
    uint16_t currentModeLength = 0;
    uint8_t pendingModeData[sizeof(uint32_t)];
    uint16_t pendingModeLength = 0;

    auto rc = decode_get_device_mode_settings_v2_resp(
        responseMsg, responseLen, &cc, &reasonCode, currentModeData,
        &currentModeLength, pendingModeData, &pendingModeLength);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        if (currentModeLength == sizeof(uint32_t))
        {
            uint32_t currentLimit;
            memcpy(&currentLimit, currentModeData, sizeof(uint32_t));
            currentLimit = le32toh(currentLimit);
            uint32_t reading =
                (currentLimit == INVALID_POWER_LIMIT) ? 0 : currentLimit / 1000;

            if (persistencyIntf)
            {
                persistencyIntf->oneShotPowerLimit(
                    static_cast<double>(reading));

                if (static_cast<double>(powerLimitsIntf->powerCap()) ==
                    persistencyIntf->persistentPowerLimit())
                {
                    persistencyIntf->persistency(true);
                }
                else
                {
                    persistencyIntf->persistency(false);
                }
            }
        }
        else
        {
            if (shouldLog("NsmOneShotPowerLimit handleResponseMsg " + getName(),
                          reasonCode, cc, rc,
                          currentModeLength != sizeof(uint32_t)))
            {
                lg2::error(
                    "NsmOneShotPowerLimit handleResponseMsg unexpected currentModeLength={LENGTH}, expected={EXPECTED}",
                    "LENGTH", currentModeLength, "EXPECTED", sizeof(uint32_t));
            }
        }
    }
    else
    {
        if (shouldLog(
                "NsmOneShotPowerLimit decode_get_device_mode_settings_v2_resp " +
                    getName(),
                reasonCode, cc, rc))
        {
            lg2::error(
                "NsmOneShotPowerLimit decode_get_device_mode_settings_v2_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        }
    }
    return cc ? cc : rc;
}

void createGPUPowerLimit(std::shared_ptr<NsmDevice> nsmDevice,
                         sdbusplus::bus::bus& bus, const std::string& name,
                         const std::string& type,
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

        auto persistencyIntf =
            std::make_shared<PowerPersistencyIntf>(bus, objPath.c_str());
        persistencyIntf->persistency(false);
        persistencyIntf->persistentPowerLimit(std::nan(""));
        persistencyIntf->oneShotPowerLimit(std::nan(""));

        std::vector<std::tuple<std::string, std::string, std::string>>
            associations = {{"", "Base_Power_Limit", inventoryObjPath}};
        associationDefinitionsIntf->associations(associations);
        auto nsmPowerLimitSensor = std::make_shared<NsmPersistentPowerLimit>(
            name, type, powerLimitsIntf, clearPowerLimitIntf,
            associationDefinitionsIntf, nsmDevice, GPU_BASE, persistencyIntf);
        nsmDevice->addSensor(nsmPowerLimitSensor, false);
        nsmDevice->addCapabilityRefreshSensor(nsmPowerLimitSensor);

        auto nsmOneShotSensor = std::make_shared<NsmOneShotPowerLimit>(
            name + "_NSM_GPU_BASE_POWER_LIMIT" + "_OneShot", type, GPU_BASE,
            persistencyIntf, powerLimitsIntf);
        nsmDevice->addSensor(nsmOneShotSensor, false);

        nsm::AsyncSetOperationHandler setPowerLimitHandler =
            std::bind(&NsmPersistentPowerLimit::setPowerLimit,
                      nsmPowerLimitSensor, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
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
        auto associationDefinitionsIntf =
            std::make_shared<AssociationDefinitionsIntf>(bus, objPath.c_str());

        auto persistencyIntf =
            std::make_shared<PowerPersistencyIntf>(bus, objPath.c_str());
        persistencyIntf->persistency(false);
        persistencyIntf->persistentPowerLimit(std::nan(""));
        persistencyIntf->oneShotPowerLimit(std::nan(""));

        std::vector<std::tuple<std::string, std::string, std::string>>
            associations = {{"Cpu_Power_copied_by_GPU", "GPU_copy_Cpu_Power",
                             inventoryObjPath}};
        associationDefinitionsIntf->associations(associations);
        auto nsmPowerLimitSensor = std::make_shared<NsmPersistentPowerLimit>(
            name, type, powerLimitsIntf, nullptr, associationDefinitionsIntf,
            nsmDevice, CPU_LIMIT_GPU_COPY, persistencyIntf);
        nsmDevice->addSensor(nsmPowerLimitSensor, false);
        nsmDevice->addCapabilityRefreshSensor(nsmPowerLimitSensor);

        auto nsmOneShotSensor = std::make_shared<NsmOneShotPowerLimit>(
            name + "_OneShot", type, CPU_LIMIT_GPU_COPY, persistencyIntf,
            powerLimitsIntf);
        nsmDevice->addSensor(nsmOneShotSensor, false);

        nsm::AsyncSetOperationHandler setPowerLimitHandler =
            std::bind(&NsmPersistentPowerLimit::setPowerLimit,
                      nsmPowerLimitSensor, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objPath)
            ->addAsyncSetOperation(powerLimitsIntf->interface, "PowerCap",
                                   AsyncSetOperationInfo{setPowerLimitHandler,
                                                         nsmPowerLimitSensor,
                                                         nsmDevice});
    }
}

} // namespace nsm
