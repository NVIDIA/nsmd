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
#include "asyncOperationManager.hpp"

#include <com/nvidia/PowerSmoothing/PowerSmoothing/server.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
namespace nsm
{
using PowerSmoothingIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerSmoothing::server::PowerSmoothing>;
class OemPowerSmoothingFeatIntf : public PowerSmoothingIntf
{
  private:
    std::shared_ptr<NsmDevice> device;
    std::string inventoryObjPath;

  public:
    OemPowerSmoothingFeatIntf(sdbusplus::bus::bus& bus,
                              const std::string& inventoryObjPath,
                              std::shared_ptr<NsmDevice> device) :
        PowerSmoothingIntf(bus, (inventoryObjPath).c_str()),
        device(device), inventoryObjPath(inventoryObjPath)

    {}
    std::string getInventoryObjPath()
    {
        return inventoryObjPath;
    }

    requester::Coroutine getPwrSmoothingControlsFromDevice()
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("getPwrSmoothingControlsFromDevice for EID: {EID}", "EID",
                  eid);

        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_get_powersmoothing_featinfo_req(0, requestMsg);

        if (rc)
        {
            lg2::error(
                "getPwrSmoothingControlsFromDevice: encode_get_powersmoothing_featinfo_req failed. eid={EID}, rc={RC}",
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
                "getPwrSmoothingControlsFromDevice SendRecvNsmMsg failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            // coverity[missing_return]
            co_return rc_;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataSize = 0;
        nsm_pwr_smoothing_featureinfo_data data{};
        rc = decode_get_powersmoothing_featinfo_resp(
            responseMsg.get(), responseLen, &cc, &reason_code, &dataSize,
            &data);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            // For feature Supported : Check if bit0 is set
            bool featSupported = (data.feature_flag & (1u << 0)) != 0 ? true
                                                                      : false;
            PowerSmoothingIntf::featureSupported(featSupported);

            // For feature Enabled: Check if bit1 is set
            bool featureEnabled = (data.feature_flag & (1u << 1)) != 0 ? true
                                                                       : false;
            PowerSmoothingIntf::powerSmoothingEnabled(featureEnabled);

            // For RampDwon Enabled: Check if bit2 is set
            bool rampDownEnabled = (data.feature_flag & (1u << 2)) != 0 ? true
                                                                        : false;
            PowerSmoothingIntf::immediateRampDownEnabled(rampDownEnabled);

            PowerSmoothingIntf::currentTempSetting(data.currentTmpSetting);
            PowerSmoothingIntf::currentTempFloorSetting(
                data.currentTmpFloorSetting);
            PowerSmoothingIntf::maxAllowedTmpFloorPercent(
                NvUFXP4_12ToDouble(data.maxTmpFloorSettingInPercent));
            PowerSmoothingIntf::minAllowedTmpFloorPercent(
                NvUFXP4_12ToDouble(data.minTmpFloorSettingInPercent));
            lg2::info("getPwrSmoothingControlsFromDevice completed");
        }
        else
        {
            lg2::error(
                "getPwrSmoothingControlsFromDevice decode_get_powersmoothing_featinfo_resp  failed.eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            // coverity[missing_return]
            co_return rc;
        }
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

    requester::Coroutine
        togglePowerSmoothingOnDevice(bool featureEnabled,
                                     AsyncOperationStatusType* status)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("togglePowerSmoothingOnDevice for EID: {EID}", "EID", eid);
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
            // verify setting is applied on the device
            co_await getPwrSmoothingControlsFromDevice();
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
                                                              status);

        // coverity[missing_return]
        co_return rc;
    }

    requester::Coroutine
        toggleImmediateRampDownOnDevice(bool ramdownEnabled,
                                        AsyncOperationStatusType* status)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("toggleImmediateRampDownOnDevice for EID: {EID}", "EID", eid);
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
            co_await getPwrSmoothingControlsFromDevice();
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
            co_await toggleImmediateRampDownOnDevice(*ramdownEnabled, status);

        co_return rc;
    }
};
} // namespace nsm
