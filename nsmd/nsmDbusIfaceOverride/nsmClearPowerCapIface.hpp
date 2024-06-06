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

#include "nsmDevice.hpp"
#include "nsmPowerCapIface.hpp"
#include "sensorManager.hpp"

#include <com/nvidia/Common/ClearPowerCap/server.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>

namespace nsm
{
using ClearPowerCapIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::Common::server::ClearPowerCap>;

class NsmClearPowerCapIntf : public ClearPowerCapIntf
{
  public:
    NsmClearPowerCapIntf(sdbusplus::bus::bus& bus, const char* path,
                         uuid_t uuid,
                         std::shared_ptr<NsmPowerCapIntf> powerCapIntf) :
        ClearPowerCapIntf(bus, path), uuid(uuid), powerCapIntf(powerCapIntf)
    {}

    void getPowerCapFromDevice()
    {
        SensorManager& manager = SensorManager::getInstance();
        auto device = manager.getNsmDevice(uuid);
        auto eid = manager.getEid(device);
        lg2::info("getPowerCapFromDevice for EID: {EID}", "EID", eid);
        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_power_limit_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_get_device_power_limit_req(0, requestMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "getPowerCapFromDevice encode_get_device_power_limit_req failed.eid = {EID} rc ={RC}",
                "EID", eid, "RC", rc);
        }
        const nsm_msg* responseMsg = NULL;
        size_t responseLen = 0;
        auto rc_ = manager.SendRecvNsmMsgSync(eid, request, &responseMsg,
                                              &responseLen);
        if (rc_)
        {
            lg2::error("SendRecvNsmMsgSync failed.eid={EID} rc={RC}", "EID",
                       eid, "RC", rc_);
            return;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size = 0;
        uint32_t requested_persistent_limit = 0;
        uint32_t requested_oneshot_limit = 0;
        uint32_t enforced_limit = 0;

        rc = decode_get_power_limit_resp(
            responseMsg, responseLen, &cc, &data_size, &reason_code,
            &requested_persistent_limit, &requested_oneshot_limit,
            &enforced_limit);
        free((void*)responseMsg);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            powerCapIntf->PowerCapIntf::powerCap(enforced_limit);
            lg2::info("getPowerCapFromDevice for EID: {EID} completed", "EID",
                      eid);
        }
        else
        {
            lg2::error(
                "getPowerCapFromDevice: decode_get_power_limit_resp with reasonCode = {REASONCODE},cc = {CC}and rc ={RC}",
                "REASONCODE", reason_code, "CC", cc, "RC", rc);
        }
    }

    void clearPowerCapOnDevice()
    {
        SensorManager& manager = SensorManager::getInstance();
        auto device = manager.getNsmDevice(uuid);
        auto eid = manager.getEid(device);
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_limit_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_set_device_power_limit_req(
            0, DEFAULT_LIMIT, PERSISTENT, ClearPowerCapIntf::defaultPowerCap(),
            requestMsg);

        if (rc)
        {
            lg2::error(
                "clearPowerCapOnDevice encode_set_device_power_limit_req failed. eid={EID}, rc={RC}",
                "EID", eid, "RC", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        const nsm_msg* responseMsg = NULL;
        size_t responseLen = 0;
        auto rc_ = manager.SendRecvNsmMsgSync(eid, request, &responseMsg,
                                              &responseLen);
        if (rc_)
        {
            lg2::error(
                "clearPowerCapOnDevice SendRecvNsmMsgSync failed for while setting power limit for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size = 0;
        rc = decode_set_power_limit_resp(responseMsg, responseLen, &cc,
                                         &data_size, &reason_code);
        free((void*)responseMsg);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            // verify setting is applied on the device
            getPowerCapFromDevice();
            lg2::info("clearPowerCapOnDevice for EID: {EID} completed", "EID",
                      eid);
        }
        else
        {
            lg2::error(
                "clearPowerCapOnDevice decode_set_power_limit_resp failed.eid ={EID},CC = {CC} reasoncode = {RC},RC = {A} ",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    int32_t clearPowerCap() override
    {
        clearPowerCapOnDevice();
        return 0;
    }

  private:
    uuid_t uuid;
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf = nullptr;
};
} // namespace nsm
