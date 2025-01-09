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

#include "deviceManager.hpp"

#include "base.h"
#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace nsm
{
// Definition of the static instance pointer
DeviceManager* DeviceManager::instance = nullptr;

void DeviceManager::discoverNsmDevice(const MctpInfos& mctpInfos)
{
    queuedMctpInfos.emplace(mctpInfos);
    if (discoverNsmDeviceTaskHandle)
    {
        if (!discoverNsmDeviceTaskHandle.done())
        {
            return;
        }
        discoverNsmDeviceTaskHandle.destroy();
    }

    auto co = discoverNsmDeviceTask();
    discoverNsmDeviceTaskHandle = co.handle;
    if (discoverNsmDeviceTaskHandle.done())
    {
        discoverNsmDeviceTaskHandle = nullptr;
    }
}

requester::Coroutine DeviceManager::discoverNsmDeviceTask()
{
    while (!queuedMctpInfos.empty())
    {
        const MctpInfos& mctpInfos = queuedMctpInfos.front();
        for (auto& mctpInfo : mctpInfos)
        {
            // try ping
            auto& [eid, mctpUuid, mctpMedium, networkdId,
                   mctpBinding] = mctpInfo;
            auto rc = co_await ping(eid);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error("NSM ping failed, rc={RC} eid={EID}", "RC", rc,
                           "EID", eid);
                continue;
            }

            lg2::info("found NSM device, eid={EID} uuid={UUID}", "EID", eid,
                      "UUID", mctpUuid);

            // get device identification from device
            uint8_t deviceType = 0;
            uint8_t instanceNumber = 0;
            rc = co_await getQueryDeviceIdentification(eid, deviceType,
                                                       instanceNumber);
            if (rc != NSM_SUCCESS)
            {
                lg2::error(
                    "NSM getQueryDeviceIdentification failed, rc={RC} eid={EID}",
                    "RC", rc, "EID", eid);
                continue;
            }

            // save the nsm device identification info
            discoveredEIDs[eid] = {mctpUuid, deviceType, instanceNumber, true};

            // update eid table [from UUID from MCTP dbus property]
            eidTable.insert(std::make_pair(
                mctpUuid, std::make_tuple(eid, mctpMedium, mctpBinding)));
        }
        queuedMctpInfos.pop();
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::ping(eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_ping_req(DEFAULT_INSTANCE_ID, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("ping failed. eid={EID} rc={RC}", "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const nsm_msg* respMsg = NULL;
    size_t respLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, &respMsg, &respLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_ping_resp(respMsg, respLen, &cc, &reason_code);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "ping decode failed. eid={EID} cc={CC} reasonCode={REASONCODE} and rc={RC}",
            "EID", eid, "CC", cc, "REASONCODE", reason_code, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::getSupportedNvidiaMessageType(
    eid_t eid, std::vector<uint8_t>& supportedTypes)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_nvidia_message_types_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_nvidia_message_types_req(DEFAULT_INSTANCE_ID,
                                                            requestMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("getSupportedNvidiaMessageType failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, &responseMsg, &responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE];
    rc = decode_get_supported_nvidia_message_types_resp(
        responseMsg, responseLen, &cc, &reason_code, types);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "get supported msg type decode failed. eid={EID} cc={CC} reasonCode={REASONCODE} and rc={RC}",
            "EID", eid, "CC", cc, "REASONCODE", reason_code, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    // copy contents of types into supportedTypes
    supportedTypes.resize(SUPPORTED_MSG_TYPE_DATA_SIZE);
    std::memcpy(supportedTypes.data(), types,
                SUPPORTED_MSG_TYPE_DATA_SIZE * sizeof(bitfield8_t));
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::getSupportedCommandCodes(
    eid_t eid, uint8_t nvidia_message_type,
    std::vector<uint8_t>& supportedCommands)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_command_codes_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_command_codes_req(
        DEFAULT_INSTANCE_ID, nvidia_message_type, requestMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("getSupportedCommandCodes failed. eid={EID} rc={RC}", "EID",
                   eid, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, &responseMsg, &responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    bitfield8_t supportedCommandCodes[SUPPORTED_COMMAND_CODE_DATA_SIZE];
    rc = decode_get_supported_command_codes_resp(
        responseMsg, responseLen, &cc, &reason_code, &supportedCommandCodes[0]);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "get supported command code decode failed. eid={EID} cc={CC} reasonCode={REASONCODE} and rc={RC}",
            "EID", eid, "CC", cc, "REASONCODE", reason_code, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    // copy supportedCommandCodes into supportedCommands
    supportedCommands.resize(SUPPORTED_COMMAND_CODE_DATA_SIZE);
    std::memcpy(supportedCommands.data(), supportedCommandCodes,
                SUPPORTED_COMMAND_CODE_DATA_SIZE * sizeof(bitfield8_t));
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::getFRU(eid_t eid,
                                           nsm::InventoryProperties& properties,
                                           const uint8_t& deviceType)
{
    // creating map to avoid sending unsupported comamnds to devices
    // Define a map that stores property IDs based on deviceType
    static const std::unordered_map<uint8_t, std::vector<uint8_t>>
        devicePropertyMap = {{NSM_DEV_ID_GPU,
                              {BOARD_PART_NUMBER, SERIAL_NUMBER, DEVICE_GUID,
                               MARKETING_NAME, BUILD_DATE}},
                             {NSM_DEV_ID_SWITCH,
                              {BOARD_PART_NUMBER, SERIAL_NUMBER, DEVICE_GUID,
                               MARKETING_NAME, BUILD_DATE}},
                             {NSM_DEV_ID_PCIE_BRIDGE,
                              {BOARD_PART_NUMBER, SERIAL_NUMBER, DEVICE_GUID,
                               MARKETING_NAME, BUILD_DATE}},
                             {NSM_DEV_ID_BASEBOARD, {}},
                             {NSM_DEV_ID_EROT,
                              {BOARD_PART_NUMBER, SERIAL_NUMBER, DEVICE_GUID,
                               MARKETING_NAME, BUILD_DATE}},
                             {NSM_DEV_ID_UNKNOWN, {}}};

    // Fetch property IDs based on deviceType; fallback to an empty list if not
    // found
    auto it = devicePropertyMap.find(deviceType);
    const std::vector<uint8_t>& propertyIds =
        (it != devicePropertyMap.end())
            ? it->second
            : devicePropertyMap.at(NSM_DEV_ID_UNKNOWN);

    for (auto propertyId : propertyIds)
    {
        auto rc = co_await getInventoryInformation(eid, propertyId, properties);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "getInventoryInformation failed for propertyId={PID} eid={EID} rc={RC}",
                "PID", propertyId, "EID", eid, "RC", rc);
        }
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    DeviceManager::updateNsmDevice(std::shared_ptr<NsmDevice> nsmDevice,
                                   uint8_t eid)
{
    // Reset messageTypesToCommandCodeMatrix to all false entries
    nsmDevice->messageTypesToCommandCodeMatrix.assign(
        NUM_NSM_TYPES, std::vector<bool>(NUM_COMMAND_CODES, false));

    // Loop through supported message types
    for (uint8_t messageType : supportedMessageTypes)
    {
        std::vector<uint8_t> supportedCommands;
        auto rc = co_await getSupportedCommandCodes(eid, messageType,
                                                    supportedCommands);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "getSupportedCommands() for message type={MT} return failed, rc={RC} eid={EID}",
                "MT", messageType, "RC", rc, "EID", eid);
            continue;
        }

        std::vector<uint8_t> supportedCommandCodes;
        utils::convertBitMaskToVector(
            supportedCommandCodes,
            reinterpret_cast<const bitfield8_t*>(&supportedCommands[0]),
            SUPPORTED_COMMAND_CODE_DATA_SIZE);
        std::stringstream ss;
        for (uint8_t commandCode : supportedCommandCodes)
        {
            nsmDevice->messageTypesToCommandCodeMatrix[messageType]
                                                      [commandCode] = true;
            ss << int(commandCode) << " ";
        }
        lg2::info("MessageType {ROW_NUM}: commandCodes {ROW_VALUES}", "ROW_NUM",
                  messageType, "ROW_VALUES", ss.str());
    }

    // Update fruDevice interface
    auto rc = co_await updateFruDeviceIntf(nsmDevice, eid);
    if (rc)
    {
        lg2::error("updateFruDeviceIntf failed, rc={RC} eid={EID}", "RC", rc,
                   "EID", eid);
    }

    // update sensors that needs to be refreshed after rediscovery event/change
    // of device capabilities
    SensorManager& sensorManager = SensorManager::getInstance();
    size_t sensorIndex{0};
    auto& sensors = nsmDevice->capabilityRefreshSensors;
    while (sensorIndex < sensors.size())
    {
        auto sensor = sensors[sensorIndex];
        co_await sensor->update(sensorManager, eid);
        ++sensorIndex;
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::getInventoryInformation(
    eid_t eid, uint8_t& propertyIdentifier, InventoryProperties& properties)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        DEFAULT_INSTANCE_ID, propertyIdentifier, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, &responseMsg, &responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    std::vector<uint8_t> data(65535, 0);

    rc = decode_get_inventory_information_resp(
        responseMsg, responseLen, &cc, &reason_code, &dataSize, data.data());
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_get_inventory_information_resp failed. eid={EID} cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "EID", eid, "CC", cc, "RESONCODE", reason_code, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::optional<InventoryPropertyData> property;
    switch (propertyIdentifier)
    {
        case BOARD_PART_NUMBER:
        case SERIAL_NUMBER:
        case MARKETING_NAME:
        case DEVICE_PART_NUMBER:
        case FRU_PART_NUMBER:
        case MEMORY_VENDOR:
        case MEMORY_PART_NUMBER:
        case BUILD_DATE:
        case FIRMWARE_VERSION:
        case INFO_ROM_VERSION:
        {
            property = std::string((char*)data.data(), dataSize);
        }
        break;
        case DEVICE_GUID:
        {
            std::vector<uint8_t> nvu8ArrVal(UUID_INT_SIZE, 0);
            if (dataSize < UUID_INT_SIZE || data.size() < UUID_INT_SIZE)
            {
                lg2::error(
                    "decode_get_inventory_information_resp invalid property data. eid={EID} porpertyID={PID}",
                    "EID", eid, "PID", propertyIdentifier);
                // coverity[missing_return]
                co_return NSM_SW_ERROR_LENGTH;
            }
            memcpy(nvu8ArrVal.data(), data.data(), dataSize);

            uuid_t uuidStr = utils::convertUUIDToString(nvu8ArrVal);
            if (uuidStr.empty())
            {
                lg2::error(
                    "getInventoryInformation id={ID} received incorrect GUID",
                    "ID", propertyIdentifier);
            }
            else
            {
                properties.emplace(propertyIdentifier, uuidStr);
            }
        }
        break;
        default:
        {
            lg2::info("getInventoryInformation unsupported id={ID}", "ID",
                      propertyIdentifier);
        }
        break;
    }

    if (property.has_value())
    {
        auto& propertyValue = property.value();
        properties.emplace(propertyIdentifier, propertyValue);

        if (const auto* propertyStringPtr =
                std::get_if<std::string>(&propertyValue))
        {
            lg2::info("id={ID} value={VALUE}", "ID", propertyIdentifier,
                      "VALUE", (*propertyStringPtr).c_str());
        }
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::getQueryDeviceIdentification(
    eid_t eid, uint8_t& deviceIdentification, uint8_t& deviceInstance)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_device_identification_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_device_identification_req(DEFAULT_INSTANCE_ID,
                                                         requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_nsm_query_device_identification_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, &responseMsg, &responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_query_device_identification_resp(
        responseMsg, responseLen, &cc, &reason_code, &deviceIdentification,
        &deviceInstance);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_query_device_identification_resp failed. eid={EID} cc={CC} reasonCode={REASONCODE} rc={RC}",
            "EID", eid, "CC", cc, "REASONCODE", reason_code, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::SendRecvNsmMsg(eid_t eid, Request& request,
                                                   const nsm_msg** responseMsg,
                                                   size_t* responseLen)
{
    auto rc = co_await requester::SendRecvNsmMsg<RequesterHandler>(
        handler, eid, request, responseMsg, responseLen);
    if (rc)
    {
        lg2::error("DeviceManager::SendRecvNsmMsg failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
    }
    // coverity[missing_return]
    co_return rc;
}

void DeviceManager::onlineMctpEndpoint(const MctpInfo& mctpInfo)
{
    MctpInfos mctpInfos{mctpInfo};
    discoverNsmDevice(mctpInfos);
}

void DeviceManager::offlineMctpEndpoint(const MctpInfo& mctpInfo)
{
    const mctp_eid_t eid = std::get<0>(mctpInfo);
    if (discoveredEIDs.find(eid) != discoveredEIDs.end())
    {
        auto& value = discoveredEIDs[eid];
        std::get<3>(value) = false; // set EID is inactive
    }

    const std::string uuid = std::get<1>(mctpInfo);
    auto nsmDevice = findNsmDeviceByUUID(nsmDevices, uuid);
    if (nsmDevice)
    {
        nsmDevice->setOffline();
    }
}

requester::Coroutine
    DeviceManager::updateFruDeviceIntf(std::shared_ptr<NsmDevice> nsmDevice,
                                       uint8_t eid)
{
    // get inventory information from device
    InventoryProperties properties{};
    if (nsmDevice->isCommandSupported(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                      NSM_GET_INVENTORY_INFORMATION))
    {
        auto rc = co_await getFRU(eid, properties, nsmDevice->getDeviceType());
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("getFRU() return failed, rc={RC} eid={EID}", "RC", rc,
                       "EID", eid);
            // coverity[missing_return]
            co_return rc;
        }
    }

    // expose inventory information to FruDevice PDI
    std::string objPath = "/xyz/openbmc_project/FruDevice/" +
                          std::to_string(eid);
    nsmDevice->fruDeviceIntf = objServer.add_unique_interface(
        objPath, "xyz.openbmc_project.FruDevice");

    if (properties.find(BOARD_PART_NUMBER) != properties.end())
    {
        nsmDevice->fruDeviceIntf->register_property(
            "BOARD_PART_NUMBER",
            std::get<std::string>(properties[BOARD_PART_NUMBER]));
    }

    if (properties.find(SERIAL_NUMBER) != properties.end())
    {
        nsmDevice->fruDeviceIntf->register_property(
            "SERIAL_NUMBER", std::get<std::string>(properties[SERIAL_NUMBER]));
    }

    if (properties.find(MARKETING_NAME) != properties.end())
    {
        nsmDevice->fruDeviceIntf->register_property(
            "MARKETING_NAME",
            std::get<std::string>(properties[MARKETING_NAME]));
    }

    if (properties.find(BUILD_DATE) != properties.end())
    {
        nsmDevice->fruDeviceIntf->register_property(
            "BUILD_DATE", std::get<std::string>(properties[BUILD_DATE]));
    }

    auto mctpUuid = utils::getUUIDFromEID(eidTable, eid);
    if (mctpUuid.has_value())
    {
        nsmDevice->uuid = *mctpUuid;
        // set deviceUuid to mctp uuid as default value
        nsmDevice->deviceUuid = nsmDevice->uuid;
    }

    if (properties.find(DEVICE_GUID) != properties.end())
    {
        nsmDevice->fruDeviceIntf->register_property(
            "DEVICE_UUID", std::get<uuid_t>(properties[DEVICE_GUID]));
        nsmDevice->deviceUuid = std::get<uuid_t>(properties[DEVICE_GUID]);
    }

    nsmDevice->fruDeviceIntf->register_property("DEVICE_TYPE",
                                                nsmDevice->getDeviceType());
    nsmDevice->fruDeviceIntf->register_property("INSTANCE_NUMBER",
                                                nsmDevice->getInstanceNumber());
    nsmDevice->fruDeviceIntf->register_property("UUID", nsmDevice->uuid);

    nsmDevice->fruDeviceIntf->initialize();

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

uint8_t DeviceManager::remapInstanceNumber(uint8_t instanceNumber,
                                           uint8_t deviceType, uuid_t& uuid,
                                           mctp_eid_t eid)
{
    // remapping instanceNumber if needed.
    if (mapInstanceNumberToInstanceNumber[deviceType].size() > 0)
    {
        auto& table = mapInstanceNumberToInstanceNumber[deviceType];
        auto it = std::find(table.begin(), table.end(), instanceNumber);
        if (it != table.end())
        {
            auto distance = std::distance(table.begin(), it);
            if (distance >= 0 && distance < 255)
            {
                return distance;
            }
        }
        lg2::info(
            "remapInstanceNumber: failed to remap instanceID:{INST} for DeviceType:{TYPE}",
            "INST", instanceNumber, "TYPE", deviceType);
    }
    else if (mapUuidToInstanceNumber[deviceType].size() > 0)
    {
        auto& table = mapUuidToInstanceNumber[deviceType];
        auto it = std::find(table.begin(), table.end(), uuid);
        if (it != table.end())
        {
            auto distance = std::distance(table.begin(), it);
            if (distance >= 0 && distance < 255)
            {
                return distance;
            }
        }
        lg2::info(
            "remapInstanceNumber: failed to remap instanceID:{INST} for DeviceType:{TYPE} by uuid:{UUID}",
            "INST", instanceNumber, "TYPE", deviceType, "UUID", uuid);
    }
    else if (mapEidToInstanceNumber[deviceType].size() > 0)
    {
        auto& table = mapEidToInstanceNumber[deviceType];
        auto it = std::find(table.begin(), table.end(), eid);
        if (it != table.end())
        {
            auto distance = std::distance(table.begin(), it);
            if (distance >= 0 && distance < 255)
            {
                return distance;
            }
        }
        lg2::info(
            "remapInstanceNumber: failed to remap instanceID:{INST} for DeviceType:{TYPE} by eid:{EID}",
            "INST", instanceNumber, "TYPE", deviceType, "EID", eid);
    }
    return instanceNumber;
}

std::optional<mctp_eid_t>
    DeviceManager::searchEID(uint8_t nsmDeviceType,
                             uint8_t nsmDeviceIntanceNumber)
{
    int matchedCnt = 0;
    mctp_eid_t matchedEid = 0;
    std::string matchedUuid{};
    bool matchedActive = false;
    for (auto& [eid, value] : discoveredEIDs)
    {
        auto& [uuid, mctpDeviceType, mctpDeviceInstanceNumber, active] = value;
        auto instanceNumber = remapInstanceNumber(mctpDeviceInstanceNumber,
                                                  nsmDeviceType, uuid, eid);

        if (nsmDeviceType == mctpDeviceType &&
            nsmDeviceIntanceNumber == instanceNumber && matchedUuid != uuid)
        {
            matchedCnt++;
            matchedEid = eid;
            matchedUuid = uuid;
            matchedActive = active;
        }
    }

    if (matchedCnt == 1 && matchedActive == true)
    {
        return matchedEid;
    }
    return std::nullopt;
}

} // namespace nsm
