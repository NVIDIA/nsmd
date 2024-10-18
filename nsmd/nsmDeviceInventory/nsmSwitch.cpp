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

#include "nsmSwitch.hpp"

#include "network-ports.h"

#include "asyncOperationManager.hpp"
#include "dBusAsyncUtils.hpp"
#include "deviceManager.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmDebugInfo.hpp"
#include "nsmDebugToken.hpp"
#include "nsmDevice.hpp"
#include "nsmEraseTrace.hpp"
#include "nsmErrorInjection.hpp"
#include "nsmErrorInjectionCommon.hpp"
#include "nsmEvent/nsmFabricManagerStateEvent.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmLogInfo.hpp"
#include "nsmManagers/nsmFabricManager.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPort/nsmPortDisableFuture.hpp"
#include "sharedMemCommon.hpp"
#include "utils.hpp"
namespace nsm
{

NsmSwitchDIReset::NsmSwitchDIReset(sdbusplus::bus::bus& bus,
                                   const std::string& name,
                                   const std::string& type,
                                   std::string& inventoryObjPath,
                                   std::shared_ptr<NsmDevice> device) :
    NsmObject(name, type)
{
    lg2::info("NsmSwitchDIReset: create sensor:{NAME}", "NAME", name.c_str());

    objPath = inventoryObjPath + name;
    resetIntf = std::make_shared<NsmResetDeviceIntf>(bus, objPath.c_str());
    resetIntf->resetType(sdbusplus::common::xyz::openbmc_project::control::
                             Reset::ResetTypes::ForceRestart);
    resetAsyncIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, objPath.c_str(), device);
}

