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
#include "../../common/nsmPropertySupport.hpp"
#include "../../common/utils.hpp"
#include "asyncOperationManager.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmAssetIntf.hpp"
#if defined(ENABLE_DEBUG_INFO)
#include "nsmDebugInfo.hpp"
#endif
#include "nsmDevice.hpp"
#if defined(ENABLE_DEBUG_INFO)
#include "nsmEraseTrace.hpp"
#endif
#if defined(ENABLE_ERROR_INJECTION)
#include "nsmErrorInjection/nsmErrorInjection.hpp"
#include "nsmErrorInjection/nsmErrorInjectionCommon.hpp"
#endif
#include "nsmEvent/nsmFabricManagerStateEvent.hpp"
#include "nsmHistograms/nsmHistogramInfo.hpp"
#include "nsmInventoryProperty.hpp"
#if defined(ENABLE_DEBUG_INFO)
#include "nsmLogInfo.hpp"
#endif
#include "nsmManagers/nsmFabricManager.hpp"
#include "nsmNvSwitchDeviceConfiguration.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPort/nsmPortDisableFuture.hpp"
#ifdef NVIDIA_SHMEM
#include "requester/mctp_endpoint_discovery.hpp"
#include "sharedMemCommon.hpp"
#endif

#include <unordered_map>
#include <variant>

