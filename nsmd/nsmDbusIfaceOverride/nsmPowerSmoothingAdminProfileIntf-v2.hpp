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

#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "nsmPowerSmoothingAdminProfileIntf.hpp"

#include <com/nvidia/PowerSmoothing/AdminPowerProfile/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>

namespace nsm
{

using AdminPowerProfileIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerSmoothing::server::AdminPowerProfile>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

class OemAdminProfileIntfV2 : public OemAdminProfileIntf

{
  public:
    OemAdminProfileIntfV2(sdbusplus::bus::bus& bus,
                          const std::string& parentPath,
                          std::shared_ptr<NsmDevice> device) :
        OemAdminProfileIntf(bus, parentPath, device)
    {}

    std::string getInventoryObjPath()
    {
        return OemAdminProfileIntf::getInventoryObjPath();
    }

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
            AdminPowerProfileIntf::tmpFloorPercent(INVALID_UINT32_VALUE);
        }
        else
        {
            AdminPowerProfileIntf::tmpFloorPercent(
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
        AdminPowerProfileIntf::rampUpRate(reading);
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
            lg2::error(
                "Failed to decode CurrentRampDownRate. Tag={TAG}, Data length={LEN}",
                "TAG", sample.tag, "LEN", sample.data_len);
            return rc;
        }
        // mw/sec to watts/sec
        double reading = utils::convertAndScaleDownUint32ToDouble(rampDownRate,
                                                                  1000);
        AdminPowerProfileIntf::rampDownRate(reading);
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
        AdminPowerProfileIntf::rampDownHysteresis(reading);
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
        AdminPowerProfileIntf::secondaryPowerFloorSetting(reading);
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
        AdminPowerProfileIntf::primaryFloorActivationWindowMultiplier(
            utils::convertAndScaleDownUint8ToDouble(
                primaryFloorActivationWindowMultiplier));
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
        AdminPowerProfileIntf::primaryFloorTargetWindowMultiplier(
            utils::convertAndScaleDownUint8ToDouble(
                primaryFloorTargetWindowMultiplier));
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
        AdminPowerProfileIntf::primaryFloorActivationOffset(reading);
        return rc;
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

    requester::Coroutine getAdminProfileFromDeviceV2(
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("getAdminProfileFromDeviceV2 for EID: {EID}", "EID", eid);

        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_get_admin_override_profile_info_v2_req(0, requestMsg);

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "encode_get_admin_override_profile_info_v2_req failed. eid={EID}, rc={RC}",
                "EID", eid, "RC", rc);
            co_return rc;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                                   responseLen);
        if (rc_)
        {
            lg2::error(
                "getAdminProfileFromDeviceV2 postPatchIO failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            co_return rc_;
        }

        uint8_t cc{};
        uint16_t telemetry_count{};
        size_t consumed_len{};
        auto response_data =
            reinterpret_cast<const uint8_t*>(responseMsg.get());
        rc = decode_get_admin_override_profile_info_v2_resp(
            responseMsg.get(), responseLen, &consumed_len, &cc,
            &telemetry_count);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            lg2::info(
                "decode_get_admin_override_profile_info_v2_resp successful");
        }
        else
        {
            lg2::error(
                "decode_get_admin_override_profile_info_v2_resp failed cc={CC}, rc={RC}",
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

    requester::Coroutine overrideAdminProfileParam(
        uint8_t parameterId, double paramValue,
        AsyncOperationStatusType* status,
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info(
            "overrideAdminProfileParam for EID: {EID} parameterId:{ID}, parameterValue: {PARAMVALUE}",
            "EID", eid, "ID", parameterId, "PARAMVALUE", paramValue);

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_setup_admin_override_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        uint32_t parameValueTobeSet = paramValue;
        if (parameterId == 0)
        {
            parameValueTobeSet = doubleToNvUFXP4_12(paramValue);
        }
        else if (parameterId == 5 || parameterId == 6)
        {
            parameValueTobeSet = doubleToNvU8(paramValue);
        }
        else
        {
            // watts/sec to watts/msec or seconds to miliseconds
            parameValueTobeSet = paramValue * 1000;
        }
        // first argument instanceid=0 is irrelevant
        auto rc = encode_setup_admin_override_req(
            0, parameterId, parameValueTobeSet, requestMsg);
        std::string msg = utils::requestMsgToHexString(request);

        if (rc)
        {
            lg2::error(
                "overrideAdminProfileParam: encode_setup_admin_override_req failed. eid={EID}, rc={RC}, paramId={ID}, paramValue={VAL}",
                "EID", eid, "RC", rc, "ID", parameterId, "VAL", paramValue);
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
                "overrideAdminProfileParam SendRecvNsmMsgSync failed for eid = {EID} rc = {RC},paramId={ID}, paramValue={VAL}, NSM_Request={MSG}",
                "EID", eid, "RC", rc_, "ID", parameterId, "VAL", paramValue,
                "MSG", msg);
            *status = AsyncOperationStatusType::WriteFailure;
            // coverity[missing_return]
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_setup_admin_override_resp(responseMsg.get(), responseLen,
                                              &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            if (device->isCommandSupported(
                    NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                    NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2))
            {
                co_await getAdminProfileFromDeviceV2(device);
            }
            else
            {
                co_await OemAdminProfileIntf::getAdminProfileFromDevice();
            }
        }
        else
        {
            lg2::error(
                "overrideAdminProfileParam decode_setup_admin_override_resp  failed.eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A},paramId={ID}, paramValue={VAL}, NSM_Request={MSG}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc, "ID",
                parameterId, "VAL", paramValue, "MSG", msg);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

    bool resetParam(double reading)
    {
        if (static_cast<uint32_t>(reading) == INVALID_POWER_LIMIT)
            return true;
        return false;
    }

    requester::Coroutine resetAdminProfileParam(
        uint8_t parameterId, AsyncOperationStatusType* status,
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        uint32_t paramValue = INVALID_POWER_LIMIT;
        lg2::info(
            "resetAdminProfileParam for EID: {EID} parameterId:{ID}, parameterValue: {PARAMVALUE}",
            "EID", eid, "ID", parameterId, "PARAMVALUE", paramValue);

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_setup_admin_override_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

        // first argument instanceid=0 is irrelevant
        auto rc = encode_setup_admin_override_req(0, parameterId, paramValue,
                                                  requestMsg);
        std::string msg = utils::requestMsgToHexString(request);

        if (rc)
        {
            lg2::error(
                "resetAdminProfileParam: encode_setup_admin_override_req failed. eid={EID}, rc={RC}, paramId={ID}, paramValue={VAL}",
                "EID", eid, "RC", rc, "ID", parameterId, "VAL", paramValue);
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
                "resetAdminProfileParam SendRecvNsmMsgSync failed for eid = {EID} rc = {RC},paramId={ID}, paramValue={VAL}, NSM_Request={MSG}",
                "EID", eid, "RC", rc_, "ID", parameterId, "VAL", paramValue,
                "MSG", msg);
            *status = AsyncOperationStatusType::WriteFailure;
            // coverity[missing_return]
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_setup_admin_override_resp(responseMsg.get(), responseLen,
                                              &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            if (device->isCommandSupported(
                    NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                    NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2))
            {
                co_await getAdminProfileFromDeviceV2(device);
            }
            else
            {
                co_await OemAdminProfileIntf::getAdminProfileFromDevice();
            }
        }
        else
        {
            lg2::error(
                "resetAdminProfileParam decode_setup_admin_override_resp  failed.eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A},paramId={ID}, paramValue={VAL}, NSM_Request={MSG}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc, "ID",
                parameterId, "VAL", paramValue, "MSG", msg);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

    requester::Coroutine
        setTmpFloorPercent(const AsyncSetOperationValueType& value,
                           AsyncOperationStatusType* status,
                           [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        const double* floorPercent = std::get_if<double>(&value);

        if (!floorPercent)
        {
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }

        if (resetParam(*floorPercent))
        {
            auto rc = co_await resetAdminProfileParam(0, status, device);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            // percent to fraction
            auto rc = co_await overrideAdminProfileParam(0, *floorPercent / 100,
                                                         status, device);
            // coverity[missing_return]
            co_return rc;
        }
    }

    requester::Coroutine
        setRampUpRate(const AsyncSetOperationValueType& value,
                      AsyncOperationStatusType* status,
                      [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        const double* ramupRate = std::get_if<double>(&value);

        if (!ramupRate)
        {
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }

        if (resetParam(*ramupRate))
        {
            auto rc = co_await resetAdminProfileParam(1, status, device);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            auto rc = co_await overrideAdminProfileParam(1, *ramupRate, status,
                                                         device);
            // coverity[missing_return]
            co_return rc;
        }
    }

    requester::Coroutine
        setRampDownRate(const AsyncSetOperationValueType& value,
                        AsyncOperationStatusType* status,
                        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        const double* rampDownRate = std::get_if<double>(&value);

        if (!rampDownRate)
        {
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }

        if (resetParam(*rampDownRate))
        {
            auto rc = co_await resetAdminProfileParam(2, status, device);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            auto rc = co_await overrideAdminProfileParam(2, *rampDownRate,
                                                         status, device);
            // coverity[missing_return]
            co_return rc;
        }
    }

    requester::Coroutine setRampDownHysteresis(
        const AsyncSetOperationValueType& value,
        AsyncOperationStatusType* status,
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        const double* rampDownHysteresis = std::get_if<double>(&value);

        if (!rampDownHysteresis)
        {
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }

        if (resetParam(*rampDownHysteresis))
        {
            auto rc = co_await resetAdminProfileParam(3, status, device);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            auto rc = co_await overrideAdminProfileParam(3, *rampDownHysteresis,
                                                         status, device);
            // coverity[missing_return]
            co_return rc;
        }
    }

    requester::Coroutine setSecondaryPowerFloor(
        const AsyncSetOperationValueType& value,
        AsyncOperationStatusType* status,
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        const double* secondaryPowerFloor = std::get_if<double>(&value);

        if (!secondaryPowerFloor)
        {
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
        if (resetParam(*secondaryPowerFloor))
        {
            auto rc = co_await resetAdminProfileParam(4, status, device);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            auto rc = co_await overrideAdminProfileParam(
                4, *secondaryPowerFloor, status, device);
            // coverity[missing_return]
            co_return rc;
        }
    }

    requester::Coroutine setPrimaryFloorActivationWindowMultiplier(
        const AsyncSetOperationValueType& value,
        AsyncOperationStatusType* status,
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        const double* primaryFloorActivationWindowMultiplier =
            std::get_if<double>(&value);

        if (!primaryFloorActivationWindowMultiplier)
        {
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
        if (resetParam(*primaryFloorActivationWindowMultiplier))
        {
            auto rc = co_await resetAdminProfileParam(5, status, device);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            auto rc = co_await overrideAdminProfileParam(
                5, *primaryFloorActivationWindowMultiplier, status, device);
            // coverity[missing_return]
            co_return rc;
        }
    }

    requester::Coroutine setPrimaryFloorTargetWindowMultiplier(
        const AsyncSetOperationValueType& value,
        AsyncOperationStatusType* status,
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        const double* primaryFloorTargetWindowMultiplier =
            std::get_if<double>(&value);

        if (!primaryFloorTargetWindowMultiplier)
        {
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
        if (resetParam(*primaryFloorTargetWindowMultiplier))
        {
            auto rc = co_await resetAdminProfileParam(6, status, device);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            auto rc = co_await overrideAdminProfileParam(
                6, *primaryFloorTargetWindowMultiplier, status, device);
            // coverity[missing_return]
            co_return rc;
        }
    }

    requester::Coroutine setPrimaryFloorActivationOffset(
        const AsyncSetOperationValueType& value,
        AsyncOperationStatusType* status,
        [[maybe_unused]] std::shared_ptr<NsmDevice> device)
    {
        const double* primaryFloorActivationOffset =
            std::get_if<double>(&value);

        if (!primaryFloorActivationOffset)
        {
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
        if (resetParam(*primaryFloorActivationOffset))
        {
            auto rc = co_await resetAdminProfileParam(7, status, device);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            auto rc = co_await overrideAdminProfileParam(
                7, *primaryFloorActivationOffset, status, device);
            // coverity[missing_return]
            co_return rc;
        }
    }
};
} // namespace nsm