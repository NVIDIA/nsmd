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
        PowerSmoothingIntf(bus, (inventoryObjPath).c_str()), device(device),
        inventoryObjPath(inventoryObjPath)

    {}
    std::string getInventoryObjPath()
    {
        return inventoryObjPath;
    }

    void togglePowerSmoothingOnDevice(bool featureEnabled)
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
                "togglePowerSmoothingOnDevice SendRecvNsmMsgSync failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_toggle_feature_state_resp(responseMsg.get(), responseLen,
                                              &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            // verify setting is applied on the device
            // getPowerCapFromDevice();
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
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    bool powerSmoothingEnabled(bool featureEnabled) override
    {
        togglePowerSmoothingOnDevice(featureEnabled);
        return PowerSmoothingIntf::powerSmoothingEnabled();
    }

    void toggleImmediateRampDownOnDevice(bool featureEnabled)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        lg2::info("toggleImmediateRampDownOnDevice for EID: {EID}", "EID", eid);
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_toggle_immediate_rampdown_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        uint8_t feature_state = featureEnabled;
        // first argument instanceid=0 is irrelevant
        auto rc = encode_toggle_immediate_rampdown_req(0, feature_state,
                                                       requestMsg);

        if (rc)
        {
            lg2::error(
                "toggleImmediateRampDownOnDevice: encode_toggle_immediate_rampdown_req failed. eid={EID}, rc={RC}",
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
                "toggleImmediateRampDownOnDevice SendRecvNsmMsgSync failed for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_toggle_immediate_rampdown_resp(
            responseMsg.get(), responseLen, &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
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
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    bool immediateRampDownEnabled(bool featureEnabled) override
    {
        toggleImmediateRampDownOnDevice(featureEnabled);
        return PowerSmoothingIntf::immediateRampDownEnabled();
    }
};
} // namespace nsm
