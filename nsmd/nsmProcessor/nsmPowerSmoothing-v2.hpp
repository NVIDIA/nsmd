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

#pragma once
#include "nsmPowerSmoothing.hpp"

#include <xyz/openbmc_project/Inventory/Decorator/Revision/server.hpp>

namespace nsm
{

// Power Smoothing Control: Get Supported Version
using RevisionIntf = object_t<Inventory::Decorator::server::Revision>;

class NsmPowerSmoothingSupportedVersion : public NsmSensor
{
  public:
    NsmPowerSmoothingSupportedVersion(
        std::string& name, std::string& type, std::string& inventoryObjPath,
        uint8_t messageType, std::shared_ptr<RevisionIntf> revisionIntf) :
        NsmSensor(name, type),
        revisionIntf(revisionIntf), inventoryObjPath(inventoryObjPath),
        messageType(messageType)
    {}

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override
    {
        std::vector<uint8_t> request(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req));
        auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_get_supported_command_codes_req(
            instanceId, messageType, requestPtr);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("genRequestMsg failed. eid={EID} rc={RC}", "EID", eid,
                       "RC", rc);
            return std::nullopt;
        }
        return request;
    }
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        bitfield8_t supportedCommandCodes[SUPPORTED_COMMAND_CODE_DATA_SIZE];
        auto rc = decode_get_supported_command_codes_resp(
            responseMsg, responseLen, &cc, &reason_code, supportedCommandCodes);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            lg2::error(
                "handleResponseMsg failed. eid={EID} cc={CC} reasonCode={REASONCODE} and rc={RC}",
                "CC", cc, "REASONCODE", reason_code, "RC", rc);
            return cc ? cc : rc;
        }
        std::string version;
        for (int i = 0; i < SUPPORTED_COMMAND_CODE_DATA_SIZE; i++)
        {
            auto isSupported = supportedCommandCodes[i / 8].byte &
                               (1 << (i % 8));
            if (isSupported && i == NSM_PWR_SMOOTHING_GET_FEATURE_INFO)
            {
                version += std::to_string(i);
            }
        }
        if (supportedCommandCodes[NSM_PWR_SMOOTHING_GET_FEATURE_INFO / 8].byte &
                (1 << (NSM_PWR_SMOOTHING_GET_FEATURE_INFO % 8)) &&
            supportedCommandCodes[NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE / 8]
                    .byte &
                (1 << (NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE % 8)) &&
            supportedCommandCodes
                    [NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION / 8]
                        .byte &
                (1 << (NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION % 8)) &&
            supportedCommandCodes
                    [NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION / 8]
                        .byte &
                (1 << (NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION % 8)))
        {
            revisionIntf->version("V1");
        }
        else if (
            supportedCommandCodes[NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2 / 8]
                    .byte &
                (1 << (NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2 % 8)) &&
            supportedCommandCodes[NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2 / 8]
                    .byte &
                (1 << (NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2 % 8)) &&
            supportedCommandCodes
                    [NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2 / 8]
                        .byte &
                (1 << (NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2 %
                       8)) &&
            supportedCommandCodes
                    [NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2 / 8]
                        .byte &
                (1 << (NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2 %
                       8)))
        {
            revisionIntf->version("V2");
        }
        else
        {
            revisionIntf->version("nan");
        }

        return NSM_SW_SUCCESS;
    }
    std::string getSupportedVersion()
    {
        return revisionIntf->version();
    }

  private:
    std::shared_ptr<RevisionIntf> revisionIntf;
    std::string inventoryObjPath;
    uint8_t messageType;
};