namespace nsm
{
namespace
{
bool dbusPropertyMapAsBool(const dbus::PropertyMap& map,
                           const dbus::Property& key)
{
    auto it = map.find(key);
    if (it == map.end())
    {
        return false;
    }
    const dbus::Value& v = it->second;
    if (std::holds_alternative<bool>(v))
    {
        return std::get<bool>(v);
    }
    if (std::holds_alternative<int64_t>(v))
    {
        return std::get<int64_t>(v) != 0;
    }
    if (std::holds_alternative<uint64_t>(v))
    {
        return std::get<uint64_t>(v) != 0;
    }
    if (std::holds_alternative<int32_t>(v))
    {
        return std::get<int32_t>(v) != 0;
    }
    if (std::holds_alternative<uint32_t>(v))
    {
        return std::get<uint32_t>(v) != 0;
    }
    return false;
}
} // namespace

NsmSwitchDIReset::NsmSwitchDIReset(sdbusplus::bus_t& bus,
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
requester::Coroutine
    NsmSwitchDI<IntfType>::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    if constexpr (std::is_same_v<IntfType, UuidIntf>)
    {
        // For UuidIntf, we need to get the device UUID from the device manager.
        mctp::MctpDiscovery& mctpDiscovery = mctp::MctpDiscovery::getInstance();
        uuid_t deviceUuid;
        auto rc = co_await getDeviceUUID(nsmDevice, mctpDiscovery, deviceUuid);
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

requester::Coroutine
    NsmSwitchDIPowerMode::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_power_mode_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_power_mode_req failed. eid={EID} rc={RC}", "EID",
                   nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
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
    auto eid = device->getEid();
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
    auto rc_ = co_await device->postPatchIO(eid, request, responseMsg,
                                            responseLen);
    if (rc_)
    {
        lg2::error(
            "setL1PowerDevice postPatchIO failed for while setting PowerMode "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc_));
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

    const auto* patchRequestedValues = std::get_if<std::vector<
        std::tuple<std::string, std::variant<bool, uint32_t, double,
                                             std::vector<uint8_t>>>>>(&value);
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
    auto eid = device->getEid();
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
    auto rc_ = co_await device->postPatchIO(eid, request, responseMsg,
                                            responseLen);
    if (rc_)
    {
        lg2::error(
            "NsmSwitchIsolationMode::setSwitchIsolationMode postPatchIO failed for"
            "eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc_));
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

std::optional<std::vector<uint8_t>>
    NsmSwitchL1PredictionMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_device_mode_setting_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_setting_req(instanceId, 0, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_device_mode_setting_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmSwitchL1PredictionMode::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    nsm_l1_prediction_mode_config predictionMode;

    auto rc = decode_get_device_mode_setting_resp(
        responseMsg, responseLen, &cc, &reason_code, &predictionMode);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        if (predictionMode == nsm_l1_prediction_mode_config::ENABLED)
        {
            enableIntf->enabled(true);
        }
        else
        {
            enableIntf->enabled(false);
        }
    }
    return cc ? cc : rc;
}

requester::Coroutine NsmSwitchL1PredictionMode::setL1PredictionMode(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* l1PredictionMode = std::get_if<bool>(&value);
    if (!l1PredictionMode)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    auto eid = device->getEid();
    lg2::info("set L1 Prediction Mode On Device for EID: {EID}", "EID", eid);

    nsm_l1_prediction_mode_config predictionMode;
    if (*l1PredictionMode)
    {
        predictionMode = nsm_l1_prediction_mode_config::ENABLED;
    }
    else
    {
        predictionMode = nsm_l1_prediction_mode_config::DISABLED;
    }

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_mode_setting_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_device_mode_setting_req(0, 0, predictionMode,
                                                 requestMsg);

    if (rc)
    {
        lg2::error(
            "NsmSwitchL1PredictionMode::setL1PredictionMode encode_set_device_mode_setting_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await device->postPatchIO(eid, request, responseMsg,
                                            responseLen);
    if (rc_)
    {
        lg2::error(
            "NsmSwitchL1PredictionMode::setL1PredictionMode postPatchIO failed for"
            "eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc_));
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_set_device_mode_setting_resp(responseMsg.get(), responseLen,
                                             &cc, &reason_code);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        lg2::info(
            "NsmSwitchL1PredictionMode::setL1PredictionMode for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        lg2::error(
            "NsmSwitchL1PredictionMode::setL1PredictionMode decode_set_device_mode_setting_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return NSM_SW_SUCCESS;
}

inline void createNsmSwitchL1PredictionMode(std::shared_ptr<NsmDevice> device,
                                            sdbusplus::bus_t& bus,
                                            const std::string& objPath,
                                            const std::string& type,
                                            const std::string& name)
{
    auto dbusObjPath = objPath + name +
                       "/Oem/Nvidia/PowerMode/L1PredictionMode";
    //  Add associations between switch and l1prediction_mode
    std::vector<utils::Association> associations{
        {"parent_switch", "l1_prediction_mode", objPath + name}};
    auto l1predictionModeAssociationIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, dbusObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    for (const auto& association : associations)
    {
        associationsList.emplace_back(association.forward, association.backward,
                                      association.absolutePath);
    }
    l1predictionModeAssociationIntf->associations(associationsList);

    // Add enable interface to the object
    auto enableIntf = std::make_shared<EnableIntf>(bus, dbusObjPath.c_str());
    auto nvSwitchL1PredictionMode = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, std::move(l1predictionModeAssociationIntf));
    device->addSensor(nvSwitchL1PredictionMode, false);

    // Add async set operation to the object
    nsm::AsyncSetOperationHandler setL1PredictionModeHandler =
        std::bind(&NsmSwitchL1PredictionMode::setL1PredictionMode,
                  nvSwitchL1PredictionMode, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    AsyncOperationManager::getInstance()
        ->getDispatcher(dbusObjPath)
        ->addAsyncSetOperation("xyz.openbmc_project.Object.Enable", "Enabled",
                               AsyncSetOperationInfo{setL1PredictionModeHandler,
                                                     nvSwitchL1PredictionMode,
                                                     device});
}

// Map NSM wire enum8 to PowerCapMode. Wire Default (0) is resolved to Enabled
// at D-Bus publish time per the power-capping contract.
static std::optional<PowerCapMode> toPowerCapModeFromGet(uint8_t nsmMode)
{
    switch (nsmMode)
    {
        case NSM_POWER_CAPPING_MODE_ENABLED:
            return PowerCapMode::Enabled;
        case NSM_POWER_CAPPING_MODE_DISABLED:
            return PowerCapMode::Disabled;
        case NSM_POWER_CAPPING_MODE_DEFAULT:
            return PowerCapMode::Default;
        default:
            return std::nullopt;
    }
}

std::optional<std::vector<uint8_t>>
    NsmSwitchPowerCappingMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, DEVICE_MODE_POWER_CAPPING, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_device_mode_settings_v2_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmSwitchPowerCappingMode::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint8_t currentMode = 0;
    uint8_t pendingMode = 0;
    uint16_t currentModeLength = 0;
    uint16_t pendingModeLength = 0;

    /* Probe lengths with null data pointers so decode cannot overflow the
     * 1-byte mode buffers below on a malformed oversized payload. */
    auto rc = decode_get_device_mode_settings_v2_resp(
        responseMsg, responseLen, &cc, &reason_code, nullptr,
        &currentModeLength, nullptr, &pendingModeLength);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        return cc ? cc : rc;
    }
    if (currentModeLength != POWER_CAPPING_MODE_DATA_SIZE ||
        (pendingModeLength != 0 &&
         pendingModeLength != POWER_CAPPING_MODE_DATA_SIZE))
    {
        return NSM_SW_ERROR_LENGTH;
    }

    rc = decode_get_device_mode_settings_v2_resp(
        responseMsg, responseLen, &cc, &reason_code, &currentMode,
        &currentModeLength, &pendingMode, &pendingModeLength);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        if (currentModeLength != POWER_CAPPING_MODE_DATA_SIZE)
        {
            return NSM_SW_ERROR_LENGTH;
        }
        auto current = toPowerCapModeFromGet(currentMode);
        if (!current)
        {
            return NSM_SW_ERROR_DATA;
        }
        // Resolve wire Default (0) to Enabled for D-Bus publish per contract
        PowerCapMode currentResolved = (*current == PowerCapMode::Default)
                                           ? PowerCapMode::Enabled
                                           : *current;
        powerCappingModeIntf->currentMode(currentResolved);

        if (pendingModeLength != 0 &&
            pendingModeLength != POWER_CAPPING_MODE_DATA_SIZE)
        {
            return NSM_SW_ERROR_LENGTH;
        }
        if (pendingModeLength == POWER_CAPPING_MODE_DATA_SIZE)
        {
            auto pending = toPowerCapModeFromGet(pendingMode);
            if (!pending)
            {
                return NSM_SW_ERROR_DATA;
            }
            PowerCapMode pendingResolved = (*pending == PowerCapMode::Default)
                                               ? PowerCapMode::Enabled
                                               : *pending;
            powerCappingModeIntf->pendingMode(pendingResolved);
        }
        else
        {
            // No pending override from device; keep Settings in sync.
            powerCappingModeIntf->pendingMode(currentResolved);
        }
    }
    return cc ? cc : rc;
}

requester::Coroutine NsmSwitchPowerCappingMode::setPowerCappingMode(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const std::string* requested = std::get_if<std::string>(&value);
    if (!requested)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    auto eid = device->getEid();
    auto mode =
        PowerCappingModeServer::convertPowerCapModeFromString(*requested);

    uint8_t data = 0;
    if (mode == PowerCapMode::Default)
    {
        data = NSM_POWER_CAPPING_MODE_DEFAULT;
    }
    else if (mode == PowerCapMode::Enabled)
    {
        data = NSM_POWER_CAPPING_MODE_ENABLED;
    }
    else if (mode == PowerCapMode::Disabled)
    {
        data = NSM_POWER_CAPPING_MODE_DISABLED;
    }
    else
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (!powerCappingModeIntf->isModeConfigurable())
    {
        *status = AsyncOperationStatusType::Unavailable;
        throw sdbusplus::error::xyz::openbmc_project::common::NotAllowed{};
    }

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_mode_settings_v2_req) +
                    POWER_CAPPING_MODE_DATA_SIZE - 1);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_settings_v2_req(
        0, DEVICE_MODE_POWER_CAPPING, &data, POWER_CAPPING_MODE_DATA_SIZE,
        requestMsg);
    if (shouldLog("setPowerCappingMode encode", uint16_t(0), uint8_t(0), rc))
    {
        lg2::error("Encoding power capping mode failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
    }
    if (rc)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await device->postPatchIO(eid, request, responseMsg,
                                            responseLen);
    if (shouldLog("setPowerCappingMode postPatchIO", uint16_t(0), uint8_t(0),
                  rc_))
    {
        lg2::error("Setting power capping mode failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", utils::nsmSwCodeToString(rc_));
    }
    if (rc_)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_set_device_mode_settings_v2_resp(responseMsg.get(), responseLen,
                                                 &cc, &reason_code);
    if (shouldLog("setPowerCappingMode response", reason_code, cc, rc))
    {
        lg2::error(
            "Setting power capping mode returned an error. eid={EID} cc={CC} reasonCode={REASON} rc={RC}",
            "EID", eid, "CC", cc, "REASON", reason_code, "RC", rc);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        powerCappingModeIntf->pendingMode(
            mode == PowerCapMode::Default ? PowerCapMode::Enabled : mode);
    }
    else
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return NSM_SW_SUCCESS;
}

inline void createNsmSwitchPowerCappingMode(std::shared_ptr<NsmDevice> device,
                                            sdbusplus::bus_t& bus,
                                            const std::string& objPath,
                                            const std::string& type,
                                            const std::string& name)
{
    auto dbusObjPath = objPath + name + "/Oem/Nvidia/PowerCappingMode";
    std::vector<utils::Association> associations{
        {"parent_switch", "power_capping_mode", objPath + name}};
    auto powerCappingModeAssociationIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, dbusObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    for (const auto& association : associations)
    {
        associationsList.emplace_back(association.forward, association.backward,
                                      association.absolutePath);
    }
    powerCappingModeAssociationIntf->associations(associationsList);

    auto powerCappingModeIntf =
        std::make_shared<PowerCappingModeIntf>(bus, dbusObjPath.c_str());
    powerCappingModeIntf->currentMode(PowerCapMode::Enabled);
    powerCappingModeIntf->pendingMode(PowerCapMode::Enabled);
    powerCappingModeIntf->isModeConfigurable(true);
    auto nvSwitchPowerCappingMode = std::make_shared<NsmSwitchPowerCappingMode>(
        name, type, powerCappingModeIntf,
        std::move(powerCappingModeAssociationIntf));
    device->addSensor(nvSwitchPowerCappingMode, false);

    nsm::AsyncSetOperationHandler setPowerCappingModeHandler =
        std::bind(&NsmSwitchPowerCappingMode::setPowerCappingMode,
                  nvSwitchPowerCappingMode, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    AsyncOperationManager::getInstance()
        ->getDispatcher(dbusObjPath)
        ->addAsyncSetOperation(
            std::string(PowerCappingModeServer::interface), "PendingMode",
            AsyncSetOperationInfo{setPowerCappingModeHandler,
                                  nvSwitchPowerCappingMode, device});
}

// Map NSM wire enum8 to LTXModeEnum. Wire Default (0) is resolved to Enabled
// at D-Bus publish time per the LTX mode contract.
static std::optional<LTXModeEnum> toLTXModeFromGet(uint8_t nsmMode)
{
    switch (nsmMode)
    {
        case NSM_LTX_MODE_ENABLED:
            return LTXModeEnum::Enabled;
        case NSM_LTX_MODE_DISABLED:
            return LTXModeEnum::Disabled;
        case NSM_LTX_MODE_DEFAULT:
            return LTXModeEnum::Default;
        default:
            return std::nullopt;
    }
}

std::optional<std::vector<uint8_t>>
    NsmSwitchLTXMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, DEVICE_MODE_LTX, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_device_mode_settings_v2_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmSwitchLTXMode::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint8_t currentMode = 0;
    uint8_t pendingMode = 0;
    uint16_t currentModeLength = 0;
    uint16_t pendingModeLength = 0;

    /* Probe lengths with null data pointers so decode cannot overflow the
     * 1-byte mode buffers below on a malformed oversized payload. */
    auto rc = decode_get_device_mode_settings_v2_resp(
        responseMsg, responseLen, &cc, &reason_code, nullptr,
        &currentModeLength, nullptr, &pendingModeLength);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        return cc ? cc : rc;
    }
    if (currentModeLength != LTX_MODE_DATA_SIZE ||
        (pendingModeLength != 0 && pendingModeLength != LTX_MODE_DATA_SIZE))
    {
        return NSM_SW_ERROR_LENGTH;
    }

    rc = decode_get_device_mode_settings_v2_resp(
        responseMsg, responseLen, &cc, &reason_code, &currentMode,
        &currentModeLength, &pendingMode, &pendingModeLength);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        if (currentModeLength != LTX_MODE_DATA_SIZE)
        {
            return NSM_SW_ERROR_LENGTH;
        }
        auto current = toLTXModeFromGet(currentMode);
        if (!current)
        {
            return NSM_SW_ERROR_DATA;
        }
        LTXModeEnum currentResolved = (*current == LTXModeEnum::Default)
                                          ? LTXModeEnum::Enabled
                                          : *current;
        ltxModeIntf->currentMode(currentResolved);

        if (pendingModeLength != 0 && pendingModeLength != LTX_MODE_DATA_SIZE)
        {
            return NSM_SW_ERROR_LENGTH;
        }
        if (pendingModeLength == LTX_MODE_DATA_SIZE)
        {
            auto pending = toLTXModeFromGet(pendingMode);
            if (!pending)
            {
                return NSM_SW_ERROR_DATA;
            }
            LTXModeEnum pendingResolved = (*pending == LTXModeEnum::Default)
                                              ? LTXModeEnum::Enabled
                                              : *pending;
            ltxModeIntf->pendingMode(pendingResolved);
        }
        else
        {
            // No pending override from device; keep Settings in sync.
            ltxModeIntf->pendingMode(currentResolved);
        }
    }
    return cc ? cc : rc;
}

requester::Coroutine NsmSwitchLTXMode::setLTXMode(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const std::string* requested = std::get_if<std::string>(&value);
    if (!requested)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    auto eid = device->getEid();
    auto mode =
        LTXModeServer::convertLinkTrainingExtendedModeFromString(*requested);

    uint8_t data = 0;
    if (mode == LTXModeEnum::Default)
    {
        data = NSM_LTX_MODE_DEFAULT;
    }
    else if (mode == LTXModeEnum::Enabled)
    {
        data = NSM_LTX_MODE_ENABLED;
    }
    else if (mode == LTXModeEnum::Disabled)
    {
        data = NSM_LTX_MODE_DISABLED;
    }
    else
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (!ltxModeIntf->isModeConfigurable())
    {
        *status = AsyncOperationStatusType::Unavailable;
        throw sdbusplus::error::xyz::openbmc_project::common::NotAllowed{};
    }

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_mode_settings_v2_req) +
                    LTX_MODE_DATA_SIZE - 1);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_settings_v2_req(
        0, DEVICE_MODE_LTX, &data, LTX_MODE_DATA_SIZE, requestMsg);
    if (shouldLog("setLTXMode encode", uint16_t(0), uint8_t(0), rc))
    {
        lg2::error("Encoding LTX mode failed. eid={EID} rc={RC}", "EID", eid,
                   "RC", rc);
    }
    if (rc)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await device->postPatchIO(eid, request, responseMsg,
                                            responseLen);
    if (shouldLog("setLTXMode postPatchIO", uint16_t(0), uint8_t(0), rc_))
    {
        lg2::error("Setting LTX mode failed. eid={EID} rc={RC}", "EID", eid,
                   "RC", utils::nsmSwCodeToString(rc_));
    }
    if (rc_)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_set_device_mode_settings_v2_resp(responseMsg.get(), responseLen,
                                                 &cc, &reason_code);
    if (shouldLog("setLTXMode response", reason_code, cc, rc))
    {
        lg2::error(
            "Setting LTX mode returned an error. eid={EID} cc={CC} reasonCode={REASON} rc={RC}",
            "EID", eid, "CC", cc, "REASON", reason_code, "RC", rc);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        ltxModeIntf->pendingMode(
            mode == LTXModeEnum::Default ? LTXModeEnum::Enabled : mode);
    }
    else
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return NSM_SW_SUCCESS;
}

