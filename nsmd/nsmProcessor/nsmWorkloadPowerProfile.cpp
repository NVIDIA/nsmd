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
#include "config.h"

#include "nsmWorkloadPowerProfile.hpp"

#include "utils.hpp"

#include <stdint.h>

#include <cstdint>

namespace nsm
{

NsmWorkloadProfileInfoAsyncIntf::NsmWorkloadProfileInfoAsyncIntf(
    sdbusplus::bus::bus& bus, const char* path,
    std::shared_ptr<NsmDevice> device) :
    ProfileInfoAsyncIntf(bus, path),
    device(device)
{}

requester::Coroutine
    NsmWorkloadProfileInfoAsyncIntf::requestEnablePresetProfile(
        AsyncOperationStatusType* status, std::vector<uint8_t>& bytes)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_enable_workload_power_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    // first argument instanceid=0 is irrelevant
    bitfield256_t profile_mask = utils::bitMapToBitfield256_t(bytes);
    std::string mask = utils::vectorTo256BitHexString(bytes);
    auto rc = encode_enable_workload_power_profile_req(0, &profile_mask,
                                                       requestMsg);
    std::string msg = utils::requestMsgToHexString(request);

    if (rc)
    {
        lg2::error(
            "requestEnablePresetProfile  encode_enable_workload_power_profile_req failed. eid={EID}, rc={RC}, msg={MSG}",
            "EID", eid, "RC", rc, "MSG", msg);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        lg2::error(
            "requestEnablePresetProfile SendRecvNsmMsgSync failed for for eid = {EID} rc = {RC}, msg={MSG}",
            "EID", eid, "RC", rc_, "MSG", msg);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_enable_workload_power_profile_resp(
        responseMsg.get(), responseLen, &cc, &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info(
            "requestEnablePresetProfile for EID: {EID} completed, msg = {MSG}",
            "EID", eid, "MSG", msg);
    }
    else
    {
        lg2::error(
            "requestEnablePresetProfile decode_enable_workload_power_profile_resp failed.eid ={EID},CC = {CC} reasoncode = {RC},RC = {A}, msg={MSG} ",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc, "MSG", msg);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmWorkloadProfileInfoAsyncIntf::doEnablePresetProfile(
    std::shared_ptr<AsyncStatusIntf> statusInterface,
    std::vector<uint8_t>& bytes)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    auto rc_ = co_await requestEnablePresetProfile(&status, bytes);

    statusInterface->status(status);

    co_return rc_;
}

sdbusplus::message::object_path
    NsmWorkloadProfileInfoAsyncIntf::enablePresetProfile(
        std::vector<uint8_t> bytes)
{
    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        lg2::error(
            "NsmWorkloadProfileInfoAsyncIntf::enablePresetProfile failed. No available result Object to allocate for the Post request.");
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }
    if (bytes.size() != 32)
    {
        lg2::error("Invalid Profile Mask");
        for (auto byte : bytes)
        {
            lg2::error("{A}", "A", byte);
        }
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    doEnablePresetProfile(statusInterface, bytes).detach();

    return objectPath;
}

requester::Coroutine
    NsmWorkloadProfileInfoAsyncIntf::requestDisablePresetProfile(
        AsyncOperationStatusType* status, std::vector<uint8_t>& bytes)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_disable_workload_power_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    // first argument instanceid=0 is irrelevant
    bitfield256_t profile_mask = utils::bitMapToBitfield256_t(bytes);
    std::string mask = utils::vectorTo256BitHexString(bytes);
    auto rc = encode_disable_workload_power_profile_req(0, &profile_mask,
                                                        requestMsg);
    std::string msg = utils::requestMsgToHexString(request);

    if (rc)
    {
        lg2::error(
            "requestDisablePresetProfile  encode_disable_workload_power_profile_req failed. eid={EID}, rc={RC}, msg={MSG}",
            "EID", eid, "RC", rc, "MSG", msg);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        lg2::error(
            "requestDisablePresetProfile SendRecvNsmMsgSync failed for for eid = {EID} rc = {RC}, msg={MSG}",
            "EID", eid, "RC", rc_, "MSG", msg);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_disable_workload_power_profile_resp(
        responseMsg.get(), responseLen, &cc, &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info(
            "requestDisablePresetProfile for EID: {EID} completed, msg = {MSG}",
            "EID", eid, "MSG", msg);
    }
    else
    {
        lg2::error(
            "requestDisablePresetProfile decode_disable_workload_power_profile_resp failed.eid ={EID},CC = {CC} reasoncode = {RC},RC = {A}, msg={MSG} ",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc, "MSG", msg);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmWorkloadProfileInfoAsyncIntf::doDisablePresetProfile(
    std::shared_ptr<AsyncStatusIntf> statusInterface,
    std::vector<uint8_t>& bytes)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    auto rc_ = co_await requestDisablePresetProfile(&status, bytes);

    statusInterface->status(status);

    co_return rc_;
}

sdbusplus::message::object_path
    NsmWorkloadProfileInfoAsyncIntf::disablePresetProfile(
        std::vector<uint8_t> bytes)
{
    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        lg2::error(
            "NsmWorkloadProfileInfoAsyncIntf::disablePresetProfile failed. No available result Object to allocate for the Post request.");
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    if (bytes.size() != 32)
    {
        lg2::error("Invalid Profile Mask");
        for (auto byte : bytes)
        {
            lg2::error("{A}", "A", byte);
        }
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    doDisablePresetProfile(statusInterface, bytes).detach();

    return objectPath;
}

//  Power Smoothing Control: Get Current Profile Information
NsmWorkLoadProfileStatus::NsmWorkLoadProfileStatus(
    std::string& name, std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<OemProfileInfoIntf> profileStatusInfo,
    std::shared_ptr<NsmWorkloadProfileInfoAsyncIntf> profileInfoAsync) :
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath), profileStatusInfo(profileStatusInfo),
    profileInfoAsync(profileInfoAsync)

{}

std::optional<std::vector<uint8_t>>
    NsmWorkLoadProfileStatus::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_status_req(instanceId,
                                                           requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_workload_power_profile_status_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmWorkLoadProfileStatus::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    workload_power_profile_status data{};

    auto rc = decode_get_workload_power_profile_status_resp(
        responseMsg, responseLen, &cc, &reason_code, &dataSize, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(&data);
        clearErrorBitMap("decode_get_workload_power_profile_status_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_workload_power_profile_status_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

void NsmWorkLoadProfileStatus::updateReading(
    struct workload_power_profile_status* data)
{
    if (data == nullptr)
    {
        lg2::debug("workload_power_profile_status data is null");
        return;
    }

    // Update values on iface
    profileStatusInfo->ProfileInfoIntf::supportedProfileMask(
        utils::bitfield256_tToBitArray(data->supported_profile_mask));
    profileStatusInfo->ProfileInfoIntf::requestedProfileMask(
        utils::bitfield256_tToBitArray(data->requested_profile_maks));
    profileStatusInfo->ProfileInfoIntf::enforcedProfileMask(
        utils::bitfield256_tToBitArray(data->enforced_profile_mask));
}

//  Get Preset Profile Information
NsmWorkloadPowerProfileCollection::NsmWorkloadPowerProfileCollection(
    std::string& name, std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<NsmDevice> device) :
    NsmObject(name, type),
    inventoryObjPath(inventoryObjPath), device(device)
{}

bool NsmWorkloadPowerProfileCollection::hasProfileId(uint16_t profileId)

{
    return supportedPowerProfiles.find(profileId) !=
           supportedPowerProfiles.end();
}

std::shared_ptr<OemWorkLoadPowerProfileIntf>
    NsmWorkloadPowerProfileCollection::getSupportedProfileById(
        uint16_t profileId)
{
    auto it = supportedPowerProfiles.find(profileId);
    if (it != supportedPowerProfiles.end())
    {
        return it->second;
    }
    lg2::debug("getSupportedProfileById: ProfileId not found: {ID}", "ID",
               profileId);
    throw std::out_of_range("profileId not found in map");
}

void NsmWorkloadPowerProfileCollection::addSupportedProfile(
    uint16_t profileId, std::shared_ptr<OemWorkLoadPowerProfileIntf> obj)

{
    supportedPowerProfiles[profileId] = obj;
}

void NsmWorkloadPowerProfileCollection::updateSupportedProfile(
    std::shared_ptr<OemWorkLoadPowerProfileIntf> obj,
    nsm_workload_power_profile_data* data)

{
    if (obj)
    {
        obj->WorkLoadPowerProfileIntf::profileId(data->profile_id);
        obj->WorkLoadPowerProfileIntf::priority(data->priority);
        obj->WorkLoadPowerProfileIntf::conflictMask(
            utils::bitfield256_tToBitArray(data->conflict_mask));
    }
}

//  Get Preset Profile Information
NsmWorkloadPowerProfilePageCollection::NsmWorkloadPowerProfilePageCollection(
    std::string& name, std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<NsmDevice> device) :
    NsmObject(name, type),
    inventoryObjPath(inventoryObjPath), device(device)
{}

std::shared_ptr<NsmWorkloadPowerProfilePage>
    NsmWorkloadPowerProfilePageCollection::getPageById(uint16_t pageId)
{
    auto it = supportedPages.find(pageId);
    if (it != supportedPages.end())
    {
        return it->second;
    }
    return nullptr;
}

bool NsmWorkloadPowerProfilePageCollection::hasPageId(uint16_t pageId)

{
    return supportedPages.find(pageId) != supportedPages.end();
}

void NsmWorkloadPowerProfilePageCollection::addPage(
    uint16_t pageId, std::shared_ptr<NsmWorkloadPowerProfilePage> obj)

{
    if (!hasPageId(pageId))
    {
        supportedPages[pageId] = obj;
        bool priority = false;
        device->addSensor(obj, priority);
    }
    else
    {
        lg2::info(
            "NsmWorkloadPowerProfilePageCollection addPage, page already exists with Page Id= {PAGE}",
            "PAGE", pageId);
    }
}

//  Get Preset Profile Information per page
NsmWorkloadPowerProfilePage::NsmWorkloadPowerProfilePage(
    std::string& name, std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<NsmDevice> device,
    std::shared_ptr<NsmWorkloadPowerProfileCollection> profileCollection,
    std::shared_ptr<NsmWorkloadPowerProfilePageCollection> pageCollection,
    std::shared_ptr<NsmWorkLoadProfileEnum> profileMapper, uint16_t pageId) :
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath), device(device),
    profileCollection(profileCollection), pageCollection(pageCollection),
    profileMapper(profileMapper), pageId(pageId)
{}

std::optional<std::vector<uint8_t>>
    NsmWorkloadPowerProfilePage::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_workload_power_profile_info_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_info_req(instanceId, pageId,
                                                         requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_workload_power_profile_info_req for page {PAGE} failed. "
            "eid={EID} rc={RC}",
            "PAGE", pageId, "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmWorkloadPowerProfilePage::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    nsm_all_workload_power_profile_meta_data data{};
    uint8_t numberOfprofiles = 0;
    uint16_t nextIdentifier = 0;

    auto rc = decode_get_workload_power_profile_info_metadata_resp(
        responseMsg, responseLen, &cc, &reason_code, &data, &numberOfprofiles);
    nextIdentifier = data.next_identifier;
    lg2::debug("number of profiles = {PID}", "PID", numberOfprofiles);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        uint16_t firstProfileIndex = pageId * numberOfprofiles;
        uint16_t lastProfileIndex = (pageId + 1) * numberOfprofiles;
        lg2::debug("firstProfileIndex = {PID}", "PID", firstProfileIndex);
        lg2::debug("lastProfileIndex = {PID}", "PID", lastProfileIndex);
        uint8_t offset = 0;
        for (int itertor = firstProfileIndex; itertor < lastProfileIndex;
             itertor++)
        {
            nsm_workload_power_profile_data profileData{};
            decode_get_workload_power_profile_info_data_resp(
                responseMsg, responseLen, &cc, &reason_code, numberOfprofiles,
                offset, &profileData);
            uint16_t profileId = profileData.profile_id;
            lg2::debug("profileId = {PID}", "PID", profileId);
            if (!profileCollection->hasProfileId(profileId))
            {
                auto profileName = profileMapper->toString(profileId);
                auto powerProfile =
                    std::make_shared<OemWorkLoadPowerProfileIntf>(
                        utils::DBusHandler::getBus(), inventoryObjPath,
                        profileId, profileName, device);
                profileCollection->addSupportedProfile(profileId, powerProfile);
            }
            profileCollection->updateSupportedProfile(
                profileCollection->getSupportedProfileById(profileId),
                &profileData);
            offset++;
        }

        // Check if next page is supported or not
        if (nextIdentifier > 0)
        {
            uint16_t nextPageId = nextIdentifier;
            // check if its a new page thats not discovered yet
            if (!pageCollection->hasPageId(nextPageId))
            {
                std::string name = getName();
                std::string type = getType();
                auto page = std::make_shared<NsmWorkloadPowerProfilePage>(
                    name, type, inventoryObjPath, device, profileCollection,
                    pageCollection, profileMapper, nextPageId);
                pageCollection->addPage(nextPageId, page);
            }
        }
        clearErrorBitMap(
            "decode_get_workload_power_profile_info_metadata_resp");
    }
    else
    {
        logHandleResponseMsg(
            "decode_get_workload_power_profile_info_metadata_resp", reason_code,
            cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

} // namespace nsm
