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
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Control/Power/Cap/server.hpp>

#include <cstdint>

namespace nsm
{
using PowerCapIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Control::Power::server::Cap>;

class NsmPowerCapIntf : public PowerCapIntf
{
  public:
    NsmPowerCapIntf(sdbusplus::bus::bus& bus, const char* path, uuid_t uuid) :
        PowerCapIntf(bus, path), uuid(uuid)
    {}

    void getPowerCapFromDevice()
    {
        SensorManager& manager = SensorManager::getInstance();
        auto device = manager.getNsmDevice(uuid);
        auto eid = manager.getEid(device);

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
            lg2::error("SendRecvNsmMsgSync failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc_);
            free((void*)responseMsg);
            return;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size = 0;
        uint32_t requested_persistent_limit_in_miliwatts;
        uint32_t requested_oneshot_limit_in_miliwatts;
        uint32_t enforced_limit_in_miliwatts;

        rc = decode_get_power_limit_resp(
            responseMsg, responseLen, &cc, &data_size, &reason_code,
            &requested_persistent_limit_in_miliwatts,
            &requested_oneshot_limit_in_miliwatts,
            &enforced_limit_in_miliwatts);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            PowerCapIntf::powerCap(enforced_limit_in_miliwatts / 1000);
            lg2::info("getPowerCapFromDevice for EID: {EID} completed", "EID",
                      eid);
        }
        else
        {
            lg2::error(
                "getPowerCapFromDevice: decode_get_power_limit_resp with reasonCode = {REASONCODE},cc = {CC}and rc = {RC} ",
                "REASONCODE", reason_code, "CC", cc, "RC", rc);
        }
    }

    void setPowerCapOnDevice(uint32_t power_limit)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto device = manager.getNsmDevice(uuid);
        auto eid = manager.getEid(device);
        lg2::info("setPowerCapOnDevice for EID: {EID}", "EID", eid);
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_limit_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_set_device_power_limit_req(
            0, NEW_LIMIT, PERSISTENT, power_limit * 1000, requestMsg);

        if (rc)
        {
            lg2::error(
                "setPowerCapOnDevice encode_set_device_power_limit_req failed. eid={EID}, rc={RC}",
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
                "setPowerCapOnDevice SendRecvNsmMsgSync failed for whilesetting power limit for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            free((void*)responseMsg);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size = 0;
        rc = decode_set_power_limit_resp(responseMsg, responseLen, &cc,
                                         &data_size, &reason_code);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            // verify setting is applied on the device
            getPowerCapFromDevice();
            lg2::info("setPowerCapOnDevice for EID: {EID} completed", "EID",
                      eid);
        }
        else
        {
            lg2::error(
                "setPowerCapOnDevice decode_set_power_limit_resp failed.eid = {EID}, CC = {CC} reasoncode = {RC}, RC = {A} ",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    uint32_t powerCap(uint32_t power_limit) override
    {
        if (power_limit > PowerCapIntf::maxPowerCapValue() ||
            power_limit < PowerCapIntf::minPowerCapValue())
        {
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
        setPowerCapOnDevice(power_limit);
        return PowerCapIntf::powerCap();
    }

  private:
    uuid_t uuid;
};
} // namespace nsm
