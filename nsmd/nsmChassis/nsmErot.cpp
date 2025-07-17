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
#include "nsmKeyMgmt.hpp"
#include "nsmSecurityRBP.hpp"
#include "sensorManager.hpp"

namespace nsm
{

NsmBuildTypeObject::NsmBuildTypeObject(const std::string& name,
                                       const std::string& type,
                                       const uuid_t& uuid, int classification,
                                       int identifier) :
    NsmSensor(name, type),
    uuid(uuid)
{
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
        lg2::debug(
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
    struct ::nsm_firmware_erot_state_info_resp erotInfo
    {};
    auto rc = decode_nsm_query_get_erot_state_parameters_resp(
        responseMsg, responseLen, &cc, &reasonCode, &erotInfo);
    LG2_ERROR_FLT(
        "decode_nsm_query_get_erot_state_parameters_resp(GET_NSM_BUILD_TYPE) failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    if (erotInfo.fq_resp_hdr.firmware_slot_count != fwSlotObjects.size())
    {
        lg2::debug(
            "GET_NSM_BUILD_TYPE sc={SlOT_COUNT}, but expected slots {SLOTS}",
            "SC", erotInfo.fq_resp_hdr.firmware_slot_count, "SLOTS",
            fwSlotObjects.size());
        if (erotInfo.fq_resp_hdr.firmware_slot_count > 0)
        {
            free(erotInfo.slot_info);
        }
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    for (int i = 0; i < erotInfo.fq_resp_hdr.firmware_slot_count; i++)
    {
        fwSlotObjects[i]->update(erotInfo.slot_info[i], erotInfo.fq_resp_hdr);
    }
    if (erotInfo.fq_resp_hdr.firmware_slot_count > 0)
    {
        free(erotInfo.slot_info);
    }
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

    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string type{};
    if (allCurrentIfaceProperties.count("Type"))
    {
        type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
    }
    if (type == "NSM_Chassis" || type == "NSM_ChassisRoT")
    {
        std::string name{};
        if (allCurrentIfaceProperties.count("Name"))
        {
            name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
        }
        auto path = std::string(chassisInventoryBasePath) + "/" + name;
        if (name.find("RoT_") == std::string::npos)
        {
            // coverity[missing_return]
            co_return NSM_SUCCESS;
        }
        uint64_t slotCount{};
        if (allCurrentIfaceProperties.count("SlotCount"))
        {
            slotCount =
                std::get<uint64_t>(allCurrentIfaceProperties.at("SlotCount"));
        }
        uuid_t uuid{};
        if (allCurrentIfaceProperties.count("UUID"))
        {
            uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
        }

        auto device = manager.getNsmDevice(uuid);
        auto& bus = utils::DBusHandler::getBus();

        std::shared_ptr<ProgressIntf> rotProgressIntf = nullptr;

        std::shared_ptr<NsmBuildTypeObject> apFirmwareType = nullptr;
        std::shared_ptr<ProgressIntf> apProgressIntf = nullptr;
        std::shared_ptr<NsmKeyMgmt> apKeyMgmt = nullptr;
        std::shared_ptr<NsmMinSecVersionObject> apMinSecVersion = nullptr;

        std::shared_ptr<NsmBuildTypeObject> ecFirmwareType = nullptr;
        std::shared_ptr<ProgressIntf> ecProgressIntf = nullptr;
        std::shared_ptr<NsmKeyMgmt> ecKeyMgmt = nullptr;
        std::shared_ptr<NsmMinSecVersionObject> ecMinSecVersion = nullptr;

        for (size_t slotIndex = 1; slotIndex <= slotCount; slotIndex++)
        {
            auto slotPath = path + "/Slot" + std::to_string(slotIndex);
            auto allSlotIfaceProperties = co_await utils::coGetAllDbusProperty(
                utils::entityManagerServiceStr, slotPath.c_str(),
                erotSlotInterface);

            std::string slotName{};
            if (allSlotIfaceProperties.count("Name"))
            {
                slotName =
                    std::get<std::string>(allSlotIfaceProperties.at("Name"));
            }
            auto classification =
                utils::DBusHandler().getDbusProperty<uint64_t>(
                    slotPath.c_str(), "ComponentClassification",
                    erotSlotInterface);
            uint64_t identifier{};
            if (allSlotIfaceProperties.count("ComponentIdentifier"))
            {
                identifier = std::get<uint64_t>(
                    allSlotIfaceProperties.at("ComponentIdentifier"));
            }
            uint64_t index{};
            if (allSlotIfaceProperties.count("ComponentIndex"))
            {
                index = std::get<uint64_t>(
                    allSlotIfaceProperties.at("ComponentIndex"));
            }
            std::string fwType{};
            if (allSlotIfaceProperties.count("FirmwareType"))
            {
                fwType = std::get<std::string>(
                    allSlotIfaceProperties.at("FirmwareType"));
            }
            auto associations = utils::getAssociations(
                slotPath, std::string(erotSlotInterface) + ".Associations");
            std::string chassisName{};
            if (allSlotIfaceProperties.count("ChassisName"))
            {
                chassisName = std::get<std::string>(
                    allSlotIfaceProperties.at("ChassisName"));
            }

            if (fwType == "AP")
            {
                auto slotObject = std::make_shared<NsmFirmwareSlot>(
                    bus, path, associations, extractNumber(slotName),
                    SlotIntf::FirmwareType::AP);
                if (apFirmwareType == nullptr)
                {
                    apFirmwareType = std::make_shared<NsmBuildTypeObject>(
                        name, type, uuid, classification, identifier);
                }
                apFirmwareType->addSlotObject(slotObject);
                if (apProgressIntf == nullptr)
                {
                    auto progressPath = std::string(chassisInventoryBasePath) +
                                        "/" + chassisName;
                    apProgressIntf = std::make_shared<ProgressIntf>(
                        bus, progressPath.c_str());
                }
                if (apKeyMgmt == nullptr)
                {
                    apKeyMgmt = std::make_shared<NsmKeyMgmt>(
                        bus, chassisName, type, uuid, apProgressIntf,
                        classification, identifier,
                        static_cast<uint8_t>(index));
                }
                apKeyMgmt->addSlotObject(slotObject);
                if (apMinSecVersion == nullptr)
                {
                    apMinSecVersion = std::make_shared<NsmMinSecVersionObject>(
                        bus, chassisName, type, uuid, classification,
                        identifier, static_cast<uint8_t>(index),
                        apProgressIntf);
                    device->addSensor(apMinSecVersion, false);
                }
                if (chassisName == name)
                {
                    rotProgressIntf = apProgressIntf;
                }
            }
            else // EC
            {
                auto slotObject = std::make_shared<NsmFirmwareSlot>(
                    bus, path, associations, extractNumber(slotName),
                    SlotIntf::FirmwareType::EC);
                if (ecFirmwareType == nullptr)
                {
                    ecFirmwareType = std::make_shared<NsmBuildTypeObject>(
                        name, type, uuid, classification, identifier);
                }
                ecFirmwareType->addSlotObject(slotObject);
                if (ecProgressIntf == nullptr)
                {
                    auto progressPath = std::string(chassisInventoryBasePath) +
                                        "/" + chassisName;
                    ecProgressIntf = std::make_shared<ProgressIntf>(
                        bus, progressPath.c_str());
                }
                if (ecKeyMgmt == nullptr)
                {
                    ecKeyMgmt = std::make_shared<NsmKeyMgmt>(
                        bus, chassisName, type, uuid, ecProgressIntf,
                        classification, identifier,
                        static_cast<uint8_t>(index));
                }
                ecKeyMgmt->addSlotObject(slotObject);
                if (ecMinSecVersion == nullptr)
                {
                    ecMinSecVersion = std::make_shared<NsmMinSecVersionObject>(
                        bus, chassisName, type, uuid, classification,
                        identifier, static_cast<uint8_t>(index),
                        ecProgressIntf);
                    device->addSensor(ecMinSecVersion, false);
                }
                if (chassisName == name)
                {
                    rotProgressIntf = ecProgressIntf;
                }
            }
        }
        if (apFirmwareType)
        {
            device->addSensor(apFirmwareType, false);
        }
        if (apKeyMgmt)
        {
            device->addSensor(apKeyMgmt, false);
        }
        if (ecFirmwareType)
        {
            device->addSensor(ecFirmwareType, false);
        }
        if (ecKeyMgmt)
        {
            device->addSensor(ecKeyMgmt, false);
        }
        if (rotProgressIntf == nullptr)
        {
            // IRoT does not have security and key management properties,
            // progress interface is not created while parsing slot properties
            auto progressPath = std::string(chassisInventoryBasePath) + "/" +
                                name;
            rotProgressIntf =
                std::make_shared<ProgressIntf>(bus, progressPath.c_str());
        }
        auto securityCfg = std::make_shared<NsmSecurityCfgObject>(
            bus, name, type, uuid, rotProgressIntf);
        device->addSensor(securityCfg, false);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

std::vector<std::string> erotInterfaces{
    "xyz.openbmc_project.Configuration.NSM_Chassis",
    "xyz.openbmc_project.Configuration.NSM_ChassisRoT"};

REGISTER_NSM_CREATION_FUNCTION(nsmErotCreateSensors, erotInterfaces)

} // namespace nsm
