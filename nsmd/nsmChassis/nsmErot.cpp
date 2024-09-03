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

#include "nsmErot.hpp"

#include "dBusAsyncUtils.hpp"
#include "nsmSecurityRBP.hpp"
#include "sensorManager.hpp"

#include <charconv>
#include <fstream>
#include <iostream>

namespace nsm
{

FirmwareSlot::FirmwareSlot(sdbusplus::bus::bus& bus, const std::string& name,
                           const std::vector<utils::Association>& _associations,
                           int slot, SlotIntf::FirmwareType fwType) :
    BuildType(bus, slotName(name, slot).c_str()),
    AssociationDefinitionsIntf(bus, slotName(name, slot).c_str()),
    SlotIntf(bus, slotName(name, slot).c_str()),
    StateIntf(bus, slotName(name, slot).c_str()),
    ExtendedVersionIntf(bus, slotName(name, slot).c_str()),
    VersionComparisonIntf(bus, slotName(name, slot).c_str()),
    SettingsIntf(bus, slotName(name, slot).c_str()),
    SecurityVersionIntf(bus, slotName(name, slot).c_str())
{
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : _associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associations(associations_list);
    slotId(slot);
    type(fwType);
}

void FirmwareSlot::updateActiveSlotAssociation()
{
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations())
    {
        if (std::get<0>(association) == "software")
        {
            const auto backwardAssoc = isActive() ? "ActiveSlot"
                                                  : "InactiveSlot";
            associations_list.emplace_back(std::get<0>(association),
                                           backwardAssoc,
                                           std::get<2>(association));
        }
        else
        {
            associations_list.emplace_back(association);
        }
    }
    associations(associations_list);
}

void FirmwareSlot::update(
    const struct ::nsm_firmware_slot_info& info,
    const struct ::nsm_firmware_erot_state_info_hdr_resp& fq_resp_hdr)
{
    static constexpr FirmwareState stateTbl[] = {
        FirmwareState::Unknown,
        FirmwareState::Activated,
        FirmwareState::PendingActivation,
        FirmwareState::Staged,
        FirmwareState::WriteInProgress,
        FirmwareState::Inactive,
        FirmwareState::FailedAuthentication};
    FirmwareBuildType btype = info.build_type == 0
                                  ? FirmwareBuildType::Development
                                  : FirmwareBuildType::Release;
    FirmwareState fstate = info.firmware_state <
                                   (sizeof(stateTbl) / sizeof(stateTbl[0]))
                               ? stateTbl[info.firmware_state]
                               : FirmwareState::Unknown;
    buildType(btype);
    state(fstate);
    slotId(info.slot_id);
    isActive(fq_resp_hdr.active_slot == info.slot_id);
    updateActiveSlotAssociation();
    extendedVersion(std::string(
        reinterpret_cast<const char*>(info.firmware_version_string)));
    firmwareComparisonNumber(info.version_comparison_stamp);
    writeProtected(info.write_protect_state);
    version(info.security_version_number);
}

NsmBuildTypeObject::NsmBuildTypeObject(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const uuid_t& uuid, int slot, int classification,
    int identifier, SlotIntf::FirmwareType fwType) :
    NsmSensor(name, type), objectPath(getName(name)), uuid(uuid),
    slotNum(slot % 2)
{
    lg2::info("BuildType: create object: {PATH}", "PATH", objectPath.c_str());
    static constexpr auto slotApFwBase = 0;
    static constexpr auto slotEcFwBase = numSlots;
    const auto slotBase = (fwType == SlotIntf::FirmwareType::AP)
                              ? (slotApFwBase)
                              : (slotEcFwBase);
    for (int slot = 0; slot < numSlots; ++slot)
    {
        fwSlots[slot] = std::make_unique<FirmwareSlot>(
            bus, objectPath, associations, slot + slotBase, fwType);
    }
    nsmRequest = {.component_classification = uint16_t(classification),
                  .component_identifier = uint16_t(identifier),
                  .component_classification_index = 0};
}

std::optional<std::vector<uint8_t>>
    NsmBuildTypeObject::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_get_erot_state_info_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &nsmRequest, requestMsg);
    if (rc)
    {
        lg2::error(
            "encode_nsm_query_get_erot_state_parameters_req(GET_NSM_BUILD_TYPE) failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmBuildTypeObject::handleResponseMsg(const nsm_msg* responseMsg,
                                              size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    struct ::nsm_firmware_erot_state_info_resp erotInfo;
    auto rc = decode_nsm_query_get_erot_state_parameters_resp(
        responseMsg, responseLen, &cc, &reasonCode, &erotInfo);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            ":decode_nsm_query_get_erot_state_parameters_resp(GET_NSM_BUILD_TYPE) rc={RC} cc={CC} reasonCode={RSC}",
            "RC", rc, "CC", cc, "RSC", reasonCode);
        return rc;
    }
    if (erotInfo.fq_resp_hdr.firmware_slot_count < slotNum)
    {
        lg2::error(
            "GET_NSM_BUILD_TYPE sc={SlOT_COUNT}, but expected slots not less than {SLOTS}",
            "SC", erotInfo.fq_resp_hdr.firmware_slot_count, "SLOTS", slotNum);
        free(erotInfo.slot_info);
        return NSM_SW_ERROR;
    }
    for (int i = 0; i < erotInfo.fq_resp_hdr.firmware_slot_count; i++)
    {
        fwSlots[i]->update(erotInfo.slot_info[i], erotInfo.fq_resp_hdr);
    }
    free(erotInfo.slot_info);
    return cc;
}

