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

#include <set>

namespace nsm
{

class NsmCommandCodes : public NsmObject
{
  public:
    NsmCommandCodes(const std::string& name, const std::string& type,
                    NsmDevice& nsmDevice, uint8_t messageType) :
        NsmObject(name, type), messageType(messageType), nsmDevice(nsmDevice)
    {}

    bool isSupported() const
    {
        return supported;
    }

    void setSupported(bool value)
    {
        supported = value;
    }

    bool needsUpdate(const uint64_t& currentTimestampInUsec) const override
    {
        // A sensor whose message type is no longer advertised stays in the
        // polling queue but is skipped by the polling loop.
        return supported && NsmObject::needsUpdate(currentTimestampInUsec);
    }

    requester::Coroutine update(std::shared_ptr<NsmDevice> device) override
    {
        if (!supported)
        {
            // Guard for update paths that do not consult needsUpdate():
            // issue no request for a de-advertised message type.
            // coverity[missing_return]
            co_return NSM_SW_SUCCESS;
        }

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_supported_command_codes_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_get_supported_command_codes_req(
            DEFAULT_INSTANCE_ID, messageType, requestMsg);

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("getSupportedCommandCodes failed. eid={EID} rc={RC}",
                       "EID", device->getEid(), "RC", rc);
            // coverity[missing_return]
            co_return rc;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = co_await device->sensorIO(device->getEid(), request, responseMsg,
                                       responseLen, true);
        if (rc)
        {
            lg2::error(
                "NsmGetEventConfig SendRecvNsmMsg failed with RC={RC}, eid={EID}",
                "RC", rc, "EID", device->getEid());
            // coverity[missing_return]
            co_return rc;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        bitfield8_t supportedCommands[SUPPORTED_COMMAND_CODE_DATA_SIZE];
        rc = decode_get_supported_command_codes_resp(
            responseMsg.get(), responseLen, &cc, &reason_code,
            supportedCommands);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            lg2::error(
                "get supported command code decode failed. eid={EID} cc={CC} reasonCode={REASONCODE} and rc={RC}",
                "EID", device->getEid(), "CC", cc, "REASONCODE", reason_code,
                "RC", rc);
            // coverity[missing_return]
            co_return cc ? cc : rc;
        }
        device->updateMessageTypesToCommandCodeMatrix(
            messageType, supportedCommands, SUPPORTED_COMMAND_CODE_DATA_SIZE);

        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

  private:
    uint8_t messageType;
    NsmDevice& nsmDevice;
    bool supported = true;
};
class NsmMsgTypes : public NsmObject
{
  private:
    NsmDevice& nsmDevice;
    std::map<uint8_t, std::shared_ptr<NsmCommandCodes>> commandCodesSensors;

  public:
    NsmMsgTypes(const std::string& name, const std::string& type,
                NsmDevice& nsmDevice) :
        NsmObject(name, type), nsmDevice(nsmDevice)
    {}

    requester::Coroutine update(std::shared_ptr<NsmDevice> device) override
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_supported_nvidia_message_types_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_get_supported_nvidia_message_types_req(
            DEFAULT_INSTANCE_ID, requestMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to encode get supported nvidia message types request, rc={RC}, eid={EID}",
                "RC", rc, "EID", device->getEid());
            // coverity[missing_return]
            co_return rc;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = co_await device->sensorIO(device->getEid(), request, responseMsg,
                                       responseLen, true);
        if (rc)
        {
            lg2::error(
                "NsmGetEventConfig SendRecvNsmMsg failed with RC={RC}, eid={EID}",
                "RC", rc, "EID", device->getEid());
            // coverity[missing_return]
            co_return rc;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE];
        rc = decode_get_supported_nvidia_message_types_resp(
            responseMsg.get(), responseLen, &cc, &reasonCode, types);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            lg2::error(
                "get supported msg type decode failed. eid={EID} cc={CC} reasonCode={REASONCODE} and rc={RC}",
                "EID", device->getEid(), "CC", cc, "REASONCODE", reasonCode,
                "RC", rc);
            // coverity[missing_return]
            co_return cc ? cc : rc;
        }

        // Convert bit matrix of types into the latest supported type set.
        std::set<uint8_t> supportedTypes;
        for (size_t i = 0; i < SUPPORTED_MSG_TYPE_DATA_SIZE * 8; i++)
        {
            auto isSupported = types[i / 8].byte & (1 << (i % 8));
            if (isSupported)
            {
                supportedTypes.emplace(static_cast<uint8_t>(i));
            }
        }

        // Treat the latest response as authoritative. Sensors are never
        // removed from the device sensor list: erasing entries would shift
        // CircularQueue indexing while the polling loop iterates. A sensor
        // whose message type is no longer advertised is disabled instead,
        // so polling skips it, and its command-code matrix row is cleared.
        // It is re-enabled if the type is advertised again later.
        for (auto& [messageType, commandCodes] : commandCodesSensors)
        {
            bool typeSupported = supportedTypes.contains(messageType);
            if (commandCodes->isSupported() && !typeSupported)
            {
                lg2::info(
                    "NsmMsgTypes: disabling stale command code sensor. eid={EID} messageType={MSG_TYPE} name={NAME}",
                    "EID", device->getEid(), "MSG_TYPE", messageType, "NAME",
                    commandCodes->getName());
                const bitfield8_t
                    noCommands[SUPPORTED_COMMAND_CODE_DATA_SIZE]{};
                device->updateMessageTypesToCommandCodeMatrix(
                    messageType, noCommands, SUPPORTED_COMMAND_CODE_DATA_SIZE);
            }
            else if (!commandCodes->isSupported() && typeSupported)
            {
                lg2::info(
                    "NsmMsgTypes: re-enabling command code sensor. eid={EID} messageType={MSG_TYPE} name={NAME}",
                    "EID", device->getEid(), "MSG_TYPE", messageType, "NAME",
                    commandCodes->getName());
            }
            commandCodes->setSupported(typeSupported);
        }

        for (auto messageType : supportedTypes)
        {
            if (commandCodesSensors.find(messageType) ==
                commandCodesSensors.end())
            {
                // New message type, create and add sensor
                auto commandSensor = std::make_shared<NsmCommandCodes>(
                    "Supported Command Codes " + std::to_string(messageType),
                    "NSM_NVIDIA_MESSAGE_TYPE_" + std::to_string(messageType),
                    nsmDevice, messageType);
                commandCodesSensors[messageType] = commandSensor;
                device->addSensor(commandSensor, false);
            }
        }

        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
};

}; // namespace nsm
