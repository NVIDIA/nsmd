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

#include "base.h"
#include "network-ports.h"

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
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
#include "nsmHistograms/nsmHistogramInfo.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmLogInfo.hpp"
#include "nsmManagers/nsmFabricManager.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPort/nsmPortDisableFuture.hpp"
#include "sharedMemCommon.hpp"

#include <unordered_map>

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
    if constexpr (std::is_same_v<IntfType, UuidIntf>)
    {
        // For UuidIntf, we need to get the device UUID from the device manager.
        DeviceManager& deviceManager = DeviceManager::getInstance();
        uuid_t deviceUuid;
        auto rc = co_await getDeviceUUID(manager, eid, deviceManager,
                                         deviceUuid);
        if (rc == NSM_SW_SUCCESS && !deviceUuid.empty())
        {
            this->invoke(pdiMethod(uuid), deviceUuid);
            co_return NSM_SUCCESS;
        }
        co_return NSM_ERROR;
    }
    // For other interfaces, we don't need to update anything.
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

nsm_power_mode_data NsmSwitchDIPowerMode::getPowerModeData()
{
    nsm_power_mode_data powerModeData;
    powerModeData.l1_hw_mode_control = invoke(pdiMethod(hwModeControl));
    powerModeData.l1_hw_mode_threshold =
        static_cast<uint32_t>(invoke(pdiMethod(hwThreshold)));
    powerModeData.l1_fw_throttling_mode = invoke(pdiMethod(fwThrottlingMode));
    powerModeData.l1_prediction_mode = invoke(pdiMethod(predictionMode));
    powerModeData.l1_hw_active_time =
        static_cast<uint16_t>(invoke(pdiMethod(hwActiveTime)));
    powerModeData.l1_hw_inactive_time =
        static_cast<uint16_t>(invoke(pdiMethod(hwInactiveTime)));
    powerModeData.l1_prediction_inactive_time =
        static_cast<uint16_t>(invoke(pdiMethod(hwPredictionInactiveTime)));

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
    nsm_power_mode_data data;

    rc = decode_get_power_mode_resp(responseMsg.get(), responseLen, &cc,
                                    &dataSize, &reasonCode, &data);

    LG2_ERROR_FLT(
        "decode_get_power_mode_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        // update values
        (data.l1_hw_mode_control == 1)
            ? invoke(pdiMethod(hwModeControl), true)
            : invoke(pdiMethod(hwModeControl), false);
        invoke(pdiMethod(hwThreshold),
               static_cast<uint64_t>(data.l1_hw_mode_threshold));
        (data.l1_fw_throttling_mode == 1)
            ? invoke(pdiMethod(fwThrottlingMode), true)
            : invoke(pdiMethod(fwThrottlingMode), false);
        (data.l1_prediction_mode == 1)
            ? invoke(pdiMethod(predictionMode), true)
            : invoke(pdiMethod(predictionMode), false);
        invoke(pdiMethod(hwActiveTime),
               static_cast<uint64_t>(data.l1_hw_active_time));
        invoke(pdiMethod(hwInactiveTime),
               static_cast<uint64_t>(data.l1_hw_inactive_time));
        invoke(pdiMethod(hwPredictionInactiveTime),
               static_cast<uint64_t>(data.l1_prediction_inactive_time));
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
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

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
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

requester::Coroutine NsmSwitchDIPowerMode::setL1PowerModePatch(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    if (asyncPatchInProgress)
    {
        lg2::error(
            "setL1PowerModePatch: Async patch operation already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }

    auto powerModePatchData = getPowerModeData();

    const auto* patchRequestedValues = std::get_if<
        std::vector<std::tuple<std::string, std::variant<bool, uint32_t>>>>(
        &value);
    if (!patchRequestedValues)
    {
        lg2::error(
            "setL1PowerModePatch: Failed to get patch values - invalid type");
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (patchRequestedValues->empty())
    {
        lg2::error("setL1PowerModePatch: Empty patch values list");
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    for (const auto& [key, val] : *patchRequestedValues)
    {
        if (key == "HWModeControl")
        {
            const bool* l1HWModeControl = std::get_if<bool>(&val);
            if (!l1HWModeControl)
            {
                lg2::error(
                    "setL1PowerModePatch: Invalid type for HWModeControl");
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            powerModePatchData.l1_hw_mode_control = *l1HWModeControl;
        }
        else if (key == "FWThrottlingMode")
        {
            const bool* l1FWThrottlingMode = std::get_if<bool>(&val);
            if (!l1FWThrottlingMode)
            {
                lg2::error(
                    "setL1PowerModePatch: Invalid type for FWThrottlingMode");
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            powerModePatchData.l1_fw_throttling_mode = *l1FWThrottlingMode;
        }
        else if (key == "PredictionMode")
        {
            const bool* l1PredictionMode = std::get_if<bool>(&val);
            if (!l1PredictionMode)
            {
                lg2::error(
                    "setL1PowerModePatch: Invalid type for PredictionMode");
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            powerModePatchData.l1_prediction_mode = *l1PredictionMode;
        }
        else if (key == "HWThreshold")
        {
            const uint32_t* l1HWThreshold = std::get_if<uint32_t>(&val);
            if (!l1HWThreshold)
            {
                lg2::error("setL1PowerModePatch: Invalid type for HWThreshold");
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            powerModePatchData.l1_hw_mode_threshold =
                static_cast<uint64_t>(*l1HWThreshold);
        }
        else if (key == "HWActiveTime")
        {
            const uint32_t* l1HWActiveTime = std::get_if<uint32_t>(&val);
            if (!l1HWActiveTime)
            {
                lg2::error(
                    "setL1PowerModePatch: Invalid type for HWActiveTime");
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            powerModePatchData.l1_hw_active_time =
                static_cast<uint64_t>(*l1HWActiveTime);
        }
        else if (key == "HWInactiveTime")
        {
            const uint32_t* l1HWInactiveTime = std::get_if<uint32_t>(&val);
            if (!l1HWInactiveTime)
            {
                lg2::error(
                    "setL1PowerModePatch: Invalid type for HWInactiveTime");
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            powerModePatchData.l1_hw_inactive_time =
                static_cast<uint64_t>(*l1HWInactiveTime);
        }
        else if (key == "HWPredictionInactiveTime")
        {
            const uint32_t* l1PredictionInactiveTime =
                std::get_if<uint32_t>(&val);
            if (!l1PredictionInactiveTime)
            {
                lg2::error(
                    "setL1PowerModePatch: Invalid type for HWPredictionInactiveTime");
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
            }
            powerModePatchData.l1_prediction_inactive_time =
                static_cast<uint64_t>(*l1PredictionInactiveTime);
        }
        else
        {
            lg2::error("setL1PowerModePatch: Unrecognized property {PROPERTY}",
                       "PROPERTY", key);
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
    }

    asyncPatchInProgress = true;
    try
    {
        const auto rc = co_await setL1PowerDevice(powerModePatchData, status,
                                                  device);
        asyncPatchInProgress = false;
        co_return rc;
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "setL1PowerModePatch: Exception during setL1PowerDevice: {ERROR}",
            "ERROR", e.what());
        asyncPatchInProgress = false;
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR;
    }
}

NsmSwitchIsolationMode::NsmSwitchIsolationMode(
    const std::string& name, const std::string& type,
    std::shared_ptr<SwitchIsolationIntf> switchIsolationIntf) :
    NsmSensor(name, type), switchIsolationIntf(switchIsolationIntf)
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

    LG2_ERROR_FLT(
        "decode_get_switch_isolation_mode_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reason_code, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
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
    }
    return cc ? cc : rc;
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

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
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
    std::string inventoryObjPath{};
    if (allBaseIfaceProperties.count("InventoryObjPath"))
    {
        inventoryObjPath = std::get<std::string>(
            allBaseIfaceProperties.at("InventoryObjPath"));
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
        nvSwitchAssociation->invoke(pdiMethod(associations), associations_list);
        nvSwitchUuid->invoke(pdiMethod(uuid), uuid);

        device->deviceSensors.emplace_back(nvSwitchIntf);
        device->addStaticSensor(nvSwitchUuid);
        device->addStaticSensor(nvSwitchAssociation);

        auto debugTokenObject = std::make_shared<NsmDebugTokenObject>(
            bus, name, associations, type, uuid);
        device->addStaticSensor(debugTokenObject);

        // NetIR dump for NVSwitch
        auto nvSwitchDebugInfoObject = std::make_shared<NsmDebugInfoObject>(
            bus, name, inventoryObjPath, type, uuid, DebugDumpType::Network);
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

#ifdef NVIDIA_HISTOGRAM
        // add power histogram
        std::string histoObjName = "Power_0";
        std::string histoDbusObjPath = dbusObjPath + "/Histograms/" +
                                       histoObjName;
        uint32_t powerHistogramID = 0;
        powerHistogramID =
            (static_cast<uint32_t>(NSM_HISTOGRAM_NAMESPACE_ID_POWER)
             << SHIFT_BITS_24) |
            (static_cast<uint32_t>(NSM_HISTOGRAM_REVISION_ID_0)
             << SHIFT_BITS_16) |
            (static_cast<uint32_t>(NSM_HISTOGRAM_ID_POWER_CONSUMPTION));

        auto powerHistoFormatIntf =
            std::make_shared<FormatIntf>(bus, histoDbusObjPath.c_str());
        auto powerHistoBucketDataIntf =
            std::make_shared<BucketInfoIntf>(bus, histoDbusObjPath.c_str());
        std::vector<std::tuple<std::string, std::string, std::string>>
            associationsList;
        associationsList.emplace_back("parent_device", "histograms",
                                      dbusObjPath);
        auto getPowerHistoFormatObject = std::make_shared<NsmHistogramFormat>(
            bus, histoObjName, "NvSwitch_Power_Histogram", powerHistoFormatIntf,
            powerHistoBucketDataIntf, dbusObjPath, associationsList,
            powerHistogramID, 0);

        auto getPowerHistoDataObject = std::make_shared<NsmHistogramData>(
            histoObjName, "NvSwitch_Power_Histogram", powerHistoFormatIntf,
            powerHistoBucketDataIntf, powerHistogramID, 0);

        device->addStaticSensor(getPowerHistoFormatObject);
        device->addSensor(getPowerHistoDataObject, false);
#endif
    }
    else if (type == "NSM_PortDisableFuture")
    {
        // Port disable future status on NVSwitch
        bool priority{};
        if (allCurrentIfaceProperties.count("Priority"))
        {
            priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
        }

        auto nvSwitchPortDisableFuture =
            std::make_shared<nsm::NsmDevicePortDisableFuture>(name, type,
                                                              inventoryObjPath);

        nvSwitchPortDisableFuture->invoke(pdiMethod(portDisableFuture),
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
        bool priority{};
        if (allCurrentIfaceProperties.count("Priority"))
        {
            priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
        }

        auto nvSwitchL1PowerMode =
            std::make_shared<NsmSwitchDIPowerMode>(name, inventoryObjPath);

        nvSwitchL1PowerMode->invoke(pdiMethod(hwModeControl), false);
        nvSwitchL1PowerMode->invoke(pdiMethod(hwThreshold), 0);
        nvSwitchL1PowerMode->invoke(pdiMethod(fwThrottlingMode), false);
        nvSwitchL1PowerMode->invoke(pdiMethod(predictionMode), false);
        nvSwitchL1PowerMode->invoke(pdiMethod(hwActiveTime), 0);
        nvSwitchL1PowerMode->invoke(pdiMethod(hwInactiveTime), 0);
        nvSwitchL1PowerMode->invoke(pdiMethod(hwPredictionInactiveTime), 0);

        device->addSensor(nvSwitchL1PowerMode, priority);
        auto objectPath = nvSwitchL1PowerMode->getInventoryObjectPath();

        nsm::AsyncSetOperationHandler setL1PowerModePatchHandler =
            std::bind(&NsmSwitchDIPowerMode::setL1PowerModePatch,
                      nvSwitchL1PowerMode, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);

        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWModeControl",
                AsyncSetOperationInfo{setL1PowerModePatchHandler,
                                      nvSwitchL1PowerMode, device});

        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "FWThrottlingMode",
                AsyncSetOperationInfo{setL1PowerModePatchHandler,
                                      nvSwitchL1PowerMode, device});

        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "PredictionMode",
                AsyncSetOperationInfo{setL1PowerModePatchHandler,
                                      nvSwitchL1PowerMode, device});

        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWThreshold",
                AsyncSetOperationInfo{setL1PowerModePatchHandler,
                                      nvSwitchL1PowerMode, device});

        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWActiveTime",
                AsyncSetOperationInfo{setL1PowerModePatchHandler,
                                      nvSwitchL1PowerMode, device});

        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWInactiveTime",
                AsyncSetOperationInfo{setL1PowerModePatchHandler,
                                      nvSwitchL1PowerMode, device});

        AsyncOperationManager::getInstance()
            ->getDispatcher(objectPath)
            ->addAsyncSetOperation(
                "com.nvidia.PowerMode", "HWPredictionInactiveTime",
                AsyncSetOperationInfo{setL1PowerModePatchHandler,
                                      nvSwitchL1PowerMode, device});
    }
    else if (type == "NSM_Switch")
    {
        auto nvSwitchObject =
            std::make_shared<NsmSwitchDI<SwitchIntf>>(name, inventoryObjPath);
        std::string switchType{};
        if (allCurrentIfaceProperties.count("SwitchType"))
        {
            switchType = std::get<std::string>(
                allCurrentIfaceProperties.at("SwitchType"));
        }
        std::vector<std::string> switchProtocols{};
        if (allCurrentIfaceProperties.count("SwitchSupportedProtocols"))
        {
            switchProtocols = std::get<std::vector<std::string>>(
                allCurrentIfaceProperties.at("SwitchSupportedProtocols"));
        }

        std::vector<SwitchIntf::SwitchType> supported_protocols;
        for (const auto& protocol : switchProtocols)
        {
            supported_protocols.emplace_back(
                SwitchIntf::convertSwitchTypeFromString(protocol));
        }
        nvSwitchObject->invoke(
            pdiMethod(type),
            SwitchIntf::convertSwitchTypeFromString(switchType));
        nvSwitchObject->invoke(pdiMethod(supportedProtocols),
                               supported_protocols);

        // maxSpeed and currentSpeed from PLDM T2

        device->addSensor(nvSwitchObject, false);
    }
    else if (type == "NSM_Asset")
    {
        auto nvSwitchAsset =
            std::make_shared<NsmSwitchDI<NsmAssetIntf>>(name, inventoryObjPath);
        std::string manufacturer{};
        if (allCurrentIfaceProperties.count("Manufacturer"))
        {
            manufacturer = std::get<std::string>(
                allCurrentIfaceProperties.at("Manufacturer"));
        }

        nvSwitchAsset->invoke(pdiMethod(manufacturer), manufacturer);
        device->addStaticSensor(nvSwitchAsset);
    }
    else if (type == "NSM_FabricManager")
    {
        std::string nameFM{};
        if (allCurrentIfaceProperties.count("Name"))
        {
            nameFM =
                std::get<std::string>(allCurrentIfaceProperties.at("Name"));
        }
        std::string inventoryObjPathFM{};
        if (allCurrentIfaceProperties.count("InventoryObjPath"))
        {
            inventoryObjPathFM = std::get<std::string>(
                allCurrentIfaceProperties.at("InventoryObjPath"));
        }
        std::string description{};
        if (allCurrentIfaceProperties.count("Description"))
        {
            description = std::get<std::string>(
                allCurrentIfaceProperties.at("Description"));
        }

        inventoryObjPathFM = inventoryObjPathFM + nameFM;
        auto fabricMgrState = std::make_shared<NsmFabricManagerState>(
            name, type, inventoryObjPath, inventoryObjPathFM, bus, description);

        device->addSensor(fabricMgrState, false, false);

        auto event = std::make_shared<NsmFabricManagerStateEvent>(
            name, type, fabricMgrState->getFabricManagerIntf(),
            fabricMgrState->getOperaStatusIntf(),
            fabricMgrState->getAggregateFabricManagerState());
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
