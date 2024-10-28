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
#include "nsmObjectFactory.hpp"
#include "nsmPowerSmoothingAdminProfileIntf.hpp"
#include "nsmPowerSmoothingCurrentProfileIface.hpp"
#include "nsmPowerSmoothingFeatureIntf.hpp"
#include "nsmPowerSmoothingPowerProfileIntf.hpp"
#include "nsmSensor.hpp"

#include <cstdint>

namespace nsm
{

//  Power Smoothing Control :  Feature Info
class NsmPowerSmoothing : public NsmSensor
{
  public:
    NsmPowerSmoothing(
        std::string& name, std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<OemPowerSmoothingFeatIntf> pwrSmoothingIntf);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateReading(struct nsm_pwr_smoothing_featureinfo_data* data);

  private:
    std::shared_ptr<OemPowerSmoothingFeatIntf> pwrSmoothingIntf;
    std::string inventoryObjPath;
};

//  Power Smoothing Control:  HW circuitry %Lifetime Usage telemetry
class NsmHwCircuitryTelemetry : public NsmSensor
{
  public:
    NsmHwCircuitryTelemetry(
        std::string& name, std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<PowerSmoothingIntf> pwrSmoothingIntf);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateReading(struct nsm_hardwarecircuitry_data* data);

  private:
    std::shared_ptr<PowerSmoothingIntf> pwrSmoothingIntf;
    std::string inventoryObjPath;
};

//  Query Admin overrides
class NsmPowerSmoothingAdminOverride : public NsmSensor
{
  public:
    NsmPowerSmoothingAdminOverride(
        std::string& name, std::string& type,
        std::shared_ptr<OemAdminProfileIntf> adminProfileIntf,
        std::string& inventoryObjPath);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateReading(struct nsm_admin_override_data* data);
    std::string getInventoryObjPath();

  private:
    std::shared_ptr<OemAdminProfileIntf> adminProfileIntf;
    std::string inventoryObjPath;
};

//  Get Preset Profile Information
class NsmPowerProfileCollection : public NsmSensor
{
  private:
    std::string inventoryObjPath;
    std::map<uint8_t, std::shared_ptr<OemPowerProfileIntf>>
        supportedPowerProfiles;
    std::shared_ptr<NsmDevice> device;

  public:
    NsmPowerProfileCollection(std::string& name, std::string& type,
                              std::string& inventoryObjPath,
                              std::shared_ptr<NsmDevice> device);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    std::shared_ptr<OemPowerProfileIntf>
        getSupportedProfileById(uint8_t profileId);
    bool hasProfileId(uint8_t profileId);
    void addSupportedProfile(uint8_t profileId,
                             std::shared_ptr<OemPowerProfileIntf> obj);
    void updateSupportedProfile(std::shared_ptr<OemPowerProfileIntf> obj,
                                nsm_preset_profile_data* data);
    std::string getProfilePathByProfileId(uint8_t profileId);
};

// Power Smoothing Control: Apply Admin Override and Activate a preset profile
class NsmCurrentPowerSmoothingProfile;
class NsmPowerSmoothingAction : public NsmObject, public ProfileActionAsyncIntf
{
  public:
    NsmPowerSmoothingAction(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<NsmCurrentPowerSmoothingProfile> currentProfile,
        std::shared_ptr<NsmDevice> device);

    // dbus method override for ProfileActionAsyncIntf
    sdbusplus::message::object_path
        activatePresetProfile(uint16_t profileID) override;
    // method to support post operation
    requester::Coroutine doActivatePresetProfile(
        std::shared_ptr<AsyncStatusIntf> statusInterface, uint16_t& profileID);
    requester::Coroutine
        requestActivatePresetProfile(AsyncOperationStatusType* status,
                                     uint16_t& profileID);

    // dbus method override for ProfileActionAsyncIntf
    sdbusplus::message::object_path applyAdminOverride() override;
    // method to support post operation
    requester::Coroutine
        doApplyAdminOverride(std::shared_ptr<AsyncStatusIntf> statusInterface);
    requester::Coroutine
        requestApplyAdminOverride(AsyncOperationStatusType* status);

  private:
    std::shared_ptr<NsmCurrentPowerSmoothingProfile> currentProfile;
    std::shared_ptr<NsmDevice> device;
    std::string inventoryObjPath;
};

//  Power Smoothing Control: Get Current Profile Information
class NsmCurrentPowerSmoothingProfile : public NsmSensor
{
  public:
    NsmCurrentPowerSmoothingProfile(
        std::string& name, std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<OemCurrentPowerProfileIntf> pwrSmoothingCurProfileIntf,
        std::shared_ptr<NsmPowerProfileCollection>
            pwrSmoothingSupportedCollection,
        std::shared_ptr<NsmPowerSmoothingAdminOverride> adminProfileSensor);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateReading(struct nsm_get_current_profile_data* data);
    std::string getProfilePath(uint8_t profileId);

  private:
    std::shared_ptr<OemCurrentPowerProfileIntf> pwrSmoothingCurProfileIntf;
    std::shared_ptr<NsmPowerProfileCollection>
        pwrSmoothingSupportedCollectionSensor;
    std::shared_ptr<NsmPowerSmoothingAdminOverride> adminProfileSensor;
    std::string inventoryObjPath;
};

} // namespace nsm
