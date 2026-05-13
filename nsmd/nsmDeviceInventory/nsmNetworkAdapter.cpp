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
#if defined(ENABLE_DEBUG_INFO)
#include "nsmDebugInfo.hpp"
#endif
#if defined(ENABLE_DEBUG_INFO)
#include "nsmEraseTrace.hpp"
#endif
#if defined(ENABLE_ERROR_INJECTION)
#include "nsmErrorInjection/nsmErrorInjectionCommon.hpp"
#endif
#if defined(ENABLE_DEBUG_INFO)
#include "nsmLogInfo.hpp"
#endif
#include "libnsm/device-configuration.h"

#include "asyncOperationManager.hpp"
#if defined(ENABLE_LLDP)
#include "nsmLLDPLib/nsmLldpPort.hpp"
#endif

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
    auto rc_ = co_await device->postPatchIO(eid, request, responseMsg,
                                            responseLen);
    if (rc_)
    {
        std::vector<uint8_t> responseVec(
            reinterpret_cast<const uint8_t*>(responseMsg.get()),
            reinterpret_cast<const uint8_t*>(responseMsg.get()) + responseLen);
        lg2::error(
            "NsmDeviceProtectionOptions::setProtectionOptions postPatchIO failed for"
            "eid={EID} rc={RC} responseBytes={RESPONSEBYTES}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc_), "RESPONSEBYTES",
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

NsmDeviceModeSettingsV2Base::NsmDeviceModeSettingsV2Base(
    const std::string& name, const std::string& type,
    enum device_mode_index deviceModeIndex, uint8_t patchabilityBitmap) :
    NsmSensor(name, type), deviceModeIndex(deviceModeIndex),
    patchabilityBitmap(patchabilityBitmap)
{}

bool NsmDeviceModeSettingsV2Base::isModeBitSet(uint8_t bitmap,
                                               uint8_t subModeOffset)
{
    return (bitmap & (1U << subModeOffset)) != 0;
}

bool NsmDeviceModeSettingsV2Base::isSubDeviceModePatchable(
    uint8_t subModeOffset) const
{
    return isModeBitSet(patchabilityBitmap, subModeOffset);
}

NsmDeviceModeSettingsV2GetBase::NsmDeviceModeSettingsV2GetBase(
    const std::string& name, const std::string& type,
    enum device_mode_index deviceModeIndex, uint8_t patchabilityBitmap) :
    NsmDeviceModeSettingsV2Base(name, type, deviceModeIndex, patchabilityBitmap)
{}

std::optional<std::vector<uint8_t>>
    NsmDeviceModeSettingsV2GetBase::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_req), 0);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, static_cast<uint32_t>(deviceModeIndex), requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        LG2_ERROR_FLT(
            "encode_get_device_mode_settings_v2_req failed. eid={EID} index={INDEX} rc={RC}",
            "EID", eid, "INDEX", deviceModeIndex, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmDeviceModeSettingsV2GetBase::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    constexpr size_t maxModeBytes = 16;
    uint8_t currentData[maxModeBytes] = {};
    uint8_t pendingData[maxModeBytes] = {};
    uint16_t currentLength = 0;
    uint16_t pendingLength = 0;

    constexpr size_t minRespLen = sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_device_mode_settings_v2_resp) -
                                  sizeof(uint8_t);
    if (responseLen >= minRespLen)
    {
        auto* resp =
            reinterpret_cast<const nsm_get_device_mode_settings_v2_resp*>(
                responseMsg->payload);
        uint16_t reportedCurrentLen = le16toh(resp->current_mode_length);
        uint16_t reportedPendingLen = le16toh(resp->pending_mode_length);
        if (reportedCurrentLen > maxModeBytes ||
            reportedPendingLen > maxModeBytes)
        {
            LG2_ERROR_FLT(
                "device mode payload exceeds buffer. index={INDEX} current={CURRENT} pending={PENDING}",
                "INDEX", deviceModeIndex, "CURRENT", reportedCurrentLen,
                "PENDING", reportedPendingLen);
            return NSM_SW_ERROR_LENGTH;
        }
    }

    auto rc = decode_get_device_mode_settings_v2_resp(
        responseMsg, responseLen, &cc, &reasonCode, currentData, &currentLength,
        pendingData, &pendingLength);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        LG2_ERROR_FLT(
            "decode_get_device_mode_settings_v2_resp failed. index={INDEX} reasonCode={REASONCODE}, cc={CC}, rc={RC}",
            "INDEX", deviceModeIndex, "REASONCODE", reasonCode, "CC", cc, "RC",
            rc);
        return cc ? cc : rc;
    }

    return handleDeviceModeGetPayload(currentData, currentLength, pendingData,
                                      pendingLength);
}

