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

#include "asyncOperationManager.hpp"

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

    requester::Coroutine getAdminProfileFromDevice()
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
                "getAdminProfileFromDevice SendRecvNsmMsg failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            // coverity[missing_return]
            co_return rc_;
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
            // fraction to percent
            if (adminProfiledata.admin_override_percent_tmp_floor ==
                INVALID_UINT16_VALUE)
            {
                // on dbus we still show INVALID_UINT32_VALUE to keep consistent
                AdminPowerProfileIntf::tmpFloorPercent(INVALID_UINT32_VALUE);
            }
            else
            {
                AdminPowerProfileIntf::tmpFloorPercent(
                    NvUFXP4_12ToDouble(
                        adminProfiledata.admin_override_percent_tmp_floor) *
                    100);
            }

            // mw/sec to watts/sec
            double reading = utils::convertAndScaleDownUint32ToDouble(
                adminProfiledata
                    .admin_override_ramup_rate_in_miliwatts_per_second,
                1000);
            AdminPowerProfileIntf::rampUpRate(reading);

            // mw/sec to watts/sec
            reading = utils::convertAndScaleDownUint32ToDouble(
                adminProfiledata
                    .admin_override_rampdown_rate_in_miliwatts_per_second,
                1000);

            AdminPowerProfileIntf::rampDownRate(reading);

            // miliseconds to seconds
            reading = utils::convertAndScaleDownUint32ToDouble(
                adminProfiledata
                    .admin_override_rampdown_hysteresis_value_in_milisec,
                1000);
            AdminPowerProfileIntf::rampDownHysteresis(reading);
            lg2::info("getAdminProfileFromDevice for EID: {EID} completed ",
                      "EID", eid);
        }
        else
        {
            lg2::error(
                "getAdminProfileFromDevice decode_setup_admin_override_resp  failed.eid = {EID}, CC = {CC} reasoncode = {RC}, RC ={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            // coverity[missing_return]
            co_return rc;
        }
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

    requester::Coroutine
        overrideAdminProfileParam(uint8_t parameterId, double paramValue,
                                  AsyncOperationStatusType* status)
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
            co_await getAdminProfileFromDevice();
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

    requester::Coroutine
        resetAdminProfileParam(uint8_t parameterId,
                               AsyncOperationStatusType* status)
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
            co_await getAdminProfileFromDevice();
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
            auto rc = co_await resetAdminProfileParam(0, status);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            // percent to fraction
            auto rc = co_await overrideAdminProfileParam(0, *floorPercent / 100,
                                                         status);
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
            auto rc = co_await resetAdminProfileParam(1, status);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            auto rc = co_await overrideAdminProfileParam(1, *ramupRate, status);
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
            auto rc = co_await resetAdminProfileParam(2, status);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            auto rc = co_await overrideAdminProfileParam(2, *rampDownRate,
                                                         status);
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
            auto rc = co_await resetAdminProfileParam(3, status);
            // coverity[missing_return]
            co_return rc;
        }
        else
        {
            auto rc = co_await overrideAdminProfileParam(3, *rampDownHysteresis,
                                                         status);
            // coverity[missing_return]
            co_return rc;
        }
    }
};
} // namespace nsm