inline void createNsmSwitchLTXMode(std::shared_ptr<NsmDevice> device,
                                   sdbusplus::bus_t& bus,
                                   const std::string& objPath,
                                   const std::string& type,
                                   const std::string& name)
{
    auto dbusObjPath = objPath + name + "/Oem/Nvidia/LTXMode";
    std::vector<utils::Association> associations{
        {"parent_switch", "ltx_mode", objPath + name}};
    auto ltxModeAssociationIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, dbusObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    for (const auto& association : associations)
    {
        associationsList.emplace_back(association.forward, association.backward,
                                      association.absolutePath);
    }
    ltxModeAssociationIntf->associations(associationsList);

    auto ltxModeIntf = std::make_shared<LTXModeIntf>(bus, dbusObjPath.c_str());
    ltxModeIntf->currentMode(LTXModeEnum::Enabled);
    ltxModeIntf->pendingMode(LTXModeEnum::Enabled);
    // IsModeConfigurable is sourced solely from the entity-manager
    // SupportLTXMode capability flag: this factory is only ever invoked from
    // the createNsmSwitchDI gate when SupportLTXMode == true (see the
    // supportLTXMode check above createNsmSwitchLTXMode's call site), so
    // hardcoding true here is equivalent to wiring it from that flag. No
    // other code path may set IsModeConfigurable.
    ltxModeIntf->isModeConfigurable(true);
    auto nvSwitchLTXMode = std::make_shared<NsmSwitchLTXMode>(
        name, type, ltxModeIntf, std::move(ltxModeAssociationIntf));
    device->addSensor(nvSwitchLTXMode, false);

    nsm::AsyncSetOperationHandler setLTXModeHandler = std::bind(
        &NsmSwitchLTXMode::setLTXMode, nvSwitchLTXMode, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3);
    AsyncOperationManager::getInstance()
        ->getDispatcher(dbusObjPath)
        ->addAsyncSetOperation(
            std::string(LTXModeServer::interface), "PendingMode",
            AsyncSetOperationInfo{setLTXModeHandler, nvSwitchLTXMode, device});
}

