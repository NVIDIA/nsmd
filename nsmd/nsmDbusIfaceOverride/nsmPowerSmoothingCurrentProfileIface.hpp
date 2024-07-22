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
#include <com/nvidia/PowerSmoothing/CurrentPowerProfile/server.hpp>
namespace nsm
{
using CurrentPowerProfileIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerSmoothing::server::CurrentPowerProfile>;
class OemCurrentPowerProfileIntf : public CurrentPowerProfileIntf
{
  private:
    std::shared_ptr<NsmDevice> device;
    std::string inventoryObjPath;
    //  signal emission is deferred until the initialization is complete
  public:
    OemCurrentPowerProfileIntf(sdbusplus::bus::bus& bus,
                               const std::string& path,
                               std::string adminProfilePath,
                               std::shared_ptr<NsmDevice> device) :
        CurrentPowerProfileIntf(bus, path.c_str(), action::defer_emit),
        device(device), inventoryObjPath(path)
    {
        CurrentPowerProfileIntf::appliedProfilePath(path);
        CurrentPowerProfileIntf::adminProfilePath(adminProfilePath);
    }

    void activatePresetProfile(uint16_t profileID) override
    {
        lg2::error("activatePresetProfile ProfileId={ID}", "ID", profileID);
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("activatePresetProfile for EID: {EID} profileID:{ID}", "EID",
                  eid, "ID", profileID);

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_active_preset_profile_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_set_active_preset_profile_req(0, profileID,
                                                       requestMsg);

        if (rc)
        {
            lg2::error(
                "activatePresetProfile: encode_set_active_preset_profile_req failed ProfileId ={ID}. eid={EID}, rc={RC}",
                "ID", profileID, "EID", eid, "RC", rc);
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
                "activatePresetProfile SendRecvNsmMsgSync failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_set_active_preset_profile_resp(
            responseMsg.get(), responseLen, &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            // verify setting is applied on the device
            // verifyOverrideAdminProfileParam(parameterId, paramValue);
            lg2::info("activatePresetProfile for EID: {EID} completed ", "EID",
                      eid);
        }
        else
        {
            lg2::error(
                "activatePresetProfile decode_set_active_preset_profile_resp  failed.eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    void applyAdminOverride() override
    {
        lg2::error("applyAdminOverride ");
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("applyAdminOverride for EID: {EID}", "EID", eid);

        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_apply_admin_override_req(0, requestMsg);

        if (rc)
        {
            lg2::error(
                "applyAdminOverride: encode_apply_admin_override_req failed. eid={EID}, rc={RC}",
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
                "applyAdminOverride SendRecvNsmMsgSync failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_apply_admin_override_resp(responseMsg.get(), responseLen,
                                              &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            lg2::info("applyAdminOverride for EID: {EID} completed ", "EID",
                      eid);
        }
        else
        {
            lg2::error(
                "activatePresetProfile decode_apply_admin_override_resp  failed.eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }
};
} // namespace nsm
