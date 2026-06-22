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

#include "nsmSoCPowerSmoothing.hpp"

#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstring>

namespace nsm
{

static const std::string socFeaturesIntfName =
    "com.nvidia.PowerSmoothing.StateOfChargeFeatures";

static void registerMaxACPowerRampRate(
    std::shared_ptr<sdbusplus::asio::dbus_interface> intf)
{
    intf->register_property("MaxACPowerRampRateWattsPerSecond",
                            uint32_t{std::numeric_limits<uint32_t>::max()});
}

static void updateMaxACPowerRampRate(
    std::shared_ptr<sdbusplus::asio::dbus_interface> intf, const uint8_t* data,
    uint16_t dataLen)
{
    if (dataLen < sizeof(uint32_t))
    {
        return;
    }
    uint32_t raw;
    std::memcpy(&raw, data, sizeof(uint32_t));
    raw = le32toh(raw);
    intf->set_property("MaxACPowerRampRateWattsPerSecond", raw);
}

static void registerPowerSmoothingEnabled(
    std::shared_ptr<sdbusplus::asio::dbus_interface> intf)
{
    intf->register_property("PowerSmoothingEnabled", bool{false});
}

static void updatePowerSmoothingEnabled(
    std::shared_ptr<sdbusplus::asio::dbus_interface> intf, const uint8_t* data,
    uint16_t dataLen)
{
    if (dataLen < sizeof(uint8_t))
    {
        return;
    }
    intf->set_property("PowerSmoothingEnabled", data[0] != 0);
}

static void
    registerProfileName(std::shared_ptr<sdbusplus::asio::dbus_interface> intf)
{
    intf->register_property("ProfileName", std::string{});
    intf->register_property("AvailableProfileNames",
                            std::vector<std::string>{});
}

static void
    updateProfileName(std::shared_ptr<sdbusplus::asio::dbus_interface> intf,
                      const uint8_t* data, uint16_t dataLen)
{
    if (dataLen < 2)
    {
        return;
    }
    intf->set_property("ProfileName", std::to_string(data[0]));

    std::vector<std::string> availableNames;
    for (uint8_t bit = 0; bit < 8; ++bit)
    {
        if (data[1] & (1 << bit))
        {
            availableNames.push_back(std::to_string(bit));
        }
    }
    intf->set_property("AvailableProfileNames", availableNames);
}

static void registerPowerBrakeEnabled(
    std::shared_ptr<sdbusplus::asio::dbus_interface> intf)
{
    intf->register_property("PowerBrakeEnabled", bool{false});
}

static void updatePowerBrakeEnabled(
    std::shared_ptr<sdbusplus::asio::dbus_interface> intf, const uint8_t* data,
    uint16_t dataLen)
{
    if (dataLen < sizeof(uint8_t))
    {
        return;
    }
    intf->set_property("PowerBrakeEnabled", data[0] != 0);
}

static const SoCModePropertyMap gpuV1ModePropertyMap = {
    {DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE,
     {{"MaxACPowerRampRateWattsPerSecond", registerMaxACPowerRampRate,
       updateMaxACPowerRampRate}}},
    {DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED,
     {{"PowerSmoothingEnabled", registerPowerSmoothingEnabled,
       updatePowerSmoothingEnabled}}},
};

static const SoCModePropertyMap mcuV1ModePropertyMap = {
    {DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE,
     {{"MaxACPowerRampRateWattsPerSecond", registerMaxACPowerRampRate,
       updateMaxACPowerRampRate}}},
    {DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED,
     {{"PowerSmoothingEnabled", registerPowerSmoothingEnabled,
       updatePowerSmoothingEnabled}}},
    {DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX,
     {{"ProfileName", registerProfileName, updateProfileName}}},
    {DEVICE_MODE_SOC_POWER_BRAKE_ENABLED,
     {{"PowerBrakeEnabled", registerPowerBrakeEnabled,
       updatePowerBrakeEnabled}}},
};

const SoCModePropertyMap& getSoCModePropertyMap(SoCDeviceType deviceType,
                                                uint32_t version)
{
    static const SoCModePropertyMap emptyMap{};
    const SoCModePropertyMap* map = &emptyMap;
    if (version == 1)
    {
        if (deviceType == SoCDeviceType::GPU)
        {
            map = &gpuV1ModePropertyMap;
        }
        else
        {
            map = &mcuV1ModePropertyMap;
        }
    }
    return *map;
}

NsmSoCPowerSmoothing::NsmSoCPowerSmoothing(
    const std::string& name, const std::string& type,
    const std::string& inventoryObjPath,
    std::shared_ptr<sdbusplus::asio::dbus_interface> socIntf,
    uint32_t deviceModeIndex, SoCMemberUpdater updater) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath), socIntf(socIntf),
    deviceModeIndex(deviceModeIndex), updater(updater)
{}

