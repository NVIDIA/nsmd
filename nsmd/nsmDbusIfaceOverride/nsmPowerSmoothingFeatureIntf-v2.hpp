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
#include "powersmoothing-powerprofile-api-v2.h"

#include "asyncOperationManager.hpp"
#include "nsmPowerSmoothingFeatureIntf.hpp"

#include <com/nvidia/PowerSmoothing/PowerSmoothing/server.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
namespace nsm
{
using PowerSmoothingIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerSmoothing::server::PowerSmoothing>;
class OemPowerSmoothingFeatIntfV2 : public OemPowerSmoothingFeatIntf
{
  public:
    OemPowerSmoothingFeatIntfV2(sdbusplus::bus::bus& bus,
                              const std::string& inventoryObjPath,
                              std::shared_ptr<NsmDevice> device) :
        OemPowerSmoothingFeatIntf(bus, inventoryObjPath, device)
    {}
    std::string getInventoryObjPath()
    {
        return OemPowerSmoothingFeatIntf::getInventoryObjPath();
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
        // For feature Supported : Check if bit0 is set
        bool featSupported = (featureFlag & (1u << 0)) != 0 ? true : false;
        PowerSmoothingIntf::featureSupported(featSupported);

        // For feature Enabled: Check if bit1 is set
        bool featureEnabled = (featureFlag & (1u << 1)) != 0 ? true : false;
        PowerSmoothingIntf::powerSmoothingEnabled(featureEnabled);

        // For RampDwon Enabled: Check if bit2 is set
        bool rampDownEnabled = (featureFlag & (1u << 2)) != 0 ? true : false;
        PowerSmoothingIntf::immediateRampDownEnabled(rampDownEnabled);

        // For Delayed Power Smoothing: Check if bit3 is set
        bool delayedPowerSmoothing = (featureFlag & (1u << 3)) != 0 ? true
                                                                    : false;
        PowerSmoothingIntf::delayedPowerSmoothingSupported(
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
        PowerSmoothingIntf::currentTempSetting(
            utils::convertAndScaleDownUint32ToDouble(currentTmpSetting, 1000));
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
            lg2::error(
                "Failed to decode Current TMP Floor Setting. Tag={TAG}, Data length={LEN}",
                "TAG", sample.tag, "LEN", sample.data_len);
            return rc;
        }
        // mw/sec to watts/sec
        PowerSmoothingIntf::currentTempFloorSetting(
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
            lg2::error(
                "Failed to decode Max TMP Floor Setting. Tag={TAG}, Data length={LEN}",
                "TAG", sample.tag, "LEN", sample.data_len);
            return rc;
        }
        // fraction to percent
        PowerSmoothingIntf::maxAllowedTmpFloorPercent(
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
        PowerSmoothingIntf::minAllowedTmpFloorPercent(
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
                "Failed to decode floor window multiplier Setting. RC={RC}",
                "RC", rc);
            return rc;
        }
        // mw/sec to watts/sec
        PowerSmoothingIntf::floorWindowMultiplier(
            utils::convertAndScaleDownUint32ToDouble(floorWindowMultiplier,
                                                     1000));
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
        double reading = utils::convertAndScaleDownUint32ToDouble(
            minPrimaryFloorActivationOffset, 1000);
        PowerSmoothingIntf::minPrimaryFloorActivationOffset(reading);
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
        double reading = utils::convertAndScaleDownUint32ToDouble(
            minPrimaryFloorActivationPoint, 1000);
        PowerSmoothingIntf::minPrimaryFloorActivationPoint(reading);
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

    requester::Coroutine getPwrSmoothingControlsFromDeviceV2(
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("getPwrSmoothingControlsFromDevice for EID: {EID}", "EID",
                  eid);

        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_get_powersmoothing_featinfo_v2_req(0, requestMsg);

        if (rc)
        {
            lg2::error(
                "getPwrSmoothingControlsFromDevice: encode_get_powersmoothing_featinfo_v2_req failed. eid={EID}, rc={RC}",
                "EID", eid, "RC", rc);
            // coverity[missing_return]
            co_return rc;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                                   responseLen);
        if (rc_)
        {
            lg2::error(
                "getPwrSmoothingControlsFromDevice postPatchIO failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            // coverity[missing_return]
            co_return rc_;
        }

        uint8_t cc{};
        uint16_t telemetry_count{};
        size_t consumed_len{};
        auto response_data =
            reinterpret_cast<const uint8_t*>(responseMsg.get());
        rc = decode_get_powersmoothing_featinfo_v2_resp(
            responseMsg.get(), responseLen, &consumed_len, &cc,
            &telemetry_count);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            lg2::info("decode_get_powersmoothing_featinfo_v2_resp successful");
        }
        else
        {
            lg2::error(
                "decode_get_powersmoothing_featinfo_v2_resp failed cc={CC}, rc={RC}",
                "CC", cc, "RC", rc);
            co_return rc;
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

        co_return rc;
    }

    requester::Coroutine
        togglePowerSmoothingOnDevice(bool featureEnabled,
                                     AsyncOperationStatusType* status,
                                     [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info(
            "togglePowerSmoothingOnDevice for EID: {EID} to Enable = {ENABLE}",
            "EID", eid, "ENABLE", featureEnabled);
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_toggle_feature_state_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        uint8_t feature_state = featureEnabled;
        // first argument instanceid=0 is irrelevant
        auto rc = encode_toggle_feature_state_req(0, feature_state, requestMsg);

        if (rc)
        {
            lg2::error(
                "togglePowerSmoothingOnDevice: encode_toggle_feature_state_req failed. eid={EID}, rc={RC}",
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
                "togglePowerSmoothingOnDevice SendRecvNsmMsg failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_toggle_feature_state_resp(responseMsg.get(), responseLen,
                                              &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            if (device->isCommandSupported(
                    NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                    NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2))
            {
                co_await getPwrSmoothingControlsFromDeviceV2(device);
            }
            else
            {
                co_await OemPowerSmoothingFeatIntf::getPwrSmoothingControlsFromDevice();
            }   
            // verify setting is applied on the device
            lg2::info("togglePowerSmoothingOnDevice for EID: {EID} completed",
                      "EID", eid);
        }
        else
        {
            lg2::error(
                "togglePowerSmoothingOnDevice decode_toggle_feature_state_resp "
                "failed.eid = {EID}, CC = {CC} reasoncode = {RC}, "
                "RC = {A} ",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
        co_return NSM_SW_SUCCESS;
    }

    requester::Coroutine setPowerSmoothingEnabled(
        const AsyncSetOperationValueType& value,
        AsyncOperationStatusType* status,
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        const bool* featureEnabled = std::get_if<bool>(&value);

        if (!featureEnabled)
        {
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
        const auto rc = co_await togglePowerSmoothingOnDevice(*featureEnabled,
                                                              status, device);

        // coverity[missing_return]
        co_return rc;
    }

    requester::Coroutine
        toggleImmediateRampDownOnDevice(bool ramdownEnabled,
                                        AsyncOperationStatusType* status,
                                        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info(
            "toggleImmediateRampDownOnDevice for EID: {EID} to Enable = {ENABLE}",
            "EID", eid, "ENABLE", ramdownEnabled);
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_toggle_immediate_rampdown_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        uint8_t feature_state = ramdownEnabled;
        // first argument instanceid=0 is irrelevant
        auto rc = encode_toggle_immediate_rampdown_req(0, feature_state,
                                                       requestMsg);

        if (rc)
        {
            lg2::error(
                "toggleImmediateRampDownOnDevice: encode_toggle_immediate_rampdown_req failed. eid={EID}, rc={RC}",
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
                "toggleImmediateRampDownOnDevice SendRecvNsmMsg failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_toggle_immediate_rampdown_resp(
            responseMsg.get(), responseLen, &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            if (device->isCommandSupported(
                    NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                    NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2))
            {
                co_await getPwrSmoothingControlsFromDeviceV2(device);
            }
            else
            {
                co_await OemPowerSmoothingFeatIntf::getPwrSmoothingControlsFromDevice();
            }   
            lg2::info(
                "toggleImmediateRampDownOnDevice for EID: {EID} completed",
                "EID", eid);
        }
        else
        {
            lg2::error(
                "toggleImmediateRampDownOnDevice decode_toggle_immediate_rampdown_resp "
                "failed.eid = {EID}, CC = {CC} reasoncode = {RC}, "
                "RC = {A} ",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
        co_return NSM_SW_SUCCESS;
    }

    requester::Coroutine setImmediateRampDownEnabled(
        const AsyncSetOperationValueType& value,
        AsyncOperationStatusType* status,
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        const bool* ramdownEnabled = std::get_if<bool>(&value);
        if (!ramdownEnabled)
        {
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
        const auto rc =
            co_await toggleImmediateRampDownOnDevice(*ramdownEnabled, status, device);

        co_return rc;
    }
};
} // namespace nsm