// Map NSM wire enum8 to UPhyModeEnum. Wire Default (0) is resolved to Enabled
// at D-Bus publish time per the UPhy mode contract.
static std::optional<UPhyModeEnum> toUPhyModeFromGet(uint8_t nsmMode)
{
    switch (nsmMode)
    {
        case NSM_UPHY_MODE_ENABLED:
            return UPhyModeEnum::Enabled;
        case NSM_UPHY_MODE_DISABLED:
            return UPhyModeEnum::Disabled;
        case NSM_UPHY_MODE_DEFAULT:
            return UPhyModeEnum::Default;
        default:
            return std::nullopt;
    }
}

std::optional<std::vector<uint8_t>>
    NsmSwitchUPhyMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, DEVICE_MODE_UPHY, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_device_mode_settings_v2_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmSwitchUPhyMode::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint8_t currentMode = 0;
    uint8_t pendingMode = 0;
    uint16_t currentModeLength = 0;
    uint16_t pendingModeLength = 0;

    /* Probe lengths with null data pointers so decode cannot overflow the
     * 1-byte mode buffers below on a malformed oversized payload. */
    auto rc = decode_get_device_mode_settings_v2_resp(
        responseMsg, responseLen, &cc, &reason_code, nullptr,
        &currentModeLength, nullptr, &pendingModeLength);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        return cc ? cc : rc;
    }
    if (currentModeLength != UPHY_MODE_DATA_SIZE ||
        (pendingModeLength != 0 && pendingModeLength != UPHY_MODE_DATA_SIZE))
    {
        return NSM_SW_ERROR_LENGTH;
    }

    rc = decode_get_device_mode_settings_v2_resp(
        responseMsg, responseLen, &cc, &reason_code, &currentMode,
        &currentModeLength, &pendingMode, &pendingModeLength);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        if (currentModeLength != UPHY_MODE_DATA_SIZE)
        {
            return NSM_SW_ERROR_LENGTH;
        }
        auto current = toUPhyModeFromGet(currentMode);
        if (!current)
        {
            return NSM_SW_ERROR_DATA;
        }
        UPhyModeEnum currentResolved = (*current == UPhyModeEnum::Default)
                                           ? UPhyModeEnum::Enabled
                                           : *current;
        uphyModeIntf->currentMode(currentResolved);

        if (pendingModeLength != 0 && pendingModeLength != UPHY_MODE_DATA_SIZE)
        {
            return NSM_SW_ERROR_LENGTH;
        }
        if (pendingModeLength == UPHY_MODE_DATA_SIZE)
        {
            auto pending = toUPhyModeFromGet(pendingMode);
            if (!pending)
            {
                return NSM_SW_ERROR_DATA;
            }
            UPhyModeEnum pendingResolved = (*pending == UPhyModeEnum::Default)
                                               ? UPhyModeEnum::Enabled
                                               : *pending;
            uphyModeIntf->pendingMode(pendingResolved);
        }
        else
        {
            // No pending override from device; keep Settings in sync.
            uphyModeIntf->pendingMode(currentResolved);
        }
    }
    return cc ? cc : rc;
}