static int extractNumber(const std::string& str)
{
    auto it = str.rbegin();
    while (it != str.rend() && std::isdigit(*it))
    {
        ++it;
    }
    auto num_start = it.base();
    std::string number_str(num_start, str.end());
    int number = 0;
    auto [ptr, ec] = std::from_chars(
        number_str.data(), number_str.data() + number_str.size(), number);
    if (ec != std::errc())
    {
        return -1;
    }
    return number;
}

requester::Coroutine nsmErotCreateSensors(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath)
{
    auto erotSlotInterface = "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    if (type == "NSM_Chassis")
    {
        auto name = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());

        if (name.find("RoT_") == std::string::npos)
        {
            // coverity[missing_return]
            co_return NSM_SUCCESS;
        }
        std::shared_ptr<NsmBuildTypeObject> firmwareTypeAp = nullptr;
        std::shared_ptr<NsmBuildTypeObject> firmwareTypeEc = nullptr;
        auto slotCount = co_await utils::coGetDbusProperty<uint64_t>(
            objPath.c_str(), "SlotCount", interface.c_str());
        std::shared_ptr<NsmMinSecVersionObject> minSecVersionEc = nullptr;
        std::shared_ptr<NsmMinSecVersionObject> minSecVersionAp = nullptr;
        auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());
        auto device = manager.getNsmDevice(uuid);
        auto& bus = utils::DBusHandler::getBus();
        std::shared_ptr<ProgressIntf> rotProgressIntf = nullptr;
        for (size_t slotIndex = 1; slotIndex <= slotCount; slotIndex++)
        {
            auto slotPath = std::string(chassisInventoryBasePath) + "/" + name +
                            "/Slot" + std::to_string(slotIndex);
            auto slotName = co_await utils::coGetDbusProperty<std::string>(
                slotPath.c_str(), "Name", erotSlotInterface);
            auto classification = co_await utils::coGetDbusProperty<uint64_t>(
                slotPath.c_str(), "ComponentClassification", erotSlotInterface);
            auto identifier = co_await utils::coGetDbusProperty<uint64_t>(
                slotPath.c_str(), "ComponentIdentifier", erotSlotInterface);
            auto index = co_await utils::coGetDbusProperty<uint64_t>(
                slotPath.c_str(), "ComponentIndex", erotSlotInterface);
            auto fwType = co_await utils::coGetDbusProperty<std::string>(
                slotPath.c_str(), "FirmwareType", erotSlotInterface);
            auto associations = utils::getAssociations(
                slotPath, std::string(erotSlotInterface) + ".Associations");
            auto chassisName = co_await utils::coGetDbusProperty<std::string>(
                slotPath.c_str(), "ChassisName", erotSlotInterface);
            std::shared_ptr<ProgressIntf> ecProgressIntf = nullptr;
            std::shared_ptr<ProgressIntf> apProgressIntf = nullptr;
            if (fwType == "AP")
            {
                if (firmwareTypeAp == nullptr)
                {
                    firmwareTypeAp = std::make_shared<NsmBuildTypeObject>(
                        bus, name, associations, type, uuid,
                        extractNumber(slotName), classification, identifier,
                        SlotIntf::FirmwareType::AP);
                    device->addSensor(firmwareTypeAp, false);
                }
                if (minSecVersionAp == nullptr)
                {
                    auto progressPath = std::string(chassisInventoryBasePath) +
                                        "/" + chassisName;
                    apProgressIntf = std::make_shared<ProgressIntf>(
                        bus, progressPath.c_str());
                    if (chassisName == name)
                    {
                        rotProgressIntf = apProgressIntf;
                    }
                    minSecVersionAp = std::make_shared<NsmMinSecVersionObject>(
                        bus, chassisName, type, uuid, classification,
                        identifier, static_cast<uint8_t>(index),
                        apProgressIntf);
                    device->addSensor(minSecVersionAp, false);
                }
            }
            else // EC
            {
                if (firmwareTypeEc == nullptr)
                {
                    firmwareTypeEc = std::make_shared<NsmBuildTypeObject>(
                        bus, name, associations, type, uuid,
                        extractNumber(slotName), classification, identifier,
                        SlotIntf::FirmwareType::EC);
                    device->addSensor(firmwareTypeEc, false);
                }
                if (minSecVersionEc == nullptr)
                {
                    auto progressPath = std::string(chassisInventoryBasePath) +
                                        "/" + chassisName;
                    ecProgressIntf = std::make_shared<ProgressIntf>(
                        bus, progressPath.c_str());
                    if (chassisName == name)
                    {
                        rotProgressIntf = ecProgressIntf;
                    }
                    minSecVersionEc = std::make_shared<NsmMinSecVersionObject>(
                        bus, chassisName, type, uuid, classification,
                        identifier, static_cast<uint8_t>(index),
                        ecProgressIntf);
                    device->addSensor(minSecVersionEc, false);
                }
            }
        }
        auto securityCfg = std::make_shared<NsmSecurityCfgObject>(
            bus, name, type, uuid, rotProgressIntf);
        device->addSensor(securityCfg, false);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

std::vector<std::string> erotInterfaces{
    "xyz.openbmc_project.Configuration.NSM_Chassis"};

REGISTER_NSM_CREATION_FUNCTION(nsmErotCreateSensors, erotInterfaces)

} // namespace nsm