NsmDeviceModeSettingsV2SetBase::NsmDeviceModeSettingsV2SetBase(
    const std::string& name, const std::string& type,
    enum device_mode_index deviceModeIndex, uint8_t patchabilityBitmap) :
    NsmDeviceModeSettingsV2Base(name, type, deviceModeIndex, patchabilityBitmap)
{}

std::optional<std::vector<uint8_t>>
    NsmDeviceModeSettingsV2SetBase::genRequestMsg(
        [[maybe_unused]] eid_t eid, [[maybe_unused]] uint8_t instanceId)
{
    return std::nullopt;
}

uint8_t NsmDeviceModeSettingsV2SetBase::handleResponseMsg(
    [[maybe_unused]] const struct nsm_msg* responseMsg,
    [[maybe_unused]] size_t responseLen)
{
    return NSM_SW_SUCCESS;
}

std::optional<std::vector<uint8_t>>
    NsmDeviceModeSettingsV2SetBase::createSetRequestMsg(
        uint8_t instanceId, const std::vector<uint8_t>& modeData) const
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) - 1 +
            modeData.size(),
        0);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_settings_v2_req(
        instanceId, static_cast<uint32_t>(deviceModeIndex), modeData.data(),
        static_cast<uint16_t>(modeData.size()), requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_set_device_mode_settings_v2_req failed. index={INDEX} rc={RC}",
            "INDEX", deviceModeIndex, "RC", rc);
        return std::nullopt;
    }

    return request;
}

NsmDPUOperationModeDeviceModeSettingsV2Get::
    NsmDPUOperationModeDeviceModeSettingsV2Get(
        const std::string& name, const std::string& type,
        uint8_t patchabilityBitmap,
        std::shared_ptr<DPUOperationModeIntf> deviceModeIntf) :
    NsmDeviceModeSettingsV2GetBase(name, type, modeIndex, patchabilityBitmap),
    deviceModeIntf(std::move(deviceModeIntf))
{}

static OperationMode rawByteToDpuOperationMode(uint8_t nsmValue)
{
    // NSM value 0 = DPU mode, 1 = NIC mode
    return nsmValue == 0 ? OperationMode::DPU : OperationMode::NIC;
}

uint8_t NsmDPUOperationModeDeviceModeSettingsV2Get::handleDeviceModeGetPayload(
    const uint8_t* currentData, uint16_t currentLength,
    const uint8_t* pendingData, uint16_t pendingLength)
{
    if (deviceModeIntf)
    {
        if (currentLength > SUB_MODE_DPU_OPERATION &&
            currentData[SUB_MODE_DPU_OPERATION] != deviceModeSettingsNoChange)
        {
            deviceModeIntf->currentMode(
                rawByteToDpuOperationMode(currentData[SUB_MODE_DPU_OPERATION]));
        }
        if (pendingLength > SUB_MODE_DPU_OPERATION &&
            pendingData[SUB_MODE_DPU_OPERATION] != deviceModeSettingsNoChange)
        {
            deviceModeIntf->pendingMode(
                rawByteToDpuOperationMode(pendingData[SUB_MODE_DPU_OPERATION]));
        }
    }

    return NSM_SW_SUCCESS;
}

NsmDPUOperationModeDeviceModeSettingsV2Set::
    NsmDPUOperationModeDeviceModeSettingsV2Set(
        const std::string& name, const std::string& type,
        uint8_t patchabilityBitmap,
        std::shared_ptr<DPUOperationModeIntf> deviceModeIntf) :
    NsmDeviceModeSettingsV2SetBase(name, type, modeIndex, patchabilityBitmap),
    deviceModeIntf(std::move(deviceModeIntf))
{}

static uint8_t dpuOperationModeToRawByte(OperationMode mode)
{
    // NSM value 0 = DPU mode, 1 = NIC mode
    return mode == OperationMode::DPU ? 0 : 1;
}

