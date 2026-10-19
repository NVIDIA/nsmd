/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

#include "nsmSwitchPowerProfile.hpp"

#include "base.h"
#include "network-ports.h"

#include "nsmDevice.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace nsm
{

namespace
{
/** @brief Async status for a Set Power Profile the device did not accept. */
AsyncOperationStatusType setFailureStatus(int rc, uint8_t cc,
                                          uint16_t reasonCode)
{
    if (rc != NSM_SW_SUCCESS)
    {
        return AsyncOperationStatusType::WriteFailure;
    }
    if (reasonCode == ERR_NOT_SUPPORTED ||
        reasonCode == ERR_PROPERTY_NOT_SUPPORTED)
    {
        return AsyncOperationStatusType::UnsupportedRequest;
    }
    switch (cc)
    {
        case NSM_ERR_INVALID_DATA:
            return AsyncOperationStatusType::InvalidArgument;
        case NSM_ERR_UNSUPPORTED_COMMAND_CODE:
        case NSM_ERR_UNSUPPORTED_MSG_TYPE:
            return AsyncOperationStatusType::UnsupportedRequest;
        case NSM_ERR_NOT_READY:
        case NSM_BUSY:
            return AsyncOperationStatusType::Unavailable;
        default:
            return AsyncOperationStatusType::WriteFailure;
    }
}
} // namespace

NsmSwitchPowerProfile::NsmSwitchPowerProfile(
    const std::string& name, const std::string& type,
    std::shared_ptr<PowerProfileServer> profileIntf) :
    NsmSensor(name, type), profileIntf(std::move(profileIntf))
{}

std::optional<std::vector<uint8_t>>
    NsmSwitchPowerProfile::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto rc = encode_get_power_profile_req(
        instanceId, reinterpret_cast<nsm_msg*>(request.data()));
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_power_profile_req failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

int NsmSwitchPowerProfile::decodeAndPublish(const nsm_msg* responseMsg,
                                            size_t responseLen, uint8_t& cc,
                                            uint16_t& reasonCode)
{
    uint8_t active = NSM_POWER_PROFILE_ID_HW_DEFAULT;
    uint8_t count = 0;
    nsm_power_profile_entry entries[NSM_POWER_PROFILE_MAX_COUNT]{};
    auto rc = decode_get_power_profile_resp(
        responseMsg, responseLen, &cc, &reasonCode, &active, &count, entries,
        NSM_POWER_PROFILE_MAX_COUNT);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        return rc;
    }

    // The hardware default is always selectable, so it is always listed first.
    // Firmware may or may not report it (with an empty name when no profiles
    // are provisioned).
    std::string defaultName(hardwareDefaultProfileName);
    std::vector<ProfileEntry> newProfiles;
    std::set<uint8_t> newIds{NSM_POWER_PROFILE_ID_HW_DEFAULT};
    std::string activeName;
    for (uint8_t i = 0; i < count; ++i)
    {
        const auto& entry = entries[i];
        std::string profileName(entry.profile_name);
        if (entry.profile_id == NSM_POWER_PROFILE_ID_HW_DEFAULT)
        {
            if (!profileName.empty())
            {
                defaultName = std::move(profileName);
            }
            continue;
        }
        if (entry.profile_id == active)
        {
            activeName = profileName;
        }
        newProfiles.emplace_back(entry.profile_id, std::move(profileName),
                                 entry.profile_id == active);
        newIds.insert(entry.profile_id);
    }
    if (active == NSM_POWER_PROFILE_ID_HW_DEFAULT)
    {
        activeName = defaultName;
    }
    newProfiles.emplace(newProfiles.begin(), NSM_POWER_PROFILE_ID_HW_DEFAULT,
                        std::move(defaultName),
                        active == NSM_POWER_PROFILE_ID_HW_DEFAULT);

    activeId = active;
    profiles = std::move(newProfiles);
    knownIds = std::move(newIds);

    if (profileIntf)
    {
        profileIntf->activeProfileId(activeId);
        profileIntf->activeProfileName(activeName);
        profileIntf->availableProfiles(profiles);
    }
    return NSM_SW_SUCCESS;
}

uint8_t NsmSwitchPowerProfile::handleResponseMsg(const nsm_msg* responseMsg,
                                                 size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decodeAndPublish(responseMsg, responseLen, cc, reasonCode);
    if ((rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS) &&
        shouldLog("NsmSwitchPowerProfile " + getName(), reasonCode, cc, rc))
    {
        lg2::error(
            "Get Power Profile failed. name={NAME} rc={RC} cc={CC} reasonCode={REASON}",
            "NAME", getName(), "RC", rc, "CC", cc, "REASON", reasonCode);
    }
    return cc ? cc : rc;
}

requester::Coroutine NsmSwitchPowerProfile::setActiveProfile(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const auto* requested = std::get_if<uint8_t>(&value);
    if (!requested)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    const uint8_t profileId = *requested;
    auto eid = device->getEid();

    if (!device->isOnline())
    {
        lg2::error("Set Power Profile: device offline. eid={EID}", "EID", eid);
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    if (profileId != NSM_POWER_PROFILE_ID_HW_DEFAULT &&
        !knownIds.contains(profileId))
    {
        lg2::error("Set Power Profile: unknown profile. eid={EID} profile={ID}",
                   "EID", eid, "ID", profileId);
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_DATA;
    }

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_profile_req));
    int rc = encode_set_power_profile_req(
        0, profileId, reinterpret_cast<nsm_msg*>(request.data()));
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_set_power_profile_req failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    rc = co_await device->postPatchIO(eid, request, responseMsg, responseLen);
    if (rc == NSM_SW_SUCCESS)
    {
        rc = responseMsg ? decode_set_power_profile_resp(
                               responseMsg.get(), responseLen, &cc, &reasonCode)
                         : NSM_SW_ERROR_NULL;
    }
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "Set Power Profile failed. eid={EID} profile={ID} rc={RC} cc={CC} reasonCode={REASON}",
            "EID", eid, "ID", profileId, "RC", rc, "CC", cc, "REASON",
            reasonCode);
        *status = setFailureStatus(rc, cc, reasonCode);
        co_return cc ? cc : rc;
    }

    lg2::info("Set Power Profile accepted. eid={EID} profile={ID}", "EID", eid,
              "ID", profileId);
    *status = AsyncOperationStatusType::Success;
    co_return NSM_SW_SUCCESS;
}

void createNsmSwitchPowerProfile(std::shared_ptr<NsmDevice> device,
                                 sdbusplus::bus_t& bus,
                                 const std::string& objPath,
                                 const std::string& name,
                                 const std::string& type)
{
    const auto dbusObjPath = objPath + name + "/Oem/Nvidia/PowerProfile";
    auto profileIntf = std::make_shared<PowerProfileIntf>(bus,
                                                          dbusObjPath.c_str());
    auto profile = std::make_shared<NsmSwitchPowerProfile>(
        name + "_PowerProfile", type, profileIntf);

    device->addSensor(profile, PollingType::RoundRobin);

    AsyncOperationManager::getInstance()
        ->getDispatcher(dbusObjPath)
        ->addAsyncSetOperation(
            std::string(PowerProfileServer::interface), "ActiveProfileId",
            AsyncSetOperationInfo{
                std::bind_front(&NsmSwitchPowerProfile::setActiveProfile,
                                profile),
                profile, device});
}

} // namespace nsm
