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

#include "mockupResponder.hpp"

#include "base.h"
#include "device-capability-discovery.h"
#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"

#include "types.hpp"
#include "utils.hpp"

#include <sys/socket.h>

#include <phosphor-logging/lg2.hpp>

using namespace utils;

#include <ctime>

namespace MockupResponder
{

MockupResponder::MockupResponder(bool verbose, sdeventplus::Event& event,
                                 sdbusplus::asio::object_server& server,
                                 eid_t eid, uint8_t deviceType,
                                 uint8_t instanceId) :
    event(event),
    verbose(verbose), server(server), eventReceiverEid(0),
    globalEventGenerationSetting(GLOBAL_EVENT_GENERATION_DISABLE)
{
    std::string path = "/xyz/openbmc_project/NSM/" + std::to_string(eid);
    iface = server.add_interface(path, "xyz.openbmc_project.NSM.Device");

    iface->register_method("genRediscoveryEvent", [&](uint8_t eid, bool ackr) {
        sendRediscoveryEvent(eid, ackr);
    });

    iface->register_method("genRawEvent",
                           [&](uint8_t eid, uint8_t nsmType, bool ackr,
                               uint8_t ver, uint8_t eventId, uint8_t eventClass,
                               uint16_t eventState) {
                               sendNsmEvent(eid, nsmType, ackr, ver, eventId,
                                            eventClass, eventState, 0, NULL);
                           });

    iface->initialize();

    mockEid = eid;
    mockDeviceType = deviceType;
    mockInstanceId = instanceId;

    sockFd = initSocket();
}

int MockupResponder::initSocket()
{
    lg2::info("connect to Mockup EID({EID})", "EID", mockEid);

    auto fd = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd == -1)
    {
        return false;
    }