requester::Coroutine NsmSwitchUPhyMode::setUPhyMode(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const std::string* requested = std::get_if<std::string>(&value);
    if (!requested)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    auto eid = device->getEid();
    auto mode = UPhyModeServer::convertUPhyModeFromString(*requested);

    uint8_t data = 0;
    if (mode == UPhyModeEnum::Default)
    {
        data = NSM_UPHY_MODE_DEFAULT;
    }
    else if (mode == UPhyModeEnum::Enabled)
    {
        data = NSM_UPHY_MODE_ENABLED;
    }
    else if (mode == UPhyModeEnum::Disabled)
    {
        data = NSM_UPHY_MODE_DISABLED;
    }
    else
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (!uphyModeIntf->isModeConfigurable())
    {
        *status = AsyncOperationStatusType::Unavailable;
        throw sdbusplus::error::xyz::openbmc_project::common::NotAllowed{};
    }

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_mode_settings_v2_req) +
                    UPHY_MODE_DATA_SIZE - 1);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_settings_v2_req(
        0, DEVICE_MODE_UPHY, &data, UPHY_MODE_DATA_SIZE, requestMsg);
    if (shouldLog("setUPhyMode encode", uint16_t(0), uint8_t(0), rc))
    {
        lg2::error("Encoding UPhy mode failed. eid={EID} rc={RC}", "EID", eid,
                   "RC", rc);
    }
    if (rc)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await device->postPatchIO(eid, request, responseMsg,
                                            responseLen);
    if (shouldLog("setUPhyMode postPatchIO", uint16_t(0), uint8_t(0), rc_))
    {
        lg2::error("Setting UPhy mode failed. eid={EID} rc={RC}", "EID", eid,
                   "RC", utils::nsmSwCodeToString(rc_));
    }
    if (rc_)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_set_device_mode_settings_v2_resp(responseMsg.get(), responseLen,
                                                 &cc, &reason_code);
    if (shouldLog("setUPhyMode response", reason_code, cc, rc))
    {
        lg2::error(
            "Setting UPhy mode returned an error. eid={EID} cc={CC} reasonCode={REASON} rc={RC}",
            "EID", eid, "CC", cc, "REASON", reason_code, "RC", rc);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        uphyModeIntf->pendingMode(
            mode == UPhyModeEnum::Default ? UPhyModeEnum::Enabled : mode);
    }
    else
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return NSM_SW_SUCCESS;
}

