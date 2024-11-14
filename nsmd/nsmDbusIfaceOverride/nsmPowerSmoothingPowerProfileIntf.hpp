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

#include <com/nvidia/PowerSmoothing/PowerProfile/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

#include <cstdint>
namespace nsm
{
using PowerProfileIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerSmoothing::server::PowerProfile>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
class OemPowerProfileIntf :
    public PowerProfileIntf,
    public AssociationDefinitionsIntf
{
  private:
    std::shared_ptr<NsmDevice> device;
    std::string parentPath;
    std::string inventoryObjPath;
    uint8_t profileId;

  public:
    OemPowerProfileIntf(sdbusplus::bus::bus& bus, const std::string& parentPath,
                        uint8_t profileId, std::shared_ptr<NsmDevice> device) :
        PowerProfileIntf(
            bus,
            (parentPath + "/profile/" + std::to_string(profileId)).c_str()),
        AssociationDefinitionsIntf(
            bus,
            (parentPath + "/profile/" + std::to_string(profileId)).c_str()),
        device(device), parentPath(parentPath),
        inventoryObjPath(parentPath + "/profile/" + std::to_string(profileId)),
        profileId(profileId)

    {
        std::vector<std::tuple<std::string, std::string, std::string>>
            associations_list;
        std::string forward = "parent_device";
        std::string backward = "power_profile";
        associations_list.emplace_back(forward, backward, parentPath);
        AssociationDefinitionsIntf::associations(associations_list);
    }
    std::string getInventoryObjPath()
    {
        return inventoryObjPath;
    }

    requester::Coroutine getProfileInfoFromDevice()
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("getProfileInfo for EID: {EID}", "EID", eid);

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_setup_admin_override_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_get_preset_profile_req(0, requestMsg);

        if (rc)
        {
            lg2::error(
                "getProfileInfo: encode_get_preset_profile_req failed for eid={EID}, rc={RC}",
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
                "getProfileInfo SendRecvNsmMsg failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);

            co_return rc_;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        nsm_get_all_preset_profile_meta_data data{};
        uint8_t numberOfprofiles = 0;
        rc = decode_get_preset_profile_metadata_resp(
            responseMsg.get(), responseLen, &cc, &reason_code, &data,
            &numberOfprofiles);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            for (int profileIdInResponse = 0;
                 profileIdInResponse < numberOfprofiles; profileIdInResponse++)
            {
                if (profileIdInResponse == profileId)
                {
                    nsm_preset_profile_data profileData{};
                    decode_get_preset_profile_data_from_resp(
                        responseMsg.get(), responseLen, &cc, &reason_code,
                        numberOfprofiles, profileId, &profileData);
                    // fraction to percent
                    PowerProfileIntf::tmpFloorPercent(
                        NvUFXP4_12ToDouble(
                            profileData.tmp_floor_setting_in_percent) *
                        100);
                    // mw/sec to watts/sec
                    PowerProfileIntf::rampUpRate(
                        profileData.ramp_up_rate_in_miliwattspersec / 1000);
                    // mw/sec to watts/sec
                    PowerProfileIntf::rampDownRate(
                        profileData.ramp_down_rate_in_miliwattspersec / 1000);
                    // milisec to second
                    PowerProfileIntf::rampDownHysteresis(
                        profileData.ramp_hysterisis_rate_in_milisec / 1000);
                    lg2::info(
                        "getProfileInfo for EID: {EID} completed for profile Id: {ID}",
                        "EID", eid, "ID", profileId);
                }
            }
        }
        else
        {
            lg2::error(
                "getProfileInfo decode_get_preset_profile_metadata_resp/decode_get_preset_profile_data_from_resp. eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            co_return rc;
        }
        co_return NSM_SW_SUCCESS;
    }

    requester::Coroutine
        updateProfileInfoOnDevice(uint8_t parameterId, double paramValue,
                                  AsyncOperationStatusType* status)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_setup_admin_override_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        uint32_t parameValueTobeSet = paramValue;
        if (parameterId == 0)
        {
            parameValueTobeSet = doubleToNvUFXP4_12(paramValue);
        }
        else
        {
            // watts/sec to watts/msec or seconds to miliseconds
            parameValueTobeSet = paramValue * 1000;
        }
        lg2::info(
            "updateProfileInfoOnDevice for EID: {EID} parameterId:{ID}, paramValue: {VALUE}, profileID: {PROFILEID}",
            "EID", eid, "ID", parameterId, "PROFILEID", profileId, "VALUE",
            paramValue);
        // first argument instanceid=0 is irrelevant
        auto rc = encode_update_preset_profile_param_req(
            0, profileId, parameterId, parameValueTobeSet, requestMsg);

        if (rc)
        {
            lg2::error(
                "updateProfileInfoOnDevice: encode_setup_admin_override_req failed(parameterId:{ID}, paramValue: {VALUE}, profileID: {PROFILEID}) for eid={EID}, rc={RC}",
                "EID", eid, "RC", rc, "ID", parameterId, "PROFILEID", profileId,
                "VALUE", paramValue);
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
                "updateProfileInfoOnDevice SendRecvNsmMsg failed(parameterId:{ID}, paramValue: {VALUE}, profileID: {PROFILEID}) for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_, "ID", parameterId, "PROFILEID",
                profileId, "VALUE", paramValue);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_update_preset_profile_param_resp(
            responseMsg.get(), responseLen, &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            // verify setting is applied on the device
            co_await getProfileInfoFromDevice();
        }
        else
        {
            lg2::error(
                "updateProfileInfoOnDevice decode_update_preset_profile_param_resp  failed(parameterId:{ID}, paramValue: {VALUE}, profileID: {PROFILEID}). eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc, "ID",
                parameterId, "PROFILEID", profileId, "VALUE", paramValue);
            *status = AsyncOperationStatusType::WriteFailure;
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
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
        // percent to fraction
        const auto rc =
            co_await updateProfileInfoOnDevice(0, *floorPercent / 100, status);
        // coverity[missing_return]
        co_return rc;
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

        const auto rc = co_await updateProfileInfoOnDevice(1, *ramupRate,
                                                           status);
        // coverity[missing_return]
        co_return rc;
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

        const auto rc = co_await updateProfileInfoOnDevice(2, *rampDownRate,
                                                           status);
        // coverity[missing_return]
        co_return rc;
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

        const auto rc =
            co_await updateProfileInfoOnDevice(3, *rampDownHysteresis, status);
        // coverity[missing_return]
        co_return rc;
    }
};
} // namespace nsm