std::vector<uint8_t>
    NsmDPUOperationModeDeviceModeSettingsV2Set::buildDpuOperationModeData(
        OperationMode mode)
{
    std::vector<uint8_t> modeData(DPU_DEVICE_MODE_DATA_SIZE, 0);
    modeData[SUB_MODE_DPU_OPERATION] = dpuOperationModeToRawByte(mode);
    return modeData;
}

requester::Coroutine NsmDPUOperationModeDeviceModeSettingsV2Set::setPendingMode(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> nsmDevice)
{
    if (asyncPatchInProgress)
    {
        lg2::error("DPU setPendingMode: patch already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }
    asyncPatchInProgress = true;

    const std::string* enumStr = std::get_if<std::string>(&value);
    if (!enumStr)
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    OperationMode requestedMode;
    try
    {
        requestedMode =
            DPUOperationModeIntf::convertOperationModeFromString(*enumStr);
    }
    catch (const sdbusplus::exception::InvalidEnumString&)
    {
        lg2::error("DPU setPendingMode: invalid enum string: {STR}", "STR",
                   *enumStr);
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (!isSubDeviceModePatchable(SUB_MODE_DPU_OPERATION))
    {
        *status = AsyncOperationStatusType::Unavailable;
        asyncPatchInProgress = false;
        throw sdbusplus::error::xyz::openbmc_project::common::NotAllowed{};
    }

    auto modeData = buildDpuOperationModeData(requestedMode);
    auto request = createSetRequestMsg(0, modeData);
    if (!request)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        asyncPatchInProgress = false;
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await nsmDevice->postPatchIO(nsmDevice->getEid(), *request,
                                              responseMsg, responseLen);
    if (rc != NSM_SW_SUCCESS)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        asyncPatchInProgress = false;
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    rc = decode_set_device_mode_settings_v2_resp(responseMsg.get(), responseLen,
                                                 &cc, &reasonCode);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        asyncPatchInProgress = false;
        if (cc != NSM_SUCCESS)
        {
            co_return cc;
        }
        co_return rc;
    }

    if (deviceModeIntf)
    {
        deviceModeIntf->pendingMode(requestedMode);
    }
    *status = AsyncOperationStatusType::Success;
    asyncPatchInProgress = false;
    co_return NSM_SW_SUCCESS;
}

NsmPCIeDeviceModeDeviceModeSettingsV2Get::
    NsmPCIeDeviceModeDeviceModeSettingsV2Get(
        const std::string& name, const std::string& type,
        uint8_t patchabilityBitmap,
        std::shared_ptr<PCIeDeviceModeIntf> pcieDeviceModeIntf) :
    NsmDeviceModeSettingsV2GetBase(name, type, modeIndex, patchabilityBitmap),
    pcieDeviceModeIntf(std::move(pcieDeviceModeIntf))
{}

static EWTrafficMode rawByteToEWTrafficMode(uint8_t val)
{
    // NSM: 0 = Disabled, 1 = Enabled
    return val == 0 ? EWTrafficMode::Disabled : EWTrafficMode::Enabled;
}

uint8_t NsmPCIeDeviceModeDeviceModeSettingsV2Get::handleDeviceModeGetPayload(
    const uint8_t* currentData, uint16_t currentLength,
    const uint8_t* pendingData, uint16_t pendingLength)
{
    auto isValid = [](const uint8_t* data, uint16_t len, uint8_t offset) {
        return len >= offset + 1 &&
               data[offset] !=
                   NsmDeviceModeSettingsV2Base::deviceModeSettingsNoChange;
    };

    if (pcieDeviceModeIntf)
    {
        if (isValid(currentData, currentLength, SUB_MODE_PCIE_MULTI_SOCKET))
        {
            pcieDeviceModeIntf->PCIeMultiSocketServer::currentMode(
                currentData[SUB_MODE_PCIE_MULTI_SOCKET]);
        }
        if (isValid(pendingData, pendingLength, SUB_MODE_PCIE_MULTI_SOCKET))
        {
            pcieDeviceModeIntf->PCIeMultiSocketServer::pendingMode(
                pendingData[SUB_MODE_PCIE_MULTI_SOCKET]);
        }
        if (isValid(currentData, currentLength,
                    SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC))
        {
            pcieDeviceModeIntf->PCIeControlledEWTrafficServer::currentMode(
                rawByteToEWTrafficMode(
                    currentData[SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC]));
        }
        if (isValid(pendingData, pendingLength,
                    SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC))
        {
            pcieDeviceModeIntf->PCIeControlledEWTrafficServer::pendingMode(
                rawByteToEWTrafficMode(
                    pendingData[SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC]));
        }
        if (isValid(currentData, currentLength, SUB_MODE_PCIE_BIFURCATION))
        {
            pcieDeviceModeIntf->PCIeBifurcationServer::currentMode(
                currentData[SUB_MODE_PCIE_BIFURCATION]);
        }
        if (isValid(pendingData, pendingLength, SUB_MODE_PCIE_BIFURCATION))
        {
            pcieDeviceModeIntf->PCIeBifurcationServer::pendingMode(
                pendingData[SUB_MODE_PCIE_BIFURCATION]);
        }
    }

    return NSM_SW_SUCCESS;
}

NsmPCIeDeviceModeDeviceModeSettingsV2Set::
    NsmPCIeDeviceModeDeviceModeSettingsV2Set(
        const std::string& name, const std::string& type,
        uint8_t patchabilityBitmap,
        std::shared_ptr<PCIeDeviceModeIntf> pcieDeviceModeIntf) :
    NsmDeviceModeSettingsV2SetBase(name, type, modeIndex, patchabilityBitmap),
    pcieDeviceModeIntf(std::move(pcieDeviceModeIntf))
{}

std::vector<uint8_t>
    NsmPCIeDeviceModeDeviceModeSettingsV2Set::buildPcieDeviceModeData(
        uint8_t multiSocketsMode, uint8_t controlledEWMode,
        uint8_t bifurcationRawMode)
{
    std::vector<uint8_t> modeData(PCIE_DEVICE_MODE_DATA_SIZE, 0);
    modeData[SUB_MODE_PCIE_MULTI_SOCKET] = multiSocketsMode;
    modeData[SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC] = controlledEWMode;
    modeData[SUB_MODE_PCIE_BIFURCATION] = bifurcationRawMode;
    return modeData;
}

requester::Coroutine NsmPCIeDeviceModeDeviceModeSettingsV2Set::setPendingModes(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> nsmDevice)
{
    if (asyncPatchInProgress)
    {
        lg2::error("PCIe setPendingModes: patch already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }
    asyncPatchInProgress = true;

    const auto* entries =
        std::get_if<std::vector<std::tuple<std::string, uint32_t>>>(&value);
    if (!entries || entries->empty())
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        asyncPatchInProgress = false;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    uint8_t multiSocketsMode = deviceModeSettingsNoChange;
    uint8_t controlledEWMode = deviceModeSettingsNoChange;
    uint8_t bifurcationRawMode = deviceModeSettingsNoChange;

    for (const auto& [key, val] : *entries)
    {
        if (key == "PCIeMultiSockets")
        {
            if (!isSubDeviceModePatchable(SUB_MODE_PCIE_MULTI_SOCKET))
            {
                *status = AsyncOperationStatusType::Unavailable;
                asyncPatchInProgress = false;
                throw sdbusplus::error::xyz::openbmc_project::common::
                    NotAllowed{};
            }
            multiSocketsMode = static_cast<uint8_t>(val);
        }
        else if (key == "PCIeControlledEWTraffic")
        {
            if (!isSubDeviceModePatchable(SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC))
            {
                *status = AsyncOperationStatusType::Unavailable;
                asyncPatchInProgress = false;
                throw sdbusplus::error::xyz::openbmc_project::common::
                    NotAllowed{};
            }
            controlledEWMode = static_cast<uint8_t>(val);
        }
        else if (key == "PCIeBifurcation")
        {
            if (!isSubDeviceModePatchable(SUB_MODE_PCIE_BIFURCATION))
            {
                *status = AsyncOperationStatusType::Unavailable;
                asyncPatchInProgress = false;
                throw sdbusplus::error::xyz::openbmc_project::common::
                    NotAllowed{};
            }
            bifurcationRawMode = static_cast<uint8_t>(val);
        }
    }

    auto modeData = buildPcieDeviceModeData(multiSocketsMode, controlledEWMode,
                                            bifurcationRawMode);
    auto request = createSetRequestMsg(0, modeData);
    if (!request)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        asyncPatchInProgress = false;
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await nsmDevice->postPatchIO(nsmDevice->getEid(), *request,
                                              responseMsg, responseLen);
    if (rc != NSM_SW_SUCCESS)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        asyncPatchInProgress = false;
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    rc = decode_set_device_mode_settings_v2_resp(responseMsg.get(), responseLen,
                                                 &cc, &reasonCode);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        asyncPatchInProgress = false;
        if (cc != NSM_SUCCESS)
        {
            co_return cc;
        }
        co_return rc;
    }

    if (pcieDeviceModeIntf)
    {
        if (multiSocketsMode != deviceModeSettingsNoChange)
        {
            pcieDeviceModeIntf->PCIeMultiSocketServer::pendingMode(
                multiSocketsMode);
        }
        if (controlledEWMode != deviceModeSettingsNoChange)
        {
            pcieDeviceModeIntf->PCIeControlledEWTrafficServer::pendingMode(
                rawByteToEWTrafficMode(controlledEWMode));
        }
        if (bifurcationRawMode != deviceModeSettingsNoChange)
        {
            pcieDeviceModeIntf->PCIeBifurcationServer::pendingMode(
                bifurcationRawMode);
        }
    }

    *status = AsyncOperationStatusType::Success;
    asyncPatchInProgress = false;
    co_return NSM_SW_SUCCESS;
}

static std::string getDeviceModeObjectPath(const std::string& inventoryObjPath,
                                           const std::string& objectName)
{
    return inventoryObjPath + "Settings/Oem/Nvidia/DeviceMode/" + objectName;
}

static std::shared_ptr<DPUOperationModeIntf> createDPUOperationModeInterface(
    sdbusplus::bus::bus& bus, const std::string& inventoryObjPath,
    const std::string& networkAdapterPath, bool isModeConfigurable)
{
    auto path = getDeviceModeObjectPath(inventoryObjPath, "DPUOperationMode");
    auto intf = std::make_shared<DPUOperationModeIntf>(bus, path.c_str());
    intf->currentMode(OperationMode::DPU);
    intf->pendingMode(OperationMode::DPU);
    intf->isModeConfigurable(isModeConfigurable);
    intf->associations(
        {{"network_adapter", "device_mode_settings", networkAdapterPath}});
    return intf;
}

static std::shared_ptr<PCIeDeviceModeIntf> createPCIeDeviceModeInterface(
    sdbusplus::bus::bus& bus, const std::string& inventoryObjPath,
    const std::string& networkAdapterPath, uint8_t pcieModeBitmap)
{
    auto path = getDeviceModeObjectPath(inventoryObjPath, "PCIeDeviceMode");
    auto intf = std::make_shared<PCIeDeviceModeIntf>(bus, path.c_str());
    intf->PCIeMultiSocketServer::currentMode(PCIE_DEVICE_MODE_SINGLE_SOCKET);
    intf->PCIeMultiSocketServer::pendingMode(PCIE_DEVICE_MODE_SINGLE_SOCKET);
    intf->PCIeMultiSocketServer::isModeConfigurable(
        NsmDeviceModeSettingsV2Base::isModeBitSet(pcieModeBitmap,
                                                  SUB_MODE_PCIE_MULTI_SOCKET));
    intf->PCIeControlledEWTrafficServer::currentMode(EWTrafficMode::Disabled);
    intf->PCIeControlledEWTrafficServer::pendingMode(EWTrafficMode::Disabled);
    intf->PCIeControlledEWTrafficServer::isModeConfigurable(
        NsmDeviceModeSettingsV2Base::isModeBitSet(
            pcieModeBitmap, SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC));
    intf->PCIeBifurcationServer::currentMode(PCIE_DEVICE_MODE_NO_BIFURCATION);
    intf->PCIeBifurcationServer::pendingMode(PCIE_DEVICE_MODE_NO_BIFURCATION);
    intf->PCIeBifurcationServer::isModeConfigurable(
        NsmDeviceModeSettingsV2Base::isModeBitSet(pcieModeBitmap,
                                                  SUB_MODE_PCIE_BIFURCATION));
    intf->associations(
        {{"network_adapter", "device_mode_settings", networkAdapterPath}});
    return intf;
}

static void registerPendingModeHandler(
    const std::string& objectPath, const std::string& interface,
    nsm::AsyncSetOperationHandler handler, std::shared_ptr<NsmSensor> setSensor,
    const std::shared_ptr<NsmDevice>& nsmDevice)
{
    AsyncOperationManager::getInstance()
        ->getDispatcher(objectPath)
        ->addAsyncSetOperation(
            interface, "PendingMode",
            AsyncSetOperationInfo{handler, std::move(setSensor), nsmDevice});
}

static void createDpuModeSensors(sdbusplus::bus::bus& bus,
                                 const std::shared_ptr<NsmDevice>& nsmDevice,
                                 const std::string& name,
                                 const std::string& type,
                                 const std::string& inventoryObjPath,
                                 uint8_t dpuModeBitmap,
                                 const std::string& networkAdapterPath)
{
    auto dpuDeviceModeIntf = createDPUOperationModeInterface(
        bus, inventoryObjPath, networkAdapterPath,
        NsmDeviceModeSettingsV2Base::isModeBitSet(dpuModeBitmap,
                                                  SUB_MODE_DPU_OPERATION));

    auto dpuGetSensor =
        std::make_shared<NsmDPUOperationModeDeviceModeSettingsV2Get>(
            name + "_DPUOperationMode_Get", type, dpuModeBitmap,
            dpuDeviceModeIntf);
    nsmDevice->addSensor(dpuGetSensor, false);

    auto dpuSetSensor =
        std::make_shared<NsmDPUOperationModeDeviceModeSettingsV2Set>(
            name + "_DPUOperationMode_Set", type, dpuModeBitmap,
            dpuDeviceModeIntf);
    nsmDevice->addSensor(dpuSetSensor, false);

    nsm::AsyncSetOperationHandler setDpuOperationModeHandler =
        std::bind(&NsmDPUOperationModeDeviceModeSettingsV2Set::setPendingMode,
                  dpuSetSensor, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3);
    registerPendingModeHandler(
        getDeviceModeObjectPath(inventoryObjPath, "DPUOperationMode"),
        std::string(DPUOperationModeServer::interface),
        setDpuOperationModeHandler, dpuSetSensor, nsmDevice);
}

static void createPcieModeSensors(sdbusplus::bus::bus& bus,
                                  const std::shared_ptr<NsmDevice>& nsmDevice,
                                  const std::string& name,
                                  const std::string& type,
                                  const std::string& inventoryObjPath,
                                  uint8_t pcieModeBitmap,
                                  const std::string& networkAdapterPath)
{
    auto pcieDeviceModeIntf = createPCIeDeviceModeInterface(
        bus, inventoryObjPath, networkAdapterPath, pcieModeBitmap);

    auto pcieGetSensor =
        std::make_shared<NsmPCIeDeviceModeDeviceModeSettingsV2Get>(
            name + "_PCIeDeviceMode_Get", type, pcieModeBitmap,
            pcieDeviceModeIntf);
    nsmDevice->addSensor(pcieGetSensor, false);

    auto pcieSetSensor =
        std::make_shared<NsmPCIeDeviceModeDeviceModeSettingsV2Set>(
            name + "_PCIeDeviceMode_Set", type, pcieModeBitmap,
            pcieDeviceModeIntf);
    nsmDevice->addSensor(pcieSetSensor, false);

    std::string pcieDeviceModeObjPath =
        getDeviceModeObjectPath(inventoryObjPath, "PCIeDeviceMode");
    nsm::AsyncSetOperationHandler setPcieModesHandler =
        std::bind(&NsmPCIeDeviceModeDeviceModeSettingsV2Set::setPendingModes,
                  pcieSetSensor, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3);
    AsyncOperationManager::getInstance()
        ->getDispatcher(pcieDeviceModeObjPath)
        ->addAsyncSetOperation("com.nvidia.DeviceMode.PCIeDeviceMode",
                               "PendingModes",
                               AsyncSetOperationInfo{setPcieModesHandler,
                                                     pcieSetSensor, nsmDevice});
}

static void createDeviceModeSensors(
    sdbusplus::bus::bus& bus, const std::shared_ptr<NsmDevice>& nsmDevice,
    const std::string& name, const std::string& type,
    const std::string& inventoryObjPath, bool hasDpuModeSupport,
    uint8_t dpuModeBitmap, bool hasPcieModeSupport, uint8_t pcieModeBitmap,
    const std::string& networkAdapterPath)
{
    if (hasDpuModeSupport)
    {
        createDpuModeSensors(bus, nsmDevice, name, type, inventoryObjPath,
                             dpuModeBitmap, networkAdapterPath);
    }
    if (hasPcieModeSupport)
    {
        createPcieModeSensors(bus, nsmDevice, name, type, inventoryObjPath,
                              pcieModeBitmap, networkAdapterPath);
    }
}

requester::Coroutine createNSMNetworkAdapter(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    std::string type{};
    if (allCurrentIfaceProperties.count("Type"))
    {
        type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
    }
    std::string inventoryObjPath{};
    if (allCurrentIfaceProperties.count("InventoryObjPath"))
    {
        inventoryObjPath = std::get<std::string>(
            allCurrentIfaceProperties.at("InventoryObjPath"));
    }

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);
    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);

    auto networkAdapterDI = std::make_shared<NsmNetworkAdapterDI>(
        bus, name, associations, type, inventoryObjPath);
    nsmDevice->addDeviceSensors(networkAdapterDI);

// Network adapters (CX/BlueField NICs) expose DebugInfo + LogInfo; Erase is
// omitted because their trace storage is volatile (erase would be a no-op).
#if defined(ENABLE_DEBUG_INFO)
    auto networkAdapterDebugInfoObject = std::make_shared<NsmDebugInfoObject>(
        bus, name, inventoryObjPath, type, uuid, DebugDumpType::Network);
    nsmDevice->addStaticSensor(networkAdapterDebugInfoObject);

    auto networkAdapterLogInfoObject = std::make_shared<NsmLogInfoObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterLogInfoObject);
#endif

#if defined(ENABLE_ERROR_INJECTION)
    createNsmErrorInjectionSensors(manager, nsmDevice,
                                   path(inventoryObjPath) / name);
#endif

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
    auto ntwAdpResetSensor = std::make_shared<NsmNetworkAdapterDIReset>(
        bus, name, type, inventoryObjPath, nsmDevice);
    nsmDevice->addDeviceSensors(ntwAdpResetSensor);
#endif

#if defined(ENABLE_NETWORK_ADAPTER_PROTECTION_OPTION)
    createDeviceProtectionOptions(nsmDevice, type, inventoryObjPath, name);
#endif

    if (allCurrentIfaceProperties.count("DeviceModesSupported"))
    {
        // DeviceModesSupported: DEVICE_MODE_NOT_SUPPORTED (-1) = mode not
        // supported;  >= 0 = patchability bitmap.
        auto deviceModesSupported = std::get<std::vector<int64_t>>(
            allCurrentIfaceProperties.at("DeviceModesSupported"));

        auto isModeSupported = [&](size_t modeIndex) -> bool {
            return modeIndex < deviceModesSupported.size() &&
                   deviceModesSupported[modeIndex] != DEVICE_MODE_NOT_SUPPORTED;
        };

        auto getPatchabilityBitmap = [&](size_t modeIndex) -> uint8_t {
            if (modeIndex < deviceModesSupported.size() &&
                deviceModesSupported[modeIndex] != DEVICE_MODE_NOT_SUPPORTED)
            {
                return static_cast<uint8_t>(deviceModesSupported[modeIndex]);
            }
            return 0;
        };

        bool dpuSupported = isModeSupported(DEVICE_MODE_DPU_OPERATION_MODE);
        bool pcieSupported = isModeSupported(DEVICE_MODE_PCIE_DEVICE_MODE);
        uint8_t dpuPatchability =
            getPatchabilityBitmap(DEVICE_MODE_DPU_OPERATION_MODE);
        uint8_t pciePatchability =
            getPatchabilityBitmap(DEVICE_MODE_PCIE_DEVICE_MODE);

        std::string networkAdapterObjPath = inventoryObjPath + name + "/";
        std::string networkAdapterPath = inventoryObjPath + name;
        createDeviceModeSensors(bus, nsmDevice, name, type,
                                networkAdapterObjPath, dpuSupported,
                                dpuPatchability, pcieSupported,
                                pciePatchability, networkAdapterPath);

        // OOB Miswiring Detection
#if defined(ENABLE_LLDP)
        if (isModeSupported(DEVICE_MODE_LLDP))
        {
            createLldpModeSensor(bus, nsmDevice, name, type,
                                 networkAdapterPath);
        }
#endif
    }

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
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
#endif

REGISTER_NSM_CREATION_FUNCTION(
    createNSMNetworkAdapter,
    "xyz.openbmc_project.Configuration.NSM_NetworkAdapter")

} // namespace nsm
