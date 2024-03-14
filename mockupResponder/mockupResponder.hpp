#pragma once

#include "base.h"

#include <sdbusplus/asio/object_server.hpp>
#include <sdeventplus/event.hpp>

#include <optional>

namespace MockupResponder
{
constexpr uint8_t MCTP_MSG_TYPE_VDM = 0x7e;
constexpr uint8_t MCTP_MSG_EMU_PREFIX = 0xFF;
// these are for use with the mctp-demux-daemon
constexpr size_t mctpMaxMessageSize = 4096;

constexpr char MCTP_SOCKET_PATH[] = "\0mctp-pcie-mux";

struct HeaderType
{
    uint8_t eid;
    uint8_t type;
};

class MockupResponder
{
  public:
    MockupResponder(bool verbose, sdeventplus::Event& event) :
        event(event), verbose(verbose)
    {}

    ~MockupResponder()
    {}

    int connectMockupEID(uint8_t eid);

    std::optional<std::vector<uint8_t>>
        processRxMsg(const std::vector<uint8_t>& request);

    // type0 handlers
    std::optional<std::vector<uint8_t>>
        unsupportedCommandHandler(const nsm_msg* requestMsg, size_t requestLen);
    std::optional<std::vector<uint8_t>> pingHandler(const nsm_msg* requestMsg,
                                                    size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getSupportNvidiaMessageTypesHandler(const nsm_msg* requestMsg,
                                            size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getSupportCommandCodeHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::optional<std::vector<uint8_t>>
        queryDeviceIdentificationHandler(const nsm_msg* requestMsg,
                                         size_t requestLen);

    // type3 handlers
    std::optional<std::vector<uint8_t>>
        getInventoryInformationHandler(const nsm_msg* requestMsg,
                                       size_t requestLen);
    std::vector<uint8_t> getProperty(uint8_t propertyIdentifier);
    std::optional<std::vector<uint8_t>>
        getTemperatureReadingHandler(const nsm_msg* requestMsg,
                                     size_t requestLen);
    std::optional<std::vector<uint8_t>>
        getCurrentPowerDrawHandler(const nsm_msg* requestMsg,
                                   size_t requestLen);

  private:
    sdeventplus::Event& event;
    bool verbose;
};

} // namespace MockupResponder