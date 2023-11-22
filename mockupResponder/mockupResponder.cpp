#include "mockupResponder.hpp"

#include "platform-environmental.h"

#include "types.hpp"
#include "utils.hpp"

#include <sys/socket.h>

#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/source/io.hpp>

namespace MockupResponder
{

const uint8_t mockupDeviceIdentification = NSM_DEV_ID_PCIE_BRIDGE;
const uint8_t mockupDeviceInstance = 0;

int MockupResponder::connectMockupEID(uint8_t eid)
{
    lg2::info("connect to Mockup EID");

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

        if (requestMsg[1] != MCTP_MSG_TYPE_VDM)
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
            iov[0].iov_base = &requestMsg[0];
            iov[0].iov_len = sizeof(requestMsg[0]) + sizeof(requestMsg[1]);
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
#define MCTP_DEMUX_PREFIX 2 // eid + mctp message type
    nsm_header_info hdrFields{};
    auto hdr = reinterpret_cast<const nsm_msg_hdr*>(requestMsg.data() +
                                                    MCTP_DEMUX_PREFIX);
    auto request = reinterpret_cast<const nsm_msg*>(hdr);
    size_t requestLen = requestMsg.size() - MCTP_DEMUX_PREFIX;

    if (NSM_SUCCESS != unpack_nsm_header(hdr, &hdrFields))
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
        case NSM_TYPE_PLATFORM_ENVIRONMENTAL:
            switch (command)
            {
                case NSM_GET_INVENTORY_INFORMATION:
                    return getInventoryInformationHandler(request, requestLen);
                case NSM_GET_TEMPERATURE_READING:
                    return getTemperatureReadingHandler(request, requestLen);
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
    auto rc = encode_cc_only_resp(
        requestMsg->hdr.instance_id, requestMsg->hdr.nvidia_msg_type,
        requestMsg->payload[0], NSM_ERR_UNSUPPORTED_COMMAND_CODE, responseMsg);
    assert(rc == NSM_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::pingHandler(const nsm_msg* requestMsg, size_t requestLen)
{
    lg2::info("pingHandler: request length={LEN}", "LEN", requestLen);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_ping_resp(requestMsg->hdr.instance_id, responseMsg);
    assert(rc == NSM_SUCCESS);
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
    bitfield8_t types[8] = {0x9, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_supported_nvidia_message_types_resp(
        requestMsg->hdr.instance_id, types, responseMsg);
    assert(rc == NSM_SUCCESS);
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
    bitfield8_t commandCode[32] = {
        0x7, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    };

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_supported_command_codes_resp(
        requestMsg->hdr.instance_id, commandCode, responseMsg);
    assert(rc == NSM_SUCCESS);
    return response;
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
        default:
            break;
    }
    return property;
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
    if (rc)
    {
        lg2::error("decode_get_inventory_information_req failed: rc={RC}", "RC",
                   rc);
        return std::nullopt;
    }

    auto property = getProperty(propertyIdentifier);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + property.size(), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = encode_get_inventory_information_resp(
        requestMsg->hdr.instance_id, NSM_SUCCESS, property.size(),
        (uint8_t*)property.data(), responseMsg);
    if (rc)
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

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_device_identification_resp(
        requestMsg->hdr.instance_id, mockupDeviceIdentification,
        mockupDeviceInstance, responseMsg);
    assert(rc == NSM_SUCCESS);
    return response;
}

std::optional<std::vector<uint8_t>>
    MockupResponder::getTemperatureReadingHandler(const nsm_msg* requestMsg,
                                                  size_t requestLen)
{
    lg2::info("getTemperatureReadingHandler: request length={LEN}", "LEN",
              requestLen);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp), 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_temperature_reading_resp(requestMsg->hdr.instance_id,
                                                  NSM_SUCCESS, 78, responseMsg);
    assert(rc == NSM_SUCCESS);
    return response;
}

} // namespace MockupResponder