    struct sockaddr_un addr
    {};

    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, MCTP_SOCKET_PATH, sizeof(MCTP_SOCKET_PATH) - 1);

    if (::connect(fd, (struct sockaddr*)&addr,
                  sizeof(MCTP_SOCKET_PATH) + sizeof(addr.sun_family) - 1) == -1)
    {
        lg2::error(
            "connect() error to mctp-demux-daemon, errno = {ERROR}, {STRERROR}",
            "ERROR", errno, "STRERROR", std::strerror(errno));
        close(fd);
        return -1;
    }

    uint8_t prefix = MCTP_MSG_EMU_PREFIX;
    uint8_t type = MCTP_MSG_TYPE_VDM;

    ssize_t ret = ::write(fd, &prefix, sizeof(uint8_t));
    if (ret < 0)
    {
        lg2::error(
            "Failed to write mockup prefix code to socket, errno = {ERROR}, {STRERROR}",
            "ERROR", errno, "STRERROR", strerror(errno));
        close(fd);
        return -1;
    }

    ret = ::write(fd, &type, sizeof(uint8_t));
    if (ret < 0)
    {
        lg2::error(
            "Failed to write VDM type code to socket, errno = {ERROR}, {STRERROR}",
            "ERROR", errno, "STRERROR", strerror(errno));
        close(fd);
        return -1;
    }

    ret = ::write(fd, &mockEid, sizeof(uint8_t));
    if (ret == -1)
    {
        lg2::error("Failed to write eid to socket, errno = {ERROR}, {STRERROR}",
                   "ERROR", errno, "STRERROR", strerror(errno));
        close(fd);
        return -1;
    }

    auto callback = [this](sdeventplus::source::IO& io, int fd,
                           uint32_t revents) mutable {
        if (!(revents & EPOLLIN))
        {
            return;
        }

        ssize_t peekedLength = recv(fd, nullptr, 0, MSG_PEEK | MSG_TRUNC);
        if (peekedLength == 0)
        {
            io.get_event().exit(0);
        }
        else if (peekedLength <= -1)
        {
            lg2::error("recv system call failed");
            return;
        }

        std::vector<uint8_t> requestMsg(peekedLength);
        auto recvDataLength =
            recv(fd, static_cast<void*>(requestMsg.data()), peekedLength, 0);
        if (recvDataLength != peekedLength)
        {
            lg2::error("Failure to read peeked length packet. peekedLength="
                       "{PEEKEDLENGTH} recvDataLength={RECVDATALENGTH}",
                       "PEEKEDLENGTH", peekedLength, "RECVDATALENGTH",
                       recvDataLength);
            return;
        }

        if (requestMsg[2] != MCTP_MSG_TYPE_VDM)
        {
            lg2::error("Received non VDM message type={TYPE}", "TYPE",
                       requestMsg[1]);
            return;
        }

        if (verbose)
        {
            utils::printBuffer(utils::Rx, requestMsg);
        }

        // Outgoing message.
        iovec iov[2]{};
        // This structure contains the parameter information for the
        // response message.
        msghdr msg{};

        // process message and send response
        auto response = processRxMsg(requestMsg);
        if (response.has_value())
        {
            constexpr uint8_t tagOwnerBitPos = 3;
            constexpr uint8_t tagOwnerMask = ~(1 << tagOwnerBitPos);
            // Set tag owner bit to 0 for NSM responses
            requestMsg[0] = requestMsg[0] & tagOwnerMask;
            iov[0].iov_base = &requestMsg[0];
            iov[0].iov_len = sizeof(requestMsg[0]) + sizeof(requestMsg[1]) +
                             sizeof(requestMsg[2]);
            iov[1].iov_base = (*response).data();
            iov[1].iov_len = (*response).size();

            msg.msg_iov = iov;
            msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

            if (verbose)
            {
                utils::printBuffer(utils::Tx, *response);
            }

            int result = sendmsg(fd, &msg, 0);
            if (result < 0)
            {
                lg2::error("sendmsg system call failed");
            }
        }
    };

    io = std::make_unique<sdeventplus::source::IO>(event, fd, EPOLLIN,
                                                   std::move(callback));
    return fd;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::processRxMsg(const std::vector<uint8_t>& rxMsg)
{
    nsm_header_info hdrFields{};
    auto hdr =
        reinterpret_cast<const nsm_msg_hdr*>(rxMsg.data() + MCTP_DEMUX_PREFIX);
    if (NSM_SUCCESS != unpack_nsm_header(hdr, &hdrFields))
    {
        lg2::error("Empty NSM request header");
        return std::nullopt;
    }

    if (NSM_EVENT_ACKNOWLEDGMENT == hdrFields.nsm_msg_type)
    {
        lg2::info("received NSM event acknowledgement length={LEN}", "LEN",
                  rxMsg.size());
        return std::nullopt;
    }

    if (NSM_REQUEST == hdrFields.nsm_msg_type)
    {
        lg2::info("received NSM request length={LEN}", "LEN", rxMsg.size());
    }
    auto request = reinterpret_cast<const nsm_msg*>(hdr);
    size_t requestLen = rxMsg.size() - MCTP_DEMUX_PREFIX;

    auto nvidiaMsgType = request->hdr.nvidia_msg_type;
    auto command = request->payload[0];

    lg2::info("nsm msg type={TYPE} command code={COMMAND}", "TYPE",
              nvidiaMsgType, "COMMAND", command);
    switch (nvidiaMsgType)
    {
        case NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY:
            switch (command)
            {
                case NSM_PING:
                    return pingHandler(request, requestLen);
                case NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES:
                    return getSupportNvidiaMessageTypesHandler(request,
                                                               requestLen);
                case NSM_SUPPORTED_COMMAND_CODES:
                    return getSupportCommandCodeHandler(request, requestLen);
                case NSM_QUERY_DEVICE_IDENTIFICATION:
                    return queryDeviceIdentificationHandler(request,
                                                            requestLen);
                case NSM_SET_EVENT_SUBSCRIPTION:
                    return setEventSubscription(request, requestLen);
                case NSM_SET_CURRENT_EVENT_SOURCES:
                    return setCurrentEventSources(request, requestLen);
                case NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT:
                    return configureEventAcknowledgement(request, requestLen);
                default:
                    lg2::error("unsupported Command:{CMD} request length={LEN}",
                               "CMD", command, "LEN", requestLen);

                    return unsupportedCommandHandler(request, requestLen);
            }
            break;
        case NSM_TYPE_NETWORK_PORT:
            switch (command)
            {
                case NSM_GET_PORT_TELEMETRY_COUNTER:
                    return getPortTelemetryCounterHandler(request, requestLen);
                case NSM_QUERY_PORT_CHARACTERISTICS:
                    return queryPortCharacteristicsHandler(request, requestLen);
                case NSM_QUERY_PORT_STATUS:
                    return queryPortStatusHandler(request, requestLen);
                default:
                    lg2::error("unsupported Command:{CMD} request length={LEN}",
                               "CMD", command, "LEN", requestLen);

                    return unsupportedCommandHandler(request, requestLen);
            }
            break;
        case NSM_TYPE_PLATFORM_ENVIRONMENTAL:
            switch (command)
            {
                case NSM_GET_INVENTORY_INFORMATION:
                    return getInventoryInformationHandler(request, requestLen);
                case NSM_GET_TEMPERATURE_READING:
                    return getTemperatureReadingHandler(request, requestLen);
                case NSM_GET_POWER_SUPPLY_STATUS:
                    return getPowerSupplyStatusHandler(request, requestLen);
                case NSM_GET_GPU_PRESENCE_POWER_STATUS:
                    return getGpuPresenceAndPowerStatusHandler(request,
                                                               requestLen);
                case NSM_GET_POWER:
                    return getCurrentPowerDrawHandler(request, requestLen);
                case NSM_GET_ENERGY_COUNT:
                    return getCurrentEnergyCountHandler(request, requestLen);
                case NSM_GET_VOLTAGE:
                    return getVoltageHandler(request, requestLen);
                case NSM_GET_ALTITUDE_PRESSURE:
                    return getAltitudePressureHandler(request, requestLen);
                case NSM_GET_DRIVER_INFO:
                    return getDriverInfoHandler(request, requestLen);
                case NSM_GET_MIG_MODE:
                    return getMigModeHandler(request, requestLen);
                case NSM_SET_MIG_MODE:
                    return setMigModeHandler(request, requestLen);
                case NSM_GET_ECC_MODE:
                    return getEccModeHandler(request, requestLen);
                case NSM_SET_ECC_MODE:
                    return setEccModeHandler(request, requestLen);
                case NSM_GET_ECC_ERROR_COUNTS:
                    return getEccErrorCountsHandler(request, requestLen);
                case NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR:
                    return getEDPpScalingFactorHandler(request, requestLen);
                case NSM_GET_CLOCK_LIMIT:
                    return getClockLimitHandler(request, requestLen);
                case NSM_GET_CURRENT_CLOCK_FREQUENCY:
                    return getCurrClockFreqHandler(request, requestLen);
                case NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME:
                    return getAccumCpuUtilTimeHandler(request, requestLen);

                case NSM_GET_ROW_REMAP_STATE_FLAGS:
                    return getRowRemapStateHandler(request, requestLen);
                case NSM_GET_ROW_REMAPPING_COUNTS:
                    return getRowRemappingCountsHandler(request, requestLen);
                case NSM_GET_MEMORY_CAPACITY_UTILIZATION:
                    return getMemoryCapacityUtilHandler(request, requestLen);
                default:
                    lg2::error("unsupported Command:{CMD} request length={LEN}",
                               "CMD", command, "LEN", requestLen);

                    return unsupportedCommandHandler(request, requestLen);
            }
            break;
        case NSM_TYPE_PCI_LINK:
            switch (command)
            {
                case NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1:
                    return queryScalarGroupTelemetryHandler(request,
                                                            requestLen);
                default:
                    lg2::error("unsupported Command:{CMD} request length={LEN}",
                               "CMD", command, "LEN", requestLen);
                    return unsupportedCommandHandler(request, requestLen);
            }
            break;

        default:
            lg2::error("unsupported Message:{TYPE} request length={LEN}",
                       "TYPE", nvidiaMsgType, "LEN", requestLen);

            return unsupportedCommandHandler(request, requestLen);
    }

    return std::nullopt;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::unsupportedCommandHandler(const nsm_msg* requestMsg,
                                               size_t requestLen)
{
    lg2::info("unsupportedCommand: request length={LEN}", "LEN", requestLen);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    [[maybe_unused]] auto rc = encode_cc_only_resp(
        requestMsg->hdr.instance_id, requestMsg->hdr.nvidia_msg_type,
        requestMsg->payload[0], NSM_ERR_UNSUPPORTED_COMMAND_CODE, reason_code,
        responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::pingHandler(const nsm_msg* requestMsg, size_t requestLen)
{
    lg2::info("pingHandler: request length={LEN}", "LEN", requestLen);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = 0;
    [[maybe_unused]] auto rc =
        encode_ping_resp(requestMsg->hdr.instance_id, reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getSupportNvidiaMessageTypesHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{
    lg2::info("getSupportNvidiaMessageTypesHandler: request length={LEN}",
              "LEN", requestLen);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_supported_nvidia_message_types_resp),
        0);

    // this is to mock that type-0 and type-3 are supported
    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {
        0x9, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    [[maybe_unused]] auto rc = encode_get_supported_nvidia_message_types_resp(
        requestMsg->hdr.instance_id, cc, reason_code, types, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getSupportCommandCodeHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    lg2::error("getSupportCommandCodeHandler: request length={LEN}", "LEN",
               requestLen);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);

    // this is to mock that 0,1,2,9 commandCodes are supported
    bitfield8_t commandCode[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {
        0x7, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    };
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    [[maybe_unused]] auto rc = encode_get_supported_command_codes_resp(
        requestMsg->hdr.instance_id, cc, reason_code, commandCode, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
}

void populateFrom(std::vector<uint8_t>& property, const std::string& data)
{
    property.resize(data.length());
    std::copy(data.data(), data.data() + data.length(), property.data());
}
void populateFrom(std::vector<uint8_t>& property, const uint32_t& data)
{
    property.resize(sizeof(data));
    std::copy((uint8_t*)&data, (uint8_t*)&data + sizeof(data), property.data());
}
void MockupResponder::generateDummyGUID(const uint8_t eid, uint8_t* data)
{
    // just adding eid for first byte and rest is zero
    memcpy(data, &eid, sizeof(eid));
}

std::vector<uint8_t> MockupResponder::getProperty(uint8_t propertyIdentifier)
{
    std::vector<uint8_t> property;
    switch (propertyIdentifier)
    {
        case BOARD_PART_NUMBER:
            populateFrom(property, "MCX750500B-0D00_DK");
            break;
        case SERIAL_NUMBER:
            populateFrom(property, "SN123456789");
            break;
        case MARKETING_NAME:
            populateFrom(property, "NV123");
            break;
        case DEVICE_GUID:
            property = std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00};
            generateDummyGUID(mockEid, property.data());
            break;
        case PRODUCT_LENGTH:
            populateFrom(property, 850);
            break;
        case PRODUCT_WIDTH:
            populateFrom(property, 730);
            break;
        case PRODUCT_HEIGHT:
            populateFrom(property, 2600);
            break;
        case MINIMUM_DEVICE_POWER_LIMIT:
            populateFrom(property, 100);
            break;
        case MAXIMUM_DEVICE_POWER_LIMIT:
            populateFrom(property, 1800);
            break;
        case PCIERETIMER_0_EEPROM_VERSION:
        {
            std::vector<uint8_t> data{0x01, 0x00, 0x1a, 0x00,
                                      0x00, 0x00, 0x0a, 0x00};
            property.resize(data.size());
            memcpy(property.data(), data.data(), data.size());
            break;
        }
        case PCIERETIMER_1_EEPROM_VERSION:
        {
            std::vector<uint8_t> data{0x01, 0x00, 0x1a, 0x00,
                                      0x00, 0x00, 0x0a, 0x00};
            property.resize(data.size());
            memcpy(property.data(), data.data(), data.size());
            break;
        }
        case PCIERETIMER_2_EEPROM_VERSION:
        {
            std::vector<uint8_t> data{0x01, 0x00, 0x1a, 0x00,
                                      0x00, 0x00, 0x0a, 0x00};
            property.resize(data.size());
            memcpy(property.data(), data.data(), data.size());
            break;
        }
        case PCIERETIMER_3_EEPROM_VERSION:
        {
            std::vector<uint8_t> data{0x01, 0x00, 0x1a, 0x00,
                                      0x00, 0x00, 0x0a, 0x00};
            property.resize(data.size());
            memcpy(property.data(), data.data(), data.size());
            break;
        }
        case PCIERETIMER_4_EEPROM_VERSION:
        {
            std::vector<uint8_t> data{0x01, 0x00, 0x1a, 0x00,
                                      0x00, 0x00, 0x0a, 0x00};
            property.resize(data.size());
            memcpy(property.data(), data.data(), data.size());
            break;
        }
        case PCIERETIMER_5_EEPROM_VERSION:
        {
            std::vector<uint8_t> data{0x01, 0x00, 0x1a, 0x00,
                                      0x00, 0x00, 0x0a, 0x00};
            property.resize(data.size());
            memcpy(property.data(), data.data(), data.size());
            break;
        }
        case PCIERETIMER_6_EEPROM_VERSION:
        {
            std::vector<uint8_t> data{0x01, 0x00, 0x1a, 0x00,
                                      0x00, 0x00, 0x0a, 0x00};
            property.resize(data.size());
            memcpy(property.data(), data.data(), data.size());
            break;
        }
        case PCIERETIMER_7_EEPROM_VERSION:
        {
            std::vector<uint8_t> data{0x01, 0x00, 0x1a, 0x00,
                                      0x00, 0x00, 0x0a, 0x00};
            property.resize(data.size());
            memcpy(property.data(), data.data(), data.size());
            break;
        }
        default:
            break;
    }
    return property;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getPortTelemetryCounterHandler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    lg2::info("getPortTelemetryCounterHandler: request length={LEN}", "LEN",
              requestLen);

    uint8_t portNumber = 0;
    auto rc = decode_get_port_telemetry_counter_req(requestMsg, requestLen,
                                                    &portNumber);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_port_telemetry_counter_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    // mock data to send [that is 25 counter data]
    std::vector<uint8_t> data{
        0xF7, 0x5A, 0x3E, 0x00, /*for supported counters, [for CX-7]*/
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    }; /*for counter values, 8 bytes each*/

    auto portTelData = reinterpret_cast<nsm_port_counter_data*>(data.data());
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_get_port_telemetry_counter_resp(requestMsg->hdr.instance_id,
                                                NSM_SUCCESS, reason_code,
                                                portTelData, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_port_telemetry_counter_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryPortCharacteristicsHandler(const nsm_msg* requestMsg,
                                                     size_t requestLen)
{
    lg2::info("queryPortCharacteristicsHandler: request length={LEN}", "LEN",
              requestLen);

    uint8_t portNumber = 0;
    auto rc = decode_query_port_characteristics_req(requestMsg, requestLen,
                                                    &portNumber);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_query_port_characteristics_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    // mock data to send
    std::vector<uint8_t> data{0x09, 0x00, 0x00, 0x00, 0x67, 0x00, 0x00, 0x00,
                              0x13, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00};

    auto portCharData =
        reinterpret_cast<nsm_port_characteristics_data*>(data.data());
    uint16_t reason_code = ERR_NULL;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_query_port_characteristics_resp(requestMsg->hdr.instance_id,
                                                NSM_SUCCESS, reason_code,
                                                portCharData, responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_query_port_characteristics_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryPortStatusHandler(const nsm_msg* requestMsg,
                                            size_t requestLen)
{
    lg2::info("queryPortStatusHandler: request length={LEN}", "LEN",
              requestLen);

    uint8_t portNumber = 0;
    auto rc = decode_query_port_status_req(requestMsg, requestLen, &portNumber);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_query_port_status_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    uint16_t reason_code = ERR_NULL;
    uint8_t port_state = NSM_PORTSTATE_UP;
    uint8_t port_status = NSM_PORTSTATUS_ENABLED;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    rc = encode_query_port_status_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                       reason_code, port_state, port_status,
                                       responseMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_query_port_status_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getInventoryInformationHandler(const nsm_msg* requestMsg,
                                                    size_t requestLen)
{
    lg2::info("getInventoryInformationHandler: request length={LEN}", "LEN",
              requestLen);

    uint8_t propertyIdentifier = 0;
    auto rc = decode_get_inventory_information_req(requestMsg, requestLen,
                                                   &propertyIdentifier);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_inventory_information_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    auto property = getProperty(propertyIdentifier);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + property.size(), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_inventory_information_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, property.size(),
        (uint8_t*)property.data(), responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_inventory_information_resp failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryDeviceIdentificationHandler(const nsm_msg* requestMsg,
                                                      size_t requestLen)
{
    lg2::info("queryDeviceIdentificationHandler: request length={LEN}", "LEN",
              requestLen);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint8_t mockupDeviceIdentification = mockDeviceType;
    uint8_t mockupDeviceInstance = mockInstanceId;

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    [[maybe_unused]] auto rc = encode_query_device_identification_resp(
        requestMsg->hdr.instance_id, cc, reason_code,
        mockupDeviceIdentification, mockupDeviceInstance, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getTemperatureReadingHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    auto request = reinterpret_cast<const nsm_get_temperature_reading_req*>(
        requestMsg->payload);
    uint8_t sensor_id{request->sensor_id};
    lg2::info(
        "getTemperatureReadingHandler: Sensor_Id={ID}, request length={LEN}",
        "LEN", requestLen, "ID", sensor_id);

    if (sensor_id == 255)
    {

        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        [[maybe_unused]] auto rc = encode_aggregate_resp(
            requestMsg->hdr.instance_id, request->hdr.command, NSM_SUCCESS, 2,
            responseMsg);

        uint8_t reading[4]{};
        size_t consumed_len;
        std::array<uint8_t, 50> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        // add sample 1
        rc = encode_aggregate_temperature_reading_data(46.189, reading,
                                                       &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0, true, reading, 4, nsm_sample,
                                          &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 2
        rc = encode_aggregate_temperature_reading_data(-0.343878, reading,
                                                       &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(39, true, reading, 4, nsm_sample,
                                          &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        return response;
    }
    else
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp), 0);

        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        uint16_t reason_code = ERR_NULL;
        [[maybe_unused]] auto rc = encode_get_temperature_reading_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, 78,
            responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        return response;
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getCurrentPowerDrawHandler(const nsm_msg* requestMsg,
                                                size_t requestLen)
{
    auto request = reinterpret_cast<const nsm_get_current_power_draw_req*>(
        requestMsg->payload);
    uint8_t sensor_id{request->sensor_id};
    lg2::info(
        "getCurrentPowerDrawHandler: Sensor_Id={ID}, request length={LEN}",
        "LEN", requestLen, "ID", sensor_id);

    if (sensor_id == 255)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        [[maybe_unused]] auto rc = encode_aggregate_resp(
            requestMsg->hdr.instance_id, request->hdr.command, NSM_SUCCESS, 3,
            responseMsg);

        auto const now = std::chrono::system_clock::now();
        std::time_t newt = std::chrono::system_clock::to_time_t(now);
        const auto timestamp = static_cast<uint64_t>(newt);
        const uint32_t power[2]{25890, 17023};
        uint8_t reading[8]{};
        size_t consumed_len;
        std::array<uint8_t, 50> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        // add sample 1
        rc = encode_aggregate_timestamp_data(timestamp, reading, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0xFF, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 2
        rc = encode_aggregate_get_current_power_draw_reading(power[0], reading,
                                                             &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 3
        rc = encode_aggregate_get_current_power_draw_reading(power[1], reading,
                                                             &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(10, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        return response;
    }
    else
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp), 0);

        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        uint16_t reason_code = ERR_NULL;
        uint32_t power{15870};
        [[maybe_unused]] auto rc = encode_get_current_power_draw_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, power,
            responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        return response;
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getDriverInfoHandler(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    lg2::info("getDriverInfoHandler: request length={LEN}", "LEN", requestLen);

    // Assuming decode_get_driver_info_req
    auto rc = decode_get_driver_info_req(requestMsg, requestLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_driver_info_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    // Prepare mock driver info data
    std::string data = "MockDriverVersion 1.0.0";
    std::vector<uint8_t> driver_info_data;
    driver_info_data.resize(data.length() + 2); // +2 for state and null string
    driver_info_data[0] = 2;                    // driver state
    int index = 1;

    for (char& c : data)
    {
        driver_info_data[index++] = static_cast<uint8_t>(c);
    }
    // Add null character at the end, position is data.length() + 1 due to
    // starting at index 0
    driver_info_data[data.length() + 1] = static_cast<uint8_t>('\0');

    lg2::info("Mock driver info - State: {STATE}, Version: {VERSION}", "STATE",
              2, "VERSION", data);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      NSM_RESPONSE_CONVENTION_LEN +
                                      driver_info_data.size(),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_driver_info_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                     reason_code, driver_info_data.size(),
                                     (uint8_t*)driver_info_data.data(),
                                     responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_driver_info_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getMigModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    auto rc = decode_common_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode request for getMigModeHandler failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    bitfield8_t flags;
    flags.byte = 1;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_MIG_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_MIG_mode_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                  reason_code, &flags, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_MIG_mode_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setMigModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    uint8_t requested_mode;
    auto rc = decode_set_MIG_mode_req(requestMsg, requestLen, &requested_mode);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_MIG_mode_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_MIG_mode_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                  reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_MIG_mode_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getEccModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    auto rc = decode_common_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode request for getEccModeHandler failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    bitfield8_t flags;
    flags.byte = 11;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_ECC_mode_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                  reason_code, &flags, responseMsg);
    assert(rc == NSM_SW_SUCCESS);

    if (rc)
    {
        lg2::error("encode_get_ECC_mode_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setEccModeHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    uint8_t requested_mode;
    auto rc = decode_set_ECC_mode_req(requestMsg, requestLen, &requested_mode);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_set_ECC_mode_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_set_ECC_mode_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                  reason_code, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_ECC_mode_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getEccErrorCountsHandler(const nsm_msg* requestMsg,
                                              size_t requestLen)
{
    auto rc = decode_common_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error(
            "decode requset for getEccErrorCountsHandler failed: rc={RC}", "RC",
            rc);
        return std::nullopt;
    }
    struct nsm_ECC_error_counts errorCounts;
    errorCounts.flags.byte = 132;
    errorCounts.sram_corrected = 1234;
    errorCounts.sram_uncorrected_secded = 4532;
    errorCounts.sram_uncorrected_parity = 6567;
    errorCounts.dram_corrected = 9876;
    errorCounts.dram_uncorrected = 9654;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_error_counts_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_ECC_error_counts_resp(requestMsg->hdr.instance_id,
                                          NSM_SUCCESS, reason_code,
                                          &errorCounts, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_ECC_error_counts_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::getPowerSupplyStatusHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    lg2::info("getPowerSupplyStatusHandler: request length={LEN}", "LEN",
              requestLen);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_supply_status_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_power_supply_status_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, 0x01, responseMsg);
    assert(rc == NSM_SUCCESS);
    return response;
}
std::optional<std::vector<uint8_t>>
    MockupResponder::getGpuPresenceAndPowerStatusHandler(
        const nsm_msg* requestMsg, size_t requestLen)
{

    lg2::info("getGpuPresenceAndPowerStatusHandler: request length={LEN}",
              "LEN", requestLen);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_gpu_presence_and_power_status_resp),
        0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_gpu_presence_and_power_status_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, ERR_NULL, 0x01, 0x01,
        responseMsg);
    assert(rc == NSM_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setEventSubscription(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    lg2::info("setEventSubscription: request length={LEN}", "LEN", requestLen);

    uint8_t globalSetting = 0;
    uint8_t receiverEid = 0;
    auto rc = decode_nsm_set_event_subscription_req(
        requestMsg, requestLen, &globalSetting, &receiverEid);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("decode_nsm_set_event_subscription_req failed RC={RC}", "RC",
                   rc);
    }

    globalEventGenerationSetting = globalSetting;
    eventReceiverEid = receiverEid;
    lg2::info("setEventSubscription: setting={SETTING} ReceiverEID={EID}",
              "SETTING", globalEventGenerationSetting, "EID", eventReceiverEid);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN, 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_cc_only_resp(
        requestMsg->hdr.instance_id, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
        NSM_SET_EVENT_SUBSCRIPTION, NSM_SUCCESS, ERR_NULL, responseMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("encode_cc_only_resp failed RC={RC}", "RC", rc);
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::setCurrentEventSources(const nsm_msg* requestMsg,
                                            size_t requestLen)
{
    lg2::info("setCurrentEventSources: request length={LEN}", "LEN",
              requestLen);

    uint8_t nvidiaMessageType = 0;
    bitfield8_t* event_sources;

    auto rc = decode_nsm_set_current_event_source_req(
        requestMsg, requestLen, &nvidiaMessageType, &event_sources);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("decode_nsm_set_current_event_source_req failed RC={RC}",
                   "RC", rc);
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN, 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_cc_only_resp(
        requestMsg->hdr.instance_id, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
        NSM_SET_CURRENT_EVENT_SOURCES, NSM_SUCCESS, ERR_NULL, responseMsg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("encode_cc_only_resp failed RC={RC}", "RC", rc);
    }

    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::configureEventAcknowledgement(const nsm_msg* requestMsg,
                                                   size_t requestLen)
{
    lg2::info("configureEventAcknowledgement: request length={LEN}", "LEN",
              requestLen);

    uint8_t nvidiaMessageType = 0;
    bitfield8_t* currentEventSourcesAcknowledgementMask;

    auto rc = decode_nsm_configure_event_acknowledgement_req(
        requestMsg, requestLen, &nvidiaMessageType,
        &currentEventSourcesAcknowledgementMask);
    if (rc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_nsm_configure_event_acknowledgement_req failed RC={RC}",
            "RC", rc);
    }

    lg2::info(
        "receive configureEventAcknowledgement request, nvidiaMessageType={MSGTYPE} mask[0]={M0} mask[1]={M1}",
        "MSGTYPE", nvidiaMessageType, "M0",
        currentEventSourcesAcknowledgementMask[0].byte, "M1",
        currentEventSourcesAcknowledgementMask[1].byte);

    std::vector<bitfield8_t> newEventSourcesAcknowledgementMask(8);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_configure_event_acknowledgement_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_nsm_configure_event_acknowledgement_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS,
        newEventSourcesAcknowledgementMask.data(), responseMsg);
    assert(rc == NSM_SUCCESS);
    return response;
}

template <class T>
void Logger(bool verbose, const char* msg, const T& data)
{
    if (verbose)
    {
        std::stringstream s;
        s << data;
        std::cout << msg << s.str() << std::endl;
    }
}

int MockupResponder::mctpSockSend(uint8_t dest_eid,
                                  std::vector<uint8_t>& requestMsg,
                                  bool verbose)
{
    if (sockFd < 0)
    {
        lg2::error("mctpSockSend failed, invalid sockFd");
        return NSM_ERROR;
    }

    msghdr msg{};
    iovec iov[2]{};

    uint8_t prefix[3];
    prefix[0] = MCTP_MSG_TAG_REQ;
    prefix[1] = dest_eid;
    prefix[2] = MCTP_MSG_TYPE_VDM;

    iov[0].iov_base = &prefix[0];
    iov[0].iov_len = sizeof(prefix);
    iov[1].iov_base = requestMsg.data();
    iov[1].iov_len = requestMsg.size();

    msg.msg_iov = iov;
    msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

    if (verbose)
    {
        utils::printBuffer(utils::Tx, requestMsg);
    }

    auto rc = sendmsg(sockFd, &msg, 0);
    if (rc < 0)
    {
        lg2::error("sendmsg system call failed rc={RC}", "RC", rc);
        return NSM_ERROR;
    }

    return NSM_SUCCESS;
}

void MockupResponder::sendRediscoveryEvent(uint8_t dest, bool ackr)
{
    lg2::info("sendRediscoveryEvent dest eid={EID}", "EID", dest);
    uint8_t instanceId = 23;
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_rediscovery_event(instanceId, ackr, msg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("sendRediscoveryEvent failed");
    }

    rc = mctpSockSend(dest, eventMsg, true);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("mctpSockSend() failed, rc={RC}", "RC", rc);
    }
}

void MockupResponder::sendNsmEvent(uint8_t dest, uint8_t nsmType, bool ackr,
                                   uint8_t ver, uint8_t eventId,
                                   uint8_t eventClass, uint16_t eventState,
                                   uint8_t dataSize, uint8_t* data)
{
    lg2::info("sendNsmEvent dest eid={EID}", "EID", dest);
    uint8_t instanceId = 23;
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                  dataSize);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_event(instanceId, nsmType, ackr, ver, eventId,
                               eventClass, eventState, dataSize, data, msg);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("sendNsmEvent failed");
    }

    rc = mctpSockSend(dest, eventMsg, true);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("mctpSockSend() failed, rc={RC}", "RC", rc);
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getCurrentEnergyCountHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    auto request = reinterpret_cast<const nsm_get_current_energy_count_req*>(
        requestMsg->payload);
    uint8_t sensor_id{request->sensor_id};
    lg2::info(
        "getCurrentEnergyCountHandler: Sensor_Id={ID}, request length={LEN}",
        "LEN", requestLen, "ID", sensor_id);

    if (sensor_id == 255)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        [[maybe_unused]] auto rc = encode_aggregate_resp(
            requestMsg->hdr.instance_id, request->hdr.command, NSM_SUCCESS, 2,
            responseMsg);

        const uint64_t energy[2]{25890, 17023};
        uint8_t reading[8]{};
        size_t consumed_len{};
        std::array<uint8_t, 50> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        // add sample 1
        rc = encode_aggregate_energy_count_data(energy[0], reading,
                                                &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 2
        rc = encode_aggregate_energy_count_data(energy[1], reading,
                                                &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(45, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        return response;
    }
    else
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_resp), 0);

        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        uint16_t reason_code = ERR_NULL;
        uint32_t energy{15870};
        [[maybe_unused]] auto rc = encode_get_current_energy_count_resp(
            requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, energy,
            responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        return response;
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getVoltageHandler(const nsm_msg* requestMsg,
                                       size_t requestLen)
{
    auto request =
        reinterpret_cast<const nsm_get_voltage_req*>(requestMsg->payload);
    uint8_t sensor_id{request->sensor_id};
    lg2::info("getVoltageHandler: Sensor_Id={ID}, request length={LEN}", "LEN",
              requestLen, "ID", sensor_id);

    if (sensor_id == 255)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        [[maybe_unused]] auto rc = encode_aggregate_resp(
            requestMsg->hdr.instance_id, request->hdr.command, NSM_SUCCESS, 2,
            responseMsg);

        const uint32_t voltage[2]{32438470, 57978897};
        uint8_t reading[8]{};
        size_t consumed_len{};
        std::array<uint8_t, 50> sample;
        auto nsm_sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

        // add sample 1
        rc = encode_aggregate_voltage_data(voltage[0], reading, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(0, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        // add sample 2
        rc = encode_aggregate_voltage_data(voltage[1], reading, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        rc = encode_aggregate_resp_sample(33, true, reading, consumed_len,
                                          nsm_sample, &consumed_len);
        assert(rc == NSM_SW_SUCCESS);

        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));

        return response;
    }
    else
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_resp), 0);

        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        uint16_t reason_code = ERR_NULL;
        uint32_t voltage{25347808};
        [[maybe_unused]] auto rc =
            encode_get_voltage_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                    reason_code, voltage, responseMsg);
        assert(rc == NSM_SW_SUCCESS);
        return response;
    }
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getAltitudePressureHandler(const nsm_msg* requestMsg,
                                                size_t requestLen)
{
    lg2::info("getAltitudePressureHandler: request length={LEN}", "LEN",
              requestLen);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_altitude_pressure_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    uint32_t pressure{943730};
    auto rc = encode_get_altitude_pressure_resp(requestMsg->hdr.instance_id,
                                                NSM_SUCCESS, reason_code,
                                                pressure, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
}
void getScalarTelemetryGroup0Data(
    struct nsm_query_scalar_group_telemetry_group_0* data)
{
    data->pci_vendor_id = 3;
    data->pci_device_id = 3;
    data->pci_subsystem_vendor_id = 3;
    data->pci_subsystem_device_id = 3;
}
void getScalarTelemetryGroup1Data(
    struct nsm_query_scalar_group_telemetry_group_1* data)
{
    data->negotiated_link_speed = 3;
    data->negotiated_link_width = 3;
    data->target_link_speed = 3;
    data->max_link_speed = 3;
    data->max_link_width = 3;
}

void getScalarTelemetryGroup2Data(
    struct nsm_query_scalar_group_telemetry_group_2* data)
{
    data->non_fatal_errors = 1111;
    data->fatal_errors = 2222;
    data->unsupported_request_count = 3333;
    data->correctable_errors = 4444;
}

void getScalarTelemetryGroup3Data(
    struct nsm_query_scalar_group_telemetry_group_3* data)
{
    data->L0ToRecoveryCount = 8769;
}

void getScalarTelemetryGroup4Data(
    struct nsm_query_scalar_group_telemetry_group_4* data)
{
    data->recv_err_cnt = 100;
    data->NAK_recv_cnt = 200;
    data->NAK_sent_cnt = 300;
    data->bad_TLP_cnt = 400;
    data->replay_rollover_cnt = 500;
    data->FC_timeout_err_cnt = 600;
    data->replay_cnt = 700;
}

void getScalarTelemetryGroup5Data(
    struct nsm_query_scalar_group_telemetry_group_5* data)
{
    data->PCIeTXBytes = 8769000;
    data->PCIeRXBytes = 876654;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::queryScalarGroupTelemetryHandler(const nsm_msg* requestMsg,
                                                      size_t requestLen)
{
    lg2::info("queryScalarGroupTelemetryHandler: request length={LEN}", "LEN",
              requestLen);
    uint8_t device_id;
    uint8_t group_index;
    [[maybe_unused]] auto rc = decode_query_scalar_group_telemetry_v1_req(
        requestMsg, requestLen, &device_id, &group_index);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_query_scalar_group_telemetry_v1_req failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }

    switch (group_index)
    {
        case 0:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            struct nsm_query_scalar_group_telemetry_group_0 data;
            getScalarTelemetryGroup0Data(&data);
            uint16_t reason_code = ERR_NULL;
            rc = encode_query_scalar_group_telemetry_v1_group0_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &data,
                responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_scalar_group_telemetry_v1_group0_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        case 1:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            struct nsm_query_scalar_group_telemetry_group_1 data;
            getScalarTelemetryGroup1Data(&data);
            uint16_t reason_code = ERR_NULL;
            rc = encode_query_scalar_group_telemetry_v1_group1_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &data,
                responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_scalar_group_telemetry_v1_group1_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        case 2:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            struct nsm_query_scalar_group_telemetry_group_2 data;
            getScalarTelemetryGroup2Data(&data);
            uint16_t reason_code = ERR_NULL;
            rc = encode_query_scalar_group_telemetry_v1_group2_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &data,
                responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_scalar_group_telemetry_v1_group2_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }

        case 3:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            struct nsm_query_scalar_group_telemetry_group_3 data;
            getScalarTelemetryGroup3Data(&data);
            uint16_t reason_code = ERR_NULL;
            rc = encode_query_scalar_group_telemetry_v1_group3_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &data,
                responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_scalar_group_telemetry_v1_group3_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }

        case 4:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            struct nsm_query_scalar_group_telemetry_group_4 data;
            getScalarTelemetryGroup4Data(&data);
            uint16_t reason_code = ERR_NULL;
            rc = encode_query_scalar_group_telemetry_v1_group4_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &data,
                responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_scalar_group_telemetry_v1_group4_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }
        case 5:
        {
            std::vector<uint8_t> response(
                sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp),
                0);
            auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
            struct nsm_query_scalar_group_telemetry_group_5 data;
            getScalarTelemetryGroup5Data(&data);
            uint16_t reason_code = ERR_NULL;
            rc = encode_query_scalar_group_telemetry_v1_group5_resp(
                requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &data,
                responseMsg);
            assert(rc == NSM_SW_SUCCESS);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "encode_query_scalar_group_telemetry_v1_group5_resp failed: rc={RC}",
                    "RC", rc);
                return std::nullopt;
            }
            return response;
        }

        default:
            break;
    }
    return std::nullopt;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getEDPpScalingFactorHandler(const nsm_msg* requestMsg,
                                                 size_t requestLen)
{
    [[maybe_unused]] auto rc = decode_common_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode request for getEDPpScalingFactorHandler failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    struct nsm_EDPp_scaling_factors scaling_factors;
    scaling_factors.default_scaling_factor = 70;
    scaling_factors.maximum_scaling_factor = 90;
    scaling_factors.minimum_scaling_factor = 60;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_programmable_EDPp_scaling_factor_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_programmable_EDPp_scaling_factor_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code, &scaling_factors,
        responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_programmable_EDPp_scaling_factor_resp failed: rc={RC}",
            "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getClockLimitHandler(const nsm_msg* requestMsg,
                                          size_t requestLen)
{
    uint8_t clock_id;
    auto rc = decode_get_clock_limit_req(requestMsg, requestLen, &clock_id);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode_get_clock_limit_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    if (clock_id != GRAPHICS_CLOCK && clock_id != MEMORY_CLOCK)
    {
        lg2::error("getClockLimitHandler: invalid clock_id = {CLOCK_ID}",
                   "CLOCKID", clock_id);
        return std::nullopt;
    }
    struct nsm_clock_limit clockLimit;
    clockLimit.requested_limit_min = 800;
    clockLimit.requested_limit_max = 1800;
    clockLimit.present_limit_min = 200;
    clockLimit.present_limit_max = 2000;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_clock_limit_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                     reason_code, &clockLimit, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_clock_limit_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getCurrClockFreqHandler(const nsm_msg* requestMsg,
                                             size_t requestLen)
{
    uint8_t clock_id;
    auto rc = decode_get_curr_clock_freq_req(requestMsg, requestLen, &clock_id);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode req for getCurrClockFreqHandler failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    if (clock_id != GRAPHICS_CLOCK && clock_id != MEMORY_CLOCK)
    {
        lg2::error("getCurrClockFreqHandler: invalid clock_id = {CLOCK_ID}",
                   "CLOCKID", clock_id);
        return std::nullopt;
    }

    uint32_t clockFreq = 970;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_curr_clock_freq_resp(requestMsg->hdr.instance_id,
                                         NSM_SUCCESS, reason_code, &clockFreq,
                                         responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_curr_clock_freq_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getAccumCpuUtilTimeHandler(const nsm_msg* requestMsg,
                                                size_t requestLen)
{
    auto rc = decode_common_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode req for getAccumCpuUtilTimeHandler failed: rc={RC}",
                   "RC", rc);
        return std::nullopt;
    }
    uint32_t context_util_time = 4987;
    uint32_t SM_util_time = 2564;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_accum_GPU_util_time_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_accum_GPU_util_time_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
        &context_util_time, &SM_util_time, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_accum_GPU_util_time_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getRowRemapStateHandler(const nsm_msg* requestMsg,
                                             size_t requestLen)
{
    auto rc = decode_get_row_remap_state_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_row_remap_state_req failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    bitfield8_t flags;
    flags.byte = 13;
    lg2::error("value of flag is {FLAGS}", "FLAGS", flags.byte);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_row_remap_state_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_row_remap_state_resp(requestMsg->hdr.instance_id,
                                         NSM_SUCCESS, reason_code, &flags,
                                         responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("encode_get_row_remap_state_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getRowRemappingCountsHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    auto rc = decode_get_row_remapping_counts_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc)
    {
        lg2::error("decode_get_row_remapping_counts_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    uint32_t correctable_error = 4987;
    uint32_t uncorrectable_error = 2564;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_row_remapping_counts_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_row_remapping_counts_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, reason_code,
        correctable_error, uncorrectable_error, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_row_remapping_counts_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getMemoryCapacityUtilHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    lg2::info("getMemoryCapacityUtilHandler: request length={LEN}", "LEN",
              requestLen);

    auto rc = decode_get_memory_capacity_util_req(requestMsg, requestLen);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("decode_get_memory_capacity_util_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    struct nsm_memory_capacity_utilization data;
    data.reserved_memory = 2345567;
    data.used_memory = 128888;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_memory_capacity_util_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    rc = encode_get_memory_capacity_util_resp(requestMsg->hdr.instance_id,
                                              NSM_SUCCESS, reason_code, &data,
                                              responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_memory_capacity_util_resp failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }
    return response;
}
} // namespace MockupResponder
