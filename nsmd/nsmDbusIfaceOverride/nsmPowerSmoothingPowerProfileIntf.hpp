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

    void getProfileInfoFromDevice()
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
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto rc_ = manager.SendRecvNsmMsgSync(eid, request, responseMsg,
                                              responseLen);
        if (rc_)
        {
            lg2::error(
                "getProfileInfo SendRecvNsmMsgSync failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
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
                    PowerProfileIntf::tmpFloorPercent(
                        profileData.tmp_floor_setting_in_percent);
                    PowerProfileIntf::rampUpRate(
                        profileData.ramp_up_rate_in_miliwattspersec);
                    PowerProfileIntf::rampDownRate(
                        profileData.ramp_down_rate_in_miliwattspersec);
                    PowerProfileIntf::rampDownHysteresis(
                        profileData.ramp_hysterisis_rate_in_miliwattspersec);
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
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    void updateProfileInfoOnDevice(uint8_t parameterId, double paramValue)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info(
            "updateProfileInfoOnDevice for EID: {EID} parameterId:{ID}, profileID: {PROFILEID}",
            "EID", eid, "ID", parameterId, "PROFILEID", profileId);

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_setup_admin_override_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        uint32_t parameValueTobeSet = paramValue;
        if (parameterId == 0)
        {
            parameValueTobeSet = doubleToNvUFXP4_12(paramValue) * 1 << 16;
        }
        // first argument instanceid=0 is irrelevant
        auto rc = encode_update_preset_profile_param_req(
            0, profileId, parameterId, parameValueTobeSet, requestMsg);

        if (rc)
        {
            lg2::error(
                "updateProfileInfoOnDevice: encode_setup_admin_override_req failed(parameterId:{ID}, profileID: {PROFILEID}) for eid={EID}, rc={RC}",
                "EID", eid, "RC", rc, "ID", parameterId, "PROFILEID",
                profileId);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto rc_ = manager.SendRecvNsmMsgSync(eid, request, responseMsg,
                                              responseLen);
        if (rc_)
        {
            lg2::error(
                "updateProfileInfoOnDevice SendRecvNsmMsgSync failed(parameterId:{ID}, profileID: {PROFILEID}) for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_, "ID", parameterId, "PROFILEID",
                profileId);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_update_preset_profile_param_resp(
            responseMsg.get(), responseLen, &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            // verify setting is applied on the device
            getProfileInfoFromDevice();
        }
        else
        {
            lg2::error(
                "updateProfileInfoOnDevice decode_update_preset_profile_param_resp  failed(parameterId:{ID}, profileID: {PROFILEID}). eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc, "ID",
                parameterId, "PROFILEID", profileId);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    double tmpFloorPercent(double floorPercent) override
    {
        updateProfileInfoOnDevice(0, floorPercent);
        return PowerProfileIntf::tmpFloorPercent();
    }

    double rampUpRate(double ramupRate) override
    {
        updateProfileInfoOnDevice(1, ramupRate);
        return PowerProfileIntf::rampUpRate();
    }

    double rampDownRate(double rampDownRate) override
    {
        updateProfileInfoOnDevice(2, rampDownRate);
        return PowerProfileIntf::rampDownRate();
    }

    double rampDownHysteresis(double rampDownHysterisis) override
    {
        updateProfileInfoOnDevice(3, rampDownHysterisis);
        return PowerProfileIntf::rampDownHysteresis();
    }
};
} // namespace nsm
