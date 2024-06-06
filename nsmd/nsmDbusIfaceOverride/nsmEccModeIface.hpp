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
#include <xyz/openbmc_project/Memory/MemoryECC/server.hpp>

#include <cstdint>

namespace nsm
{
using EccModeIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Memory::server::MemoryECC>;

class NsmEccModeIntf : public EccModeIntf
{
  public:
    NsmEccModeIntf(sdbusplus::bus::bus& bus, const char* path,
                   std::shared_ptr<NsmDevice> device) :
        EccModeIntf(bus, path), device(device)
    {}

    void getECCModeFromDevice()
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("getECCModeFromDevice for EID: {EID}", "EID", eid);
        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_get_ECC_mode_req(0, requestMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("getECCModeFromDevice encode_get_ECC_mode_req failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);
        }
        const nsm_msg* responseMsg = NULL;
        size_t responseLen = 0;
        auto rc_ = manager.SendRecvNsmMsgSync(eid, request, &responseMsg,
                                              &responseLen);
        if (rc_)
        {
            if (rc_ != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
            {
                lg2::error("SendRecvNsmMsgSync failed. "
                           "eid={EID} rc={RC}",
                           "EID", eid, "RC", rc_);
            }
            return;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        bitfield8_t flags;
        uint16_t data_size = 0;

        rc = decode_get_ECC_mode_resp(responseMsg, responseLen, &cc, &data_size,
                                      &reason_code, &flags);
        free((void*)responseMsg);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            EccModeIntf::eccModeEnabled(flags.bits.bit0);
            EccModeIntf::pendingECCState(flags.bits.bit1);
            lg2::info("getECCModeFromDevice for EID: {EID} completed", "EID",
                      eid);
        }
        else
        {
            lg2::error(
                "getECCModeFromDevice: decode_get_ECC_mode_resp with reasonCode = {REASONCODE},cc = {CC}and rc={RC}",
                "REASONCODE", reason_code, "CC", cc, "RC", rc);
        }
    }

    void setECCModeOnDevice(bool eccMode)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("setECCModeOnDevice for EID: {EID}", "EID", eid);
        // NSM spec expects  requestedECCMode mode to be uint8_t
        uint8_t requestedECCMode = static_cast<uint8_t>(eccMode);
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_ECC_mode_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_set_ECC_mode_req(0, requestedECCMode, requestMsg);

        if (rc)
        {
            lg2::error(
                "setECCModeOnDevice encode_set_ECC_mode_req failed. eid={EID} rc={RC}",
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
            if (rc_ != NSM_ERR_UNSUPPORTED_COMMAND_CODE)
            {
                lg2::error(
                    "setECCModeOnDevice SendRecvNsmMsgSync failed for while setting ECCMode "
                    "eid={EID} rc={RC}",
                    "EID", eid, "RC", rc_);
            }
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size = 0;
        rc = decode_set_ECC_mode_resp(responseMsg, responseLen, &cc,
                                      &reason_code, &data_size);
        free((void*)responseMsg);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            // verify setting is applied on the device
            getECCModeFromDevice();
            lg2::info("setECCModeOnDevice for EID: {EID} completed", "EID",
                      eid);
        }
        else
        {
            lg2::error(
                "setECCModeOnDevice decode_set_ECC_mode_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            lg2::error("throwing write failure exception");
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    bool eccModeEnabled() const override
    {
        return EccModeIntf::eccModeEnabled();
    }

    bool eccModeEnabled(bool eccMode) override
    {
        setECCModeOnDevice(eccMode);
        return EccModeIntf::eccModeEnabled();
    }

  private:
    std::shared_ptr<NsmDevice> device;
};
} // namespace nsm
