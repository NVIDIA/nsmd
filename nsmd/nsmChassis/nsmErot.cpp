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

#include "asyncOperationManager.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmApSkuId.hpp"
#include "nsmKeyMgmt.hpp"
#include "nsmRoTProperty.hpp"
#include "nsmSecurityRBP.hpp"
#include "nsmUpdateApSkuId.hpp"
#include "sensorManager.hpp"

#include <algorithm>

namespace nsm
{

NsmBuildTypeObject::NsmBuildTypeObject(const std::string& name,
                                       const std::string& type,
                                       const uuid_t& uuid, int classification,
                                       int identifier) :
    NsmSensor(name, type), uuid(uuid)
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
    struct ::nsm_firmware_erot_state_info_resp erotInfo{};
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

requester::Coroutine parseSlots(const std::string& path, uint64_t slotCount,
                                const std::string& rotSlotInterface,
                                std::vector<SlotInfo>& slots)
{
    for (size_t slotIndex = 1; slotIndex <= slotCount; slotIndex++)
    {
        auto slotPath = path + "/Slot" + std::to_string(slotIndex);
        auto allSlotIfaceProperties = co_await utils::coGetAllDbusProperty(
            utils::entityManagerServiceStr, slotPath.c_str(),
            rotSlotInterface.c_str());

        SlotInfo slot{};
        if (allSlotIfaceProperties.count("Name"))
        {
            slot.slotName =
                std::get<std::string>(allSlotIfaceProperties.at("Name"));
        }
        if (allSlotIfaceProperties.count("ComponentClassification"))
        {
            slot.classification = std::get<uint64_t>(
                allSlotIfaceProperties.at("ComponentClassification"));
        }
        if (allSlotIfaceProperties.count("ComponentIdentifier"))
        {
            slot.identifier = std::get<uint64_t>(
                allSlotIfaceProperties.at("ComponentIdentifier"));
        }
        if (allSlotIfaceProperties.count("ComponentIndex"))
        {
            slot.index =
                std::get<uint64_t>(allSlotIfaceProperties.at("ComponentIndex"));
        }
        if (allSlotIfaceProperties.count("FirmwareType"))
        {
            slot.fwType = std::get<std::string>(
                allSlotIfaceProperties.at("FirmwareType"));
        }
        slot.associations = utils::getAssociations(
            slotPath, std::string(rotSlotInterface) + ".Associations");
        if (allSlotIfaceProperties.count("ChassisName"))
        {
            slot.chassisName =
                std::get<std::string>(allSlotIfaceProperties.at("ChassisName"));
        }

        slot.isRoT = false;
        if (allSlotIfaceProperties.count("IsRoT"))
        {
            slot.isRoT = std::get<bool>(allSlotIfaceProperties.at("IsRoT"));
        }
        slot.reportSkuWithNsm = false;
        if (allSlotIfaceProperties.count("ReportSkuWithNsm"))
        {
            slot.reportSkuWithNsm =
                std::get<bool>(allSlotIfaceProperties.at("ReportSkuWithNsm"));
        }

        slot.enableUpdateSKU = false;
        if (allSlotIfaceProperties.count("SetRotPropertyList"))
        {
            try
            {
                auto propertyList = std::get<std::vector<std::string>>(
                    allSlotIfaceProperties.at("SetRotPropertyList"));

                if (std::find(propertyList.begin(), propertyList.end(),
                              "AP_SKU_ID") != propertyList.end())
                {
                    slot.enableUpdateSKU = true;
                }
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "Failed to parse SetRotPropertyList for slot:{SLOT}: {ERROR}",
                    "SLOT", slot.slotName, "ERROR", e.what());
            }
        }

        if (slot.fwType == "EC")
        {
            slot.isRoT = true;
        }

        slots.push_back(slot);
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine nsmErotCreateSensors(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath)
{
    auto rotSlotInterface = "xyz.openbmc_project.Configuration.NSM_RoT_Slot";

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
        uuid_t uuid{};
        if (allCurrentIfaceProperties.count("UUID"))
        {
            uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
        }

        auto path = std::string(chassisInventoryBasePath) + "/" + name;
        uint64_t slotCount{};
        if (allCurrentIfaceProperties.count("SlotCount"))
        {
            slotCount =
                std::get<uint64_t>(allCurrentIfaceProperties.at("SlotCount"));
        }
        else
        {
            // coverity[missing_return]
            co_return NSM_SUCCESS; // not a RoT chassis
        }

        bool imageCopyEnabled = false;
        if (allCurrentIfaceProperties.count("ImageCopyEnabled"))
        {
            imageCopyEnabled = std::get<bool>(
                allCurrentIfaceProperties.at("ImageCopyEnabled"));
        }

        bool inbandUpdatePolicyEnabled = false;
        if (allCurrentIfaceProperties.count("InbandUpdatePolicyEnabled"))
        {
            inbandUpdatePolicyEnabled = std::get<bool>(
                allCurrentIfaceProperties.at("InbandUpdatePolicyEnabled"));
        }

        bool imageCopyPolicyEnabled = false;
        if (allCurrentIfaceProperties.count("ImageCopyPolicyEnabled"))
        {
            imageCopyPolicyEnabled = std::get<bool>(
                allCurrentIfaceProperties.at("ImageCopyPolicyEnabled"));
        }

        auto device = manager.getNsmDeviceFromStaticUUID(uuid);

        if (!device)
        {
            lg2::error("Failed to get NsmDevice for UUID:{UUID}", "UUID", uuid);
            co_return NSM_SW_ERROR;
        }

        auto& bus = utils::DBusHandler::getBus();

        std::shared_ptr<ProgressIntf> rotProgressIntf = nullptr;

        std::map<std::string, std::shared_ptr<NsmBuildTypeObject>>
            apFirmwareTypeMap;
        std::map<std::string, std::shared_ptr<ProgressIntf>> apProgressIntfMap;
        std::map<std::string, std::shared_ptr<NsmKeyMgmt>> apKeyMgmtMap;
        std::map<std::string, std::shared_ptr<NsmMinSecVersionObject>>
            apMinSecVersionMap;

        std::map<std::string, std::shared_ptr<NsmApSkuIdObject>>
            apSkuIdSensorMap;
        std::map<std::string, std::shared_ptr<NsmUpdateApSkuIdIntf>>
            apUpdateSkuIntfMap;

        std::shared_ptr<NsmBuildTypeObject> ecFirmwareType = nullptr;
        std::shared_ptr<ProgressIntf> ecProgressIntf = nullptr;
        std::shared_ptr<NsmKeyMgmt> ecKeyMgmt = nullptr;
        std::shared_ptr<NsmMinSecVersionObject> ecMinSecVersion = nullptr;
        std::shared_ptr<NsmInbandUpdatePolicyObject> inbandUpdatePolicy =
            nullptr;
        std::shared_ptr<NsmImageCopyPolicyObject> imageCopyPolicySensor =
            nullptr;
        std::shared_ptr<NsmImageCopyObject> imageCopyObject = nullptr;

        std::vector<SlotInfo> allSlots;
        uint8_t parseRc = co_await parseSlots(path, slotCount, rotSlotInterface,
                                              allSlots);
        if (parseRc != NSM_SW_SUCCESS)
        {
            lg2::error("Failed to parse slots for {PATH}", "PATH", path);
            co_return parseRc;
        }

        for (const auto& slot : allSlots)
        {
            if (slot.fwType == "AP")
            {
                auto slotObject = std::make_shared<NsmFirmwareSlot>(
                    bus, path, slot.associations, extractNumber(slot.slotName),
                    SlotIntf::FirmwareType::AP, slot.chassisName);

                const std::string& slotKey = slot.isRoT ? name
                                                        : slot.chassisName;

                if (apFirmwareTypeMap.find(slotKey) == apFirmwareTypeMap.end())
                {
                    apFirmwareTypeMap[slotKey] =
                        std::make_shared<NsmBuildTypeObject>(
                            name, type, uuid, slot.classification,
                            slot.identifier);
                }
                apFirmwareTypeMap[slotKey]->addSlotObject(slotObject);

                if (apProgressIntfMap.find(slot.chassisName) ==
                    apProgressIntfMap.end())
                {
                    auto progressPath = std::string(chassisInventoryBasePath) +
                                        "/" + slot.chassisName;
                    apProgressIntfMap[slot.chassisName] =
                        std::make_shared<ProgressIntf>(bus,
                                                       progressPath.c_str());
                }

                if (apKeyMgmtMap.find(slot.chassisName) == apKeyMgmtMap.end())
                {
                    apKeyMgmtMap[slot.chassisName] =
                        std::make_shared<NsmKeyMgmt>(
                            bus, slot.chassisName, type, uuid,
                            apProgressIntfMap[slot.chassisName],
                            slot.classification, slot.identifier,
                            static_cast<uint8_t>(slot.index));
                }
                apKeyMgmtMap[slot.chassisName]->addSlotObject(slotObject);

                if (apMinSecVersionMap.find(slot.chassisName) ==
                    apMinSecVersionMap.end())
                {
                    apMinSecVersionMap[slot.chassisName] =
                        std::make_shared<NsmMinSecVersionObject>(
                            bus, slot.chassisName, type, uuid,
                            slot.classification, slot.identifier,
                            static_cast<uint8_t>(slot.index),
                            apProgressIntfMap[slot.chassisName]);
                    device->addSensor(apMinSecVersionMap[slot.chassisName],
                                      PollingType::RoundRobin);
                }

                if (imageCopyPolicySensor == nullptr && imageCopyPolicyEnabled)
                {
                    auto inventoryPath = std::string(chassisInventoryBasePath) +
                                         "/" + name;

                    imageCopyPolicySensor =
                        std::make_shared<NsmImageCopyPolicyObject>(
                            bus, name, type, slot.classification,
                            slot.identifier, static_cast<uint8_t>(slot.index));
                    device->addSensor(imageCopyPolicySensor,
                                      PollingType::RoundRobin);

                    // Register async set handler for ImageCopyPolicy property
                    nsm::AsyncSetOperationHandler imageCopyPolicyHandler =
                        std::bind(&nsm::updateImageCopyPolicyHandler,
                                  std::placeholders::_1, std::placeholders::_2,
                                  std::placeholders::_3, slot.classification,
                                  slot.identifier,
                                  static_cast<uint8_t>(slot.index));
                    AsyncOperationManager::getInstance()
                        ->getDispatcher(inventoryPath)
                        ->addAsyncSetOperation(
                            "com.nvidia.ImageCopyPolicy", "ImageCopyPolicy",
                            AsyncSetOperationInfo{imageCopyPolicyHandler,
                                                  imageCopyPolicySensor,
                                                  device});
                    lg2::info(
                        "ImageCopyPolicy interface created successfully for chassis:{CHASSIS}",
                        "CHASSIS", name);
                }
                if (slot.reportSkuWithNsm &&
                    apSkuIdSensorMap.find(slot.chassisName) ==
                        apSkuIdSensorMap.end())
                {
                    apSkuIdSensorMap[slot.chassisName] =
                        std::make_shared<NsmApSkuIdObject>(
                            bus, slot.chassisName, type, uuid,
                            apProgressIntfMap[slot.chassisName],
                            slot.classification, slot.identifier,
                            static_cast<uint8_t>(slot.index),
                            slot.associations);
                    device->addSensor(apSkuIdSensorMap[slot.chassisName],
                                      PollingType::RoundRobin);
                }

                if (slot.enableUpdateSKU &&
                    apUpdateSkuIntfMap.find(slot.chassisName) ==
                        apUpdateSkuIntfMap.end())
                {
                    auto inventoryPath = std::string(chassisInventoryBasePath) +
                                         "/" + slot.chassisName;
                    lg2::info(
                        "Creating UpdateSKU interface for chassis:{CHASSIS} path:{PATH} with AP component (class={CLASS}, id={ID}, idx={IDX})",
                        "CHASSIS", slot.chassisName, "PATH", inventoryPath,
                        "CLASS", slot.classification, "ID", slot.identifier,
                        "IDX", static_cast<uint8_t>(slot.index));
                    apUpdateSkuIntfMap[slot.chassisName] =
                        std::make_shared<NsmUpdateApSkuIdIntf>(
                            bus, slot.chassisName, type, inventoryPath);
                    device->addDeviceSensors(
                        apUpdateSkuIntfMap[slot.chassisName]);
                    nsm::AsyncSetOperationHandler updateSkuHandler =
                        std::bind(&updateApSkuIdHandler, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3,
                                  slot.classification, slot.identifier,
                                  static_cast<uint8_t>(slot.index));
                    AsyncOperationManager::getInstance()
                        ->getDispatcher(inventoryPath)
                        ->addAsyncSetOperation(
                            "xyz.openbmc_project.Inventory.Decorator.SKU",
                            "SKU",
                            AsyncSetOperationInfo{
                                updateSkuHandler,
                                apSkuIdSensorMap[slot.chassisName], device});
                }
            }
            else // EC
            {
                auto slotObject = std::make_shared<NsmFirmwareSlot>(
                    bus, path, slot.associations, extractNumber(slot.slotName),
                    SlotIntf::FirmwareType::EC, slot.chassisName);

                if (ecFirmwareType == nullptr)
                {
                    ecFirmwareType = std::make_shared<NsmBuildTypeObject>(
                        name, type, uuid, slot.classification, slot.identifier);
                }
                ecFirmwareType->addSlotObject(slotObject);
                if (ecProgressIntf == nullptr)
                {
                    auto progressPath = std::string(chassisInventoryBasePath) +
                                        "/" + name;
                    ecProgressIntf = std::make_shared<ProgressIntf>(
                        bus, progressPath.c_str());
                }
                if (ecKeyMgmt == nullptr)
                {
                    ecKeyMgmt = std::make_shared<NsmKeyMgmt>(
                        bus, name, type, uuid, ecProgressIntf,
                        slot.classification, slot.identifier,
                        static_cast<uint8_t>(slot.index));
                }
                ecKeyMgmt->addSlotObject(slotObject);
                if (ecMinSecVersion == nullptr)
                {
                    ecMinSecVersion = std::make_shared<NsmMinSecVersionObject>(
                        bus, name, type, uuid, slot.classification,
                        slot.identifier, static_cast<uint8_t>(slot.index),
                        ecProgressIntf);
                    device->addSensor(ecMinSecVersion, PollingType::RoundRobin);
                }
            }

            if (inbandUpdatePolicy == nullptr && inbandUpdatePolicyEnabled)
            {
                auto inventoryPath = std::string(chassisInventoryBasePath) +
                                     "/" + name;

                inbandUpdatePolicy =
                    std::make_shared<NsmInbandUpdatePolicyObject>(
                        bus, name, slot.classification, slot.identifier,
                        static_cast<uint8_t>(slot.index));
                device->addSensor(inbandUpdatePolicy, PollingType::RoundRobin);

                // Register async set handler for InbandUpdatePolicy property
                // with Async.Set support
                nsm::AsyncSetOperationHandler inbandUpdatePolicyHandler =
                    std::bind(&nsm::updateInbandUpdatePolicyHandler,
                              std::placeholders::_1, std::placeholders::_2,
                              std::placeholders::_3, slot.classification,
                              slot.identifier,
                              static_cast<uint8_t>(slot.index));
                AsyncOperationManager::getInstance()
                    ->getDispatcher(inventoryPath)
                    ->addAsyncSetOperation(
                        "com.nvidia.InbandUpdatePolicy", "InbandUpdatePolicy",
                        AsyncSetOperationInfo{inbandUpdatePolicyHandler,
                                              inbandUpdatePolicy, device});
            }

            if (imageCopyObject == nullptr and imageCopyEnabled)
            {
                imageCopyObject =
                    std::make_shared<NsmImageCopyObject>(bus, name, uuid);
                device->addSensor(imageCopyObject, PollingType::RoundRobin);
            }
        }

        for (const auto& [_, apFwType] : apFirmwareTypeMap)
        {
            if (apFwType)
            {
                device->addSensor(apFwType, PollingType::RoundRobin);
            }
        }
        for (const auto& [_, apKeyMgmtObj] : apKeyMgmtMap)
        {
            if (apKeyMgmtObj)
            {
                device->addSensor(apKeyMgmtObj, PollingType::RoundRobin);
            }
        }

        if (ecFirmwareType)
        {
            device->addSensor(ecFirmwareType, PollingType::RoundRobin);
        }
        if (ecKeyMgmt)
        {
            device->addSensor(ecKeyMgmt, PollingType::RoundRobin);
        }
        if (rotProgressIntf == nullptr)
        {
            if (ecProgressIntf != nullptr)
            {
                rotProgressIntf = ecProgressIntf;
            }
            else
            {
                auto progressPath = std::string(chassisInventoryBasePath) +
                                    "/" + name;
                rotProgressIntf =
                    std::make_shared<ProgressIntf>(bus, progressPath.c_str());
            }
        }
        auto securityCfg = std::make_shared<NsmSecurityCfgObject>(
            bus, name, type, uuid, rotProgressIntf);
        device->addSensor(securityCfg, PollingType::RoundRobin);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

std::vector<std::string> erotInterfaces{
    "xyz.openbmc_project.Configuration.NSM_Chassis",
    "xyz.openbmc_project.Configuration.NSM_ChassisRoT"};

REGISTER_NSM_CREATION_FUNCTION(nsmErotCreateSensors, erotInterfaces)

} // namespace nsm