class NsmPowerSmoothingV2 : public NsmPowerSmoothing
{
  public:
    NsmPowerSmoothingV2(
        std::string& name, std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<OemPowerSmoothingFeatIntf> pwrSmoothingIntf,
        std::shared_ptr<NsmDevice> device) :
        NsmPowerSmoothing(name, type, inventoryObjPath, pwrSmoothingIntf,
                          device)
    {}

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override
    {
        if (!NsmPowerSmoothing::getDevice()->isCommandSupported(
                NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2))
        {
            return NsmPowerSmoothing::genRequestMsg(eid, instanceId);
        }
        int rc = NSM_SW_SUCCESS;
        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_req));
        auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
        rc = encode_get_powersmoothing_featinfo_v2_req(instanceId, requestPtr);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug("encode_get_powersmoothing_featinfo_req failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);
            return std::nullopt;
        }
        return request;
    }

    struct TelemetrySample
    {
        uint8_t tag;
        uint8_t data_len;
        const uint8_t* data;
        bool valid;
    };

    int updateFeatureFlagSample(const TelemetrySample& sample)
    {
        uint32_t featureFlag;
        int rc = NSM_SW_SUCCESS;
        rc = decodeFeatureFlagSample(sample.data, sample.data_len,
                                     &featureFlag);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Feature Flag. RC={RC}", "RC", rc);
            return rc;
        }
        bool featureSupported = (featureFlag & (1u << 0)) != 0 ? true : false;
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::featureSupported(featureSupported);
        bool featureEnabled = (featureFlag & (1u << 1)) != 0 ? true : false;
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::powerSmoothingEnabled(featureEnabled);
        bool rampDownEnabled = (featureFlag & (1u << 2)) != 0 ? true : false;
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::immediateRampDownEnabled(rampDownEnabled);
        bool delayedPowerSmoothing = (featureFlag & (1u << 3)) != 0 ? true
                                                                    : false;
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::delayedPowerSmoothingSupported(
                delayedPowerSmoothing);
        return rc;
    }

    int updateCurrentTMPSettingSample(const TelemetrySample& sample)
    {
        uint32_t currentTmpSetting;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentTMPSettingSample(sample.data, sample.data_len,
                                           &currentTmpSetting);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Current TMP Setting. RC={RC}", "RC",
                       rc);
            return rc;
        }
        // mw/sec to watts/sec
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::currentTempSetting(
                utils::convertAndScaleDownUint32ToDouble(currentTmpSetting,
                                                         1000));
        return rc;
    }

    int updateCurrentTMPFloorSettingSample(const TelemetrySample& sample)
    {
        uint32_t currentTmpFloorSetting;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentTMPFloorSettingSample(sample.data, sample.data_len,
                                                &currentTmpFloorSetting);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Current TMP Floor Setting. RC={RC}",
                       "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::currentTempFloorSetting(
                utils::convertAndScaleDownUint32ToDouble(currentTmpFloorSetting,
                                                         1000));
        return rc;
    }

    int updateMaxTmpFloorSettingSample(const TelemetrySample& sample)
    {
        uint16_t maxTmpFloorSetting;
        int rc = NSM_SW_SUCCESS;
        rc = decodeMaxTmpFloorSettingSample(sample.data, sample.data_len,
                                            &maxTmpFloorSetting);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Max TMP Floor Setting. RC={RC}", "RC",
                       rc);
            return rc;
        }
        // fraction to percent
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::maxAllowedTmpFloorPercent(
                NvUFXP4_12ToDouble(maxTmpFloorSetting) * 100);
        return rc;
    }

    int updateMinTmpFloorSettingSample(const TelemetrySample& sample)
    {
        uint16_t minTmpFloorSetting;
        int rc = NSM_SW_SUCCESS;
        rc = decodeMinTmpFloorSettingSample(sample.data, sample.data_len,
                                            &minTmpFloorSetting);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Min TMP Floor Setting. RC={RC}", "RC",
                       rc);
            return rc;
        }
        // fraction to percent
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::minAllowedTmpFloorPercent(
                NvUFXP4_12ToDouble(minTmpFloorSetting) * 100);
        return rc;
    }

    int updateFloorWindowMultiplierSample(const TelemetrySample& sample)
    {
        uint32_t floorWindowMultiplier;
        int rc = NSM_SW_SUCCESS;
        rc = decodeFloorWindowMultiplierSample(sample.data, sample.data_len,
                                               &floorWindowMultiplier);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode Current Secondary Floor Setting. RC={RC}",
                "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        double reading = utils::convertAndScaleDownUint32ToDouble(
            floorWindowMultiplier, 1000);
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::floorWindowMultiplier(reading);
        return rc;
    }

    int updateMinPrimaryFloorActivationOffsetSample(
        const TelemetrySample& sample)
    {
        uint32_t minPrimaryFloorActivationOffset;
        int rc = NSM_SW_SUCCESS;
        rc = decodeMinPrimaryFloorActivationOffset(
            sample.data, sample.data_len, &minPrimaryFloorActivationOffset);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode Min Primary Floor Activation Offset. RC={RC}",
                "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::minPrimaryFloorActivationOffset(
                utils::convertAndScaleDownUint32ToDouble(
                    minPrimaryFloorActivationOffset, 1000));
        return rc;
    }

    int updateMinPrimaryFloorActivationPointSample(
        const TelemetrySample& sample)
    {
        uint32_t minPrimaryFloorActivationPoint;
        int rc = NSM_SW_SUCCESS;
        rc = decodeMinPrimaryFloorActivationPoint(
            sample.data, sample.data_len, &minPrimaryFloorActivationPoint);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode Min Primary Floor Activation Point. RC={RC}",
                "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        NsmPowerSmoothing::getPwrSmoothingIntf()
            ->PowerSmoothingIntf::minPrimaryFloorActivationPoint(
                utils::convertAndScaleDownUint32ToDouble(
                    minPrimaryFloorActivationPoint, 1000));
        return rc;
    }

    int handleSample(const TelemetrySample& sample)
    {
        uint8_t returnValue = NSM_SW_SUCCESS;
        if (sample.tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        {
            return returnValue;
        }

        if (sample.tag == FEATURE_FLAG_TAG)
        {
            return updateFeatureFlagSample(sample);
        }
        else if (sample.tag == CURRENT_TMP_SETTING_TAG)
        {
            return updateCurrentTMPSettingSample(sample);
        }
        else if (sample.tag == TMP_FLOOR_SETTING_TAG)
        {
            return updateCurrentTMPFloorSettingSample(sample);
        }
        else if (sample.tag == MAX_TMP_FLOOR_SETTING_TAG)
        {
            return updateMaxTmpFloorSettingSample(sample);
        }
        else if (sample.tag == MIN_TMP_FLOOR_SETTING_TAG)
        {
            return updateMinTmpFloorSettingSample(sample);
        }
        else if (sample.tag == FLOOR_WINDOW_MULTIPLIER_TAG)
        {
            return updateFloorWindowMultiplierSample(sample);
        }
        else if (sample.tag == MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG)
        {
            return updateMinPrimaryFloorActivationOffsetSample(sample);
        }
        else if (sample.tag == MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG)
        {
            return updateMinPrimaryFloorActivationPointSample(sample);
        }
        else
        {
            lg2::error("Unknown tag in admin profile response.", "TAG",
                       sample.tag);
            returnValue = NSM_SW_ERROR_LENGTH;
        }
        return returnValue;
    }

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override
    {
        if (!NsmPowerSmoothing::getDevice()->isCommandSupported(
                NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2))
        {
            return NsmPowerSmoothing::handleResponseMsg(responseMsg,
                                                        responseLen);
        }

        uint8_t cc{};
        uint16_t telemetry_count{};
        size_t consumed_len{};
        auto response_data = reinterpret_cast<const uint8_t*>(responseMsg);
        int rc = decode_get_powersmoothing_featinfo_v2_resp(
            responseMsg, responseLen, &consumed_len, &cc, &telemetry_count);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            lg2::debug("decode_get_powersmoothing_featinfo_v2_resp successful");
        }
        else
        {
            lg2::error("decode_get_powersmoothing_featinfo_v2_resp failed");
            return cc ? cc : rc;
        }

        while (telemetry_count--)
        {
            uint8_t tag;
            bool valid;
            const uint8_t* data;
            size_t data_len;

            responseLen -= consumed_len;
            response_data += consumed_len;

            auto sample = reinterpret_cast<const nsm_aggregate_resp_sample*>(
                response_data);

            rc = decode_aggregate_resp_sample(sample, responseLen,
                                              &consumed_len, &tag, &valid,
                                              &data, &data_len);

            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "responseHandler: decode_aggregate_resp_sample failed. "
                    "Tag={TAG}, rc={RC}, valid_bit={VALID}",
                    "TAG", tag, "RC", rc, "VALID", valid);

                continue;
            }

            rc = handleSample(TelemetrySample{
                tag, static_cast<uint8_t>(data_len), data, valid});
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::debug(
                    "responseHandler: decoding failed for one or more samples. "
                    "rc={RC}",
                    "RC", rc);
            }
        }
        NsmPowerSmoothing::updateMetricOnSharedMemory();
        return rc;
    }
};

