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
    struct ::nsm_firmware_erot_state_info_resp erotInfo;
    auto rc = decode_nsm_query_get_erot_state_parameters_resp(
        responseMsg, responseLen, &cc, &reasonCode, &erotInfo);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        clearErrorBitMap(
            "decode_nsm_query_get_erot_state_parameters_resp(GET_NSM_BUILD_TYPE)");
    }
    else
    {
        logHandleResponseMsg(
            "decode_nsm_query_get_erot_state_parameters_resp(GET_NSM_BUILD_TYPE)",
            reasonCode, cc, rc);
    }
    if (erotInfo.fq_resp_hdr.firmware_slot_count < fwSlotObjects.size())
    {
        lg2::error(
            "GET_NSM_BUILD_TYPE sc={SlOT_COUNT}, but expected slots not less than {SLOTS}",
            "SC", erotInfo.fq_resp_hdr.firmware_slot_count, "SLOTS",
            fwSlotObjects.size());
        free(erotInfo.slot_info);
        return NSM_SW_ERROR;
    }
    for (int i = 0; i < erotInfo.fq_resp_hdr.firmware_slot_count; i++)
    {
        fwSlotObjects[i]->update(erotInfo.slot_info[i], erotInfo.fq_resp_hdr);
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
        auto path = std::string(chassisInventoryBasePath) + "/" + name;
        if (name.find("RoT_") == std::string::npos)
        {
            // coverity[missing_return]
            co_return NSM_SUCCESS;
        }
        auto slotCount = co_await utils::coGetDbusProperty<uint64_t>(
            objPath.c_str(), "SlotCount", interface.c_str());
        auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());
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
            auto slotName = co_await utils::coGetDbusProperty<std::string>(
                slotPath.c_str(), "Name", erotSlotInterface);
            auto classification =
                utils::DBusHandler().getDbusProperty<uint64_t>(
                    slotPath.c_str(), "ComponentClassification",
                    erotSlotInterface);
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

            if (fwType == "AP")
            {
                auto slotObject = std::make_shared<NsmFirmwareSlot>(
                    bus, path, associations, extractNumber(slotName),
                    SlotIntf::FirmwareType::AP);
                if (apFirmwareType == nullptr)
                {
                    apFirmwareType = std::make_shared<NsmBuildTypeObject>(
                        name, type, uuid, classification, identifier);
                    device->addSensor(apFirmwareType, false);
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
                    device->addSensor(apKeyMgmt, false);
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
                    device->addSensor(ecFirmwareType, false);
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
                    device->addSensor(ecKeyMgmt, false);
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
    "xyz.openbmc_project.Configuration.NSM_Chassis"};

REGISTER_NSM_CREATION_FUNCTION(nsmErotCreateSensors, erotInterfaces)

} // namespace nsm
