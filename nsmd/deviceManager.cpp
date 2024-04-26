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

#include <cstdint>
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
            auto& [eid, uuid, mctpMedium, networkdId, mctpBinding] = mctpInfo;
            auto rc = co_await ping(eid);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error("NSM ping failed, rc={RC} eid={EID}", "RC", rc,
                           "EID", eid);
                continue;
            }

            lg2::info("found NSM device, eid={EID} uuid={UUID}", "EID", eid,
                      "UUID", uuid);

            auto nsmDevice = findNsmDeviceByUUID(nsmDevices, uuid);
            if (nsmDevice)
            {
                lg2::info(
                    "The NSM device has been discovered before, uuid={UUID}",
                    "UUID", uuid);
                continue;
            }

            nsmDevice = std::make_shared<NsmDevice>(uuid);
            nsmDevices.emplace_back(nsmDevice);

            // get supported commands for device for each message type
            for (uint8_t messageType : supportedMessageTypes)
            {
                std::vector<uint8_t> supportedCommands;
                rc = co_await getSupportedCommandCodes(eid, messageType,
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
                                                              [commandCode] =
                        true;
                    ss << int(commandCode) << " ";
                }
                lg2::info("MessageType {ROW_NUM}: commandCodes {ROW_VALUES}",
                          "ROW_NUM", messageType, "ROW_VALUES", ss.str());
            }

            // get inventory information from device
            InventoryProperties properties{};
            rc = co_await getFRU(eid, properties);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error("getFRU() return failed, rc={RC} eid={EID}", "RC",
                           rc, "EID", eid);
                continue;
            }

            uint8_t deviceIdentification = 0;
            uint8_t deviceInstance = 0;
            rc = co_await getQueryDeviceIdentification(
                eid, deviceIdentification, deviceInstance);
            if (rc != NSM_SUCCESS)
            {
                lg2::error(
                    "NSM getQueryDeviceIdentification failed, rc={RC} eid={EID}",
                    "RC", rc, "EID", eid);
                continue;
            }

            // update eid table [from UUID from MCTP dbus property]
            eidTable.insert(std::make_pair(
                uuid, std::make_tuple(eid, mctpMedium, mctpBinding)));

            // expose inventory information to FruDevice PDI
            nsmDevice->fruDeviceIntf = objServer.add_interface(
                "/xyz/openbmc_project/FruDevice/" + std::to_string(eid),
                "xyz.openbmc_project.FruDevice");

            if (properties.find(BOARD_PART_NUMBER) != properties.end())
            {
                nsmDevice->fruDeviceIntf->register_property(
                    "BOARD_PART_NUMBER",
                    std::get<std::string>(properties[BOARD_PART_NUMBER]));
            }

            if (properties.find(SERIAL_NUMBER) != properties.end())
            {
                nsmDevice->fruDeviceIntf->register_property(
                    "SERIAL_NUMBER",
                    std::get<std::string>(properties[SERIAL_NUMBER]));
            }

            nsmDevice->fruDeviceIntf->register_property("DEVICE_TYPE",
                                                        deviceIdentification);
            nsmDevice->fruDeviceIntf->register_property("INSTANCE_NUMBER",
                                                        deviceInstance);
            nsmDevice->fruDeviceIntf->register_property("UUID", uuid);

            nsmDevice->fruDeviceIntf->initialize();
        }
        queuedMctpInfos.pop();
    }
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::ping(eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t instanceId = instanceIdDb.next(eid);

    auto rc = encode_ping_req(instanceId, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("ping failed. eid={EID} rc={RC}", "EID", eid, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const nsm_msg* respMsg = NULL;
    size_t respLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, &respMsg, &respLen);
    if (rc)
    {
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
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::getSupportedNvidiaMessageType(
    eid_t eid, std::vector<uint8_t>& supportedTypes)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_nvidia_message_types_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t instanceId = instanceIdDb.next(eid);
    auto rc =
        encode_get_supported_nvidia_message_types_req(instanceId, requestMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("getSupportedNvidiaMessageType failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        instanceIdDb.free(eid, instanceId);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, &responseMsg, &responseLen);
    if (rc)
    {
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
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    // copy contents of types into supportedTypes
    supportedTypes.resize(SUPPORTED_MSG_TYPE_DATA_SIZE);
    std::memcpy(supportedTypes.data(), types,
                SUPPORTED_MSG_TYPE_DATA_SIZE * sizeof(bitfield8_t));
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::getSupportedCommandCodes(
    eid_t eid, uint8_t nvidia_message_type,
    std::vector<uint8_t>& supportedCommands)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_command_codes_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t instanceId = instanceIdDb.next(eid);
    auto rc = encode_get_supported_command_codes_req(
        instanceId, nvidia_message_type, requestMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("getSupportedCommandCodes failed. eid={EID} rc={RC}", "EID",
                   eid, "RC", rc);
        instanceIdDb.free(eid, instanceId);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, &responseMsg, &responseLen);
    if (rc)
    {
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
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    // copy supportedCommandCodes into supportedCommands
    supportedCommands.resize(SUPPORTED_COMMAND_CODE_DATA_SIZE);
    std::memcpy(supportedCommands.data(), supportedCommandCodes,
                SUPPORTED_COMMAND_CODE_DATA_SIZE * sizeof(bitfield8_t));
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::getFRU(eid_t eid,
                                           nsm::InventoryProperties& properties)
{
    std::vector<uint8_t> propertyIds = {BOARD_PART_NUMBER, SERIAL_NUMBER};
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
            nsmDevice
                ->messageTypesToCommandCodeMatrix[messageType][commandCode] =
                true;
            ss << int(commandCode) << " ";
        }
        lg2::info("MessageType {ROW_NUM}: commandCodes {ROW_VALUES}", "ROW_NUM",
                  messageType, "ROW_VALUES", ss.str());
    }
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::getInventoryInformation(
    eid_t eid, uint8_t& propertyIdentifier, InventoryProperties& properties)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t instanceId = instanceIdDb.next(eid);

    auto rc = encode_get_inventory_information_req(
        instanceId, propertyIdentifier, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, &responseMsg, &responseLen);
    if (rc)
    {
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
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

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
            std::string property((char*)data.data(), dataSize);
            properties.emplace(propertyIdentifier, property);
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
                co_return NSM_SW_ERROR_LENGTH;
            }
            memcpy(nvu8ArrVal.data(), data.data(), dataSize);

            uuid_t uuidStr = utils::convertUUIDToString(nvu8ArrVal);
            properties.emplace(propertyIdentifier, uuidStr);
        }
        break;
        default:
        {
            lg2::info("getInventoryInformation unsupported id={ID}", "ID",
                      propertyIdentifier);
        }
        break;
    }

    for (auto& p : properties)
    {
        lg2::info("id={ID} value={VALUE}", "ID", p.first, "VALUE",
                  std::get<std::string>(p.second).c_str());
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine DeviceManager::getQueryDeviceIdentification(
    eid_t eid, uint8_t& deviceIdentification, uint8_t& deviceInstance)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_device_identification_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t instanceId = instanceIdDb.next(eid);
    auto rc =
        encode_nsm_query_device_identification_req(instanceId, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_nsm_query_device_identification_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const nsm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, &responseMsg, &responseLen);
    if (rc)
    {
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
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

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
        lg2::error("SendRecvNsmMsg failed. eid={EID} rc={RC}", "EID", eid, "RC",
                   rc);
    }
    co_return rc;
}

} // namespace nsm