class NsmCurrentPowerSmoothingProfileV2 : public NsmCurrentPowerSmoothingProfile
{
  public:
    NsmCurrentPowerSmoothingProfileV2(
        std::string& name, std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<OemCurrentPowerProfileIntf> pwrSmoothingCurProfileIntf,
        std::shared_ptr<NsmPowerProfileCollection>
            pwrSmoothingSupportedCollection,
        std::shared_ptr<NsmPowerSmoothingAdminOverride> adminProfileSensor,
        std::shared_ptr<NsmDevice> device) :
        NsmCurrentPowerSmoothingProfile(
            name, type, inventoryObjPath, pwrSmoothingCurProfileIntf,
            pwrSmoothingSupportedCollection, adminProfileSensor, device)
    {}

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override
    {
        if (!NsmCurrentPowerSmoothingProfile::getDevice()->isCommandSupported(
                NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2))
        {
            return NsmCurrentPowerSmoothingProfile::genRequestMsg(eid,
                                                                  instanceId);
        }

        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_req));
        auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_get_current_profile_info_v2_req(instanceId,
                                                         requestPtr);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug("encode_get_current_profile_info_req failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);
            return std::nullopt;
        }

        return request;
    }

    struct TelemetrySample
    {
        uint8_t tag;
        uint8_t data_len;
        const uint8_t* data;
        bool valid;
    };

    int updateActivePresetProfileIdSample(const TelemetrySample& sample)
    {
        uint8_t activePresetProfileId;
        int rc = NSM_SW_SUCCESS;
        rc = decodeActivePresetProfileSample(sample.data, sample.data_len,
                                             &activePresetProfileId);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Current TMP Setting. RC={RC}", "RC",
                       rc);
            return rc;
        }
        // Get the profile path
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::appliedProfilePath(
                NsmCurrentPowerSmoothingProfile::getProfilePath(
                    activePresetProfileId));
        return rc;
    }

    int updateAdminOverrideMaskSample(const TelemetrySample& sample)
    {
        uint16_t adminOverrideMask;
        int rc = NSM_SW_SUCCESS;
        rc = decodeAdminOverrideMaskSample(sample.data, sample.data_len,
                                           &adminOverrideMask);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Admin Override Mask. RC={RC}", "RC",
                       rc);
            return rc;
        }

        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::tmpFloorPercentApplied(
                (adminOverrideMask & (1u << 0)) != 0 ? true : false);
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::rampUpRateApplied(
                (adminOverrideMask & (1u << 1)) != 0 ? true : false);
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::rampDownRateApplied(
                (adminOverrideMask & (1u << 2)) != 0 ? true : false);
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::rampDownHysteresisApplied(
                (adminOverrideMask & (1u << 3)) != 0 ? true : false);
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::secondaryPowerFloorSettingApplied(
                (adminOverrideMask & (1u << 4)) != 0 ? true : false);
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::
                primaryFloorActivationWindowMultiplierApplied(
                    (adminOverrideMask & (1u << 5)) != 0 ? true : false);
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::
                primaryFloorTargetWindowMultiplierApplied(
                    (adminOverrideMask & (1u << 6)) != 0 ? true : false);
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::primaryFloorActivationOffsetApplied(
                (adminOverrideMask & (1u << 7)) != 0 ? true : false);
        return rc;
    }

    int updateCurrentTMPFloorSample(const TelemetrySample& sample)
    {
        uint16_t currentTmpFloorSetting;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentTMPFloorSample(sample.data, sample.data_len,
                                         &currentTmpFloorSetting);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Current TMP Floor Setting. RC={RC}",
                       "RC", rc);
            return rc;
        }
        // Update values on iface
        // fraction to percent
        if (currentTmpFloorSetting == INVALID_UINT16_VALUE)
        {
            NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
                ->CurrentPowerProfileIntf::tmpFloorPercent(
                    INVALID_UINT32_VALUE);
        }
        else
        {
            NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
                ->CurrentPowerProfileIntf::tmpFloorPercent(
                    NvUFXP4_12ToDouble(currentTmpFloorSetting) * 100);
        }
        return rc;
    }

    int updateCurrentRampUpRateSample(const TelemetrySample& sample)
    {
        uint32_t rampUpRate;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentRampupRateSample(sample.data, sample.data_len,
                                           &rampUpRate);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Ramp Up Rate. RC={RC}", "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::rampUpRate(
                utils::convertAndScaleDownUint32ToDouble(rampUpRate, 1000));
        return rc;
    }

    int updateCurrentRampDownRateSample(const TelemetrySample& sample)
    {
        uint32_t rampDownRate;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentRampdownRateSample(sample.data, sample.data_len,
                                             &rampDownRate);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Ramp Down Rate. RC={RC}", "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::rampDownRate(
                utils::convertAndScaleDownUint32ToDouble(rampDownRate, 1000));
        return rc;
    }

    int updateCurrentRampDownHysteresisSample(const TelemetrySample& sample)
    {
        uint32_t rampDownHysteresis;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentRampdownHysteresisSample(sample.data, sample.data_len,
                                                   &rampDownHysteresis);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode Current Ramp Down Hysteresis. RC={RC}",
                       "RC", rc);
            return rc;
        }
        // miliseconds to seconds
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::rampDownHysteresis(
                utils::convertAndScaleDownUint32ToDouble(rampDownHysteresis,
                                                         1000));
        return rc;
    }

    int updateCurrentSecondaryFloorSample(const TelemetrySample& sample)
    {
        uint32_t secondaryPowerFloorSetting;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentSecondaryFloorSample(sample.data, sample.data_len,
                                               &secondaryPowerFloorSetting);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode Secondary Power Floor Setting. RC={RC}", "RC",
                rc);
            return rc;
        }
        // mw/sec to watts/sec
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::secondaryPowerFloorSetting(
                utils::convertAndScaleDownUint32ToDouble(
                    secondaryPowerFloorSetting, 1000));
        return rc;
    }

    int updateCurrentPrimaryFloorActivationWindowMultiplierSample(
        const TelemetrySample& sample)
    {
        uint8_t primaryFloorActivationWindowMultiplier;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentPrimaryFloorActivationWindowMultiplierSample(
            sample.data, sample.data_len,
            &primaryFloorActivationWindowMultiplier);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode Primary Floor Activation Window Multiplier. RC={RC}",
                "RC", rc);
            return rc;
        }
        // milli seconds to seconds
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::primaryFloorActivationWindowMultiplier(
                utils::convertAndScaleDownUint8ToDouble(
                    primaryFloorActivationWindowMultiplier, 1000));
        return rc;
    }

    int updateCurrentPrimaryFloorTargetWindowSample(
        const TelemetrySample& sample)
    {
        uint8_t primaryFloorTargetWindowMultiplier;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentPrimaryFloorTargetWindowSample(
            sample.data, sample.data_len, &primaryFloorTargetWindowMultiplier);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode Primary Floor Target Window Multiplier. RC={RC}",
                "RC", rc);
            return rc;
        }
        // milli seconds to seconds
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::primaryFloorTargetWindowMultiplier(
                utils::convertAndScaleDownUint8ToDouble(
                    primaryFloorTargetWindowMultiplier, 1000));
        return rc;
    }

    int updateCurrentPrimaryFloorActivationOffsetSample(
        const TelemetrySample& sample)
    {
        uint32_t primaryFloorActivationOffset;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentPrimaryFloorActivationOffsetSample(
            sample.data, sample.data_len, &primaryFloorActivationOffset);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode Primary Floor Activation Offset. RC={RC}",
                "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        NsmCurrentPowerSmoothingProfile::getPwrSmoothingCurProfileIntf()
            ->CurrentPowerProfileIntf::primaryFloorActivationOffset(
                utils::convertAndScaleDownUint32ToDouble(
                    primaryFloorActivationOffset, 1000));
        return rc;
    }

    int handleSample(const TelemetrySample& sample)
    {
        uint8_t returnValue = NSM_SW_SUCCESS;
        if (sample.tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        {
            return returnValue;
        }

        if (sample.tag == ACTIVE_PRESET_PROFILE_ID_TAG)
        {
            return updateActivePresetProfileIdSample(sample);
        }
        else if (sample.tag == ADMIN_OVERRIDE_MASK_TAG)
        {
            return updateAdminOverrideMaskSample(sample);
        }
        else if (sample.tag == CURRENT_TMP_FLOOR_SETTING_TAG)
        {
            return updateCurrentTMPFloorSample(sample);
        }
        else if (sample.tag == CURRENT_RAMPUP_RATE_TAG)
        {
            return updateCurrentRampUpRateSample(sample);
        }
        else if (sample.tag == CURRENT_RAMPDOWN_RATE_TAG)
        {
            return updateCurrentRampDownRateSample(sample);
        }
        else if (sample.tag == CURRENT_RAMPDOWN_HYSTERESIS_TAG)
        {
            return updateCurrentRampDownHysteresisSample(sample);
        }
        else if (sample.tag == CURRENT_SECONDARY_FLOOR_TAG)
        {
            return updateCurrentSecondaryFloorSample(sample);
        }
        else if (sample.tag ==
                 CURRENT_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG)
        {
            return updateCurrentPrimaryFloorActivationWindowMultiplierSample(
                sample);
        }
        else if (sample.tag == CURRENT_PRIMARY_FLOOR_TARGET_WINDOW_TAG)
        {
            return updateCurrentPrimaryFloorTargetWindowSample(sample);
        }
        else if (sample.tag == CURRENT_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG)
        {
            return updateCurrentPrimaryFloorActivationOffsetSample(sample);
        }
        else
        {
            lg2::error("Unknown tag in admin profile response.", "TAG",
                       sample.tag);
            returnValue = NSM_SW_ERROR_LENGTH;
        }
        return returnValue;
    }

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override
    {
        if (!NsmCurrentPowerSmoothingProfile::getDevice()->isCommandSupported(
                NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2))
        {
            return NsmCurrentPowerSmoothingProfile::handleResponseMsg(
                responseMsg, responseLen);
        }

        uint8_t cc{};
        uint16_t telemetry_count{};
        size_t consumed_len{};
        auto response_data = reinterpret_cast<const uint8_t*>(responseMsg);
        int rc = decode_get_current_profile_info_v2_resp(
            responseMsg, responseLen, &consumed_len, &cc, &telemetry_count);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            lg2::debug("decode_get_current_profile_info_resp_v2 successful");
        }
        else
        {
            lg2::error("decode_get_current_profile_info_resp_v2 failed");
            return cc ? cc : rc;
        }

        while (telemetry_count--)
        {
            uint8_t tag;
            bool valid;
            const uint8_t* data;
            size_t data_len;

            responseLen -= consumed_len;
            response_data += consumed_len;

            auto sample = reinterpret_cast<const nsm_aggregate_resp_sample*>(
                response_data);

            rc = decode_aggregate_resp_sample(sample, responseLen,
                                              &consumed_len, &tag, &valid,
                                              &data, &data_len);

            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "responseHandler: decode_aggregate_resp_sample failed. "
                    "Tag={TAG}, rc={RC}, valid_bit={VALID}",
                    "TAG", tag, "RC", rc, "VALID", valid);

                continue;
            }

            rc = handleSample(TelemetrySample{
                tag, static_cast<uint8_t>(data_len), data, valid});

            if (rc != NSM_SW_SUCCESS)
            {
                lg2::debug(
                    "responseHandler: decoding failed for one or more samples. "
                    "rc={RC}",
                    "RC", rc);
            }
        }
        NsmCurrentPowerSmoothingProfile::updateMetricOnSharedMemory();
        return rc;
    }
};

