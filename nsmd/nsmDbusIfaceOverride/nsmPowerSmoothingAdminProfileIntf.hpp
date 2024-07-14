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
#include <com/nvidia/PowerSmoothing/AdminPowerProfile/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
namespace nsm
{
using AdminPowerProfileIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PowerSmoothing::server::AdminPowerProfile>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
class OemAdminProfileIntf :
    public AdminPowerProfileIntf,
    public AssociationDefinitionsIntf
{
  private:
    std::shared_ptr<NsmDevice> device;
    std::string parentPath;
    std::string inventoryObjPath;

  public:
    OemAdminProfileIntf(sdbusplus::bus::bus& bus, const std::string& parentPath,
                        std::shared_ptr<NsmDevice> device) :
        AdminPowerProfileIntf(bus,
                              (parentPath + "/profile/admin_profile").c_str()),
        AssociationDefinitionsIntf(
            bus, (parentPath + "/profile/admin_profile").c_str()),
        device(device), parentPath(parentPath),
        inventoryObjPath(parentPath + "/profile/admin_profile")

    {
        std::vector<std::tuple<std::string, std::string, std::string>>
            associations_list;
        std::string forward = "parent_device";
        std::string backward = "admin_override";
        associations_list.emplace_back(forward, backward, parentPath);
        AssociationDefinitionsIntf::associations(associations_list);
    }
    std::string getInventoryObjPath()
    {
        return inventoryObjPath;
    }

    void getAdminProfileFromDevice()
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("getAdminProfileFromDevice for EID: {EID}", "EID", eid);

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_setup_admin_override_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_query_admin_override_req(0, requestMsg);

        if (rc)
        {
            lg2::error(
                "getAdminProfileFromDevice: encode_setup_admin_override_req failed. eid={EID}, rc={RC}",
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
                "getAdminProfileFromDevice SendRecvNsmMsgSync failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataSize = 0;
        nsm_admin_override_data adminProfiledata{};
        rc = decode_query_admin_override_resp(responseMsg.get(), responseLen,
                                              &cc, &reason_code, &dataSize,
                                              &adminProfiledata);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            AdminPowerProfileIntf::tmpFloorPercent(
                adminProfiledata.admin_override_percent_tmp_floor);
            AdminPowerProfileIntf::rampUpRate(
                adminProfiledata
                    .admin_override_ramup_rate_in_miliwatts_per_second);
            AdminPowerProfileIntf::rampDownRate(
                adminProfiledata
                    .admin_override_rampdown_rate_in_miliwatts_per_second);
            AdminPowerProfileIntf::rampDownHysteresis(
                adminProfiledata
                    .admin_override_rampdown_hysteresis_value_in_milisec);
            lg2::info("getAdminProfileFromDevice for EID: {EID} completed ",
                      "EID", eid);
        }
        else
        {
            lg2::error(
                "getAdminProfileFromDevice decode_setup_admin_override_resp  failed.eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    void overrideAdminProfileParam(uint8_t parameterId, double paramValue)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("overrideAdminProfileParam for EID: {EID} parameterId:{ID}",
                  "EID", eid, "ID", parameterId);

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_setup_admin_override_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_setup_admin_override_req(0, parameterId, paramValue,
                                                  requestMsg);

        if (rc)
        {
            lg2::error(
                "overrideAdminProfileParam: encode_setup_admin_override_req failed. eid={EID}, rc={RC}",
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
                "overrideAdminProfileParam SendRecvNsmMsgSync failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_setup_admin_override_resp(responseMsg.get(), responseLen,
                                              &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            getAdminProfileFromDevice();
        }
        else
        {
            lg2::error(
                "overrideAdminProfileParam decode_setup_admin_override_resp  failed.eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    double tmpFloorPercent(double floorPercent) override
    {
        overrideAdminProfileParam(0, floorPercent);
        return AdminPowerProfileIntf::tmpFloorPercent();
    }

    double rampUpRate(double ramupRate) override
    {
        overrideAdminProfileParam(1, ramupRate);
        return AdminPowerProfileIntf::rampUpRate();
    }

    double rampDownRate(double rampDownRate) override
    {
        overrideAdminProfileParam(2, rampDownRate);
        return AdminPowerProfileIntf::rampDownRate();
    }

    double rampDownHysteresis(double rampDownHysterisis) override
    {
        overrideAdminProfileParam(3, rampDownHysterisis);
        return AdminPowerProfileIntf::rampDownHysteresis();
    }
};
} // namespace nsm