inline void createNsmSwitchUPhyMode(std::shared_ptr<NsmDevice> device,
                                    sdbusplus::bus_t& bus,
                                    const std::string& objPath,
                                    const std::string& type,
                                    const std::string& name)
{
    auto dbusObjPath = objPath + name + "/Oem/Nvidia/UPhyMode";
    std::vector<utils::Association> associations{
        {"parent_switch", "uphy_mode", objPath + name}};
    auto uphyModeAssociationIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, dbusObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    for (const auto& association : associations)
    {
        associationsList.emplace_back(association.forward, association.backward,
                                      association.absolutePath);
    }
    uphyModeAssociationIntf->associations(associationsList);

    auto uphyModeIntf = std::make_shared<UPhyModeIntf>(bus,
                                                       dbusObjPath.c_str());
    uphyModeIntf->currentMode(UPhyModeEnum::Enabled);
    uphyModeIntf->pendingMode(UPhyModeEnum::Enabled);
    // IsModeConfigurable is sourced solely from the entity-manager
    // SupportUPhyMode capability flag: this factory is only ever invoked
    // from the createNsmSwitchDI gate when SupportUPhyMode == true (see the
    // supportUPhyMode check above createNsmSwitchUPhyMode's call site), so
    // hardcoding true here is equivalent to wiring it from that flag. No
    // other code path may set IsModeConfigurable.
    uphyModeIntf->isModeConfigurable(true);
    auto nvSwitchUPhyMode = std::make_shared<NsmSwitchUPhyMode>(
        name, type, uphyModeIntf, std::move(uphyModeAssociationIntf));
    device->addSensor(nvSwitchUPhyMode, false);

    nsm::AsyncSetOperationHandler setUPhyModeHandler = std::bind(
        &NsmSwitchUPhyMode::setUPhyMode, nvSwitchUPhyMode,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    AsyncOperationManager::getInstance()
        ->getDispatcher(dbusObjPath)
        ->addAsyncSetOperation(std::string(UPhyModeServer::interface),
                               "PendingMode",
                               AsyncSetOperationInfo{setUPhyModeHandler,
                                                     nvSwitchUPhyMode, device});
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

    auto device = manager.getNsmDeviceFromStaticUUID(uuid);

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

        device->addDeviceSensors(nvSwitchIntf);
        device->addStaticSensor(nvSwitchUuid);
        device->addStaticSensor(nvSwitchAssociation);

        auto SupportL1PredictionMode = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "SupportL1PredictionMode", baseInterface.c_str());
        if (SupportL1PredictionMode)
        {
            createNsmSwitchL1PredictionMode(device, bus, inventoryObjPath, type,
                                            name);
        }

        // An absent support property means the mode is not exposed. Read it
        // from the cached base properties so absence needs no D-Bus fetch.
        bool supportPowerCappingMode = false;
        auto powerCappingModeProperty =
            allBaseIfaceProperties.find("SupportPowerCappingMode");
        if (powerCappingModeProperty != allBaseIfaceProperties.end())
        {
            const bool* value =
                std::get_if<bool>(&powerCappingModeProperty->second);
            supportPowerCappingMode = value != nullptr && *value;
        }
        if (supportPowerCappingMode)
        {
            createNsmSwitchPowerCappingMode(device, bus, inventoryObjPath, type,
                                            name);
        }

        bool supportLTXMode = false;
        auto ltxModeProperty = allBaseIfaceProperties.find("SupportLTXMode");
        if (ltxModeProperty != allBaseIfaceProperties.end())
        {
            const bool* value = std::get_if<bool>(&ltxModeProperty->second);
            supportLTXMode = value != nullptr && *value;
        }
        if (supportLTXMode)
        {
            createNsmSwitchLTXMode(device, bus, inventoryObjPath, type, name);
        }

        bool supportUPhyMode = false;
        auto uphyModeProperty = allBaseIfaceProperties.find("SupportUPhyMode");
        if (uphyModeProperty != allBaseIfaceProperties.end())
        {
            const bool* value = std::get_if<bool>(&uphyModeProperty->second);
            supportUPhyMode = value != nullptr && *value;
        }
        if (supportUPhyMode)
        {
            createNsmSwitchUPhyMode(device, bus, inventoryObjPath, type, name);
        }

// NVSwitch exposes the full NetIR set (DebugInfo + LogInfo + Erase);
// non-volatile flash requires erase after collection.
#if defined(ENABLE_DEBUG_INFO)
        auto nvSwitchDebugInfoObject = std::make_shared<NsmDebugInfoObject>(
            bus, name, inventoryObjPath, type, uuid, DebugDumpType::Network);
        device->addStaticSensor(nvSwitchDebugInfoObject);

        auto nvSwitchEraseTraceObject = std::make_shared<NsmEraseTraceObject>(
            bus, name, inventoryObjPath, type, uuid);
        device->addStaticSensor(nvSwitchEraseTraceObject);

#endif

        // Device Reset for NVSwitch
        auto nvSwitchResetSensor = std::make_shared<NsmSwitchDIReset>(
            bus, name, type, inventoryObjPath, device);
        device->addDeviceSensors(nvSwitchResetSensor);

#if defined(ENABLE_ERROR_INJECTION)
        createNsmErrorInjectionSensors(manager, device,
                                       path(inventoryObjPath) / name);
#endif

        std::string dbusObjPath = inventoryObjPath + name;

        const bool supportNvSwitchDeviceConfiguration = dbusPropertyMapAsBool(
            allBaseIfaceProperties, "SupportNvSwitchDeviceConfiguration");
        addNvSwitchDeviceConfigurationSensorIfEnabled(
            supportNvSwitchDeviceConfiguration, bus, name, dbusObjPath, device);

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
    else if (type == "NSM_Chassis_Attributes")
    {
        auto nvSwitchChassisAttributes =
            std::make_shared<NsmSwitchDI<NsmAssetIntf>>(name, inventoryObjPath);
        std::string manufacturer = MANUFACTURER_NVIDIA;
        nvSwitchChassisAttributes->invoke(pdiMethod(manufacturer),
                                          manufacturer);

        markAssetPropertiesNotSupported(*nvSwitchChassisAttributes,
                                        {FRU_PART_NUMBER, SERIAL_NUMBER});

        const auto modelProperty = getModelInventoryProperty(
            device->getDeviceType(), device->getDeviceRole());
        device->addStaticSensor(
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
                *nvSwitchChassisAttributes, modelProperty));
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
        device->addDeviceEvent(event, NSM_TYPE_NETWORK_PORT,
                               NSM_FABRIC_MANAGER_STATE_EVENT);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

std::vector<std::string> nvSwitchInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVSwitch",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.PortDisableFuture",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.PowerMode",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.ChassisAttributes",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch.FabricManager"};

REGISTER_NSM_CREATION_FUNCTION(createNsmSwitchDI, nvSwitchInterfaces)
} // namespace nsm