class NsmPowerSmoothingAdminOverrideV2 : public NsmPowerSmoothingAdminOverride
{
  public:
    NsmPowerSmoothingAdminOverrideV2(
        std::string& name, std::string& type,
        std::shared_ptr<OemAdminProfileIntf> adminProfileIntf,
        std::string& inventoryObjPath, std::shared_ptr<NsmDevice> device) :
        NsmPowerSmoothingAdminOverride(name, type, adminProfileIntf,
                                       inventoryObjPath, device)
    {}

    struct TelemetrySample
    {
        uint8_t tag;
        uint8_t data_len;
        const uint8_t* data;
        bool valid;
    };

    int updateCurrentTMPFloorSample(const TelemetrySample& sample)
    {
        uint16_t tmpFloorPercent;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentTMPFloorSample(sample.data, sample.data_len,
                                         &tmpFloorPercent);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode CurrentPercentTMPFloor. RC={RC}", "RC",
                       rc);
            return rc;
        }
        // fraction to percent
        if (tmpFloorPercent == INVALID_UINT16_VALUE)
        {
            // on dbus we still show INVALID_UINT32_VALUE to keep
            // consistent
            NsmPowerSmoothingAdminOverride::getAdminProfileIntf()
                ->AdminPowerProfileIntf::tmpFloorPercent(INVALID_UINT32_VALUE);
        }
        else
        {
            NsmPowerSmoothingAdminOverride::getAdminProfileIntf()
                ->AdminPowerProfileIntf::tmpFloorPercent(
                    NvUFXP4_12ToDouble(tmpFloorPercent) * 100);
        }
        return rc;
    }

    int updateCurrentRampUpRateSample(const TelemetrySample& sample)
    {
        uint32_t rampUpRate;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentRampupRateSample(sample.data, sample.data_len,
                                           &rampUpRate);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode CurrentRampUpRate. RC={RC}", "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        double reading = utils::convertAndScaleDownUint32ToDouble(rampUpRate,
                                                                  1000);
        NsmPowerSmoothingAdminOverride::getAdminProfileIntf()
            ->AdminPowerProfileIntf::rampUpRate(reading);
        return rc;
    }

    int updateCurrentRampDownRateSample(const TelemetrySample& sample)
    {
        uint32_t rampDownRate;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentRampdownRateSample(sample.data, sample.data_len,
                                             &rampDownRate);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode CurrentRampDownRate. RC={RC}", "RC",
                       rc);
            return rc;
        }
        // mw/sec to watts/sec
        double reading = utils::convertAndScaleDownUint32ToDouble(rampDownRate,
                                                                  1000);

        NsmPowerSmoothingAdminOverride::getAdminProfileIntf()
            ->AdminPowerProfileIntf::rampDownRate(reading);
        return rc;
    }

    int updateCurrentRampDownHysteresisSample(const TelemetrySample& sample)
    {
        uint32_t rampDownHysteresis;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentRampdownHysteresisSample(sample.data, sample.data_len,
                                                   &rampDownHysteresis);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode CurrentRampDownHysteresis. RC={RC}",
                       "RC", rc);
            return rc;
        }
        // miliseconds to seconds
        double reading =
            utils::convertAndScaleDownUint32ToDouble(rampDownHysteresis, 1000);
        NsmPowerSmoothingAdminOverride::getAdminProfileIntf()
            ->AdminPowerProfileIntf::rampDownHysteresis(reading);
        return rc;
    }

    int updateCurrentSecondaryFloorSample(const TelemetrySample& sample)
    {
        uint32_t secondaryFloorSetting;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentSecondaryFloorSample(sample.data, sample.data_len,
                                               &secondaryFloorSetting);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode CurrentSecondaryFloorSetting. RC={RC}",
                       "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        double reading = utils::convertAndScaleDownUint32ToDouble(
            secondaryFloorSetting, 1000);
        NsmPowerSmoothingAdminOverride::getAdminProfileIntf()
            ->AdminPowerProfileIntf::secondaryPowerFloorSetting(reading);
        return rc;
    }

    int updateCurrentPrimaryFloorActivationWindowMultiplierSample(
        const TelemetrySample& sample)
    {
        uint8_t primaryFloorActivationWindowMultiplier;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentPrimaryFloorActivationWindowMultiplierSample(
            sample.data, sample.data_len,
            &primaryFloorActivationWindowMultiplier);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode CurrentPrimaryFloorActivationWindowMultiplier. RC={RC}",
                "RC", rc);
            return rc;
        }
        // milli seconds to seconds
        NsmPowerSmoothingAdminOverride::getAdminProfileIntf()
            ->AdminPowerProfileIntf::primaryFloorActivationWindowMultiplier(
                utils::convertAndScaleDownUint8ToDouble(
                    primaryFloorActivationWindowMultiplier, 1000));
        return rc;
    }

    int updateCurrentPrimaryFloorTargetWindowSample(
        const TelemetrySample& sample)
    {
        uint8_t primaryFloorTargetWindowMultiplier;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentPrimaryFloorTargetWindowSample(
            sample.data, sample.data_len, &primaryFloorTargetWindowMultiplier);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode CurrentPrimaryFloorTargetWindowMultiplier. RC={RC}",
                "RC", rc);
            return rc;
        }
        // milli seconds to seconds
        NsmPowerSmoothingAdminOverride::getAdminProfileIntf()
            ->AdminPowerProfileIntf::primaryFloorTargetWindowMultiplier(
                utils::convertAndScaleDownUint8ToDouble(
                    primaryFloorTargetWindowMultiplier, 1000));
        return rc;
    }

    int updateCurrentPrimaryFloorActivationOffsetSample(
        const TelemetrySample& sample)
    {
        uint32_t primaryFloorActivationOffset;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentPrimaryFloorActivationOffsetSample(
            sample.data, sample.data_len, &primaryFloorActivationOffset);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode CurrentPrimaryFloorActivationOffset. RC={RC}",
                "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        double reading = utils::convertAndScaleDownUint32ToDouble(
            primaryFloorActivationOffset, 1000);
        NsmPowerSmoothingAdminOverride::getAdminProfileIntf()
            ->AdminPowerProfileIntf::primaryFloorActivationOffset(reading);
        return rc;
    }

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override
    {
        if (!NsmPowerSmoothingAdminOverride::getDevice()->isCommandSupported(
                NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2))
        {
            return NsmPowerSmoothingAdminOverride::genRequestMsg(eid,
                                                                 instanceId);
        }
        int rc = NSM_SW_SUCCESS;
        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_req));
        auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
        rc = encode_get_admin_override_profile_info_v2_req(instanceId,
                                                           requestPtr);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug("encode_query_admin_override_req failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);
            return std::nullopt;
        }
        return request;
    }

    int handleSample(const TelemetrySample& sample)
    {
        uint8_t returnValue = NSM_SW_SUCCESS;
        if (sample.tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        {
            return returnValue;
        }

        if (sample.tag == ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG)
        {
            return updateCurrentTMPFloorSample(sample);
        }
        else if (sample.tag == ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG)
        {
            return updateCurrentRampUpRateSample(sample);
        }
        else if (sample.tag == ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG)
        {
            return updateCurrentRampDownRateSample(sample);
        }
        else if (sample.tag == ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG)
        {
            return updateCurrentRampDownHysteresisSample(sample);
        }
        else if (sample.tag == ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG)
        {
            return updateCurrentSecondaryFloorSample(sample);
        }
        else if (
            sample.tag ==
            ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG)
        {
            return updateCurrentPrimaryFloorActivationWindowMultiplierSample(
                sample);
        }
        else if (sample.tag ==
                 ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG)
        {
            return updateCurrentPrimaryFloorTargetWindowSample(sample);
        }
        else if (sample.tag ==
                 ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG)
        {
            return updateCurrentPrimaryFloorActivationOffsetSample(sample);
        }
        else
        {
            lg2::error("Unknown tag in admin profile response.", "TAG",
                       sample.tag);
            returnValue = NSM_SW_ERROR_LENGTH;
        }
        return returnValue;
    }

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override
    {
        if (!NsmPowerSmoothingAdminOverride::getDevice()->isCommandSupported(
                NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2))
        {
            return NsmPowerSmoothingAdminOverride::handleResponseMsg(
                responseMsg, responseLen);
        }
        uint8_t cc{};
        uint16_t telemetry_count{};
        size_t consumed_len{};
        auto response_data = reinterpret_cast<const uint8_t*>(responseMsg);
        auto rc = decode_get_admin_override_profile_info_v2_resp(
            responseMsg, responseLen, &consumed_len, &cc, &telemetry_count);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            lg2::debug(
                "decode_get_admin_override_profile_info_v2_resp successful");
        }
        else
        {
            lg2::error(
                "decode_get_admin_override_profile_info_v2_resp failed cc={CC}, rc={RC}",
                "CC", cc, "RC", rc);
            return rc;
        }

        while (telemetry_count--)
        {
            uint8_t tag;
            bool valid;
            const uint8_t* data;
            size_t data_len;

            responseLen -= consumed_len;
            response_data += consumed_len;

            auto sample = reinterpret_cast<const nsm_aggregate_resp_sample*>(
                response_data);

            rc = decode_aggregate_resp_sample(sample, responseLen,
                                              &consumed_len, &tag, &valid,
                                              &data, &data_len);

            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "responseHandler: decode_aggregate_resp_sample failed. "
                    "Tag={TAG}, rc={RC}, valid_bit={VALID}",
                    "TAG", tag, "RC", rc, "VALID", valid);

                continue;
            }

            rc = handleSample(TelemetrySample{
                tag, static_cast<uint8_t>(data_len), data, valid});
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::debug(
                    "responseHandler: decoding failed for one or more samples. "
                    "rc={RC}",
                    "RC", rc);
            }
        }
        return rc;
    }
};