std::optional<std::vector<uint8_t>>
    NsmSoCPowerSmoothing::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, deviceModeIndex, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmSoCPowerSmoothing encode_get_device_mode_settings_v2_req failed. eid={EID} rc={RC} modeIndex={IDX}",
            "EID", eid, "RC", rc, "IDX", deviceModeIndex);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmSoCPowerSmoothing::handleResponseMsg(const struct nsm_msg* responseMsg,
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
        if (updater && socIntf && currentModeLength > 0)
        {
            updater(currentModeData, currentModeLength);
        }
    }
    else
    {
        if (shouldLog(
                "NsmSoCPowerSmoothing decode_get_device_mode_settings_v2_resp "
                "objPath=" +
                    inventoryObjPath + " modeIndex=" +
                    std::to_string(deviceModeIndex) + " name=" + getName(),
                reasonCode, cc, rc))
        {
            lg2::error(
                "NsmSoCPowerSmoothing decode_get_device_mode_settings_v2_resp failure | name: {NAME}, reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, modeIndex: {IDX}",
                "NAME", getName(), "REASONCODE", reasonCode, "CC", cc, "RC", rc,
                "IDX", deviceModeIndex);
        }
    }
    return cc ? cc : rc;
}

requester::Coroutine
    NsmSoCPowerSmoothing::setSoCSetting(const AsyncSetOperationValueType& value,
                                        AsyncOperationStatusType* status,
                                        std::shared_ptr<NsmDevice> nsmDevice)
{
    auto eid = nsmDevice->getEid();
    lg2::info("init setSoCSetting: eid={EID} modeIndex={IDX}", "EID", eid,
              "IDX", deviceModeIndex);
    std::vector<uint8_t> payload;

    switch (deviceModeIndex)
    {
        case DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE:
        {
            const auto* u32Val = std::get_if<uint32_t>(&value);
            if (!u32Val)
            {
                lg2::error(
                    "setSoCSetting: invalid value type for modeIndex={IDX} eid={EID}",
                    "IDX", deviceModeIndex, "EID", eid);
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            lg2::info("setSoCSetting: eid={EID} modeIndex={IDX} value={VAL}",
                      "EID", eid, "IDX", deviceModeIndex, "VAL", *u32Val);

            uint32_t raw = htole32(*u32Val);
            payload.assign(reinterpret_cast<const uint8_t*>(&raw),
                           reinterpret_cast<const uint8_t*>(&raw) +
                               sizeof(raw));
            break;
        }
        case DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED:
        {
            const auto* boolVal = std::get_if<bool>(&value);
            if (!boolVal)
            {
                lg2::error(
                    "setSoCSetting: invalid value type for modeIndex={IDX} eid={EID}",
                    "IDX", deviceModeIndex, "EID", eid);
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            lg2::info("setSoCSetting: eid={EID} modeIndex={IDX} value={VAL}",
                      "EID", eid, "IDX", deviceModeIndex, "VAL", *boolVal);
            uint8_t v = *boolVal ? 1 : 0;
            payload.assign(&v, &v + sizeof(v));
            break;
        }
        case DEVICE_MODE_SOC_POWER_BRAKE_ENABLED:
        {
            const auto* boolVal = std::get_if<bool>(&value);
            if (!boolVal)
            {
                lg2::error(
                    "setSoCSetting: invalid value type for modeIndex={IDX} eid={EID}",
                    "IDX", deviceModeIndex, "EID", eid);
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            lg2::info("setSoCSetting: eid={EID} modeIndex={IDX} value={VAL}",
                      "EID", eid, "IDX", deviceModeIndex, "VAL", *boolVal);
            uint8_t v = *boolVal ? 1 : 0;
            payload.assign(&v, &v + sizeof(v));
            break;
        }
        case DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX:
        {
            const auto* strVal = std::get_if<std::string>(&value);
            if (!strVal)
            {
                lg2::error(
                    "setSoCSetting: invalid value type for modeIndex={IDX} eid={EID}",
                    "IDX", deviceModeIndex, "EID", eid);
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            lg2::info("setSoCSetting: eid={EID} modeIndex={IDX} value={VAL}",
                      "EID", eid, "IDX", deviceModeIndex, "VAL", *strVal);
            try
            {
                auto parsed = std::stoul(*strVal);
                if (parsed > std::numeric_limits<uint8_t>::max())
                {
                    throw std::out_of_range("value exceeds uint8_t range");
                }
                uint8_t v = static_cast<uint8_t>(parsed);
                payload.assign(&v, &v + sizeof(v));
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "setSoCSetting: invalid preset index string for modeIndex={IDX} eid={EID}: {ERR}",
                    "IDX", deviceModeIndex, "EID", eid, "ERR", e.what());
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            break;
        }
        default:
            lg2::error("setSoCSetting: unsupported modeIndex={IDX} eid={EID}",
                       "IDX", deviceModeIndex, "EID", eid);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_DATA;
    }

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_mode_settings_v2_req) +
                    payload.size() - 1);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_device_mode_settings_v2_req(
        0, deviceModeIndex, payload.data(),
        static_cast<uint16_t>(payload.size()), requestMsg);

    if (rc)
    {
        lg2::error(
            "setSoCSetting encode_set_device_mode_settings_v2_req failed for eid={EID} modeIndex={IDX}, rc={RC}",
            "EID", eid, "IDX", deviceModeIndex, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->postPatchIO(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "setSoCSetting postPatchIO failed for eid={EID} modeIndex={IDX}, rc={RC}",
            "EID", eid, "IDX", deviceModeIndex, "RC", rc);
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
            "setSoCSetting decode_set_device_mode_settings_v2_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, modeIndex: {IDX}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "IDX",
            deviceModeIndex);
    }
    else
    {
        lg2::info("setSoCSetting success for eid={EID} modeIndex={IDX}", "EID",
                  nsmDevice->getEid(), "IDX", deviceModeIndex);
    }

    *status = (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
                  ? AsyncOperationStatusType::Success
                  : AsyncOperationStatusType::WriteFailure;
    co_return cc ? cc : rc;
}

void createSoCPowerSmoothing(std::shared_ptr<NsmDevice> nsmDevice,
                             sdbusplus::bus_t& /* bus */,
                             const std::string& name, const std::string& type,
                             const std::string& inventoryObjPath,
                             SoCDeviceType deviceType, uint32_t version,
                             bool priority)
{
    const auto& propertyMap = getSoCModePropertyMap(deviceType, version);

    auto socIntf = SensorManager::getInstance().getObjServer().add_interface(
        inventoryObjPath, socFeaturesIntfName);

    if (!socIntf)
    {
        lg2::error(
            "createSoCPowerSmoothing: failed to create interface on {PATH}",
            "PATH", inventoryObjPath);
        return;
    }

    for (const auto& [modeIndex, propInfos] : propertyMap)
    {
        for (const auto& propInfo : propInfos)
        {
            propInfo.registrar(socIntf);
        }
    }

    socIntf->initialize();

    for (const auto& [modeIndex, propInfos] : propertyMap)
    {
        for (const auto& propInfo : propInfos)
        {
            auto boundUpdater = [socIntf, rawUpdater = propInfo.updater](
                                    const uint8_t* data, uint16_t dataLen) {
                rawUpdater(socIntf, data, dataLen);
            };
            auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
                name, type, inventoryObjPath, socIntf, modeIndex,
                std::move(boundUpdater));

            nsmDevice->addSensor(sensor, priority);

            AsyncOperationManager::getInstance()
                ->getDispatcher(inventoryObjPath)
                ->addAsyncSetOperation(
                    socFeaturesIntfName, propInfo.propertyName,
                    AsyncSetOperationInfo{
                        std::bind_front(&NsmSoCPowerSmoothing::setSoCSetting,
                                        sensor),
                        sensor, nsmDevice});
        }
    }
}

} // namespace nsm
