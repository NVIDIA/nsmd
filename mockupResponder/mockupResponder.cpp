#include "mockupResponder.hpp"

#include "base.h"
#include "network-ports.h"
#include "platform-environmental.h"

#include "types.hpp"
#include "utils.hpp"

#include <sys/socket.h>

#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/source/io.hpp>

#include <ctime>

namespace MockupResponder
{

int MockupResponder::connectMockupEID(uint8_t eid, uint8_t deviceType, uint8_t instanceId)
{
    lg2::info("connect to Mockup EID");

    mockEid = eid;
    mockDeviceType = deviceType;
    mockInstanceId = instanceId;
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

    ret = ::write(fd, &eid, sizeof(uint8_t));
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

    auto io = std::make_unique<sdeventplus::source::IO>(event, fd, EPOLLIN,
                                                        std::move(callback));
    return event.loop();
}

std::optional<std::vector<uint8_t>>
    MockupResponder::processRxMsg(const std::vector<uint8_t>& requestMsg)
{
    nsm_header_info hdrFields{};
    auto hdr = reinterpret_cast<const nsm_msg_hdr*>(requestMsg.data() +
                                                    MCTP_DEMUX_PREFIX);
    auto request = reinterpret_cast<const nsm_msg*>(hdr);
    size_t requestLen = requestMsg.size() - MCTP_DEMUX_PREFIX;

    if (NSM_SW_SUCCESS != unpack_nsm_header(hdr, &hdrFields))
    {
        lg2::error("Empty NSM request header");
        return std::nullopt;
    }

    if (NSM_REQUEST == hdrFields.nsm_msg_type)
    {
        lg2::error("received NSM request length={LEN}", "LEN",
                   requestMsg.size());
    }

    auto nvidiaMsgType = request->hdr.nvidia_msg_type;
    auto command = request->payload[0];

    lg2::error("nsm msg type={TYPE} command code={COMMAND}", "TYPE",
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
                case NSM_GET_POWER:
                    return getCurrentPowerDrawHandler(request, requestLen);
                case NSM_GET_DRIVER_INFO:
                    return getDriverInfoHandler(request, requestLen);
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
    auto rc = encode_cc_only_resp(
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
    auto rc =
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
    auto rc = encode_get_supported_nvidia_message_types_resp(
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
    auto rc = encode_get_supported_command_codes_resp(
        requestMsg->hdr.instance_id, cc, reason_code, commandCode, responseMsg);
    assert(rc == NSM_SW_SUCCESS);
    return response;
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
        {
            std::string data = "MCX750500B-0D00_DK";
            property.resize(data.length());
            memcpy(property.data(), data.data(), data.length());
            break;
        }
        case SERIAL_NUMBER:
        {
            std::string data = "SN123456789";
            property.resize(data.length());
            memcpy(property.data(), data.data(), data.length());
            break;
        }
        case DEVICE_GUID:
        {
            std::vector<uint8_t> data{0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00};
            generateDummyGUID(mockEid, data.data());

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
    auto rc = encode_query_device_identification_resp(
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

        auto rc = encode_aggregate_resp(requestMsg->hdr.instance_id,
                                        request->hdr.command, NSM_SUCCESS, 2,
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
        auto rc = encode_get_temperature_reading_resp(
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

        auto rc = encode_aggregate_resp(requestMsg->hdr.instance_id,
                                        request->hdr.command, NSM_SUCCESS, 3,
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
        auto rc = encode_get_current_power_draw_resp(
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
    const char* mockVersion = "MockDriverVersion 1.0.0";
    size_t versionLength =
        std::strlen(mockVersion) + 1; // Include null terminator

    auto driver_info = reinterpret_cast<struct nsm_driver_info*>(
        malloc(sizeof(struct nsm_driver_info) + versionLength));
    if (!driver_info)
    {
        // Handle memory allocation failure
        lg2::error("Failed to allocate memory for driver_info");
        return std::nullopt;
    }

    // Initialize driver_info to zero, including the newly allocated memory
    memset(driver_info, 0, sizeof(struct nsm_driver_info) - 1 + versionLength);

    // Mocking driver state and version for the response
    driver_info->driver_state = 2;
    memcpy(driver_info->driver_version, mockVersion,
           versionLength); // copy null string character

    // Calculate total response size: header, fixed part of response, and
    // dynamic driver version length
    size_t totalResponseSize = sizeof(nsm_msg_hdr) +
                               sizeof(nsm_get_driver_info_resp) - 1 +
                               versionLength; // -1 because of FAM

    lg2::info("Mock driver info - State: {STATE}, Version: {VERSION}", "STATE",
              static_cast<int>(driver_info->driver_state), "VERSION",
              mockVersion);

    std::vector<uint8_t> response(totalResponseSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    // Assuming encode_get_driver_info_resp fills in the response with the
    // provided driver state and version
    rc = encode_get_driver_info_resp(requestMsg->hdr.instance_id, NSM_SUCCESS,
                                     reason_code, driver_info, responseMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_driver_info_resp failed: rc={RC}", "RC", rc);
        return std::nullopt;
    }

    return response;
}
} // namespace MockupResponder