class NsmPowerProfileCollectionV2 : public NsmPowerProfileCollection
{
  public:
    NsmPowerProfileCollectionV2(std::string& name, std::string& type,
                                std::string& inventoryObjPath,
                                std::shared_ptr<NsmDevice> device) :
        NsmPowerProfileCollection(name, type, inventoryObjPath, device)
    {}

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override
    {
        if (!NsmPowerProfileCollection::getDevice()->isCommandSupported(
                NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2))
        {
            return NsmPowerProfileCollection::genRequestMsg(eid, instanceId);
        }
        int rc = NSM_SW_SUCCESS;
        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_req));
        auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
        rc = encode_get_preset_profile_info_v2_req(instanceId, requestPtr);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug("encode_get_preset_profile_info_v2_req failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);
            return std::nullopt;
        }
        return request;
    }

    struct TelemetrySample
    {
        uint8_t tag;
        uint8_t data_len;
        const uint8_t* data;
        bool valid;
    };

    int updateCurrentTMPFloorSample(const TelemetrySample& sample,
                                    uint8_t profId)
    {
        uint16_t tmpFloorPercent;
        int rc = NSM_SW_SUCCESS;
        auto powerProfileIntf =
            NsmPowerProfileCollection::getSupportedProfileById(profId);
        rc = decodeCurrentTMPFloorSample(sample.data, sample.data_len,
                                         &tmpFloorPercent);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode CurrentPercentTMPFloor. RC={RC}", "RC",
                       rc);
            return rc;
        }
        if (tmpFloorPercent == INVALID_UINT16_VALUE)
        {
            powerProfileIntf->PowerProfileIntf::tmpFloorPercent(
                INVALID_UINT32_VALUE);
        }
        else
        {
            // fraction to percent
            powerProfileIntf->PowerProfileIntf::tmpFloorPercent(
                NvUFXP4_12ToDouble(tmpFloorPercent) * 100);
        }
        return rc;
    }

    int updateCurrentRampUpRateSample(const TelemetrySample& sample,
                                      uint8_t profId)
    {
        auto powerProfileIntf =
            NsmPowerProfileCollection::getSupportedProfileById(profId);
        uint32_t rampUpRate;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentRampupRateSample(sample.data, sample.data_len,
                                           &rampUpRate);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode CurrentRampUpRate. RC={RC}", "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        powerProfileIntf->PowerProfileIntf::rampUpRate(
            utils::convertAndScaleDownUint32ToDouble(rampUpRate, 1000));
        return rc;
    }

    int updateCurrentRampDownRateSample(const TelemetrySample& sample,
                                        uint8_t profId)
    {
        auto powerProfileIntf =
            NsmPowerProfileCollection::getSupportedProfileById(profId);
        uint32_t rampDownRate;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentRampdownRateSample(sample.data, sample.data_len,
                                             &rampDownRate);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode CurrentRampDownRate. RC={RC}", "RC",
                       rc);
            return rc;
        }
        // mw/sec to watts/sec
        powerProfileIntf->PowerProfileIntf::rampDownRate(
            utils::convertAndScaleDownUint32ToDouble(rampDownRate, 1000));
        return rc;
    }

    int updateCurrentRampDownHysteresisSample(const TelemetrySample& sample,
                                              uint8_t profId)
    {
        auto powerProfileIntf =
            NsmPowerProfileCollection::getSupportedProfileById(profId);
        uint32_t rampDownHysteresis;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentRampdownHysteresisSample(sample.data, sample.data_len,
                                                   &rampDownHysteresis);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode CurrentRampDownHysteresis. RC={RC}",
                       "RC", rc);
            return rc;
        }
        // miliseconds to seconds
        powerProfileIntf->PowerProfileIntf::rampDownHysteresis(
            utils::convertAndScaleDownUint32ToDouble(rampDownHysteresis, 1000));
        return rc;
    }

    int updateCurrentSecondaryFloorSample(const TelemetrySample& sample,
                                          uint8_t profId)
    {
        auto powerProfileIntf =
            NsmPowerProfileCollection::getSupportedProfileById(profId);
        uint32_t secondaryFloorSetting;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentSecondaryFloorSample(sample.data, sample.data_len,
                                               &secondaryFloorSetting);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to decode CurrentSecondaryFloorSetting. RC={RC}",
                       "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        double reading = utils::convertAndScaleDownUint32ToDouble(
            secondaryFloorSetting, 1000);
        powerProfileIntf->PowerProfileIntf::secondaryPowerFloorSetting(reading);
        return rc;
    }

    int updateCurrentPrimaryFloorActivationWindowMultiplierSample(
        const TelemetrySample& sample, uint8_t profId)
    {
        auto powerProfileIntf =
            NsmPowerProfileCollection::getSupportedProfileById(profId);
        uint8_t primaryFloorActivationWindowMultiplier;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentPrimaryFloorActivationWindowMultiplierSample(
            sample.data, sample.data_len,
            &primaryFloorActivationWindowMultiplier);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode CurrentPrimaryFloorActivationWindowMultiplier. RC={RC}",
                "RC", rc);
            return rc;
        }
        // milli seconds to seconds
        powerProfileIntf
            ->PowerProfileIntf::primaryFloorActivationWindowMultiplier(
                utils::convertAndScaleDownUint8ToDouble(
                    primaryFloorActivationWindowMultiplier, 1000));
        return rc;
    }

    int updateCurrentPrimaryFloorTargetWindowSample(
        const TelemetrySample& sample, uint8_t profId)
    {
        auto powerProfileIntf =
            NsmPowerProfileCollection::getSupportedProfileById(profId);
        uint8_t primaryFloorTargetWindowMultiplier;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentPrimaryFloorTargetWindowSample(
            sample.data, sample.data_len, &primaryFloorTargetWindowMultiplier);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode CurrentPrimaryFloorTargetWindowMultiplier. RC={RC}",
                "RC", rc);
            return rc;
        }
        // milli seconds to seconds
        powerProfileIntf->PowerProfileIntf::primaryFloorTargetWindowMultiplier(
            utils::convertAndScaleDownUint8ToDouble(
                primaryFloorTargetWindowMultiplier, 1000));
        return rc;
    }

    int updateCurrentPrimaryFloorActivationOffsetSample(
        const TelemetrySample& sample, uint8_t profId)
    {
        auto powerProfileIntf =
            NsmPowerProfileCollection::getSupportedProfileById(profId);
        uint32_t primaryFloorActivationOffset;
        int rc = NSM_SW_SUCCESS;
        rc = decodeCurrentPrimaryFloorActivationOffsetSample(
            sample.data, sample.data_len, &primaryFloorActivationOffset);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode CurrentPrimaryFloorActivationOffset. RC={RC}",
                "RC", rc);
            return rc;
        }
        // mw to watts
        double reading = utils::convertAndScaleDownUint32ToDouble(
            primaryFloorActivationOffset, 1000);
        powerProfileIntf->PowerProfileIntf::primaryFloorActivationOffset(
            reading);
        return rc;
    }

    int handleSample(const TelemetrySample& sample)
    {
        uint8_t returnValue = NSM_SW_SUCCESS;

        uint8_t tag = sample.tag >> 3;
        uint8_t profId = sample.tag & 0x07;

        if (!NsmPowerProfileCollection::hasProfileId(profId))
        {
            auto nsmDevice = NsmPowerProfileCollection::getDevice();
            auto powerProfileIntf = std::make_shared<OemPowerProfileIntfV2>(
                utils::DBusHandler::getBus(),
                NsmPowerProfileCollection::getInventoryObjPath(), profId,
                nsmDevice);
            // Add async set operations for each profile parameter
            AsyncOperationManager::getInstance()
                ->getDispatcher(powerProfileIntf->getInventoryObjPath())
                ->addAsyncSetOperation(
                    powerProfileIntf->PowerProfileIntf::interface,
                    "TMPFloorPercent",
                    AsyncSetOperationInfo{
                        std::bind_front(
                            &OemPowerProfileIntfV2::setTmpFloorPercent,
                            powerProfileIntf),
                        {},
                        nsmDevice});
            AsyncOperationManager::getInstance()
                ->getDispatcher(powerProfileIntf->getInventoryObjPath())
                ->addAsyncSetOperation(
                    powerProfileIntf->PowerProfileIntf::interface, "RampUpRate",
                    AsyncSetOperationInfo{
                        std::bind_front(&OemPowerProfileIntfV2::setRampUpRate,
                                        powerProfileIntf),
                        {},
                        nsmDevice});
            AsyncOperationManager::getInstance()
                ->getDispatcher(powerProfileIntf->getInventoryObjPath())
                ->addAsyncSetOperation(
                    powerProfileIntf->PowerProfileIntf::interface,
                    "RampDownRate",
                    AsyncSetOperationInfo{
                        std::bind_front(&OemPowerProfileIntfV2::setRampDownRate,
                                        powerProfileIntf),
                        {},
                        nsmDevice});

            AsyncOperationManager::getInstance()
                ->getDispatcher(powerProfileIntf->getInventoryObjPath())
                ->addAsyncSetOperation(
                    powerProfileIntf->PowerProfileIntf::interface,
                    "RampDownHysteresis",
                    AsyncSetOperationInfo{
                        std::bind_front(
                            &OemPowerProfileIntfV2::setRampDownHysteresis,
                            powerProfileIntf),
                        {},
                        nsmDevice});
            AsyncOperationManager::getInstance()
                ->getDispatcher(powerProfileIntf->getInventoryObjPath())
                ->addAsyncSetOperation(
                    powerProfileIntf->PowerProfileIntf::interface,
                    "SecondaryPowerFloorSetting",
                    AsyncSetOperationInfo{
                        std::bind_front(
                            &OemPowerProfileIntfV2::setSecondaryPowerFloor,
                            powerProfileIntf),
                        {},
                        nsmDevice});
            AsyncOperationManager::getInstance()
                ->getDispatcher(powerProfileIntf->getInventoryObjPath())
                ->addAsyncSetOperation(
                    powerProfileIntf->PowerProfileIntf::interface,
                    "PrimaryFloorActivationWindowMultiplier",
                    AsyncSetOperationInfo{
                        std::bind_front(
                            &OemPowerProfileIntfV2::
                                setPrimaryFloorActivationWindowMultiplier,
                            powerProfileIntf),
                        {},
                        nsmDevice});
            AsyncOperationManager::getInstance()
                ->getDispatcher(powerProfileIntf->getInventoryObjPath())
                ->addAsyncSetOperation(
                    powerProfileIntf->PowerProfileIntf::interface,
                    "PrimaryFloorTargetWindowMultiplier",
                    AsyncSetOperationInfo{
                        std::bind_front(
                            &OemPowerProfileIntfV2::
                                setPrimaryFloorTargetWindowMultiplier,
                            powerProfileIntf),
                        {},
                        nsmDevice});
            AsyncOperationManager::getInstance()
                ->getDispatcher(powerProfileIntf->getInventoryObjPath())
                ->addAsyncSetOperation(
                    powerProfileIntf->PowerProfileIntf::interface,
                    "PrimaryFloorActivationOffset",
                    AsyncSetOperationInfo{
                        std::bind_front(&OemPowerProfileIntfV2::
                                            setPrimaryFloorActivationOffset,
                                        powerProfileIntf),
                        {},
                        nsmDevice});
            NsmPowerProfileCollection::addSupportedProfile(profId,
                                                           powerProfileIntf);
        }

        if (tag == PRESET_PROFILE_TMP_FLOOR_SETTING_TAG)
        {
            return updateCurrentTMPFloorSample(sample, profId);
        }
        else if (tag == PRESET_PROFILE_RAMPUP_RATE_TAG)
        {
            return updateCurrentRampUpRateSample(sample, profId);
        }
        else if (tag == PRESET_PROFILE_RAMPDOWN_RATE_TAG)
        {
            return updateCurrentRampDownRateSample(sample, profId);
        }
        else if (tag == PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG)
        {
            return updateCurrentRampDownHysteresisSample(sample, profId);
        }
        else if (tag == PRESET_PROFILE_SECONDARY_FLOOR_TAG)
        {
            return updateCurrentSecondaryFloorSample(sample, profId);
        }
        else if (tag ==
                 PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG)
        {
            return updateCurrentPrimaryFloorActivationWindowMultiplierSample(
                sample, profId);
        }
        else if (tag == PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG)
        {
            return updateCurrentPrimaryFloorTargetWindowSample(sample, profId);
        }
        else if (tag == PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG)
        {
            return updateCurrentPrimaryFloorActivationOffsetSample(sample,
                                                                   profId);
        }
        else
        {
            lg2::error("Unknown tag in admin profile response.", "TAG",
                       sample.tag);
            returnValue = NSM_SW_ERROR_LENGTH;
        }

        return returnValue;
    }

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override
    {
        if (!NsmPowerProfileCollection::getDevice()->isCommandSupported(
                NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2))
        {
            return NsmPowerProfileCollection::handleResponseMsg(responseMsg,
                                                                responseLen);
        }
        uint8_t cc{};
        uint16_t telemetry_count{};
        size_t consumed_len{};
        auto response_data = reinterpret_cast<const uint8_t*>(responseMsg);
        auto rc = decode_get_preset_profile_info_v2_resp(
            responseMsg, responseLen, &consumed_len, &cc, &telemetry_count);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            lg2::debug("decode_get_preset_profile_info_v2_resp successful");
        }
        else
        {
            lg2::error(
                "decode_get_preset_profile_info_v2_resp failed cc={CC}, rc={RC}",
                "CC", cc, "RC", rc);
            return rc;
        }

        while (telemetry_count--)
        {
            uint8_t tag;
            bool valid;
            const uint8_t* data;
            size_t data_len;

            responseLen -= consumed_len;
            response_data += consumed_len;

            auto sample = reinterpret_cast<const nsm_aggregate_resp_sample*>(
                response_data);

            rc = decode_aggregate_resp_sample(sample, responseLen,
                                              &consumed_len, &tag, &valid,
                                              &data, &data_len);

            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "responseHandler: decode_aggregate_resp_sample failed. "
                    "Tag={TAG}, rc={VALID}",
                    "TAG", tag, "RC", rc, "VALID", valid);

                continue;
            }

            rc = handleSample(TelemetrySample{
                tag, static_cast<uint8_t>(data_len), data, valid});
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::debug(
                    "responseHandler: decoding failed for one or more samples. "
                    "rc={RC}",
                    "RC", rc);
            }
        }

        return rc;
    }
};
} // namespace nsm
