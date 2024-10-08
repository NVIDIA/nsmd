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
#include "config.h"

#include "nsmPowerSmoothing.hpp"

#include <cstdint>

namespace nsm
{
NsmPowerSmoothing::NsmPowerSmoothing(
    std::string& name, std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<OemPowerSmoothingFeatIntf> pwrSmoothingIntf) :
    NsmSensor(name, type),
    pwrSmoothingIntf(pwrSmoothingIntf), inventoryObjPath(inventoryObjPath)
{}

std::optional<std::vector<uint8_t>>
    NsmPowerSmoothing::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_powersmoothing_featinfo_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPowerSmoothing::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    nsm_pwr_smoothing_featureinfo_data data{};

    auto rc = decode_get_powersmoothing_featinfo_resp(
        responseMsg, responseLen, &cc, &reason_code, &dataSize, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(&data);
        clearErrorBitMap("decode_get_powersmoothing_featinfo_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_powersmoothing_featinfo_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

void NsmPowerSmoothing::updateReading(
    struct nsm_pwr_smoothing_featureinfo_data* data)
{
    if (data == nullptr)
    {
        lg2::debug("nsm_pwr_smoothing_featureinfo_data data is null");
        return;
    }
    // Update values on iface

    // For feature Supported : Check if bit0 is set
    bool featSupported = (data->feature_flag & (1u << 0)) != 0 ? true : false;
    pwrSmoothingIntf->PowerSmoothingIntf::featureSupported(featSupported);

    // For feature Enabled: Check if bit1 is set
    bool featureEnabled = (data->feature_flag & (1u << 1)) != 0 ? true : false;
    pwrSmoothingIntf->PowerSmoothingIntf::powerSmoothingEnabled(featureEnabled);

    // For RampDwon Enabled: Check if bit2 is set
    bool rampDownEnabled = (data->feature_flag & (1u << 2)) != 0 ? true : false;
    pwrSmoothingIntf->PowerSmoothingIntf::immediateRampDownEnabled(
        rampDownEnabled);

    pwrSmoothingIntf->PowerSmoothingIntf::currentTempSetting(
        data->currentTmpSetting);
    pwrSmoothingIntf->PowerSmoothingIntf::currentTempFloorSetting(
        data->currentTmpFloorSetting);
    pwrSmoothingIntf->PowerSmoothingIntf::maxAllowedTmpFloorPercent(
        NvUFXP4_12ToDouble(data->maxTmpFloorSettingInPercent));
    pwrSmoothingIntf->PowerSmoothingIntf::minAllowedTmpFloorPercent(
        NvUFXP4_12ToDouble(data->minTmpFloorSettingInPercent));
}

// HW lifetime usage
NsmHwCircuitryTelemetry::NsmHwCircuitryTelemetry(
    std::string& name, std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<PowerSmoothingIntf> pwrSmoothingIntf) :
    NsmSensor(name, type),
    pwrSmoothingIntf(pwrSmoothingIntf), inventoryObjPath(inventoryObjPath)
{}

std::optional<std::vector<uint8_t>>
    NsmHwCircuitryTelemetry::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_hardware_lifetime_cricuitry_req(instanceId,
                                                         requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_hardware_lifetime_cricuitry_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmHwCircuitryTelemetry::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    nsm_hardwarecircuitry_data data{};

    auto rc = decode_get_hardware_lifetime_cricuitry_resp(
        responseMsg, responseLen, &cc, &reason_code, &dataSize, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(&data);
        clearErrorBitMap("decode_get_hardware_lifetime_cricuitry_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_hardware_lifetime_cricuitry_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

void NsmHwCircuitryTelemetry::updateReading(
    struct nsm_hardwarecircuitry_data* data)
{
    if (data == nullptr)
    {
        lg2::debug("nsm_hardwarecircuitry_data data is null");
        return;
    }
    // Update values on iface
    pwrSmoothingIntf->PowerSmoothingIntf::lifeTimeRemaining(
        NvUFXP8_24ToDouble(data->reading));
}

//  Power Smoothing Control: Get Current Profile Information
NsmCurrentPowerSmoothingProfile::NsmCurrentPowerSmoothingProfile(
    std::string& name, std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<OemCurrentPowerProfileIntf> pwrSmoothingCurProfileIntf,
    std::shared_ptr<NsmPowerProfileCollection>
        pwrSmoothingSupportedCollectionSensor,
    std::shared_ptr<NsmPowerSmoothingAdminOverride> adminProfileSensor) :
    NsmSensor(name, type),
    pwrSmoothingCurProfileIntf(pwrSmoothingCurProfileIntf),
    pwrSmoothingSupportedCollectionSensor(
        pwrSmoothingSupportedCollectionSensor),
    adminProfileSensor(adminProfileSensor), inventoryObjPath(inventoryObjPath)

{}

std::optional<std::vector<uint8_t>>
    NsmCurrentPowerSmoothingProfile::genRequestMsg(eid_t eid,
                                                   uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_current_profile_info_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_current_profile_info_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmCurrentPowerSmoothingProfile::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    nsm_get_current_profile_data data{};

    auto rc = decode_get_current_profile_info_resp(
        responseMsg, responseLen, &cc, &reason_code, &dataSize, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(&data);
        clearErrorBitMap("decode_get_current_profile_info_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_current_profile_info_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}
std::string NsmCurrentPowerSmoothingProfile::getProfilePath(uint8_t profileId)
{
    std::string profilePath = inventoryObjPath;
    if (pwrSmoothingSupportedCollectionSensor->hasProfileId(profileId))
    {
        profilePath =
            pwrSmoothingSupportedCollectionSensor->getProfilePathByProfileId(
                profileId);
    }
    return profilePath;
}

void NsmCurrentPowerSmoothingProfile::updateReading(
    struct nsm_get_current_profile_data* data)
{
    if (data == nullptr)
    {
        lg2::debug("nsm_get_current_profile_data data is null");
        return;
    }
    // Update values on iface

    pwrSmoothingCurProfileIntf->CurrentPowerProfileIntf::tmpFloorPercent(
        NvUFXP4_12ToDouble(data->current_percent_tmp_floor));
    pwrSmoothingCurProfileIntf->CurrentPowerProfileIntf::tmpFloorPercentApplied(
        data->admin_override_mask.bits.tmp_floor_override);

    pwrSmoothingCurProfileIntf->CurrentPowerProfileIntf::rampUpRate(
        data->current_rampup_rate_in_miliwatts_per_second);
    pwrSmoothingCurProfileIntf->CurrentPowerProfileIntf::rampUpRateApplied(
        data->admin_override_mask.bits.rampup_rate_override);

    pwrSmoothingCurProfileIntf->CurrentPowerProfileIntf::rampDownRate(
        data->current_rampdown_rate_in_miliwatts_per_second);
    pwrSmoothingCurProfileIntf->CurrentPowerProfileIntf::rampDownRateApplied(
        data->admin_override_mask.bits.rampdown_rate_override);

    pwrSmoothingCurProfileIntf->CurrentPowerProfileIntf::rampDownHysteresis(
        data->current_rampdown_hysteresis_value_in_milisec);
    pwrSmoothingCurProfileIntf
        ->CurrentPowerProfileIntf::rampDownHysteresisApplied(
            data->admin_override_mask.bits.hysteresis_value_override);

    pwrSmoothingCurProfileIntf->CurrentPowerProfileIntf::appliedProfilePath(
        getProfilePath(data->current_active_profile_id));
}

// Query Admin overrides
NsmPowerSmoothingAdminOverride::NsmPowerSmoothingAdminOverride(
    std::string& name, std::string& type,
    std::shared_ptr<OemAdminProfileIntf> adminProfileIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type),
    adminProfileIntf(adminProfileIntf), inventoryObjPath(inventoryObjPath)
{}

std::optional<std::vector<uint8_t>>
    NsmPowerSmoothingAdminOverride::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_admin_override_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_query_admin_override_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPowerSmoothingAdminOverride::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    nsm_admin_override_data data{};

    auto rc = decode_query_admin_override_resp(responseMsg, responseLen, &cc,
                                               &reason_code, &dataSize, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(&data);
        clearErrorBitMap("decode_query_admin_override_resp");
    }
    else
    {
        logHandleResponseMsg("decode_query_admin_override_resp", reason_code,
                             cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

void NsmPowerSmoothingAdminOverride::updateReading(
    struct nsm_admin_override_data* data)
{
    if (data == nullptr)
    {
        lg2::debug("nsm_admin_override_data data is null");
        return;
    }
    // Update values on iface
    adminProfileIntf->AdminPowerProfileIntf::tmpFloorPercent(
        NvUFXP4_12ToDouble(data->admin_override_percent_tmp_floor));
    adminProfileIntf->AdminPowerProfileIntf::rampUpRate(
        data->admin_override_ramup_rate_in_miliwatts_per_second);
    adminProfileIntf->AdminPowerProfileIntf::rampDownRate(
        data->admin_override_rampdown_rate_in_miliwatts_per_second);
    adminProfileIntf->AdminPowerProfileIntf::rampDownHysteresis(
        data->admin_override_rampdown_hysteresis_value_in_milisec);
}

std::string NsmPowerSmoothingAdminOverride::getInventoryObjPath()
{
    return inventoryObjPath;
}

//  Get Preset Profile Information

NsmPowerProfileCollection::NsmPowerProfileCollection(
    std::string& name, std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<NsmDevice> device) :
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath), device(device)
{}

bool NsmPowerProfileCollection::hasProfileId(uint8_t profileId)

{
    return supportedPowerProfiles.find(profileId) !=
           supportedPowerProfiles.end();
}

std::shared_ptr<OemPowerProfileIntf>
    NsmPowerProfileCollection::getSupportedProfileById(uint8_t profileId)
{
    auto it = supportedPowerProfiles.find(profileId);
    if (it != supportedPowerProfiles.end())
    {
        return it->second;
    }
    lg2::debug("getSupportedProfileById: ProfileId not found: {ID}", "ID",
               profileId);
    throw std::out_of_range("profileId not found in map");
}

void NsmPowerProfileCollection::addSupportedProfile(
    uint8_t profileId, std::shared_ptr<OemPowerProfileIntf> obj)

{
    supportedPowerProfiles[profileId] = obj;
}

void NsmPowerProfileCollection::updateSupportedProfile(
    std::shared_ptr<OemPowerProfileIntf> obj, nsm_preset_profile_data* data)

{
    if (obj)
    {
        obj->PowerProfileIntf::tmpFloorPercent(
            NvUFXP4_12ToDouble(data->tmp_floor_setting_in_percent));
        obj->PowerProfileIntf::rampUpRate(
            data->ramp_up_rate_in_miliwattspersec);
        obj->PowerProfileIntf::rampDownRate(
            data->ramp_down_rate_in_miliwattspersec);
        obj->PowerProfileIntf::rampDownHysteresis(
            data->ramp_hysterisis_rate_in_miliwattspersec);
    }
}

std::string
    NsmPowerProfileCollection::getProfilePathByProfileId(uint8_t profileId)

{
    if (hasProfileId(profileId))
        return supportedPowerProfiles[profileId]->getInventoryObjPath();
    return "/";
}

std::optional<std::vector<uint8_t>>
    NsmPowerProfileCollection::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_preset_profile_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_preset_profile_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPowerProfileCollection::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    nsm_get_all_preset_profile_meta_data data{};
    uint8_t numberOfprofiles = 0;

    auto rc = decode_get_preset_profile_metadata_resp(
        responseMsg, responseLen, &cc, &reason_code, &data, &numberOfprofiles);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        for (int profileId = 0; profileId < numberOfprofiles; profileId++)
        {
            nsm_preset_profile_data profileData{};
            decode_get_preset_profile_data_from_resp(
                responseMsg, responseLen, &cc, &reason_code, numberOfprofiles,
                profileId, &profileData);
            if (!hasProfileId(profileId))
            {
                auto powerProfile = std::make_shared<OemPowerProfileIntf>(
                    utils::DBusHandler::getBus(), inventoryObjPath, profileId,
                    device);

                AsyncOperationManager::getInstance()
                    ->getDispatcher(powerProfile->getInventoryObjPath())
                    ->addAsyncSetOperation(
                        powerProfile->PowerProfileIntf::interface,
                        "TMPFloorPercent",
                        AsyncSetOperationInfo{
                            std::bind_front(
                                &OemPowerProfileIntf::setTmpFloorPercent,
                                powerProfile),
                            {},
                            device});
                AsyncOperationManager::getInstance()
                    ->getDispatcher(powerProfile->getInventoryObjPath())
                    ->addAsyncSetOperation(
                        powerProfile->PowerProfileIntf::interface, "RampUpRate",
                        AsyncSetOperationInfo{
                            std::bind_front(&OemPowerProfileIntf::setRampUpRate,
                                            powerProfile),
                            {},
                            device});
                AsyncOperationManager::getInstance()
                    ->getDispatcher(powerProfile->getInventoryObjPath())
                    ->addAsyncSetOperation(
                        powerProfile->PowerProfileIntf::interface,
                        "RampDownRate",
                        AsyncSetOperationInfo{
                            std::bind_front(
                                &OemPowerProfileIntf::setRampDownRate,
                                powerProfile),
                            {},
                            device});
                AsyncOperationManager::getInstance()
                    ->getDispatcher(powerProfile->getInventoryObjPath())
                    ->addAsyncSetOperation(
                        powerProfile->PowerProfileIntf::interface,
                        "RampDownHysteresis",
                        AsyncSetOperationInfo{
                            std::bind_front(
                                &OemPowerProfileIntf::setRampDownHysteresis,
                                powerProfile),
                            {},
                            device});
                addSupportedProfile(profileId, powerProfile);
            }
            updateSupportedProfile(getSupportedProfileById(profileId),
                                   &profileData);
        }
        clearErrorBitMap("decode_get_preset_profile_metadata_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_preset_profile_metadata_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

NsmPowerSmoothingAction::NsmPowerSmoothingAction(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    std::string& inventoryObjPath, std::shared_ptr<NsmDevice> device) :
    NsmObject(name, type),
    ProfileActionAsyncIntf(bus, inventoryObjPath.c_str()), device(device),
    inventoryObjPath(inventoryObjPath)
{}

requester::Coroutine NsmPowerSmoothingAction::requestActivatePresetProfile(
    AsyncOperationStatusType* status, uint16_t& profileID)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_active_preset_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    // first argument instanceid=0 is irrelevant
    auto rc = encode_set_active_preset_profile_req(0, profileID, requestMsg);

    if (rc)
    {
        lg2::error(
            "requestActivatePresetProfile  encode_set_active_preset_profile_req failed. eid={EID}, rc={RC}",
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
            "requestActivatePresetProfile SendRecvNsmMsgSync failed for for eid = {EID} rc = {RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_set_active_preset_profile_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info("requestActivatePresetProfile for EID: {EID} completed",
                  "EID", eid);
    }
    else
    {
        lg2::error(
            "requestActivatePresetProfile decode_set_active_preset_profile_resp failed.eid ={EID},CC = {CC} reasoncode = {RC},RC = {A} ",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmPowerSmoothingAction::doActivatePresetProfile(
    std::shared_ptr<AsyncStatusIntf> statusInterface, uint16_t& profileID)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    auto rc_ = co_await requestActivatePresetProfile(&status, profileID);

    statusInterface->status(status);

    co_return rc_;
}

sdbusplus::message::object_path
    NsmPowerSmoothingAction::activatePresetProfile(uint16_t profileID)
{
    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        lg2::error(
            "NsmPowerSmoothingAction::activatePresetProfile failed. No available result Object to allocate for the Post request.");
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    doActivatePresetProfile(statusInterface, profileID).detach();

    return objectPath;
}

requester::Coroutine NsmPowerSmoothingAction::requestApplyAdminOverride(
    AsyncOperationStatusType* status)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    lg2::info("requestApplyAdminOverride for EID: {EID}", "EID", eid);

    // first argument instanceid=0 is irrelevant
    auto rc = encode_apply_admin_override_req(0, requestMsg);

    if (rc)
    {
        lg2::error(
            "requestActivatePresetProfile  encode_apply_admin_override_req failed. eid={EID}, rc={RC}",
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
            "requestActivatePresetProfile SendRecvNsmMsgSync failed for for eid = {EID} rc = {RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_apply_admin_override_resp(responseMsg.get(), responseLen, &cc,
                                          &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info("requestActivatePresetProfile for EID: {EID} completed",
                  "EID", eid);
    }
    else
    {
        lg2::error(
            "requestActivatePresetProfile decode_set_active_preset_profile_resp failed.eid ={EID},CC = {CC} reasoncode = {RC},RC = {A} ",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmPowerSmoothingAction::doApplyAdminOverride(
    std::shared_ptr<AsyncStatusIntf> statusInterface)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    auto rc_ = co_await requestApplyAdminOverride(&status);

    statusInterface->status(status);

    co_return rc_;
}

sdbusplus::message::object_path NsmPowerSmoothingAction::applyAdminOverride()
{
    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        lg2::error(
            "NsmPowerSmoothingAction::applyAdminOverride failed. No available result Object to allocate for the Post request.");
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    doApplyAdminOverride(statusInterface).detach();

    return objectPath;
}

} // namespace nsm