template <typename IntfType>
requester::Coroutine NsmSwitchDI<IntfType>::update(SensorManager& manager,
                                                   eid_t eid)
{
    DeviceManager& deviceManager = DeviceManager::getInstance();
    auto uuid = utils::getUUIDFromEID(deviceManager.getEidTable(), eid);
    if (uuid)
    {
        if constexpr (std::is_same_v<IntfType, UuidIntf>)
        {
            auto nsmDevice = manager.getNsmDevice(*uuid);
            if (nsmDevice)
            {
                this->pdi().uuid(nsmDevice->deviceUuid);
            }
        }
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

struct nsm_power_mode_data NsmSwitchDIPowerMode::getPowerModeData()
{
    struct nsm_power_mode_data powerModeData;
    powerModeData.l1_hw_mode_control = this->pdi().hwModeControl();
    powerModeData.l1_hw_mode_threshold =
        static_cast<uint32_t>(this->pdi().hwThreshold());
    powerModeData.l1_fw_throttling_mode = this->pdi().fwThrottlingMode();
    powerModeData.l1_prediction_mode = this->pdi().predictionMode();
    powerModeData.l1_hw_active_time =
        static_cast<uint16_t>(this->pdi().hwActiveTime());
    powerModeData.l1_hw_inactive_time =
        static_cast<uint16_t>(this->pdi().hwInactiveTime());
    powerModeData.l1_prediction_inactive_time =
        static_cast<uint16_t>(this->pdi().hwPredictionInactiveTime());

    return powerModeData;
}

requester::Coroutine NsmSwitchDIPowerMode::update(SensorManager& manager,
                                                  eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_power_mode_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_power_mode_req failed. eid={EID} rc={RC}", "EID",
                   eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    struct nsm_power_mode_data data;

    rc = decode_get_power_mode_resp(responseMsg.get(), responseLen, &cc,
                                    &dataSize, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // update values
        (data.l1_hw_mode_control == 1) ? this->pdi().hwModeControl(true)
                                       : this->pdi().hwModeControl(false);
        this->pdi().hwThreshold(
            static_cast<uint64_t>(data.l1_hw_mode_threshold));
        (data.l1_fw_throttling_mode == 1) ? this->pdi().fwThrottlingMode(true)
                                          : this->pdi().fwThrottlingMode(false);
        (data.l1_prediction_mode == 1) ? this->pdi().predictionMode(true)
                                       : this->pdi().predictionMode(false);
        this->pdi().hwActiveTime(static_cast<uint64_t>(data.l1_hw_active_time));
        this->pdi().hwInactiveTime(
            static_cast<uint64_t>(data.l1_hw_inactive_time));
        this->pdi().hwPredictionInactiveTime(
            static_cast<uint64_t>(data.l1_prediction_inactive_time));
        clearErrorBitMap("decode_get_power_mode_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_power_mode_resp", reasonCode, cc, rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1PowerDevice(
    struct nsm_power_mode_data data,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("setL1PowerDevice for EID: {EID}", "EID", eid);

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_power_mode_req(0, requestMsg, data);

    if (rc)
    {
        lg2::error(
            "setL1PowerDevice encode_set_power_mode_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        lg2::error(
            "setL1PowerDevice SendRecvNsmMsgSync failed for while setting PowerMode "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_set_power_mode_resp(responseMsg.get(), responseLen, &cc,
                                    &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info("setL1PowerDevice for EID: {EID} completed", "EID", eid);
    }
    else
    {
        lg2::error(
            "setL1PowerDevice decode_set_power_mode_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        lg2::error("throwing write failure exception");
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1HWModeControl(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* l1HWModeControl = std::get_if<bool>(&value);

    if (!l1HWModeControl)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    if (*l1HWModeControl)
    {
        l1PowerModeData.l1_hw_mode_control = 1;
    }
    else
    {
        l1PowerModeData.l1_hw_mode_control = 0;
    }

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1FWThrottlingMode(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* l1FWThrottlingMode = std::get_if<bool>(&value);

    if (!l1FWThrottlingMode)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    if (*l1FWThrottlingMode)
    {
        l1PowerModeData.l1_fw_throttling_mode = 1;
    }
    else
    {
        l1PowerModeData.l1_fw_throttling_mode = 0;
    }

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1PredictionMode(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* l1PredictionMode = std::get_if<bool>(&value);

    if (!l1PredictionMode)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    if (*l1PredictionMode)
    {
        l1PowerModeData.l1_prediction_mode = 1;
    }
    else
    {
        l1PowerModeData.l1_prediction_mode = 0;
    }

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1HWThreshold(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const uint32_t* l1HWThreshold = std::get_if<uint32_t>(&value);

    if (!l1HWThreshold)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    l1PowerModeData.l1_hw_mode_threshold =
        static_cast<uint64_t>(*l1HWThreshold);

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1HWActiveTime(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const uint32_t* l1HWActiveTime = std::get_if<uint32_t>(&value);

    if (!l1HWActiveTime)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    l1PowerModeData.l1_hw_active_time = static_cast<uint64_t>(*l1HWActiveTime);

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1HWInactiveTime(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const uint32_t* l1HWInactiveTime = std::get_if<uint32_t>(&value);

    if (!l1HWInactiveTime)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    l1PowerModeData.l1_hw_inactive_time =
        static_cast<uint64_t>(*l1HWInactiveTime);

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmSwitchDIPowerMode::setL1HWPredictionInactiveTime(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const uint32_t* l1HWPredictionInactiveTime = std::get_if<uint32_t>(&value);

    if (!l1HWPredictionInactiveTime)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    auto l1PowerModeData = getPowerModeData();
    l1PowerModeData.l1_prediction_inactive_time =
        static_cast<uint64_t>(*l1HWPredictionInactiveTime);

    const auto rc = co_await setL1PowerDevice(l1PowerModeData, status, device);
    asyncPatchInProgress = false;
    // coverity[missing_return]
    co_return rc;
}

NsmSwitchIsolationMode::NsmSwitchIsolationMode(
    const std::string& name, const std::string& type,
    std::shared_ptr<SwitchIsolationIntf> switchIsolationIntf) :
    NsmSensor(name, type),
    switchIsolationIntf(switchIsolationIntf)
{}

std::optional<std::vector<uint8_t>>
    NsmSwitchIsolationMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_switch_isolation_mode_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_switch_isolation_mode_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmSwitchIsolationMode::handleResponseMsg(const struct nsm_msg* responseMsg,
                                              size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint8_t isolationMode;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_switch_isolation_mode_resp(
        responseMsg, responseLen, &cc, &reason_code, &isolationMode);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        if (isolationMode == SWITCH_COMMUNICATION_MODE_ENABLED)
        {
            switchIsolationIntf->isolationMode(
                SwitchCommunicationMode::SwitchCommunicationEnabled);
        }
        else if (isolationMode == SWITCH_COMMUNICATION_MODE_DISABLED)
        {
            switchIsolationIntf->isolationMode(
                SwitchCommunicationMode::SwitchCommunicationDisabled);
        }
        else
        {
            switchIsolationIntf->isolationMode(
                SwitchCommunicationMode::SwitchCommunicationUnknown);
        }
        clearErrorBitMap("decode_get_switch_isolation_mode_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_switch_isolation_mode_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

requester::Coroutine NsmSwitchIsolationMode::setSwitchIsolationMode(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const std::string* reqIsolationMode = std::get_if<std::string>(&value);
    if (reqIsolationMode == NULL)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("set Switch Isolation Mode On Device for EID: {EID}", "EID", eid);

    uint8_t isolationMode;
    if (*reqIsolationMode == "SwitchCommunicationEnabled")
    {
        isolationMode = SWITCH_COMMUNICATION_MODE_ENABLED;
    }
    else if (*reqIsolationMode == "SwitchCommunicationDisabled")
    {
        isolationMode = SWITCH_COMMUNICATION_MODE_DISABLED;
    }
    else
    {
        lg2::error(
            "NsmSwitchIsolationMode::setSwitchIsolationMode invalid isolation mode {MODE}",
            "MODE", *reqIsolationMode);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_DATA;
    }
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_switch_isolation_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_switch_isolation_mode_req(0, isolationMode,
                                                   requestMsg);

    if (rc)
    {
        lg2::error(
            "NsmSwitchIsolationMode::setSwitchIsolationMode encode_set_switch_isolation_mode_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        lg2::error(
            "NsmSwitchIsolationMode::setSwitchIsolationMode SendRecvNsmMsgSync failed for"
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_set_switch_isolation_mode_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info(
            "NsmSwitchIsolationMode::setSwitchIsolationMode for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        lg2::error(
            "NsmSwitchIsolationMode::setSwitchIsolationMode decode_set_switch_isolation_mode_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine createNsmSwitchDI(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch";

    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", baseInterface.c_str());
    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == "NSM_NVSwitch")
    {
        auto nvSwitchIntf =
            std::make_shared<NsmSwitchDI<NvSwitchIntf>>(name, inventoryObjPath);
        auto nvSwitchUuid =
            std::make_shared<NsmSwitchDI<UuidIntf>>(name, inventoryObjPath);
        auto nvSwitchAssociation =
            std::make_shared<NsmSwitchDI<AssociationDefinitionsInft>>(
                name, inventoryObjPath);

        std::vector<utils::Association> associations{};
        co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                          associations);

        std::vector<std::tuple<std::string, std::string, std::string>>
            associations_list;
        for (const auto& association : associations)
        {
            associations_list.emplace_back(association.forward,
                                           association.backward,
                                           association.absolutePath);
        }
        nvSwitchAssociation->pdi().associations(associations_list);
        nvSwitchUuid->pdi().uuid(uuid);

        device->deviceSensors.emplace_back(nvSwitchIntf);
        device->addStaticSensor(nvSwitchUuid);
        device->addStaticSensor(nvSwitchAssociation);

        auto debugTokenObject = std::make_shared<NsmDebugTokenObject>(
            bus, name, associations, type, uuid);
        device->addStaticSensor(debugTokenObject);

        // NetIR dump for NVSwitch
        auto nvSwitchDebugInfoObject = std::make_shared<NsmDebugInfoObject>(
            bus, name, inventoryObjPath, type, uuid);
        device->addStaticSensor(nvSwitchDebugInfoObject);

        auto nvSwitchEraseTraceObject = std::make_shared<NsmEraseTraceObject>(
            bus, name, inventoryObjPath, type, uuid);
        device->addStaticSensor(nvSwitchEraseTraceObject);

        auto nvSwitchLogInfoObject = std::make_shared<NsmLogInfoObject>(
            bus, name, inventoryObjPath, type, uuid);
        device->addStaticSensor(nvSwitchLogInfoObject);

        // Device Reset for NVSwitch
        auto nvSwitchResetSensor = std::make_shared<NsmSwitchDIReset>(
            bus, name, type, inventoryObjPath, device);
        device->deviceSensors.push_back(nvSwitchResetSensor);

        createNsmErrorInjectionSensors(manager, device,
                                       path(inventoryObjPath) / name);

        std::string dbusObjPath = inventoryObjPath + name;
        auto isolationModeIntf =
            std::make_shared<SwitchIsolationIntf>(bus, dbusObjPath.c_str());
        auto isolationModeSensor = std::make_shared<NsmSwitchIsolationMode>(
            name, type, isolationModeIntf);
        device->addSensor(isolationModeSensor, false);

        nsm::AsyncSetOperationHandler setIsolationModeHandler =
            std::bind(&NsmSwitchIsolationMode::setSwitchIsolationMode,
                      isolationModeSensor, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(dbusObjPath)
            ->addAsyncSetOperation(
                "com.nvidia.SwitchIsolation", "IsolationMode",
                AsyncSetOperationInfo{setIsolationModeHandler,
                                      isolationModeSensor, device});
    }
    else if (type == "NSM_PortDisableFuture")
    {
        // Port disable future status on NVSwitch
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto nvSwitchPortDisableFuture =
            std::make_shared<nsm::NsmDevicePortDisableFuture>(name, type,
                                                              inventoryObjPath);

        nvSwitchPortDisableFuture->pdi().portDisableFuture(
            std::vector<uint8_t>{});
        device->addSensor(nvSwitchPortDisableFuture, priority);

        nsm::AsyncSetOperationHandler setPortDisableFutureHandler =
            std::bind(&NsmDevicePortDisableFuture::setPortDisableFuture,
                      nvSwitchPortDisableFuture, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);

        AsyncOperationManager::getInstance()
            ->getDispatcher(nvSwitchPortDisableFuture->getInventoryObjectPath())
            ->addAsyncSetOperation(
                "com.nvidia.NVLink.NVLinkDisableFuture", "PortDisableFuture",
                AsyncSetOperationInfo{setPortDisableFutureHandler,
                                      nvSwitchPortDisableFuture, device});
    }
    else if (type == "NSM_PowerMode")
    {
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto nvSwitchL1PowerMode =
            std::make_shared<NsmSwitchDIPowerMode>(name, inventoryObjPath);

        nvSwitchL1PowerMode->pdi().hwModeControl(false);
        nvSwitchL1PowerMode->pdi().hwThreshold(0);
        nvSwitchL1PowerMode->pdi().fwThrottlingMode(false);
        nvSwitchL1PowerMode->pdi().predictionMode(false);
        nvSwitchL1PowerMode->pdi().hwActiveTime(0);
        nvSwitchL1PowerMode->pdi().hwInactiveTime(0);
        nvSwitchL1PowerMode->pdi().hwPredictionInactiveTime(0);

        device->addSensor(nvSwitchL1PowerMode, priority);
        auto objectPath = nvSwitchL1PowerMode->getInventoryObjectPath();

        nsm::AsyncSetOperationHandler setL1HWModeControlHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1HWModeControl,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWModeControl",
                AsyncSetOperationInfo{setL1HWModeControlHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1FWThrottlingModeHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1FWThrottlingMode,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "FWThrottlingMode",
                AsyncSetOperationInfo{setL1FWThrottlingModeHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1PredictionModeHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1PredictionMode,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "PredictionMode",
                AsyncSetOperationInfo{setL1PredictionModeHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1HWThresholdHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1HWThreshold,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWThreshold",
                AsyncSetOperationInfo{setL1HWThresholdHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1HWActiveTimeHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1HWActiveTime,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWActiveTime",
                AsyncSetOperationInfo{setL1HWActiveTimeHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1HWInactiveTimeHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1HWInactiveTime,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWInactiveTime",
                AsyncSetOperationInfo{setL1HWInactiveTimeHandler,
                                      nvSwitchL1PowerMode, device});

        nsm::AsyncSetOperationHandler setL1HWPredictionInactiveTimeHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1HWPredictionInactiveTime,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWPredictionInactiveTime",
                AsyncSetOperationInfo{setL1HWPredictionInactiveTimeHandler,
                                      nvSwitchL1PowerMode, device});
    }
    else if (type == "NSM_Switch")
    {
        auto nvSwitchObject =
            std::make_shared<NsmSwitchDI<SwitchIntf>>(name, inventoryObjPath);
        auto switchType = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "SwitchType", interface.c_str());
        auto switchProtocols =
            co_await utils::coGetDbusProperty<std::vector<std::string>>(
                objPath.c_str(), "SwitchSupportedProtocols", interface.c_str());

        std::vector<SwitchIntf::SwitchType> supported_protocols;
        for (const auto& protocol : switchProtocols)
        {
            supported_protocols.emplace_back(
                SwitchIntf::convertSwitchTypeFromString(protocol));
        }
        nvSwitchObject->pdi().type(
            SwitchIntf::convertSwitchTypeFromString(switchType));
        nvSwitchObject->pdi().supportedProtocols(supported_protocols);

        // maxSpeed and currentSpeed from PLDM T2

        device->addSensor(nvSwitchObject, false);
    }
    else if (type == "NSM_Asset")
    {
        auto nvSwitchAsset =
            std::make_shared<NsmSwitchDI<NsmAssetIntf>>(name, inventoryObjPath);
        auto manufacturer = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());

        nvSwitchAsset->pdi().manufacturer(manufacturer);
        device->addStaticSensor(nvSwitchAsset);
    }
    else if (type == "NSM_FabricManager")
    {
        auto nameFM = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());
        auto inventoryObjPathFM =
            co_await utils::coGetDbusProperty<std::string>(
                objPath.c_str(), "InventoryObjPath", interface.c_str());
        auto description = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Description", interface.c_str());

        auto fabricMgrState = std::make_shared<NsmFabricManagerState>(
            nameFM, type, inventoryObjPathFM, manager, bus, description);

        device->addSensor(fabricMgrState, false, false);

        auto event = std::make_shared<NsmFabricManagerStateEvent>(
            name, type, fabricMgrState->getFabricManagerIntf(),
            fabricMgrState->getOperaStatusIntf());
        device->deviceEvents.push_back(event);
        device->eventDispatcher.addEvent(NSM_TYPE_NETWORK_PORT,
                                         NSM_FABRIC_MANAGER_STATE_EVENT, event);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

std::vector<std::string> nvSwitchInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVSwitch",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.PortDisableFuture",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.PowerMode",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.FabricManager"};

REGISTER_NSM_CREATION_FUNCTION(createNsmSwitchDI, nvSwitchInterfaces)
} // namespace nsm
