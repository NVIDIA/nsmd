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
#include "nsmSensor.hpp"
#include "nsmWorkloadPowerProfileInfoIface.hpp"

#include <sdbusplus/asio/object_server.hpp>

#include <cstdint>
#include <memory>

namespace nsm
{
// Enable/Disable PowerProfiles
class NsmWorkloadProfileInfoAsyncIntf : public ProfileInfoAsyncIntf
{
  public:
    NsmWorkloadProfileInfoAsyncIntf(sdbusplus::bus::bus& bus, const char* path,
                                    std::shared_ptr<NsmDevice> device);

    sdbusplus::message::object_path
        enablePresetProfile(std::vector<uint8_t> bytes) override;
    requester::Coroutine
        doEnablePresetProfile(std::shared_ptr<AsyncStatusIntf> statusInterface,
                              std::vector<uint8_t>& bytes);
    requester::Coroutine
        requestEnablePresetProfile(AsyncOperationStatusType* status,
                                   std::vector<uint8_t>& bytes);

    sdbusplus::message::object_path
        disablePresetProfile(std::vector<uint8_t> bytes) override;
    requester::Coroutine
        doDisablePresetProfile(std::shared_ptr<AsyncStatusIntf> statusInterface,
                               std::vector<uint8_t>& bytes);
    requester::Coroutine
        requestDisablePresetProfile(AsyncOperationStatusType* status,
                                    std::vector<uint8_t>& bytes);

  private:
    std::shared_ptr<NsmDevice> device;
    std::string inventoryObjPath;
};
// Enum for ProfileID Mapping
class NsmWorkLoadProfileEnum : public NsmObject
{
  public:
    // Enum to hold the values
    enum DynamicEnum
    {

    };

    // Constructor that initializes the map
    NsmWorkLoadProfileEnum(std::string& name, std::string& type,
                           const std::vector<std::string>& strings) :
        NsmObject(name, type)
    {
        for (size_t i = 0; i < strings.size(); ++i)
        {
            enumToStringMap[i] = strings[i];
            stringToEnumMap[strings[i]] = i;
        }
    }

    // Method to get string representation by integer value
    std::string toString(uint16_t enumValue) const
    {
        auto it = enumToStringMap.find(enumValue);
        if (it != enumToStringMap.end())
        {
            return it->second;
        }
        return "Unknown";
    }

    // Method to get integer value by string representation
    int toEnum(const std::string& str) const
    {
        auto it = stringToEnumMap.find(str);
        if (it != stringToEnumMap.end())
        {
            return it->second;
        }
        return -1; // Indicates the string is not a valid enum
    }

  private:
    std::map<int, std::string> enumToStringMap;
    std::map<std::string, int> stringToEnumMap;
};

//  Workload Power Profile: Get Preset Profile Information
//  collection of all workload power profiles across pages
class NsmWorkloadPowerProfileCollection : public NsmObject
{
  private:
    std::string inventoryObjPath;
    std::map<uint8_t, std::shared_ptr<OemWorkLoadPowerProfileIntf>>
        supportedPowerProfiles;
    std::shared_ptr<NsmDevice> device;

  public:
    NsmWorkloadPowerProfileCollection(std::string& name, std::string& type,
                                      std::string& inventoryObjPath,
                                      std::shared_ptr<NsmDevice> device);

    std::shared_ptr<OemWorkLoadPowerProfileIntf>
        getSupportedProfileById(uint16_t profileId);
    bool hasProfileId(uint16_t profileId);
    void addSupportedProfile(uint8_t profileId,
                             std::shared_ptr<OemWorkLoadPowerProfileIntf> obj);
    void
        updateSupportedProfile(std::shared_ptr<OemWorkLoadPowerProfileIntf> obj,
                               nsm_workload_power_profile_data* data);
};

//  Workload Power Profile: Get Preset Profile Information
//  collection of all pages
class NsmWorkloadPowerProfilePage;
class NsmWorkloadPowerProfilePageCollection : public NsmObject
{
  private:
    std::string inventoryObjPath;
    std::map<uint16_t, std::shared_ptr<NsmWorkloadPowerProfilePage>>
        supportedPages;
    std::shared_ptr<NsmDevice> device;

  public:
    NsmWorkloadPowerProfilePageCollection(std::string& name, std::string& type,
                                          std::string& inventoryObjPath,
                                          std::shared_ptr<NsmDevice> device);

    std::shared_ptr<NsmWorkloadPowerProfilePage> getPageById(uint16_t pageId);
    bool hasPageId(uint16_t pageId);
    void addPage(uint16_t pageId,
                 std::shared_ptr<NsmWorkloadPowerProfilePage> obj);
};

//  Workload Power Profile: Get Preset Profile Information per page
class NsmWorkloadPowerProfilePage : public NsmSensor
{
  private:
    std::string inventoryObjPath;
    std::shared_ptr<NsmDevice> device;
    // each page contains multiple profiles so profileCollection reference used
    // to add new profiles
    std::shared_ptr<NsmWorkloadPowerProfileCollection> profileCollection;
    // This PowerProfilePage adds next page if its supported
    std::shared_ptr<NsmWorkloadPowerProfilePageCollection> pageCollection;
    std::shared_ptr<NsmWorkLoadProfileEnum> profileMapper;
    const uint16_t pageId;

  public:
    NsmWorkloadPowerProfilePage(
        std::string& name, std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<NsmDevice> device,
        std::shared_ptr<NsmWorkloadPowerProfileCollection> profileCollection,
        std::shared_ptr<NsmWorkloadPowerProfilePageCollection> pageCollection,
        std::shared_ptr<NsmWorkLoadProfileEnum> profileMapper, uint16_t pageId);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
};

//  Workload Power Profile Control: Get Pre-set profile Status Information

class NsmWorkLoadProfileStatus : public NsmSensor
{
  public:
    NsmWorkLoadProfileStatus(
        std::string& name, std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<OemProfileInfoIntf> profileStatusInfo,
        std::shared_ptr<NsmWorkloadProfileInfoAsyncIntf> profileInfoAsync);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateReading(struct workload_power_profile_status* data);

  private:
    std::string inventoryObjPath;
    std::shared_ptr<OemProfileInfoIntf> profileStatusInfo;
    std::shared_ptr<NsmWorkloadProfileInfoAsyncIntf> profileInfoAsync;
};

} // namespace nsm
