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
        lg2::error("encode_get_powersmoothing_featinfo_req failed. "
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
        lg2::error("nsm_pwr_smoothing_featureinfo_data data is null");
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
        lg2::error("encode_get_hardware_lifetime_cricuitry_req failed. "
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
        lg2::error("nsm_hardwarecircuitry_data data is null");
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
        lg2::error("encode_get_current_profile_info_req failed. "
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
        lg2::error("nsm_get_current_profile_data data is null");
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
        lg2::error("encode_query_admin_override_req failed. "
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
        lg2::error("nsm_admin_override_data data is null");
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
    lg2::error("getSupportedProfileById: ProfileId not found: {ID}", "ID",
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
        lg2::error("encode_get_preset_profile_req failed. "
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

} // namespace nsm
